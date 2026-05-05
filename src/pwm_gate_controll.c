#include "pwm_gate_controll.h"

#include "config.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"

#define PWM_FREQUENCY 50
#define PWM_ACTIVE_DUTY 512
#define PWM_IDLE_DUTY 0

static const char *TAG = "PWM_GATE_CTRL";

static esp_err_t gate_pwm_set_channel_duty(ledc_channel_t channel, uint32_t duty)
{
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting PWM channel %d duty: %d", channel, err);
        return err;
    }

    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error updating PWM channel %d duty: %d", channel, err);
        return err;
    }

    return ESP_OK;
}

static esp_err_t gate_pwm_configure_channel(ledc_channel_t channel, int gpio_num)
{
    const ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = gpio_num,
        .duty = PWM_IDLE_DUTY,
        .hpoint = 0
    };

    return ledc_channel_config(&ledc_channel);
}

esp_err_t ledc_init(void)
{
    const ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = PWM_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_RETURN_ON_ERROR(ledc_timer_config(&ledc_timer), TAG, "ledc_timer_config failed");
    ESP_RETURN_ON_ERROR(
        gate_pwm_configure_channel(LEDC_CHANNEL_0, GATE_PWM_OPEN_GPIO),
        TAG,
        "Failed to configure open PWM channel");
    ESP_RETURN_ON_ERROR(
        gate_pwm_configure_channel(LEDC_CHANNEL_1, GATE_PWM_CLOSE_GPIO),
        TAG,
        "Failed to configure close PWM channel");

    ESP_LOGI(TAG,
             "LEDC initialized: open GPIO=%d close GPIO=%d frequency=%d Hz resolution=%d bits",
             GATE_PWM_OPEN_GPIO,
             GATE_PWM_CLOSE_GPIO,
             PWM_FREQUENCY,
             10);
    return ESP_OK;
}

esp_err_t gate_pwm_set_open(void)
{
    ESP_RETURN_ON_ERROR(gate_pwm_set_channel_duty(LEDC_CHANNEL_1, PWM_IDLE_DUTY), TAG, "Failed to idle close PWM");
    ESP_RETURN_ON_ERROR(gate_pwm_set_channel_duty(LEDC_CHANNEL_0, PWM_ACTIVE_DUTY), TAG, "Failed to set open PWM");
    ESP_LOGI(TAG, "Gate PWM command: open active, close idle");
    return ESP_OK;
}

esp_err_t gate_pwm_set_idle(void)
{
    ESP_RETURN_ON_ERROR(gate_pwm_set_channel_duty(LEDC_CHANNEL_0, PWM_IDLE_DUTY), TAG, "Failed to idle open PWM");
    ESP_RETURN_ON_ERROR(gate_pwm_set_channel_duty(LEDC_CHANNEL_1, PWM_IDLE_DUTY), TAG, "Failed to idle close PWM");
    ESP_LOGI(TAG, "Gate PWM command: both outputs idle");
    return ESP_OK;
}

esp_err_t gate_pwm_set_close(void)
{
    ESP_RETURN_ON_ERROR(gate_pwm_set_channel_duty(LEDC_CHANNEL_0, PWM_IDLE_DUTY), TAG, "Failed to idle open PWM");
    ESP_RETURN_ON_ERROR(gate_pwm_set_channel_duty(LEDC_CHANNEL_1, PWM_ACTIVE_DUTY), TAG, "Failed to set close PWM");
    ESP_LOGI(TAG, "Gate PWM command: open idle, close active");
    return ESP_OK;
}
