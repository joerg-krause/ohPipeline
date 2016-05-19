#include <OpenHome/Private/TestFramework.h>
#include <OpenHome/Private/SuiteUnitTest.h>
#include <OpenHome/Media/Pipeline/AudioReservoir.h>
#include <OpenHome/Media/Pipeline/DecodedAudioReservoir.h>
#include <OpenHome/Media/Pipeline/EncodedAudioReservoir.h>
#include <OpenHome/Media/Pipeline/Msg.h>
#include <OpenHome/Media/InfoProvider.h>
#include <OpenHome/Media/Utils/AllocatorInfoLogger.h>
#include <OpenHome/Media/Utils/ProcessorPcmUtils.h>
#include <OpenHome/Media/ClockPuller.h>
#include <OpenHome/Functor.h>

#include <string.h>
#include <vector>
#include <list>

using namespace OpenHome;
using namespace OpenHome::TestFramework;
using namespace OpenHome::Media;

namespace OpenHome {
namespace Media {

class SuiteAudioReservoir : public Suite, private IMsgProcessor
{
    static const TUint kDecodedAudioCount = 512;
    static const TUint kMsgAudioPcmCount  = 512;
    static const TUint kMsgSilenceCount   = 1;

    static const TUint kReservoirSize = Jiffies::kPerMs * 500;
    static const TUint kMaxStreams = 5;

    static const TUint kSampleRate  = 44100;
    static const TUint kNumChannels = 2;
public:
    SuiteAudioReservoir();
    ~SuiteAudioReservoir();
    void Test() override;
private: // from IMsgProcessor
    Msg* ProcessMsg(MsgMode* aMsg) override;
    Msg* ProcessMsg(MsgTrack* aMsg) override;
    Msg* ProcessMsg(MsgDrain* aMsg) override;
    Msg* ProcessMsg(MsgDelay* aMsg) override;
    Msg* ProcessMsg(MsgEncodedStream* aMsg) override;
    Msg* ProcessMsg(MsgAudioEncoded* aMsg) override;
    Msg* ProcessMsg(MsgMetaText* aMsg) override;
    Msg* ProcessMsg(MsgStreamInterrupted* aMsg) override;
    Msg* ProcessMsg(MsgHalt* aMsg) override;
    Msg* ProcessMsg(MsgFlush* aMsg) override;
    Msg* ProcessMsg(MsgWait* aMsg) override;
    Msg* ProcessMsg(MsgDecodedStream* aMsg) override;
    Msg* ProcessMsg(MsgBitRate* aMsg) override;
    Msg* ProcessMsg(MsgAudioPcm* aMsg) override;
    Msg* ProcessMsg(MsgSilence* aMsg) override;
    Msg* ProcessMsg(MsgPlayable* aMsg) override;
    Msg* ProcessMsg(MsgQuit* aMsg) override;
private:
    enum EMsgType
    {
        ENone
       ,EMsgAudioPcm
       ,EMsgPlayable
       ,EMsgDecodedStream
       ,EMsgBitRate
       ,EMsgMode
       ,EMsgTrack
       ,EMsgDrain
       ,EMsgDelay
       ,EMsgEncodedStream
       ,EMsgMetaText
       ,EMsgStreamInterrupted
       ,EMsgHalt
       ,EMsgFlush
       ,EMsgWait
       ,EMsgQuit
    };
enum EMsgGenerationState
{
    EGenerateSingle
   ,EFillReservoir
   ,EExit
};
private:
    void GenerateMsg(EMsgType aType);
    void GenerateMsgs(EMsgType aType);
    void Generate(EMsgGenerationState aState, EMsgType aType);
    void MsgEnqueueThread();
    TBool EnqueueMsg(EMsgType aType);
    MsgAudio* CreateAudio();
    TBool ReservoirIsFull() const { return static_cast<AudioReservoir*>(iReservoir)->IsFull(); }
private:
    MsgFactory* iMsgFactory;
    TrackFactory* iTrackFactory;
    AllocatorInfoLogger iInfoAggregator;
    DecodedAudioReservoir* iReservoir;
    ThreadFunctor* iThread;
    EMsgGenerationState iMsgGenerationState;
    EMsgType iNextGeneratedMsg;
    EMsgType iLastMsg;
    Semaphore iSemUpstream;
    Semaphore iSemUpstreamComplete;
    TUint64 iTrackOffset;
};

class SuiteReservoirHistory : public SuiteUnitTest, private IMsgProcessor, private IClockPullerReservoir
{
    static const TUint kSampleRate  = 44100;
    static const TUint kNumChannels = 2;
    static const TUint kBitDepth = 16;
    static const TUint kReservoirSize = Jiffies::kPerMs * 1000;
    static const TUint kMaxStreams = 5;
public:
    SuiteReservoirHistory();
    ~SuiteReservoirHistory();
private: // from SuiteUnitTest
    void Setup() override;
    void TearDown() override;
private:
    void TestNotifySizeNotCalledUntilStarted();
    void TestNotifySizeNotCalledAfterDownstreamStop();
    void TestNotifySizeNotCalledAfterDrain();
    void TestNotifySizeCalledTwicePerAudioMsg();
private:
    MsgMode* CreateMsgMode();
    MsgTrack* CreateMsgTrack();
    MsgDecodedStream* CreateMsgDecodedStream();
    MsgAudioPcm* CreateMsgAudioPcm();
    void WaitForReservoirToEmpty();
    void PullerThread();
private: // from IMsgProcessor
    Msg* ProcessMsg(MsgMode* aMsg) override;
    Msg* ProcessMsg(MsgTrack* aMsg) override;
    Msg* ProcessMsg(MsgDrain* aMsg) override;
    Msg* ProcessMsg(MsgDelay* aMsg) override;
    Msg* ProcessMsg(MsgEncodedStream* aMsg) override;
    Msg* ProcessMsg(MsgAudioEncoded* aMsg) override;
    Msg* ProcessMsg(MsgMetaText* aMsg) override;
    Msg* ProcessMsg(MsgStreamInterrupted* aMsg) override;
    Msg* ProcessMsg(MsgHalt* aMsg) override;
    Msg* ProcessMsg(MsgFlush* aMsg) override;
    Msg* ProcessMsg(MsgWait* aMsg) override;
    Msg* ProcessMsg(MsgDecodedStream* aMsg) override;
    Msg* ProcessMsg(MsgBitRate* aMsg) override;
    Msg* ProcessMsg(MsgAudioPcm* aMsg) override;
    Msg* ProcessMsg(MsgSilence* aMsg) override;
    Msg* ProcessMsg(MsgPlayable* aMsg) override;
    Msg* ProcessMsg(MsgQuit* aMsg) override;
private: // from IClockPullerReservoir
    void Start(TUint aExpectedDecodedReservoirJiffies) override;
    void Stop() override;
    void Reset() override;
    void NotifySize(TUint aJiffies) override;
private:
    MsgFactory* iMsgFactory;
    TrackFactory* iTrackFactory;
    AllocatorInfoLogger iInfoAggregator;
    DecodedAudioReservoir* iReservoir;
    ThreadFunctor* iThread;
    TBool iStopPullerThread;
    TByte iBuf[DecodedAudio::kMaxBytes];
    TUint64 iTrackOffset;
    TUint iStartCount;
    TUint iStopCount;
    TUint iNotifySizeCount;
    IClockPullerReservoir* iClockPuller;
};

class SuiteEncodedReservoir : public SuiteUnitTest, private IStreamHandler, private IMsgProcessor, private IFlushIdProvider
{
    static const TUint kTrySeekResponse = 42;
    static const TUint kStreamId = 5;
public:
    SuiteEncodedReservoir();
private: // from SuiteUnitTest
    void Setup() override;
    void TearDown() override;
private: // from IStreamHandler
    EStreamPlay OkToPlay(TUint aStreamId) override;
    TUint TrySeek(TUint aStreamId, TUint64 aOffset) override;
    TUint TryStop(TUint aStreamId) override;
    void NotifyStarving(const Brx& aMode, TUint aStreamId, TBool aStarving) override;
private: // from IMsgProcessor
    Msg* ProcessMsg(MsgMode* aMsg) override;
    Msg* ProcessMsg(MsgTrack* aMsg) override;
    Msg* ProcessMsg(MsgDrain* aMsg) override;
    Msg* ProcessMsg(MsgDelay* aMsg) override;
    Msg* ProcessMsg(MsgEncodedStream* aMsg) override;
    Msg* ProcessMsg(MsgAudioEncoded* aMsg) override;
    Msg* ProcessMsg(MsgMetaText* aMsg) override;
    Msg* ProcessMsg(MsgStreamInterrupted* aMsg) override;
    Msg* ProcessMsg(MsgHalt* aMsg) override;
    Msg* ProcessMsg(MsgFlush* aMsg) override;
    Msg* ProcessMsg(MsgWait* aMsg) override;
    Msg* ProcessMsg(MsgDecodedStream* aMsg) override;
    Msg* ProcessMsg(MsgBitRate* aMsg) override;
    Msg* ProcessMsg(MsgAudioPcm* aMsg) override;
    Msg* ProcessMsg(MsgSilence* aMsg) override;
    Msg* ProcessMsg(MsgPlayable* aMsg) override;
    Msg* ProcessMsg(MsgQuit* aMsg) override;
private: // from IFlushIdProvider
    TUint NextFlushId() override;
private:
    enum EMsgType
    {
        ENone
       ,EMsgTrack
       ,EMsgEncodedStream
       ,EMsgAudioEncoded
       ,EMsgFlush
       ,EMsgQuit
    };
private:
    void PullNext(EMsgType aType);
    void PushEncodedStream();
    void PushEncodedAudio(TByte aFill);
private:
    void TestStreamHandlerCallsPassedOn();
    void TestSeekBackwards();
    void TestSeekForwardsIntoReservoir();
    void TestSeekForwardsBeyondReservoir();
    void TestNewStreamInterruptsSeek();
private:
    MsgFactory* iMsgFactory;
    TrackFactory* iTrackFactory;
    AllocatorInfoLogger iInfoAggregator;
    EncodedAudioReservoir* iReservoir;
    TUint iNextFlushId;
    EMsgType iLastMsg;
    TUint iOkToPlayCount;
    TUint iTrySeekCount;
    TUint iTryStopCount;
    TUint iNotifyStarvingCount;
    TUint iPulledFlushId;
    TUint iEncAudioFill;
    TByte iAudioSrc[EncodedAudio::kMaxBytes];
    TByte iAudioDest[EncodedAudio::kMaxBytes];
};

} // namespace Media
} // namespace OpenHome


// SuiteAudioReservoir

SuiteAudioReservoir::SuiteAudioReservoir()
    : Suite("Decoded Audio Reservoir tests")
    , iLastMsg(ENone)
    , iSemUpstream("TRSV", 0)
    , iSemUpstreamComplete("TRSV", 0)
    , iTrackOffset(0)
{
    MsgFactoryInitParams init;
    init.SetMsgAudioPcmCount(kMsgAudioPcmCount, kDecodedAudioCount);
    init.SetMsgSilenceCount(kMsgSilenceCount);
    init.SetMsgDecodedStreamCount(kMaxStreams+2);
    init.SetMsgModeCount(2);
    iMsgFactory = new MsgFactory(iInfoAggregator, init);
    iTrackFactory = new TrackFactory(iInfoAggregator, 1);
    iReservoir = new DecodedAudioReservoir(*iMsgFactory, kReservoirSize, kMaxStreams);
    iThread = new ThreadFunctor("TEST", MakeFunctor(*this, &SuiteAudioReservoir::MsgEnqueueThread));
    iThread->Start();
    iSemUpstreamComplete.Wait();
}

SuiteAudioReservoir::~SuiteAudioReservoir()
{
    iMsgGenerationState = EExit;
    iSemUpstream.Signal();
    delete iThread;
    delete iReservoir;
    delete iMsgFactory;
    delete iTrackFactory;
}

void SuiteAudioReservoir::Test()
{
    /*
    Test goes something like
        Add single 0xff filled audio.  Check it can be Pull()ed.
        Check that Silence, Track, MetaText, Quit & Halt msgs are passed through.
        Add audio until we exceed MaxSize.  Check adding thread is blocked.
        Pull single audio msg.  Add Flush; check that next msg Pull()ed is the Flush and that reservoir is now empty.
    */

    // Add single 0xff filled audio.  Check it can be Pull()ed.
    TEST(iLastMsg == ENone);
    GenerateMsg(EMsgAudioPcm);
    iSemUpstreamComplete.Wait();
    Msg* msg = iReservoir->Pull();
    msg = msg->Process(*this);
    TEST(iLastMsg == EMsgAudioPcm);
    ASSERT(msg == nullptr);

    // Check that uninteresting msgs are passed through.
    EMsgType types[] = { EMsgDecodedStream, EMsgBitRate, EMsgMode,
                         EMsgTrack, EMsgDrain, EMsgDelay, EMsgEncodedStream,
                         EMsgMetaText, EMsgStreamInterrupted, EMsgFlush, EMsgWait,
                         EMsgHalt, EMsgQuit };
    for (TUint i=0; i<sizeof(types)/sizeof(types[0]); i++) {
        EMsgType msgType = types[i];
        GenerateMsg(msgType);
        iSemUpstreamComplete.Wait();
        msg = iReservoir->Pull();
        msg = msg->Process(*this);
        msg->RemoveRef();
        TEST(iLastMsg == msgType);
    }

    // Add audio until we exceed MaxSize.  Check adding thread is blocked.
    GenerateMsgs(EMsgAudioPcm);
    while (iReservoir->Jiffies() < kReservoirSize) {
        Thread::Sleep(10);
    }
    TUint jiffies = iReservoir->Jiffies();
    // lazy check that Enqueue has blocked
    // ...sleep for a while then check size of reservoir is unchanged
    Thread::Sleep(25);
    TEST(iReservoir->Jiffies() == jiffies);

    // Pull single msg to unblock iThread
    msg = iReservoir->Pull();
    msg = msg->Process(*this);
    ASSERT(msg == nullptr);
    jiffies = iReservoir->Jiffies();

    // Keep adding DecodedStream until Enqueue blocks
    for (TUint i=0; i<kMaxStreams-1; i++) {
        GenerateMsg(EMsgDecodedStream);
        iSemUpstreamComplete.Wait();
    }
    TEST(!ReservoirIsFull());
    GenerateMsg(EMsgDecodedStream);
    while (!ReservoirIsFull()) {
        Thread::Sleep(1);
    }
    TEST(ReservoirIsFull());
    TEST(iReservoir->Jiffies() == jiffies);
    do {
        TEST(ReservoirIsFull());
        msg = iReservoir->Pull();
        msg = msg->Process(*this);
    } while (iLastMsg == EMsgAudioPcm);
    TEST(iLastMsg == EMsgDecodedStream);
    msg->RemoveRef();
    TEST(!ReservoirIsFull());
}

void SuiteAudioReservoir::GenerateMsg(EMsgType aType)
{
    Generate(EGenerateSingle, aType);
}

void SuiteAudioReservoir::GenerateMsgs(EMsgType aType)
{
    Generate(EFillReservoir, aType);
}

void SuiteAudioReservoir::Generate(EMsgGenerationState aState, EMsgType aType)
{
    iMsgGenerationState = aState;
    iNextGeneratedMsg = aType;
    iSemUpstream.Signal();
}

void SuiteAudioReservoir::MsgEnqueueThread()
{
    for (;;) {
        iSemUpstreamComplete.Signal();
        iSemUpstream.Wait();
        switch (iMsgGenerationState)
        {
        case EGenerateSingle:
            EnqueueMsg(iNextGeneratedMsg);
            break;
        case EFillReservoir:
            while (!EnqueueMsg(iNextGeneratedMsg)) {
            }
            break;
        case EExit:
            return;
        }
    }
}

TBool SuiteAudioReservoir::EnqueueMsg(EMsgType aType)
{
    TBool shouldBlock = false;
    Msg* msg = nullptr;
    switch (aType)
    {
    default:
    case ENone:
    case EMsgPlayable:
        ASSERTS();
        break;
    case EMsgAudioPcm:
    {
        MsgAudio* audio = CreateAudio();
        shouldBlock = (iReservoir->SizeInJiffies() + audio->Jiffies() >= kReservoirSize);
        msg = audio;
        break;
    }
    case EMsgDecodedStream:
        msg = iMsgFactory->CreateMsgDecodedStream(0, 0, 0, 0, 0, Brx::Empty(), 0, 0, false, false, false, false, nullptr);
        break;
    case EMsgBitRate:
        msg = iMsgFactory->CreateMsgBitRate(1);
        break;
    case EMsgMode:
        msg = iMsgFactory->CreateMsgMode(Brx::Empty(), true, true, ModeClockPullers(), false, false);
        break;
    case EMsgTrack:
    {
        Track* track = iTrackFactory->CreateTrack(Brx::Empty(), Brx::Empty());
        msg = iMsgFactory->CreateMsgTrack(*track);
        track->RemoveRef();
    }
        break;
    case EMsgDrain:
        msg = iMsgFactory->CreateMsgDrain(Functor());
        break;
    case EMsgDelay:
        msg = iMsgFactory->CreateMsgDelay(Jiffies::kPerMs * 5);
        break;
    case EMsgEncodedStream:
        msg = iMsgFactory->CreateMsgEncodedStream(Brn("http://127.0.0.1:65535"), Brn("metatext"), 0, 0, 0, false, false, nullptr);
        break;
    case EMsgMetaText:
        msg = iMsgFactory->CreateMsgMetaText(Brn("metatext"));
        break;
    case EMsgStreamInterrupted:
        msg = iMsgFactory->CreateMsgStreamInterrupted();
        break;
    case EMsgHalt:
        msg = iMsgFactory->CreateMsgHalt();
        break;
    case EMsgFlush:
        msg = iMsgFactory->CreateMsgFlush(1);
        break;
    case EMsgWait:
        msg = iMsgFactory->CreateMsgWait();
        break;
    case EMsgQuit:
        msg = iMsgFactory->CreateMsgQuit();
        break;
    }
    iReservoir->Push(msg);
    return shouldBlock;
}

MsgAudio* SuiteAudioReservoir::CreateAudio()
{
    static const TUint kDataBytes = 3 * 1024;
    TByte encodedAudioData[kDataBytes];
    (void)memset(encodedAudioData, 0xff, kDataBytes);
    Brn encodedAudioBuf(encodedAudioData, kDataBytes);
    MsgAudioPcm* audio = iMsgFactory->CreateMsgAudioPcm(encodedAudioBuf, kNumChannels, kSampleRate, 16, AudioDataEndian::Little, iTrackOffset);
    iTrackOffset += audio->Jiffies();
    return audio;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgMode* aMsg)
{
    iLastMsg = EMsgMode;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgTrack* aMsg)
{
    iLastMsg = EMsgTrack;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgDrain* aMsg)
{
    iLastMsg = EMsgDrain;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgDelay* aMsg)
{
    iLastMsg = EMsgDelay;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgEncodedStream* aMsg)
{
    iLastMsg = EMsgEncodedStream;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgAudioEncoded* /*aMsg*/)
{
    ASSERTS(); /* only expect to deal with decoded audio at this stage of the pipeline */
    return nullptr;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgMetaText* aMsg)
{
    iLastMsg = EMsgMetaText;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgStreamInterrupted* aMsg)
{
    iLastMsg = EMsgStreamInterrupted;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgHalt* aMsg)
{
    iLastMsg = EMsgHalt;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgFlush* aMsg)
{
    iLastMsg = EMsgFlush;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgWait* aMsg)
{
    iLastMsg = EMsgWait;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgDecodedStream* aMsg)
{
    iLastMsg = EMsgDecodedStream;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgBitRate* aMsg)
{
    iLastMsg = EMsgBitRate;
    return aMsg;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgAudioPcm* aMsg)
{
    iLastMsg = EMsgAudioPcm;
    MsgPlayable* playable = aMsg->CreatePlayable();
    ProcessorPcmBufTest pcmProcessor;
    playable->Read(pcmProcessor);
    Brn buf(pcmProcessor.Buf());
//    playable->RemoveRef();
    const TByte* ptr = buf.Ptr();
    const TInt firstSubsample = (ptr[0]<<8) | ptr[1];
    const TUint bytes = buf.Bytes();
    const TInt lastSubsample = (ptr[bytes-2]<<8) | ptr[bytes-1];
    TEST(firstSubsample == 0xffff);
    TEST(firstSubsample == lastSubsample);
    if (firstSubsample != lastSubsample) {
        Print("firstSubsample=%08x, lastSubsample=%08x\n", firstSubsample, lastSubsample);
    }
    playable->RemoveRef();
    return nullptr;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgSilence* /*aMsg*/)
{
    ASSERTS(); // MsgSilence not used in this test
    return nullptr;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgPlayable* /*aMsg*/)
{
    ASSERTS(); // MsgPlayable not used in this test
    return nullptr;
}

Msg* SuiteAudioReservoir::ProcessMsg(MsgQuit* aMsg)
{
   iLastMsg = EMsgQuit;
    return aMsg;
}


// SuiteReservoirHistory

SuiteReservoirHistory::SuiteReservoirHistory()
    : SuiteUnitTest("DecodedReservoir History")
{
    AddTest(MakeFunctor(*this, &SuiteReservoirHistory::TestNotifySizeNotCalledUntilStarted), "TestNotifySizeNotCalledUntilStarted");
    AddTest(MakeFunctor(*this, &SuiteReservoirHistory::TestNotifySizeNotCalledAfterDownstreamStop), "TestNotifySizeNotCalledAfterDownstreamStop");
    AddTest(MakeFunctor(*this, &SuiteReservoirHistory::TestNotifySizeNotCalledAfterDrain), "TestNotifySizeNotCalledAfterDrain");
    AddTest(MakeFunctor(*this, &SuiteReservoirHistory::TestNotifySizeCalledTwicePerAudioMsg), "TestNotifySizeCalledTwicePerAudioMsg");
}

SuiteReservoirHistory::~SuiteReservoirHistory()
{
}

void SuiteReservoirHistory::Setup()
{
    MsgFactoryInitParams init;
    init.SetMsgAudioPcmCount(20, 20);
    init.SetMsgModeCount(2);
    init.SetMsgDecodedStreamCount(2);
    iMsgFactory = new MsgFactory(iInfoAggregator, init);
    iTrackFactory = new TrackFactory(iInfoAggregator, 1);
    iReservoir = new DecodedAudioReservoir(*iMsgFactory, kReservoirSize, kMaxStreams);
    iThread = new ThreadFunctor("RHPT", MakeFunctor(*this, &SuiteReservoirHistory::PullerThread));
    iThread->Start();

    memset(iBuf, 0xff, sizeof(iBuf));
    iStopPullerThread = false;
    iTrackOffset = 0;
    iStartCount = iStopCount = iNotifySizeCount = 0;
    iClockPuller = nullptr;
}

void SuiteReservoirHistory::TearDown()
{
    delete iThread;
    delete iReservoir;
    delete iMsgFactory;
    delete iTrackFactory;
}

void SuiteReservoirHistory::TestNotifySizeNotCalledUntilStarted()
{
    iReservoir->Push(CreateMsgMode());
    iReservoir->Push(CreateMsgTrack());
    iReservoir->Push(CreateMsgDecodedStream());
    iReservoir->Push(CreateMsgAudioPcm());
    TEST(iStartCount == 0);
    TEST(iStopCount == 0);
    WaitForReservoirToEmpty();
    TEST(iStartCount == 0);
    iClockPuller->Start(0);
    iReservoir->Push(CreateMsgAudioPcm());
    WaitForReservoirToEmpty();
    TEST(iStartCount == 1);
    TEST(iNotifySizeCount > 0);
    iReservoir->Push(iMsgFactory->CreateMsgQuit());
}

void SuiteReservoirHistory::TestNotifySizeNotCalledAfterDownstreamStop()
{
    iReservoir->Push(CreateMsgMode());
    iReservoir->Push(CreateMsgTrack());
    iReservoir->Push(CreateMsgDecodedStream());
    WaitForReservoirToEmpty();
    iClockPuller->Start(0);
    iReservoir->Push(CreateMsgAudioPcm());
    WaitForReservoirToEmpty();
    const TUint prevNotifySizeCount = iNotifySizeCount;
    TEST(iNotifySizeCount > 0);
    TEST(iStartCount == 1);
    TEST(iStopCount == 0);

    iClockPuller->Stop();
    TEST(iStopCount == 1);
    TEST(iStartCount == 1);
    iReservoir->Push(CreateMsgAudioPcm());
    iReservoir->Push(CreateMsgAudioPcm());
    iReservoir->Push(CreateMsgAudioPcm());
    WaitForReservoirToEmpty();
    TEST(iStartCount == 1);
    TEST(iStopCount == 1);
    TEST(iNotifySizeCount == prevNotifySizeCount);

    iReservoir->Push(iMsgFactory->CreateMsgQuit());
}

void SuiteReservoirHistory::TestNotifySizeNotCalledAfterDrain()
{
    iReservoir->Push(CreateMsgMode());
    iReservoir->Push(CreateMsgTrack());
    iReservoir->Push(CreateMsgDecodedStream());
    WaitForReservoirToEmpty();
    iClockPuller->Start(0);
    iReservoir->Push(CreateMsgAudioPcm());
    WaitForReservoirToEmpty();
    const TUint prevNotifySizeCount = iNotifySizeCount;
    TEST(iNotifySizeCount > 0);
    TEST(iStartCount == 1);
    TEST(iStopCount == 0);

    iReservoir->Push(iMsgFactory->CreateMsgDrain(Functor()));
    WaitForReservoirToEmpty();
    TEST(iStopCount == 1);
    TEST(iStartCount == 1);
    iReservoir->Push(CreateMsgAudioPcm());
    iReservoir->Push(CreateMsgAudioPcm());
    iReservoir->Push(CreateMsgAudioPcm());
    WaitForReservoirToEmpty();
    TEST(iStopCount == 1);
    TEST(iStartCount == 1);
    TEST(iNotifySizeCount == prevNotifySizeCount);

    iClockPuller->Start(0);
    iReservoir->Push(CreateMsgAudioPcm());
    WaitForReservoirToEmpty();
    TEST(iStopCount == 1);
    TEST(iStartCount == 2);
    TEST(iNotifySizeCount > prevNotifySizeCount);

    iReservoir->Push(iMsgFactory->CreateMsgQuit());
}

void SuiteReservoirHistory::TestNotifySizeCalledTwicePerAudioMsg()
{
    iReservoir->Push(CreateMsgMode());
    iReservoir->Push(CreateMsgTrack());
    iReservoir->Push(CreateMsgDecodedStream());
    iReservoir->Push(CreateMsgAudioPcm());
    WaitForReservoirToEmpty();
    iClockPuller->Start(0);
    TEST(iStartCount == 1);
    TEST(iStopCount == 0);
    TEST(iNotifySizeCount == 0);

    static const TUint kAudioMsgCount = 10;
    for (TUint i=0; i<kAudioMsgCount; i++) {
        iReservoir->Push(CreateMsgAudioPcm());
    }
    WaitForReservoirToEmpty();
    TEST(iStartCount == 1);
    TEST(iStopCount == 0);
    TEST(iNotifySizeCount == kAudioMsgCount * 2);

    iReservoir->Push(iMsgFactory->CreateMsgQuit());
}

MsgMode* SuiteReservoirHistory::CreateMsgMode()
{
    return iMsgFactory->CreateMsgMode(Brn("ClockPullTest"), false, true, ModeClockPullers(this), false, false);
}

MsgTrack* SuiteReservoirHistory::CreateMsgTrack()
{
    auto track = iTrackFactory->CreateTrack(Brx::Empty(), Brx::Empty());
    auto msgTrack = iMsgFactory->CreateMsgTrack(*track);
    track->RemoveRef();
    return msgTrack;
}

MsgDecodedStream* SuiteReservoirHistory::CreateMsgDecodedStream()
{
    return iMsgFactory->CreateMsgDecodedStream(100, 12, 16, 44100, 2, Brn("dummy"), 1LL<<40, 0, false, false, false, false, nullptr);
}

MsgAudioPcm* SuiteReservoirHistory::CreateMsgAudioPcm()
{
    Brn audioBuf(iBuf, sizeof(iBuf));
    auto audio = iMsgFactory->CreateMsgAudioPcm(audioBuf, kNumChannels, kSampleRate, kBitDepth, AudioDataEndian::Little, iTrackOffset);
    iTrackOffset += audio->Jiffies();
    return audio;
}

void SuiteReservoirHistory::WaitForReservoirToEmpty()
{
    while (!iReservoir->IsEmpty()) {
        Thread::Sleep(1);
    }
}

void SuiteReservoirHistory::PullerThread()
{
    while (!iStopPullerThread) {
        Msg* msg = iReservoir->Pull();
        msg = msg->Process(*this);
        msg->RemoveRef();
        Thread::Sleep(1);
    }

    // consume any remaining msgs in case pushing thread is blocked
    while (!iReservoir->IsEmpty()) {
        iReservoir->Pull()->RemoveRef();
    }
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgAudioEncoded* /*aMsg*/)
{
    ASSERTS(); // only MsgAudioPcm and MsgSilence expected in this test
    return nullptr;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgAudioPcm* aMsg)
{
    return aMsg;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgSilence* /*aMsg*/)
{
    ASSERTS(); // don't expect anything upstream of DecodedAudioReservoir to generate silence
    return nullptr;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgPlayable* /*aMsg*/)
{
    ASSERTS(); // only MsgAudioPcm expected in this test
    return nullptr;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgDecodedStream* aMsg)
{
    return aMsg;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgBitRate* aMsg)
{
    return aMsg;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgMode* aMsg)
{
    iClockPuller = aMsg->ClockPullers().ReservoirLeft();
    return aMsg;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgTrack* aMsg)
{
    return aMsg;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgDrain* aMsg)
{
    return aMsg;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgDelay* aMsg)
{
    return aMsg;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgEncodedStream* /*aMsg*/)
{
    ASSERTS(); // only MsgAudioPcm and MsgSilence expected in this test
    return nullptr;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgMetaText* /*aMsg*/)
{
    ASSERTS(); // only MsgAudioPcm and MsgSilence expected in this test
    return nullptr;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgStreamInterrupted* /*aMsg*/)
{
    ASSERTS(); // only MsgAudioPcm and MsgSilence expected in this test
    return nullptr;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgHalt* /*aMsg*/)
{
    ASSERTS(); // only MsgAudioPcm and MsgSilence expected in this test
    return nullptr;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgFlush* /*aMsg*/)
{
    ASSERTS(); // only MsgAudioPcm and MsgSilence expected in this test
    return nullptr;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgWait* /*aMsg*/)
{
    ASSERTS(); // only MsgAudioPcm and MsgSilence expected in this test
    return nullptr;
}

Msg* SuiteReservoirHistory::ProcessMsg(MsgQuit* aMsg)
{
    iStopPullerThread = true;
    return aMsg;
}

void SuiteReservoirHistory::Start(TUint /*aExpectedDecodedReservoirJiffies*/)
{
    iStartCount++;
}

void SuiteReservoirHistory::Stop()
{
    iStopCount++;
}

void SuiteReservoirHistory::Reset()
{
}

void SuiteReservoirHistory::NotifySize(TUint /*aJiffies*/)
{
    iNotifySizeCount++;
}


// SuiteEncodedReservoir

SuiteEncodedReservoir::SuiteEncodedReservoir()
    : SuiteUnitTest("EncodedReservoir")
{
    AddTest(MakeFunctor(*this, &SuiteEncodedReservoir::TestStreamHandlerCallsPassedOn), "TestStreamHandlerCallsPassedOn");
    AddTest(MakeFunctor(*this, &SuiteEncodedReservoir::TestSeekBackwards), "TestSeekBackwards");
    AddTest(MakeFunctor(*this, &SuiteEncodedReservoir::TestSeekForwardsIntoReservoir), "TestSeekForwardsIntoReservoir");
    AddTest(MakeFunctor(*this, &SuiteEncodedReservoir::TestSeekForwardsBeyondReservoir), "TestSeekForwardsBeyondReservoir");
    AddTest(MakeFunctor(*this, &SuiteEncodedReservoir::TestNewStreamInterruptsSeek), "TestNewStreamInterruptsSeek");
}

void SuiteEncodedReservoir::Setup()
{
    MsgFactoryInitParams init;
    init.SetMsgAudioEncodedCount(11, 10);
    init.SetMsgEncodedStreamCount(3);
    init.SetMsgModeCount(2);
    iMsgFactory = new MsgFactory(iInfoAggregator, init);
    iTrackFactory = new TrackFactory(iInfoAggregator, 1);
    iReservoir = new EncodedAudioReservoir(*iMsgFactory, *this, 100/*max_msg*/, 10/*max_streams*/);
    iNextFlushId = MsgFlush::kIdInvalid;
    iLastMsg = ENone;
    iOkToPlayCount = iTrySeekCount = iTryStopCount = iNotifyStarvingCount = 0;
    iPulledFlushId = MsgFlush::kIdInvalid;
    iEncAudioFill = 12345;
    (void)memset(iAudioDest, 0, sizeof(iAudioDest));
}

void SuiteEncodedReservoir::TearDown()
{
    delete iReservoir;
    delete iTrackFactory;
    delete iMsgFactory;
}

EStreamPlay SuiteEncodedReservoir::OkToPlay(TUint /*aStreamId*/)
{
    iOkToPlayCount++;
    return ePlayNo;
}

TUint SuiteEncodedReservoir::TrySeek(TUint /*aStreamId*/, TUint64 /*aOffset*/)
{
    iTrySeekCount++;
    return kTrySeekResponse;
}

TUint SuiteEncodedReservoir::TryStop(TUint /*aStreamId*/)
{
    iTryStopCount++;
    return MsgFlush::kIdInvalid;
}

void SuiteEncodedReservoir::NotifyStarving(const Brx& /*aMode*/, TUint /*aStreamId*/, TBool /*aStarving*/)
{
    iNotifyStarvingCount++;
}

Msg* SuiteEncodedReservoir::ProcessMsg(MsgEncodedStream* aMsg)
{
    iLastMsg = EMsgEncodedStream;
    return aMsg;
}

Msg* SuiteEncodedReservoir::ProcessMsg(MsgAudioEncoded* aMsg)
{
    iLastMsg = EMsgAudioEncoded;
    (void)memset(iAudioDest, 0, sizeof(iAudioDest));
    aMsg->CopyTo(iAudioDest);
    iEncAudioFill = iAudioDest[0];
    return aMsg;
}

Msg* SuiteEncodedReservoir::ProcessMsg(MsgFlush* aMsg)
{
    iLastMsg = EMsgFlush;
    iPulledFlushId = aMsg->Id();
    return aMsg;
}

Msg* SuiteEncodedReservoir::ProcessMsg(MsgMode* aMsg)              { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgTrack* aMsg)             { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgDrain* aMsg)             { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgDelay* aMsg)             { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgMetaText* aMsg)          { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgStreamInterrupted* aMsg) { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgHalt* aMsg)              { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgWait* aMsg)              { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgDecodedStream* aMsg)     { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgBitRate* aMsg)           { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgAudioPcm* aMsg)          { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgSilence* aMsg)           { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgPlayable* aMsg)          { ASSERTS(); return aMsg; }
Msg* SuiteEncodedReservoir::ProcessMsg(MsgQuit* aMsg)              { ASSERTS(); return aMsg; }

TUint SuiteEncodedReservoir::NextFlushId()
{
    return ++iNextFlushId;
}

void SuiteEncodedReservoir::PullNext(EMsgType aType)
{
    Msg* msg = iReservoir->Pull();
    msg = msg->Process(*this);
    if (msg != nullptr) {
        msg->RemoveRef();
    }
    TEST(iLastMsg == aType);
}

void SuiteEncodedReservoir::PushEncodedStream()
{
    auto msg = iMsgFactory->CreateMsgEncodedStream(Brx::Empty(), Brx::Empty(), 1234567LL, 0, kStreamId, true/*seekable*/, false/*live*/, this/*stream handler*/);
    iReservoir->Push(msg);
}

void SuiteEncodedReservoir::PushEncodedAudio(TByte aFill)
{
    (void)memset(iAudioSrc, aFill, sizeof(iAudioSrc));
    Brn buf(iAudioSrc, sizeof(iAudioSrc));
    auto msg = iMsgFactory->CreateMsgAudioEncoded(buf);
    iReservoir->Push(msg);
}

void SuiteEncodedReservoir::TestStreamHandlerCallsPassedOn()
{
    PushEncodedStream();
    PullNext(EMsgEncodedStream);

    TEST(iOkToPlayCount == 0);
    (void)iReservoir->OkToPlay(kStreamId);
    TEST(iOkToPlayCount == 1);

    TEST(iTryStopCount == 0);
    (void)iReservoir->TryStop(kStreamId);
    TEST(iTryStopCount == 1);

    TEST(iNotifyStarvingCount == 0);
    iReservoir->NotifyStarving(Brx::Empty(), kStreamId, true);
    TEST(iNotifyStarvingCount == 1);

    TEST(iTrySeekCount == 0);
    TEST(iReservoir->TrySeek(kStreamId+1, 0) == kTrySeekResponse);
    TEST(iTrySeekCount == 1);
}

void SuiteEncodedReservoir::TestSeekBackwards()
{
    PushEncodedStream();
    PushEncodedAudio(1);
    PushEncodedAudio(2);
    PullNext(EMsgEncodedStream);
    PullNext(EMsgAudioEncoded);
    TEST(iEncAudioFill == 1);
    TEST(iReservoir->TrySeek(kStreamId, EncodedAudio::kMaxBytes-1) == kTrySeekResponse);
    PullNext(EMsgAudioEncoded);
    TEST(iEncAudioFill == 2);
}

void SuiteEncodedReservoir::TestSeekForwardsIntoReservoir()
{
    PushEncodedStream();
    PushEncodedAudio(1);
    PushEncodedAudio(1);
    PushEncodedAudio(1);
    PushEncodedAudio(1);
    PushEncodedAudio(2);
    PullNext(EMsgEncodedStream);
    static const TUint64 kSeekPos = EncodedAudio::kMaxBytes*3 + 100;
    const TUint flushId = iReservoir->TrySeek(kStreamId, kSeekPos);
    TEST(flushId != MsgFlush::kIdInvalid);
    TEST(flushId != kTrySeekResponse);
    PullNext(EMsgFlush);
    TEST(iPulledFlushId == flushId);
    PullNext(EMsgAudioEncoded);
    TEST(iEncAudioFill == 1);
    PullNext(EMsgAudioEncoded);
    TEST(iEncAudioFill == 2);
}

void SuiteEncodedReservoir::TestSeekForwardsBeyondReservoir()
{
    PushEncodedStream();
    PushEncodedAudio(1);
    PushEncodedAudio(2);
    PushEncodedAudio(3);
    PullNext(EMsgEncodedStream);
    static const TUint64 kSeekPos = EncodedAudio::kMaxBytes * 10;
    TEST(iReservoir->TrySeek(kStreamId, kSeekPos) == kTrySeekResponse);
    PullNext(EMsgAudioEncoded);
    TEST(iEncAudioFill == 1);
    PullNext(EMsgAudioEncoded);
    TEST(iEncAudioFill == 2);
    PullNext(EMsgAudioEncoded);
    TEST(iEncAudioFill == 3);
}

void SuiteEncodedReservoir::TestNewStreamInterruptsSeek()
{
    PushEncodedStream();
    PushEncodedAudio(1);
    PushEncodedAudio(2);
    PushEncodedAudio(3);
    PushEncodedAudio(4);
    PushEncodedStream();
    PushEncodedAudio(5);
    PushEncodedAudio(6);
    PullNext(EMsgEncodedStream);
    static const TUint64 kSeekPos = EncodedAudio::kMaxBytes * 5;
    const TUint flushId = iReservoir->TrySeek(kStreamId, kSeekPos);
    TEST(flushId != MsgFlush::kIdInvalid);
    TEST(flushId != kTrySeekResponse);
    PullNext(EMsgFlush);
    TEST(iPulledFlushId == flushId);
    PullNext(EMsgEncodedStream);
    PullNext(EMsgAudioEncoded);
    TEST(iEncAudioFill == 5);
    PullNext(EMsgAudioEncoded);
    TEST(iEncAudioFill == 6);
}



void TestAudioReservoir()
{
    Runner runner("Decoded Audio Reservoir tests\n");
    runner.Add(new SuiteAudioReservoir());
    runner.Add(new SuiteReservoirHistory());
    runner.Add(new SuiteEncodedReservoir());
    runner.Run();
}
