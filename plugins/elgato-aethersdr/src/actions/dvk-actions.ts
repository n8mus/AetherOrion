import { action, type KeyDownEvent, type WillAppearEvent } from "@elgato/streamdeck";
import { TciAction } from "./tci-action.js";

@action({ UUID: "com.aethersdr.radio.dvk-play" })
export class DVKPlayAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send("rx_play:0,true;");
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle("PLAY");
	}
}

@action({ UUID: "com.aethersdr.radio.dvk-record" })
export class DVKRecordAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send("rx_record:0,true;");
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle("REC");
	}
}
