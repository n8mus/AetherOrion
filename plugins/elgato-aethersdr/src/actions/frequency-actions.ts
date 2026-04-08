import { action, type KeyDownEvent, type WillAppearEvent } from "@elgato/streamdeck";
import { TciAction } from "./tci-action.js";

const BANDS: Record<string, number> = {
	"160m": 1900000, "80m": 3800000, "60m": 5357000, "40m": 7200000,
	"30m": 10125000, "20m": 14225000, "17m": 18118000, "15m": 21300000,
	"12m": 24940000, "10m": 28400000, "6m": 50125000,
};

const BAND_ORDER = ["160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"];

function currentBandIndex(freq: number): number {
	// Find closest band
	let best = 0;
	let bestDist = Infinity;
	for (let i = 0; i < BAND_ORDER.length; i++) {
		const dist = Math.abs(freq - BANDS[BAND_ORDER[i]]);
		if (dist < bestDist) { bestDist = dist; best = i; }
	}
	return best;
}

function makeBandAction(band: string, freq: number) {
	class BandAction extends TciAction {
		override async onKeyDown(ev: KeyDownEvent): Promise<void> {
			this.send(`vfo:0,0,${freq};`);
		}

		updateDisplay(ev?: WillAppearEvent): void {
			ev?.action.setTitle(band);
		}
	}
	return BandAction;
}

@action({ UUID: "com.aethersdr.radio.band-160m" })
export class Band160m extends makeBandAction("160m", 1900000) {}
@action({ UUID: "com.aethersdr.radio.band-80m" })
export class Band80m extends makeBandAction("80m", 3800000) {}
@action({ UUID: "com.aethersdr.radio.band-60m" })
export class Band60m extends makeBandAction("60m", 5357000) {}
@action({ UUID: "com.aethersdr.radio.band-40m" })
export class Band40m extends makeBandAction("40m", 7200000) {}
@action({ UUID: "com.aethersdr.radio.band-30m" })
export class Band30m extends makeBandAction("30m", 10125000) {}
@action({ UUID: "com.aethersdr.radio.band-20m" })
export class Band20m extends makeBandAction("20m", 14225000) {}
@action({ UUID: "com.aethersdr.radio.band-17m" })
export class Band17m extends makeBandAction("17m", 18118000) {}
@action({ UUID: "com.aethersdr.radio.band-15m" })
export class Band15m extends makeBandAction("15m", 21300000) {}
@action({ UUID: "com.aethersdr.radio.band-12m" })
export class Band12m extends makeBandAction("12m", 24940000) {}
@action({ UUID: "com.aethersdr.radio.band-10m" })
export class Band10m extends makeBandAction("10m", 28400000) {}
@action({ UUID: "com.aethersdr.radio.band-6m" })
export class Band6m extends makeBandAction("6m", 50125000) {}

@action({ UUID: "com.aethersdr.radio.band-up" })
export class BandUpAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		const idx = currentBandIndex(this.state.frequency);
		const next = Math.min(idx + 1, BAND_ORDER.length - 1);
		this.send(`vfo:0,0,${BANDS[BAND_ORDER[next]]};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle("Band\u25B2");
	}
}

@action({ UUID: "com.aethersdr.radio.band-down" })
export class BandDownAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		const idx = currentBandIndex(this.state.frequency);
		const next = Math.max(idx - 1, 0);
		this.send(`vfo:0,0,${BANDS[BAND_ORDER[next]]};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle("Band\u25BC");
	}
}

@action({ UUID: "com.aethersdr.radio.tune-up" })
export class TuneUpAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send(`vfo:0,0,${this.state.frequency + 100};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle("Tune\u25B2");
	}
}

@action({ UUID: "com.aethersdr.radio.tune-down" })
export class TuneDownAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send(`vfo:0,0,${this.state.frequency - 100};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle("Tune\u25BC");
	}
}
