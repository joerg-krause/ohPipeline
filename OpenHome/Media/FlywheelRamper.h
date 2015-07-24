#ifndef HEADER_FLYWHEEL_RAMPER
#define HEADER_FLYWHEEL_RAMPER

#include <OpenHome/Types.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Media/Pipeline/Msg.h>
#include <vector>

namespace OpenHome {
namespace Media {


///////////////////////////////////////////////////////////////////////////////////////////
//
// FlywheelRamper: uses an algorthim to generate ramp audio aRampMs long.
//
// Needs to be fed a block of audio to allow it to generate ramp.
// Generation audio block must be exactly aGenerationJiffies long
// Client supplies generation audio via IPcmProcessor object returned via Ramp method.
// Ramp audio is written to IWriter specified in Ramp method
//
// Generation audio is assumed to be 32bit in big endian format
// Ramp audio is 32bit and written out in big endian format
///////////////////////////////////////////////////////////////////////////////////////////

class FlywheelRamper
{
    friend class PcmProcessorFwr;

public:
    static const TUint kMaxChannelCount = 2;
    static const TUint kBytesPerSample = 4; // 32 bit audio

public:
    FlywheelRamper(TUint aGenerationJiffies, TUint aRampMs); // audio quantity needed to generate a ramp, ramp duration

    IPcmProcessor& Ramp(IWriter& aWriter, TUint aSampleRate, TUint aChannelCount);
    TUint GenerationJiffies() const;
    TUint RampMs() const;

    static TUint Bytes(TUint iSampleRate, TUint aChannelCount, TUint aJiffies, TUint aBytesPerSample);
    static void LogBuf(const Brx& aBuf);
    static TInt32 Int32(Bwx& aSample, TUint aIndex=0);


private:
    void CreateRamp(const Brx& aGenerationSamples);
    void ApplyRamp(Bwx& aSamples);
    void ScaleSample(Bwx& aSample, TUint aScaleFactor);

private:
    TUint iGenerationJiffies;
    TUint iRampMs;
    Bwh iRampSamples;
    IPcmProcessor* iProcessor;
    IWriter* iWriter;
    TUint iSampleRate;
    TUint iChannelCount;

};


///////////////////////////////////////////////////////////////////////////////////////////
//
// PcmProcessorFwr : Used to feed audio to FlywheelRamper object
//
// Audio can be passed in one complete block or multiple fragments/samples
// Begin block must be called once before any audio is passed
// End block must be called once when complete audio block has been passed
//
// Prior to feeding entire audio block to FlywheelRamper...
// 8 bit audio samples are converted to 32bit (by padding lower 24bits with 0 or 0xffffff)
// 16 bit audio samples are converted to 32bit (by padding lower 16bits with 0 or 0xffff)
// 24 bit audio samples are converted to 32bit (by padding lower 8bits with 0 or 0xff)
///////////////////////////////////////////////////////////////////////////////////////////

class PcmProcessorFwr : public IPcmProcessor
{
public:
    PcmProcessorFwr(FlywheelRamper& aFwr, TUint aGenerationJiffies);

    // IPcmProcessor
    virtual void BeginBlock();
    virtual TBool ProcessFragment8(const Brx& aData, TUint aNumChannels);
    virtual TBool ProcessFragment16(const Brx& aData, TUint aNumChannels);
    virtual TBool ProcessFragment24(const Brx& aData, TUint aNumChannels);
    virtual void ProcessSample8(const TByte* aSample, TUint aNumChannels);
    virtual void ProcessSample16(const TByte* aSample, TUint aNumChannels);
    virtual void ProcessSample24(const TByte* aSample, TUint aNumChannels);
    virtual void EndBlock();

private:
    FlywheelRamper& iFwr;
    Bwh iSamples;
    TUint iNumChannels;
};


////////////////////////////////////////////////////////////////////////////
//
// ConvolutionModel:
//
// Constructed with a list of 32 bit coefficients
//
// Process method takes in a block of 32 bit samples, and a count
// Outputs a new block of 32 bit samples.
// Count determines the number of samples in output block.
//
// Max count = no. of samples in input block + no. of coeffs - 1
//
// The value of each sample in output block is calculated by:
//
// sampleOut n = coeff(1)*sampleIn(n) + coeff(2)*sampleIn(n-1) + coeff(3)*sampleIn(n-2)...
//
// for n=1 to x (where x= num samples in + num coeffs -1):

class ConvolutionModel : public INonCopyable
{
private:
    static const TUint kScaleShiftBase = 23;

public:
    ConvolutionModel(const std::vector<TInt32>& aCoeffs, TUint aCoeffScaling, TUint aDataInScaling, TUint aDataOutScaling);

    void Process(const Brx& aSamplesIn,  Bwx& aSamplesOut, TUint aCount);
    void Process(const Brx& aSamplesIn, Bwx& aSamplesOut);

private:
    const std::vector<TInt32> iCoeffs;
    std::vector<TInt32> iSamples;
    TUint iCoeffScaling;
    TUint iDataInScaling;
    TUint iDataOutScaling;
    TUint iScaleShift;
};

//////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//
// FeedbackModel:
//
// Constructed with a list of 32 bit coefficients
//
// Process method takes in a block of 32 bit samples, and a count
// Outputs a new block of 32 bit samples.
// Count determines the number of samples in output block.
//
// Max count = no. of samples in input block + no. of coeffs
//
// The value of each sample in output block is calculated by:
//
// sampleOut n = sampleIn(n) + coeff(1)*sampleIn(n-1) + coeff(2)*sampleIn(n-2)...
//
// for n=1 to x (where x= num samples in + num coeffs):

class FeedbackModel : public INonCopyable
{
public:
    FeedbackModel(const std::vector<TInt32>& aCoeffs, TUint aCoeffScaling, TUint aDataInScaling, TUint aDataOutScaling);

    void Process(const Brx& aSamplesIn, Bwx& aSamplesOut, TUint aCount);
    void Process(const Brx& aSamplesIn, Bwx& aSamplesOut);

private:
    const std::vector<TInt32> iCoeffs;
    std::vector<TInt32> iSamples;
    TUint iCoeffScaling;
    TUint iDataInScaling;
    TUint iDataOutScaling;
    //TUint iScaleShift;
};



} // Media
} // OpenHome


#endif // HEADER_FLYWHEEL_RAMPER
