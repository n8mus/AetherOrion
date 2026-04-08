import { action, type KeyDownEvent, type WillAppearEvent } from "@elgato/streamdeck";
import { TciAction } from "./tci-action.js";

@action({ UUID: "com.aethersdr.radio.mox-toggle" })
export class MOXToggleAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send(`trx:0,${!this.state.transmitting};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(this.state.transmitting ? "MOX ON" : "MOX");
	}
}

@action({ UUID: "com.aethersdr.radio.tune-toggle" })
export class TUNEToggleAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send(`tune:0,${!this.state.tuning};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(this.state.tuning ? "TUNE ON" : "TUNE");
	}
}

@action({ UUID: "com.aethersdr.radio.rf-power" })
export class RFPowerAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		// Cycle through power levels: 5, 10, 25, 50, 75, 100
		const levels = [5, 10, 25, 50, 75, 100];
		const cur = this.state.rfPower;
		const next = levels.find(l => l > cur) ?? levels[0];
		this.send(`drive:${next};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(`${this.state.rfPower}W`);
	}
}

@action({ UUID: "com.aethersdr.radio.tune-power" })
export class TunePowerAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		const levels = [5, 10, 15, 25, 50];
		const cur = this.state.tunePower;
		const next = levels.find(l => l > cur) ?? levels[0];
		this.send(`tune_drive:${next};`);
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(`TUN ${this.state.tunePower}W`);
	}
}
