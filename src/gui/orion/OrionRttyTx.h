#pragma once

// N8EM fork, RTTY stage 3: the RTTY over on the host's own transmit path.
//
// The deck asks for text; this object keys the radio through the producer
// PTT API under a TxCoordinator request captured at the keystroke, renders
// the engine's modulator in 20 ms blocks at the engine's rate (24 kHz
// stereo float32, the modem route's format) onto
// AudioEngine::sendModemTxAudio, and drops PTT only after the backend has
// drained the last block (RadioModel::txAudioFinished). The same shape as
// gui/RxPlaybackTransmitter, which transmits a finite recording; here the
// stream is open-ended until end() and the engine says the tail is out.
//
// On the Orion the audio reaches the SignaLink only on the PC mic input;
// begin() refuses on the radio's own jack rather than key a silent over.

#include "models/TxController.h"   // TxCoordinator::Request / Context

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QString>

class QTimer;

namespace AetherSDR {
class AudioEngine;
class RadioModel;
class SliceModel;
}

namespace AetherSDR::orion::rtty {

class RttyEngine;

class OrionRttyTx : public QObject {
    Q_OBJECT
public:
    OrionRttyTx(RadioModel* radio, AudioEngine* audio, RttyEngine* engine, QObject* parent = nullptr);
    ~OrionRttyTx() override;

    // Keyed, or waiting for the radio to confirm the key.
    bool active() const { return m_active; }
    // Text is queued and still goes out after end() until the tail plays.
    bool ending() const;

    // Key up on `slice` (made the TX slice first) and start the modulator
    // diddling. False with `whyNot` when nothing began; once it has, every
    // outcome arrives through finished().
    bool begin(SliceModel* slice, QString* whyNot = nullptr);
    void send(const QString& text);       // queue; begins the over if needed
    void end();                           // play out, tail, drain, unkey
    void abort(const QString& reason);    // drop PTT now

signals:
    void activeChanged(bool active);
    void finished(bool aborted, const QString& reason);

private:
    void startAudioAfterPtt();
    void pace();
    void onTxAudioFinished(quint64 token, int drainMs);
    void finish(bool aborted, const QString& reason);
    void disconnectPttConfirmation();

    QPointer<RadioModel> m_radio;
    QPointer<AudioEngine> m_audio;
    QPointer<RttyEngine> m_engine;
    QTimer* m_pacer{nullptr};
    TxCoordinator::Producer m_producer;
    TxCoordinator::Request m_request;
    TxCoordinator::Context m_context;
    quint64 m_generation{0};
    qint64 m_sentSamples{0};
    bool m_active{false};
    bool m_audioStartArmed{false};
    bool m_awaitingFinish{false};
    bool m_restoreAudioDaxMode{false};
    bool m_previousAudioDaxMode{false};
    QElapsedTimer m_paceClock;
    QMetaObject::Connection m_pttConfirm;
    QMetaObject::Connection m_pttConfirmed;
};

}  // namespace AetherSDR::orion::rtty
