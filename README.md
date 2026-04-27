# IoT projekt — vjazdová brána

ESP32-C6 firmware (PlatformIO / ESP-IDF) + webové rozhranie (Plain HTML/JS, nginx
v Dockeri). Komunikácia cez MQTT-over-WebSocket, žiadny vlastný backend.

```
┌──────────┐   MQTT/TLS   ┌────────┐   WSS    ┌──────────┐
│  ESP32   │ ───────────► │ Broker │ ───────► │ Web (PC/ │
│  brána   │ ◄─────────── │        │ ◄─────── │ telefón) │
└──────────┘              └────────┘          └──────────┘
```

---

## 1) Spustenie webu (najjednoduchšie)

Potrebuješ len **Docker**.

```sh
docker build -t gate-web ./web
docker run -d --rm -p 8080:80 --name gate-web gate-web
```

Otvor **http://localhost:8080** (na telefóne `http://<IP-PC>:8080`).

Zastavenie: `docker rm -f gate-web`

### Nastavenie brokera

Pri prvom otvorení choď do **Nastavenia** v menu a vyplň:
- **Broker host / port / path** (napr. `mqtt.xdlabs.cloud` / `8084` / `/mqtt`, TLS zapnuté)
- **Užívateľ + heslo** k brokeru

Ulozí sa do `localStorage` prehliadača — pre každé zariadenie / prehliadač zvlášť.
Tu môžeš aj prepnúť **Developer mode** a premenovať zariadenia.

> Bez Dockeru: `cd web/public && python3 -m http.server 8080`

---

## 2) Flash firmware na ESP32-C6

Potrebuješ **PlatformIO** (CLI alebo VS Code extension).

```sh
# 1. Vyplň prihlasovacie údaje
cp include/secrets.h.example include/secrets.h   # ak existuje, inak vytvor podľa vzoru
#   #define WIFI_SSID      "..."
#   #define WIFI_PASSWORD  "..."
#   #define MQTT_BROKER_URI "mqtts://mqtt.xdlabs.cloud:8883"
#   #define MQTT_USERNAME  "..."
#   #define MQTT_PASSWORD  "..."

# 2. Nastav unikátne ID brány
# include/config.h:  #define NODE_ID "6767"

# 3. Build & flash
pio run -t upload
pio device monitor
```

---

## 3) MQTT topiky

| Topic                     | Smer       | Payload (JSON) |
| ------------------------- | ---------- | -------------- |
| `gate/<id>/cmd`           | web → ESP  | `{"id":"...","command":"open|close|stop"}` |
| `gate/<id>/reply`         | ESP → web  | `{"id":"...","status":"ok|err","message":"..."}` |
| `gate/<id>/gate_status`   | ESP → web  | `{"state":"open|closed|opening|closing|stopped"}` (retained) |
| `gate/<id>/device_info`   | ESP → web  | `{"node_id","wifi","ssid","mqtt","gate_state","ip","rssi"}` (retained) |

Web sa predplatí na `gate/+/...` — všetky brány sa objavia automaticky.

---

## 4) Štruktúra

```
├── src/, include/        # ESP-IDF firmware (PlatformIO)
├── platformio.ini
└── web/                  # Webové rozhranie + Docker
    ├── Dockerfile
    ├── .env.example
    └── public/           # HTML/CSS/JS (žiadny build step)
```

Detaily webu: [web/README.md](web/README.md).
