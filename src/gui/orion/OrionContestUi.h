#pragma once

// N8EM fork: the contest deck's menu entries and wiring (contest stage 2b).
// The deck is gui/orion/OrionContestDeck (the console's, ported); the
// manager window gui/orion/OrionContestWindow. See FORK-NOTES.md.

#include <functional>

#include <QColor>
#include <QString>

class QMainWindow;
class QMenu;

namespace AetherSDR {

class CwRxModel;
class RadioModel;
class SliceModel;

// Adds "Contest panel" (Ctrl+Shift+C) and "Contest log..." to `menu`.
// The deck takes the bottom dock under the spectrum, the slot the CW deck
// uses (the two are exclusive), keys through the CW window, reads the
// station's CW reader, and logs to the contest's own database; the
// manager window starts and resumes contests, edits the log and writes
// the Cabrillo. `activeSlice` is the dial and mode; `spot` sends a call
// to the cluster. Call after installOrionCwWindow().
void installOrionContest(QMainWindow* window, QMenu* menu, RadioModel* model,
                         CwRxModel* decoder, std::function<SliceModel*()> activeSlice,
                         std::function<void(const QString&, double, const QString&)> spot = {});

// Raise or lower the deck. False when installOrionContest() has not run.
bool setOrionContestDeckVisible(bool on);

// The running contest's colour for a spot: new mult red, workable blue,
// dupe grey, zero points dark. Invalid when no contest is open.
QColor orionContestSpotColour(const QString& call, double freqMhz);

}  // namespace AetherSDR
