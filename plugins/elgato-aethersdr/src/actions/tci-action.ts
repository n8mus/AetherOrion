/**
 * Base class for TCI-backed Stream Deck actions.
 * Provides access to the shared TCI client and state update wiring.
 */

import { SingletonAction, type KeyDownEvent, type KeyUpEvent, type WillAppearEvent, type WillDisappearEvent } from "@elgato/streamdeck";
import { tciClient, type RadioState } from "../plugin.js";

export abstract class TciAction extends SingletonAction {
	protected contexts: Set<string> = new Set();

	override async onWillAppear(ev: WillAppearEvent): Promise<void> {
		this.contexts.add(ev.action.id);
		this.updateDisplay(ev);
	}

	override async onWillDisappear(ev: WillDisappearEvent): Promise<void> {
		this.contexts.delete(ev.action.id);
	}

	/** Called when radio state changes — override to update button title/image. */
	abstract updateDisplay(ev?: WillAppearEvent): void;

	protected send(cmd: string): void {
		tciClient.send(cmd);
	}

	protected get state(): RadioState {
		return tciClient.state;
	}

	protected formatFrequency(hz: number): string {
		const mhz = Math.floor(hz / 1000000);
		const khz = Math.floor((hz % 1000000) / 1000);
		const hzPart = hz % 1000;
		return `${mhz}.${String(khz).padStart(3, "0")}.${String(hzPart).padStart(3, "0")}`;
	}
}
