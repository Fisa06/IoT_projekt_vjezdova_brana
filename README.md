# IoT Gate Controller

ESP32-C6 firmware built with PlatformIO/ESP-IDF plus a small static web dashboard served by nginx in Docker. The firmware talks to the broker over MQTT/TLS, and the browser dashboard talks to the same broker over MQTT-over-WebSocket. There is no custom backend.

## Architecture

```text
+-------------+   MQTT/TLS   +-------------+   MQTT over WSS    +------------------+
| ESP32-C6    | -----------> | MQTT broker | <----------------> | Web dashboard    |
| gate unit   | <----------- |             |                    | phone / desktop  |
+-------------+              +-------------+                    +------------------+
```

## Repository Layout

```text
src/ and include/   ESP-IDF firmware sources
web/                Static dashboard and Docker files
platformio.ini      PlatformIO environment definition
partitions.csv      ESP32 partition table
docker-compose.yml  Local dashboard container setup
```

## Firmware Overview

The firmware currently covers:

- Wi-Fi provisioning using the ESP-IDF provisioning manager and SoftAP mode
- MQTT command handling for `open`, `close`, and `stop`
- Gate state tracking based on end-stop and obstacle inputs
- PWM-based actuator control
- Periodic device status reporting

Important configuration lives in:

- [include/config.h](C:/Users/karel/CLionProjects/IoT_projekt_vjezdova_brana/include/config.h) for `NODE_ID` and GPIO mapping
- `include/secrets.h` for MQTT credentials and provisioning POP
- [platformio.ini](C:/Users/karel/CLionProjects/IoT_projekt_vjezdova_brana/platformio.ini) for the PlatformIO board and framework setup

## Dashboard Overview

The dashboard is a static HTML/CSS/JS app with no build step. It:

- connects directly to the broker over WebSocket
- discovers devices by subscribing to `gate/+/...`
- stores broker settings in `localStorage`
- can also preload broker settings from `/config.json`

More details are in [web/README.md](C:/Users/karel/CLionProjects/IoT_projekt_vjezdova_brana/web/README.md).

## Firmware Setup

### 1. Create Local Secrets

Create `include/secrets.h` based on [include/secrets.example.h](C:/Users/karel/CLionProjects/IoT_projekt_vjezdova_brana/include/secrets.example.h):

```c
#define WIFI_PROV_POP "replace-me"
#define MQTT_BROKER_URI "mqtts://example.com"
#define MQTT_BROKER_PORT 8883
#define MQTT_USERNAME "replace-me"
#define MQTT_PASSWORD "replace-me"
```

Notes:

- Wi-Fi credentials are provisioned at runtime and are not stored in `secrets.h`.
- `include/secrets.h` is gitignored and should stay local.

### 2. Adjust Hardware Configuration

Set a unique node ID and verify the GPIO mapping in [include/config.h](C:/Users/karel/CLionProjects/IoT_projekt_vjezdova_brana/include/config.h).

### 3. Build and Flash

```sh
pio run
pio run -t upload
pio device monitor
```

### 4. Reset Saved Wi-Fi Credentials

Hold GPIO13 low during boot to clear saved provisioning data.

## Dashboard Setup

### Option 1: Docker Compose

```sh
docker compose up -d --build
```

This publishes the dashboard on `http://localhost:8082`.

Stop it with:

```sh
docker compose down
```

### Option 2: Docker Only

```sh
docker build -t gate-web ./web
docker run -d --rm -p 8082:80 --name gate-web gate-web
```

### Option 3: Static Local Server

```sh
cd web/public
python -m http.server 8080
```

If you use the static server route, open `http://localhost:8080`.

## Dashboard Broker Configuration

You have two ways to configure broker access:

### In the UI

Open the Settings view and fill in:

- broker host
- broker port
- broker path, usually `/mqtt`
- TLS on/off
- username and password

These values are stored in browser `localStorage`.

### Via Container Environment

The nginx container generates `/config.json` on startup from:

- `MQTT_BROKER_URL`
- `MQTT_USERNAME`
- `MQTT_PASSWORD`

The example format is shown in [web/public/config.example.json](C:/Users/karel/CLionProjects/IoT_projekt_vjezdova_brana/web/public/config.example.json).

## MQTT Topics

| Topic | Direction | Payload |
| --- | --- | --- |
| `gate/<id>/cmd` | web -> ESP | `{"id":"...","command":"open|close|stop"}` |
| `gate/<id>/reply` | ESP -> web | `{"id":"...","status":"accepted|error","message":"..."}` |
| `gate/<id>/gate_status` | ESP -> web | `{"state":"open|closed|opening|closing|stopped"}` |
| `gate/<id>/device_info` | ESP -> web | `{"node_id","wifi","ssid","mqtt","gate_state","ip","rssi"}` |

Notes:

- `gate_status` and `device_info` are published as retained messages.
- A command reply of `accepted` means the firmware accepted the command for processing.
- Redundant commands such as `close` while already closed are rejected with `status: "error"` and a descriptive message.

## Useful Commands

```sh
pio run
pio check
pio device monitor
docker compose up -d --build
docker compose down
```

