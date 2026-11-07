// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from the N8EM Orion console (src/rtty/RttyEngine.cpp) for the AetherSDR
// Orion fork. Stage 1: namespace, include paths, the sample rate. Stage 3:
// the pacat transmit path replaced by a pulled modulator (see the header).
#include "core/backends/orion/rtty/RttyEngine.h"

#include <QDateTime>
#include <QRegularExpression>
#include "core/AppSettings.h"
#include <algorithm>
#include <cmath>

namespace AetherSDR::orion::rtty {

namespace {
constexpr int kRate = 48000;

constexpr int kTailMs = 200;                  // mark after the last stop bit

const QRegularExpression& callRe() {
    static const QRegularExpression re(
        "^[A-Z0-9]{1,3}[0-9][A-Z0-9]*[A-Z](/[A-Z0-9]+)?$|"
        "^[A-Z0-9]+/[A-Z0-9]{1,3}[0-9][A-Z0-9]*[A-Z]$");
    return re;
}
}  // namespace

RttyEngine::Settings RttyEngine::loadSettings() {
    auto& q = AppSettings::instance();
    Settings s;
    s.baud = q.value(QStringLiteral("OrionRtty/baud"), s.baud).toDouble();
    s.shiftHz = q.value(QStringLiteral("OrionRtty/shift"), s.shiftHz).toInt();
    s.markHz = q.value(QStringLiteral("OrionRtty/mark"), s.markHz).toInt();
    s.reverse = q.value(QStringLiteral("OrionRtty/reverse"), s.reverse).toBool();
    s.usos = q.value(QStringLiteral("OrionRtty/usos"), s.usos).toBool();
    s.diddle = q.value(QStringLiteral("OrionRtty/diddle"), s.diddle).toBool();
    s.txLevelDb = q.value(QStringLiteral("OrionRtty/txLevelDb"), s.txLevelDb).toInt();
    s.squelch = q.value(QStringLiteral("OrionRtty/squelch"), s.squelch).toDouble();
    s.afc = q.value(QStringLiteral("OrionRtty/afc"), s.afc).toBool();
    return s;
}

void RttyEngine::saveSettings(const Settings& s) {
    auto& q = AppSettings::instance();
    q.setValue(QStringLiteral("OrionRtty/baud"), s.baud);
    q.setValue(QStringLiteral("OrionRtty/shift"), s.shiftHz);
    q.setValue(QStringLiteral("OrionRtty/mark"), s.markHz);
    q.setValue(QStringLiteral("OrionRtty/reverse"), s.reverse);
    q.setValue(QStringLiteral("OrionRtty/usos"), s.usos);
    q.setValue(QStringLiteral("OrionRtty/diddle"), s.diddle);
    q.setValue(QStringLiteral("OrionRtty/txLevelDb"), s.txLevelDb);
    q.setValue(QStringLiteral("OrionRtty/squelch"), s.squelch);
    q.setValue(QStringLiteral("OrionRtty/afc"), s.afc);
    q.save();
}

RttyEngine::RttyEngine(int sampleRate, QObject* parent)
    : QObject(parent), rate_(sampleRate > 0 ? sampleRate : kRate) {
    demod_.onChar = [this](char c) { onChar(c); };
    apply(loadSettings());
}

RttyEngine::~RttyEngine() = default;

void RttyEngine::apply(const Settings& s, bool save) {
    s_ = s;
    const bool rev = s.reverse != usb_;        // USB flips the sense itself
    RttyDemod::Params dp;
    dp.sampleRate = rate_;
    dp.markHz = s.markHz;
    dp.spaceHz = s.markHz + s.shiftHz;
    dp.baud = s.baud;
    dp.reverse = rev;
    dp.usos = s.usos;
    dp.squelch = s.squelch;
    dp.afc = s.afc;
    demod_.setParams(dp);                      // resets the channel
    RttyMod::Params mp;
    mp.sampleRate = rate_;
    mp.markHz = s.markHz;
    mp.spaceHz = s.markHz + s.shiftHz;
    mp.baud = s.baud;
    mp.stopBits = s.baud > 60 ? 2.0 : 1.5;
    mp.reverse = rev;
    mp.usos = s.usos;
    mp.diddle = s.diddle;
    mp.level = float(std::pow(10.0, std::clamp(s.txLevelDb, -30, 0) / 20.0));
    mod_.setParams(mp);
    if (save) saveSettings(s);
}

void RttyEngine::setSidebandUsb(bool usb) {
    if (usb == usb_) return;
    usb_ = usb;
    apply(s_, false);
}

void RttyEngine::setToneCenterHz(int hz) {
    Settings s = s_;
    s.markHz = std::clamp(hz - s.shiftHz / 2, 300, 3500);
    apply(s, false);
    demod_.setAfcTrack(true);           // the operator's pick: hold here
}

double RttyEngine::effectiveMarkHz() const {
    return s_.markHz + demod_.afcHz();
}

// ---- receive ---------------------------------------------------------------

void RttyEngine::setRxEnabled(bool on) {
    if (on == rxOn_) return;
    rxOn_ = on;
    if (on) { demod_.reset(); line_.clear(); }
}

void RttyEngine::feedAudio(const float* s, int n) {
    if (!rxOn_ || n <= 0) return;
    demod_.processReal(s, size_t(n));
}

void RttyEngine::onChar(char c) {
    const QChar qc = QChar::fromLatin1(c);
    emit textDecoded(QString(qc));
    if (c == '\n') { scanLine(); line_.clear(); return; }
    line_ += qc;
    if (line_.size() > 400) line_.remove(0, 200);
    // Calls are found at word boundaries so a line without a LF (most
    // contest exchanges) still reports.
    if (c == ' ') scanLine();
}

// "CQ CQ DE W1AW W1AW K" -> W1AW; "N8EM DE W1AW" -> W1AW, to me.
void RttyEngine::scanLine() {
    const QStringList w = line_.toUpper().split(' ', Qt::SkipEmptyParts);
    for (int i = 1; i + 1 < w.size(); ++i) {
        if (w[i] != QLatin1String("DE")) continue;
        QString call = w[i + 1];
        call.remove(QRegularExpression(QStringLiteral("[^A-Z0-9/]")));
        if (!callRe().match(call).hasMatch()) continue;
        if (!myCall_.isEmpty() && call.section('/', 0, 0) == myCall_) continue;
        const bool toMe = !myCall_.isEmpty() && w[i - 1] == myCall_;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        if (call == lastHeard_ && now - lastHeardSecs_ < 10 && !toMe) continue;
        lastHeard_ = call;
        lastHeardSecs_ = now;
        emit heard(call, toMe);
    }
}

// ---- transmit --------------------------------------------------------------

void RttyEngine::txBegin() {
    mod_.clear();
    {
        // Transmit where we receive (fldigi's rule): the AFC's trim rides
        // along, so the far end hears us on the pair it is sending on.
        RttyMod::Params mp = mod_.params();
        const int mark = int(std::lround(effectiveMarkHz()));
        mp.markHz = mark;
        mp.spaceHz = mark + s_.shiftHz;
        mp.diddle = s_.diddle;
        mod_.setParams(mp);
    }
    ending_ = tailDone_ = false;
    tailSamples_ = 0;
    txOn_ = true;
}

void RttyEngine::send(const QString& text) {
    if (!txOn_) return;
    if (ending_) {
        // Text before the over finished draining: the same over goes on.
        ending_ = tailDone_ = false;
        tailSamples_ = 0;
        RttyMod::Params p = mod_.params();
        p.diddle = s_.diddle;
        mod_.setParams(p);
    }
    mod_.queue(text.toLatin1().toStdString());
    emit txProgress(int(mod_.pending()));
}

void RttyEngine::end() {
    if (!txOn_) return;
    ending_ = true;                            // txRender() finishes the job
}

void RttyEngine::txAbort() {
    mod_.clear();
    ending_ = tailDone_ = false;
    tailSamples_ = 0;
    txOn_ = false;
    emit txProgress(0);
}

bool RttyEngine::txRender(float* out, int n) {
    if (!txOn_ || n <= 0) return false;
    if (!ending_) {
        mod_.render(out, size_t(n));
        emit txProgress(int(mod_.pending()));
        return true;
    }
    if (mod_.busy()) {
        // Let the queued text finish; no diddle after it.
        RttyMod::Params p = mod_.params();
        if (p.diddle) { p.diddle = false; mod_.setParams(p); }
        mod_.render(out, size_t(n));
        emit txProgress(int(mod_.pending()));
        return true;
    }
    if (!tailDone_) {
        mod_.renderMark(out, size_t(n));
        tailSamples_ += n;
        if (tailSamples_ >= qint64(kTailMs) * rate_ / 1000) tailDone_ = true;
        emit txProgress(0);
        return true;
    }
    // Everything is out: the producer drops PTT after its own drain.
    RttyMod::Params p = mod_.params();
    p.diddle = s_.diddle;
    mod_.setParams(p);
    tailSamples_ = 0;
    ending_ = false;
    txOn_ = false;
    return false;
}

}  // namespace AetherSDR::orion::rtty
