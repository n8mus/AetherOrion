import { action, type KeyDownEvent, type WillAppearEvent } from "@elgato/streamdeck";
import { TciAction } from "./tci-action.js";

function makeDspToggle(uuid: string, label: string, tciCmd: string, stateKey: keyof import("../tci-client.js").RadioState) {
	class DspToggle extends TciAction {
		override async onKeyDown(ev: KeyDownEvent): Promise<void> {
			const current = this.state[stateKey] as boolean;
			this.send(`${tciCmd}:0,${!current};`);
		}

		updateDisplay(ev?: WillAppearEvent): void {
			const on = this.state[stateKey] as boolean;
			ev?.action.setTitle(on ? `[${label}]` : label);
		}
	}
	return DspToggle;
}

@action({ UUID: "com.aethersdr.radio.nb-toggle" })
export class NBToggle extends makeDspToggle("nb", "NB", "rx_nb", "nbOn") {}

@action({ UUID: "com.aethersdr.radio.nr-toggle" })
export class NRToggle extends makeDspToggle("nr", "NR", "rx_nr", "nrOn") {}

@action({ UUID: "com.aethersdr.radio.anf-toggle" })
export class ANFToggle extends makeDspToggle("anf", "ANF", "rx_anf", "anfOn") {}

@action({ UUID: "com.aethersdr.radio.apf-toggle" })
export class APFToggle extends makeDspToggle("apf", "APF", "rx_apf", "apfOn") {}

@action({ UUID: "com.aethersdr.radio.sql-toggle" })
export class SQLToggle extends makeDspToggle("sql", "SQL", "sql_enable", "sqlOn") {}
