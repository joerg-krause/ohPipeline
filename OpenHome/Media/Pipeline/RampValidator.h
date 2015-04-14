#ifndef HEADER_PIPELINE_RAMP_VALIDATOR
#define HEADER_PIPELINE_RAMP_VALIDATOR

#include <OpenHome/Types.h>
#include <OpenHome/Private/Standard.h>
#include <OpenHome/Media/Pipeline/Msg.h>

namespace OpenHome {
namespace Media {

/*
Utility class which is not part of the generic pipeline.
May be used to validate ramps output by the overall pipeline or individual elements.
*/

class RampValidator : public IPipelineElementUpstream, public IPipelineElementDownstream, private IMsgProcessor, private INonCopyable
{
public:
    RampValidator(IPipelineElementUpstream& aUpstream);
    RampValidator(IPipelineElementDownstream& aDownstream);
    virtual ~RampValidator();
public: // from IPipelineElementUpstream
    Msg* Pull() override;
public: // from IPipelineElementDownstream
    void Push(Msg* aMsg) override;
private:
    void Reset();
    void ProcessAudio(const Ramp& aRamp);
private: // IMsgProcessor
    Msg* ProcessMsg(MsgMode* aMsg) override;
    Msg* ProcessMsg(MsgSession* aMsg) override;
    Msg* ProcessMsg(MsgTrack* aMsg) override;
    Msg* ProcessMsg(MsgDelay* aMsg) override;
    Msg* ProcessMsg(MsgEncodedStream* aMsg) override;
    Msg* ProcessMsg(MsgAudioEncoded* aMsg) override;
    Msg* ProcessMsg(MsgMetaText* aMsg) override;
    Msg* ProcessMsg(MsgHalt* aMsg) override;
    Msg* ProcessMsg(MsgFlush* aMsg) override;
    Msg* ProcessMsg(MsgWait* aMsg) override;
    Msg* ProcessMsg(MsgDecodedStream* aMsg) override;
    Msg* ProcessMsg(MsgAudioPcm* aMsg) override;
    Msg* ProcessMsg(MsgSilence* aMsg) override;
    Msg* ProcessMsg(MsgPlayable* aMsg) override;
    Msg* ProcessMsg(MsgQuit* aMsg) override;
private:
    IPipelineElementUpstream* iUpstream;
    IPipelineElementDownstream* iDownstream;
    TBool iRamping;
    TBool iRampedDown;
    TBool iWaitingForAudio;
    Ramp::EDirection iRampDirection;
    TUint iLastRamp;
};

} // namespace Media
} // namespace OpenHome

#endif // HEADER_PIPELINE_RAMP_VALIDATOR