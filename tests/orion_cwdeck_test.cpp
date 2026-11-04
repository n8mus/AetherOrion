// SPDX-License-Identifier: GPL-2.0-or-later
// The CW deck (gui/orion/OrionCwDeck) on the CW window, offscreen, no radio:
//  1. One QSO record: a call typed in the deck is the CW window's DX call
//     and the shared QsoContext's; a report typed there is what [rst sent]
//     keys; the window's own boxes follow.
//  2. The F-keys key THROUGH the window: F2 (HIS) sends "<call> DE <me>";
//     an empty field refuses and says which; F12 wipes.
//  3. The type pane: Enter keys the line; LIVE streams keystrokes.
//  4. The info line: worked-before, country and the bearing from the
//     station callables; SP/LP arm only with a rotor.
//  5. The reflexes: Enter in a report asks to log, ←/→ on an empty call box
//     walk the spots, ↑/↓ move the speed, F-keys work from the main
//     window with focus elsewhere.
//
//   cmake --build build --target orion_cwdeck_test && ./build/orion_cwdeck_test

#include "core/AppSettings.h"
#include "core/backends/orion/log/QsoContext.h"
#include "gui/orion/OrionCwDeck.h"
#include "gui/orion/OrionCwWindow.h"
#include "models/CwxModel.h"

#include <QAbstractButton>
#include <QApplication>
#include <QMenuBar>
#include <QPushButton>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include <cstdio>

using namespace AetherSDR;
using orion::logbook::QsoContext;

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s: %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) ++g_failures;
}

}  // namespace

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    // The deck reads the window's F-key memories; start from the defaults.
    for (int i = 0; i < OrionCwWindow::kFkeys; ++i) {
        AppSettings::instance().setValue(QStringLiteral("OrionCw/fkey/F") + QString::number(i + 1),
                                         OrionCwWindow::fkeyDefault(i));
    }
    AppSettings::instance().setValue(QStringLiteral("OrionCw/live"), false);
    AppSettings::instance().setValue(QStringLiteral("OrionCw/word"), false);

    CwxModel cwx;
    QStringList sent;
    QObject::connect(&cwx, &CwxModel::transmissionRequested, &cwx,
                     [&sent](const QString& text, int) { sent << text; });
    cwx.setSpeed(25);

    QsoContext ctx;
    OrionCwWindow win(&cwx, &ctx);
    win.setStationFields(QStringLiteral("N8EM"), QStringLiteral("Jon"), QStringLiteral("Toledo"),
                         QStringLiteral("EN81"));

    QMainWindow mw;
    auto* deck = new OrionCwDeck(&win, &ctx, &mw);
    mw.setCentralWidget(deck);
    mw.resize(1200, 300);
    mw.show();
    QTest::qWaitForWindowExposed(&mw);

    std::printf("-- one QSO record\n");
    QTest::keyClicks(deck->callBox(), QStringLiteral("w1aw"));
    check(deck->call() == QStringLiteral("W1AW"), "the call box uppercases");
    check(ctx.call() == QStringLiteral("W1AW"), "the shared record has the call");
    check(win.hisCall() == QStringLiteral("W1AW"), "the CW window's DX call follows");
    deck->rstRcvdBox()->clear();
    QTest::keyClicks(deck->rstRcvdBox(), QStringLiteral("579"));
    check(ctx.rstRcvd() == QStringLiteral("579"), "RST R typed in the deck is the record's");
    ctx.setRstSent(QStringLiteral("559"));
    check(deck->rstSentBox()->text() == QStringLiteral("559"),
          "a report set elsewhere lands in the deck's box");
    QTest::keyClicks(deck->nameBox(), QStringLiteral("Bob"));
    check(ctx.handle() == QStringLiteral("Bob"), "NAME is the handle, his word");

    std::printf("-- the F-keys key through the window\n");
    sent.clear();
    deck->keyFkey(1);   // HIS: [his call] DE [my call]
    check(!sent.isEmpty() && sent.last().contains(QStringLiteral("W1AW DE N8EM")),
          "F2 keyed HIS DE ME through the window");
    sent.clear();
    deck->keyFkey(2);   // RST: ... UR RST [rst sent] [rst sent] BK
    check(!sent.isEmpty() && sent.last().contains(QStringLiteral("UR RST 559 559")),
          "F3 keyed the report from the shared record");
    deck->keyFkey(11);  // F12 WIPE
    check(deck->call().isEmpty() && ctx.call().isEmpty(), "F12 wiped the call everywhere");
    check(deck->rstSentBox()->text() == QStringLiteral("599")
              && ctx.rstRcvd() == QStringLiteral("599"),
          "F12 put both reports back to 599");
    check(deck->nameBox()->text().isEmpty(), "F12 cleared the name");
    sent.clear();
    deck->keyFkey(1);
    check(sent.isEmpty(), "an empty call keys nothing");

    std::printf("-- the type pane\n");
    QTest::keyClicks(deck->callBox(), QStringLiteral("K1ABC"));
    sent.clear();
    deck->typePane()->setFocus();
    QTest::keyClicks(deck->typePane(), QStringLiteral("TU 73"));
    check(sent.isEmpty(), "nothing goes out while typing (buffered)");
    QTest::keyClick(deck->typePane(), Qt::Key_Return);
    check(!sent.isEmpty() && sent.last().contains(QStringLiteral("TU 73")), "Enter keyed the line");
    check(deck->typePane()->document()->lineCount() >= 2, "the line stays as history");
    win.toggle(OrionCwWindow::Toggle::Live)->setChecked(true);
    sent.clear();
    QTest::keyClicks(deck->typePane(), QStringLiteral("CQ"));
    check(sent.join(QString()) == QStringLiteral("CQ"), "LIVE streams each keystroke");
    win.toggle(OrionCwWindow::Toggle::Live)->setChecked(false);
    deck->stopEverything();

    std::printf("-- the info line\n");
    OrionCwDeck::Station st;
    st.workedStatus = [](const QString&, qint64) { return QChar('N'); };
    st.country = [](const QString&) { return QStringLiteral("United States"); };
    st.dxLatLon = [](const QString&, double& lat, double& lon) {
        lat = 41.7;   // Connecticut
        lon = -72.7;
        return true;
    };
    st.myLatLon = [](double& lat, double& lon) {
        lat = 41.65;   // Toledo
        lon = -83.55;
        return true;
    };
    deck->setStation(st);
    deck->setRig(7'025'000, QStringLiteral("CW"));
    check(deck->infoText().contains(QStringLiteral("United States")), "the country shows");
    check(deck->infoText().contains(QStringLiteral("NEW")), "never worked says NEW");
    check(deck->infoText().contains(QStringLiteral("°")), "a bearing shows when both ends are known");
    check(!deck->findChild<QPushButton*>(QStringLiteral("orionCwDeckSp"))->isEnabled(),
          "SP is dark without a rotor");
    double turned = -1.0;
    st.turnRotor = [&turned](double az) { turned = az; };
    deck->setStation(st);
    auto* sp = deck->findChild<QPushButton*>(QStringLiteral("orionCwDeckSp"));
    check(sp->isEnabled(), "SP lights with a rotor");
    QSignalSpy aimed(deck, &OrionCwDeck::rotorAimed);
    sp->click();
    check(turned > 60.0 && turned < 110.0, "SP turned the rotor east");
    check(aimed.count() == 1, "and said so");

    std::printf("-- the reflexes\n");
    QSignalSpy logs(deck, &OrionCwDeck::logRequested);
    QTest::keyClick(deck->rstRcvdBox(), Qt::Key_Return);
    check(logs.count() == 1, "Enter in a report asks to log");
    QTest::keyClick(deck->nameBox(), Qt::Key_Return);
    check(logs.count() == 2, "Enter in NAME asks to log");
    deck->clearForNext();
    QSignalSpy walks(deck, &OrionCwDeck::walkSpots);
    deck->callBox()->setFocus();
    QTest::keyClick(deck->callBox(), Qt::Key_Right);
    QTest::keyClick(deck->callBox(), Qt::Key_Left);
    check(walks.count() == 2 && walks.at(0).at(0).toInt() == 1 && walks.at(1).at(0).toInt() == -1,
          "arrows on an empty call box walk the spots both ways");
    deck->prefillCall(QStringLiteral("VE3XYZ"), 7'030'000);
    QTest::keyClick(deck->callBox(), Qt::Key_Right);
    check(walks.count() == 3, "an untouched landing keeps walking");
    QTest::keyClicks(deck->callBox(), QStringLiteral("A"));
    QTest::keyClick(deck->callBox(), Qt::Key_Right);
    check(walks.count() == 3, "a typed call owns the arrows");
    const int wpm0 = win.speedWpm();
    QTest::keyClick(deck->typePane(), Qt::Key_Up);
    check(win.speedWpm() == wpm0 + 1 && cwx.speed() == wpm0 + 1, "Up from the type pane raises the speed");
    QTest::keyClick(deck->callBox(), Qt::Key_Down);
    check(win.speedWpm() == wpm0, "Down from the call box lowers it");
    // F-keys from the main window, focus on a stranger: the deck's app-level
    // filter takes the key before any QShortcut can.
    deck->callBox()->clear();
    QTest::keyClicks(deck->callBox(), QStringLiteral("W1AW"));
    sent.clear();
    mw.menuBar()->setFocus();
    QTest::keyClick(&mw, Qt::Key_F2);
    check(!sent.isEmpty() && sent.last().contains(QStringLiteral("W1AW DE N8EM")),
          "F2 keys from the main window with focus elsewhere");
    // Esc from the main window, focus elsewhere, stops the keying.
    win.toggle(OrionCwWindow::Toggle::Live)->setChecked(true);
    deck->typePane()->setFocus();
    QTest::keyClicks(deck->typePane(), QStringLiteral("CQ"));
    mw.menuBar()->setFocus();
    QSignalSpy cleared(&cwx, &CwxModel::transmissionCancelled);
    QTest::keyClick(&mw, Qt::Key_Escape);
    check(cleared.count() >= 1, "Esc from the main window stops the keying");
    win.toggle(OrionCwWindow::Toggle::Live)->setChecked(false);
    mw.hide();
    sent.clear();
    QTest::keyClick(&mw, Qt::Key_F2);
    check(sent.isEmpty(), "a hidden deck has no keys");

    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
