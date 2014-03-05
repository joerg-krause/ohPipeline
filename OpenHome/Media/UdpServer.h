#ifndef HEADER_PIPELINE_UDPSERVER
#define HEADER_PIPELINE_UDPSERVER

#include <OpenHome/Private/Fifo.h>
#include <OpenHome/Private/Network.h>

namespace OpenHome {
namespace Media {

/**
 * Storage class for the output of a UdpSocketBase::Receive call
 */
class MsgUdp
{
public:
    MsgUdp(TUint aMaxSize);
    ~MsgUdp();
    void Read(SocketUdp& aSocket);
    const Brx& Buffer();
    OpenHome::Endpoint& Endpoint();
private:
    Bwh iBuf;
    OpenHome::Endpoint iEndpoint;
};

/**
 * Class for a continuously running server which buffers packets while active
 * and discards packets when deactivated
 */
class SocketUdpServer : public SocketUdp
{
public:
    SocketUdpServer(Environment& aEnv, TUint aMaxSize, TUint aMaxPackets, TUint aPort = 0, TIpAddress aInterface = 0);
    ~SocketUdpServer();
    void Open();
    void Close();
    Endpoint Receive(Bwx& aBuf);
private:
    static void CopyMsgToBuf(MsgUdp& aMsg, Bwx& aBuf, Endpoint& aEndpoint);
    void Requeue(MsgUdp& aMsg);
    void ServerThread();
    void CurrentAdapterChanged();
private:
    Environment& iEnv;
    TUint iMaxSize;
    TBool iOpen;
    Fifo<MsgUdp*> iFifoWaiting;
    Fifo<MsgUdp*> iFifoReady;
    MsgUdp* iDiscard;
    Mutex iLock;
    Semaphore iSemaphore;
    ThreadFunctor* iServerThread;
    TBool iQuit;
    TUint iAdapterListenerId;
};

/**
 * Class for managing a collection of SocketUdpServers. UdpServerManager owns
 * all the SocketUdpServers contained within it.
 */
class UdpServerManager
{
public:
    UdpServerManager(Environment& aEnv, TUint aMaxSize, TUint aMaxPackets);
    ~UdpServerManager();
    TUint CreateServer(TUint aPort = 0, TIpAddress aInterface = 0); // return ID of server
    SocketUdpServer& Find(TUint aId); // find server by ID
    void CloseAll();
    void OpenAll();
private:
    std::vector<SocketUdpServer*> iServers;
    Environment& iEnv;
    TUint iMaxSize;
    TUint iMaxPackets;
    Mutex iLock;
};

} // namespace Media
} // namespace OpenHome

#endif // HEADER_PIPELINE_UDPSERVER
