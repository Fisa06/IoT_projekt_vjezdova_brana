//
// Created by karel on 15.04.2026.
//

#ifndef IOT_PROJEKT_VJEZDOVA_BRANA_PWM_GATE_CONTROLL_H
#define IOT_PROJEKT_VJEZDOVA_BRANA_PWM_GATE_CONTROLL_H

#include "esp_err.h"

/* PWM output setup for the two gate inputs. */
esp_err_t ledc_init(void);

esp_err_t gate_pwm_set_open(void);
esp_err_t gate_pwm_set_idle(void);
esp_err_t gate_pwm_set_close(void);

#endif //IOT_PROJEKT_VJEZDOVA_BRANA_PWM_GATE_CONTROLL_H
