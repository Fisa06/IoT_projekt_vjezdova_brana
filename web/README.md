# Gate Control Web

Static dashboard for the gate controller project. It is built with plain HTML, CSS, and JavaScript, served by nginx, and connects directly to the MQTT broker over WebSocket.

## Features

- no build step
- no backend service
- broker settings stored in browser `localStorage`
- optional bootstrap configuration from `/config.json`
- automatic device discovery via `gate/+/...` subscriptions

## Run with Docker

From the repository root:

```sh
docker compose up -d --build
```

Or from the `web/` directory:

```sh
docker build -t gate-web .
docker run -d --rm -p 8082:80 --name gate-web gate-web
```

## Run Without Docker

```sh
cd public
python -m http.server 8080
```

Then open `http://localhost:8080`.

## Runtime Configuration

The dashboard supports two configuration sources:

### 1. Browser settings

The Settings page lets the user define:

- broker host
- broker port
- broker path
- TLS enabled or disabled
- username
- password
- developer mode

These values are persisted in browser `localStorage`.

### 2. `/config.json`

When served from the Docker container, nginx generates `/config.json` at startup from:

- `MQTT_BROKER_URL`
- `MQTT_USERNAME`
- `MQTT_PASSWORD`

See [public/config.example.json](C:/Users/karel/CLionProjects/IoT_projekt_vjezdova_brana/web/public/config.example.json) for the expected structure.

## File Structure

```text
public/
  index.html
  css/styles.css
  js/
    app.js
    config.js
    settings.js
    mqtt-client.js
    device-store.js
    logger.js
    views/
      menu.js
      device.js
      logs.js
      settings.js
```

## MQTT Topics

| Topic | Payload |
| --- | --- |
| `gate/<id>/cmd` | `{"id","command":"open|close|stop"}` |
| `gate/<id>/reply` | `{"id","status":"accepted|error","message":"..."}` |
| `gate/<id>/gate_status` | `{"state"}` |
| `gate/<id>/device_info` | `{"node_id","wifi","ssid","mqtt","gate_state","ip","rssi"}` |

The dashboard subscribes to `gate/+/device_info`, `gate/+/gate_status`, and `gate/+/reply`, so devices appear automatically when they publish retained state.
