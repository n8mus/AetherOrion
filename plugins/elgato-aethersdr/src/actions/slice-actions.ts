import { action, type KeyDownEvent, type WillAppearEvent } from "@elgato/streamdeck";
import { TciAction } from "./tci-action.js";

@action({ UUID: "com.aethersdr.radio.split-toggle" })
export class SplitToggleAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send(`split_enable:0,${!this.state.split};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(this.state.split ? "[SPLIT]" : "SPLIT");
	}
}

@action({ UUID: "com.aethersdr.radio.lock-toggle" })
export class LockToggleAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send(`lock:0,${!this.state.locked};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(this.state.locked ? "[LOCK]" : "LOCK");
	}
}

@action({ UUID: "com.aethersdr.radio.rit-toggle" })
export class RITToggleAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send(`rit_enable:0,${!this.state.ritOn};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(this.state.ritOn ? "[RIT]" : "RIT");
	}
}

@action({ UUID: "com.aethersdr.radio.xit-toggle" })
export class XITToggleAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send(`xit_enable:0,${!this.state.xitOn};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(this.state.xitOn ? "[XIT]" : "XIT");
	}
}
