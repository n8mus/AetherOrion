#pragma once

// N8EM fork: the CW sending/reading window, the Orion console's CwWindow
// re-expressed on AetherSDR's own plumbing (step 10). Fork rule 2 allows the
// console's appearance here (operator, 2026-10-19): this window replaces
// AetherSDR's CWX panel for the station, so it has to look and work like the
// one the operator already has muscle memory for.
//
// What is the console's: the layout, the dark shack-distance styling, the
// type-ahead line with Word/Live keying, the DX box, F1-F12 macros on three
// rows of four with right-click editing, and the plain-language macro
// vocabulary (core/backends/orion/CwMacros.h).
//
// What is AetherSDR's: every byte on the air goes through CwxModel, so the
// TX coordinator, TUNE interlock, sidetone and the backend's own keyer path
// are exactly the ones the CWX panel uses. Decoded CW comes from
// AetherSDR's CwRxModel. Nothing here talks to a serial port.
//
// The QSO it is about is shared with the Log QSO window through
// logbook::QsoContext, so a call or a report typed in either window is what
// the F-keys send. See FORK-NOTES.md.

#include <functional>

#include <QDialog>

#include "core/backends/orion/CwMacros.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>

class QAbstractButton;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QGroupBox;
class QPushButton;
class QSlider;
class QSpinBox;

namespace AetherSDR {

class CwxModel;

namespace orion::logbook {
class QsoContext;
}

class OrionCwWindow : public QDialog {
    Q_OBJECT

public:
    static constexpr int kFkeys = 12;

    // `cwx` keys the text; `ctx` is the shared current-QSO record (may be
    // null when the logbook is not built in).
    OrionCwWindow(CwxModel* cwx, orion::logbook::QsoContext* ctx, QWidget* parent = nullptr);

    // The operator's own callsign/name/QTH/grid for the [my ...] fields.
    void setStationFields(const QString& call, const QString& name, const QString& qth,
                          const QString& grid);
    // Arms [his call] AND shows the call in the DX box, so every click path
    // (spot, decode pane, skimmer) is visible and correctable.
    void setHisCall(const QString& call);
    [[nodiscard]] QString hisCall() const;

    // Steady key-down for tuning; the host supplies it (AetherSDR's TUNE).
    void setTuneHandler(std::function<void(bool)> handler);
    // AetherSDR's own zero-beat, put within reach of the typing hand. The
    // button appears only once a host installs this.
    void setZeroBeatHandler(std::function<void()> handler);
    // The reader's decoders, and which one is running. `choose` is called
    // with the chosen key; the window remembers it across restarts.
    void setDecoders(const QStringList& keys, const QString& current,
                     std::function<void(const QString&)> choose);

    // The radio's own CW settings (sidetone volume and pitch, QSK delay,
    // envelope attack/decay) and its internal keyer, read-only. `apply` is
    // called with the fields the operator changed; `showRigCw` puts the
    // radio's answers back on the controls without echoing a set down the
    // CAT link. Hidden until a host installs the handler.
    void setRigCwHandler(std::function<void(const QVariantMap&)> apply);
    void showRigCw(const QVariantMap& state);

    // ── The CW deck's hooks (gui/orion/OrionCwDeck) ─────────────────────
    // The deck is a second front end on this window's keyer, decoder choice
    // and rig controls — it must never grow a keyer of its own. These are
    // the hooks it drives; the window stays the single master.
    // Typed text with the plain-language fields expanded; returns the name
    // of an empty field the text asked for (nothing keyed), or "".
    QString sendTyped(const QString& text);
    // The contest deck's keying: its engine expanded the macro already,
    // so the text goes out as it is, no fields, no refusal.
    void keyExternal(const QString& text);
    void streamChars(const QString& chars);  // live keys: raw, no expansion
    void unsendChars(int n);                 // live keys: backspace
    void stopKeying();                       // Esc / STOP, from outside
    [[nodiscard]] int speedWpm() const;
    void setSpeedWpm(int wpm);
    enum class Toggle { Live, Word, RxDecode };
    [[nodiscard]] QAbstractButton* toggle(Toggle t) const;   // the window's own box
    enum class RigCtl { Vol, Pitch, Qsk, Rise };
    [[nodiscard]] QSlider* rigSlider(RigCtl c) const;        // the window's own slider
    [[nodiscard]] QString rigKeyerText() const;              // "on · 29 wpm · wt 50"
    [[nodiscard]] bool rigCwAvailable() const;
    [[nodiscard]] QComboBox* decoderBox() const { return m_decoder; }
    [[nodiscard]] bool tuneAvailable() const { return static_cast<bool>(m_tune); }
    void setTune(bool on);                   // the TUNE button, from outside
    void editFkey(int i);                    // the F-key editor dialog
    [[nodiscard]] QString fkeySpec(int i) const;   // "label|text"
    [[nodiscard]] static QString fkeyDefault(int i);

public slots:
    void appendRx(const QString& text);   // decoded CW from AetherSDR's reader
    void setRxStats(float pitchHz, float wpm);
    void clearRx();

signals:
    // True while the window is visible with RX decode checked — the host
    // gates the CW reader on it, so it costs nothing while closed.
    void rxDecodeWanted(bool on);
    // Double-click on a callsign-shaped token in the decode pane: the fldigi
    // move — copy says who they are, one gesture logs them.
    void callDoubleClicked(const QString& call);
    // A DX call is settled (typed and left, or spent on the air).
    void hisCallEntered(const QString& call);
    // Keying speed moved (spin, pot, deck) — mirrors follow.
    void speedChanged(int wpm);
    void rigKeyerTextChanged(const QString& line);
    void rigCwAvailableChanged(bool on);
    // The F-key memories were edited (labels on every mirror go stale).
    void fkeysChanged();
    // LOG button / Enter in an RST box: log the shared QsoContext as it
    // stands (the host owns the log and the fan-out).
    void logRequested();

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    QString sendText(const QString& text);   // returns the empty field name
    void sendLine();
    // The fields an F-key macro can spend, read fresh at every press.
    [[nodiscard]] orion::CwMacroFields macroFieldsNow() const;
    void editMemory(int i);
    void applyFkeyButton(int i);
    void applyContext();                 // ctx -> the DX box
    void pushDxCall(const QString& call);
    void announceHisCall();
    void tintDxCall();
    void updateRxInfo();
    void updateStatus(const QString& s = QString());

    CwxModel* m_cwx = nullptr;
    orion::logbook::QsoContext* m_ctx = nullptr;
    bool m_ctxEcho = false;              // applying ctx -> don't write back
    std::function<void(bool)> m_tune;

    QLineEdit* m_line = nullptr;
    QLineEdit* m_dxCall = nullptr;
    QLineEdit* m_rstS = nullptr;         // report given / taken, shared
    QLineEdit* m_rstR = nullptr;         //   with LOG and the CW deck
    QLabel* m_status = nullptr;
    QLabel* m_sentView = nullptr;
    QSpinBox* m_wpm = nullptr;
    QPushButton* m_tuneBtn = nullptr;
    QPushButton* m_zeroBeatBtn = nullptr;
    std::function<void()> m_zeroBeat;
    QCheckBox* m_live = nullptr;         // stream keystrokes as typed
    QCheckBox* m_word = nullptr;         // space bar releases each word
    QCheckBox* m_rxOn = nullptr;
    QGroupBox* m_rigBox = nullptr;
    QSlider* m_rigVol = nullptr;
    QSlider* m_rigPitch = nullptr;
    QSlider* m_rigQsk = nullptr;
    QSlider* m_rigAttack = nullptr;
    QLabel* m_rigVolVal = nullptr;
    QLabel* m_rigPitchVal = nullptr;
    QLabel* m_rigQskVal = nullptr;
    QLabel* m_rigAttackVal = nullptr;
    QLabel* m_rigKeyer = nullptr;
    std::function<void(const QVariantMap&)> m_rigCw;
    QComboBox* m_decoder = nullptr;
    std::function<void(const QString&)> m_chooseDecoder;
    QLabel* m_rxInfo = nullptr;
    QPlainTextEdit* m_rx = nullptr;
    QPushButton* m_mem[kFkeys] = {};

    QString m_myCall, m_myName, m_myQth, m_myGrid;
    QString m_hisCall;
    QString m_pushedCall;                // last call announced
    int m_prevLen = 0;                   // live mode: chars already streamed
    int m_rxWpm = 0;
    double m_rxPitch = -1.0;
    double m_shownHz = -1.0;             // the Hz actually on the label
};

}  // namespace AetherSDR
