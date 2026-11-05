// SPDX-License-Identifier: GPL-2.0-or-later
// Ported from the N8EM Orion console (src/contest/QtcDialog.h) for the AetherSDR
// Orion fork (contest stage 2b). Namespace and include paths, the fork's
// stand-ins for the console's CW window, rotor, callbook and bearings
// (gui/orion/OrionContestHost.h), AppSettings for QSettings. The screen
// itself is the console's (fork rule 2, the contest exception). See
// FORK-NOTES.md.
#pragma once
#include <QDialog>
#include <QList>
#include <functional>

#include "core/backends/orion/contest/ContestDb.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace AetherSDR::orion::contest {

// WAE QTC sending. The flow that survived a real contest weekend:
// work the station → open this → Load → Send all → Confirm & log when
// he acknowledges (Cancel if it falls apart — nothing is spent). The
// To box follows the deck's call box until the operator types over it.
// Confirm is all-or-nothing; confirmed QSOs freeze forever (enforced
// by ContestDb, not discipline). Esc stops the keyer — never closes.
class QtcDialog : public QDialog {
    Q_OBJECT
public:
    QtcDialog(ContestDb* db, std::function<void(const QString&)> keyFn,
              std::function<void()> stopFn, QWidget* parent = nullptr);

    void openFor(qint64 contestId);      // reset + show
    void setRigFreq(qint64 hz) { rigHz_ = hz; }

public slots:
    void followCall(const QString& call);  // deck call box rides in

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    void load();
    void sendHeader();
    void sendRow(int i);
    void sendAll();
    void confirmBlock();
    void cancelBlock();
    void refreshCounters();
    QString rowText(int i) const;          // "0030 DL1ABC 456"

    ContestDb* db_;
    std::function<void(const QString&)> keyFn_;
    std::function<void()> stopFn_;
    qint64 contestId_ = -1;
    qint64 rigHz_ = 0;
    int block_ = 0;
    bool manualTo_ = false;                // operator typed; stop following
    QList<ContestQso> alloc_;

    QLineEdit* to_ = nullptr;
    QLabel* sentTo_ = nullptr;
    QLabel* blockLbl_ = nullptr;
    QLabel* status_ = nullptr;
    QTableWidget* table_ = nullptr;
    QPushButton* confirmBtn_ = nullptr;
};

}  // namespace AetherSDR::orion::contest
