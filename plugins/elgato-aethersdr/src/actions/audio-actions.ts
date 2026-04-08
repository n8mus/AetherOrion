import { action, type KeyDownEvent, type WillAppearEvent } from "@elgato/streamdeck";
import { TciAction } from "./tci-action.js";

@action({ UUID: "com.aethersdr.radio.mute-toggle" })
export class MuteToggleAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send(`mute:${!this.state.muted};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(this.state.muted ? "MUTED" : "MUTE");
	}
}

@action({ UUID: "com.aethersdr.radio.volume-up" })
export class VolumeUpAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		const vol = Math.min(this.state.volume + 5, 100);
		this.send(`volume:${vol};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(`Vol ${this.state.volume}`);
	}
}

@action({ UUID: "com.aethersdr.radio.volume-down" })
export class VolumeDownAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		const vol = Math.max(this.state.volume - 5, 0);
		this.send(`volume:${vol};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(`Vol ${this.state.volume}`);
	}
}
