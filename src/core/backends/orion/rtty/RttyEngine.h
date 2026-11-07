// SPDX-License-Identifier: GPL-3.0-or-later
// Ported from the N8EM Orion console (src/rtty/RttyEngine.h) for the AetherSDR
// Orion fork. Stage 1: namespace, include paths, the sample rate. Stage 3:
// the console's pacat transmit path is gone; the modulator is pulled by
// the host's own TX audio producer (gui/orion/OrionRttyTx).
#pragma once
#include "core/backends/orion/rtty/RttyDemod.h"
#include "core/backends/orion/rtty/RttyMod.h"
#include <QObject>
#include <QString>

namespace AetherSDR::orion::rtty {

// The console's RTTY modem, one object the main window owns (2026-11-07):
//
//   receive  — RttyDemod on the RADIO's audio (the SignaLink capture the
//              CW reader shares, AudioCwSource's raw tap): the Orion's
//              front end, filter and AGC do the selecting, as they do for
//              CW copy (operator's finding — the SDR IQ loses on air).
//              Decoded text out in lines; calls heard in CQ / DE patterns
//              out as `heard` for the band map.
//   transmit — RttyMod, pulled: AFSK on LSB, mark 2125 / space 2295,
//              170 Hz shift, 45.45 baud. txBegin() arms the modulator
//              (diddling until text arrives), the producer calls txRender()
//              for each block it pushes down the host's TX audio route,
//              end() lets the queue play out and adds a mark tail, after
//              which txRender() returns false and the producer drops PTT
//              — never before. txAbort() stops it now (Esc).
//
// The engine keys nothing itself: PTT and the radio's mode are the
// producer's, as everywhere else.
class RttyEngine : public QObject {
    Q_OBJECT
public:
    struct Settings {
        double baud = 45.45;          // 45.45 / 50 / 75
        int shiftHz = 170;            // 170 / 200 / 425 / 850
        int markHz = 2125;            // audio mark tone (space = mark + shift)
        bool reverse = false;         // swap mark/space (the other sideband)
        bool usos = true;             // unshift on space
        bool diddle = true;           // LTRS idle while keyed with nothing to say
        int txLevelDb = -6;           // AFSK level, dBFS peak (-30..0)
        double squelch = 0.5;         // demod keying-contrast squelch, 0 = off
        bool afc = true;              // the demod's pair search (+-200 Hz)
    };
    static Settings loadSettings();           // rtty/*
    static void saveSettings(const Settings& s);

    // `sampleRate`: the audio the host feeds and the modulator renders at
    // (AetherSDR's decoder PCM is 24 kHz; the console's SignaLink capture
    // was 48 kHz).
    explicit RttyEngine(int sampleRate = 48000, QObject* parent = nullptr);
    int sampleRate() const { return rate_; }
    ~RttyEngine() override;

    void apply(const Settings& s, bool save = true);   // both directions
    const Settings& settings() const { return s_; }
    // The radio's sideband. On USB the standard signal (mark the higher
    // RF) lands with the mark as the HIGHER audio tone, i.e. reversed
    // against the LSB convention the tones are named for — so USB flips
    // the sense itself; REV is the operator's extra flip on top.
    void setSidebandUsb(bool usb);
    bool sidebandUsb() const { return usb_; }
    // Put the tone pair where the operator clicked in the passband (the
    // fldigi waterfall click): mark = centre - shift/2, not saved — a
    // fresh session starts on the convention again. The AFC trims it.
    void setToneCenterHz(int hz);
    int markHz() const { return s_.markHz; }                 // nominal
    double effectiveMarkHz() const;                          // + AFC
    void resetAfc() { demod_.resetAfc(); }

    // ---- receive ----
    void setRxEnabled(bool on);
    bool rxEnabled() const { return rxOn_; }
    void feedAudio(const float* s, int n);    // 48 kHz mono, any thread-safe caller's thread = ours
    RttyDemod::Status rxStatus() const { return demod_.status(); }
    void setMyCall(const QString& c) { myCall_ = c.trimmed().toUpper(); }

    // ---- transmit ----
    void txBegin();                           // arm: tones where we receive, diddle
    void send(const QString& text);           // queue text (keyed as it comes)
    void end();                               // play out, tail, then txRender() -> false
    void txAbort();                           // now
    // Fill `out` with n mono samples of the over. False when the over is
    // complete (ended, text out, tail played): nothing was written.
    bool txRender(float* out, int n);
    bool txActive() const { return txOn_; }
    bool ending() const { return ending_; }
    size_t pending() const { return mod_.pending(); }

signals:
    void textDecoded(const QString& text);    // characters as they come
    // A call seen in "CQ ... DE CALL" / "CALL1 DE CALL2": the sender, and
    // whether CALL1 was us.
    void heard(const QString& call, bool toMe);
    void txProgress(int pendingChars);        // for the deck's "sent" line

private:
    void onChar(char c);
    void scanLine();

    Settings s_;
    int rate_ = 48000;
    bool usb_ = false;
    RttyDemod demod_;
    RttyMod mod_;
    bool rxOn_ = false;
    QString line_;                            // the line being decoded
    QString myCall_;
    QString lastHeard_;
    qint64 lastHeardSecs_ = 0;

    bool txOn_ = false;
    bool ending_ = false, tailDone_ = false;
    qint64 tailSamples_ = 0;
};

}  // namespace AetherSDR::orion::rtty
