#include <OpenHome/Media/Codec/CodecController.h>
#include <OpenHome/Media/Codec/Container.h>
#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Private/Printer.h>
#include <OpenHome/Private/Debug.h>
#include <OpenHome/Media/Msg.h>
#include <OpenHome/Media/Codec/Id3v2.h>

using namespace OpenHome;
using namespace OpenHome::Media;
using namespace OpenHome::Media::Codec;

// CodecBase

CodecBase::~CodecBase()
{
}

void CodecBase::StreamCompleted()
{
}

CodecBase::CodecBase()
    : iController(NULL)
{
}

void CodecBase::Construct(ICodecController& aController, MsgFactory& aMsgFactory)
{
    iController = &aController;
    iMsgFactory = &aMsgFactory;
}


// CodecController

CodecController::CodecController(MsgFactory& aMsgFactory, IPipelineElementUpstream& aUpstreamElement, IPipelineElementDownstream& aDownstreamElement)
    : iMsgFactory(aMsgFactory)
    , iUpstreamElement(aUpstreamElement)
    , iDownstreamElement(aDownstreamElement)
    , iActiveCodec(NULL)
    , iQueueTrackData(false)
    , iQuit(false)
    , iAudioEncoded(NULL)
{
    iDecoderThread = new ThreadFunctor("CDEC", MakeFunctor(*this, &CodecController::CodecThread));
    iDecoderThread->Start();
}

CodecController::~CodecController()
{
    delete iDecoderThread;
    if (iAudioEncoded != NULL) {
        iAudioEncoded->RemoveRef();
    }
}

void CodecController::AddCodec(CodecBase* aCodec)
{
    aCodec->Construct(*this, iMsgFactory);
    iCodecs.push_back(aCodec);
}

void CodecController::CodecThread()
{
    while (!iQuit) {
        try {
            iQueueTrackData = false;
            iActiveCodec = NULL;
            try {
                for (;;) {
                    PullMsg();
                }
            }
            catch (CodecStreamStart&) {
                iQueueTrackData = true;
            }

            try {
                do {
                    PullMsg();
                } while (iAudioEncoded == NULL || iAudioEncoded->Bytes() < kMaxRecogniseBytes);

                /* we can only CopyTo a max of kMaxRecogniseBytes bytes.  If we have more data than this, 
                   split the msg, select a codec then add the fragments back together before processing */
                MsgAudioEncoded* remaining = NULL;
                if (iAudioEncoded->Bytes() > kMaxRecogniseBytes) {
                    remaining = iAudioEncoded->Split(kMaxRecogniseBytes);
                }
                iAudioEncoded->CopyTo(iReadBuf);
                Brn recogBuf(iReadBuf, kMaxRecogniseBytes);
                for (size_t i=0; i<iCodecs.size(); i++) {
                    CodecBase* codec = iCodecs[i];
                    if (codec->Recognise(recogBuf)) {
                        iActiveCodec = codec;
                        break;
                    }
                }
                iAudioEncoded->Add(remaining);

                // FIXME - handle iActiveCodec being NULL (i.e. unsupported data)
                iActiveCodec->Process();
            }
            catch (CodecStreamStart) {
                // FIXME
            }
            catch (CodecStreamCorrupt) {
                // FIXME
            }
        }
        catch (CodecStreamEnded) {
            // FIXME
        }
        catch (CodecStreamFlush) {
            // FIXME
        }
    }
}

void CodecController::PullMsg()
{
    Msg* msg = iUpstreamElement.Pull();
    msg = msg->Process(*this);
    ASSERT_DEBUG(msg == NULL);
}

void CodecController::Queue(Msg* aMsg)
{
    iDownstreamElement.Push(aMsg);
}

void CodecController::Read(Bwx& aBuf, TUint aBytes)
{
    while (iAudioEncoded == NULL || iAudioEncoded->Bytes() < aBytes) {
        PullMsg();
    }
    MsgAudioEncoded* remaining = NULL;
    if (iAudioEncoded->Bytes() > aBytes) {
        remaining = iAudioEncoded->Split(aBytes);
    }
    ASSERT(aBuf.Bytes() + aBytes <= aBuf.MaxBytes());
    TByte* ptr = const_cast<TByte*>(aBuf.Ptr()) + aBuf.Bytes();
    iAudioEncoded->CopyTo(ptr);
    aBuf.SetBytes(aBuf.Bytes() + aBytes);
    iAudioEncoded->RemoveRef();
    iAudioEncoded = remaining;
}

void CodecController::Output(MsgAudioFormat* aMsg)
{
    Queue(aMsg);
}

void CodecController::Output(MsgAudioPcm* aMsg)
{
    Queue(aMsg);
}

Msg* CodecController::ProcessMsg(MsgAudioEncoded* aMsg)
{
    if (!iQueueTrackData) {
        aMsg->RemoveRef();
    }
    else if (iAudioEncoded == NULL) {
        iAudioEncoded = aMsg;
    }
    else {
        iAudioEncoded->Add(aMsg);
    }
    return NULL;
}

Msg* CodecController::ProcessMsg(MsgAudioPcm* /*aMsg*/)
{
    ASSERTS(); // not expected at this stage of the pipeline
    return NULL;
}

Msg* CodecController::ProcessMsg(MsgSilence* /*aMsg*/)
{
    ASSERTS(); // not expected at this stage of the pipeline
    return NULL;
}

Msg* CodecController::ProcessMsg(MsgPlayable* /*aMsg*/)
{
    ASSERTS(); // not expected at this stage of the pipeline
    return NULL;
}

Msg* CodecController::ProcessMsg(MsgAudioFormat* /*aMsg*/)
{
    ASSERTS(); // expect this to be generated by a codec
    // FIXME - volkano has containers which also generate this
    return NULL;
}

Msg* CodecController::ProcessMsg(MsgTrack* aMsg)
{
    Queue(aMsg);
    THROW(CodecStreamStart);
}

Msg* CodecController::ProcessMsg(MsgMetaText* aMsg)
{
    if (!iQueueTrackData) {
        aMsg->RemoveRef();
    }
    else {
        Queue(aMsg);
    }
    return NULL;
}

Msg* CodecController::ProcessMsg(MsgHalt* aMsg)
{
    Queue(aMsg);
    return NULL;
}

Msg* CodecController::ProcessMsg(MsgFlush* aMsg)
{
    Queue(aMsg);
    THROW(CodecStreamFlush);
}

Msg* CodecController::ProcessMsg(MsgQuit* aMsg)
{
    iQuit = true;
    Queue(aMsg);
    THROW(CodecStreamEnded);
}
