// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// N8EM fork: MASTER.SCP, the contest super-check-partial call list, as the
// console loaded it: a named file, else cqrlog's copy, else the system one.
// Shared by the CW skimmer (call confidence) and the contest deck (super
// check and the practice caller pool).

#include <QSet>
#include <QString>

namespace AetherSDR::orion {

QSet<QString> loadMasterScp(const QString& preferred = QString());

}  // namespace AetherSDR::orion
