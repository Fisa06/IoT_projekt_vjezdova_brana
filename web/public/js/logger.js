// Global log buffer + pub/sub. Views render from the buffer.
const MAX_LINES = 500;
const buffer = [];        // newest first
const listeners = new Set();

export const logger = {
    log(line) {
        const entry = { ts: Date.now(), text: String(line) };
        buffer.unshift(entry);
        if (buffer.length > MAX_LINES) buffer.length = MAX_LINES;
        listeners.forEach(fn => fn(entry));
    },

    list() { return buffer.slice(); },

    clear() {
        buffer.length = 0;
        listeners.forEach(fn => fn(null));
    },

    subscribe(fn) {
        listeners.add(fn);
        return () => listeners.delete(fn);
    },
};
