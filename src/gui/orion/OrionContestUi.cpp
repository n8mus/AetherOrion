#include "gui/orion/OrionContestUi.h"

#include <QAction>
#include <QDateTime>
#include <QDockWidget>
#include <QMap>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QStatusBar>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/AppSettings.h"
#include "core/backends/IRadioBackend.h"
#include "core/backends/orion/MasterScp.h"
#include "core/backends/orion/contest/ContestDb.h"
#include "core/backends/orion/contest/CtyLookup.h"
#include "gui/AppletPanel.h"
#include "gui/orion/OrionContestDeck.h"
#include "gui/orion/OrionContestHost.h"
#include "gui/orion/OrionContestWindow.h"
#include "gui/orion/OrionCwUi.h"
#include "gui/orion/OrionCwWindow.h"
#include "gui/orion/OrionDeckArrows.h"
#include "core/backends/orion/log/LogbookIndex.h"
#include "gui/orion/OrionLogbookUi.h"
#include "gui/orion/OrionRotorApplet.h"
#include "core/backends/orion/log/OrionLogbook.h"
#include "models/CwRxModel.h"
#include "models/TransmitModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/SpotModel.h"

namespace AetherSDR {

using orion::contest::ContestDb;
using orion::contest::ContestDeck;
using orion::contest::ContestWindow;
using orion::contest::CtyLookup;
using orion::contest::OrionContestCallbook;
using orion::contest::OrionContestRotor;

namespace {
QPointer<QDockWidget> g_dock;
QPointer<QAction> g_deckAction;
QPointer<ContestDeck> g_deck;

QString adifMode(const QString& sliceMode)
{
    const QString m = sliceMode.trimmed().toUpper();
    if (m == QLatin1String("USB") || m == QLatin1String("LSB")) return QStringLiteral("SSB");
    if (m == QLatin1String("CWL") || m == QLatin1String("CWU")) return QStringLiteral("CW");
    if (m == QLatin1String("DIGU") || m == QLatin1String("DIGL")) return QStringLiteral("DATA");
    return m.isEmpty() ? QStringLiteral("CW") : m;
}

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

bool setOrionContestDeckVisible(bool on)
{
    if (!g_deckAction) {
        return false;
    }
    if (g_deckAction->isChecked() != on) {
        g_deckAction->setChecked(on);
    }
    return true;
}

QColor orionContestSpotColour(const QString& call, double freqMhz)
{
    if (!g_deck || !g_deck->contestActive()) {
        return {};
    }
    switch (g_deck->classifySpot(call, qint64(std::llround(freqMhz * 1e6)))) {
    case 'M': return QColor(255, 82, 82);    // new mult
    case 'N': return QColor(93, 178, 240);   // workable
    case 'W': return QColor(108, 122, 136);  // dupe
    case 'Z': return QColor(74, 74, 74);     // zero points
    default: return {};
    }
}

void installOrionContest(QMainWindow* window, QMenu* menu, RadioModel* model,
                         CwRxModel* decoder, std::function<SliceModel*()> activeSlice,
                         std::function<void(const QString&, double, const QString&)> spot)
{
    auto* deckAct = menu->addAction(QObject::tr("Contest panel"));
    deckAct->setCheckable(true);
    deckAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    deckAct->setToolTip(QObject::tr("The contest deck under the spectrum: N1MM-style entry with "
                                    "ESM, F1-F12, super check partial, the rate and score line. "
                                    "The spectrum above is the band map."));
    auto* mgrAct = menu->addAction(QObject::tr("Contest log..."));
    mgrAct->setToolTip(QObject::tr("Start or resume a contest, the full log with edit and "
                                   "delete, the Cabrillo file, the summary"));
    g_deckAction = deckAct;

    auto db = std::make_shared<QPointer<ContestDb>>();
    const auto ensureDb = [=]() -> ContestDb* {
        if (!*db) {
            auto* d = new ContestDb(window);
            if (!d->open()) {
                if (window->statusBar()) {
                    window->statusBar()->showMessage(
                        QObject::tr("contest.db failed to open — contest mode unavailable"), 8000);
                }
                delete d;
                return nullptr;
            }
            *db = d;
        }
        return db->data();
    };
    const auto say = [window](const QString& text, int ms) {
        if (window->statusBar()) window->statusBar()->showMessage(text, ms);
    };
    const auto rigHz = [activeSlice]() -> qint64 {
        SliceModel* s = activeSlice ? activeSlice() : nullptr;
        return s ? qint64(std::llround(s->frequency() * 1e6)) : 0;
    };
    const auto rigMode = [activeSlice]() -> QString {
        SliceModel* s = activeSlice ? activeSlice() : nullptr;
        return adifMode(s ? s->mode() : QString());
    };

    // The manager window, lazily; the deck follows whatever it opens.
    auto mgr = std::make_shared<QPointer<ContestWindow>>();
    const auto openManager = [=] {
        ContestDb* d = ensureDb();
        if (!d) return;
        if (!*mgr) {
            orion::logbook::LogDb* logDb = nullptr;
#ifdef AETHER_ORION_LOGBOOK
            if (orion::logbook::OrionLogbook* book = orionLogbook()) logDb = book->db();
#endif
            auto* w = new ContestWindow(d, &cty(), logDb, window);
            *mgr = w;
            QObject::connect(w, &ContestWindow::contestOpened, window, [](qint64 id) {
                if (g_deck) g_deck->openContestId(id);
            });
            QObject::connect(w, &ContestWindow::contestDeleted, window, [](qint64 id) {
                if (g_deck && g_deck->currentContestId() == id) g_deck->closeContest();
            });
            QObject::connect(w, &ContestWindow::pushedToLogbook, window,
                             [say](const QList<qint64>& ids) {
                                 say(QObject::tr("%1 contest QSO(s) copied to the station log")
                                         .arg(ids.size()), 6000);
                             });
        }
        (*mgr)->show();
        (*mgr)->raise();
        (*mgr)->activateWindow();
    };
    QObject::connect(mgrAct, &QAction::triggered, window, openManager);

    const auto build = [=] {
        if (g_deck) return true;
        ContestDb* d = ensureDb();
        if (!d) return false;
        OrionCwWindow* cw = ensureOrionCwWindow();
        if (!cw) {
            say(QObject::tr("The CW window is not installed; the contest deck needs it."), 6000);
            return false;
        }
        OrionRotorApplet* applet = nullptr;
        if (auto* panel = window->findChild<AppletPanel*>()) applet = panel->orionRotorApplet();
        auto* rotor = new OrionContestRotor(applet, window);
        auto* callbook = new OrionContestCallbook(window);
        auto* deck = new ContestDeck(d, &cty(), cw, rotor, callbook, window);
        g_deck = deck;
        // Super check partial and the practice caller pool: MASTER.SCP, the
        // same file the skimmer checks its finds against.
        {
            const QSet<QString> scp = orion::loadMasterScp();
            deck->setMasterScp(scp);
            if (scp.isEmpty()) {
                say(QObject::tr("No MASTER.SCP found (cqrlog's copy or /usr/share/cqrlog/"
                                "ctyfiles): super check and practice have no call pool"), 8000);
            }
        }

        QObject::connect(deck, &ContestDeck::openManagerRequested, window, openManager);
        QObject::connect(deck, &ContestDeck::spotDxRequested, window,
                         [spot, say](const QString& call, qint64 hz) {
                             if (!spot) {
                                 say(QObject::tr("spot NOT sent — no cluster hook"), 4000);
                                 return;
                             }
                             spot(call, hz / 1e6, QString());
                         });
        // Phone contests: Space in the entry is the console's MOX toggle,
        // through AetherSDR's own transmit model (the MOX button's path).
        QObject::connect(deck, &ContestDeck::pttToggleRequested, window, [model] {
            if (model && model->isConnected()) {
                model->transmitModel().setMox(!model->transmitModel().isMox());
            }
        });
        // VFO B: the second slice's dial. SO2V's ears and the transmitter
        // move are the host's; a SUB take tunes A onto B's station.
        deck->setVfoBSource([model]() -> qint64 {
            if (!model) return 0;
            const QList<SliceModel*> slices = model->slices();
            return slices.size() > 1 ? qint64(std::llround(slices.at(1)->frequency() * 1e6)) : 0;
        });
        QObject::connect(deck, &ContestDeck::subTakeRequested, window,
                         [=](const QString& call) {
                             SliceModel* a = activeSlice ? activeSlice() : nullptr;
                             const QList<SliceModel*> slices = model ? model->slices() : QList<SliceModel*>{};
                             if (!a || slices.size() < 2) return;
                             const double bMhz = slices.at(1)->frequency();
                             a->setFrequency(bMhz);
                             deck->prefillCall(call, qint64(std::llround(bMhz * 1e6)));
                             say(QObject::tr("SUB: %1 — A tuned to %2 kHz")
                                     .arg(call).arg(bMhz * 1000.0, 0, 'f', 1), 6000);
                         });
        // SO2V on: A in the left ear, B in the right (the previous routing
        // remembered and put back on off), VFO B walked by the arrows while
        // RUN is lit. The transmitter never leaves on B: the deck emits
        // pounceDone before so2vChanged(false). B's own CW reader is not
        // here yet (one reader, on the active slice).
        auto prevRouting = std::make_shared<QVariantMap>();
        const auto orion = [model] {
            return model && model->isConnected()
                   && model->backendDeclaresExtension(QStringLiteral("orion"));
        };
        QObject::connect(deck, &ContestDeck::so2vChanged, window, [=](bool on) {
            if (!orion()) {
                say(QObject::tr("SO2V: no Orion connected"), 4000);
                return;
            }
            if (on) {
                static quint64 reqId = 0x534F3256ull;   // "SO2V"
                const quint64 id = ++reqId;
                auto* guard = new QObject(window);
                QObject::connect(model->backend(), &IRadioBackend::extensionResult, guard,
                                 [=](quint64 rid, const QVariant& result) {
                                     if (rid != id) return;
                                     *prevRouting = result.toMap();
                                     guard->deleteLater();
                                 });
                model->invokeBackendExtension(QStringLiteral("orion"),
                                              QStringLiteral("audio.routing.get"), id);
                QVariantMap ears;
                ears.insert(QStringLiteral("left"), QStringLiteral("M"));
                ears.insert(QStringLiteral("right"), QStringLiteral("S"));
                model->invokeBackendExtension(QStringLiteral("orion"),
                                              QStringLiteral("audio.routing.set"), 0, ears);
                say(QObject::tr("SO2V on: A in the left ear, B in the right — ←/→ walk B "
                                "while RUN is lit, Enter in SUB pounces (TX on B)"), 6000);
            } else {
                if (!prevRouting->isEmpty()) {
                    model->invokeBackendExtension(QStringLiteral("orion"),
                                                  QStringLiteral("audio.routing.set"), 0,
                                                  *prevRouting);
                    prevRouting->clear();
                }
                say(QObject::tr("SO2V off"), 3000);
            }
        });
        // Pounce: the transmitter onto B for this QSO (A keeps the run, B
        // stays on the station: no park); pounceDone puts it back on A.
        QObject::connect(deck, &ContestDeck::subPounceRequested, window, [=](const QString& call) {
            if (!orion()) return;
            const QList<SliceModel*> slices = model->slices();
            if (slices.size() > 1 && slices.at(1)->isLocked()) {
                say(QObject::tr("SO2V: pounce blocked — VFO B is LOCKED"), 6000);
                return;
            }
            QVariantMap vfo;
            vfo.insert(QStringLiteral("vfo"), QStringLiteral("B"));
            model->invokeBackendExtension(QStringLiteral("orion"), QStringLiteral("so2v.tx.set"),
                                          0, vfo);
            const double bMhz = slices.size() > 1 ? slices.at(1)->frequency() : 0.0;
            say(QObject::tr("SO2V: TX on B — work %1 on %2 kHz, A holds the run")
                    .arg(call).arg(bMhz * 1000.0, 0, 'f', 1), 6000);
        });
        QObject::connect(deck, &ContestDeck::pounceDone, window, [=] {
            if (!orion()) return;
            QVariantMap vfo;
            vfo.insert(QStringLiteral("vfo"), QStringLiteral("A"));
            model->invokeBackendExtension(QStringLiteral("orion"), QStringLiteral("so2v.tx.set"),
                                          0, vfo);
            say(QObject::tr("SO2V: TX back on A"), 4000);
        });
        // A typed, unworked call abandoned by turning the knob is parked on
        // the band map as a local spot (N1MM's "QSYing wipes the call and
        // spots it"), through the spot model the cluster feeds.
        QObject::connect(deck, &ContestDeck::callParked, window, [=](const QString& call, qint64 hz) {
            if (!model) return;
            static int nextParkedId = -700000;   // its own range, below the passive ids
            QMap<QString, QString> kvs;
            kvs[QStringLiteral("callsign")] = call;
            kvs[QStringLiteral("rx_freq")] = QString::number(hz / 1e6, 'f', 6);
            kvs[QStringLiteral("tx_freq")] = QString::number(hz / 1e6, 'f', 6);
            kvs[QStringLiteral("source")] = QStringLiteral("Parked");
            kvs[QStringLiteral("spotter_callsign")] =
                AppSettings::instance().value(QStringLiteral("StationCallsign")).toString();
            kvs[QStringLiteral("lifetime_seconds")] = QString::number(30 * 60);
            kvs[QStringLiteral("timestamp")] = QString::number(QDateTime::currentSecsSinceEpoch());
            kvs[QStringLiteral("color")] = QStringLiteral("#FFE0B45D");
            model->spotModel().applySpotStatus(nextParkedId--, kvs);
            say(QObject::tr("%1 parked at %2 kHz").arg(call).arg(hz / 1000.0, 0, 'f', 1), 4000);
        });
        // ←/→: the next workable spot up or down THIS band (dupes and zero
        // points skipped, other bands left alone), the slice tunes there,
        // the call lands.
        QObject::connect(deck, &ContestDeck::walkSpots, window, [=](int dir) {
            SliceModel* s = activeSlice ? activeSlice() : nullptr;
            if (!s || !model) return;
            // SO2V + RUN: the arrows hunt with VFO B, fenced to A's band; the
            // pan never moves and the landing goes to the SUB line.
            const bool walkB = deck->arrowsWalkB();
            const QList<SliceModel*> slices = model->slices();
            SliceModel* b = slices.size() > 1 ? slices.at(1) : nullptr;
            if (walkB && !b) {
                say(QObject::tr("SO2V: no VFO B slice to walk"), 4000);
                return;
            }
            const QString band =
                orion::logbook::LogbookIndex::bandForHz(qint64(std::llround(s->frequency() * 1e6)));
            std::vector<const SpotData*> spots;
            int onBand = 0;
            for (auto it = model->spotModel().spots().cbegin();
                 it != model->spotModel().spots().cend(); ++it) {
                if (it->rxFreqMhz <= 0.0 || it->callsign.isEmpty()) continue;
                const qint64 hz = qint64(std::llround(it->rxFreqMhz * 1e6));
                if (orion::logbook::LogbookIndex::bandForHz(hz) != band) continue;
                ++onBand;
                const char cls = deck->classifySpot(it->callsign, hz);
                if (cls == 'W' || cls == 'Z') continue;
                spots.push_back(&*it);
            }
            if (spots.empty()) {
                say(onBand == 0 ? QObject::tr("no spots on %1 to walk").arg(band)
                                : QObject::tr("no new spot on %1 (grey = dupe or zero points, skipped)")
                                      .arg(band), 4000);
                return;
            }
            std::sort(spots.begin(), spots.end(), [](const SpotData* a, const SpotData* b) {
                return a->rxFreqMhz < b->rxFreqMhz;
            });
            // B on another band is brought back to this band's first spot.
            const double bHere = b ? b->frequency() : 0.0;
            const bool bOnBand = b && orion::logbook::LogbookIndex::bandForHz(
                                          qint64(std::llround(bHere * 1e6))) == band;
            const double here = walkB ? (bOnBand ? bHere : (dir > 0 ? 0.0 : 1e9)) : s->frequency();
            const double eps = 0.0000201;
            const SpotData* pick = nullptr;
            if (dir > 0) {
                for (const SpotData* sp : spots) if (sp->rxFreqMhz > here + eps) { pick = sp; break; }
            } else {
                for (auto it = spots.rbegin(); it != spots.rend(); ++it)
                    if ((*it)->rxFreqMhz < here - eps) { pick = *it; break; }
            }
            if (!pick) {
                say(QObject::tr("no new spot %1 on %2 (grey = dupe or zero points, skipped)")
                        .arg(dir > 0 ? QObject::tr("above") : QObject::tr("below"), band), 4000);
                return;
            }
            const qint64 pickHz = qint64(std::llround(pick->rxFreqMhz * 1e6));
            if (walkB) {
                if (b->isLocked()) {
                    say(QObject::tr("VFO B is LOCKED"), 4000);
                    return;
                }
                b->setFrequency(pick->rxFreqMhz);
                deck->setSubCall(pick->callsign, pickHz);
                say(QObject::tr("VFO B -> %1 %2 kHz").arg(pick->callsign).arg(pickHz / 1000.0, 0, 'f', 1),
                    4000);
                return;
            }
            s->setFrequency(pick->rxFreqMhz);
            deck->prefillCall(pick->callsign, pickHz);
        });
        if (decoder) {
            QObject::connect(decoder, &CwRxModel::textDecoded, deck,
                             [deck](const QString& text, float) { deck->appendRead(text); });
            QObject::connect(decoder, &CwRxModel::unscoredTextDecoded, deck,
                             &ContestDeck::appendRead);
        }
        // The dial and mode once a second, and the call frame: the nearest
        // spot within 500 Hz of the dial offers itself; Space grabs it.
        auto* feed = new QTimer(deck);
        feed->setInterval(1000);
        QObject::connect(feed, &QTimer::timeout, deck, [=] {
            if (!deck->isVisible()) return;
            deck->setRig(rigHz(), rigMode());
            const qint64 cur = rigHz();
            const SpotData* best = nullptr;
            qint64 bestD = 501;
            if (model) {
                for (auto it = model->spotModel().spots().cbegin();
                     it != model->spotModel().spots().cend(); ++it) {
                    const qint64 hz = qint64(std::llround(it->rxFreqMhz * 1e6));
                    const qint64 dd = std::llabs(hz - cur);
                    if (dd < bestD) { bestD = dd; best = &*it; }
                }
            }
            if (best) {
                const qint64 hz = qint64(std::llround(best->rxFreqMhz * 1e6));
                deck->setNearbySpot(best->callsign, deck->classifySpot(best->callsign, hz), hz);
            } else {
                deck->setNearbySpot(QString(), 0, 0);
            }
        });
        feed->start();
        // The spot markers recolour as the log changes (dupes, mults).
        QObject::connect(d, &ContestDb::changed, window, [model] {
            if (model) emit model->spotModel().spotsRefreshed();
        });

        auto* dock = new QDockWidget(window);
        dock->setObjectName(QStringLiteral("orionContestDeckDock"));
        // The arrows from anywhere in the main window while the deck is up
        // (the pan included), as the console had it.
        auto* arrows = new orion::DeckArrowFilter(window, [deck](int dir) {
            emit deck->walkSpots(dir);
        }, deck);
        QObject::connect(dock, &QDockWidget::visibilityChanged, arrows,
                         &orion::DeckArrowFilter::setActive);
        dock->setTitleBarWidget(new QWidget(dock));
        dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        dock->setAllowedAreas(Qt::BottomDockWidgetArea);
        dock->setWidget(deck);
        deck->setMinimumHeight(260);
        window->addDockWidget(Qt::BottomDockWidgetArea, dock);
        g_dock = dock;
        return true;
    };

    QObject::connect(deckAct, &QAction::toggled, window, [=](bool on) {
        if (on) {
            setOrionCwDeckVisible(false);   // one deck under the pan
            if (!build()) {
                deckAct->setChecked(false);
                return;
            }
            g_deck->setRig(rigHz(), rigMode());
            g_dock->show();
            g_deck->setFocus();
            if (!g_deck->contestActive()) openManager();   // nothing loaded: pick one
            if (model) emit model->spotModel().spotsRefreshed();
        } else if (g_dock) {
            g_dock->hide();
            if (model) emit model->spotModel().spotsRefreshed();
        }
    });
}

}  // namespace AetherSDR
