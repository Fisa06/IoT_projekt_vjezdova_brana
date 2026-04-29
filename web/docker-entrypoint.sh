#!/bin/sh
# Generate /config.json at container start from environment variables,
# so the same image can target different brokers without rebuilding.
set -e

: "${MQTT_BROKER_URL:=}"
: "${MQTT_USERNAME:=}"
: "${MQTT_PASSWORD:=}"

json_escape() {
  printf '%s' "$1" | awk '
    BEGIN { ORS = "" }
    {
      gsub(/\\/, "\\\\")
      gsub(/"/, "\\\"")
      gsub(/\t/, "\\t")
      gsub(/\r/, "\\r")
      if (NR > 1) {
        printf "\\n"
      }
      printf "%s", $0
    }
  '
}

cat > /usr/share/nginx/html/config.json <<EOF
{
  "brokerUrl": "$(json_escape "$MQTT_BROKER_URL")",
  "username":  "$(json_escape "$MQTT_USERNAME")",
  "password":  "$(json_escape "$MQTT_PASSWORD")"
}
EOF
