#pragma once

// N8EM fork: the station logbook's screens (step 9, stage 1), in AetherSDR's
// own widgets: a "Logbook" menu (Log QSO..., ADIF import/export, the cqrlog
// switches) and the Log QSO window. The log itself is the Orion console's,
// ported (core/backends/orion/log/). See FORK-NOTES.md.

#include <functional>

#include <QColor>
#include <QString>

class QMainWindow;
class QMenuBar;

namespace AetherSDR {

namespace orion::logbook {
class OrionLogbook;
class QsoContext;
}

class RadioModel;
class SliceModel;

// Creates the logbook (opening its log and starting the cqrlog pull) and
// adds the Logbook menu to `bar`. `activeSlice` supplies the frequency and
// mode the QSO window fills in.
// `spot` sends a call to the DX cluster the host is connected to, and
// `showOnMap` puts the station on AetherSDR's map; both are the host's to
// own, so the Log QSO window only says what it wants.
void installOrionLogbook(QMainWindow* window, QMenuBar* bar, RadioModel* model,
                         std::function<SliceModel*()> activeSlice,
                         std::function<void(const QString&, double, const QString&)> spot = {},
                         std::function<void(double, double, const QString&)> showOnMap = {});

// The station being worked right now, shared by the Log QSO window and the
// CW window (core/backends/orion/log/QsoContext.h). Null until
// installOrionLogbook() has run.
orion::logbook::QsoContext* orionQsoContext();

// Worked-before colour for a spot, as the console colours them: never
// worked red, worked on another band amber, worked this band green,
// confirmed grey. Invalid when the logbook has no data, the call is
// unknown, or the operator turned the colours off.
QColor orionWorkedSpotColour(const QString& call, double freqMhz);

// The station logbook itself (worked-before, the country, cty.dat). Null
// until installOrionLogbook() has run.
orion::logbook::OrionLogbook* orionLogbook();

// LOG from the CW side (the CW window's LOG, the CW deck's LOG IT): the
// shared QsoContext IS the QSO. Call and the two reports are the
// operator's; `hz`, `adifMode` and UTC are the station's now; country and
// zones from cty.dat; whatever the callbook knows (name, QTH, grid) rides
// along from the record. The NAME box's word is the HANDLE, and stands in
// for NAME until the callbook speaks. The same call on the same band and
// mode within five minutes is the same QSO, not a second: refused. On
// success the record is cleared (every window on it empties) and the
// logbook's own fan-out runs (cqrlog mirror, uploads). Returns the line
// for the status bar; `ok` says whether a QSO was written.
QString orionLogFromContext(qint64 hz, const QString& adifMode, bool* ok = nullptr);

}  // namespace AetherSDR
