# Gate Control Web

This is the small dashboard I used for the gate project demo. I kept it as plain HTML, CSS and JavaScript so it can be opened easily and there is no frontend build step to explain or maintain.

## Features

- no frontend build step
- no backend service in this repository
- broker settings saved in browser `localStorage`
- optional Docker startup config through `/config.json`
- automatic device discovery from retained MQTT topics
- gate state and fault display from `gate_status`
- Wi-Fi RSSI/channel and device data from `device_info`

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

The dashboard can get broker settings in two ways:

### 1. Browser settings

The Settings page lets me enter:

- broker host
- broker port
- broker path
- TLS enabled or disabled
- username
- password
- developer mode

These values stay in browser `localStorage`.

### 2. `/config.json`

When it runs in Docker, `docker-entrypoint.sh` writes `/config.json` from:

- `MQTT_BROKER_URL`
- `MQTT_USERNAME`
- `MQTT_PASSWORD`

See [public/config.example.json](public/config.example.json) for the expected structure.

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
| `gate/<id>/gate_status` | `{"state","fault","message"}` |
| `gate/<id>/device_info` | `{"node_id","manufacturer","firmware_version","technology","wifi","ssid","channel","mqtt","gate_state","ip","rssi","report_interval_ms"}` |

The dashboard subscribes to `gate/+/device_info`, `gate/+/gate_status`, and `gate/+/reply`. Devices appear automatically after they publish retained state.
