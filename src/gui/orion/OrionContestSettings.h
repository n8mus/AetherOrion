// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// N8EM fork: the contest screens' settings, the console's QSettings keys
// mapped onto AetherSDR's AppSettings. The console spelled them
// "contest/x", "station/callsign", "station/grid"...; here the contest's
// own live under OrionContest/, and the station's are AetherSDR's (the
// callsign) or the fork's CW page (the grid). Geometry is stored as
// base64 text, since AppSettings keeps strings.

#include <QByteArray>
#include <QString>
#include <QVariant>

#include "core/AppSettings.h"

namespace AetherSDR::orion::contest {

inline QString csKey(const QString& key)
{
    if (key == QLatin1String("station/callsign")) return QStringLiteral("StationCallsign");
    if (key == QLatin1String("station/grid")) return QStringLiteral("OrionCw/myGrid");
    if (key == QLatin1String("station/email")) return QStringLiteral("OrionContest/email");
    if (key == QLatin1String("club/email")) return QStringLiteral("OrionContest/clubEmail");
    if (key == QLatin1String("cw/wpm")) return QStringLiteral("OrionContest/wpm");
    if (key.startsWith(QLatin1String("contest/"))) {
        return QStringLiteral("OrionContest/") + key.mid(8);
    }
    return QStringLiteral("OrionContest/") + key;
}

inline QVariant csValue(const QString& key, const QVariant& fallback = {})
{
    return AppSettings::instance().value(csKey(key), fallback);
}

inline void csSet(const QString& key, const QVariant& value)
{
    auto& s = AppSettings::instance();
    s.setValue(csKey(key), value);
    s.save();
}

inline void csRemove(const QString& key)
{
    auto& s = AppSettings::instance();
    s.remove(csKey(key));
    s.save();
}

inline QByteArray csGeom(const QString& key)
{
    return QByteArray::fromBase64(csValue(key).toString().toLatin1());
}

inline void csSetGeom(const QString& key, const QByteArray& geom)
{
    csSet(key, QString::fromLatin1(geom.toBase64()));
}

}  // namespace AetherSDR::orion::contest
