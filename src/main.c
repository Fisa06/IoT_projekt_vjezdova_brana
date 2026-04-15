#include <stdio.h>
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
// LEDC configuration constants
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_GPIO 19
#define PWM_FREQUENCY 50
#define LEDC_RESOLUTION LEDC_TIMER_10_BIT  // 10-bit resolution (0-1023)

// Calculate duty cycle values (10-bit: 0-1023)
#define DUTY_25_PERCENT 256    // 1023 * 0.25 ≈ 256
#define DUTY_75_PERCENT 768    // 1023 * 0.75 ≈ 768

void ledc_init() {
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_RESOLUTION,
        .freq_hz = PWM_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Configure LEDC channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_GPIO,
        .duty = DUTY_25_PERCENT,  // Start with 25%
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);

    printf("LEDC PWM initialized on pin %d with 50 Hz frequency\n", LEDC_GPIO);
}

void set_duty_cycle(uint32_t duty_value) {
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty_value);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    printf("Duty cycle set to: %lu (0-1023)\n", duty_value);
}

void swap_duty_cycle() {
    uint32_t current_duty = ledc_get_duty(LEDC_MODE, LEDC_CHANNEL);
    uint32_t new_duty = (current_duty == DUTY_25_PERCENT) ? DUTY_75_PERCENT : DUTY_25_PERCENT;
    set_duty_cycle(new_duty);
}

void app_main() {
    ledc_init();

    // Example: Start with 25%, then swap to 75% every 2 seconds
    while(1) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        swap_duty_cycle();
    }
}
