#pragma once

#include <QByteArray>
#include <memory>
#include <vector>
#include <cstdint>

namespace r8b { class CDSPResampler24; }

namespace AetherSDR {

// High-quality sample rate converter using r8brain-free-src (MIT).
// Wraps r8b::CDSPResampler24 with int16 ↔ double conversion.
//
// Each instance handles one fixed rate ratio. Create separate instances
// for upsample and downsample paths.
//
// Thread safety: NOT thread-safe. Use one instance per thread/path.

class Resampler {
public:
    // maxBlockSamples: max mono samples per process() call
    Resampler(double srcRate, double dstRate, int maxBlockSamples = 4096);
    ~Resampler();

    // Resample mono int16 PCM. Returns resampled mono int16.
    QByteArray process(const int16_t* in, int numSamples);

    // Convenience: stereo int16 → mono downsample → resampled mono int16
    QByteArray processStereoToMono(const int16_t* stereoIn, int numStereoFrames);

    // Convenience: mono int16 → resampled → duplicated to stereo int16
    QByteArray processMonoToStereo(const int16_t* monoIn, int numSamples);

    // Convenience: stereo int16 → downmix to mono → resample → duplicate to stereo int16
    QByteArray processStereoToStereo(const int16_t* stereoIn, int numStereoFrames);

    double srcRate() const { return m_srcRate; }
    double dstRate() const { return m_dstRate; }

private:
    double m_srcRate;
    double m_dstRate;
    std::unique_ptr<r8b::CDSPResampler24> m_resampler;
    std::vector<double> m_inBuf;   // int16 → double conversion buffer
};

} // namespace AetherSDR
