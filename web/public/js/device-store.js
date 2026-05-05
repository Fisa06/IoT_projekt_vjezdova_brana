// Keeps the devices shown in the dashboard.
import { mqtt } from './mqtt-client.js';
import { logger } from './logger.js';

const STORAGE_KEY = 'gate.manualDevices';
const NAMES_STORAGE_KEY = 'gate.deviceNames';
const MAX_EVENTS = 100;

class DeviceStore {
    constructor() {
        this.map = new Map();
        this.listeners = new Set();
    }

    init() {
        try { this.names = JSON.parse(localStorage.getItem(NAMES_STORAGE_KEY) || '{}'); }
        catch { this.names = {}; }

        try {
            const ids = JSON.parse(localStorage.getItem(STORAGE_KEY) || '[]');
            ids.forEach(id => this._ensure(id, { manual: true }));
        } catch { /* ignore broken localStorage */ }

        mqtt.onMessage((msg) => this._handleMessage(msg));
    }

    list() {
        return Array.from(this.map.values()).sort((a, b) =>
            this.displayName(a).localeCompare(this.displayName(b))
        );
    }

    get(nodeId) {
        return this.map.get(nodeId) || null;
    }

    displayName(devOrId) {
        const id = typeof devOrId === 'string' ? devOrId : devOrId.nodeId;
        return this.names[id] || id;
    }

    rename(nodeId, name) {
        name = String(name || '').trim();
        if (name) this.names[nodeId] = name;
        else      delete this.names[nodeId];
        localStorage.setItem(NAMES_STORAGE_KEY, JSON.stringify(this.names));
        this._notify();
    }

    addManual(nodeId) {
        nodeId = nodeId.trim();
        if (!nodeId) return false;
        const dev = this._ensure(nodeId, { manual: true });
        dev.manual = true;
        this._persistManual();
        this._notify();
        logger.log(`Added device: ${nodeId}`);
        return true;
    }

    remove(nodeId) {
        if (!this.map.delete(nodeId)) return;
        this._persistManual();
        this._notify();
    }

    subscribe(fn) {
        this.listeners.add(fn);
        return () => this.listeners.delete(fn);
    }

    _ensure(nodeId, init = {}) {
        let dev = this.map.get(nodeId);
        if (!dev) {
            dev = {
                nodeId,
                manual: false,
                state: null,
                gateStatus: null,
                info: null,
                lastSeen: null,
                events: [],
                ...init,
            };
            this.map.set(nodeId, dev);
        }
        return dev;
    }

    _handleMessage(msg) {
        if (!msg.nodeId) return;
        const dev = this._ensure(msg.nodeId);
        dev.lastSeen = Date.now();

        if (msg.kind === 'deviceInfo' && msg.json) {
            dev.info = msg.json;
            if (msg.json.gate_state) dev.state = msg.json.gate_state;
        } else if (msg.kind === 'gateStatus' && msg.json) {
            const newState = msg.json.state;
            const oldFault = dev.gateStatus?.fault || 'none';
            const newFault = msg.json.fault || 'none';
            if (newState && newState !== dev.state) {
                this._pushEvent(dev, `state: ${newState}`);
            }
            if (newFault !== oldFault && newFault !== 'none') {
                this._pushEvent(dev, `fault: ${newFault}${msg.json.message ? ` - ${msg.json.message}` : ''}`);
            }
            dev.state = newState ?? dev.state;
            dev.gateStatus = msg.json;
        } else if (msg.kind === 'reply' && msg.json) {
            this._pushEvent(dev, `reply ${msg.json.status || ''} ${msg.json.message || ''}`.trim());
        }

        this._notify();
    }

    _pushEvent(dev, text) {
        dev.events.unshift({ ts: Date.now(), text });
        if (dev.events.length > MAX_EVENTS) dev.events.length = MAX_EVENTS;
        logger.log(`${dev.nodeId}: ${text}`);
    }

    _persistManual() {
        const ids = Array.from(this.map.values()).filter(d => d.manual).map(d => d.nodeId);
        localStorage.setItem(STORAGE_KEY, JSON.stringify(ids));
    }

    _notify() {
        this.listeners.forEach(fn => fn());
    }
}

export const devices = new DeviceStore();
