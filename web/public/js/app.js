// Starts the dashboard and switches between the few simple views.
import { loadConfig } from './config.js';
import { settings } from './settings.js';
import { mqtt } from './mqtt-client.js';
import { devices } from './device-store.js';
import { logger } from './logger.js';
import { renderMenu } from './views/menu.js';
import { renderDevice } from './views/device.js';
import { renderLogs } from './views/logs.js';
import { renderSettings, applyDevModeClass } from './views/settings.js';

const viewEl = document.getElementById('view');

const routes = [
    { pattern: /^\/$/,                handler: () => renderMenu(viewEl) },
    { pattern: /^\/logs$/,            handler: () => renderLogs(viewEl) },
    { pattern: /^\/settings$/,        handler: () => renderSettings(viewEl) },
    { pattern: /^\/device\/([^/]+)$/, handler: (m) => renderDevice(viewEl, decodeURIComponent(m[1])) },
];

function router() {
    const hash = location.hash.replace(/^#/, '') || '/';
    for (const r of routes) {
        const m = hash.match(r.pattern);
        if (m) {
            highlightNav(hash);
            r.handler(m);
            return;
        }
    }
    viewEl.innerHTML = '<p class="empty">Page not found.</p>';
}

function highlightNav(hash) {
    document.querySelectorAll('#nav-menu a').forEach(a => {
        a.classList.toggle('active', a.dataset.route === hash || (hash === '/' && a.dataset.route === '/'));
    });
}

async function main() {
    const cfg = await loadConfig();
    settings.init(cfg);
    applyDevModeClass();
    devices.init();
    mqtt.init({
        onStatus: updateConnectionIndicator,
        onLog:    (line) => logger.log(line),
    });
    initMobileMenu();
    window.addEventListener('hashchange', router);
    router();
}

function initMobileMenu() {
    const toggle = document.getElementById('menu-toggle');
    const backdrop = document.getElementById('backdrop');
    const close = () => document.body.classList.remove('menu-open');
    toggle?.addEventListener('click', () => document.body.classList.toggle('menu-open'));
    backdrop?.addEventListener('click', close);
    document.getElementById('nav-menu')?.addEventListener('click', (e) => {
        if (e.target.tagName === 'A') {
            close();
        }
    });
    window.addEventListener('hashchange', close);
}

function updateConnectionIndicator(status) {
    const dot = document.getElementById('conn-dot');
    const text = document.getElementById('conn-text');
    dot.className = 'dot ' + (status === 'connected' ? 'on'
                            : status === 'connecting' ? 'warn'
                            : status === 'error'      ? 'err' : 'off');
    text.textContent = status === 'connected'  ? 'Connected'
                     : status === 'connecting' ? 'Connecting...'
                     : status === 'error'      ? 'Error'
                     :                           'Disconnected';
}

main();
