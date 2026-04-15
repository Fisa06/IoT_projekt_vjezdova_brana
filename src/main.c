#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "freertos/task.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "mqtt.h"
#include "pwm_gate_controll.h"
#include "wifi_provisioning.h"

static const char *TAG = "WiFi_kepper";
static bool wifi_connected = false;  // Flag to check if Wi-Fi is connected

// Event handler to manage Wi-Fi events
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        esp_wifi_connect(); // Automatically reconnect
        ESP_LOGI(TAG, "Wi-Fi disconnected, reconnecting...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        char ip_address_str[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_address_str, 16);
 ESP_LOGI(TAG, "Got IP: %s", ip_address_str);
    }
} // wifi handler function ( is registed as handler function in wifi_init_sta)


// Function to initialize and connect Wi-Fi
void wifi_init_sta() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS Flash init failed: %s", esp_err_to_name(ret));
    }

    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create an event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default station (STA) network interface instance
    esp_netif_create_default_wifi_sta();

    // Initialize the Wi-Fi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register Wi-Fi and IP event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // Configure Wi-Fi connection parameters
    wifi_config_t wifi_config = {
            .sta = {
                    .ssid = "Karel - iPhone",  // SSID
                    .password = "x",  // Password
            },
    };
    /*strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));*/

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));  // Set Wi-Fi to station mode
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));  // Set configuration
    ESP_ERROR_CHECK(esp_wifi_start());  // Start Wi-Fi

    ESP_LOGI(TAG, "Wi-Fi initialization complete.");
}






void app_main() {
    ledc_init();
    wifi_provisioning_start();

    // Example: Start with 25%, then swap to 75% every 2 seconds
    while(1) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        swap_duty_cycle();
    }
}
