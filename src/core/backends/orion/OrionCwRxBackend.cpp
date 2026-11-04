#include "core/backends/orion/OrionCwRxBackend.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#include "core/AppSettings.h"
#include "core/backends/orion/skimmer/CwDecoder.h"

namespace AetherSDR::orion {

namespace {
// AetherSDR hands every decoder its PCM at this rate (DecoderPcmBlock).
constexpr double kRateHz = static_cast<double>(DecoderPcmBlock::kSampleRateHz);

// The pitch meter: a 8192-sample window (~0.34 s, ~2.9 Hz bins), measured
// every 6000 samples (four times a second), the console's figures.
constexpr int kPitchN = 8192;
constexpr int kPitchEvery = 6000;
constexpr double kPitchLoHz = 200.0;
constexpr double kPitchHiHz = 1200.0;
constexpr float kToneOverMedian = 16.0f;   // a line is a tone when 12 dB over the band

// Where in that stream the tone sits. Real-valued samples put the signal at
// +/- the audio pitch, so the reader listens at the operator's sidetone
// pitch — the note they tune a station onto.
double pitchHz()
{
    const int hz = AppSettings::instance()
                       .value(QStringLiteral("CwSidetonePitch"), 600)
                       .toInt();
    return hz >= 100 && hz <= 3000 ? static_cast<double>(hz) : 600.0;
}
}  // namespace

OrionCwRxBackend::OrionCwRxBackend(QObject* parent)
    : CwRxBackend(parent)
{
    m_decoder = std::make_unique<skim::CwDecoder>(kRateHz, pitchHz());
    m_decoder->setBrain(skim::CwDecoder::Brain::Engine);
    m_decoder->setSom(true);
    m_decoder->setAttack(1);
    m_decoder->setDecay(1);
    m_decoder->setSquelch(12.0);
    m_pitchHz = static_cast<float>(pitchHz());
    m_pitchBuf.reserve(kPitchN + kPitchEvery);

    // The loose stream is the one for human eyes: SOM's closest letter
    // rather than the call miner's honest '*'. No calibrated cost here, so
    // it rides the unscored signal.
    connect(m_decoder.get(), &skim::CwDecoder::textLoose, this,
            [this](const QString& text) {
                if (!text.isEmpty()) {
                    emit unscoredTextDecoded(text);
                }
            });
    connect(m_decoder.get(), &skim::CwDecoder::wpmEstimated, this, [this](int wpm) {
        if (wpm > 0) {
            m_wpm = static_cast<float>(wpm);
            emit statsUpdated(m_measuredHz, m_wpm);
        }
    });
}

OrionCwRxBackend::~OrionCwRxBackend() = default;

void OrionCwRxBackend::start()
{
    // The pitch may have moved since this backend was built (the operator
    // keeps the reader on the note they listen at).
    const double hz = pitchHz();
    if (!qFuzzyCompare(hz, static_cast<double>(m_pitchHz))) {
        m_pitchHz = static_cast<float>(hz);
        m_decoder->retune(hz);
    }
    m_decoder->setEnabled(true);
    m_running = true;
    emit statusChanged();
}

void OrionCwRxBackend::stop()
{
    m_decoder->setEnabled(false);
    m_running = false;
    m_pitchBuf.clear();
    m_pitchFill = 0;
    if (m_measuredHz > 0.0f) {
        m_measuredHz = -1.0f;
        emit statsUpdated(m_measuredHz, m_wpm);
    }
    emit statusChanged();
}

void OrionCwRxBackend::reset()
{
    m_decoder->retune(pitchHz());
    m_wpm = 0.0f;
    m_pitchBuf.clear();
    m_pitchFill = 0;
    m_measuredHz = -1.0f;
}

void OrionCwRxBackend::feedFixed24(const DecoderPcmBlock& block)
{
    if (!m_running || block.samples.isEmpty()) {
        return;
    }
    // Real-valued IQ, the way the bench feeds it: the decoder's own front
    // end does the mixing, filtering and envelope from here.
    std::vector<std::complex<float>> iq(static_cast<std::size_t>(block.samples.size()));
    for (int i = 0; i < block.samples.size(); ++i) {
        iq[static_cast<std::size_t>(i)] = {block.samples[i], 0.0f};
    }
    m_decoder->processIq(iq.data(), iq.size());

    // The meter's rolling window, measured four times a second.
    m_pitchBuf.insert(m_pitchBuf.end(), block.samples.cbegin(), block.samples.cend());
    if (static_cast<int>(m_pitchBuf.size()) > kPitchN) {
        m_pitchBuf.erase(m_pitchBuf.begin(), m_pitchBuf.end() - kPitchN);
    }
    m_pitchFill += block.samples.size();
    if (m_pitchFill >= kPitchEvery && static_cast<int>(m_pitchBuf.size()) == kPitchN) {
        m_pitchFill = 0;
        measurePitch();
    }
}

// The strongest line between 200 and 1200 Hz, when it stands well clear
// of the band's median: a Hann window over the last kPitchN samples and
// one Goertzel per bin at the window's own resolution. (A few hundred
// bins over eight thousand samples, four times a second: cheap.)
void OrionCwRxBackend::measurePitch()
{
    static std::vector<float> windowed;
    static std::vector<float> hann;
    if (static_cast<int>(hann.size()) != kPitchN) {
        hann.resize(kPitchN);
        for (int i = 0; i < kPitchN; ++i) {
            hann[static_cast<std::size_t>(i)] =
                0.5f - 0.5f * std::cos(2.0f * static_cast<float>(M_PI) * i / (kPitchN - 1));
        }
    }
    windowed.resize(kPitchN);
    for (int i = 0; i < kPitchN; ++i) {
        windowed[static_cast<std::size_t>(i)] =
            m_pitchBuf[static_cast<std::size_t>(i)] * hann[static_cast<std::size_t>(i)];
    }
    const double binHz = kRateHz / kPitchN;
    const int b0 = static_cast<int>(kPitchLoHz / binHz);
    const int b1 = static_cast<int>(kPitchHiHz / binHz);
    std::vector<float> mags;
    mags.reserve(static_cast<std::size_t>(b1 - b0 + 1));
    int peakBin = b0;
    float peakMag = -1.0f;
    for (int b = b0; b <= b1; ++b) {
        // Goertzel at bin b.
        const float w = 2.0f * std::cos(2.0f * static_cast<float>(M_PI) * b / kPitchN);
        float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
        for (int i = 0; i < kPitchN; ++i) {
            s0 = windowed[static_cast<std::size_t>(i)] + w * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const float mag = s1 * s1 + s2 * s2 - w * s1 * s2;   // power
        mags.push_back(mag);
        if (mag > peakMag) {
            peakMag = mag;
            peakBin = b;
        }
    }
    std::nth_element(mags.begin(), mags.begin() + static_cast<long>(mags.size() / 2),
                     mags.end());
    const float median = mags[mags.size() / 2];
    const bool tone = peakMag > 0.0f && peakMag >= median * kToneOverMedian && peakBin > b0
                      && peakBin < b1;
    // The bin centre: a bin is ~2.9 Hz, which is the readout's resolution
    // and well inside what a zero-beat needs.
    const float hz = tone ? static_cast<float>(peakBin * binHz) : -1.0f;
    const bool changed = (hz < 0.0f) != (m_measuredHz < 0.0f)
                         || (hz > 0.0f && std::abs(hz - m_measuredHz) > 1.5f);
    if (changed) {
        m_measuredHz = hz;
        emit statsUpdated(m_measuredHz, m_wpm);
    }
}

QString OrionCwRxBackend::status() const
{
    if (!m_running) {
        return tr("stopped");
    }
    return m_wpm > 0.0f ? tr("%1 WPM").arg(static_cast<int>(m_wpm)) : tr("listening");
}

QString OrionCwRxBackend::detail() const
{
    return tr("Orion fldigi engine, at %1 Hz").arg(static_cast<int>(m_pitchHz));
}

}  // namespace AetherSDR::orion
