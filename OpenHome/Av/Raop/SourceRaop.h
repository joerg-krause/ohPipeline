#ifndef HEADER_SOURCE_RAOP
#define HEADER_SOURCE_RAOP

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Av/Source.h>
#include <OpenHome/Media/Msg.h>
#include <OpenHome/Media/PipelineObserver.h>
#include <OpenHome/Media/UdpServer.h>

namespace OpenHome {
    class Environment;
namespace Net {
    class DvStack;
}
namespace Media {
    class PipelineManager;
    class UriProviderSingleTrack;
    class IRaopDiscovery;
    class RaopDiscovery;
}
namespace Av {

class IRaopObserver
{
public:
    //virtual ~IObserverRaop() {}
    //virtual void NotifyStreamStart(TUint aAudioPort, TUint aControlPort, TUint aTimingPort) = 0;
    virtual void NotifyStreamStart() = 0;
};

class SourceRaop : public Source, public IRaopObserver, private Media::IPipelineObserver
{
public:
    SourceRaop(Environment& aEnv, Net::DvStack& aDvStack, Media::PipelineManager& aPipeline, Media::UriProviderSingleTrack& aUriProvider, const Brx& aDeviceName, TUint aDiscoveryPort);
    ~SourceRaop();
    Media::IRaopDiscovery& Discovery();
private: // from ISource
    void Activate();
    void Deactivate();
private: // from IRaopObserver
    void NotifyStreamStart();
private: // from IPipelineObserver
    void NotifyPipelineState(Media::EPipelineState aState);
    void NotifyTrack(Media::Track& aTrack, const Brx& aMode, TUint aIdPipeline);
    void NotifyMetaText(const Brx& aText);
    void NotifyTime(TUint aSeconds, TUint aTrackDurationSeconds);
    void NotifyStreamInfo(const Media::DecodedStreamInfo& aStreamInfo);
private:
    static const TUint kMaxUdpSize = 1472;
    static const TUint kMaxUdpPackets = 25;
    static const TUint kPortAudio = 60400;
    static const TUint kPortControl = 60401;
    static const TUint kPortTiming = 60402;
    Mutex iLock;
    Media::PipelineManager& iPipeline;
    Media::UriProviderSingleTrack& iUriProvider;
    Media::RaopDiscovery* iRaopDiscovery;
    Media::UdpServerManager iServerManager;
    Media::Track* iTrack;
    TUint iTrackPosSeconds;
    TUint iPipelineTrackId;
    TUint iStreamId;
    Media::EPipelineState iTransportState;
};

} // namespace Av
} // namespace OpenHome

#endif // HEADER_SOURCE_RAOP
