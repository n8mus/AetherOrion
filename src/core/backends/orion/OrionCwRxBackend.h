#pragma once

// N8EM fork: the Orion console's CW reader, offered beside AetherSDR's own
// in the CW window's decoder picker (step 10): "orion-fldigi", the engine
// ported from fldigi (its tracker and SOM fuzzy character matching),
// already in the tree for the skimmer. (The console's Bayesian experiment
// was dropped, 2026-11-03.)
//
// This is a thin CwRxBackend over skim::CwDecoder, which does its own tone
// extraction and envelope from the sample stream; AetherSDR's fixed 24 kHz
// decoder PCM goes in as real-valued IQ, exactly as the bench tool feeds it
// (tests/orion_cw_bench.cpp). No calibrated cost, so decoded text rides
// unscoredTextDecoded.
//
// The reader also MEASURES the tone (the console's AudioCwSource pitch
// meter, cut down): a Hann-windowed spectrum of the last third of a second,
// four times a second, the strongest line between 200 and 1200 Hz when it
// stands well clear of the band's median. That is what estimatedPitch()
// returns, so the host's zero-beat (configured pitch minus the measured
// one) works with this reader as it does with AetherSDR's own; -1 while no
// tone is there, which the zero-beat refuses rather than tuning on a
// guess. See FORK-NOTES.md.

#include <memory>
#include <vector>

#include <QString>

#include "models/CwRxModel.h"

namespace AetherSDR::orion {

namespace skim {
class CwDecoder;
}

class OrionCwRxBackend final : public CwRxBackend {
    Q_OBJECT

public:
    explicit OrionCwRxBackend(QObject* parent = nullptr);
    ~OrionCwRxBackend() override;

    void start() override;
    void stop() override;
    void reset() override;
    void feedFixed24(const DecoderPcmBlock& block) override;
    [[nodiscard]] bool isRunning() const override { return m_running; }
    // The measured tone, or -1 while none is heard.
    [[nodiscard]] float estimatedPitch() const override { return m_measuredHz; }
    [[nodiscard]] float estimatedSpeed() const override { return m_wpm; }
    [[nodiscard]] QString status() const override;
    [[nodiscard]] QString detail() const override;

private:
    void measurePitch();

    std::unique_ptr<skim::CwDecoder> m_decoder;
    bool m_running = false;
    float m_pitchHz = 0.0f;      // the decoder's mixer: the sidetone pitch
    float m_measuredHz = -1.0f;  // the tone actually heard
    float m_wpm = 0.0f;
    std::vector<float> m_pitchBuf;   // rolling window for the meter
    int m_pitchFill = 0;             // samples since the last measurement
};

}  // namespace AetherSDR::orion
