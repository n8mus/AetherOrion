#pragma once

#include <QByteArray>

#ifdef HAVE_RADE
struct OpusDecoder;
struct OpusEncoder;
#endif

namespace AetherSDR {

// Thin wrapper around libopus for SmartLink compressed audio.
// Encodes/decodes mono 24kHz audio in 20ms frames (480 samples).
// Opus payload is raw bytes — no byte-swapping needed in VITA-49.
//
// RX: Opus frame bytes → decode → mono int16 PCM → duplicate to stereo
// TX: stereo int16 PCM → average to mono → encode → Opus frame bytes
//
// Requires libopus (bundled via RADE build). No-op stub without HAVE_RADE.

class OpusCodec {
public:
    OpusCodec();
    ~OpusCodec();

    OpusCodec(const OpusCodec&) = delete;
    OpusCodec& operator=(const OpusCodec&) = delete;

    bool isValid() const;

    // Decode an Opus frame → stereo int16 24kHz PCM (for AudioEngine)
    QByteArray decode(const QByteArray& opusFrame);

    // Encode stereo int16 24kHz PCM → Opus frame bytes
    QByteArray encode(const QByteArray& pcmStereo);

    // Bitrate in bits/sec (default 32000). Higher = better quality, more bandwidth.
    void setBitrate(int bps);
    int bitrate() const { return m_bitrate; }

private:
    int m_bitrate{32000};

#ifdef HAVE_RADE
    OpusDecoder* m_decoder{nullptr};
    OpusEncoder* m_encoder{nullptr};
    static constexpr int SAMPLE_RATE = 24000;
    static constexpr int CHANNELS    = 1;      // mono
    static constexpr int FRAME_MS    = 20;
    static constexpr int FRAME_SIZE  = SAMPLE_RATE * FRAME_MS / 1000;  // 480 samples
    static constexpr int MAX_OPUS_BYTES = 4000; // max encoded frame size
#endif
};

} // namespace AetherSDR
