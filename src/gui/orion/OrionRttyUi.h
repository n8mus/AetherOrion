#pragma once

// N8EM fork: the RTTY deck's menu entry and wiring (RTTY stage 2). The deck
// is gui/orion/OrionRttyDeck (the console's, ported); the modem is
// core/backends/orion/rtty/RttyEngine. See FORK-NOTES.md.

#include <functional>

#include <QString>

class QMainWindow;
class QMenu;

namespace AetherSDR {

class AudioEngine;
class RadioModel;
class SliceModel;

// Adds "RTTY panel" (Ctrl+Shift+Y) to `menu`. The deck takes the bottom
// dock under the spectrum, the slot the CW and contest decks use (the
// three are exclusive). Receive: the modem reads the active slice's audio
// through the same decoder tap AetherSDR's own RTTY reader uses, with the
// sideband sense from the slice's mode. Transmit: an F-key or Enter keys
// the radio through the producer PTT API and the modulator's AFSK goes
// down the host's modem audio route (gui/orion/OrionRttyTx), through the SignaLink,
// whatever the mic selection. `activeSlice` is the dial and mode; `spot` sends a call to
// the cluster.
void installOrionRttyDeck(QMainWindow* window, QMenu* menu, RadioModel* model,
                          AudioEngine* audio, std::function<SliceModel*()> activeSlice,
                          std::function<void(const QString&, double, const QString&)> spot = {});

// Raise or lower the deck. False when installOrionRttyDeck() has not run.
bool setOrionRttyDeckVisible(bool on);

}  // namespace AetherSDR
