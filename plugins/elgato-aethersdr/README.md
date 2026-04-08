# AetherSDR Elgato Stream Deck Plugin

Controls FlexRadio via AetherSDR's TCI WebSocket server. 43 actions for TX, bands, modes, DSP, audio, and DVK.

Works with the **official Elgato Stream Deck app** on macOS and Windows.

## Prerequisites

- AetherSDR v0.8.6+ with TCI server enabled
- Elgato Stream Deck app v6.6+
- Node.js 20+ (bundled by Elgato SDK)

## Building

```bash
cd plugins/elgato-aethersdr
npm install
npm run build
```

The built plugin is output to `com.aethersdr.radio.sdPlugin/`.

## Installing

```bash
npm run package
```

This creates `aethersdr.streamDeckPlugin`. Double-click it to install in the Stream Deck app.

## Development

```bash
npm run watch
```

This rebuilds on file changes and hot-reloads in the Stream Deck app.

## Configuration

The plugin connects to AetherSDR's TCI server at `ws://localhost:40001` by default.
Ensure TCI is enabled: **Settings > Autostart TCI with AetherSDR**.

## Actions

### TX Control
- **PTT** — Hold to transmit
- **MOX Toggle** — Toggle transmit
- **TUNE Toggle** — Toggle tune carrier
- **RF Power** — Cycle power levels
- **Tune Power** — Cycle tune power

### Frequency / Band
- **Band 160m–6m** — Direct band select (11 bands)
- **Band Up / Down** — Step through bands
- **Tune Up / Down** — Nudge frequency

### Mode
- USB, LSB, CW, AM, FM, DIGU, DIGL, FT8

### Audio
- **Mute Toggle**, **Volume Up**, **Volume Down**

### DSP
- **NB**, **NR**, **ANF**, **APF**, **SQL** toggles

### Slice
- **Split**, **Lock**, **RIT**, **XIT** toggles

### DVK
- **Play**, **Record**
