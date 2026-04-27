# Gate Control Web

Statický web (HTML/CSS/JS, žiadny build, žiadny backend) → MQTT-over-WebSocket.

## Spustenie

```sh
docker build -t gate-web .
docker run -d --rm -p 8080:80 --name gate-web gate-web
```

Otvor http://localhost:8080 → **Nastavenia** → vyplň broker (host, port, user, heslo).
Uloží sa do `localStorage`.

> Bez Dockeru: `cd public && python3 -m http.server 8080`

## Štruktúra

```
public/
├── index.html
├── css/styles.css
└── js/
    ├── app.js          # router
    ├── config.js       # /config.json
    ├── settings.js     # localStorage
    ├── mqtt-client.js  # mqtt.js (CDN)
    ├── device-store.js
    ├── logger.js
    └── views/{menu,device,logs,settings}.js
```

## MQTT topiky

| Topic                    | Payload |
| ------------------------ | ------- |
| `gate/<id>/cmd`          | `{"id","command":"open|close|stop"}` |
| `gate/<id>/reply`        | `{"id","status","message"}` |
| `gate/<id>/gate_status`  | `{"state"}` (retained) |
| `gate/<id>/device_info`  | `{"node_id","wifi","ssid","mqtt","gate_state","ip","rssi"}` (retained) |

Subscribuje `gate/+/...` → zariadenia sa objavia samé.
