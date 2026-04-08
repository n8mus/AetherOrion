import { action, type KeyDownEvent, type WillAppearEvent } from "@elgato/streamdeck";
import { TciAction } from "./tci-action.js";

function makeModeAction(mode: string) {
	class ModeAction extends TciAction {
		override async onKeyDown(ev: KeyDownEvent): Promise<void> {
			this.send(`modulation:0,${mode};`);
		}

		updateDisplay(ev?: WillAppearEvent): void {
			const active = this.state.mode === mode;
			ev?.action.setTitle(active ? `[${mode}]` : mode);
		}
	}
	return ModeAction;
}

@action({ UUID: "com.aethersdr.radio.mode-usb" })
export class ModeUSB extends makeModeAction("USB") {}
@action({ UUID: "com.aethersdr.radio.mode-lsb" })
export class ModeLSB extends makeModeAction("LSB") {}
@action({ UUID: "com.aethersdr.radio.mode-cw" })
export class ModeCW extends makeModeAction("CW") {}
@action({ UUID: "com.aethersdr.radio.mode-am" })
export class ModeAM extends makeModeAction("AM") {}
@action({ UUID: "com.aethersdr.radio.mode-fm" })
export class ModeFM extends makeModeAction("FM") {}
@action({ UUID: "com.aethersdr.radio.mode-digu" })
export class ModeDIGU extends makeModeAction("DIGU") {}
@action({ UUID: "com.aethersdr.radio.mode-digl" })
export class ModeDIGL extends makeModeAction("DIGL") {}
@action({ UUID: "com.aethersdr.radio.mode-ft8" })
export class ModeFT8 extends makeModeAction("FT8") {}
