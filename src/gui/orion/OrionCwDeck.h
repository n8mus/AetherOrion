// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// N8EM fork: the CW deck, the Orion console's src/cw/CwDeck re-expressed on
// the fork's own pieces. The contest deck's shape, cut down to a plain CW
// QSO, parked under the spectrum (operator's design, 2026-11-02):
//
//   [ CW READ ] [ CALL · NAME · S · R · 0-BEAT · LOG IT ] [ CW TYPE ]
//               [ country · wkd b4         Spot SP LP  ]
//               [ F1 .. F12, two rows of six             ]
//               [ WPM TUNE STOP     RX · reader · CW WIN ]
//               [ VOL / PITCH   QSK / RISE   (rig, over CAT) ]
//               [ read wpm · Hz                rig keyer line ]
//
// Day-to-day CW on the contest deck's reflexes: ←/→ walk the panadapter's
// spots while the call box is empty, ↑/↓ change the keying speed from ANY
// box (the type pane too), F1-F12 key the CW window's own macros, F12
// wipes, Esc stops keying, Enter in CALL moves to RST R, Enter in a report
// or the name box LOGS. The QSO is the shared QsoContext, so the Log QSO
// window, the CW window and this deck see one call and one pair of reports.
//
// The deck owns no keyer and no decoder: every send goes through the CW
// window (OrionCwWindow, the single front for CwxModel), every reader
// setting and rig slider here is a mirror of that window's own control.
// What it needs from the rest of the station (worked-before, the country,
// positions, the rotor) arrives as callables, so the deck compiles and
// runs whether or not the logbook is built in. See FORK-NOTES.md.

#include <QHash>
#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

class QComboBox;
class QDialog;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QSpinBox;

namespace AetherSDR {

struct CallsignInfo;
class OrionCwWindow;

namespace orion::logbook {
class QsoContext;
}

class OrionCwDeck : public QWidget {
    Q_OBJECT

public:
    // What the host station knows; any of these may be left empty.
    struct Station {
        // 'N' never worked, 'B' worked but not this band, 'W' worked this
        // band, 'C' confirmed this band, '?' no data.
        std::function<QChar(const QString& call, qint64 hz)> workedStatus;
        std::function<QString(const QString& call)> country;
        // The station's position from cty.dat (its country's centre).
        std::function<bool(const QString& call, double& lat, double& lon)> dxLatLon;
        std::function<bool(double& lat, double& lon)> myLatLon;
        // The rotor: present when the station has one. `turn` sends the
        // bearing; nothing here chooses to turn by itself.
        std::function<void(double azDeg)> turnRotor;
    };

    OrionCwDeck(OrionCwWindow* cw, orion::logbook::QsoContext* ctx, QWidget* parent = nullptr);

    void setStation(Station station);
    void setRig(qint64 hz, const QString& adifMode);
    [[nodiscard]] qint64 rigHz() const { return m_rigHz; }
    void nudgeSpeed(int delta);             // ↑/↓ from the pan or any box
    // A spot click / arrow landing: the call into the box, anchored at the
    // frequency it lives on. The arrows keep walking while the landing is
    // untouched.
    void prefillCall(const QString& call, qint64 hz = 0);
    [[nodiscard]] QString call() const;

    // For tests and the automation bridge.
    [[nodiscard]] QLineEdit* callBox() const { return m_call; }
    [[nodiscard]] QLineEdit* nameBox() const { return m_name; }
    [[nodiscard]] QLineEdit* rstSentBox() const { return m_rstS; }
    [[nodiscard]] QLineEdit* rstRcvdBox() const { return m_rstR; }
    [[nodiscard]] QPlainTextEdit* typePane() const { return m_type; }
    [[nodiscard]] QPlainTextEdit* readPane() const { return m_read; }
    [[nodiscard]] QString infoText() const;
    void keyFkey(int idx0);                 // F1..F12 (0-based); 11 wipes
    void stopEverything();                  // Esc

public slots:
    void appendRead(const QString& text);   // decoded CW
    void setRxStats(float pitchHz, float wpm);
    void clearForNext();                    // after a log: boxes, focus
    void refreshInfo();                     // country · wkd b4 · heading

signals:
    void walkSpots(int dir);                // ←/→ on an empty call box
    void logRequested();                    // LOG IT / Enter in a report
    void spotDxRequested(const QString& call, qint64 hz);
    void rotorAimed(double azDeg, double distKm, const QString& label);
    void zeroBeatRequested();
    void cwWindowRequested();

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    void buildUi();
    void applyFkeyLabels();
    void wipe();                            // F12
    void onCallEdited();
    void requestCallbook(const QString& call);
    void applyCallbook(const CallsignInfo& info);
    void sendTypedLine();                   // Enter in the type pane
    void onTypeChanged();                   // live / word keying
    [[nodiscard]] QString currentTypeLine() const;
    void setStatus(const QString& s);
    void setRigCwAvailable(bool on);
    void syncDecoderBox();

    // Pop-out panes (the contest deck's ⧉): read and type float as their
    // own windows; closing one returns it.
    struct PaneSlot {
        QWidget* pane = nullptr;
        QString title;
        QDialog* fly = nullptr;
        std::function<void(QWidget*)> reinsert;
    };
    QHash<QString, PaneSlot> m_panes;
    void addPopout(const QString& key, const QString& title, QWidget* pane,
                   QHBoxLayout* headerRow, std::function<void(QWidget*)> reinsert);
    void floatPane(const QString& key);
    void unfloatPane(const QString& key, bool saveGeom);
    // Right-click text size on a pane.
    void sizeMenu(QPlainTextEdit* pane, const QString& key);

    OrionCwWindow* m_cw;
    orion::logbook::QsoContext* m_ctx;
    Station m_station;
    bool m_ctxEcho = false;
    QHash<QString, QString> m_callbookGrid;   // call -> grid (hits only)
    QString m_asked;                          // the call last sent to the callbook

    qint64 m_rigHz = 0;
    QString m_rigMode = QStringLiteral("CW");
    bool m_callFromSpot = false;          // box filled by walk/click, untouched
    double m_spAz = -1.0, m_lpAz = -1.0, m_distKm = -1.0;

    // widgets
    QWidget* m_readPane = nullptr;
    QWidget* m_typePane = nullptr;
    QPlainTextEdit* m_read = nullptr;
    QPlainTextEdit* m_type = nullptr;
    QLabel* m_sent = nullptr;
    QLineEdit* m_call = nullptr;
    QLineEdit* m_name = nullptr;
    QLineEdit* m_rstS = nullptr;
    QLineEdit* m_rstR = nullptr;
    QLabel* m_info = nullptr;
    QPushButton* m_spBtn = nullptr;
    QPushButton* m_lpBtn = nullptr;
    QPushButton* m_fk[12] = {};
    QSpinBox* m_wpm = nullptr;
    QPushButton* m_tuneBtn = nullptr;
    QComboBox* m_decoder = nullptr;
    bool m_decoderEcho = false;
    QSlider* m_rigVol = nullptr;   QLabel* m_rigVolVal = nullptr;
    QSlider* m_rigPitch = nullptr; QLabel* m_rigPitchVal = nullptr;
    QSlider* m_rigQsk = nullptr;   QLabel* m_rigQskVal = nullptr;
    QSlider* m_rigRise = nullptr;  QLabel* m_rigRiseVal = nullptr;
    QWidget* m_rigRow[2] = {};
    QLabel* m_rigKeyer = nullptr;
    QLabel* m_rxInfo = nullptr;
    QLabel* m_status = nullptr;
    int m_rxWpm = 0;
    double m_rxPitch = -1.0;
    double m_shownHz = -1.0;
    QString m_typeLine;                   // the line being typed (live/word)
    int m_typeSent = 0;                   // chars of it already keyed (word)
    bool m_keysArmed = false;             // the app-level F-key filter is in
};

}  // namespace AetherSDR
