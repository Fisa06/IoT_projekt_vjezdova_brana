// Settings view: broker config + developer mode.
import { settings } from '../settings.js';
import { mqtt } from '../mqtt-client.js';

export function renderSettings(root) {
    const s = settings.get();
    root.innerHTML = `
        <h2 class="section-title">Nastavenia</h2>
        <form class="settings-form" id="settings-form">
            <fieldset class="panel">
                <h3>MQTT broker</h3>
                <label>Adresa
                    <input type="text" name="brokerHost" value="${escapeAttr(s.brokerHost)}" required>
                </label>
                <label>Port
                    <input type="text" name="brokerPort" value="${escapeAttr(s.brokerPort)}" required>
                </label>
                <label>Cesta (path)
                    <input type="text" name="brokerPath" value="${escapeAttr(s.brokerPath)}" required>
                </label>
                <label class="checkbox">
                    <input type="checkbox" name="brokerTls" ${s.brokerTls ? 'checked' : ''}>
                    Šifrované pripojenie (wss://)
                </label>
                <label>Používateľ
                    <input type="text" name="username" value="${escapeAttr(s.username)}">
                </label>
                <label>Heslo
                    <input type="password" name="password" value="${escapeAttr(s.password)}">
                </label>
            </fieldset>

            <fieldset class="panel">
                <h3>Aplikácia</h3>
                <label class="checkbox">
                    <input type="checkbox" name="devMode" ${s.devMode ? 'checked' : ''}>
                    Vývojársky režim (zobraz technické detaily a logy)
                </label>
            </fieldset>

            <div class="form-actions">
                <button type="submit">Uložiť a pripojiť</button>
                <span id="save-hint" class="hint"></span>
            </div>
        </form>
    `;

    root.querySelector('#settings-form').addEventListener('submit', (e) => {
        e.preventDefault();
        const f = new FormData(e.target);
        const patch = {
            brokerHost: String(f.get('brokerHost')).trim(),
            brokerPort: Number(f.get('brokerPort')) || 8084,
            brokerPath: String(f.get('brokerPath')).trim() || '/mqtt',
            brokerTls:  f.get('brokerTls') === 'on',
            username:   String(f.get('username') ?? ''),
            password:   String(f.get('password') ?? ''),
            devMode:    f.get('devMode') === 'on',
        };
        settings.save(patch);
        mqtt.reconnect();
        const hint = root.querySelector('#save-hint');
        hint.textContent = 'Uložené, pripájam sa…';
        setTimeout(() => { hint.textContent = ''; }, 2500);
        applyDevModeClass();
    });
}

export function applyDevModeClass() {
    document.body.classList.toggle('dev-mode', settings.get().devMode);
}

function escapeAttr(s) {
    return String(s ?? '').replace(/"/g, '&quot;');
}
