// Detail page for one gate unit.
import { devices } from '../device-store.js';
import { mqtt } from '../mqtt-client.js';

export function renderDevice(root, nodeId) {
    const name = devices.displayName(nodeId);
    root.innerHTML = `
        <h2 class="section-title">
            <a href="#/" style="color:#9aa0ab; text-decoration:none;">&larr; Devices</a>
            &nbsp;/&nbsp; <span id="title-name">${escapeHtml(name)}</span>
        </h2>
        <div class="device-detail">
            <div class="panel" id="status-panel"></div>
            <div class="panel">
                <h3>Controls</h3>
                <div class="controls">
                    <button data-cmd="open">Open</button>
                    <button data-cmd="stop" class="secondary">Stop</button>
                    <button data-cmd="close" class="danger">Close</button>
                </div>
                <h3 style="margin-top:18px;">Name</h3>
                <form class="rename-form" id="rename-form">
                    <input type="text" id="name-input" placeholder="Custom name (empty = node_id)"
                           value="${escapeAttr(devices.displayName(nodeId) === nodeId ? '' : devices.displayName(nodeId))}">
                    <button type="submit">Rename</button>
                </form>
                <p style="margin-top:14px;">
                    <button class="secondary" id="btn-remove">Remove from list</button>
                </p>
            </div>
            <div class="panel event-log dev-only">
                <h3>Event log</h3>
                <ul id="ev-list"></ul>
            </div>
        </div>
    `;

    const repaint = () => {
        const dev = devices.get(nodeId) || { nodeId, state: null, info: null, events: [], lastSeen: null };
        root.querySelector('#title-name').textContent = devices.displayName(nodeId);
        root.querySelector('#status-panel').innerHTML = statusPanel(dev);
        const evList = root.querySelector('#ev-list');
        if (evList) {
            evList.innerHTML = dev.events.length
                ? dev.events.map(e => `<li>[${new Date(e.ts).toLocaleTimeString()}] ${escapeHtml(e.text)}</li>`).join('')
                : '<li class="empty">No events yet.</li>';
        }
    };

    repaint();
    const unsub = devices.subscribe(repaint);
    window.addEventListener('hashchange', unsub, { once: true });

    root.querySelectorAll('[data-cmd]').forEach(btn => {
        btn.addEventListener('click', () => mqtt.sendCommand(nodeId, btn.dataset.cmd));
    });
    root.querySelector('#btn-remove').addEventListener('click', () => {
        devices.remove(nodeId);
        location.hash = '#/';
    });
    root.querySelector('#rename-form').addEventListener('submit', (e) => {
        e.preventDefault();
        devices.rename(nodeId, root.querySelector('#name-input').value);
    });
}

function statusPanel(d) {
    const state = d.state || 'unknown';
    const seen = d.lastSeen ? new Date(d.lastSeen).toLocaleString() : '-';
    const info = d.info || {};
    const gateStatus = d.gateStatus || {};
    const fault = gateStatus.fault || 'none';
    const message = gateStatus.message || '';
    const wifi = info.wifi || 'unknown';
    const ssid = info.ssid || '';
    const wifiOnline = wifi === 'connected';
    const wifiLabel = wifiOnline
        ? (ssid ? `Connected to "${ssid}"` : 'Connected')
        : (wifi === 'disconnected' ? 'Disconnected' : 'Unknown');
    const wifiClass = wifiOnline ? 'on' : (wifi === 'disconnected' ? 'err' : 'off');
    const faultClass = fault === 'none' ? 'ok' : 'err';
    return `
        <h3>Status</h3>
        <div class="kv">
            <div class="k">Gate state</div><div><span class="state-badge state-${escapeHtml(state)}">${escapeHtml(state)}</span></div>
            <div class="k">Wi-Fi</div><div><span class="wifi-badge"><span class="dot ${wifiClass}"></span>${escapeHtml(wifiLabel)}</span></div>
            <div class="k">Fault</div><div><span class="fault-badge fault-${faultClass}">${escapeHtml(fault)}</span></div>
            ${message ? `<div class="k">Message</div><div>${escapeHtml(message)}</div>` : ''}
            <div class="k">Technology</div><div>${escapeHtml(info.technology ?? '-')}</div>
            <div class="k">SSID</div>      <div>${escapeHtml(info.ssid ?? '-')}</div>
            <div class="k">Channel</div>   <div>${escapeHtml(info.channel ?? '-')}</div>
            <div class="k">Manufacturer</div><div>${escapeHtml(info.manufacturer ?? '-')}</div>
            <div class="k">Firmware</div>  <div>${escapeHtml(info.firmware_version ?? '-')}</div>
            <div class="k">Report</div>    <div>${escapeHtml(formatInterval(info.report_interval_ms))}</div>
            <div class="k dev-only">node_id</div><div class="dev-only">${escapeHtml(d.nodeId)}</div>
            <div class="k dev-only">MQTT</div>      <div class="dev-only">${escapeHtml(info.mqtt ?? '-')}</div>
            <div class="k dev-only">IP</div>        <div class="dev-only">${escapeHtml(info.ip ?? '-')}</div>
            <div class="k">RSSI</div>      <div>${escapeHtml(info.rssi ?? '-')}</div>
            <div class="k">Last seen</div> <div>${escapeHtml(seen)}</div>
        </div>
    `;
}

function formatInterval(ms) {
    return Number.isFinite(ms) ? `${ms} ms` : '-';
}

function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, c => ({
        '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'
    }[c]));
}

function escapeAttr(s) {
    return String(s ?? '').replace(/"/g, '&quot;');
}
