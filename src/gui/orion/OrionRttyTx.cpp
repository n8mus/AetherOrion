#include "gui/orion/OrionRttyTx.h"

#include "core/AudioEngine.h"
#include "core/LogManager.h"
#include "core/backends/orion/rtty/RttyEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QTimer>

#include <algorithm>
#include <vector>

namespace AetherSDR::orion::rtty {

namespace {
// The modem route's wire: stereo float32 at the engine's rate (24 kHz).
constexpr qsizetype kFrameBytes = 2 * static_cast<qsizetype>(sizeof(float));
// The schedule the other modem producers use: a lead before sample zero,
// 20 ms blocks, a cushion ahead of real time, a short tail after the drain.
constexpr int kLeadMs = 200;
constexpr int kChunkMs = 20;
constexpr int kLeadBufferMs = 120;
constexpr int kTailMs = 150;
constexpr int kPttConfirmMs = 2000;

// One counter for every session in the process, far above the small
// per-object counters other producers use (see RxPlaybackTransmitter).
quint64 nextSessionToken()
{
    static quint64 token = quint64(1) << 41;
    return ++token;
}
}  // namespace

OrionRttyTx::OrionRttyTx(RadioModel* radio, AudioEngine* audio, RttyEngine* engine, QObject* parent)
    : QObject(parent), m_radio(radio), m_audio(audio), m_engine(engine)
{
    m_pacer = new QTimer(this);
    m_pacer->setInterval(kChunkMs);
    connect(m_pacer, &QTimer::timeout, this, &OrionRttyTx::pace);
    if (!m_radio) return;
    m_producer = m_radio->registerTxProducer(this);
    connect(m_radio, &RadioModel::txAudioFinished, this, &OrionRttyTx::onTxAudioFinished);
    connect(m_radio, &RadioModel::connectionStateChanged, this, [this](bool connected) {
        if (!connected && m_active) finish(true, tr("the radio disconnected"));
    });
    connect(&m_radio->transmitModel(), &TransmitModel::pttBlocked, this, [this](const QString& message) {
        if (m_active) finish(true, tr("PTT blocked: %1").arg(message));
    });
    // The operator unkeying from anywhere else ends the over rather than
    // leaving a pacer feeding a transmitter that is no longer ours.
    connect(&m_radio->transmitModel(), &TransmitModel::transmittingChanged, this, [this](bool tx) {
        if (!tx && m_active && m_audioStartArmed) finish(true, tr("the transmitter was unkeyed"));
    });
}

OrionRttyTx::~OrionRttyTx()
{
    if (m_active) finish(true, QStringLiteral("shutting down"));
}

bool OrionRttyTx::ending() const
{
    return m_active && m_engine && m_engine->ending();
}

bool OrionRttyTx::begin(SliceModel* slice, QString* whyNot)
{
    const auto refuse = [whyNot](const QString& why) {
        if (whyNot) *whyNot = why;
        return false;
    };
    if (m_active) return true;
    if (!m_radio || !m_audio || !m_engine) return refuse(tr("the radio, audio engine or modem is gone"));
    if (!m_radio->isConnected()) return refuse(tr("no radio is connected"));
    if (!m_radio->backendCapabilities().canTransmit) return refuse(tr("this radio cannot transmit"));
    if (!m_radio->backendCapabilities().takesTxAudioOverSeam && !m_radio->backendCapabilities().hostModulates)
        return refuse(tr("this radio takes no host audio"));
    if (!m_producer.valid()) return refuse(tr("no transmit producer"));
    m_request = m_producer.request();
    if (!m_request.valid()) return refuse(tr("the transmit request is not valid"));

    m_generation = nextSessionToken();
    m_audioStartArmed = false;
    m_awaitingFinish = false;
    m_sentSamples = 0;

    if (slice && !slice->isTxSlice()) slice->setTxSlice(true);

    m_active = true;
    emit activeChanged(true);
    m_engine->txBegin();

    // Local DAX TX mode keeps the microphone off the wire while we modulate.
    m_previousAudioDaxMode = m_audio->isDaxTxMode();
    m_restoreAudioDaxMode = true;
    m_audio->setDaxTxMode(true);

    // The Orion reports its own key (the ?S meter): sample zero waits for it.
    const bool waitsForRadioPtt = m_radio->backendCapabilities().hasRadioPttReadback;
    const quint64 generation = m_generation;
    disconnectPttConfirmation();
    if (waitsForRadioPtt) {
        const auto confirmed = [this, generation](bool transmitting) {
            if (!transmitting || !m_active || generation != m_generation) return;
            disconnectPttConfirmation();
            startAudioAfterPtt();
        };
        m_pttConfirm = connect(m_radio, &RadioModel::radioTransmittingChanged, this, confirmed);
        m_pttConfirmed = connect(m_radio, &RadioModel::radioTransmitConfirmed, this, confirmed);
    }
    if (!m_radio->requestProducerPttOn(m_request, TransmitModel::PttSource::Dax)) {
        finish(true, tr("PTT request was refused"));
        return refuse(tr("PTT request was refused"));
    }
    m_context = m_radio->captureTxMedia(m_request);
    if (!m_active) return refuse(tr("the over ended before it began"));
    if (waitsForRadioPtt) {
        if (m_radio->isRadioTransmitting()) {
            disconnectPttConfirmation();
            startAudioAfterPtt();
            return true;
        }
        QTimer::singleShot(kPttConfirmMs, this, [this, generation] {
            if (!m_active || m_audioStartArmed || generation != m_generation) return;
            finish(true, tr("radio did not confirm PTT within %1 ms").arg(kPttConfirmMs));
        });
        return true;
    }
    if (!m_radio->transmitModel().isTransmitting()) {
        finish(true, tr("PTT did not engage"));
        return refuse(tr("PTT did not engage"));
    }
    startAudioAfterPtt();
    return true;
}

void OrionRttyTx::send(const QString& text)
{
    if (!m_active || !m_engine) return;
    m_engine->send(text);
}

void OrionRttyTx::end()
{
    if (!m_active || !m_engine) return;
    m_engine->end();
}

void OrionRttyTx::abort(const QString& reason)
{
    if (m_active) finish(true, reason);
}

void OrionRttyTx::startAudioAfterPtt()
{
    if (!m_active || m_audioStartArmed) return;
    m_audioStartArmed = true;
    const quint64 generation = m_generation;
    QTimer::singleShot(kLeadMs, this, [this, generation] {
        if (!m_active || generation != m_generation) return;
        m_paceClock.restart();
        pace();
        if (m_active) m_pacer->start();
    });
}

void OrionRttyTx::pace()
{
    if (!m_active || !m_audio || !m_engine) return;
    if (m_awaitingFinish) return;
    const int rate = m_engine->sampleRate();
    const qint64 nowMs = m_paceClock.isValid() ? m_paceClock.elapsed() : 0;
    const qint64 targetSamples = qint64(rate) * (nowMs + kLeadBufferMs) / 1000;
    const int block = rate * kChunkMs / 1000;
    std::vector<float> mono(static_cast<size_t>(block), 0.0f);
    while (m_sentSamples < targetSamples) {
        if (!m_engine->txRender(mono.data(), block)) {
            // The over is out: the completion barrier behind the last block;
            // the backend answers with its drain time.
            m_pacer->stop();
            m_awaitingFinish = true;
            const quint64 generation = m_generation;
            QPointer<AudioEngine> audio = m_audio;
            const bool queued = QMetaObject::invokeMethod(
                m_audio, [audio, generation, context = m_context] {
                    if (audio) audio->finishModemTxAudio(generation, context);
                }, Qt::QueuedConnection);
            if (!queued) finish(true, tr("could not queue the TX-audio completion barrier"));
            return;
        }
        QByteArray chunk(qsizetype(block) * kFrameBytes, Qt::Uninitialized);
        auto* out = reinterpret_cast<float*>(chunk.data());
        for (int i = 0; i < block; ++i) {
            const float v = std::clamp(mono[size_t(i)], -1.0f, 1.0f);
            out[2 * i] = v;
            out[2 * i + 1] = v;
        }
        m_sentSamples += block;
        QPointer<AudioEngine> audio = m_audio;
        QMetaObject::invokeMethod(m_audio, [audio, chunk, context = m_context] {
            if (audio) audio->sendModemTxAudio(chunk, context);
        }, Qt::QueuedConnection);
    }
}

void OrionRttyTx::onTxAudioFinished(quint64 token, int drainMs)
{
    if (!m_active || !m_awaitingFinish || token != m_generation) return;
    m_awaitingFinish = false;
    QTimer::singleShot(kTailMs + std::max(0, drainMs), this, [this, token] {
        if (m_active && token == m_generation) finish(false, tr("over complete"));
    });
}

void OrionRttyTx::disconnectPttConfirmation()
{
    if (m_pttConfirm)   { disconnect(m_pttConfirm);   m_pttConfirm = {}; }
    if (m_pttConfirmed) { disconnect(m_pttConfirmed); m_pttConfirmed = {}; }
}

void OrionRttyTx::finish(bool aborted, const QString& reason)
{
    m_pacer->stop();
    disconnectPttConfirmation();
    const bool wasActive = m_active;
    m_active = false;
    m_audioStartArmed = false;
    m_awaitingFinish = false;
    if (m_engine) m_engine->txAbort();
    if (m_radio && m_request.valid()) {
        if (aborted) m_radio->abortProducerPtt(m_request, TransmitModel::PttSource::Dax);
        else         m_radio->requestProducerPttOff(m_request, TransmitModel::PttSource::Dax);
    }
    m_request = {};
    if (m_audio) {
        if (m_restoreAudioDaxMode) m_audio->setDaxTxMode(m_previousAudioDaxMode);
        m_audio->discardTxMedia(m_context);
    }
    m_context = {};
    m_restoreAudioDaxMode = false;
    if (wasActive) {
        qCInfo(lcAudio).noquote() << QStringLiteral("OrionRttyTx: %1: %2")
                                         .arg(aborted ? QStringLiteral("aborted") : QStringLiteral("finished"), reason);
        emit activeChanged(false);
        emit finished(aborted, reason);
    }
}

}  // namespace AetherSDR::orion::rtty
