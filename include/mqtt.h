//
// Created by karel on 15.04.2026.
//

#ifndef IOT_PROJEKT_VJEZDOVA_BRANA_MQTT_H
#define IOT_PROJEKT_VJEZDOVA_BRANA_MQTT_H

#include "esp_err.h"

#include "gate_keeper.h"

esp_err_t mqtt_init(void);
void mqtt_report_gate_status_change(gate_keeper_status_t status);

#endif //IOT_PROJEKT_VJEZDOVA_BRANA_MQTT_H
