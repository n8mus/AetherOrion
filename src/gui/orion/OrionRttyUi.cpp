#include "gui/orion/OrionRttyUi.h"

#include <QAction>
#include <QDateTime>
#include <QDockWidget>
#include <QKeySequence>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QPointer>
#include <QStatusBar>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/AppSettings.h"
#include "core/DecoderPcmAdapter.h"
#include "core/backends/orion/log/LogbookIndex.h"
#include "core/backends/orion/rtty/RttyEngine.h"
#include "gui/AppletPanel.h"
#include "gui/orion/OrionContestHost.h"
#ifdef AETHER_ORION_CONTEST
#include "gui/orion/OrionContestUi.h"
#endif
#include "gui/orion/OrionCwUi.h"
#include "gui/orion/OrionDeckArrows.h"
#include "gui/orion/OrionLogbookUi.h"
#include "gui/orion/OrionRttyDeck.h"
#include "gui/orion/OrionRotorApplet.h"
#include "core/backends/orion/contest/CtyLookup.h"
#include "core/backends/orion/log/OrionLogbook.h"
#include "models/DecoderAudioModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/SpotModel.h"

namespace AetherSDR {

using orion::contest::CtyLookup;
using orion::contest::OrionContestCallbook;
using orion::contest::OrionContestRotor;
using orion::rtty::RttyDeck;
using orion::rtty::RttyEngine;

namespace {
QPointer<QDockWidget> g_dock;
QPointer<QAction> g_deckAction;
QPointer<RttyDeck> g_deck;

// The console's cty.dat reader, loaded once from AetherSDR's own copy.
const CtyLookup& cty()
{
    static CtyLookup lookup;
    static bool tried = false;
    if (!tried) {
        tried = true;
        lookup.load(QStringLiteral(":/cty.dat"));
    }
    return lookup;
}
}  // namespace

bool setOrionRttyDeckVisible(bool on)
{
    if (!g_deckAction) {
        return false;
    }
    if (g_deckAction->isChecked() != on) {
        g_deckAction->setChecked(on);
    }
    return true;
}

void installOrionRttyDeck(QMainWindow* window, QMenu* menu, RadioModel* model,
                          std::function<SliceModel*()> activeSlice,
                          std::function<void(const QString&, double, const QString&)> spot)
{
    auto* act = menu->addAction(QObject::tr("RTTY panel"));
    act->setCheckable(true);
    act->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Y")));
    act->setToolTip(QObject::tr("The RTTY deck under the spectrum: the console's modem reading "
                                "the slice's audio, CALL/NAME/RST entry with LOG IT, F1-F12, "
                                "a type-to-send pane. Transmit is not wired on this radio yet."));
    g_deckAction = act;

    const auto say = [window](const QString& text, int ms) {
        if (window->statusBar()) window->statusBar()->showMessage(text, ms);
    };
    const auto rigHz = [activeSlice]() -> qint64 {
        SliceModel* s = activeSlice ? activeSlice() : nullptr;
        return s ? qint64(std::llround(s->frequency() * 1e6)) : 0;
    };

    auto deck = std::make_shared<QPointer<RttyDeck>>();
    auto engine = std::make_shared<QPointer<RttyEngine>>();
    auto audio = std::make_shared<std::unique_ptr<DecoderAudioModel>>();

    const auto build = [=] {
        if (*deck) return;
        // The modem, at AetherSDR's decoder PCM rate.
        auto* eng = new RttyEngine(DecoderPcmBlock::kSampleRateHz, window);
        *engine = eng;
        eng->apply(RttyEngine::loadSettings(), /*save=*/false);
        eng->setMyCall(AppSettings::instance().value(QStringLiteral("StationCallsign")).toString());

        OrionRotorApplet* applet = nullptr;
        if (auto* panel = window->findChild<AppletPanel*>()) applet = panel->orionRotorApplet();
        auto* rotor = new OrionContestRotor(applet, window);
        auto* callbook = new OrionContestCallbook(window);
        orion::logbook::LogbookIndex* idx = nullptr;
#ifdef AETHER_ORION_LOGBOOK
        if (orion::logbook::OrionLogbook* book = orionLogbook()) idx = book->index();
#endif
        auto* d = new RttyDeck(eng, idx, &cty(), rotor, callbook, window);
        *deck = d;
#ifdef AETHER_ORION_LOGBOOK
        d->setContext(orionQsoContext());
#endif

        // Receive: the active slice's audio, 24 kHz mono, into the demod.
        *audio = std::make_unique<DecoderAudioModel>(*model, DecoderAudioModel::Consumer::Rtty);
        QObject::connect(audio->get(), &DecoderAudioModel::pcmReady, eng,
                         [eng](const DecoderPcmBlock& block) {
                             if (!block.samples.isEmpty()) {
                                 eng->feedAudio(block.samples.constData(), block.samples.size());
                             }
                         });
        QObject::connect(eng, &RttyEngine::textDecoded, d, &RttyDeck::appendRead);
        // Calls heard in CQ / DE patterns: onto the band map as RTTY spots.
        QObject::connect(eng, &RttyEngine::heard, window, [=](const QString& call, bool) {
            if (!model) return;
            const qint64 hz = rigHz();
            if (hz <= 0) return;
            static int nextRttyId = -800000;
            QMap<QString, QString> kvs;
            kvs[QStringLiteral("callsign")] = call;
            kvs[QStringLiteral("rx_freq")] = QString::number(hz / 1e6, 'f', 6);
            kvs[QStringLiteral("tx_freq")] = QString::number(hz / 1e6, 'f', 6);
            kvs[QStringLiteral("source")] = QStringLiteral("RTTY");
            kvs[QStringLiteral("mode")] = QStringLiteral("RTTY");
            kvs[QStringLiteral("lifetime_seconds")] = QString::number(10 * 60);
            kvs[QStringLiteral("timestamp")] = QString::number(QDateTime::currentSecsSinceEpoch());
            kvs[QStringLiteral("color")] = QStringLiteral("#FF5DB2F0");
            model->spotModel().applySpotStatus(nextRttyId--, kvs);
        });

        // LOG from the deck: the shared record into the station log.
        QObject::connect(d, &RttyDeck::logRequested, window, [=] {
#ifdef AETHER_ORION_LOGBOOK
            bool ok = false;
            const QString line = orionLogFromContext(rigHz(), QStringLiteral("RTTY"), &ok);
            say(line, ok ? 4000 : 6000);
            if (ok && *deck) (*deck)->clearForNext();
#else
            say(QObject::tr("The station log is not built in (Qt6::Sql missing)."), 6000);
#endif
        });
        QObject::connect(d, &RttyDeck::spotDxRequested, window,
                         [spot, say](const QString& call, qint64 hz) {
                             if (!spot) {
                                 say(QObject::tr("spot NOT sent — no cluster hook"), 4000);
                                 return;
                             }
                             spot(call, hz / 1e6, QString());
                         });
        // Transmit: stage 3 (the host's own TX audio path).
        QObject::connect(d, &RttyDeck::sendRequested, window, [=](const QString&, bool) {
            say(QObject::tr("RTTY transmit is not wired on this radio yet (receive only)"), 5000);
            (*deck)->showTx(false);
        });
        QObject::connect(d, &RttyDeck::txHoldChanged, window, [=](bool on) {
            if (on) say(QObject::tr("RTTY transmit is not wired on this radio yet (receive only)"), 5000);
            (*deck)->showTx(false);
        });
        QObject::connect(d, &RttyDeck::abortRequested, window, [=] { (*deck)->showTx(false); });
        // NARROW: 500 Hz on the tone pair, through the slice's filter.
        QObject::connect(d, &RttyDeck::narrowRequested, window, [=](bool on) {
            SliceModel* s = activeSlice ? activeSlice() : nullptr;
            if (!s || !*engine) return;
            if (on) {
                const auto& st = (*engine)->settings();
                const int lo = st.markHz - 100;
                const int hi = st.markHz + st.shiftHz + 100;
                const bool lsb = s->mode().toUpper().startsWith(QLatin1String("LSB"));
                if (lsb) s->setFilterWidth(-hi, -lo);
                else s->setFilterWidth(lo, hi);
            } else {
                s->setFilterWidth(s->mode().toUpper().startsWith(QLatin1String("LSB")) ? -2800 : 100,
                                  s->mode().toUpper().startsWith(QLatin1String("LSB")) ? -100 : 2800);
            }
            (*deck)->showNarrow(on);
        });
        // ←/→: the next spot up or down the band under the dial.
        QObject::connect(d, &RttyDeck::walkSpots, window, [=](int dir) {
            SliceModel* s = activeSlice ? activeSlice() : nullptr;
            if (!s || !model) return;
            const QString band =
                orion::logbook::LogbookIndex::bandForHz(qint64(std::llround(s->frequency() * 1e6)));
            std::vector<const SpotData*> spots;
            for (auto it = model->spotModel().spots().cbegin();
                 it != model->spotModel().spots().cend(); ++it) {
                if (it->rxFreqMhz <= 0.0 || it->callsign.isEmpty()) continue;
                if (orion::logbook::LogbookIndex::bandForHz(qint64(std::llround(it->rxFreqMhz * 1e6)))
                    != band) continue;
                spots.push_back(&*it);
            }
            std::sort(spots.begin(), spots.end(), [](const SpotData* a, const SpotData* b) {
                return a->rxFreqMhz < b->rxFreqMhz;
            });
            const double here = s->frequency();
            const double eps = 0.0000201;
            const SpotData* pick = nullptr;
            if (dir > 0) {
                for (const SpotData* sp : spots) if (sp->rxFreqMhz > here + eps) { pick = sp; break; }
            } else {
                for (auto it = spots.rbegin(); it != spots.rend(); ++it)
                    if ((*it)->rxFreqMhz < here - eps) { pick = *it; break; }
            }
            if (!pick) {
                say(QObject::tr("no spot %1 on %2 to walk")
                        .arg(dir > 0 ? QObject::tr("above") : QObject::tr("below"), band), 4000);
                return;
            }
            s->setFrequency(pick->rxFreqMhz);
            d->prefillCall(pick->callsign, qint64(std::llround(pick->rxFreqMhz * 1e6)));
        });
        // The dial, mode and sideband sense, once a second while the deck is up.
        auto* feed = new QTimer(d);
        feed->setInterval(1000);
        QObject::connect(feed, &QTimer::timeout, d, [=] {
            if (!(*deck)->isVisible()) return;
            SliceModel* s = activeSlice ? activeSlice() : nullptr;
            (*deck)->setRig(rigHz(), QStringLiteral("RTTY"));
            if (s && *engine) (*engine)->setSidebandUsb(s->mode().toUpper().startsWith(QLatin1String("USB")));
            if (s && *audio) (*audio)->setSlice(s);
        });
        feed->start();

        auto* dock = new QDockWidget(window);
        dock->setObjectName(QStringLiteral("orionRttyDeckDock"));
        dock->setTitleBarWidget(new QWidget(dock));
        dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        dock->setAllowedAreas(Qt::BottomDockWidgetArea);
        dock->setWidget(d);
        d->setMinimumHeight(250);
        window->addDockWidget(Qt::BottomDockWidgetArea, dock);
        g_dock = dock;
        auto* arrows = new orion::DeckArrowFilter(window, [d](int dir) { emit d->walkSpots(dir); }, d);
        QObject::connect(dock, &QDockWidget::visibilityChanged, arrows,
                         &orion::DeckArrowFilter::setActive);
    };

    QObject::connect(act, &QAction::toggled, window, [=](bool on) {
        if (on) {
            setOrionCwDeckVisible(false);        // one deck under the pan
#ifdef AETHER_ORION_CONTEST
            setOrionContestDeckVisible(false);
#endif
            build();
            SliceModel* s = activeSlice ? activeSlice() : nullptr;
            if (s && *audio) (*audio)->setSlice(s);
            if (*audio) (*audio)->setEnabled(true);
            if (*engine) (*engine)->setRxEnabled(true);
            (*deck)->setRig(rigHz(), QStringLiteral("RTTY"));
            g_dock->show();
            (*deck)->setFocus();
            say(QObject::tr("RTTY: reading the slice's audio; the AFC locks the strongest tone pair "
                            "in the passband. Receive only for now."), 8000);
        } else if (g_dock) {
            if (*engine) (*engine)->setRxEnabled(false);
            if (*audio) (*audio)->setEnabled(false);
            g_dock->hide();
        }
    });
}

}  // namespace AetherSDR
