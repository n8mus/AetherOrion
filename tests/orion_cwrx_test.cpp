// SPDX-License-Identifier: GPL-2.0-or-later
// The Orion reader's tone meter (OrionCwRxBackend::estimatedPitch): a
// 700 Hz tone in AetherSDR's 24 kHz decoder PCM reads back as ~700 Hz
// within a second; noise alone reads as "no tone" (-1), which the host's
// zero-beat refuses. This is what the deck's and the VFO box's 0-BEAT
// run on when the Orion reader is the one listening.
//
//   cmake --build build --target orion_cwrx_test && ./build/orion_cwrx_test

#include "core/AppSettings.h"
#include "core/backends/orion/OrionCwRxBackend.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>

#include <cmath>
#include <cstdio>
#include <random>

using namespace AetherSDR;

namespace {
int g_failures = 0;
void check(bool ok, const char* what)
{
    std::printf("  %s: %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) ++g_failures;
}

void feedSeconds(orion::OrionCwRxBackend& rx, double seconds, double toneHz, float toneAmp,
                 float noiseAmp, std::mt19937& rng)
{
    std::normal_distribution<float> noise(0.0f, 1.0f);
    const int rate = DecoderPcmBlock::kSampleRateHz;
    const int total = static_cast<int>(seconds * rate);
    const int blockN = 480;
    int t = 0;
    while (t < total) {
        DecoderPcmBlock b;
        b.samples.resize(blockN);
        for (int i = 0; i < blockN; ++i, ++t) {
            float v = noiseAmp * noise(rng);
            if (toneHz > 0.0) {
                v += toneAmp * static_cast<float>(std::sin(2.0 * M_PI * toneHz * t / rate));
            }
            b.samples[i] = v;
        }
        rx.feedFixed24(b);
    }
}
}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    AppSettings::instance().setValue(QStringLiteral("OrionCw/pitchHz"), 600);
    std::mt19937 rng(7);

    orion::OrionCwRxBackend rx;
    check(rx.estimatedPitch() < 0.0f, "no tone before anything is heard");
    rx.start();
    QSignalSpy stats(&rx, &CwRxBackend::statsUpdated);

    std::printf("-- a 700 Hz tone in noise\n");
    feedSeconds(rx, 1.0, 700.0, 0.3f, 0.05f, rng);
    const float heard = rx.estimatedPitch();
    std::printf("  measured %.1f Hz\n", heard);
    check(std::abs(heard - 700.0f) <= 4.0f, "the meter reads the tone within a bin");
    check(stats.count() >= 1, "statsUpdated carried it");

    std::printf("-- the tone moves to 540 Hz\n");
    feedSeconds(rx, 1.0, 540.0, 0.3f, 0.05f, rng);
    check(std::abs(rx.estimatedPitch() - 540.0f) <= 4.0f, "the meter follows");

    std::printf("-- noise alone\n");
    feedSeconds(rx, 1.0, 0.0, 0.0f, 0.05f, rng);
    check(rx.estimatedPitch() < 0.0f, "noise alone is no tone");

    std::printf("-- stop clears it\n");
    feedSeconds(rx, 0.5, 700.0, 0.3f, 0.05f, rng);
    rx.stop();
    check(rx.estimatedPitch() < 0.0f, "stopped: no tone");

    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
