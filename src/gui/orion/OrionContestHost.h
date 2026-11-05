// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// N8EM fork: what the contest deck needs from the rest of the station,
// in the shapes the console's deck was written against (its RotorLink and
// QrzLookup), so the deck ports with its wiring intact. The host builds
// one of each over AetherSDR's own pieces: the rotor tile on the applet
// panel, and CallsignLookupService. See FORK-NOTES.md.

#include <QObject>
#include <QPointer>
#include <QString>

namespace AetherSDR {

class OrionRotorApplet;
struct CallsignInfo;

namespace orion::contest {

// The rotor, as the deck sees it: where the beam is, where it was last
// told to go, and the one way to turn it.
class OrionContestRotor : public QObject {
    Q_OBJECT
public:
    explicit OrionContestRotor(OrionRotorApplet* applet, QObject* parent = nullptr);
    [[nodiscard]] bool connected() const;
    [[nodiscard]] double azimuth() const;   // -1 unknown
    [[nodiscard]] double target() const;    // -1 none asked
    void turnTo(double azDeg);

signals:
    void azimuthChanged(double azDeg);
    void connectedChanged(bool on);

private:
    QPointer<OrionRotorApplet> m_applet;
};

// The callbook, as the deck sees it: ask by call, hear name/QTH/grid and
// the station's zones. Over AetherSDR's CallsignLookupService (QRZ, with
// its cache); silent when no callbook login is set.
class OrionContestCallbook : public QObject {
    Q_OBJECT
public:
    explicit OrionContestCallbook(QObject* parent = nullptr);
    void lookup(const QString& call);

signals:
    void result(const QString& call, bool ok, const QString& name, const QString& qth,
                const QString& grid, const QString& err);
    void zones(const QString& call, int cqz, int ituz);

private:
    void onInfo(const CallsignInfo& info);
};

}  // namespace orion::contest
}  // namespace AetherSDR
