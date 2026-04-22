#include "pwm_gate_controll.h"

#include "driver/ledc.h"
#include "esp_log.h"

#define LEDC_GPIO 19
#define PWM_FREQUENCY 50

#define DUTY_25_PERCENT 256
#define DUTY_50_PERCENT 512
#define DUTY_75_PERCENT 768

static const char *TAG = "PWM_GATE_CTRL";

esp_err_t ledc_init() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = PWM_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_GPIO,
        .duty = DUTY_50_PERCENT,
        .hpoint = 0
    };

    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %d", err);
        return err;
    }

    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting ledc channel config: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "LEDC initialized with frequency %d Hz and resolution %d bits", PWM_FREQUENCY, 10);
    return ESP_OK;
}

esp_err_t set_duty_cycle(uint32_t duty_value) {
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting duty cycle: %d", err);
        return err;
    }

    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error updating duty cycle: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "PWM duty cycle set to %d%%", (duty_value * 100) / 1023);
    return ESP_OK;
}

esp_err_t gate_pwm_set_open(void) {
    return set_duty_cycle(DUTY_25_PERCENT);
}

esp_err_t gate_pwm_set_idle(void) {
    return set_duty_cycle(DUTY_50_PERCENT);
}

esp_err_t gate_pwm_set_close(void) {
    return set_duty_cycle(DUTY_75_PERCENT);
}
