/**
 * AetherSDR Elgato Stream Deck Plugin
 *
 * Controls FlexRadio via AetherSDR's TCI WebSocket server.
 * 43 actions: TX, frequency/band, mode, audio, DSP, slice, DVK.
 */

import streamDeck from "@elgato/streamdeck";
import { TciClient, type RadioState } from "./tci-client.js";

// Shared TCI client — all actions use this single connection
export const tciClient = new TciClient();
export type { RadioState };

// Import all actions so decorators register them
import "./actions/ptt.js";
import "./actions/tx-actions.js";
import "./actions/frequency-actions.js";
import "./actions/mode-actions.js";
import "./actions/audio-actions.js";
import "./actions/dsp-actions.js";
import "./actions/slice-actions.js";
import "./actions/dvk-actions.js";

// Connect to TCI server on plugin start
tciClient.connect();

// Log connection state
tciClient.on("connected", () => {
	streamDeck.logger.info("Connected to AetherSDR TCI server");
});

tciClient.on("disconnected", () => {
	streamDeck.logger.info("Disconnected from AetherSDR TCI server — will reconnect");
});

// Connect to Stream Deck
streamDeck.connect();
