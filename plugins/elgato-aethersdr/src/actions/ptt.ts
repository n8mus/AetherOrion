import { action, type KeyDownEvent, type KeyUpEvent, type WillAppearEvent } from "@elgato/streamdeck";
import { TciAction } from "./tci-action.js";

@action({ UUID: "com.aethersdr.radio.ptt" })
export class PTTAction extends TciAction {
	override async onKeyDown(ev: KeyDownEvent): Promise<void> {
		this.send("trx:0,true;");
	}

	override async onKeyUp(ev: KeyUpEvent): Promise<void> {
		this.send("trx:0,false;");
	}

	updateDisplay(ev?: WillAppearEvent): void {
		ev?.action.setTitle(this.state.transmitting ? "TX" : "PTT");
	}
}
