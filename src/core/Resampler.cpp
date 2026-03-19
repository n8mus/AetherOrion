#include "Resampler.h"

#include "CDSPResampler.h"

namespace AetherSDR {

Resampler::Resampler(double srcRate, double dstRate, int maxBlockSamples)
    : m_srcRate(srcRate)
    , m_dstRate(dstRate)
    , m_resampler(std::make_unique<r8b::CDSPResampler24>(srcRate, dstRate, maxBlockSamples))
{
    m_inBuf.reserve(maxBlockSamples);
}

Resampler::~Resampler() = default;

QByteArray Resampler::process(const int16_t* in, int numSamples)
{
    if (numSamples <= 0) return {};

    // Convert int16 → double
    m_inBuf.resize(numSamples);
    for (int i = 0; i < numSamples; ++i)
        m_inBuf[i] = in[i];

    // Resample
    double* outPtr = nullptr;
    int outLen = m_resampler->process(m_inBuf.data(), numSamples, outPtr);

    if (outLen <= 0 || !outPtr) return {};

    // Convert double → int16
    QByteArray result(outLen * 2, Qt::Uninitialized);
    auto* dst = reinterpret_cast<int16_t*>(result.data());
    for (int i = 0; i < outLen; ++i) {
        double v = outPtr[i];
        if (v > 32767.0) v = 32767.0;
        if (v < -32768.0) v = -32768.0;
        dst[i] = static_cast<int16_t>(v);
    }
    return result;
}

QByteArray Resampler::processStereoToMono(const int16_t* stereoIn, int numStereoFrames)
{
    if (numStereoFrames <= 0) return {};

    // Downmix stereo → mono
    m_inBuf.resize(numStereoFrames);
    for (int i = 0; i < numStereoFrames; ++i)
        m_inBuf[i] = (stereoIn[2 * i] + stereoIn[2 * i + 1]) * 0.5;

    // Resample
    double* outPtr = nullptr;
    int outLen = m_resampler->process(m_inBuf.data(), numStereoFrames, outPtr);

    if (outLen <= 0 || !outPtr) return {};

    // Convert double → int16 mono
    QByteArray result(outLen * 2, Qt::Uninitialized);
    auto* dst = reinterpret_cast<int16_t*>(result.data());
    for (int i = 0; i < outLen; ++i) {
        double v = outPtr[i];
        if (v > 32767.0) v = 32767.0;
        if (v < -32768.0) v = -32768.0;
        dst[i] = static_cast<int16_t>(v);
    }
    return result;
}

QByteArray Resampler::processMonoToStereo(const int16_t* monoIn, int numSamples)
{
    if (numSamples <= 0) return {};

    // Convert int16 → double
    m_inBuf.resize(numSamples);
    for (int i = 0; i < numSamples; ++i)
        m_inBuf[i] = monoIn[i];

    // Resample
    double* outPtr = nullptr;
    int outLen = m_resampler->process(m_inBuf.data(), numSamples, outPtr);

    if (outLen <= 0 || !outPtr) return {};

    // Convert double → int16 stereo (duplicate mono to L+R)
    QByteArray result(outLen * 4, Qt::Uninitialized);
    auto* dst = reinterpret_cast<int16_t*>(result.data());
    for (int i = 0; i < outLen; ++i) {
        double v = outPtr[i];
        if (v > 32767.0) v = 32767.0;
        if (v < -32768.0) v = -32768.0;
        int16_t s = static_cast<int16_t>(v);
        dst[2 * i]     = s;
        dst[2 * i + 1] = s;
    }
    return result;
}

QByteArray Resampler::processStereoToStereo(const int16_t* stereoIn, int numStereoFrames)
{
    if (numStereoFrames <= 0) return {};

    // Downmix stereo → mono, resample, duplicate back to stereo
    m_inBuf.resize(numStereoFrames);
    for (int i = 0; i < numStereoFrames; ++i)
        m_inBuf[i] = (stereoIn[2 * i] + stereoIn[2 * i + 1]) * 0.5;

    double* outPtr = nullptr;
    int outLen = m_resampler->process(m_inBuf.data(), numStereoFrames, outPtr);

    if (outLen <= 0 || !outPtr) return {};

    QByteArray result(outLen * 4, Qt::Uninitialized);
    auto* dst = reinterpret_cast<int16_t*>(result.data());
    for (int i = 0; i < outLen; ++i) {
        double v = outPtr[i];
        if (v > 32767.0) v = 32767.0;
        if (v < -32768.0) v = -32768.0;
        int16_t s = static_cast<int16_t>(v);
        dst[2 * i]     = s;
        dst[2 * i + 1] = s;
    }
    return result;
}

} // namespace AetherSDR
