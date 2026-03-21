#include "OpusCodec.h"
#include "LogManager.h"

#ifdef HAVE_RADE
#include <opus.h>
#endif

#include <cstring>

namespace AetherSDR {

OpusCodec::OpusCodec()
{
#ifdef HAVE_RADE
    int err;
    m_decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
    if (err != OPUS_OK || !m_decoder)
        qCWarning(lcAudio) << "OpusCodec: decoder create failed:" << opus_strerror(err);

    m_encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_AUDIO, &err);
    if (err != OPUS_OK || !m_encoder) {
        qCWarning(lcAudio) << "OpusCodec: encoder create failed:" << opus_strerror(err);
    } else {
        opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(m_bitrate));
        opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(10));
        opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    }
#endif
}

OpusCodec::~OpusCodec()
{
#ifdef HAVE_RADE
    if (m_decoder) opus_decoder_destroy(m_decoder);
    if (m_encoder) opus_encoder_destroy(m_encoder);
#endif
}

bool OpusCodec::isValid() const
{
#ifdef HAVE_RADE
    return m_decoder != nullptr && m_encoder != nullptr;
#else
    return false;
#endif
}

QByteArray OpusCodec::decode(const QByteArray& opusFrame)
{
#ifdef HAVE_RADE
    if (!m_decoder || opusFrame.isEmpty()) return {};

    // Decode Opus → mono int16 PCM
    int16_t monoOut[FRAME_SIZE];
    int samples = opus_decode(m_decoder,
        reinterpret_cast<const unsigned char*>(opusFrame.constData()),
        opusFrame.size(), monoOut, FRAME_SIZE, 0);

    if (samples <= 0) {
        qCWarning(lcAudio) << "OpusCodec: decode failed:" << opus_strerror(samples);
        return {};
    }

    // Duplicate mono → stereo for AudioEngine
    QByteArray stereo(samples * 2 * sizeof(int16_t), Qt::Uninitialized);
    auto* dst = reinterpret_cast<int16_t*>(stereo.data());
    for (int i = 0; i < samples; ++i) {
        dst[i * 2]     = monoOut[i];
        dst[i * 2 + 1] = monoOut[i];
    }
    return stereo;
#else
    Q_UNUSED(opusFrame);
    return {};
#endif
}

QByteArray OpusCodec::encode(const QByteArray& pcmStereo)
{
#ifdef HAVE_RADE
    if (!m_encoder || pcmStereo.isEmpty()) return {};

    // Convert stereo int16 → mono int16 (average L+R)
    const auto* src = reinterpret_cast<const int16_t*>(pcmStereo.constData());
    int stereoSamples = pcmStereo.size() / sizeof(int16_t);
    int monoSamples = stereoSamples / 2;

    // Opus needs exactly FRAME_SIZE samples per encode call
    if (monoSamples != FRAME_SIZE) return {};

    int16_t monoIn[FRAME_SIZE];
    for (int i = 0; i < FRAME_SIZE; ++i)
        monoIn[i] = static_cast<int16_t>((src[i * 2] + src[i * 2 + 1]) / 2);

    // Encode mono → Opus
    unsigned char opusOut[MAX_OPUS_BYTES];
    int bytes = opus_encode(m_encoder, monoIn, FRAME_SIZE,
                            opusOut, MAX_OPUS_BYTES);

    if (bytes <= 0) {
        qCWarning(lcAudio) << "OpusCodec: encode failed:" << opus_strerror(bytes);
        return {};
    }

    return QByteArray(reinterpret_cast<const char*>(opusOut), bytes);
#else
    Q_UNUSED(pcmStereo);
    return {};
#endif
}

void OpusCodec::setBitrate(int bps)
{
    m_bitrate = bps;
#ifdef HAVE_RADE
    if (m_encoder)
        opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(bps));
#endif
}

} // namespace AetherSDR
