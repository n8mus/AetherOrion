// SPDX-License-Identifier: GPL-2.0-or-later
#include "gui/orion/OrionContestHost.h"

#include "core/CallsignInfo.h"
#include "core/CallsignLookupService.h"
#include "gui/orion/OrionRotorApplet.h"

namespace AetherSDR::orion::contest {

OrionContestRotor::OrionContestRotor(OrionRotorApplet* applet, QObject* parent)
    : QObject(parent)
    , m_applet(applet)
{
    if (m_applet) {
        connect(m_applet, &OrionRotorApplet::azimuthReported, this,
                &OrionContestRotor::azimuthChanged);
    }
}

bool OrionContestRotor::connected() const
{
    return m_applet && m_applet->hasRadio();
}

double OrionContestRotor::azimuth() const
{
    return m_applet ? m_applet->reportedAzimuth() : -1.0;
}

double OrionContestRotor::target() const
{
    return m_applet ? m_applet->askedBearing() : -1.0;
}

void OrionContestRotor::turnTo(double azDeg)
{
    if (m_applet) {
        m_applet->turnTo(azDeg);
    }
}

OrionContestCallbook::OrionContestCallbook(QObject* parent)
    : QObject(parent)
{
    auto& svc = CallsignLookupService::instance();
    connect(&svc, &CallsignLookupService::infoReady, this,
            [this](const CallsignInfo& info, bool) { onInfo(info); });
    connect(&svc, &CallsignLookupService::lookupFailed, this,
            [this](const QString& call, const QString& message) {
                emit result(call.trimmed().toUpper(), false, QString(), QString(), QString(),
                            message);
            });
}

void OrionContestCallbook::lookup(const QString& call)
{
    const QString c = call.trimmed().toUpper();
    if (c.size() < 3) return;
    auto& svc = CallsignLookupService::instance();
    if (!svc.enabled() || (!svc.hasCredentials() && !svc.hasCachedEntry(c))) {
        emit result(c, false, QString(), QString(), QString(),
                    QStringLiteral("no callbook login"));
        return;
    }
    svc.lookup(c);
}

void OrionContestCallbook::onInfo(const CallsignInfo& info)
{
    const QString c = info.call.trimmed().toUpper();
    if (c.isEmpty()) return;
    QString name = info.nameFmt.trimmed();
    if (name.isEmpty()) {
        name = (info.firstName + QLatin1Char(' ') + info.lastName).trimmed();
    }
    QString qth = info.city.trimmed();
    if (!info.state.trimmed().isEmpty()) {
        qth = qth.isEmpty() ? info.state.trimmed() : qth + QStringLiteral(", ") + info.state.trimmed();
    }
    emit result(c, true, name, qth, info.grid.trimmed().toUpper(), QString());
    if (info.cqZone > 0) {
        emit zones(c, info.cqZone, 0);
    }
}

}  // namespace AetherSDR::orion::contest
