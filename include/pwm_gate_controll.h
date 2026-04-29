//
// Created by karel on 15.04.2026.
//

#ifndef IOT_PROJEKT_VJEZDOVA_BRANA_PWM_GATE_CONTROLL_H
#define IOT_PROJEKT_VJEZDOVA_BRANA_PWM_GATE_CONTROLL_H

#include "esp_err.h"

/**
 * @brief Initialize two LEDC PWM outputs for the gate mechanism.
 * @details One PWM output drives the open input, the other drives the close input.
 *          Both outputs are idle after initialization.
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ledc_init(void);

esp_err_t gate_pwm_set_open(void);
esp_err_t gate_pwm_set_idle(void);
esp_err_t gate_pwm_set_close(void);

#endif //IOT_PROJEKT_VJEZDOVA_BRANA_PWM_GATE_CONTROLL_H
