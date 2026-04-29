# IoT Gate Controller

ESP32-C6 firmware for a driveway gate plus a small browser dashboard. The firmware is built with PlatformIO and ESP-IDF. The dashboard is a static HTML/CSS/JS app served by nginx in Docker. Both sides communicate through an MQTT broker, so there is no custom backend service in this repository.

## Contents

- [Architecture](#architecture)
- [Technical Solution](#technical-solution)
- [Repository Layout](#repository-layout)
- [Hardware Configuration](#hardware-configuration)
- [Firmware Behavior](#firmware-behavior)
- [Firmware Setup](#firmware-setup)
- [Dashboard Setup](#dashboard-setup)
- [MQTT Protocol](#mqtt-protocol)
- [Development Commands](#development-commands)
- [Troubleshooting](#troubleshooting)
- [Current Caveats](#current-caveats)

## Architecture

```text
+-------------+   MQTT/TLS   +-------------+   MQTT over WSS   +------------------+
| ESP32-C6    | -----------> | MQTT broker | <----------------> | Web dashboard    |
| gate unit   | <----------- |             |                    | phone / desktop  |
+-------------+              +-------------+                    +------------------+
```

The ESP32 publishes retained status messages and subscribes to commands for its own node ID. The web dashboard subscribes to wildcard topics, so every gate unit that publishes retained state can appear automatically in the UI.

## Technical Solution

This project uses Wi-Fi as the runtime radio technology. The assignment assumes a family house with 230V power at the gate, no data cable, and no existing outdoor Wi-Fi coverage. Wi-Fi is suitable here because an outdoor access point or mesh node can be added near the gate, the range is enough for a typical driveway installation, and MQTT/TLS support is mature on ESP32-C6. BLE is used only for initial Wi-Fi provisioning, so the user can configure credentials from a phone without temporarily joining an ESP SoftAP network.

The transport/application layer is MQTT over TLS from the ESP32 to the broker. The dashboard uses MQTT over secure WebSockets when running in a browser. MQTT is used because commands, retained state, last-will status, and periodic telemetry map naturally to topics. Gate state is retained so a freshly opened dashboard immediately receives the last known state.

The demo hardware uses the ESP32-C6-DevKitM-1 integrated 2.4 GHz PCB antenna. For an outdoor installation, the recommended additional hardware is an outdoor Wi-Fi AP or mesh node covering the driveway, an isolated 230V AC to low-voltage DC power supply, and a weatherproof enclosure. If the enclosure or mounting location attenuates the signal too much, use a board/module variant with an external 2.4 GHz antenna connector and an IP-rated antenna mounted outside the enclosure.

The firmware publishes `device_info` every 5 seconds. This is intentionally short for a classroom/demo setup so RSSI and channel changes are visible quickly. In a battery-powered or production system, this interval should be increased. Current limitations are listed in [Current Caveats](#current-caveats).

## Repository Layout

```text
src/
  main.c                  Firmware entrypoint and boot-time reset button handling
  gate_keeper.c           Gate state machine, end-stop handling, obstacle handling
  mqtt.c                  MQTT client, command parsing, status publishing
  pwm_gate_controll.c     LEDC PWM setup and gate actuator output positions
  wifi_provisioning.c     ESP-IDF Wi-Fi provisioning flow

include/
  config.h                Node ID and GPIO mapping
  secrets.example.h       Template for local MQTT/provisioning secrets
  *.h                     Public firmware module headers

web/
  Dockerfile              nginx image for the dashboard
  docker-entrypoint.sh    Generates /config.json from environment variables
  public/                 Static dashboard files

platformio.ini            PlatformIO environment definition
partitions.csv            ESP32 partition table
docker-compose.yml        Local dashboard container setup
```

## Hardware Configuration

The active hardware mapping is defined in [`include/config.h`](include/config.h) and [`src/pwm_gate_controll.c`](src/pwm_gate_controll.c).

| Signal | GPIO | Direction | Active Level | Notes |
| --- | ---: | --- | --- | --- |
| Open end switch | 23 | input | low | Internal pull-up enabled |
| Closed end switch | 9 | input | low | Internal pull-up enabled |
| Obstacle sensor | 12 | input | low | Internal pull-up enabled |
| Local gate control button | 13 | input | low | Toggle/stop local gate control |
| Gate open PWM input | 15 | output | PWM | 50 Hz LEDC output |
| Gate close PWM input | 22 | output | PWM | 50 Hz LEDC output |
| Wi-Fi reset button | 20 | input | low | Hold low during boot to clear saved Wi-Fi credentials |

The gate inputs and local control button are debounced in software. A signal must be stable for three polling samples before the firmware treats it as active or inactive.

## Firmware Behavior

### Startup Flow

1. Initialize both PWM outputs and set the gate actuator to idle.
2. Start the gate keeper task and prime the input states.
3. Configure the Wi-Fi reset button on GPIO20.
4. If GPIO20 is held low during boot, clear saved Wi-Fi provisioning credentials.
5. Start Wi-Fi provisioning or connect to saved Wi-Fi.
6. Start the MQTT client.

### Gate States

The externally published gate state can be one of:

| State | Meaning |
| --- | --- |
| `open` | Open end switch is active |
| `closed` | Closed end switch is active |
| `opening` | Gate is currently moving toward the open end switch |
| `closing` | Gate is currently moving toward the closed end switch |
| `stopped` | Gate is idle and neither end switch is active, or inputs are invalid |

### Command Rules

The MQTT command API accepts `open`, `close`, and `stop`.

- `open` is rejected when the gate is already `open` or `opening`.
- `close` is rejected when the gate is already `closed` or `closing`.
- Opposite-direction commands while moving are rejected; stop the gate before reversing direction.
- `stop` is allowed and returns the actuator to idle.
- If both end switches are active at the same time, movement is stopped and the state resolves to `stopped`.
- If an obstacle is detected while opening or closing, the firmware immediately stops the gate.
- After an obstacle stop, the firmware waits 5 seconds and retries the interrupted movement when the obstacle input is clear.
- If movement takes longer than the configured timeout, the firmware stops the gate.

### Local Button Control

GPIO13 is a local gate control button with the same active-low debounce logic as the other inputs.

- If the gate is closed, one press starts opening.
- If the gate is open, one press starts closing.
- If the gate is moving, one press stops the gate and remembers the opposite direction.
- After a local stop, the next press moves the gate in that opposite direction.
- If the gate is stopped between end switches without a remembered direction, the default local action is to open.

The local button still goes through the gate keeper safety checks, so invalid end-switch states, obstacle handling, and movement timeouts continue to apply.

### PWM Positions

The PWM layer currently uses a 10-bit LEDC timer at 50 Hz. The gate mechanism has two PWM inputs, so only one direction output is active at a time:

| Action | Open PWM Duty | Close PWM Duty |
| --- | ---: | ---: |
| Open | 512 / 50% | 0 / 0% |
| Idle | 0 / 0% | 0 / 0% |
| Close | 0 / 0% | 512 / 50% |

These values are project-specific actuator commands for the assignment demo, not generic hobby-servo pulse widths.

## Firmware Setup

### Requirements

- PlatformIO Core or a PlatformIO-capable IDE
- ESP32-C6 board matching `esp32-c6-devkitm-1` with 4MB flash
- MQTT broker with TLS support for the firmware
- MQTT-over-WebSocket support if you want to use the browser dashboard

### 1. Create Local Secrets

Create `include/secrets.h` based on [`include/secrets.example.h`](include/secrets.example.h):

```c
#define WIFI_PROV_POP "replace-me"
#define MQTT_BROKER_URI "mqtts://example.com"
#define MQTT_BROKER_PORT 8883
#define MQTT_USERNAME "replace-me"
#define MQTT_PASSWORD "replace-me"
```

Notes:

- Wi-Fi credentials are provisioned at runtime and are not stored in `secrets.h`.
- `include/secrets.h` is ignored by git and should stay local.
- `WIFI_PROV_POP` is the proof-of-possession string used during Wi-Fi provisioning.

### 2. Set the Node ID

Set a unique `NODE_ID` in [`include/config.h`](include/config.h):

```c
#define NODE_ID "6767"
#define DEVICE_MANUFACTURER "VUTID-6767"
#define DEVICE_FIRMWARE_VERSION "1.0.0"
```

The node ID becomes part of every MQTT topic, for example `gate/6767/device_info`.
Set `DEVICE_MANUFACTURER` to your VUT ID before final submission.

### 3. Build, Upload, and Monitor

```sh
pio run
pio run -t upload
pio device monitor
```

The monitor speed is configured as `115200` in [`platformio.ini`](platformio.ini).

### 4. Provision Wi-Fi Over BLE

On first boot, or after clearing saved credentials, the device starts the ESP-IDF BLE provisioning service. The device advertises this BLE name:

```text
PROV_<last three bytes of STA MAC>
```

Use the configured `WIFI_PROV_POP` when the provisioning client asks for proof of possession. The firmware also logs the exact QR payload to the serial monitor:

```json
{"ver":"v1","name":"PROV_<last three bytes of STA MAC>","pop":"<WIFI_PROV_POP>","transport":"ble"}
```

You can scan that payload with Espressif's ESP BLE Provisioning app or enter the BLE device name and POP manually in a compatible provisioning client. After provisioning finishes, the firmware releases the BLE provisioning manager and starts the Wi-Fi station connection.

### 5. Reset Saved Wi-Fi Credentials

Hold GPIO20 low while the board boots. The firmware clears saved provisioning credentials, then starts provisioning again.

## Dashboard Setup

### Option 1: Docker Compose

From the repository root:

```sh
docker compose up -d --build
```

The dashboard is published at:

```text
http://localhost:8082
```

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

Then open:

```text
http://localhost:8080
```

### Dashboard Broker Configuration

The dashboard can be configured in two ways.

The normal interactive path is the Settings view in the browser. It stores these values in `localStorage`:

- broker host
- broker port
- broker path, usually `/mqtt`
- TLS enabled or disabled
- username
- password
- developer mode

The Docker image can also generate `/config.json` from environment variables at container startup:

| Environment Variable | Purpose |
| --- | --- |
| `MQTT_BROKER_URL` | Full WebSocket broker URL, for example `wss://broker.example.com:8084/mqtt` |
| `MQTT_USERNAME` | Broker username |
| `MQTT_PASSWORD` | Broker password |

See [`web/public/config.example.json`](web/public/config.example.json) for the file format.

## MQTT Protocol

All topics use this prefix:

```text
gate/<node_id>/
```

For `NODE_ID "6767"`, the firmware uses topics such as `gate/6767/cmd` and `gate/6767/gate_status`.

### Topics

| Topic | Direction | Retained | Payload |
| --- | --- | --- | --- |
| `gate/<id>/cmd` | web -> ESP | no | Command request |
| `gate/<id>/reply` | ESP -> web | no | Command acknowledgement or rejection |
| `gate/<id>/gate_status` | ESP -> web | yes | Current gate state |
| `gate/<id>/device_info` | ESP -> web | yes | Periodic device info |

### Command Request

```json
{
  "id": "web-1710000000000",
  "command": "open"
}
```

Allowed commands:

- `open`
- `close`
- `stop`

### Command Reply

Accepted command:

```json
{
  "id": "web-1710000000000",
  "status": "accepted"
}
```

Rejected command:

```json
{
  "id": "web-1710000000000",
  "status": "error",
  "message": "gate already closed"
}
```

Important: `accepted` means the firmware accepted and queued the command. It does not mean the gate has already reached the target end position. Use `gate_status` for actual state.

### Gate Status

```json
{
  "state": "stopped",
  "fault": "obstacle_detected",
  "message": "Obstacle detected; movement stopped and retry is pending"
}
```

Valid states:

- `open`
- `closed`
- `opening`
- `closing`
- `stopped`

Valid fault values:

- `none`
- `timeout`
- `invalid_limits`
- `obstacle_detected`

The `message` field is only present when `fault` is not `none`.

### Device Info

```json
{
  "node_id": "6767",
  "manufacturer": "VUTID-6767",
  "firmware_version": "1.0.0",
  "technology": "WiFi",
  "wifi": "connected",
  "mqtt": "connected",
  "gate_state": "closed",
  "ip": "192.168.1.50",
  "rssi": -55,
  "ssid": "example-wifi",
  "channel": 6,
  "report_interval_ms": 5000
}
```

`device_info` is published immediately after MQTT connects and then periodically while MQTT is connected. For Wi-Fi, it includes RSSI, SSID, and channel as the radio-link parameters required by the assignment.

## Development Commands

Firmware:

```sh
pio run
pio run -t upload
pio device monitor
pio check
```

Dashboard:

```sh
docker compose up -d --build
docker compose down
```

Useful paths:

```text
.pio/build/esp32-c6-devkitm-1/firmware.bin
web/public/index.html
web/public/js/
```

## Troubleshooting

### `pio` or `platformio` is not found

On this machine, PlatformIO may be available through the local PlatformIO virtual environment:

```powershell
& 'C:\Users\karel\.platformio\penv\Scripts\pio.exe' run
```

or:

```powershell
& 'C:\Users\karel\.platformio\penv\Scripts\platformio.exe' run
```

### Static Analysis Reports `unusedFunction`

Some ESP-IDF entrypoints and module functions are referenced through framework conventions or across translation units in a way that Cppcheck may not fully understand. The project uses targeted `cppcheck-suppress unusedFunction` comments only for those false positives.

Run:

```sh
pio check
```

Expected result:

```text
No defects found
```

### MQTT Reply Says `error`

The firmware rejects commands that do not make sense for the current state. Examples:

- `close` while already `closed`
- `close` while already `closing`
- `open` while already `open`
- `open` while already `opening`

This is intentional. Check the `message` field in the reply payload.

### MQTT Status Looks Stale

`gate_status` and `device_info` are retained MQTT messages. When a dashboard reconnects, it may immediately receive the last retained state before any new state is published.

### Wi-Fi Logs Mention `ADDBA` or `DELBA`

These are ESP-IDF Wi-Fi stack logs related to 802.11 block acknowledgement negotiation. If Wi-Fi and MQTT stay connected, they are usually informational noise rather than an application problem.

## Current Caveats

- The dashboard source still contains some Slovak UI strings.
- The firmware uses fixed PWM duty values; calibrate them for the actual actuator hardware before real deployment.
- The project does not currently include automated firmware unit tests.
- MQTT credentials and provisioning POP are compile-time firmware secrets in `include/secrets.h`.
