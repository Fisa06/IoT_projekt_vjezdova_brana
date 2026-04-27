#!/bin/sh
# Generate /config.json at container start from environment variables,
# so the same image can target different brokers without rebuilding.
set -e

: "${MQTT_BROKER_URL:=}"
: "${MQTT_USERNAME:=}"
: "${MQTT_PASSWORD:=}"

cat > /usr/share/nginx/html/config.json <<EOF
{
  "brokerUrl": "${MQTT_BROKER_URL}",
  "username":  "${MQTT_USERNAME}",
  "password":  "${MQTT_PASSWORD}"
}
EOF

exec nginx -g 'daemon off;'
