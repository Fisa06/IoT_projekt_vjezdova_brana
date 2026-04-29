// Device list view + add-device form.
import { devices } from '../device-store.js';
import { mqtt } from '../mqtt-client.js';

export function renderMenu(root) {
    root.innerHTML = `
        <h2 class="section-title">Zariadenia</h2>
        <form class="add-device" id="add-form">
            <input type="text" id="add-input" placeholder="node_id (napr. 6767)" required>
            <button type="submit">Pridať</button>
            <button type="button" class="secondary" id="rescan-btn">Vyhľadať</button>
        </form>
        <div id="device-grid" class="device-grid"></div>
    `;

    const grid = root.querySelector('#device-grid');

    const repaint = () => {
        const items = devices.list();
        if (items.length === 0) {
            grid.innerHTML = '<p class="empty">Žiadne zariadenia. Pridaj node_id alebo počkaj na auto-discovery cez MQTT.</p>';
            return;
        }
        grid.innerHTML = items.map(deviceCard).join('');
        grid.querySelectorAll('[data-node]').forEach(el => {
            el.addEventListener('click', () => {
                location.hash = `#/device/${encodeURIComponent(el.dataset.node)}`;
            });
        });
    };

    repaint();
    const unsub = devices.subscribe(repaint);

    // Detach when navigating away.
    window.addEventListener('hashchange', unsub, { once: true });

    root.querySelector('#add-form').addEventListener('submit', (e) => {
        e.preventDefault();
        const input = root.querySelector('#add-input');
        if (devices.addManual(input.value)) input.value = '';
    });

    root.querySelector('#rescan-btn').addEventListener('click', () => {
        mqtt.rescan();
    });
}

function deviceCard(d) {
    const state = d.state || 'unknown';
    const ip    = d.info?.ip   || '–';
    const wifi  = d.info?.wifi || 'unknown';
    const ssid  = d.info?.ssid || '';
    const fault = d.gateStatus?.fault || 'none';
    const faultClass = fault === 'none' ? 'ok' : 'err';
    const wifiOnline = wifi === 'connected';
    const wifiLabel  = wifiOnline
        ? (ssid ? `Wi-Fi: ${ssid}` : 'Online')
        : (wifi === 'disconnected' ? 'Offline' : 'Neznáme');
    const wifiClass  = wifiOnline ? 'on' : (wifi === 'disconnected' ? 'err' : 'off');
    const name  = devices.displayName(d);
    const showId = name !== d.nodeId;
    return `
        <div class="device-card" data-node="${escapeHtml(d.nodeId)}">
            <div class="name">${escapeHtml(name)}</div>
            ${showId ? `<div class="subname">${escapeHtml(d.nodeId)}</div>` : ''}
            <div class="meta">
                <span class="state-badge state-${escapeHtml(state)}">${escapeHtml(state)}</span>
                <span class="wifi-badge"><span class="dot ${wifiClass}"></span>${escapeHtml(wifiLabel)}</span>
                ${fault !== 'none' ? `<span class="fault-badge fault-${faultClass}">${escapeHtml(fault)}</span>` : ''}
                <span class="dev-only">ip: ${escapeHtml(ip)}</span>
            </div>
        </div>
    `;
}

function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, c => ({
        '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'
    }[c]));
}
