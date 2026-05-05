// Small wrapper around mqtt.js, loaded from CDN.
import mqttLib from 'https://esm.sh/mqtt@5.10.1/dist/mqtt.esm.js';
import { settings } from './settings.js';

const TOPIC_PATTERNS = {
    deviceInfo: /^gate\/([^/]+)\/device_info$/,
    gateStatus: /^gate\/([^/]+)\/gate_status$/,
    reply:      /^gate\/([^/]+)\/reply$/,
};

class MqttClient {
    constructor() {
        this.client = null;
        this.callbacks = {};
        this.handlers = new Set();
    }

    init(callbacks) {
        this.callbacks = callbacks || {};
        this._connect();
    }

    reconnect() {
        if (this.client) {
            try { this.client.end(true); } catch { /* ignore */ }
            this.client = null;
        }
        this._connect();
    }

    _connect() {
        const s = settings.get();
        if (!s.brokerHost) {
            this.callbacks.onStatus?.('disconnected');
            this._info('Broker is not configured. Open Settings first.');
            return;
        }
        const url = settings.brokerUrl();
        this.callbacks.onStatus?.('connecting');
        this._info(`MQTT connecting to ${url}`);

        const opts = {
            clean: true,
            connectTimeout: 8000,
            reconnectPeriod: 4000,
        };
        if (s.username) opts.username = s.username;
        if (s.password) opts.password = s.password;

        this.client = mqttLib.connect(url, opts);

        this.client.on('connect', () => {
            this.callbacks.onStatus?.('connected');
            this._info('MQTT connected');
            this.client.subscribe('gate/+/device_info', { qos: 1 });
            this.client.subscribe('gate/+/gate_status', { qos: 1 });
            this.client.subscribe('gate/+/reply',       { qos: 1 });
        });

        this.client.on('reconnect', () => this.callbacks.onStatus?.('connecting'));
        this.client.on('close',     () => this.callbacks.onStatus?.('disconnected'));
        this.client.on('error', (err) => {
            this.callbacks.onStatus?.('error');
            this._info(`MQTT error: ${err.message}`);
        });

        this.client.on('message', (topic, payload) => {
            const text = payload.toString();
            let parsed = null;
            try { parsed = JSON.parse(text); } catch { /* ignore */ }
            const msg = { topic, raw: text, json: parsed, ...this._classify(topic) };
            this.handlers.forEach(h => h(msg));
        });
    }

    _classify(topic) {
        for (const [kind, re] of Object.entries(TOPIC_PATTERNS)) {
            const m = topic.match(re);
            if (m) return { kind, nodeId: m[1] };
        }
        return { kind: 'unknown', nodeId: null };
    }

    onMessage(handler) {
        this.handlers.add(handler);
        return () => this.handlers.delete(handler);
    }

    sendCommand(nodeId, command) {
        if (!this.client) {
            this._info('MQTT client is not initialized');
            return false;
        }
        if (!this.client.connected) {
            this._info('MQTT is not connected, command not sent');
            return false;
        }
        const id = `web-${Date.now()}`;
        const topic = `gate/${nodeId}/cmd`;
        const payload = JSON.stringify({ id, command });
        this._debug(`-> ${topic} ${payload}`);
        this.client.publish(topic, payload, { qos: 1 }, (err) => {
            if (err) {
                this._info(`Command not sent: ${err.message || err}`);
            } else {
                this._debug(`PUBLISH ok (id=${id})`);
            }
        });
        return true;
    }

    rescan() {
        if (!this.client || !this.client.connected) {
            this._info('Rescan skipped: MQTT is not connected');
            return false;
        }
        const topics = ['gate/+/device_info', 'gate/+/gate_status', 'gate/+/reply'];
        this._info('Rescanning devices...');
        this.client.unsubscribe(topics, () => {
            this.client.subscribe(topics, { qos: 1 }, (err) => {
                if (err) this._info(`Rescan error: ${err.message || err}`);
                else     this._debug('Rescan finished');
            });
        });
        return true;
    }

    _info(line) {
        this.callbacks.onLog?.(line);
    }

    _debug(line) {
        if (settings.get().devMode) this.callbacks.onLog?.(line);
    }
}

export const mqtt = new MqttClient();
