#ifndef HEADER_PIPELINE_CODEC_FLAC
#define HEADER_PIPELINE_CODEC_FLAC

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Media/Codec/CodecController.h>
#include <OpenHome/Media/Msg.h>
#include <FLAC/stream_decoder.h>

namespace OpenHome {
namespace Media {
namespace Codec {

class CodecFlac : public CodecBase
{
public:
    CodecFlac();
    ~CodecFlac();
private: // from CodecBase
    TBool Recognise(const Brx& aData);
    void Process();
    void StreamCompleted();
private:
    void Initialise();
public:
    FLAC__StreamDecoderReadStatus CallbackRead(const FLAC__StreamDecoder* aDecoder,
                                               TUint8 aBuffer[],  TUint* aBytes);
    FLAC__StreamDecoderSeekStatus CallbackSeek(const FLAC__StreamDecoder* aDecoder,
                                               TUint64 aOffsetBytes);
    FLAC__StreamDecoderTellStatus CallbackTell(const FLAC__StreamDecoder* aDecoder,
                                               TUint64* aOffsetBytes);
    FLAC__StreamDecoderLengthStatus CallbackLength(const FLAC__StreamDecoder* aDecoder,
                                                   TUint64* aStreamBytes);
    TBool CallbackEof(const FLAC__StreamDecoder* aDecoder);
    FLAC__StreamDecoderWriteStatus CallbackWrite(const FLAC__StreamDecoder* aDecoder,
                                                 const FLAC__Frame* aFrame,
                                                 const TInt32* const aBuffer[]);
    void CallbackMetadata(const FLAC__StreamDecoder* aDecoder,
                          const FLAC__StreamMetadata* aMetadata);

    void CallbackError(const FLAC__StreamDecoder* aDecoder,
                       FLAC__StreamDecoderErrorStatus aStatus);
private:
    TByte iBuf[DecodedAudio::kMaxBytes];
    FLAC__StreamDecoder* iDecoder;
    Brn iName;
    TUint64 iTrackOffset;
    TBool iMsgFormatSent;
    TBool iOgg;
};

}; // namespace Codec
}; // namespace Media
}; // namespace OpenHome

#endif // HEADER_PIPELINE_CODEC_FLAC
