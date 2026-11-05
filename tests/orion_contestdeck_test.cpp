// SPDX-License-Identifier: GPL-2.0-or-later
// The contest deck (gui/orion/OrionContestDeck) on the CW window, offscreen,
// no radio, no keyer: a contest started in its own database, a call and
// zone typed, Enter keying the exchange THROUGH the CW window (captured
// from CwxModel), Enter again logging, the score line, the spot classes
// (dupe / new mult / workable) that colour the band map, F12 wiping, Esc
// stopping, and the two reflexes the deck shares with the CW deck.
//
//   cmake --build build --target orion_contestdeck_test && ./build/orion_contestdeck_test

#include "core/AppSettings.h"
#include "core/backends/orion/contest/ContestDb.h"
#include "core/backends/orion/contest/ContestDef.h"
#include "core/backends/orion/contest/CtyLookup.h"
#include "core/backends/orion/log/QsoContext.h"
#include "gui/orion/OrionContestDeck.h"
#include "gui/orion/OrionContestHost.h"
#include "gui/orion/OrionCwWindow.h"
#include "models/CwxModel.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cstdio>

using namespace AetherSDR;
using namespace AetherSDR::orion::contest;
using orion::logbook::QsoContext;

// The deck is built with no rotor and no callbook here; these stand in for
// gui/orion/OrionContestHost.cpp, which needs the applet panel.
namespace AetherSDR::orion::contest {
OrionContestRotor::OrionContestRotor(OrionRotorApplet*, QObject* parent) : QObject(parent) {}
bool OrionContestRotor::connected() const { return false; }
double OrionContestRotor::azimuth() const { return -1.0; }
double OrionContestRotor::target() const { return -1.0; }
void OrionContestRotor::turnTo(double) {}
OrionContestCallbook::OrionContestCallbook(QObject* parent) : QObject(parent) {}
void OrionContestCallbook::lookup(const QString&) {}
void OrionContestCallbook::onInfo(const CallsignInfo&) {}
}  // namespace AetherSDR::orion::contest

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
    QTemporaryDir settingsDir;
    qputenv("AETHER_SETTINGS_DIR", settingsDir.path().toUtf8());
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    auto& settings = AppSettings::instance();
    settings.setValue(QStringLiteral("StationCallsign"), QStringLiteral("N8EM"));
    settings.setValue(QStringLiteral("OrionCw/myGrid"), QStringLiteral("EN81"));
    settings.setValue(QStringLiteral("OrionContest/esm"), true);

    CtyLookup cty;
    if (!cty.load(QString::fromLatin1(ORION_TEST_CTY_DAT))) {
        std::printf("FAIL  cannot load %s\n", ORION_TEST_CTY_DAT);
        return 1;
    }
    QTemporaryDir tmp;
    ContestDb db;
    check(db.open(tmp.path() + QStringLiteral("/contest.sqlite")), "the contest log opens");

    CwxModel cwx;
    QStringList sent;
    QObject::connect(&cwx, &CwxModel::transmissionRequested, &cwx,
                     [&sent](const QString& text, int) { sent << text; });
    cwx.setSpeed(28);
    QsoContext ctx;
    OrionCwWindow win(&cwx, &ctx);
    win.setStationFields(QStringLiteral("N8EM"), QStringLiteral("Jon"), QStringLiteral("Toledo"),
                         QStringLiteral("EN81"));

    QMainWindow mw;
    auto* deck = new ContestDeck(&db, &cty, &win, nullptr, nullptr, &mw);
    mw.setCentralWidget(deck);
    mw.resize(1600, 320);
    mw.show();
    QTest::qWaitForWindowExposed(&mw);

    std::printf("-- a contest starts\n");
    const ContestDef* def = contestDef(QStringLiteral("CQ-WW-CW"));
    check(def != nullptr, "CQ WW DX CW is in the rules table");
    ContestRow row;
    row.defId = def->id;
    row.title = QStringLiteral("CQ WW DX CW — test");
    row.startUtc = QDateTime::currentDateTimeUtc();
    row.sentExch = QStringLiteral("4");
    const qint64 id = db.createContest(row);
    check(id >= 0, "the contest row is created");
    check(deck->openContestId(id), "the deck opens it");
    check(deck->contestActive() && deck->currentContestId() == id, "and reports it active");
    deck->setRig(14'025'000, QStringLiteral("CW"));
    auto* call = deck->findChild<QLineEdit*>(QStringLiteral("entryCall"));
    auto* zone = deck->findChild<QLineEdit*>(QStringLiteral("exchEdit1"));
    auto* score = deck->findChild<QLabel*>(QStringLiteral("orionContestScore"));
    check(call && zone && score, "the entry boxes and the score line exist");
    if (!call || !zone || !score) return 1;

    std::printf("-- the QSO: call, zone, Enter keys the exchange, Enter logs\n");
    call->setFocus();
    QTest::keyClicks(call, QStringLiteral("dl1abc"));
    check(call->text() == QStringLiteral("DL1ABC"), "the call box uppercases");
    check(deck->classifySpot(QStringLiteral("DL1ABC"), 14'025'000) == 'M',
          "before the QSO: DL on 20 m is a new mult");
    zone->clear();
    QTest::keyClicks(zone, QStringLiteral("14"));
    sent.clear();
    QTest::keyClick(zone, Qt::Key_Return);
    check(!sent.isEmpty(), "Enter keyed something");
    check(!sent.isEmpty() && sent.join(QLatin1Char(' ')).contains(QStringLiteral("DL1ABC")),
          "the exchange carried his call");
    check(db.qsos(id).isEmpty(), "and nothing is logged yet (ESM beat one)");
    QTest::keyClick(zone, Qt::Key_Return);
    const QList<ContestQso> qsos = db.qsos(id);
    check(qsos.size() == 1, "the second Enter logged the QSO");
    if (qsos.size() == 1) {
        check(qsos.first().v.call == QStringLiteral("DL1ABC"), "with the call");
        check(qsos.first().v.exch1 == QStringLiteral("14"), "and the zone");
        check(qsos.first().v.band == QStringLiteral("20M"), "on the dial's band");
    }
    check(call->text().isEmpty(), "the entry wiped for the next one");
    check(score->text().startsWith(QStringLiteral("1 Q")), "the score line counts it");

    std::printf("-- the spot classes that colour the band map\n");
    check(deck->classifySpot(QStringLiteral("DL1ABC"), 14'025'000) == 'W',
          "the worked station on the same band is a dupe");
    check(deck->classifySpot(QStringLiteral("DL1ABC"), 7'025'000) != 'W',
          "but not on another band (per-band dupes)");
    check(deck->classifySpot(QStringLiteral("DL2XYZ"), 14'025'000) == 'N',
          "another DL in the worked zone is workable, no new mult");
    check(deck->classifySpot(QStringLiteral("JA1XYZ"), 14'025'000) == 'M',
          "a JA is a new zone and country");
    // Own country scores zero in CQ WW, but a zone still counts: W1 is
    // zone 5, N8EM zone 4, so the first W1 is a new mult, not a zero.
    check(deck->classifySpot(QStringLiteral("W1AW"), 14'025'000) == 'M',
          "own country, new zone: still a mult");

    std::printf("-- the reflexes\n");
    QTest::keyClicks(call, QStringLiteral("G3XYZ"));
    QTest::keyClick(call, Qt::Key_F12);
    check(call->text().isEmpty(), "F12 wipes the entry");
    QSignalSpy cancelled(&cwx, &CwxModel::transmissionCancelled);
    QTest::keyClick(call, Qt::Key_Escape);
    check(cancelled.count() >= 1, "Esc stops the keying through the CW window");
    QSignalSpy walks(deck, &ContestDeck::walkSpots);
    QTest::keyClick(call, Qt::Key_Right);
    check(walks.count() == 1 && walks.at(0).at(0).toInt() == 1,
          "Right on an empty call box walks the spots");
    const int wpm0 = win.speedWpm();
    deck->nudgeSpeed(+2);
    check(win.speedWpm() == wpm0 + 2 && cwx.speed() == wpm0 + 2,
          "the deck's speed moves the CW window's and the keyer's");

    std::printf("%s\n", g_failures ? "FAILED" : "all ok");
    return g_failures ? 1 : 0;
}
