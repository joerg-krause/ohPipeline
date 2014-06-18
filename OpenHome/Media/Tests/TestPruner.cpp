#include <OpenHome/Private/TestFramework.h>
#include <OpenHome/Media/Pruner.h>
#include <OpenHome/Media/Msg.h>
#include "AllocatorInfoLogger.h"
#include <OpenHome/Private/SuiteUnitTest.h>
#include <OpenHome/Functor.h>

#include <vector>

using namespace OpenHome;
using namespace OpenHome::TestFramework;
using namespace OpenHome::Media;

namespace OpenHome {
namespace Media {

class SuitePruner : public SuiteUnitTest, private IPipelineElementUpstream, private IMsgProcessor
{
public:
    SuitePruner();
private: // from SuiteUnitTest
    void Setup();
    void TearDown();
private:
    enum EMsgType
    {
        ENone
       ,EMsgMode
       ,EMsgSession
       ,EMsgTrack
       ,EMsgDelay
       ,EMsgEncodedStream
       ,EMsgAudioEncoded
       ,EMsgMetaText
       ,EMsgHalt
       ,EMsgFlush
       ,EMsgWait
       ,EMsgDecodedStream
       ,EMsgAudioPcm
       ,EMsgSilence
       ,EMsgPlayable
       ,EMsgQuit
    };
private:
    EMsgType DoPull();
    void MsgsDiscarded();
    void QuitDoesntWaitForAudio();
    void HaltPassedOn();
    void DecodedStreamPassedOn();
    void TrackWithoutAudioAllMsgsDiscarded();
    void SilenceUnblocksTrackMsgs();
private: // from IPipelineElementUpstream
    Msg* Pull();
private: // IMsgProcessor
    Msg* ProcessMsg(MsgMode* aMsg);
    Msg* ProcessMsg(MsgSession* aMsg);
    Msg* ProcessMsg(MsgTrack* aMsg);
    Msg* ProcessMsg(MsgDelay* aMsg);
    Msg* ProcessMsg(MsgEncodedStream* aMsg);
    Msg* ProcessMsg(MsgAudioEncoded* aMsg);
    Msg* ProcessMsg(MsgMetaText* aMsg);
    Msg* ProcessMsg(MsgHalt* aMsg);
    Msg* ProcessMsg(MsgFlush* aMsg);
    Msg* ProcessMsg(MsgWait* aMsg);
    Msg* ProcessMsg(MsgDecodedStream* aMsg);
    Msg* ProcessMsg(MsgAudioPcm* aMsg);
    Msg* ProcessMsg(MsgSilence* aMsg);
    Msg* ProcessMsg(MsgPlayable* aMsg);
    Msg* ProcessMsg(MsgQuit* aMsg);
private:
    MsgFactory* iMsgFactory;
    TrackFactory* iTrackFactory;
    AllocatorInfoLogger iInfoAggregator;
    Pruner* iPruner;
    EMsgType iLastPulledMsg;
    TUint iNextTrackId;
    TUint iPulledTrackId;
    std::vector<EMsgType> iPendingMsgs;
};

} // namespace Media
} // namespace OpenHome

// SuitePruner

SuitePruner::SuitePruner()
    : SuiteUnitTest("Pruner tests")
{
    AddTest(MakeFunctor(*this, &SuitePruner::MsgsDiscarded), "MsgsDiscarded");
    AddTest(MakeFunctor(*this, &SuitePruner::QuitDoesntWaitForAudio), "QuitDoesntWaitForAudio");
    AddTest(MakeFunctor(*this, &SuitePruner::HaltPassedOn), "HaltPassedOn");
    AddTest(MakeFunctor(*this, &SuitePruner::DecodedStreamPassedOn), "DecodedStreamPassedOn");
    AddTest(MakeFunctor(*this, &SuitePruner::TrackWithoutAudioAllMsgsDiscarded), "TrackWithoutAudioAllMsgsDiscarded");
    AddTest(MakeFunctor(*this, &SuitePruner::SilenceUnblocksTrackMsgs), "SilenceUnblocksTrackMsgs");
}

void SuitePruner::Setup()
{
    iMsgFactory = new MsgFactory(iInfoAggregator, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1);
    iTrackFactory = new TrackFactory(iInfoAggregator, 3);
    iPruner = new Pruner(*this);
    iNextTrackId = 0;
    iPulledTrackId = UINT_MAX/2;
}

void SuitePruner::TearDown()
{
    delete iPruner;
    delete iMsgFactory;
    delete iTrackFactory;
}

SuitePruner::EMsgType SuitePruner::DoPull()
{
    Msg* msg = iPruner->Pull();
    msg = msg->Process(*this);
    if (msg != NULL) {
        msg->RemoveRef();
    }
    return iLastPulledMsg;
}

#define NUM_EMEMS(arr) sizeof(arr) / sizeof(arr[0])

void SuitePruner::MsgsDiscarded()
{
    EMsgType msgs[] = { EMsgMode, EMsgSession, EMsgTrack, EMsgDelay, EMsgEncodedStream, EMsgMetaText, EMsgWait, EMsgAudioPcm };
    iPendingMsgs.assign(msgs, msgs+NUM_EMEMS(msgs));
    TEST(DoPull() == EMsgTrack);
    TEST(DoPull() == EMsgAudioPcm);
    TEST(iPendingMsgs.size() == 0);
}

void SuitePruner::QuitDoesntWaitForAudio()
{
    EMsgType msgs[] = { EMsgTrack, EMsgEncodedStream, EMsgQuit };
    iPendingMsgs.assign(msgs, msgs+NUM_EMEMS(msgs));
    TEST(DoPull() == EMsgTrack);
    TEST(DoPull() == EMsgQuit);
    TEST(iPendingMsgs.size() == 0);
}

void SuitePruner::HaltPassedOn()
{
    EMsgType msgs[] = { EMsgTrack, EMsgEncodedStream, EMsgHalt, EMsgAudioPcm };
    iPendingMsgs.assign(msgs, msgs+NUM_EMEMS(msgs));
    TEST(DoPull() == EMsgTrack);
    TEST(DoPull() == EMsgHalt);
    TEST(iPendingMsgs.size() == 0);
    TEST(DoPull() == EMsgAudioPcm);
}

void SuitePruner::DecodedStreamPassedOn()
{
    EMsgType msgs[] = { EMsgTrack, EMsgEncodedStream, EMsgDecodedStream, EMsgAudioPcm };
    iPendingMsgs.assign(msgs, msgs+NUM_EMEMS(msgs));
    TEST(DoPull() == EMsgTrack);
    TEST(DoPull() == EMsgDecodedStream);
    TEST(iPendingMsgs.size() == 0);
    TEST(DoPull() == EMsgAudioPcm);
}

void SuitePruner::TrackWithoutAudioAllMsgsDiscarded()
{
    EMsgType msgs[] = { EMsgTrack, EMsgEncodedStream, EMsgDecodedStream, EMsgHalt, EMsgTrack, EMsgEncodedStream, EMsgDecodedStream, EMsgAudioPcm };
    iPendingMsgs.assign(msgs, msgs+NUM_EMEMS(msgs));
    TEST(DoPull() == EMsgTrack);
    TEST(DoPull() == EMsgDecodedStream);
    TEST(DoPull() == EMsgAudioPcm);
    TEST(iPendingMsgs.size() == 0);
}

void SuitePruner::SilenceUnblocksTrackMsgs()
{
    EMsgType msgs[] = { EMsgTrack, EMsgEncodedStream, EMsgDecodedStream, EMsgSilence };
    iPendingMsgs.assign(msgs, msgs+NUM_EMEMS(msgs));
    TEST(DoPull() == EMsgTrack);
    TEST(DoPull() == EMsgDecodedStream);
    TEST(DoPull() == EMsgSilence);
    TEST(iPendingMsgs.size() == 0);
}

Msg* SuitePruner::ProcessMsg(MsgMode* aMsg)
{
    iLastPulledMsg = EMsgMode;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgSession* aMsg)
{
    iLastPulledMsg = EMsgSession;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgTrack* aMsg)
{
    iPulledTrackId = aMsg->Track().Id();
    iLastPulledMsg = EMsgTrack;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgDelay* aMsg)
{
    iLastPulledMsg = EMsgDelay;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgEncodedStream* aMsg)
{
    iLastPulledMsg = EMsgEncodedStream;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgAudioEncoded* aMsg)
{
    iLastPulledMsg = EMsgAudioEncoded;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgMetaText* aMsg)
{
    iLastPulledMsg = EMsgMetaText;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgHalt* aMsg)
{
    iLastPulledMsg = EMsgHalt;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgFlush* aMsg)
{
    iLastPulledMsg = EMsgFlush;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgWait* aMsg)
{
    iLastPulledMsg = EMsgWait;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgDecodedStream* aMsg)
{
    iLastPulledMsg = EMsgDecodedStream;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgAudioPcm* aMsg)
{
    iLastPulledMsg = EMsgAudioPcm;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgSilence* aMsg)
{
    iLastPulledMsg = EMsgSilence;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgPlayable* aMsg)
{
    iLastPulledMsg = EMsgPlayable;
    return aMsg;
}

Msg* SuitePruner::ProcessMsg(MsgQuit* aMsg)
{
    iLastPulledMsg = EMsgQuit;
    return aMsg;
}

Msg* SuitePruner::Pull()
{
    static const TUint kStreamId      = 0;
    static const TUint kBitDepth      = 24;
    static const TUint kSampleRate    = 44100;
    static const TUint kBitRate       = kBitDepth * kSampleRate;
    static const TUint64 kTrackLength = Jiffies::kPerSecond * 60;
    static const TBool kLossless      = true;
    static const TBool kSeekable      = false;
    static const TBool kLive          = false;
    static const TUint kNumChannels   = 2;
    static TUint64 iTrackOffset = 0;

    EMsgType msgType = iPendingMsgs[0];
    iPendingMsgs.erase(iPendingMsgs.begin());
    switch (msgType)
    {
    case EMsgMode:
        return iMsgFactory->CreateMsgMode(Brx::Empty(), true, true, NULL);
    case EMsgSession:
        return iMsgFactory->CreateMsgSession();
    case EMsgTrack:
    {
        Track* track = iTrackFactory->CreateTrack(Brx::Empty(), Brx::Empty());
        Msg* msg = iMsgFactory->CreateMsgTrack(*track, iNextTrackId);
        track->RemoveRef();
        iNextTrackId++;
        return msg;
    }
    case EMsgDelay:
        return iMsgFactory->CreateMsgDelay(0);
    case EMsgEncodedStream:
    {
        return iMsgFactory->CreateMsgEncodedStream(Brx::Empty(), Brx::Empty(), UINT_MAX/4, kStreamId, kSeekable, kLive, NULL);
    }
    case EMsgMetaText:
    {
        return iMsgFactory->CreateMsgMetaText(Brn("dummy metatext"));
    }
    case EMsgHalt:
        return iMsgFactory->CreateMsgHalt();
    case EMsgWait:
        return iMsgFactory->CreateMsgWait();
    case EMsgDecodedStream:
    {
        return iMsgFactory->CreateMsgDecodedStream(kStreamId, kBitRate, kBitDepth, kSampleRate, kNumChannels, Brn("Dummy codec"), kTrackLength, 0, kLossless, kSeekable, kLive, NULL);
    }
    case EMsgAudioPcm:
    {
        static const TUint kDataBytes = 3 * 1024;
        TByte encodedAudioData[kDataBytes];
        (void)memset(encodedAudioData, 0xff, kDataBytes);
        Brn encodedAudioBuf(encodedAudioData, kDataBytes);
        MsgAudio* audio = iMsgFactory->CreateMsgAudioPcm(encodedAudioBuf, kNumChannels, kSampleRate, 16, EMediaDataLittleEndian, iTrackOffset);
        iTrackOffset += audio->Jiffies();
        return audio;
    }
    case EMsgSilence:
    {
        return iMsgFactory->CreateMsgSilence(Jiffies::kPerMs * 5);
    }
    case EMsgQuit:
        return iMsgFactory->CreateMsgQuit();
    case EMsgAudioEncoded:
    case EMsgFlush:
    case EMsgPlayable:
    default:
        ASSERTS();
        return NULL;
    }
}



void TestPruner()
{
    Runner runner("Pruner tests\n");
    runner.Add(new SuitePruner());
    runner.Run();
}
