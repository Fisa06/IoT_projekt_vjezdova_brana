//
// Created by karel on 15.04.2026.
//

#ifndef IOT_PROJEKT_VJEZDOVA_BRANA_PWM_GATE_CONTROLL_H
#define IOT_PROJEKT_VJEZDOVA_BRANA_PWM_GATE_CONTROLL_H
#include "esp_err.h"

/**
 * @brief Initialize LEDC PWM on GPIO pin 15 with 50 Hz frequency
 * @details Configures the LEDC timer and channel with 10-bit resolution.
 *          Initial duty cycle is set to 50%.
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ledc_init();

/**
 * @brief Set PWM duty cycle
 * @param duty_value Duty cycle value (0-1023 for 10-bit resolution)
 *                   - 256 = 25%
 *                   - 512 = 50%
 *                   - 768 = 75%
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t set_duty_cycle(uint32_t duty_value);

esp_err_t gate_pwm_set_open(void);
esp_err_t gate_pwm_set_idle(void);
esp_err_t gate_pwm_set_close(void);

#endif //IOT_PROJEKT_VJEZDOVA_BRANA_PWM_GATE_CONTROLL_H
