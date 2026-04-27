// Persistent user settings (localStorage), merged on top of /config.json.
// Reactive: subscribers are notified on save().
const STORAGE_KEY = 'gate.settings';

const DEFAULTS = {
    brokerHost: '',
    brokerPort: 8084,
    brokerPath: '/mqtt',
    brokerTls:  true,
    username:   '',
    password:   '',
    devMode:    false,
};

const listeners = new Set();
let current = { ...DEFAULTS };

export const settings = {
    /** Initialize from saved settings, falling back to values from config.json. */
    init(fromConfig = {}) {
        // Map a brokerUrl from config.json to host/port/path/tls if present.
        const seed = { ...DEFAULTS };
        if (fromConfig.brokerUrl) {
            try {
                const u = new URL(fromConfig.brokerUrl);
                seed.brokerTls  = u.protocol === 'wss:';
                seed.brokerHost = u.hostname;
                seed.brokerPort = u.port ? Number(u.port) : (seed.brokerTls ? 8084 : 8083);
                seed.brokerPath = u.pathname || '/mqtt';
            } catch { /* ignore */ }
        }
        if (fromConfig.username != null) seed.username = fromConfig.username;
        if (fromConfig.password != null) seed.password = fromConfig.password;

        // Saved values win (but only those keys actually set).
        let saved = {};
        try { saved = JSON.parse(localStorage.getItem(STORAGE_KEY) || '{}'); } catch { /* ignore */ }
        current = { ...seed, ...saved };
    },

    get() { return { ...current }; },

    save(patch) {
        current = { ...current, ...patch };
        localStorage.setItem(STORAGE_KEY, JSON.stringify(current));
        listeners.forEach(fn => fn(current));
    },

    /** Build a wss://host:port/path style URL from the current settings. */
    brokerUrl() {
        const proto = current.brokerTls ? 'wss' : 'ws';
        const path  = current.brokerPath.startsWith('/') ? current.brokerPath : '/' + current.brokerPath;
        return `${proto}://${current.brokerHost}:${current.brokerPort}${path}`;
    },

    subscribe(fn) {
        listeners.add(fn);
        return () => listeners.delete(fn);
    },
};
