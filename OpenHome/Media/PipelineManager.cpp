#include <OpenHome/Media/PipelineManager.h>
#include <OpenHome/Media/Pipeline/Pipeline.h>
#include <OpenHome/Media/Protocol/Protocol.h>
#include <OpenHome/Media/Filler.h>
#include <OpenHome/Media/IdManager.h>
#include <OpenHome/Private/Printer.h>
#include <OpenHome/Media/Debug.h>

#include <limits.h>

using namespace OpenHome;
using namespace OpenHome::Media;

// PipelineManager

PipelineManager::PipelineManager(IInfoAggregator& aInfoAggregator, TrackFactory& aTrackFactory)
    : iLock("PLM1")
    , iPublicLock("PLM2")
    , iPipelineState(EPipelineStopped)
    , iPipelineStoppedSem("PLM3", 1)
{
    iPipeline = new Pipeline(aInfoAggregator, *this, iPrefetchObserver);
    iIdManager = new IdManager(*iPipeline);
    iFiller = new Filler(*iPipeline, *iIdManager, aTrackFactory, iPrefetchObserver, iPipeline->SenderMinLatencyMs() * Jiffies::kPerMs);
    iProtocolManager = new ProtocolManager(*iFiller, *iIdManager, *iPipeline);
    iFiller->Start(*iProtocolManager);
}

PipelineManager::~PipelineManager()
{
    delete iPipeline;
    delete iProtocolManager;
    delete iFiller;
    delete iIdManager;
    for (TUint i=0; i<iUriProviders.size(); i++) {
        delete iUriProviders[i];
    }
}

void PipelineManager::Quit()
{
    AutoMutex _(iPublicLock);
    iLock.Wait();
    const TBool waitStop = (iPipelineState != EPipelineStopped);
    const TUint haltId = iFiller->Stop();
    iPipeline->RemoveCurrentStream();
    iLock.Signal();
    iIdManager->InvalidatePending();
    if (waitStop) {
        iPipeline->Stop(haltId);
        iPipelineStoppedSem.Wait();
    }
    iPipeline->Quit();
    iFiller->Quit();
}

void PipelineManager::Add(Codec::CodecBase* aCodec)
{
    iPipeline->AddCodec(aCodec);
}

void PipelineManager::Add(Protocol* aProtocol)
{
    iProtocolManager->Add(aProtocol);
}

void PipelineManager::Add(ContentProcessor* aContentProcessor)
{
    iProtocolManager->Add(aContentProcessor);
}

void PipelineManager::Add(UriProvider* aUriProvider)
{
    iUriProviders.push_back(aUriProvider);
    iFiller->Add(*aUriProvider);
}

void PipelineManager::Start()
{
    iPipeline->Start();
}

void PipelineManager::AddObserver(IPipelineObserver& aObserver)
{
    iLock.Wait();
    iObservers.push_back(&aObserver);
    iLock.Signal();
}

void PipelineManager::RemoveObserver(IPipelineObserver& aObserver)
{
    iLock.Wait();
    for (TUint i=0; i<iObservers.size(); i++) {
        if (iObservers[i] == &aObserver) {
            iObservers.erase(iObservers.begin()+i);
            break;
        }
    }
    iLock.Signal();
}

void PipelineManager::AddObserver(ITrackObserver& aObserver)
{
    iPipeline->AddObserver(aObserver);
}

void PipelineManager::Begin(const Brx& aMode, TUint aTrackId)
{
    AutoMutex _(iPublicLock);
    iLock.Wait();
    iMode.Replace(aMode);
    iTrackId = aTrackId;
    iLock.Signal();
    iFiller->Play(aMode, aTrackId);
}

void PipelineManager::Play()
{
    AutoMutex _(iPublicLock);
    iPipeline->Play();
}

void PipelineManager::Pause()
{
    AutoMutex _(iPublicLock);
    iPipeline->Pause();
}

void PipelineManager::Wait(TUint aFlushId)
{
    AutoMutex _(iPublicLock);
    iPipeline->Wait(aFlushId);
}

void PipelineManager::Stop()
{
    AutoMutex _(iPublicLock);
    const TUint haltId = iFiller->Stop();
    iPipeline->Stop(haltId);
    iIdManager->InvalidatePending(); /* don't use InvalidateAll - iPipeline->Stop() will
                                        have removed current stream.  InvalidateAll ends
                                        up with Stopper trying to halt (pause) which would
                                        override the attempt to Stop it. */
}

void PipelineManager::StopPrefetch(const Brx& aMode, TUint aTrackId)
{
    AutoMutex _(iPublicLock);
    LOG(kMedia, "PipelineManager::StopPrefetch(");
    LOG(kMedia, aMode);
    LOG(kMedia, ", %u)\n", aTrackId);
    /*const TUint haltId = */iFiller->Stop(); // FIXME - could get away without Filler generating a Halt here
    iPipeline->RemoveCurrentStream();
    iIdManager->InvalidatePending();
    iPrefetchObserver.SetTrack(aTrackId==Track::kIdNone? iFiller->NullTrackId() : aTrackId);
    iFiller->PlayLater(aMode, aTrackId);
    iPipeline->Play(); // in case pipeline is paused/stopped, force it to pull until a new track
    try {
        iPrefetchObserver.Wait(5000); /* It's possible that a protocol module will block without
                                         ever delivering content.  Other pipeline operations which
                                         might interrupt it are blocked by iPublicLock so we
                                         timeout after 5s as a workaround */
    }
    catch (Timeout&) {
        Log::Print("WARNING: Timeout from PipelineManager::StopPrefetch.  trackId=%u, mode=", aTrackId);
        Log::Print(aMode);
        Log::Print("\n");
    }
}

void PipelineManager::RemoveAll()
{
    AutoMutex _(iPublicLock);
    /*TUint haltId = */iFiller->Stop();
    iLock.Wait();
    iPipeline->RemoveCurrentStream();
    iLock.Signal();
    iIdManager->InvalidateAll();
}

TBool PipelineManager::Seek(TUint aTrackId, TUint aStreamId, TUint aSecondsAbsolute)
{
    AutoMutex _(iPublicLock);
    return iPipeline->Seek(aTrackId, aStreamId, aSecondsAbsolute);
}

TBool PipelineManager::Next()
{
    AutoMutex _(iPublicLock);
    if (iMode.Bytes() == 0) {
        return false; // nothing playing or ready to be played so nothing we can advance relative to
    }
    (void)iFiller->Stop();
    /* Previously tried using iIdManager->InvalidateAt() to invalidate the current track only.
       If we're playing a low res track, there is a large window when we'll be playing that but
       pre-fetching the track to follow it.  InvalidateAt() will fail to clear that following
       track from the pipeline. */
    iIdManager->InvalidateAll();
    return iFiller->Next(iMode);
}

TBool PipelineManager::Prev()
{
    AutoMutex _(iPublicLock);
    if (iMode.Bytes() == 0) {
        return false; // nothing playing or ready to be played so nothing we can advance relative to
    }
    (void)iFiller->Stop();
    iIdManager->InvalidateAll();
    return iFiller->Prev(iMode);
}

TBool PipelineManager::SupportsMimeType(const Brx& aMimeType)
{
    return iPipeline->SupportsMimeType(aMimeType);
}

IPipelineElementDownstream* PipelineManager::SetSender(IPipelineElementDownstream& aSender)
{
    return iPipeline->SetSender(aSender);
}

TUint PipelineManager::SenderMinLatencyMs() const
{
    return iPipeline->SenderMinLatencyMs();
}

Msg* PipelineManager::Pull()
{
    return iPipeline->Pull();
}

void PipelineManager::InvalidateAt(TUint aId)
{
    iIdManager->InvalidateAt(aId);
}

void PipelineManager::InvalidateAfter(TUint aId)
{
    iIdManager->InvalidateAfter(aId);
}

void PipelineManager::InvalidatePending()
{
    iIdManager->InvalidatePending();
}

void PipelineManager::InvalidateAll()
{
    iIdManager->InvalidateAll();
}

void PipelineManager::NotifyPipelineState(EPipelineState aState)
{
    for (TUint i=0; i<iObservers.size(); i++) {
        iObservers[i]->NotifyPipelineState(aState);
    }
    iLock.Wait();
    iPipelineState = aState;
    iLock.Signal();
    if (iPipelineState == EPipelineStopped) {
        iPipelineStoppedSem.Signal();
    }
    else {
        (void)iPipelineStoppedSem.Clear();
    }
}

void PipelineManager::NotifyTrack(Track& aTrack, const Brx& aMode, TUint aIdPipeline)
{
    iLock.Wait();
    iMode.Replace(aMode);
    iTrackId = aTrack.Id();
    iLock.Signal();
    for (TUint i=0; i<iObservers.size(); i++) {
        iObservers[i]->NotifyTrack(aTrack, aMode, aIdPipeline);
    }
}

void PipelineManager::NotifyMetaText(const Brx& aText)
{
    for (TUint i=0; i<iObservers.size(); i++) {
        iObservers[i]->NotifyMetaText(aText);
    }
}

void PipelineManager::NotifyTime(TUint aSeconds, TUint aTrackDurationSeconds)
{
    for (TUint i=0; i<iObservers.size(); i++) {
        iObservers[i]->NotifyTime(aSeconds, aTrackDurationSeconds);
    }
}

void PipelineManager::NotifyStreamInfo(const DecodedStreamInfo& aStreamInfo)
{
    for (TUint i=0; i<iObservers.size(); i++) {
        iObservers[i]->NotifyStreamInfo(aStreamInfo);
    }
}


// PipelineManager::PrefetchObserver

PipelineManager::PrefetchObserver::PrefetchObserver()
    : iLock("PFO1")
    , iSem("PFO2", 0)
    , iTrackId(UINT_MAX)
{
}

PipelineManager::PrefetchObserver::~PrefetchObserver()
{
    ASSERT(iTrackId == UINT_MAX);
}

void PipelineManager::PrefetchObserver::Quit()
{
    iTrackId = UINT_MAX;
    iSem.Signal();
}

void PipelineManager::PrefetchObserver::SetTrack(TUint aTrackId)
{
    iLock.Wait();
    (void)iSem.Clear();
    iTrackId = aTrackId;
    iLock.Signal();
}

void PipelineManager::PrefetchObserver::Wait(TUint aTimeoutMs)
{
    iSem.Wait(aTimeoutMs);
}

void PipelineManager::PrefetchObserver::NotifyTrackFailed(TUint aTrackId)
{
    CheckTrack(aTrackId);
}

void PipelineManager::PrefetchObserver::NotifyStreamPlayStatus(TUint aTrackId, TUint /*aStreamId*/, EStreamPlay /*aStatus*/)
{
    CheckTrack(aTrackId);
}

void PipelineManager::PrefetchObserver::CheckTrack(TUint aTrackId)
{
    iLock.Wait();
    if (iTrackId != UINT_MAX) {
        LOG(kMedia, "PipelineManager::PrefetchObserver::CheckTrack expected %u, got %u\n", iTrackId, aTrackId);
    }
    if (aTrackId == iTrackId) {
        iSem.Signal();
        iTrackId = UINT_MAX;
    }
    iLock.Signal();
}
