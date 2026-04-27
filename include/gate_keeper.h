#ifndef IOT_PROJEKT_VJEZDOVA_BRANA_GATE_KEEPER_H
#define IOT_PROJEKT_VJEZDOVA_BRANA_GATE_KEEPER_H

#include "esp_err.h"

typedef enum {
    GATE_KEEPER_COMMAND_OPEN = 0,
    GATE_KEEPER_COMMAND_CLOSE,
    GATE_KEEPER_COMMAND_STOP,
} gate_keeper_command_t;

typedef enum {
    GATE_KEEPER_STATUS_STOPPED = 0,
    GATE_KEEPER_STATUS_OPENING,
    GATE_KEEPER_STATUS_OPEN,
    GATE_KEEPER_STATUS_CLOSING,
    GATE_KEEPER_STATUS_CLOSED,
} gate_keeper_status_t;

esp_err_t gate_keeper_init(void);
esp_err_t gate_keeper_validate_command(gate_keeper_command_t command);
esp_err_t gate_keeper_send_command(gate_keeper_command_t command);
gate_keeper_status_t gate_keeper_get_status(void);

#endif //IOT_PROJEKT_VJEZDOVA_BRANA_GATE_KEEPER_H
