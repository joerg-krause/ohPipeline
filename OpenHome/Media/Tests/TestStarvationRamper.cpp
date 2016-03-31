#include <OpenHome/Private/TestFramework.h>
#include <OpenHome/Private/SuiteUnitTest.h>
#include <OpenHome/Media/Pipeline/StarvationRamper.h>
#include <OpenHome/Media/Pipeline/Msg.h>
#include <OpenHome/Media/InfoProvider.h>
#include <OpenHome/Media/Utils/AllocatorInfoLogger.h>
#include <OpenHome/Media/Utils/ProcessorPcmUtils.h>
#include <OpenHome/Media/Pipeline/StarvationMonitor.h>
#include <OpenHome/Media/Pipeline/ElementObserver.h>

#include <list>
#include <limits.h>

using namespace OpenHome;
using namespace OpenHome::TestFramework;
using namespace OpenHome::Media;

namespace OpenHome {
namespace Media {

class SuiteStarvationRamper : public SuiteUnitTest
                            , private IPipelineElementUpstream
                            , private IMsgProcessor
                            , private IStreamHandler
                            , private IStarvationMonitorObserver
{
    static const TUint kMaxAudioBuffer = Jiffies::kPerMs * 10;
    static const TUint kRampUpDuration = Jiffies::kPerMs * 50;
    static const TUint kExpectedFlushId = 5;
    static const TUint kSampleRate = 44100;
    static const TUint kBitDepth = 16;
    static const TUint kNumChannels = 2;
    static const TUint kAudioPcmBytes = 1024;
    static const Brn kMode;
public:
    SuiteStarvationRamper();
    ~SuiteStarvationRamper();
private: // from SuiteUnitTest
    void Setup() override;
    void TearDown() override;
private: // from IPipelineElementUpstream
    Msg* Pull() override;
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
private: // from IStreamHandler
    EStreamPlay OkToPlay(TUint aStreamId) override;
    TUint TrySeek(TUint aStreamId, TUint64 aOffset) override;
    TUint TryStop(TUint aStreamId) override;
    void NotifyStarving(const Brx& aMode, TUint aStreamId, TBool aStarving) override;
private: // from IStarvationMonitorObserver
    void NotifyStarvationMonitorBuffering(TBool aBuffering) override;
private:
    enum EMsgType
    {
        ENone
       ,EMsgMode
       ,EMsgTrack
       ,EMsgDrain
       ,EMsgDelay
       ,EMsgEncodedStream
       ,EMsgMetaText
       ,EMsgStreamInterrupted
       ,EMsgDecodedStream
       ,EMsgAudioPcm
       ,EMsgSilence
       ,EMsgHalt
       ,EMsgFlush
       ,EMsgWait
       ,EMsgQuit
    };
private:
    void AddPending(Msg* aMsg);
    void PullNext();
    void PullNext(EMsgType aExpectedMsg);
    Msg* CreateTrack();
    Msg* CreateDecodedStream();
    Msg* CreateAudio();
    void Quit();
private:
    void TestMsgsPassWhenRunning();
    void TestBlocksWhenHasMaxAudio();
    void TestNoRampAroundHalt();
    void TestRampsAroundStarvation();
    void TestNotifyStarvingAroundStarvation();
    void TestReportsBuffering();
private:
    AllocatorInfoLogger iInfoAggregator;
    TrackFactory* iTrackFactory;
    MsgFactory* iMsgFactory;
    StarvationRamper* iStarvationRamper;
    ElementObserverSync* iEventCallback;
    Mutex iPendingMsgLock;
    Semaphore iMsgAvailable;
    EMsgType iLastPulledMsg;
    TBool iRampingUp;
    TBool iRampingDown;
    TBool iBuffering;
    TBool iStarted;
    TUint iStreamId;
    TUint64 iTrackOffset;
    TUint64 iJiffies;
    std::list<Msg*> iPendingMsgs;
    TUint iLastRampPos;
    TUint iNextStreamId;
    Bws<kAudioPcmBytes> iPcmData;
    TBool iStarving;
    TUint iStarvingStreamId;
};

} // namespace Media
} // namespace OpenHome


const Brn SuiteStarvationRamper::kMode("DummyMode");

SuiteStarvationRamper::SuiteStarvationRamper()
    : SuiteUnitTest("StarvationRamper")
    , iPendingMsgLock("SSR1")
    , iMsgAvailable("SSR2", 0)
{
    AddTest(MakeFunctor(*this, &SuiteStarvationRamper::TestMsgsPassWhenRunning), "TestMsgsPassWhenRunning");
    AddTest(MakeFunctor(*this, &SuiteStarvationRamper::TestBlocksWhenHasMaxAudio), "TestBlocksWhenHasMaxAudio");
    AddTest(MakeFunctor(*this, &SuiteStarvationRamper::TestNoRampAroundHalt), "TestNoRampAroundHalt");
    AddTest(MakeFunctor(*this, &SuiteStarvationRamper::TestRampsAroundStarvation), "TestRampsAroundStarvation");
    AddTest(MakeFunctor(*this, &SuiteStarvationRamper::TestNotifyStarvingAroundStarvation), "TestNotifyStarvingAroundStarvation");
    AddTest(MakeFunctor(*this, &SuiteStarvationRamper::TestReportsBuffering), "TestReportsBuffering");

    // audio data with left=0x7f, right=0x00
    iPcmData.SetBytes(iPcmData.MaxBytes());
    TByte* p = const_cast<TByte*>(iPcmData.Ptr());
    const TUint samples = iPcmData.MaxBytes() / ((kBitDepth/8) * kNumChannels);
    for (TUint i=0; i<samples; i++) {
        *p++ = 0x7f;
        *p++ = 0x7f;
        *p++ = 0x00;
        *p++ = 0x00;
    }
}

SuiteStarvationRamper::~SuiteStarvationRamper()
{
}

void SuiteStarvationRamper::Setup()
{
    iStreamId = UINT_MAX;
    iTrackOffset = 0;
    iJiffies = 0;
    iRampingUp = iRampingDown = iBuffering = iStarted = false;
    iLastRampPos = Ramp::kMax;
    iNextStreamId = 1;
    iStarving = false;
    iStarvingStreamId = IPipelineIdProvider::kStreamIdInvalid;

    iTrackFactory = new TrackFactory(iInfoAggregator, 5);
    iEventCallback = new ElementObserverSync();
    MsgFactoryInitParams init;
    init.SetMsgAudioPcmCount(52, 50);
    init.SetMsgSilenceCount(20);
    init.SetMsgDecodedStreamCount(3);
    init.SetMsgTrackCount(3);
    init.SetMsgEncodedStreamCount(3);
    init.SetMsgMetaTextCount(3);
    init.SetMsgHaltCount(2);
    init.SetMsgFlushCount(2);
    init.SetMsgModeCount(2);
    init.SetMsgWaitCount(2);
    init.SetMsgDelayCount(2);
    iMsgFactory = new MsgFactory(iInfoAggregator, init);
    iStarvationRamper = new StarvationRamper(*iMsgFactory, *this, *this, *iEventCallback,
                                             kMaxAudioBuffer, kPriorityHigh, kRampUpDuration, 10);
    (void)iMsgAvailable.Clear();
}

void SuiteStarvationRamper::TearDown()
{
    while (iPendingMsgs.size() > 0) {
        iPendingMsgs.front()->RemoveRef();
        iPendingMsgs.pop_front();
    }
    delete iStarvationRamper;
    delete iEventCallback;
    delete iMsgFactory;
    delete iTrackFactory;
}

Msg* SuiteStarvationRamper::Pull()
{
    iMsgAvailable.Wait();
    AutoMutex _(iPendingMsgLock);
    Msg* msg = iPendingMsgs.front();
    iPendingMsgs.pop_front();
    return msg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgMode* aMsg)
{
    iLastPulledMsg = EMsgMode;
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgTrack* aMsg)
{
    iLastPulledMsg = EMsgTrack;
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgDrain* aMsg)
{
    iLastPulledMsg = EMsgDrain;
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgDelay* aMsg)
{
    iLastPulledMsg = EMsgDelay;
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgEncodedStream* aMsg)
{
    iLastPulledMsg = EMsgEncodedStream;
    iStreamId = aMsg->StreamId();
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgAudioEncoded* /*aMsg*/)
{
    ASSERTS();
    return nullptr;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgMetaText* aMsg)
{
    iLastPulledMsg = EMsgMetaText;
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgStreamInterrupted* aMsg)
{
    iLastPulledMsg = EMsgStreamInterrupted;
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgHalt* aMsg)
{
    iLastPulledMsg = EMsgHalt;
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgFlush* aMsg)
{
    iLastPulledMsg = EMsgFlush;
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgWait* aMsg)
{
    iLastPulledMsg = EMsgWait;
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgDecodedStream* aMsg)
{
    iLastPulledMsg = EMsgDecodedStream;
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgBitRate* /*aMsg*/)
{
    ASSERTS();
    return nullptr;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgAudioPcm* aMsg)
{
    iLastPulledMsg = EMsgAudioPcm;
    iJiffies += aMsg->Jiffies();
    const Media::Ramp& ramp = aMsg->Ramp();
    if (iRampingDown) {
        TEST(ramp.Direction() == Ramp::EDown);
        TEST(ramp.Start() == iLastRampPos);
        if (ramp.End() == Ramp::kMin) {
            iRampingDown = false;
        }
    }
    else if (iRampingUp) {
        TEST(ramp.Direction() == Ramp::EUp);
        TEST(ramp.Start() == iLastRampPos);
        if (ramp.End() == Ramp::kMax) {
            iRampingUp = false;
        }
    }
    else {
        TEST(ramp.Direction() == Ramp::ENone);
    }
    iLastRampPos = ramp.End();

    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgSilence* aMsg)
{
    iLastPulledMsg = EMsgSilence;
    return aMsg;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgPlayable* /*aMsg*/)
{
    ASSERTS();
    return nullptr;
}

Msg* SuiteStarvationRamper::ProcessMsg(MsgQuit* aMsg)
{
    iLastPulledMsg = EMsgQuit;
    return aMsg;
}

EStreamPlay SuiteStarvationRamper::OkToPlay(TUint /*aStreamId*/)
{
    ASSERTS();
    return ePlayNo;
}

TUint SuiteStarvationRamper::TrySeek(TUint /*aStreamId*/, TUint64 /*aOffset*/)
{
    ASSERTS();
    return MsgFlush::kIdInvalid;
}

TUint SuiteStarvationRamper::TryStop(TUint /*aStreamId*/)
{
    ASSERTS();
    return MsgFlush::kIdInvalid;
}

void SuiteStarvationRamper::NotifyStarving(const Brx& aMode, TUint aStreamId, TBool aStarving)
{
    TEST(aMode == kMode);
    iStarving = aStarving;
    iStarvingStreamId = aStreamId;
}

void SuiteStarvationRamper::NotifyStarvationMonitorBuffering(TBool aBuffering)
{
    iBuffering = aBuffering;
}

void SuiteStarvationRamper::AddPending(Msg* aMsg)
{
    iPendingMsgLock.Wait();
    iPendingMsgs.push_back(aMsg);
    iPendingMsgLock.Signal();
    iMsgAvailable.Signal();
    if (!iStarted) {
        iStarvationRamper->Start();
        iStarted = true;
    }
}

void SuiteStarvationRamper::PullNext()
{
    Msg* msg = iStarvationRamper->Pull();
    msg = msg->Process(*this);
    msg->RemoveRef();
}

void SuiteStarvationRamper::PullNext(EMsgType aExpectedMsg)
{
    Msg* msg = iStarvationRamper->Pull();
    msg = msg->Process(*this);
    msg->RemoveRef();
    if (iLastPulledMsg != aExpectedMsg) {
        static const TChar* types[] ={
            "None"
            , "MsgMode"
            , "MsgTrack"
            , "MsgDrain"
            , "MsgDelay"
            , "MsgEncodedStream"
            , "MsgMetaText"
            , "MsgStreamInterrupted"
            , "MsgDecodedStream"
            , "MsgAudioPcm"
            , "MsgSilence"
            , "MsgHalt"
            , "MsgFlush"
            , "MsgWait"
            , "MsgQuit" };
        Print("Expected %s, got %s\n", types[aExpectedMsg], types[iLastPulledMsg]);
    }
    TEST(iLastPulledMsg == aExpectedMsg);
}

Msg* SuiteStarvationRamper::CreateTrack()
{
    Track* track = iTrackFactory->CreateTrack(Brx::Empty(), Brx::Empty());
    Msg* msg = iMsgFactory->CreateMsgTrack(*track);
    track->RemoveRef();
    return msg;
}

Msg* SuiteStarvationRamper::CreateDecodedStream()
{
    return iMsgFactory->CreateMsgDecodedStream(iNextStreamId, 100, kBitDepth, kSampleRate, kNumChannels, Brn("notARealCodec"), 1LL<<38, 0, true, true, false, false, this);
}

Msg* SuiteStarvationRamper::CreateAudio()
{
    MsgAudioPcm* audio = iMsgFactory->CreateMsgAudioPcm(iPcmData, kNumChannels, kSampleRate, kBitDepth, EMediaDataEndianLittle, iTrackOffset);
    iTrackOffset += audio->Jiffies();
    return audio;
}

void SuiteStarvationRamper::Quit()
{
    iRampingDown = true; // if Pull() is called before StarvationRamper pulls the Halt below, it'll start a ramp down
    AddPending(iMsgFactory->CreateMsgHalt());
    AddPending(iMsgFactory->CreateMsgQuit());
    do {
        PullNext();
    } while (iLastPulledMsg != EMsgQuit);
}

void SuiteStarvationRamper::TestMsgsPassWhenRunning()
{
    AddPending(iMsgFactory->CreateMsgMode(kMode, false, true, ModeClockPullers(), false, false));
    AddPending(CreateTrack());
    AddPending(iMsgFactory->CreateMsgDrain(Functor()));
    AddPending(CreateDecodedStream());
    AddPending(CreateAudio());
    AddPending(iMsgFactory->CreateMsgSilence(Jiffies::kPerMs * 3));
    AddPending(iMsgFactory->CreateMsgHalt());
    AddPending(iMsgFactory->CreateMsgQuit());

    PullNext(EMsgMode);
    PullNext(EMsgTrack);
    PullNext(EMsgDrain);
    PullNext(EMsgDecodedStream);
    PullNext(EMsgAudioPcm);
    PullNext(EMsgSilence);
    PullNext(EMsgHalt);
    PullNext(EMsgQuit);
}

void SuiteStarvationRamper::TestBlocksWhenHasMaxAudio()
{
    AddPending(iMsgFactory->CreateMsgMode(kMode, false, true, ModeClockPullers(), false, false));
    AddPending(CreateTrack());
    AddPending(CreateDecodedStream());
    TUint audioCount = 0;
    do {
        AddPending(CreateAudio());
        audioCount++;
    } while (iTrackOffset < kMaxAudioBuffer);
    AddPending(CreateAudio());
    audioCount++;
    AddPending(iMsgFactory->CreateMsgQuit());

    PullNext(EMsgMode);
    PullNext(EMsgTrack);
    PullNext(EMsgDecodedStream);
    TInt retries = 100;
    while (retries-- > 0) {
        if (iPendingMsgs.size() == 2) { // 2 == EMsgAudioPcm + EMsgQuit
            break;
        }
        ASSERT(retries != 0);
    }
    Thread::Sleep(100); // wait long enough for pending audio to be pulled if SR is running
    TEST(iPendingMsgs.size() == 2);
    while (audioCount-- > 0) {
        PullNext(EMsgAudioPcm);
    }
    PullNext(EMsgQuit);
}

void SuiteStarvationRamper::TestNoRampAroundHalt()
{
    AddPending(iMsgFactory->CreateMsgMode(kMode, false, true, ModeClockPullers(), false, false));
    AddPending(CreateTrack());
    AddPending(CreateDecodedStream());
    AddPending(CreateAudio());
    AddPending(CreateAudio());
    AddPending(iMsgFactory->CreateMsgHalt());
    AddPending(CreateAudio());
    AddPending(CreateAudio());
    AddPending(iMsgFactory->CreateMsgQuit());
    ASSERT(!iRampingDown);
    ASSERT(!iRampingUp);

    PullNext(EMsgMode);
    PullNext(EMsgTrack);
    PullNext(EMsgDecodedStream);
    PullNext(EMsgAudioPcm);
    PullNext(EMsgAudioPcm);
    PullNext(EMsgHalt);
    PullNext(EMsgAudioPcm);
    PullNext(EMsgAudioPcm);
    PullNext(EMsgQuit);
}

void SuiteStarvationRamper::TestRampsAroundStarvation()
{
    // ramps down after > kMaxAudioBuffer of prior audio, ramp down takes StarvationRamper::kRampDownJiffies
    AddPending(iMsgFactory->CreateMsgMode(kMode, false, true, ModeClockPullers(), false, false));
    AddPending(CreateTrack());
    AddPending(CreateDecodedStream());
    TInt audioCount = 0;
    do {
        AddPending(CreateAudio());
        audioCount++;
    } while (iTrackOffset < StarvationRamper::kRampDownJiffies);

    PullNext(EMsgMode);
    PullNext(EMsgTrack);
    PullNext(EMsgDecodedStream);
    while (audioCount-- > 0) {
        PullNext(EMsgAudioPcm);
    }
    iRampingDown = true;
    iJiffies = 0;
    while (iRampingDown) {
        PullNext(EMsgAudioPcm);
    }
    TEST(iJiffies == StarvationRamper::kRampDownJiffies);
    PullNext(EMsgHalt);
    TEST(iStarvationRamper->iState == StarvationRamper::State::RampingUp);

    // ramps up once audio is available, ramp up takes kRampUpDuration
    iRampingUp = true;
    iJiffies = 0;
    TUint64 trackOffsetStart = iTrackOffset;
    do {
        AddPending(CreateAudio());
    } while (iTrackOffset - trackOffsetStart < kRampUpDuration);
    while (iRampingUp) {
        PullNext(EMsgAudioPcm);
    }
    TEST(iJiffies == kRampUpDuration);
    TEST(iStarvationRamper->iState == StarvationRamper::State::Running);

    // clear any split msg at the end of the ramp up
    const TBool empty = iStarvationRamper->IsEmpty();
    AddPending(CreateDecodedStream());
    if (!empty) {
        PullNext(EMsgAudioPcm);
    }

    // ramps down after < kMaxAudioBuffer of prior audio, ramp down takes StarvationRamper::kRampDownJiffies
    AddPending(CreateAudio());
    PullNext(EMsgDecodedStream);
    PullNext(EMsgAudioPcm);
    iRampingDown = true;
    iJiffies = 0;
    while (iRampingDown) {
        PullNext(EMsgAudioPcm);
    }
    TEST(iJiffies == StarvationRamper::kRampDownJiffies);
    PullNext(EMsgHalt);
    TEST(iStarvationRamper->iState == StarvationRamper::State::RampingUp);

    Quit();
}

void SuiteStarvationRamper::TestNotifyStarvingAroundStarvation()
{
    TEST(!iStarving);
    AddPending(iMsgFactory->CreateMsgMode(kMode, false, true, ModeClockPullers(), false, false));
    AddPending(CreateTrack());
    AddPending(CreateDecodedStream());
    PullNext(EMsgMode);
    PullNext(EMsgTrack);
    PullNext(EMsgDecodedStream);
    TEST(!iStarving);
    AddPending(CreateAudio());
    PullNext(EMsgAudioPcm);
    iRampingDown = true;
    PullNext(EMsgAudioPcm);
    TEST(iStarving);
    while (iRampingDown) {
        PullNext(EMsgAudioPcm);
    }
    TEST(iStarving);
    PullNext(EMsgHalt);

    iRampingUp = true;
    AddPending(CreateAudio());
    PullNext(EMsgAudioPcm);
    TEST(!iStarving);

    Quit();
}

void SuiteStarvationRamper::TestReportsBuffering()
{
    TEST(iBuffering);
    AddPending(iMsgFactory->CreateMsgMode(kMode, false, true, ModeClockPullers(), false, false));
    AddPending(CreateTrack());
    AddPending(CreateDecodedStream());
    PullNext(EMsgMode);
    TEST(iBuffering);
    PullNext(EMsgTrack);
    TEST(iBuffering);
    PullNext(EMsgDecodedStream);
    TEST(iBuffering);
    AddPending(CreateAudio());
    PullNext(EMsgAudioPcm);
    TEST(!iBuffering);
    iRampingDown = true;
    while (iRampingDown) {
        PullNext(EMsgAudioPcm);
        TEST(iBuffering);
    }
    PullNext(EMsgHalt);
    AddPending(CreateAudio());
    iRampingUp = true;
    PullNext(EMsgAudioPcm);
    TEST(!iBuffering);
    iRampingUp = false;
    iRampingDown = true;
    PullNext(EMsgAudioPcm);
    TEST(iBuffering);

    AddPending(CreateTrack());
    AddPending(CreateDecodedStream());
    AddPending(CreateAudio());
    do {
        PullNext();
    } while (iLastPulledMsg != EMsgTrack);
    iRampingDown = false;
    TEST(iBuffering);
    PullNext(EMsgDecodedStream);
    TEST(iBuffering);
    PullNext(EMsgAudioPcm);
    TEST(!iBuffering);

    AddPending(CreateTrack());
    AddPending(CreateDecodedStream());
    AddPending(CreateAudio());
    Thread::Sleep(50); // short wait to allow StarvationRamper to pull the above msgs
    PullNext(EMsgTrack);
    PullNext(EMsgDecodedStream);
    PullNext(EMsgAudioPcm);
    TEST(!iBuffering);

    Quit();
}



void TestStarvationRamper()
{
    Runner runner("StarvationRamper tests\n");
    runner.Add(new SuiteStarvationRamper());
    runner.Run();
}
