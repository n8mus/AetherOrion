// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/backends/orion/MasterScp.h"

#include <QDir>
#include <QFile>
#include <QStringList>

namespace AetherSDR::orion {

QSet<QString> loadMasterScp(const QString& preferred)
{
    QSet<QString> out;
    QStringList paths;
    if (!preferred.isEmpty()) {
        paths << preferred;
    } else {
        paths << QDir::homePath() + QStringLiteral("/.config/cqrlog/MASTER.SCP")
              << QStringLiteral("/usr/share/cqrlog/ctyfiles/MASTER.SCP");
    }
    for (const QString& p : paths) {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        while (!f.atEnd()) {
            const QByteArray line = f.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#')) {
                continue;
            }
            out.insert(QString::fromLatin1(line).toUpper());
        }
        break;
    }
    return out;
}

}  // namespace AetherSDR::orion
