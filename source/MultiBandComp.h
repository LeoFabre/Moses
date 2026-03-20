/*
*   Multiband Compressor Module
*       by Jacob Curtis
*
*   Originally inspired by Daniel Rudrich's "Simple Compressor"
*   https://github.com/DanielRudrich/SimpleCompressor
*
*   Envelope implementaiton from:
*   https://christianfloisand.wordpress.com/2014/06/09/dynamics-processing-compressorlimiter-part-1/
*
*/

#pragma once

#define numOutputs 2
#define numBands 4
#include <array>
#include <juce_dsp/juce_dsp.h>

// ---------------------------------------------------------------------------
// SIMD support — ARM NEON (primary: PocketBeagle 2 / Cortex-A53)
//                x86 SSE2  (desktop development / testing)
// ---------------------------------------------------------------------------
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  #include <arm_neon.h>
  #define MOSES_USE_SIMD 1
  #define MOSES_USE_NEON 1
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
  #include <emmintrin.h>
  #define MOSES_USE_SIMD 1
  #define MOSES_USE_SSE2 1
#endif

using namespace juce;

// ---------------------------------------------------------------------------
// SIMDHelpers — thin platform abstraction + portable transcendentals
//
// Each platform branch defines Vec4f/Vec4i/Vec4u and a common set of
// inline helper functions.  log_ps / exp_ps (and their log10 / pow10
// wrappers) are then written once against that interface so the algorithm
// is never duplicated.
//
// log_ps / exp_ps adapted from Julien Pommier's sse_mathfun (zlib licence,
// http://gruntthepeon.free.fr/ssemath/), accurate to ~1 ULP for normals.
// ---------------------------------------------------------------------------
#ifdef MOSES_USE_SIMD
namespace SIMDHelpers
{

// ---- NEON (ARM) ------------------------------------------------------------
#ifdef MOSES_USE_NEON

using Vec4f = float32x4_t;
using Vec4i = int32x4_t;
using Vec4u = uint32x4_t;

inline Vec4f broadcast (float s)            { return vdupq_n_f32 (s); }
inline Vec4f zero()                         { return vdupq_n_f32 (0.0f); }
inline Vec4f vadd (Vec4f a, Vec4f b)        { return vaddq_f32 (a, b); }
inline Vec4f vsub (Vec4f a, Vec4f b)        { return vsubq_f32 (a, b); }
inline Vec4f vmul (Vec4f a, Vec4f b)        { return vmulq_f32 (a, b); }
inline Vec4f vmadd (Vec4f acc, Vec4f a, Vec4f b) { return vmlaq_f32 (acc, a, b); } // acc + a*b
inline Vec4f vmin (Vec4f a, Vec4f b)        { return vminq_f32 (a, b); }
inline Vec4f vmax (Vec4f a, Vec4f b)        { return vmaxq_f32 (a, b); }
inline Vec4f vabs (Vec4f a)                 { return vabsq_f32 (a); }
inline Vec4f vload (const float* p)         { return vld1q_f32 (p); }
inline void  vstore (float* p, Vec4f v)     { vst1q_f32 (p, v); }

// lane 0 = a0, lane 1 = a1, lane 2 = a2, lane 3 = a3
inline Vec4f vset4 (float a0, float a1, float a2, float a3)
{
    const float arr[4] = { a0, a1, a2, a3 };
    return vld1q_f32 (arr);
}

// Float comparisons — all-1s lanes where true, all-0s where false
inline Vec4f vcmplt_f (Vec4f a, Vec4f b) { return vreinterpretq_f32_u32 (vcltq_f32 (a, b)); }
inline Vec4f vcmpgt_f (Vec4f a, Vec4f b) { return vreinterpretq_f32_u32 (vcgtq_f32 (a, b)); }

// Bitwise ops on float vectors
inline Vec4f vand (Vec4f a, Vec4f b) {
    return vreinterpretq_f32_u32 (vandq_u32 (vreinterpretq_u32_f32 (a), vreinterpretq_u32_f32 (b)));
}
inline Vec4f vandnot (Vec4f a, Vec4f b) { // ~a & b
    return vreinterpretq_f32_u32 (vbicq_u32 (vreinterpretq_u32_f32 (b), vreinterpretq_u32_f32 (a)));
}
inline Vec4f vor (Vec4f a, Vec4f b) {
    return vreinterpretq_f32_u32 (vorrq_u32 (vreinterpretq_u32_f32 (a), vreinterpretq_u32_f32 (b)));
}

// Horizontal min — pairwise reduction via vpmin_f32
inline float hmin (Vec4f v)
{
    float32x2_t lo = vget_low_f32 (v);
    float32x2_t hi = vget_high_f32 (v);
    float32x2_t p  = vpmin_f32 (lo, hi);
    p = vpmin_f32 (p, p);
    return vget_lane_f32 (p, 0);
}

// Integer helpers for log_ps / exp_ps
inline Vec4u castToUint (Vec4f v)     { return vreinterpretq_u32_f32 (v); }
inline Vec4f castFromUint (Vec4u v)   { return vreinterpretq_f32_u32 (v); }
inline Vec4f castFromInt (Vec4i v)    { return vreinterpretq_f32_s32 (v); }
inline Vec4i shrI23 (Vec4u v)         { return vreinterpretq_s32_u32 (vshrq_n_u32 (v, 23)); }
inline Vec4i subI (Vec4i a, Vec4i b)  { return vsubq_s32 (a, b); }
inline Vec4i addI (Vec4i a, Vec4i b)  { return vaddq_s32 (a, b); }
inline Vec4i shlI23 (Vec4i v)         { return vshlq_n_s32 (v, 23); }
inline Vec4i set1i (int s)            { return vdupq_n_s32 (s); }
inline Vec4u set1u (unsigned s)       { return vdupq_n_u32 (s); }
inline Vec4u andU (Vec4u a, Vec4u b)  { return vandq_u32 (a, b); }
inline Vec4u orU  (Vec4u a, Vec4u b)  { return vorrq_u32 (a, b); }
inline Vec4i cvtToInt (Vec4f v)       { return vcvtq_s32_f32 (v); } // truncates toward zero
inline Vec4f cvtToFloat (Vec4i v)     { return vcvtq_f32_s32 (v); }

// ---- SSE2 (x86) ------------------------------------------------------------
#elif defined (MOSES_USE_SSE2)

using Vec4f = __m128;
using Vec4i = __m128i;
using Vec4u = __m128i;

inline Vec4f broadcast (float s)            { return _mm_set1_ps (s); }
inline Vec4f zero()                         { return _mm_setzero_ps(); }
inline Vec4f vadd (Vec4f a, Vec4f b)        { return _mm_add_ps (a, b); }
inline Vec4f vsub (Vec4f a, Vec4f b)        { return _mm_sub_ps (a, b); }
inline Vec4f vmul (Vec4f a, Vec4f b)        { return _mm_mul_ps (a, b); }
inline Vec4f vmadd (Vec4f acc, Vec4f a, Vec4f b) { return _mm_add_ps (acc, _mm_mul_ps (a, b)); }
inline Vec4f vmin (Vec4f a, Vec4f b)        { return _mm_min_ps (a, b); }
inline Vec4f vmax (Vec4f a, Vec4f b)        { return _mm_max_ps (a, b); }
inline Vec4f vabs (Vec4f a)                 { return _mm_and_ps (a, _mm_castsi128_ps (_mm_set1_epi32 (0x7FFFFFFF))); }
inline Vec4f vload (const float* p)         { return _mm_loadu_ps (p); }
inline void  vstore (float* p, Vec4f v)     { _mm_storeu_ps (p, v); }

// lane 0 = a0, ..., lane 3 = a3  (_mm_set_ps order is reversed)
inline Vec4f vset4 (float a0, float a1, float a2, float a3) { return _mm_set_ps (a3, a2, a1, a0); }

inline Vec4f vcmplt_f (Vec4f a, Vec4f b)    { return _mm_cmplt_ps (a, b); }
inline Vec4f vcmpgt_f (Vec4f a, Vec4f b)    { return _mm_cmpgt_ps (a, b); }
inline Vec4f vand (Vec4f a, Vec4f b)        { return _mm_and_ps (a, b); }
inline Vec4f vandnot (Vec4f a, Vec4f b)     { return _mm_andnot_ps (a, b); } // ~a & b
inline Vec4f vor (Vec4f a, Vec4f b)         { return _mm_or_ps (a, b); }

inline float hmin (Vec4f v)
{
    Vec4f s = _mm_shuffle_ps (v, v, _MM_SHUFFLE (2, 3, 0, 1));
    v = _mm_min_ps (v, s);
    s = _mm_shuffle_ps (v, v, _MM_SHUFFLE (1, 0, 3, 2));
    v = _mm_min_ps (v, s);
    float r; _mm_store_ss (&r, v); return r;
}

inline Vec4u castToUint (Vec4f v)     { return _mm_castps_si128 (v); }
inline Vec4f castFromUint (Vec4u v)   { return _mm_castsi128_ps (v); }
inline Vec4f castFromInt (Vec4i v)    { return _mm_castsi128_ps (v); }
inline Vec4i shrI23 (Vec4u v)         { return _mm_srli_epi32 (v, 23); }
inline Vec4i subI (Vec4i a, Vec4i b)  { return _mm_sub_epi32 (a, b); }
inline Vec4i addI (Vec4i a, Vec4i b)  { return _mm_add_epi32 (a, b); }
inline Vec4i shlI23 (Vec4i v)         { return _mm_slli_epi32 (v, 23); }
inline Vec4i set1i (int s)            { return _mm_set1_epi32 (s); }
inline Vec4u set1u (unsigned s)       { return _mm_set1_epi32 ((int) s); }
inline Vec4u andU (Vec4u a, Vec4u b)  { return _mm_and_si128 (a, b); }
inline Vec4u orU  (Vec4u a, Vec4u b)  { return _mm_or_si128 (a, b); }
inline Vec4i cvtToInt (Vec4f v)       { return _mm_cvttps_epi32 (v); }
inline Vec4f cvtToFloat (Vec4i v)     { return _mm_cvtepi32_ps (v); }

#endif // MOSES_USE_NEON / MOSES_USE_SSE2

// ---------------------------------------------------------------------------
// Transcendentals — written once using the helpers above, compiled for
// whichever platform is active.
// ---------------------------------------------------------------------------

inline Vec4f log_ps (Vec4f x)
{
    const Vec4f one = broadcast (1.0f);

    x = vmax (x, castFromUint (set1u (0x00800000u))); // clamp to min normal float

    // Extract biased exponent and subtract 127
    Vec4u xi   = castToUint (x);
    Vec4i emm0 = subI (shrI23 (xi), set1i (0x7f));

    // Normalise mantissa to [0.5, 1): clear exponent bits, set to 0.5's exponent
    x = castFromUint (orU (andU (xi, set1u (~0x7f800000u)), castToUint (broadcast (0.5f))));

    Vec4f e = cvtToFloat (emm0);
    e = vadd (e, one);

    // Cephes trick: if x < sqrt(0.5) correct e and x
    Vec4f mask = vcmplt_f (x, broadcast (0.707106781186547524f));
    Vec4f tmp  = vand (x, mask);
    x = vsub (x, one);
    e = vsub (e, vand (one, mask));
    x = vadd (x, tmp);

    Vec4f z = vmul (x, x);

    // 8th-order Horner for ln(1+x), x in [-0.293, 0.414]
    Vec4f y = broadcast ( 7.0376836292e-2f);
    y = vmadd (broadcast (-1.1514610310e-1f), y, x);
    y = vmadd (broadcast ( 1.1676998740e-1f), y, x);
    y = vmadd (broadcast (-1.2420140846e-1f), y, x);
    y = vmadd (broadcast ( 1.4249322787e-1f), y, x);
    y = vmadd (broadcast (-1.6668057665e-1f), y, x);
    y = vmadd (broadcast ( 2.0000714765e-1f), y, x);
    y = vmadd (broadcast (-2.4999993993e-1f), y, x);
    y = vmadd (broadcast ( 3.3333331174e-1f), y, x);
    y = vmul (y, x);
    y = vmul (y, z);

    tmp = vmul (e, broadcast (-2.12194440e-4f));
    y   = vadd (y, tmp);
    tmp = vmul (z, broadcast (0.5f));
    y   = vsub (y, tmp);
    tmp = vmul (e, broadcast (0.693359375f));
    x   = vadd (x, y);
    x   = vadd (x, tmp);
    return x;
}

inline Vec4f exp_ps (Vec4f x)
{
    const Vec4f one = broadcast (1.0f);

    x = vmin (x, broadcast ( 88.3762626647949f));
    x = vmax (x, broadcast (-88.3762626647949f));

    // fx = round (x * log2(e)) via floor(x*log2(e) + 0.5)
    Vec4f fx = vmadd (broadcast (0.5f), x, broadcast (1.44269504088896341f));

    // floor via truncate-toward-zero + correction for negatives
    Vec4i emm0 = cvtToInt (fx);
    Vec4f tmp  = cvtToFloat (emm0);
    Vec4f corr = vand (one, vcmpgt_f (tmp, fx));
    fx = vsub (tmp, corr);

    // Range reduction: x -= fx * ln2
    tmp = vmul (fx, broadcast (0.693359375f));
    Vec4f z = vmul (fx, broadcast (-2.12194440e-4f));
    x = vsub (x, tmp);
    x = vsub (x, z);
    z = vmul (x, x);

    // 6th-order Horner for e^x near 0
    Vec4f y = broadcast (1.9875691500e-4f);
    y = vmadd (broadcast (1.3981999507e-3f), y, x);
    y = vmadd (broadcast (8.3334519073e-3f), y, x);
    y = vmadd (broadcast (4.1665795894e-2f), y, x);
    y = vmadd (broadcast (1.6666665459e-1f), y, x);
    y = vmadd (broadcast (5.0000001201e-1f), y, x);
    y = vmul (y, z);
    y = vadd (y, x);
    y = vadd (y, one);

    // Multiply by 2^floor(fx) via IEEE 754 exponent field
    emm0 = cvtToInt (fx);
    emm0 = addI (emm0, set1i (0x7f));
    emm0 = shlI23 (emm0);
    return vmul (y, castFromInt (emm0));
}

// log10(x) = ln(x) * log10(e)
inline Vec4f log10_ps (Vec4f x) { return vmul (log_ps (x), broadcast (0.4342944819032518f)); }
// 10^x = e^(x * ln(10))
inline Vec4f pow10_ps (Vec4f x) { return exp_ps (vmul (x, broadcast (2.302585092994046f))); }

} // namespace SIMDHelpers
#endif // MOSES_USE_SIMD


struct Parameters {
    float crossoverFreqA, crossoverFreqB, crossoverFreqC;
    std::array<float, numBands> threshold;
    std::array<float, numBands> attackTime;
    std::array<float, numBands> releaseTime;
    std::array<float, numBands> slope;
    std::array<float, numBands> makeUpGain;
    bool stereo{ true };
    std::array<bool, numBands> listen{ false, false, false, false };
    std::array<bool, numBands> kill{ false, false, false, false };
};

class MultiBandComp
{
public:
    void setParameters(const AudioProcessorValueTreeState& apvts)
    {
        setCrossovers(apvts);
        anyListen = false;
        parameters.stereo = apvts.getRawParameterValue("stereo")->load();
        for (int band = 0; band < numBands; band++)
        {
            const auto bandNum = String(band + 1);
            parameters.threshold[band] = apvts.getRawParameterValue("threshold" + bandNum)->load();
            parameters.makeUpGain[band] = apvts.getRawParameterValue("makeUp" + bandNum)->load();
            const float attackInput = apvts.getRawParameterValue("attack" + bandNum)->load();
            parameters.attackTime[band] = std::exp(
                -1.0f / ((attackInput / 1000.0f) * static_cast<float>(sampleRate)));
            const float releaseInput = apvts.getRawParameterValue("release" + bandNum)->load();
            parameters.releaseTime[band] = std::exp(
                -1.0f / ((releaseInput / 1000.0f) * static_cast<float>(sampleRate)));
            const float ratio = apvts.getRawParameterValue("ratio" + bandNum)->load();
            parameters.slope[band] = 1.0f - (1.0f / ratio);
            parameters.kill[band] = apvts.getRawParameterValue("kill" + bandNum)->load();
            parameters.listen[band] = apvts.getRawParameterValue("listen" + bandNum)->load();
            if (parameters.listen[band])
            {
                anyListen = true;
            }
        }
    }

    void prepare(double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        bufferSize = maxBlockSize;
        dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = maxBlockSize;
        spec.numChannels = numOutputs;
        stage1LowBuffer.setSize(numOutputs, maxBlockSize);
        stage1HighBuffer.setSize(numOutputs, maxBlockSize);
        for (int band = 0; band < numBands; band++)
        {
            bandBuffers[band].setSize(numOutputs, maxBlockSize);
            envelopeBuffers[band].setSize(numOutputs, maxBlockSize);
            bandChains[band].prepare(spec);
        }
        stage1LowChain.prepare(spec);
        stage1HighChain.prepare(spec);
    }

    void process(AudioBuffer<float>& inputBuffer)
    {
        // stage 1
        stage1LowBuffer.makeCopyOf(inputBuffer, true);
        stage1HighBuffer.makeCopyOf(inputBuffer, true);
        applyStage1Filters();
        // stage 2
        bandBuffers[0].makeCopyOf(stage1LowBuffer, true);
        bandBuffers[1].makeCopyOf(stage1LowBuffer, true);
        bandBuffers[2].makeCopyOf(stage1HighBuffer, true);
        bandBuffers[3].makeCopyOf(stage1HighBuffer, true);
        applyStage2Filters();
        // compression
        createEnvelopes();
        applyCompression();
        outputActiveBands(inputBuffer);
    }

    std::array<std::array<float, numOutputs>, numBands> getGainReduction()
    {
        std::array<std::array<float, numOutputs>, numBands> output;
        for (int band = 0; band < numBands; band++)
        {
            for (int channel = 0; channel < numOutputs; channel++)
            {
                output[band][channel] = outputGainReduction[channel + band * 2] * -1.0f;
            }
        }
        return output;
    }

    std::array<std::array<float, numOutputs>, numBands> getOutputLevels()
    {
        return outputLevels;
    }


private:
    void setCrossovers(const AudioProcessorValueTreeState& apvts)
    {
        const float tempA = apvts.getRawParameterValue("crossoverFreqA")->load();
        const float tempB = apvts.getRawParameterValue("crossoverFreqB")->load();
        const float tempC = apvts.getRawParameterValue("crossoverFreqC")->load();
        // ensure crossovers don't overlap: B -> A -> C
        parameters.crossoverFreqA = jmax(tempA, tempB * 1.25f);
        parameters.crossoverFreqB = jmin(tempB, tempA * 0.8f);
        parameters.crossoverFreqA = jmin(parameters.crossoverFreqA, tempC * 0.8f);
        parameters.crossoverFreqC = jmax(tempC, tempA * 1.25f);
        // send new values to parameters
        apvts.getParameter("crossoverFreqA")->setValueNotifyingHost(
            freqRange.convertTo0to1(parameters.crossoverFreqA));
        apvts.getParameter("crossoverFreqB")->setValueNotifyingHost(
            freqRange.convertTo0to1(parameters.crossoverFreqB));
        apvts.getParameter("crossoverFreqC")->setValueNotifyingHost(
            freqRange.convertTo0to1(parameters.crossoverFreqC));
    }

    void applyStage1Filters()
    {
        stage1LowChain.get<0>().setType(dsp::LinkwitzRileyFilterType::lowpass);
        stage1LowChain.get<0>().setCutoffFrequency(parameters.crossoverFreqA);
        stage1LowChain.get<1>().setType(dsp::LinkwitzRileyFilterType::allpass);
        stage1LowChain.get<1>().setCutoffFrequency(parameters.crossoverFreqC);
        stage1HighChain.get<0>().setType(dsp::LinkwitzRileyFilterType::highpass);
        stage1HighChain.get<0>().setCutoffFrequency(parameters.crossoverFreqA);
        stage1HighChain.get<1>().setType(dsp::LinkwitzRileyFilterType::allpass);
        stage1HighChain.get<1>().setCutoffFrequency(parameters.crossoverFreqB);
        dsp::AudioBlock<float> lowBlock(stage1LowBuffer);
        dsp::AudioBlock<float> highBlock(stage1HighBuffer);
        dsp::ProcessContextReplacing<float> lowContext(lowBlock);
        dsp::ProcessContextReplacing<float> highContext(highBlock);
        stage1LowChain.process(lowContext);
        stage1HighChain.process(highContext);
    }

    void applyStage2Filters()
    {
        bandChains[0].setType(dsp::LinkwitzRileyFilterType::lowpass);
        bandChains[0].setCutoffFrequency(parameters.crossoverFreqB);
        bandChains[1].setType(dsp::LinkwitzRileyFilterType::highpass);
        bandChains[1].setCutoffFrequency(parameters.crossoverFreqB);
        bandChains[2].setType(dsp::LinkwitzRileyFilterType::lowpass);
        bandChains[2].setCutoffFrequency(parameters.crossoverFreqC);
        bandChains[3].setType(dsp::LinkwitzRileyFilterType::highpass);
        bandChains[3].setCutoffFrequency(parameters.crossoverFreqC);
        for (int band = 0; band < numBands; band++)
        {
            dsp::AudioBlock<float> block(bandBuffers[band]);
            dsp::ProcessContextReplacing<float> context(block);
            bandChains[band].process(context);
        }
    }

    void applyHisteresis(float& compLevel, float inputSample, int band)
    {
        float histeresis = (compLevel < inputSample) ?
            parameters.attackTime[band] : parameters.releaseTime[band];
        compLevel = inputSample + histeresis * (compLevel - inputSample);
    }

    // -----------------------------------------------------------------------
    // Envelope detection
    // -----------------------------------------------------------------------

    // Dispatches to the SIMD stereo path or the scalar path.
    void createEnvelopes()
    {
#ifdef MOSES_USE_SIMD
        if (parameters.stereo)
        {
            createEnvelopesStereoSIMD();
            return;
        }
#endif
        createEnvelopesScalar();
    }

    // Original scalar implementation (used for mono mode and non-SSE2 builds).
    void createEnvelopesScalar()
    {
        for (int sample = 0; sample < bufferSize; sample++)
        {
            for (int band = 0; band < numBands; band++)
            {
                if (parameters.stereo)
                {
                    const float maxSample = jmax(std::abs(bandBuffers[band].getSample(0, sample)),
                        std::abs(bandBuffers[band].getSample(1, sample)));
                    applyHisteresis(compressionLevel[band], maxSample, band);
                    for (int channel = 0; channel < numOutputs; channel++)
                    {
                        envelopeBuffers[band].setSample(channel, sample, compressionLevel[band]);
                    }
                }
                else
                {
                    for (int channel = 0; channel < numOutputs; channel++)
                    {
                        const float inputSample = std::abs(
                            bandBuffers[band].getSample(channel, sample));
                        applyHisteresis(compressionLevel[channel + band * 2], inputSample, band);
                        envelopeBuffers[band].setSample(
                            channel, sample, compressionLevel[channel + band * 2]);
                    }
                }
            }
        }
    }

#ifdef MOSES_USE_SIMD
    // SIMD stereo envelope detection.
    //
    // All 4 bands are independent per sample, so we pack them into one vector
    // register and execute the hysteresis IIR in a single vector pass per sample.
    // The per-sample IIR dependency within each band is preserved because samples
    // are processed sequentially — only the band dimension is vectorised.
    void createEnvelopesStereoSIMD()
    {
        using namespace SIMDHelpers;

        // Pack per-band time constants into SIMD registers once per block.
        Vec4f attackTimes  = vset4 (parameters.attackTime[0],  parameters.attackTime[1],
                                    parameters.attackTime[2],  parameters.attackTime[3]);
        Vec4f releaseTimes = vset4 (parameters.releaseTime[0], parameters.releaseTime[1],
                                    parameters.releaseTime[2], parameters.releaseTime[3]);

        // Restore running compression levels (indices 0..3 for stereo mode).
        Vec4f compLevels = vset4 (compressionLevel[0], compressionLevel[1],
                                  compressionLevel[2], compressionLevel[3]);

        // Pre-fetch buffer pointers to avoid per-sample virtual dispatch.
        const float* ch0[numBands], *ch1[numBands];
        float* env0[numBands], *env1[numBands];
        for (int b = 0; b < numBands; b++)
        {
            ch0[b]  = bandBuffers[b].getReadPointer (0);
            ch1[b]  = bandBuffers[b].getReadPointer (1);
            env0[b] = envelopeBuffers[b].getWritePointer (0);
            env1[b] = envelopeBuffers[b].getWritePointer (1);
        }

        for (int sample = 0; sample < bufferSize; sample++)
        {
            // Gather one sample from each band, take abs, then max across channels.
            Vec4f abs0      = vabs (vset4 (ch0[0][sample], ch0[1][sample], ch0[2][sample], ch0[3][sample]));
            Vec4f abs1      = vabs (vset4 (ch1[0][sample], ch1[1][sample], ch1[2][sample], ch1[3][sample]));
            Vec4f maxSample = vmax (abs0, abs1);

            // Select attack or release coefficient per band.
            Vec4f mask   = vcmplt_f (compLevels, maxSample);
            Vec4f coeffs = vor (vand (mask, attackTimes), vandnot (mask, releaseTimes));

            // First-order IIR: compLevel = input + coeff * (compLevel - input)
            compLevels = vadd (maxSample, vmul (coeffs, vsub (compLevels, maxSample)));

            // Scatter the 4 band values back to their respective envelope buffers.
            float cl[4];
            vstore (cl, compLevels);
            for (int b = 0; b < numBands; b++)
            {
                env0[b][sample] = cl[b];
                env1[b][sample] = cl[b];
            }
        }

        // Persist updated compression levels for the next block.
        float cl[4];
        vstore (cl, compLevels);
        for (int b = 0; b < numBands; b++)
            compressionLevel[b] = cl[b];
    }
#endif // MOSES_USE_SIMD

    // -----------------------------------------------------------------------
    // Compression gain application
    // -----------------------------------------------------------------------

    // Dispatches to SIMD or scalar.
    void applyCompression()
    {
#ifdef MOSES_USE_SIMD
        applyCompressionSIMD();
#else
        applyCompressionScalar();
#endif
    }

    // Scalar fallback — also fixes the original O(N²) getRMSLevel bug by
    // moving the metering call outside the sample loop.
    void applyCompressionScalar()
    {
        for (int band = 0; band < numBands * numOutputs; band++)
            outputGainReduction[band] = 0.0f;

        for (int sample = 0; sample < bufferSize; sample++)
        {
            for (int band = 0; band < numBands; band++)
            {
                for (int channel = 0; channel < numOutputs; channel++)
                {
                    // apply threshold and ratio to envelope
                    float currentGainReduction = parameters.slope[band] *
                        (parameters.threshold[band] - Decibels::gainToDecibels(
                            envelopeBuffers[band].getSample(channel, sample)));
                    // remove positive gain reduction
                    currentGainReduction = jmin(0.0f, currentGainReduction);
                    // set gr meter values
                    outputGainReduction[channel + band * 2] = jmin(
                        currentGainReduction, outputGainReduction[channel + band * 2]);
                    // convert decibels to gain and add makeup gain
                    currentGainReduction = std::pow(10.0f,
                        0.05f * (currentGainReduction + parameters.makeUpGain[band]));
                    // apply compression to buffer
                    bandBuffers[band].setSample(channel, sample,
                        bandBuffers[band].getSample(channel, sample) * currentGainReduction);
                }
            }
        }
        // Compute output levels once per block (not per sample as in original).
        for (int band = 0; band < numBands; band++)
            for (int channel = 0; channel < numOutputs; channel++)
                outputLevels[band][channel] = bandBuffers[band].getRMSLevel(channel, 0, bufferSize);
    }

#ifdef MOSES_USE_SIMD
    // SIMD compression gain application.
    //
    // Processes bufferSize samples in chunks of 4 using SIMD log10 / pow10
    // approximations, replacing the per-sample scalar transcendentals.
    // A scalar tail handles the remaining 0–3 samples when bufferSize % 4 != 0.
    void applyCompressionSIMD()
    {
        using namespace SIMDHelpers;

        for (int i = 0; i < numBands * numOutputs; i++)
            outputGainReduction[i] = 0.0f;

        for (int band = 0; band < numBands; band++)
        {
            const Vec4f slope         = broadcast (parameters.slope[band]);
            const Vec4f thresh        = broadcast (parameters.threshold[band]);
            const Vec4f makeUp        = broadcast (parameters.makeUpGain[band]);
            const Vec4f zeroV         = zero();
            const Vec4f twenty        = broadcast (20.0f);
            const Vec4f pointZeroFive = broadcast (0.05f);

            for (int channel = 0; channel < numOutputs; channel++)
            {
                float*       audioPtr = bandBuffers[band].getWritePointer (channel);
                const float* envPtr   = envelopeBuffers[band].getReadPointer (channel);

                Vec4f minGR = zeroV; // accumulates peak gain reduction for metering

                // ---- 4-wide SIMD loop ----------------------------------------
                int sample = 0;
                for (; sample <= bufferSize - 4; sample += 4)
                {
                    // gainReductionDB = slope * (threshold - 20 * log10(env))
                    Vec4f env  = vload (envPtr + sample);
                    Vec4f dB   = vmul (twenty, log10_ps (env));
                    Vec4f gr   = vmul (slope, vsub (thresh, dB));
                    gr = vmin (gr, zeroV); // clamp to <= 0 dB

                    minGR = vmin (minGR, gr);

                    // Linear gain = 10^(0.05 * (gr + makeUpGain))
                    Vec4f gain  = pow10_ps (vmul (pointZeroFive, vadd (gr, makeUp)));
                    Vec4f audio = vload (audioPtr + sample);
                    vstore (audioPtr + sample, vmul (audio, gain));
                }

                // Horizontal min: reduce 4 lanes to a single peak-GR scalar.
                outputGainReduction[channel + band * 2] = jmin (hmin (minGR),
                    outputGainReduction[channel + band * 2]);

                // ---- Scalar tail (0–3 remaining samples) ---------------------
                for (; sample < bufferSize; sample++)
                {
                    float gr = parameters.slope[band] *
                        (parameters.threshold[band] -
                         Decibels::gainToDecibels (envPtr[sample]));
                    gr = jmin (0.0f, gr);
                    outputGainReduction[channel + band * 2] = jmin (gr,
                        outputGainReduction[channel + band * 2]);
                    audioPtr[sample] *= std::pow (10.0f,
                        0.05f * (gr + parameters.makeUpGain[band]));
                }

                // RMS metering computed once per block, not per sample.
                outputLevels[band][channel] = bandBuffers[band].getRMSLevel (channel, 0, bufferSize);
            }
        }
    }
#endif // MOSES_USE_SIMD

    void outputActiveBands(AudioBuffer<float>& buffer)
    {
        buffer.clear();
        for (int channel = 0; channel < numOutputs; channel++)
        {
            for (int band = 0; band < numBands; band++)
            {
                if (parameters.kill[band])
                {
                    bandBuffers[band].clear(channel, 0, bufferSize);
                    continue;
                }
                if (anyListen)
                {
                    if (parameters.listen[band])
                    {
                        buffer.addFrom(channel, 0,
                            bandBuffers[band].getReadPointer(channel), bufferSize);
                    }
                }
                else
                {
                    buffer.addFrom(channel, 0,
                        bandBuffers[band].getReadPointer(channel), bufferSize);
                }
            }
        }
    }

    double sampleRate{ 0.0 };
    int bufferSize{ 0 };
    bool anyListen{ false };
    Parameters parameters;
    const NormalisableRange<float> freqRange{ 20.0f, 15000.0f, 1.0f, 0.25f };
    std::array<float, numBands * numOutputs> compressionLevel;
    std::array<float, numBands * numOutputs> outputGainReduction;
    std::array<std::array<float, numOutputs>, numBands> outputLevels;
    AudioBuffer<float> stage1LowBuffer, stage1HighBuffer;
    std::array<AudioBuffer<float>, numBands> bandBuffers;
    std::array<AudioBuffer<float>, numBands> envelopeBuffers;
    dsp::ProcessorChain<dsp::LinkwitzRileyFilter<float>,
        dsp::LinkwitzRileyFilter<float>> stage1LowChain, stage1HighChain;
   dsp::LinkwitzRileyFilter<float> bandChains[4];
};

/*  Signal Flow Diagram:
*                                             |--- xoFreqC HPF -> band4 ---|
*          |--- xoFreqA HPF -> xoFreqB APF ---|                            |
*          |                                  |--- xoFreqC LPF -> band3 ---|
* input ---|                                                               |--- output
*          |                                  |--- xoFreqB HPF -> band2 ---|
*          |--- xoFreqA LPF -> xoFreqC APF ---|                            |
*                                             |--- xoFreqB LPF -> band1 ---|
*/
