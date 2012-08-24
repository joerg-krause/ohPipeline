#ifndef HEADER_PIPELINE_MSG
#define HEADER_PIPELINE_MSG

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Exception.h>
#include <OpenHome/Private/Fifo.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Av/InfoProvider.h>

EXCEPTION(AllocatorNoMemory); // heaps are expected to be setup to avoid this.  Any instance of this exception indicates a system design error.

namespace OpenHome {
namespace Media {

class Allocated;

class AllocatorBase : private Av::IInfoProvider
{
public:
    ~AllocatorBase();
    void Free(Allocated* aPtr);
    TUint CellsTotal() const;
    TUint CellBytes() const;
    TUint CellsUsed() const;
    TUint CellsUsedMax() const;
    void GetStats(TUint& aCellsTotal, TUint& aCellBytes, TUint& aCellsUsed, TUint& aCellsUsedMax) const;
    static const Brn kQueryMemory;
protected:
    AllocatorBase(const TChar* aName, TUint aNumCells, TUint aCellBytes, Av::IInfoAggregator& aInfoAggregator);
    Allocated* DoAllocate();
private: // from Av::IInfoProvider
    void QueryInfo(const Brx& aQuery, IWriter& aWriter);
protected:
    Fifo<Allocated*> iFree;
private:
    mutable Mutex iLock;
    const TChar* iName;
    TUint iCellsTotal;
    TUint iCellBytes;
    TUint iCellsUsed;
    TUint iCellsUsedMax;
};
    
template <class T> class Allocator : public AllocatorBase
{
public:
    Allocator(const TChar* aName, TUint aNumCells, Av::IInfoAggregator& aInfoAggregator);
    T* Allocate();
};

template <class T> Allocator<T>::Allocator(const TChar* aName, TUint aNumCells, Av::IInfoAggregator& aInfoAggregator)
    : AllocatorBase(aName, aNumCells, sizeof(T), aInfoAggregator)
{
    for (TUint i=0; i<aNumCells; i++) {
        iFree.Write(new T(*this));
    }
}

template <class T> T* Allocator<T>::Allocate()
{
    return static_cast<T*>(DoAllocate());
}

class IMsgProcessor;

class Allocated
{
    friend class AllocatorBase;
public:
    void AddRef();
    void RemoveRef();
protected:
    Allocated(AllocatorBase& aAllocator);
protected:
    ~Allocated();
private:
    virtual void Clear();
protected:
    AllocatorBase& iAllocator;
private:
    Mutex iLock;
    TUint iRefCount;
};

enum EMediaDataEndian
{
    EMediaDataLittleEndian
   ,EMediaDataBigEndian
};

class DecodedAudio : public Allocated
{
    friend class MsgFactory;
public:
    static const TUint kMaxBytes = 100; // FIXME
    static const TUint kMaxSubsamples = kMaxBytes/4;
public:
    DecodedAudio(AllocatorBase& aAllocator);
    const TUint* Ptr() const;
    const TUint* PtrOffsetSamples(TUint aSamples) const;
    const TUint* PtrOffsetBytes(TUint aBytes) const;
    const TUint* PtrOffsetBytes(const TUint* aFrom, TUint aBytes) const;
    TUint Bytes() const;
    TUint BytesFromJiffies(TUint& aJiffies) const;
    TUint JiffiesFromBytes(TUint aBytes) const;
private:
    void Construct(const Brx& aData, TUint aChannels, TUint aSampleRate, TUint aBitDepth, EMediaDataEndian aEndian); // sample rate, bit-depth, num channels, const brx& (ptr/len), endianness);
    static void UnpackBigEndian(TUint32* aDst, const TUint8* aSrc, TUint aBitDepth, TUint aNumSubsamples);
    static void UnpackLittleEndian(TUint32* aDst, const TUint8* aSrc, TUint aBitDepth, TUint aNumSubsamples);
    TUint JiffiesPerSample() const;
private:
    static const TUint kBytesPerSubsample = sizeof(TUint);

    static const TUint kJiffiesPerSecond = 56448000; // lcm(384000, 352800)
    static const TUint kJiffiesMinSplit = 56448000 / 25; // can only split by Jiffies on multiples of 40ms
    static const TUint kJiffiesPerMs = kJiffiesPerSecond / 1000;

    //Number of jiffies per sample
    static const TUint kJiffies7350   = kJiffiesPerSecond / 7350;
    static const TUint kJiffies8000   = kJiffiesPerSecond / 8000;
    static const TUint kJiffies11025  = kJiffiesPerSecond / 11025;
    static const TUint kJiffies12000  = kJiffiesPerSecond / 12000;
    static const TUint kJiffies14700  = kJiffiesPerSecond / 14700;
    static const TUint kJiffies16000  = kJiffiesPerSecond / 16000;
    static const TUint kJiffies22050  = kJiffiesPerSecond / 22050;
    static const TUint kJiffies24000  = kJiffiesPerSecond / 24000;
    static const TUint kJiffies29400  = kJiffiesPerSecond / 29400;
    static const TUint kJiffies32000  = kJiffiesPerSecond / 32000;
    static const TUint kJiffies44100  = kJiffiesPerSecond / 44100;
    static const TUint kJiffies48000  = kJiffiesPerSecond / 48000;
    static const TUint kJiffies88200  = kJiffiesPerSecond / 88200;
    static const TUint kJiffies96000  = kJiffiesPerSecond / 96000;
    static const TUint kJiffies176400 = kJiffiesPerSecond / 176400;
    static const TUint kJiffies192000 = kJiffiesPerSecond / 192000;
private:
    TUint iSubsamples[kMaxSubsamples]; // one sub-sample per channel == 1 sample
    TUint iSubsampleCount;
    TUint iChannels;
    //TUint64 iSampleStart; // pos of first sample in iData within the track
    //TUint64 iSamplesTotal; // total num samples in the track
    TUint iSampleRate;
    TUint iBitRate; // what is this ??
    TUint iBitDepth;
    TUint iJiffiesPerSample; // cached on construction for convenience
    //TBool iLossless;
    //Bws<kMaxCodecNameBytes> iCodecName;
    //TUint iSamples; // bytes/4 - remove
};

class Msg : public Allocated
{
    friend class MsgQueue;
public:
    virtual void Process(IMsgProcessor& aProcessor) = 0;
protected:
    Msg(AllocatorBase& aAllocator);
private:
    Msg* iNextMsg;
};

class MsgAudio : public Msg
{
    friend class MsgFactory;
public:
    MsgAudio(AllocatorBase& aAllocator);
    MsgAudio* SplitJiffies(TUint& aAt); // returns block after aAt.  Rounds aAt down to the nearest sample boundary
    MsgAudio* SplitBytes(TUint aAt); // returns block after aAt
    void Add(MsgAudio* aMsg); // combines MsgAudio instances so they report longer durations etc
    void CopyTo(TUint* aDest); // does a RemoveRef at the end.  // FIXME - might need to pass sample rate and #channels (to allow for SplitJiffies & MsgSilence...)
    MsgAudio* Clone(); // create new MsgAudio, take ref to DecodedAudio, copy ptr/bytes
    TUint Bytes() const; // FIXME - might need to pass sample rate and #channels (to allow for SplitJiffies & MsgSilence...)
    TUint Jiffies() const;
    // MsgPlayable* Pin(TUint aSampleRate, TUint aNumChannels); // allows MsgSilence to decide how many Bytes() it has.  'real' MsgAudio asserts if asked to change to different values. // May be replaced by pipeline element
    // MsgPlayable should be only function with Bytes() or CopyTo()
    // Pin should also 'return' true/false
private:
    void Construct(DecodedAudio* aDecodedAudio);
private: // from Msg
    void Clear();
    void Process(IMsgProcessor& aProcessor);
private:
    DecodedAudio* iAudioData;
    const TUint* iPtr;
    TUint iBytes;
    TUint iOffsetBytes;
    MsgAudio* iNextAudio;
    TUint iJiffies; // not absolutely required but useful to cache this
};

class MsgTrack : public Msg
{
public:
    static const TUint kMaxBytes = 100; // FIXME
public:
    MsgTrack(AllocatorBase& aAllocator);
private: // from Msg
    void Process(IMsgProcessor& aProcessor);
private:
    // track uri & meta data
};

class MsgStartOfAudio : public Msg
{
public:
    MsgStartOfAudio(AllocatorBase& aAllocator);
private: // from Msg
    void Process(IMsgProcessor& aProcessor);
private:
    static const TUint kMaxCodecNameBytes = 32;
    //TUint64 iSamplesTotal; // total num samples in the track
    //TBool iLossless;
    //Bws<kMaxCodecNameBytes> iCodecName;
};

class MsgMetaText : public Msg
{
public:
    static const TUint kMaxBytes = 100; // FIXME
public:
    MsgMetaText(AllocatorBase& aAllocator);
private: // from Msg
    void Process(IMsgProcessor& aProcessor);
};

class IMsgProcessor
{
public:
    virtual void ProcessMsg(MsgAudio& aMsg) = 0;
    virtual void ProcessMsg(MsgTrack& aMsg) = 0;
    virtual void ProcessMsg(MsgStartOfAudio& aMsg) = 0;
    virtual void ProcessMsg(MsgMetaText& aMsg) = 0;
};

class MsgQueue
{
public:
    MsgQueue();
    ~MsgQueue();
    void Enqueue(Msg* aMsg);
    Msg* Dequeue();
private:
    Mutex iLock;
    Semaphore iSem;
    Msg* iHead;
    Msg* iTail;
};

class MsgFactory // owned by Pipeline object
{
public:
    MsgFactory(Av::IInfoAggregator& aInfoAggregator, TUint aDecodedAudioCount, TUint aMsgAudioCount, TUint aMsgTrackCount, TUint aMsgMetaTextCount);
    MsgAudio* CreateMsgAudio(const Brx& aData, TUint aChannels, TUint aSampleRate, TUint aBitDepth, EMediaDataEndian aEndian);
    MsgTrack* CreateMsgTrack();
    MsgMetaText* CreateMsgMetaText();
private:
    DecodedAudio* CreateDecodedAudio(const Brx& aData, TUint aChannels, TUint aSampleRate, TUint aBitDepth, EMediaDataEndian aEndian);
private:
    Allocator<DecodedAudio> iAllocatorDecodedAudio;
    Allocator<MsgAudio> iAllocatorMsgAudio;
    Allocator<MsgTrack> iAllocatorMsgTrack;
    Allocator<MsgMetaText> iAllocatorMsgMetaText;
};

} // namespace Media
} // namespace OpenHome

#endif // HEADER_PIPELINE_MSG
