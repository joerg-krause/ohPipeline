#include <OpenHome/Media/Pipeline/Stopper.h>
#include <OpenHome/Types.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/Printer.h>
#include <OpenHome/Media/Pipeline/Msg.h>
#include <OpenHome/Media/Debug.h>

using namespace OpenHome;
using namespace OpenHome::Media;

Stopper::Stopper(MsgFactory& aMsgFactory, IPipelineElementUpstream& aUpstreamElement, IStopperObserver& aObserver, TUint aRampDuration)
    : iMsgFactory(aMsgFactory)
    , iUpstreamElement(aUpstreamElement)
    , iObserver(aObserver)
    , iLock("STP1")
    , iSem("STP2", 0)
    , iStreamPlayObserver(NULL)
    , iRampDuration(aRampDuration)
    , iTargetHaltId(MsgHalt::kIdInvalid)
    , iTrackId(0)
    , iStreamId(IPipelineIdProvider::kStreamIdInvalid)
    , iStreamHandler(NULL)
    , iBuffering(false)
    , iQuit(false)
{
    iState = EStopped;
    NewStream();
    iCheckedStreamPlayable = true; // override setting from NewStream() - we don't want to call OkToPlay() when we see a first MsgTrack
}

Stopper::~Stopper()
{
}

void Stopper::SetStreamPlayObserver(IStreamPlayObserver& aObserver)
{
    iStreamPlayObserver = &aObserver;
}

void Stopper::Play()
{
    AutoMutex a(iLock);
    LOG(kPipeline, "Stopper::Play(), iState=%s\n", State());
    switch (iState)
    {
    case ERunning:
        break;
    case ERampingDown:
        SetState(ERampingUp);
        iRemainingRampSize = iRampDuration - iRemainingRampSize;
        // don't change iCurrentRampValue - just start ramp up from whatever value it is already at
        break;
    case ERampingUp:
        // We're already starting.  No Benefit in allowing another Play request to interrupt this.
        break;
    case EPaused:
        SetState(ERampingUp);
        iRemainingRampSize = iRampDuration;
        iSem.Signal();
        break;
    case EStopped:
        SetState(ERunning);
        iSem.Signal();
        break;
    case EFlushing:
        break;
    }
    iTargetHaltId = MsgHalt::kIdInvalid;
    iObserver.PipelinePlaying();
}

void Stopper::BeginPause()
{
    AutoMutex a(iLock);
    LOG(kPipeline, "Stopper::BeginPause(), iState=%s\n", State());
    if (iQuit) {
        return;
    }

    if (iBuffering) {
        HandlePaused();
        return;
    }

    switch (iState)
    {
    case ERunning:
        iRemainingRampSize = iRampDuration;
        iCurrentRampValue = Ramp::kMax;
        SetState(ERampingDown);
        break;
    case ERampingDown:
        // We're already pausing.  No Benefit in allowing another Pause request to interrupt this.
        return;
    case ERampingUp:
        iRemainingRampSize = iRampDuration - iRemainingRampSize;
        // don't change iCurrentRampValue - just start ramp down from whatever value it is already at
        SetState(ERampingDown);
        break;
    case EPaused:
    case EStopped:
        return;
    case EFlushing:
        HandleStopped();
        break;
    }
}

void Stopper::BeginStop(TUint aHaltId)
{
    AutoMutex a(iLock);
    LOG(kPipeline, "Stopper::BeginStop(%u), iState=%s\n", aHaltId, State());
    if (iQuit) {
        return;
    }

    iTargetHaltId = aHaltId;
    if (iBuffering) {
        HandleStopped();
        return;
    }

    switch (iState)
    {
    case ERunning:
        iRemainingRampSize = iRampDuration;
        iCurrentRampValue = Ramp::kMax;
        SetState(ERampingDown);
        break;
    case ERampingDown:
        break;
    case ERampingUp:
        iRemainingRampSize = iRampDuration - iRemainingRampSize;
        // don't change iCurrentRampValue - just start ramp down from whatever value it is already at
        SetState(ERampingDown);
        break;
    case EPaused:
        // restart pulling, discarding data until a new stream or our target MsgHalt
        iSem.Signal();
        iFlushStream = true;
        break;
    case EStopped:
        return;
    case EFlushing:
        HandleStopped();
        break;
    }
}

void Stopper::StopNow()
{
    iLock.Wait();
    HandleStopped();
    iLock.Signal();
}

void Stopper::Quit()
{
    iQuit = true;
    if (iState == EStopped || iState == EPaused) {
        iFlushStream = true;
    }
    Play();
}

Msg* Stopper::Pull()
{
    Msg* msg;
    do {
        if (iHaltPending) {
            msg = iMsgFactory.CreateMsgHalt();
            iHaltPending = false;
        }
        else {
            if (iState == EPaused || iState == EStopped) {
                LOG(kPipeline, "Stopper::Pull(), waiting, iState=%s\n", State());
                iSem.Wait();
            }
            msg = (iQueue.IsEmpty()? iUpstreamElement.Pull() : iQueue.Dequeue());
            iLock.Wait();
            msg = msg->Process(*this);
            iLock.Signal();
        }
    } while (msg == NULL);
    iLock.Wait();
    iBuffering = false;
    iLock.Signal();
    return msg;
}

Msg* Stopper::ProcessMsg(MsgMode* aMsg)
{
    return aMsg;
}

Msg* Stopper::ProcessMsg(MsgSession* aMsg)
{
    return aMsg;
}

Msg* Stopper::ProcessMsg(MsgTrack* aMsg)
{
    /* IdManager expects OkToPlay to be called for every stream that is added to it.
       This isn't the case if CodecController fails to recognise the format of a stream.
       Catch this here by using iCheckedStreamPlayable to spot when we haven't tried to
       play a stream. */
    if (aMsg->StartOfStream()) {
        if (!iCheckedStreamPlayable) {
            if (iStreamHandler != NULL) {
                OkToPlay();
            }
            else if (iStreamPlayObserver != NULL) {
                iStreamPlayObserver->NotifyTrackFailed(iTrackId);
                iCheckedStreamPlayable = true;
            }
        }
        NewStream();
    }

    iTrackId = aMsg->Track().Id();
    return aMsg;
}

Msg* Stopper::ProcessMsg(MsgDelay* aMsg)
{
    return aMsg;
}

Msg* Stopper::ProcessMsg(MsgEncodedStream* aMsg)
{
    /* IdManager expects OkToPlay to be called for every stream that is added to it.
       This isn't the case if CodecController fails to recognise the format of a stream.
       Catch this here by using iCheckedStreamPlayable to spot when we haven't tried to
       play a stream. */
    if (!iCheckedStreamPlayable && iStreamHandler != NULL) {
        OkToPlay();
    }

    NewStream();
    iStreamId = aMsg->StreamId();
    iStreamHandler = aMsg->StreamHandler();
    if (aMsg->Live()) {
        /* we won't receive MsgDecodedStream (or anything else) until we call OkToPlay
           Don't want to do this unconditionally, as waiting for MsgDecodedStream for
           non-live streams allows additional metadata to make it to the Reporter before
           we risk the pipeline stalling when response to OkToPlay is eLater. */
        OkToPlay();
    }
    aMsg->RemoveRef();
    return NULL;
}

Msg* Stopper::ProcessMsg(MsgAudioEncoded* /*aMsg*/)
{
    ASSERTS();
    return NULL;
}

Msg* Stopper::ProcessMsg(MsgMetaText* aMsg)
{
    return ProcessFlushable(aMsg);
}

Msg* Stopper::ProcessMsg(MsgHalt* aMsg)
{
    if (iTargetHaltId == aMsg->Id()) {
        iTargetHaltId = MsgHalt::kIdInvalid;
        HandleStopped();
    }
    return aMsg;
}

Msg* Stopper::ProcessMsg(MsgFlush* aMsg)
{
    aMsg->RemoveRef();
    return NULL;
}

Msg* Stopper::ProcessMsg(MsgWait* aMsg)
{
    return aMsg;
}

Msg* Stopper::ProcessMsg(MsgDecodedStream* aMsg)
{
    if (!aMsg->StreamInfo().Live() && !iCheckedStreamPlayable) {
        OkToPlay();
    }
    Msg* msg = ProcessFlushable(aMsg);
    if (msg != NULL) {
        const DecodedStreamInfo& stream = aMsg->StreamInfo();
        msg = iMsgFactory.CreateMsgDecodedStream(stream.StreamId(), stream.BitRate(), stream.BitDepth(),
                                                 stream.SampleRate(), stream.NumChannels(), stream.CodecName(), 
                                                 stream.TrackLength(), stream.SampleStart(), stream.Lossless(), 
                                                 stream.Seekable(), stream.Live(), this);
        aMsg->RemoveRef();
    }
    return msg;
}

Msg* Stopper::ProcessMsg(MsgAudioPcm* aMsg)
{
    if (iState == ERampingDown || iState == ERampingUp) {
        MsgAudio* split;
        if (aMsg->Jiffies() > iRemainingRampSize && iRemainingRampSize > 0) {
            split = aMsg->Split(iRemainingRampSize);
            if (split != NULL) {
                iQueue.EnqueueAtHead(split);
            }
        }
        split = NULL;
        const Ramp::EDirection direction = (iState == ERampingDown? Ramp::EDown : Ramp::EUp);
        if (iRemainingRampSize > 0) {
            iCurrentRampValue = aMsg->SetRamp(iCurrentRampValue, iRemainingRampSize, direction, split);
        }
        if (split != NULL) {
            iQueue.EnqueueAtHead(split);
        }
        if (iRemainingRampSize == 0) {
            RampCompleted();
        }
        return aMsg;
    }

    return ProcessFlushable(aMsg);
}

Msg* Stopper::ProcessMsg(MsgSilence* aMsg)
{
    if (iState == ERampingDown || iState == ERampingUp) {
        RampCompleted();
    }
    return ProcessFlushable(aMsg);
}

Msg* Stopper::ProcessMsg(MsgPlayable* /*aMsg*/)
{
    ASSERTS();
    return NULL;
}

Msg* Stopper::ProcessMsg(MsgQuit* aMsg)
{
    if (iStreamHandler != NULL) {
        iStreamHandler->TryStop(iStreamId);
    }
    return aMsg;
}

EStreamPlay Stopper::OkToPlay(TUint /*aStreamId*/)
{
    ASSERTS();
    return ePlayNo;
}

TUint Stopper::TrySeek(TUint /*aStreamId*/, TUint64 /*aOffset*/)
{
    ASSERTS();
    return MsgFlush::kIdInvalid;
}

TUint Stopper::TryStop(TUint /*aStreamId*/)
{
    ASSERTS();
    return MsgFlush::kIdInvalid;
}

void Stopper::NotifyStarving(const Brx& aMode, TUint aStreamId)
{
    iLock.Wait();
    if (iState != ERampingDown) {
        iBuffering = true;
    }
    else {
        if (iTargetHaltId == MsgHalt::kIdInvalid) {
            HandlePaused();
        }
        else {
            HandleStopped();
        }
    }
    if (iStreamHandler != NULL) {
        iStreamHandler->NotifyStarving(aMode, aStreamId);
    }
    iLock.Signal();
}

Msg* Stopper::ProcessFlushable(Msg* aMsg)
{
    if (iFlushStream) {
        aMsg->RemoveRef();
        return NULL;
    }
    return aMsg;
}

void Stopper::OkToPlay()
{
    ASSERT(iStreamHandler != NULL);
    EStreamPlay canPlay = iStreamHandler->OkToPlay(iStreamId);
    if (iQuit) {
        SetState(EFlushing);
        iFlushStream = true;
    }
    else {
        LOG(kPipeline, "Stopper - OkToPlay returned %s.  trackId=%u, streamId=%u.\n", kStreamPlayNames[canPlay], iTrackId, iStreamId);

        switch (canPlay)
        {
        case ePlayYes:
            iObserver.PipelinePlaying();
            break;
        case ePlayNo:
            /*TUint flushId = */iStreamHandler->TryStop(iStreamId);
            SetState(EFlushing);
            iFlushStream = true;
            iHaltPending = true;
            break;
        case ePlayLater:
            HandleStopped();
            iHaltPending = true;
            break;
        default:
            ASSERTS();
        }
    }
    if (iStreamPlayObserver != NULL) {
        iStreamPlayObserver->NotifyStreamPlayStatus(iTrackId, iStreamId, canPlay);
    }
    iCheckedStreamPlayable = true;
}

void Stopper::RampCompleted()
{
    if (iState == ERampingDown) {
        if (iTargetHaltId == MsgHalt::kIdInvalid) {
            HandlePaused();
        }
        else {
            ASSERT(iStreamHandler != NULL);
            (void)iStreamHandler->TryStop(iStreamId);
            SetState(ERunning);
            iFlushStream = true;
        }
        iHaltPending = true;
    }
    else { // iState == ERampingUp
        SetState(ERunning);
    }
}

void Stopper::NewStream()
{
    iRemainingRampSize = 0;
    iCurrentRampValue = Ramp::kMax;
    SetState(ERunning);
    iStreamHandler = NULL;
    iCheckedStreamPlayable = false;
    iHaltPending = false;
    iFlushStream = false;
}

void Stopper::HandlePaused()
{
    SetState(EPaused);
    (void)iSem.Clear();
    iObserver.PipelinePaused();
}

void Stopper::HandleStopped()
{
    SetState(EStopped);
    (void)iSem.Clear();
    iObserver.PipelineStopped();
}

void Stopper::SetState(EState aState)
{
    LOG(kPipeline, "Stopper changing state from %s to %s\n", State(), State(aState));
    LOG(kPipeline, "  iRemainingRampSize=%u, iCurrentRampValue=%08x\n", iRemainingRampSize, iCurrentRampValue);
    iState = aState;
}

const TChar* Stopper::State() const
{
    return State(iState);
}

const TChar* Stopper::State(EState aState)
{ // static
    const TChar* state = NULL;
    switch (aState)
    {
    case ERunning:
        state = "Running";
        break;
    case ERampingDown:
        state = "RampingDown";
        break;
    case ERampingUp:
        state = "RampingUp";
        break;
    case EPaused:
        state = "Paused";
        break;
    case EStopped:
        state = "Stopped";
        break;
    case EFlushing:
        state = "Flushing";
        break;
    default:
        ASSERTS();
    }
    return state;
}
