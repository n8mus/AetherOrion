#pragma once

#include <QObject>
#include <QByteArray>

#ifdef HAVE_RADE
struct rade;
struct LPCNetEncState;
// FARGANState is defined in fargan.h (opus), included only in .cpp
// Use void* here to avoid pulling in opus headers
#endif

namespace AetherSDR {

// Wraps the RADE v1 (Radio Autoencoder) codec for FreeDV digital voice.
// The radio is set to DIGU (SSB passthrough). RADE handles the encoding
// and decoding locally:
//
// TX: Mic(24kHz stereo) → 16kHz mono → LPCNet features → rade_tx() → RADE_COMP(8kHz) → 24kHz stereo → DAX TX
// RX: DAX RX(24kHz stereo) → 8kHz mono → RADE_COMP → rade_rx() → features → FARGAN → 16kHz mono → 24kHz stereo
//
class RADEEngine : public QObject {
    Q_OBJECT

public:
    explicit RADEEngine(QObject* parent = nullptr);
    ~RADEEngine() override;

    bool start();
    void stop();
    bool isActive() const;
    bool isSynced() const;

public slots:
    // Feed DAX RX audio (24kHz stereo int16) for decoding.
    // channel is the DAX channel number (1-4), only processes channel 1.
    void feedRxAudio(int channel, const QByteArray& pcm);

    // Feed mic audio (24kHz stereo int16) for encoding.
    void feedTxAudio(const QByteArray& pcm);

    // Flush TX encoder state (call on MOX release to prevent stale audio)
    void resetTx();

signals:
    void rxSpeechReady(const QByteArray& pcm);   // Decoded speech, 24kHz stereo int16
    void txModemReady(const QByteArray& pcm);     // Encoded modem, 24kHz stereo int16
    void syncChanged(bool synced);
    void snrChanged(float snrDb);

private:
#ifdef HAVE_RADE
    struct rade*         m_rade{nullptr};
    LPCNetEncState*      m_lpcnetEnc{nullptr};
    void*                m_fargan{nullptr};  // actually FARGANState*, opaque here
    bool                 m_synced{false};

    // TX accumulation: 12 frames of NB_TOTAL_FEATURES = 432 floats
    QByteArray m_txFeatAccum;
    int        m_txFrameCount{0};

    // RX accumulation: rade_nin() RADE_COMP samples
    QByteArray m_rxAccum;

    bool m_farganWarmedUp{false};
#endif

    // Resampling helpers
    // 24kHz stereo int16 → 8kHz mono int16 (3:1 decimation, take left channel)
    static QByteArray downsample24kTo8k(const QByteArray& stereo24k);
    // 8kHz mono int16 → 24kHz stereo int16 (1:3 interpolation)
    static QByteArray upsample8kTo24k(const QByteArray& mono8k);
    // 24kHz stereo int16 → 16kHz mono int16 (3:2 decimation)
    static QByteArray downsample24kTo16k(const QByteArray& stereo24k);
    // 16kHz mono int16 → 24kHz stereo int16 (2:3 interpolation)
    static QByteArray upsample16kTo24k(const QByteArray& mono16k);
};

} // namespace AetherSDR
