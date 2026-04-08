/**
 * TCI WebSocket client for AetherSDR.
 *
 * Connects to AetherSDR's TCI server (default ws://localhost:40001) and
 * provides typed send/receive for radio control. Auto-reconnects on
 * disconnect with exponential backoff.
 */

import WebSocket from "ws";
import { EventEmitter } from "events";

export interface RadioState {
	frequency: number;        // Hz
	mode: string;             // USB, LSB, CW, etc.
	transmitting: boolean;
	mox: boolean;
	tuning: boolean;
	muted: boolean;
	volume: number;           // 0–100
	rfPower: number;          // 0–100
	tunePower: number;        // 0–100
	nbOn: boolean;
	nrOn: boolean;
	anfOn: boolean;
	apfOn: boolean;
	sqlOn: boolean;
	sqlLevel: number;
	split: boolean;
	locked: boolean;
	ritOn: boolean;
	xitOn: boolean;
	ritFreq: number;
	xitFreq: number;
}

export class TciClient extends EventEmitter {
	private ws: WebSocket | null = null;
	private host: string;
	private port: number;
	private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
	private reconnectDelay = 3000;
	private connected = false;

	public state: RadioState = {
		frequency: 14225000,
		mode: "USB",
		transmitting: false,
		mox: false,
		tuning: false,
		muted: false,
		volume: 50,
		rfPower: 100,
		tunePower: 25,
		nbOn: false,
		nrOn: false,
		anfOn: false,
		apfOn: false,
		sqlOn: false,
		sqlLevel: 0,
		split: false,
		locked: false,
		ritOn: false,
		xitOn: false,
		ritFreq: 0,
		xitFreq: 0,
	};

	constructor(host = "localhost", port = 40001) {
		super();
		this.host = host;
		this.port = port;
	}

	connect(): void {
		if (this.ws) {
			this.ws.removeAllListeners();
			this.ws.close();
		}

		const url = `ws://${this.host}:${this.port}`;
		this.ws = new WebSocket(url);

		this.ws.on("open", () => {
			this.connected = true;
			this.reconnectDelay = 3000;
			this.emit("connected");
		});

		this.ws.on("message", (data: WebSocket.Data) => {
			const msg = data.toString();
			this.parseTciMessage(msg);
		});

		this.ws.on("close", () => {
			this.connected = false;
			this.emit("disconnected");
			this.scheduleReconnect();
		});

		this.ws.on("error", () => {
			// Error is followed by close — reconnect handled there
		});
	}

	disconnect(): void {
		if (this.reconnectTimer) {
			clearTimeout(this.reconnectTimer);
			this.reconnectTimer = null;
		}
		if (this.ws) {
			this.ws.removeAllListeners();
			this.ws.close();
			this.ws = null;
		}
		this.connected = false;
	}

	isConnected(): boolean {
		return this.connected;
	}

	send(cmd: string): void {
		if (this.ws && this.ws.readyState === WebSocket.OPEN) {
			this.ws.send(cmd);
		}
	}

	setHost(host: string, port: number): void {
		this.host = host;
		this.port = port;
		if (this.connected) {
			this.disconnect();
			this.connect();
		}
	}

	private scheduleReconnect(): void {
		if (this.reconnectTimer) { return; }
		this.reconnectTimer = setTimeout(() => {
			this.reconnectTimer = null;
			this.connect();
		}, this.reconnectDelay);
		this.reconnectDelay = Math.min(this.reconnectDelay * 1.5, 30000);
	}

	private parseTciMessage(msg: string): void {
		// TCI messages: "command:param1,param2,...;\n"
		for (const line of msg.split("\n")) {
			const trimmed = line.trim().replace(/;$/, "");
			if (!trimmed) { continue; }

			const colonIdx = trimmed.indexOf(":");
			if (colonIdx < 0) { continue; }

			const cmd = trimmed.substring(0, colonIdx).toLowerCase();
			const params = trimmed.substring(colonIdx + 1).split(",");

			switch (cmd) {
				case "vfo":
					if (params.length >= 3) {
						this.state.frequency = parseInt(params[2], 10);
					}
					break;
				case "modulation":
					if (params.length >= 2) {
						this.state.mode = params[1];
					}
					break;
				case "trx":
					if (params.length >= 2) {
						this.state.transmitting = params[1] === "true";
					}
					break;
				case "mox":
					if (params.length >= 1) {
						this.state.mox = params[0] === "true";
					}
					break;
				case "tune":
					if (params.length >= 1) {
						this.state.tuning = params[0] === "true";
					}
					break;
				case "mute":
					if (params.length >= 1) {
						this.state.muted = params[0] === "true";
					}
					break;
				case "volume":
					if (params.length >= 1) {
						this.state.volume = parseInt(params[0], 10);
					}
					break;
				case "drive":
					if (params.length >= 1) {
						this.state.rfPower = parseInt(params[0], 10);
					}
					break;
				case "tune_drive":
					if (params.length >= 1) {
						this.state.tunePower = parseInt(params[0], 10);
					}
					break;
				case "rx_nb":
					if (params.length >= 2) {
						this.state.nbOn = params[1] === "true";
					}
					break;
				case "rx_nr":
					if (params.length >= 2) {
						this.state.nrOn = params[1] === "true";
					}
					break;
				case "rx_anf":
					if (params.length >= 2) {
						this.state.anfOn = params[1] === "true";
					}
					break;
				case "sql_enable":
					if (params.length >= 2) {
						this.state.sqlOn = params[1] === "true";
					}
					break;
				case "sql_level":
					if (params.length >= 2) {
						this.state.sqlLevel = parseInt(params[1], 10);
					}
					break;
				case "split_enable":
					if (params.length >= 2) {
						this.state.split = params[1] === "true";
					}
					break;
				case "lock":
					if (params.length >= 2) {
						this.state.locked = params[1] === "true";
					}
					break;
				case "rit_enable":
					if (params.length >= 2) {
						this.state.ritOn = params[1] === "true";
					}
					break;
				case "xit_enable":
					if (params.length >= 2) {
						this.state.xitOn = params[1] === "true";
					}
					break;
				case "rit_offset":
					if (params.length >= 2) {
						this.state.ritFreq = parseInt(params[1], 10);
					}
					break;
				case "xit_offset":
					if (params.length >= 2) {
						this.state.xitFreq = parseInt(params[1], 10);
					}
					break;
			}

			this.emit("stateChanged", this.state);
		}
	}
}
