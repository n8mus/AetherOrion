#include "gui/orion/OrionCwUi.h"

#include <QAction>
#include <QDockWidget>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QStatusBar>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/backends/IRadioBackend.h"

#include <functional>

#include "core/AppSettings.h"
#include "core/CtyDatParser.h"
#include "core/MaidenheadLocator.h"
#include "gui/AppletPanel.h"
#include "gui/orion/OrionCwDeck.h"
#include "gui/orion/OrionCwWindow.h"
#include "gui/orion/OrionRotorApplet.h"
#ifdef AETHER_ORION_LOGBOOK
#include "core/backends/orion/log/OrionLogbook.h"
#include "gui/orion/OrionLogbookUi.h"
#endif
#include "models/CwRxModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/SpotModel.h"

namespace AetherSDR {

namespace {
// The one CW window: how to build it without showing it (the deck needs
// it standing behind the scenes) and how to raise it from elsewhere in the
// main window (the CWX indicator opens this instead of AetherSDR's CWX
// panel).
std::function<OrionCwWindow*()> g_ensure;
std::function<void()> g_open;
// The deck's dock, once built.
QPointer<QDockWidget> g_deckDock;
QPointer<QAction> g_deckAction;

// The slice's mode as the log spells it.
QString adifMode(const QString& sliceMode)
{
    const QString m = sliceMode.trimmed().toUpper();
    if (m == QLatin1String("USB") || m == QLatin1String("LSB")) return QStringLiteral("SSB");
    if (m == QLatin1String("CWL") || m == QLatin1String("CWU")) return QStringLiteral("CW");
    if (m == QLatin1String("DIGU") || m == QLatin1String("DIGL")) return QStringLiteral("DATA");
    return m.isEmpty() ? QStringLiteral("CW") : m;
}
}  // namespace

bool orionCwReplacesCwxPanel()
{
    return AppSettings::instance()
        .value(QStringLiteral("OrionCw/replaceCwx"), true)
        .toBool();
}

void installOrionCwWindow(QMainWindow* window, QMenu* menu, RadioModel* model,
                          CwRxModel* decoder, std::function<void()> zeroBeat,
                          std::function<void(const QString&)> selectDecoder)
{
    menu->addSeparator();
    auto* act = menu->addAction(QObject::tr("CW window..."));
    act->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+W")));
    act->setToolTip(QObject::tr("Type-ahead CW, F1-F12 macros in plain language, and the CW "
                                "reader — sharing the station being worked with the Log QSO "
                                "window"));

    auto cw = std::make_shared<QPointer<OrionCwWindow>>();
    const auto ensure = [=]() -> OrionCwWindow* {
        if (!*cw) {
            orion::logbook::QsoContext* ctx = nullptr;
#ifdef AETHER_ORION_LOGBOOK
            ctx = orionQsoContext();
#endif
            auto* w = new OrionCwWindow(&model->cwxModel(), ctx, window);
            *cw = w;

            // The operator's own station, for the [my ...] macro fields. The
            // callsign is AetherSDR's own; it has no setting for the
            // operator's name, town or grid, so those three are the fork's
            // (Radio Setup -> Orion).
            auto& s = AppSettings::instance();
            w->setStationFields(s.value(QStringLiteral("StationCallsign")).toString(),
                                s.value(QStringLiteral("OrionCw/myName")).toString(),
                                s.value(QStringLiteral("OrionCw/myQth")).toString(),
                                s.value(QStringLiteral("OrionCw/myGrid")).toString());

            // The radio's own CW settings, both ways: the window sends what
            // the operator changed, and the backend's reports put the
            // radio's answers back on the controls. Aether's side panel is
            // untouched and keeps working — both are views of one radio.
            {
                // Re-armed on each connect, so a reconnect does not leave a
                // dead listener behind — and so a window built before the
                // radio answered (the CW deck builds it at startup) still
                // gets the rig box once it does.
                auto guard = std::make_shared<QPointer<QObject>>();
                const auto arm = [model, w, guard] {
                    delete guard->data();
                    *guard = nullptr;
                    if (!model->isConnected() || !model->backend()
                        || !model->backendDeclaresExtension(QStringLiteral("orion"))) {
                        return;
                    }
                    if (!w->rigCwAvailable()) {
                        w->setRigCwHandler([model](const QVariantMap& fields) {
                            model->invokeBackendExtension(QStringLiteral("orion"),
                                                          QStringLiteral("cw.rig.set"), 0, fields);
                        });
                    }
                    *guard = new QObject(w);
                    QObject::connect(model->backend(), &IRadioBackend::extensionStatus,
                                     guard->data(),
                                     [w](const QString& ns, const QString& kind,
                                         const QVariantMap& f) {
                                         if (ns == QLatin1String("orion")
                                             && kind == QLatin1String("cw.rig")) {
                                             w->showRigCw(f);
                                         }
                                     });
                    // And ask once, so the window opens showing the radio.
                    model->invokeBackendExtension(QStringLiteral("orion"),
                                                  QStringLiteral("cw.rig.get"), 0);
                };
                QObject::connect(model, &RadioModel::connectionStateChanged, w,
                                 [arm](bool) { arm(); });
                arm();
            }
            if (zeroBeat) {
                w->setZeroBeatHandler(zeroBeat);
            }
            if (decoder) {
                // The reader's decoders. The operator's pick is remembered;
                // AetherSDR's own is the first-run default, and a key that
                // no longer exists falls back to it rather than to nothing.
                const QStringList keys = CwRxModel::availableBackends();
                QString want = AppSettings::instance()
                                   .value(QStringLiteral("OrionCw/decoder"),
                                          QStringLiteral("ggmorse"))
                                   .toString();
                if (!keys.contains(want)) {
                    want = QStringLiteral("ggmorse");
                }
                // Through the host, never straight into the model: see the
                // header. Without a host hook the box still shows which
                // reader is running, but cannot change it.
                if (selectDecoder && decoder->backendKey() != want) {
                    selectDecoder(want);
                }
                w->setDecoders(keys, want, selectDecoder);
            }
            if (decoder) {
                // AetherSDR's CW reader feeds the console-style pane. Both
                // text signals: a backend without a calibrated cost emits
                // the unscored one.
                QObject::connect(decoder, &CwRxModel::textDecoded, w,
                                 [w](const QString& text, float) { w->appendRx(text); });
                QObject::connect(decoder, &CwRxModel::unscoredTextDecoded, w,
                                 &OrionCwWindow::appendRx);
                QObject::connect(decoder, &CwRxModel::statsUpdated, w,
                                 [w](float pitch, float speed) { w->setRxStats(pitch, speed); });
            }
        }
        return cw->data();
    };
    const auto open = [ensure] {
        OrionCwWindow* w = ensure();
        w->show();
        w->raise();
        w->activateWindow();
    };
    QObject::connect(act, &QAction::triggered, window, open);
    g_ensure = ensure;
    g_open = open;
}

bool setOrionCwDeckVisible(bool on)
{
    if (!g_deckAction) {
        return false;
    }
    if (g_deckAction->isChecked() != on) {
        g_deckAction->setChecked(on);   // toggled -> the dock
    }
    return true;
}

void installOrionCwDeck(QMainWindow* window, QMenu* menu, RadioModel* model,
                        CwRxModel* decoder, std::function<SliceModel*()> activeSlice,
                        std::function<void(const QString&, double, const QString&)> spot,
                        std::function<void()> zeroBeat)
{
    if (!g_ensure) {
        return;   // installOrionCwWindow() first
    }
    auto* act = menu->addAction(QObject::tr("CW panel"));
    act->setCheckable(true);
    act->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    act->setToolTip(QObject::tr("A CW deck under the spectrum: reader, CALL/NAME/RST entry with "
                                "LOG IT, F1-F12, the keyer and the radio's CW controls, a "
                                "type-to-send pane. Arrows walk the spots, up/down change "
                                "speed, F12 wipes."));
    g_deckAction = act;

    // The dial and mode the deck logs at: the active slice, read when
    // asked. Hz, because the log and the band map think in Hz.
    const auto rigHz = [activeSlice]() -> qint64 {
        SliceModel* s = activeSlice ? activeSlice() : nullptr;
        return s ? qint64(std::llround(s->frequency() * 1e6)) : 0;
    };
    const auto rigMode = [activeSlice]() -> QString {
        SliceModel* s = activeSlice ? activeSlice() : nullptr;
        return adifMode(s ? s->mode() : QString());
    };
    const auto say = [window](const QString& text, int ms) {
        if (window->statusBar()) window->statusBar()->showMessage(text, ms);
    };

    // LOG from the CW side, whichever button asked (the CW window's LOG,
    // the deck's LOG IT): the shared record into the station log.
    auto deck = std::make_shared<QPointer<OrionCwDeck>>();
    const auto logFromContext = [=] {
#ifdef AETHER_ORION_LOGBOOK
        bool ok = false;
        const QString line = orionLogFromContext(rigHz(), rigMode(), &ok);
        say(line, ok ? 4000 : 6000);
        if (ok && *deck) (*deck)->clearForNext();
#else
        say(QObject::tr("The station log is not built in (Qt6::Sql missing)."), 6000);
#endif
    };
    {
        OrionCwWindow* w = g_ensure();
        QObject::connect(w, &OrionCwWindow::logRequested, window, logFromContext);
    }

    const auto build = [=] {
        if (*deck) return;
        OrionCwWindow* w = g_ensure();
        orion::logbook::QsoContext* ctx = nullptr;
#ifdef AETHER_ORION_LOGBOOK
        ctx = orionQsoContext();
#endif
        auto* d = new OrionCwDeck(w, ctx, window);
        *deck = d;

        // What the station knows.
        OrionCwDeck::Station st;
        st.myLatLon = [model](double& lat, double& lon) {
            // The operator's own grid (Radio Setup -> Orion), else the one
            // the radio reports.
            const QString mine =
                AppSettings::instance().value(QStringLiteral("OrionCw/myGrid")).toString().trimmed();
            if (MaidenheadLocator::toLatLon(mine, lat, lon)) return true;
            return model && MaidenheadLocator::toLatLon(model->gpsGrid(), lat, lon);
        };
#ifdef AETHER_ORION_LOGBOOK
        if (orion::logbook::OrionLogbook* book = orionLogbook()) {
            st.workedStatus = [book](const QString& call, qint64 hz) {
                return book->workedStatus(call, hz);
            };
            st.country = [book](const QString& call) { return book->country(call).name; };
            st.dxLatLon = [book](const QString& call, double& lat, double& lon) {
                const CtyDatParser* cty = book->cty();
                if (!cty) return false;
                const DxccEntity* e = cty->entityForCallsign(call);
                if (!e || !e->hasLatLon) return false;
                lat = e->latitude;
                lon = e->longitude;
                return true;
            };
            QObject::connect(book, &orion::logbook::OrionLogbook::workedIndexUpdated, d,
                             &OrionCwDeck::refreshInfo);
        }
#endif
        // The rotor tile on the right-hand panel, when the station has one.
        if (auto* panel = window->findChild<AppletPanel*>()) {
            if (OrionRotorApplet* rotor = panel->orionRotorApplet()) {
                st.turnRotor = [rotor](double az) { rotor->turnTo(az); };
            }
        }
        d->setStation(std::move(st));

        QObject::connect(d, &OrionCwDeck::logRequested, window, logFromContext);
        QObject::connect(d, &OrionCwDeck::cwWindowRequested, window, [] {
            if (g_open) g_open();
        });
        if (zeroBeat) {
            QObject::connect(d, &OrionCwDeck::zeroBeatRequested, window, zeroBeat);
        }
        QObject::connect(d, &OrionCwDeck::spotDxRequested, window,
                         [spot, say](const QString& call, qint64 hz) {
                             if (!spot) {
                                 say(QObject::tr("spot NOT sent — no cluster hook"), 4000);
                                 return;
                             }
                             spot(call, hz / 1e6, QString());
                         });
        // ←/→: the next spot up or down the band from the dial, wrapping;
        // the slice tunes there and the call lands in the box.
        QObject::connect(d, &OrionCwDeck::walkSpots, window, [=](int dir) {
            SliceModel* s = activeSlice ? activeSlice() : nullptr;
            if (!s || !model) return;
            std::vector<const SpotData*> spots;
            for (auto it = model->spotModel().spots().cbegin();
                 it != model->spotModel().spots().cend(); ++it) {
                if (it->rxFreqMhz > 0.0 && !it->callsign.isEmpty()) spots.push_back(&*it);
            }
            if (spots.empty()) {
                say(QObject::tr("no spots on the map"), 2000);
                return;
            }
            std::sort(spots.begin(), spots.end(), [](const SpotData* a, const SpotData* b) {
                return a->rxFreqMhz < b->rxFreqMhz;
            });
            const double here = s->frequency();
            const double eps = 0.0000201;   // 20.1 Hz: "on this spot already"
            const SpotData* pick = nullptr;
            if (dir > 0) {
                for (const SpotData* sp : spots) {
                    if (sp->rxFreqMhz > here + eps) { pick = sp; break; }
                }
                if (!pick) pick = spots.front();   // wrap
            } else {
                for (auto it = spots.rbegin(); it != spots.rend(); ++it) {
                    if ((*it)->rxFreqMhz < here - eps) { pick = *it; break; }
                }
                if (!pick) pick = spots.back();
            }
            s->setFrequency(pick->rxFreqMhz);
            (*deck)->prefillCall(pick->callsign, qint64(std::llround(pick->rxFreqMhz * 1e6)));
        });
        if (decoder) {
            // AetherSDR's CW reader into the deck's pane, both text signals.
            QObject::connect(decoder, &CwRxModel::textDecoded, d,
                             [d](const QString& text, float) { d->appendRead(text); });
            QObject::connect(decoder, &CwRxModel::unscoredTextDecoded, d,
                             &OrionCwDeck::appendRead);
            QObject::connect(decoder, &CwRxModel::statsUpdated, d,
                             [d](float pitch, float speed) { d->setRxStats(pitch, speed); });
        }
        // The dial and mode, once a second while the deck is up.
        auto* feed = new QTimer(d);
        feed->setInterval(1000);
        QObject::connect(feed, &QTimer::timeout, d, [=] {
            if ((*deck)->isVisible()) (*deck)->setRig(rigHz(), rigMode());
        });
        feed->start();

        // The slot: a bottom dock of the main window, no title bar, not
        // closable by itself — the menu item is the switch.
        auto* dock = new QDockWidget(window);
        dock->setObjectName(QStringLiteral("orionCwDeckDock"));
        dock->setTitleBarWidget(new QWidget(dock));
        dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        dock->setAllowedAreas(Qt::BottomDockWidgetArea);
        dock->setWidget(d);
        d->setMinimumHeight(236);
        window->addDockWidget(Qt::BottomDockWidgetArea, dock);
        g_deckDock = dock;
    };

    QObject::connect(act, &QAction::toggled, window, [=](bool on) {
        if (on) {
            build();
            (*deck)->setRig(rigHz(), rigMode());
            g_deckDock->show();
            (*deck)->setFocus();
        } else if (g_deckDock) {
            g_deckDock->hide();
        }
    });
}

bool openOrionCwWindow()
{
    if (!g_open) {
        return false;
    }
    g_open();
    return true;
}

}  // namespace AetherSDR
