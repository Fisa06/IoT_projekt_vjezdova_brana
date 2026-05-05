// Log view used mostly during testing.
import { logger } from '../logger.js';

export function renderLogs(root) {
    root.innerHTML = `
        <div class="logs-header">
            <h2 class="section-title">Logs</h2>
            <button class="secondary" id="clear-logs">Clear</button>
        </div>
        <div class="logs-panel">
            <ul id="logs-list"></ul>
        </div>
    `;

    const listEl = root.querySelector('#logs-list');

    const repaint = () => {
        const items = logger.list();
        if (items.length === 0) {
            listEl.innerHTML = '<li class="empty">No log entries.</li>';
            return;
        }
        listEl.innerHTML = items.map(e =>
            `<li><span class="ts">${new Date(e.ts).toLocaleTimeString()}</span> ${escapeHtml(e.text)}</li>`
        ).join('');
    };

    repaint();
    const unsub = logger.subscribe(repaint);
    window.addEventListener('hashchange', unsub, { once: true });

    root.querySelector('#clear-logs').addEventListener('click', () => logger.clear());
}

function escapeHtml(s) {
    return String(s).replace(/[&<>"']/g, c => ({
        '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'
    }[c]));
}
