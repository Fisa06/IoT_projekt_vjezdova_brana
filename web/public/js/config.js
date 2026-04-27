// Loads runtime config from /config.json. Falls back to sensible defaults.
// In Docker the file is generated at container start from env vars.
const DEFAULTS = {
    brokerUrl: '',
    username:  '',
    password:  '',
};

export async function loadConfig() {
    try {
        const res = await fetch('/config.json', { cache: 'no-store' });
        if (!res.ok) throw new Error('no config.json');
        return { ...DEFAULTS, ...(await res.json()) };
    } catch {
        return { ...DEFAULTS };
    }
}
