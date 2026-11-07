// SPDX-License-Identifier: GPL-2.0-or-later
// The RTTY deck (gui/orion/OrionRttyDeck) offscreen, no radio, no audio
// device: AFSK rendered by the modulator at the decoder tap's 24 kHz,
// fed through the engine, lands in the read pane; the call box fills the
// shared QSO record; F1 asks the host to send (nothing keys here).
//
//   cmake --build build --target orion_rttydeck_test && ./build/orion_rttydeck_test

#include "core/AppSettings.h"
#include "core/backends/orion/contest/CtyLookup.h"
#include "core/backends/orion/log/QsoContext.h"
#include "core/backends/orion/rtty/RttyEngine.h"
#include "core/backends/orion/rtty/RttyMod.h"
#include "gui/orion/OrionContestHost.h"
#include "gui/orion/OrionRttyDeck.h"

#include <QApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cstdio>
#include <vector>

using namespace AetherSDR;
using namespace AetherSDR::orion::rtty;
using orion::logbook::QsoContext;

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
    QApplication app(argc, argv);

    const int rate = 24000;
    RttyEngine eng(rate);
    RttyEngine::Settings st = eng.settings();
    st.markHz = 2125; st.shiftHz = 170; st.baud = 45.45;
    eng.apply(st, /*save=*/false);
    eng.setRxEnabled(true);

    orion::contest::CtyLookup cty;
    RttyDeck deck(&eng, nullptr, &cty, nullptr, nullptr);
    QsoContext ctx;
    deck.setContext(&ctx);
    deck.setRig(14083000, QStringLiteral("RTTY"));
    deck.show();
    QTest::qWait(50);

    // AFSK at the tap's rate, through the engine, into the read pane.
    RttyMod::Params mp;
    mp.sampleRate = rate; mp.markHz = 2125; mp.spaceHz = 2295; mp.baud = 45.45;
    RttyMod mod(mp);
    QString got;
    QObject::connect(&eng, &RttyEngine::textDecoded, [&got](const QString& t) { got += t; });
    QObject::connect(&eng, &RttyEngine::textDecoded, &deck, &RttyDeck::appendRead);  // as the installer does
    mod.queue("  CQ CQ DE IK3ORD IK3ORD K  ");
    std::vector<float> buf(rate / 50);
    for (int i = 0; i < rate * 12 / int(buf.size()); ++i) {
        mod.render(buf.data(), buf.size());
        eng.feedAudio(buf.data(), int(buf.size()));
    }
    QTest::qWait(50);
    auto* read = deck.findChild<QPlainTextEdit*>(QStringLiteral("rttyRead"));
    check(read != nullptr, "read pane present");
    const QString text = read ? read->toPlainText() : QString();
    std::printf("  decoded [%s]; pane: [%s]\n", qPrintable(got.simplified()), qPrintable(text.simplified()));
    check(text.contains(QStringLiteral("IK3ORD")), "the call came through the modem into the read pane");

    // The call box writes the shared QSO record.
    deck.prefillCall(QStringLiteral("IK3ORD"), 14083000);
    QTest::qWait(20);
    check(ctx.call() == QStringLiteral("IK3ORD"), "call box fills the QSO context");

    // F1 asks the host to send; the host, not the deck, keys.
    QSignalSpy sent(&deck, &RttyDeck::sendRequested);
    AppSettings::instance().setValue(QStringLiteral("OrionRtty/fkey/F1"), QStringLiteral("CQ|CQ CQ DE N8EM K"));
    QTest::keyClick(&deck, Qt::Key_F1);
    QTest::qWait(20);
    check(sent.count() == 1, "F1 emits sendRequested");
    if (sent.count() == 1) {
        check(sent.at(0).at(0).toString().contains(QStringLiteral("CQ CQ")), "the F1 text is the macro");
    }

    std::printf("%s\n", g_failures ? "FAILED" : "all passed");
    return g_failures ? 1 : 0;
}
