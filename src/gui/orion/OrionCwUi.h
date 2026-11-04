#pragma once

// N8EM fork: the CW window's menu entry and wiring (step 10). The window
// itself is gui/orion/OrionCwWindow.h. See FORK-NOTES.md.

#include <functional>

#include <QString>

class QMainWindow;
class QMenu;

namespace AetherSDR {

class CwRxModel;
class RadioModel;

// Adds "CW window..." to `menu` and wires it: text keying through
// RadioModel's CwxModel, decoded CW from `decoder`, the station fields from
// AetherSDR's own station settings, and the shared current-QSO record from
// the logbook (so the Log QSO window and this one are about one station).
// `selectDecoder` asks the HOST to change CW readers: switching the model
// directly stops the old backend without starting the new one and leaves
// the host's own decode lifecycle (audio routing, the panels' state) out of
// step — the reader goes silent whichever one you pick.
void installOrionCwWindow(QMainWindow* window, QMenu* menu, RadioModel* model,
                          CwRxModel* decoder, std::function<void()> zeroBeat = {},
                          std::function<void(const QString&)> selectDecoder = {});

// Shows the CW window (creating it on first use). False when
// installOrionCwWindow() has not run, so the caller can fall back.
bool openOrionCwWindow();

class SliceModel;

// The CW deck (gui/orion/OrionCwDeck.h): a strip under the spectrum, in a
// bottom dock of the main window, raised by Tools -> CW panel (Ctrl+Shift+P).
// Call after installOrionCwWindow(): the deck is a second front on that
// window's keyer. `activeSlice` is the dial and mode the deck logs at and
// walks spots on; `spot` sends a call to the cluster; `zeroBeat` is the
// host's own, for the 0-BEAT button.
void installOrionCwDeck(QMainWindow* window, QMenu* menu, RadioModel* model,
                        CwRxModel* decoder, std::function<SliceModel*()> activeSlice,
                        std::function<void(const QString&, double, const QString&)> spot = {},
                        std::function<void()> zeroBeat = {});

// Raise or lower the deck. False when installOrionCwDeck() has not run.
bool setOrionCwDeckVisible(bool on);

// True while the operator wants the fork's CW window instead of AetherSDR's
// CWX panel (fork rule 2's CW exception; setting OrionCw/replaceCwx,
// default on). MainWindow asks before it builds the CWX panel.
bool orionCwReplacesCwxPanel();

}  // namespace AetherSDR
