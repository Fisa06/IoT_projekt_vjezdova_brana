#include "mqtt.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "config.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gate_keeper.h"
#include "lwip/ip4_addr.h"
#include "mqtt_client.h"
#include "secrets.h"

#define MQTT_COMMAND_TOPIC "gate/" NODE_ID "/cmd"
#define MQTT_REPLY_TOPIC "gate/" NODE_ID "/reply"
#define MQTT_GATE_STATUS_TOPIC "gate/" NODE_ID "/gate_status"
#define MQTT_DEVICE_INFO_TOPIC "gate/" NODE_ID "/device_info"

static const char *TAG = "MQTT";
static const TickType_t DEVICE_INFO_PUBLISH_PERIOD = pdMS_TO_TICKS(5000);

static esp_mqtt_client_handle_t s_mqtt_client;
static bool s_mqtt_connected;
static gate_keeper_status_t s_last_published_gate_status = (gate_keeper_status_t) -1;
static gate_keeper_fault_t s_last_published_gate_fault = (gate_keeper_fault_t) -1;

static const char *gate_keeper_status_to_string(gate_keeper_status_t status)
{
    switch (status) {
        case GATE_KEEPER_STATUS_STOPPED:
            return "stopped";
        case GATE_KEEPER_STATUS_OPENING:
            return "opening";
        case GATE_KEEPER_STATUS_OPEN:
            return "open";
        case GATE_KEEPER_STATUS_CLOSING:
            return "closing";
        case GATE_KEEPER_STATUS_CLOSED:
            return "closed";
        default:
            return "unknown";
    }
}

static bool mqtt_publish_json(const char *topic, const cJSON *json, int qos, int retain)
{
    char *payload = cJSON_PrintUnformatted(json);
    if (payload == NULL) {
        ESP_LOGE(TAG, "Failed to allocate JSON payload for topic %s", topic);
        return false;
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, qos, retain);
    cJSON_free(payload);

    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish to topic %s", topic);
        return false;
    }

    return true;
}

static void mqtt_publish_reply(const char *id, const char *status, const char *message)
{
    if (!s_mqtt_connected || s_mqtt_client == NULL) {
        return;
    }

    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to allocate reply JSON");
        return;
    }

    cJSON_AddStringToObject(json, "id", id);
    cJSON_AddStringToObject(json, "status", status);
    if (message != NULL) {
        cJSON_AddStringToObject(json, "message", message);
    }

    mqtt_publish_json(MQTT_REPLY_TOPIC, json, 1, 0);
    cJSON_Delete(json);
}

static void mqtt_publish_gate_status_if_changed(void)
{
    gate_keeper_status_t status;
    gate_keeper_fault_t fault;
    cJSON *json;

    if (!s_mqtt_connected || s_mqtt_client == NULL) {
        return;
    }

    status = gate_keeper_get_status();
    fault = gate_keeper_get_fault();
    if (status == s_last_published_gate_status && fault == s_last_published_gate_fault) {
        return;
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to allocate gate status JSON");
        return;
    }

    cJSON_AddStringToObject(json, "state", gate_keeper_status_to_string(status));
    cJSON_AddStringToObject(json, "fault", gate_keeper_get_fault_string());
    const char *fault_message = gate_keeper_get_fault_message();
    if (fault_message[0] != '\0') {
        cJSON_AddStringToObject(json, "message", fault_message);
    }

    if (mqtt_publish_json(MQTT_GATE_STATUS_TOPIC, json, 1, 1)) {
        s_last_published_gate_status = status;
        s_last_published_gate_fault = fault;
    }

    cJSON_Delete(json);
}

void mqtt_report_gate_status_change(gate_keeper_status_t status)
{
    (void) status;
    s_last_published_gate_status = (gate_keeper_status_t) -1;
    s_last_published_gate_fault = (gate_keeper_fault_t) -1;
    mqtt_publish_gate_status_if_changed();
}

static void mqtt_publish_device_info(void)
{
    cJSON *json;
    esp_netif_t *netif;
    esp_netif_ip_info_t ip_info;
    wifi_ap_record_t ap_info;
    const char *wifi_status = "disconnected";
    char ip_string[16] = "";

    if (!s_mqtt_connected || s_mqtt_client == NULL) {
        return;
    }

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        wifi_status = "connected";
    } else {
        memset(&ap_info, 0, sizeof(ap_info));
    }

    netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        esp_ip4addr_ntoa(&ip_info.ip, ip_string, sizeof(ip_string));
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to allocate device info JSON");
        return;
    }

    cJSON_AddStringToObject(json, "node_id", NODE_ID);
    cJSON_AddStringToObject(json, "manufacturer", DEVICE_MANUFACTURER);
    cJSON_AddStringToObject(json, "firmware_version", DEVICE_FIRMWARE_VERSION);
    cJSON_AddStringToObject(json, "technology", DEVICE_CONNECTIVITY_TECHNOLOGY);
    cJSON_AddStringToObject(json, "wifi", wifi_status);
    cJSON_AddStringToObject(json, "mqtt", s_mqtt_connected ? "connected" : "disconnected");
    cJSON_AddStringToObject(json, "gate_state", gate_keeper_status_to_string(gate_keeper_get_status()));
    cJSON_AddStringToObject(json, "ip", ip_string[0] != '\0' ? ip_string : "0.0.0.0");
    cJSON_AddNumberToObject(json, "report_interval_ms", DEVICE_INFO_PUBLISH_PERIOD * portTICK_PERIOD_MS);

    if (wifi_status[0] == 'c') {
        cJSON_AddNumberToObject(json, "rssi", ap_info.rssi);
        cJSON_AddStringToObject(json, "ssid", (const char *) ap_info.ssid);
        cJSON_AddNumberToObject(json, "channel", ap_info.primary);
    }

    mqtt_publish_json(MQTT_DEVICE_INFO_TOPIC, json, 1, 1);
    cJSON_Delete(json);
}

static gate_keeper_command_t mqtt_parse_gate_command(const char *command, bool *known)
{
    if (strcmp(command, "open") == 0) {
        *known = true;
        return GATE_KEEPER_COMMAND_OPEN;
    }

    if (strcmp(command, "close") == 0) {
        *known = true;
        return GATE_KEEPER_COMMAND_CLOSE;
    }

    if (strcmp(command, "stop") == 0) {
        *known = true;
        return GATE_KEEPER_COMMAND_STOP;
    }

    *known = false;
    return GATE_KEEPER_COMMAND_STOP;
}

static const char *mqtt_command_rejection_message(gate_keeper_command_t command)
{
    gate_keeper_status_t status = gate_keeper_get_status();

    switch (command) {
        case GATE_KEEPER_COMMAND_OPEN:
            if (status == GATE_KEEPER_STATUS_OPEN) {
                return "gate already open";
            }
            if (status == GATE_KEEPER_STATUS_OPENING) {
                return "gate already opening";
            }
            if (status == GATE_KEEPER_STATUS_CLOSING) {
                return "gate is closing; stop before opening";
            }
            break;
        case GATE_KEEPER_COMMAND_CLOSE:
            if (status == GATE_KEEPER_STATUS_CLOSED) {
                return "gate already closed";
            }
            if (status == GATE_KEEPER_STATUS_CLOSING) {
                return "gate already closing";
            }
            if (status == GATE_KEEPER_STATUS_OPENING) {
                return "gate is opening; stop before closing";
            }
            break;
        case GATE_KEEPER_COMMAND_STOP:
            return "stop command rejected";
        default:
            return "unknown command";
    }

    return "command rejected";
}

static void mqtt_handle_command_message(const char *payload, int payload_len)
{
    cJSON *json;
    cJSON *id_item;
    cJSON *command_item;
    bool known_command;
    gate_keeper_command_t command;
    esp_err_t err;

    json = cJSON_ParseWithLength(payload, payload_len);
    if (json == NULL) {
        mqtt_publish_reply("unknown", "error", "invalid json");
        return;
    }

    id_item = cJSON_GetObjectItemCaseSensitive(json, "id");
    command_item = cJSON_GetObjectItemCaseSensitive(json, "command");
    if (!cJSON_IsString(id_item) || id_item->valuestring == NULL) {
        cJSON_Delete(json);
        mqtt_publish_reply("unknown", "error", "missing id");
        return;
    }

    if (!cJSON_IsString(command_item) || command_item->valuestring == NULL) {
        mqtt_publish_reply(id_item->valuestring, "error", "missing command");
        cJSON_Delete(json);
        return;
    }

    command = mqtt_parse_gate_command(command_item->valuestring, &known_command);
    if (!known_command) {
        mqtt_publish_reply(id_item->valuestring, "error", "unknown command");
        cJSON_Delete(json);
        return;
    }

    err = gate_keeper_send_command(command);
    if (err != ESP_OK) {
        const char *message = err == ESP_ERR_INVALID_STATE
                                  ? mqtt_command_rejection_message(command)
                                  : esp_err_to_name(err);
        mqtt_publish_reply(id_item->valuestring, "error", message);
        cJSON_Delete(json);
        return;
    }

    mqtt_publish_reply(id_item->valuestring, "accepted", NULL);
    cJSON_Delete(json);
}

static void mqtt_device_info_task(void *arg)
{
    (void) arg;

    for (;;) {
        if (s_mqtt_connected) {
            mqtt_publish_device_info();
        }

        vTaskDelay(DEVICE_INFO_PUBLISH_PERIOD);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    (void) handler_args;
    (void) base;

    switch ((esp_mqtt_event_id_t) event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            s_mqtt_connected = true;
            s_last_published_gate_status = (gate_keeper_status_t) -1;
            esp_mqtt_client_subscribe(event->client, MQTT_COMMAND_TOPIC, 1);
            mqtt_publish_device_info();
            mqtt_publish_gate_status_if_changed();
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            s_mqtt_connected = false;
            break;
        case MQTT_EVENT_DATA:
            if (event->topic_len == (int) strlen(MQTT_COMMAND_TOPIC) &&
                strncmp(event->topic, MQTT_COMMAND_TOPIC, event->topic_len) == 0) {
                if (event->current_data_offset != 0 || event->data_len != event->total_data_len) {
                    ESP_LOGW(TAG, "Ignoring fragmented MQTT command payload");
                    mqtt_publish_reply("unknown", "error", "fragmented payload unsupported");
                    break;
                }
                mqtt_handle_command_message(event->data, event->data_len);
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error event");
            break;
        default:
            break;
    }
}

esp_err_t mqtt_init(void)
{
    static bool device_info_task_started;

    if (s_mqtt_client != NULL) {
        return ESP_OK;
    }

    const esp_mqtt_client_config_t mqtt_client_config = {
        .broker = {
            .address = {
                .uri = MQTT_BROKER_URI,
                .port = MQTT_BROKER_PORT
            },
            .verification = {
                .crt_bundle_attach = esp_crt_bundle_attach,
            },
        },
        .session = {
            .keepalive = 120,
            .last_will = {
                .topic = MQTT_DEVICE_INFO_TOPIC,
                .msg = "{\"node_id\":\"" NODE_ID "\",\"mqtt\":\"disconnected\"}",
                .qos = 1,
                .retain = 1
            }
        },
        .credentials = {
            .username = MQTT_USERNAME,
            .client_id = NODE_ID,
            .authentication = {
                .password = MQTT_PASSWORD
            }
        }
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_client_config);
    if (s_mqtt_client == NULL) {
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(
        esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL),
        TAG,
        "Failed to register MQTT event handler");

    if (!device_info_task_started) {
        BaseType_t task_created = xTaskCreate(
            mqtt_device_info_task,
            "mqtt_device_info",
            4096,
            NULL,
            4,
            NULL);
        if (task_created != pdPASS) {
            return ESP_FAIL;
        }
        device_info_task_started = true;
    }

    return esp_mqtt_client_start(s_mqtt_client);
}
