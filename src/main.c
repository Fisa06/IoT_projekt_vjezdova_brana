#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "freertos/task.h"
#include "driver/gpio.h"

#include "gate_keeper.h"
#include "mqtt.h"
#include "pwm_gate_controll.h"
#include "wifi_provisioning.h"

static const char *TAG = "MAIN";
static const gpio_num_t WIFI_RESET_BUTTON_GPIO = GPIO_NUM_13;

static void wifi_reset_button_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << WIFI_RESET_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&config));
}

// cppcheck-suppress unusedFunction
void app_main() {
    ESP_ERROR_CHECK(ledc_init());
    ESP_ERROR_CHECK(gate_keeper_init());
    wifi_reset_button_init();
    if (gpio_get_level(WIFI_RESET_BUTTON_GPIO) == 0) {
        ESP_LOGI(TAG, "GPIO13 held low at boot, clearing saved Wi-Fi credentials");
        wifi_provisioning_reset();
    }

    wifi_provisioning_start();
    ESP_ERROR_CHECK(mqtt_init());

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
