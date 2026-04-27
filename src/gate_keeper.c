#include "gate_keeper.h"

#include <stdbool.h>

#include "config.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mqtt.h"
#include "pwm_gate_controll.h"

typedef enum {
    GATE_STATE_IDLE = 0,
    GATE_STATE_OPENING,
    GATE_STATE_CLOSING,
} gate_state_t;

static const char *TAG = "GATE_KEEPER";
static const TickType_t GATE_KEEPER_POLL_TICKS = pdMS_TO_TICKS(20);
static const UBaseType_t GATE_KEEPER_QUEUE_LENGTH = 8;
static const uint32_t GATE_KEEPER_TASK_STACK_SIZE = 4096;
static const UBaseType_t GATE_KEEPER_TASK_PRIORITY = 5;

static QueueHandle_t s_gate_command_queue;
static TaskHandle_t s_gate_keeper_task_handle;
static gate_keeper_status_t s_gate_status = GATE_KEEPER_STATUS_STOPPED;

static bool gate_input_active(gpio_num_t gpio_num)
{
    return gpio_get_level(gpio_num) == 0;
}

static bool gate_open_limit_active(void)
{
    return gate_input_active((gpio_num_t) GATE_END_OPEN_GPIO);
}

static bool gate_close_limit_active(void)
{
    return gate_input_active((gpio_num_t) GATE_END_CLOSE_GPIO);
}

static bool gate_obstacle_active(void)
{
    return gate_input_active((gpio_num_t) GATE_OBSTACLE_GPIO);
}

static gate_keeper_status_t gate_keeper_resolve_status(gate_state_t state)
{
    if (gate_open_limit_active()) {
        return GATE_KEEPER_STATUS_OPEN;
    }

    if (gate_close_limit_active()) {
        return GATE_KEEPER_STATUS_CLOSED;
    }

    if (state == GATE_STATE_OPENING) {
        return GATE_KEEPER_STATUS_OPENING;
    }

    if (state == GATE_STATE_CLOSING) {
        return GATE_KEEPER_STATUS_CLOSING;
    }

    return GATE_KEEPER_STATUS_STOPPED;
}

static void gate_keeper_refresh_status(gate_state_t state)
{
    gate_keeper_status_t new_status = gate_keeper_resolve_status(state);
    if (new_status != s_gate_status) {
        s_gate_status = new_status;
        mqtt_report_gate_status_change(new_status);
        return;
    }

    s_gate_status = new_status;
}

static esp_err_t gate_keeper_gpio_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << GATE_END_OPEN_GPIO) |
                        (1ULL << GATE_END_CLOSE_GPIO) |
                        (1ULL << GATE_OBSTACLE_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&config);
}

static void gate_keeper_start_opening(gate_state_t *state)
{
    if (gate_open_limit_active()) {
        ESP_LOGI(TAG, "Open command ignored, open end switch already active");
        ESP_ERROR_CHECK(gate_pwm_set_idle());
        *state = GATE_STATE_IDLE;
        gate_keeper_refresh_status(*state);
        return;
    }

    ESP_ERROR_CHECK(gate_pwm_set_open());
    *state = GATE_STATE_OPENING;
    gate_keeper_refresh_status(*state);
    ESP_LOGI(TAG, "Gate opening");
}

static void gate_keeper_start_closing(gate_state_t *state)
{
    if (gate_close_limit_active()) {
        ESP_LOGI(TAG, "Close command ignored, close end switch already active");
        ESP_ERROR_CHECK(gate_pwm_set_idle());
        *state = GATE_STATE_IDLE;
        gate_keeper_refresh_status(*state);
        return;
    }

    ESP_ERROR_CHECK(gate_pwm_set_close());
    *state = GATE_STATE_CLOSING;
    gate_keeper_refresh_status(*state);
    ESP_LOGI(TAG, "Gate closing");
}

static void gate_keeper_stop(gate_state_t *state, const char *reason)
{
    ESP_LOGI(TAG, "Gate stopping: %s", reason);
    ESP_ERROR_CHECK(gate_pwm_set_idle());
    *state = GATE_STATE_IDLE;
    gate_keeper_refresh_status(*state);
}

static void gate_keeper_handle_command(gate_state_t *state, gate_keeper_command_t command)
{
    switch (command) {
        case GATE_KEEPER_COMMAND_OPEN:
            if (*state == GATE_STATE_OPENING) {
                ESP_LOGI(TAG, "Open command ignored, gate is already opening");
                return;
            }
            gate_keeper_start_opening(state);
            break;
        case GATE_KEEPER_COMMAND_CLOSE:
            if (*state == GATE_STATE_CLOSING) {
                ESP_LOGI(TAG, "Close command ignored, gate is already closing");
                return;
            }
            gate_keeper_start_closing(state);
            break;
        case GATE_KEEPER_COMMAND_STOP:
            gate_keeper_stop(state, "stop command");
            break;
        default:
            ESP_LOGW(TAG, "Unknown gate command received: %d", command);
            break;
    }
}

static void gate_keeper_update_motion(gate_state_t *state)
{
    if (*state == GATE_STATE_OPENING && gate_open_limit_active()) {
        ESP_LOGI(TAG, "Open end switch reached, idling gate");
        ESP_ERROR_CHECK(gate_pwm_set_idle());
        *state = GATE_STATE_IDLE;
        gate_keeper_refresh_status(*state);
        return;
    }

    if (*state == GATE_STATE_CLOSING && gate_close_limit_active()) {
        ESP_LOGI(TAG, "Close end switch reached, idling gate");
        ESP_ERROR_CHECK(gate_pwm_set_idle());
        *state = GATE_STATE_IDLE;
        gate_keeper_refresh_status(*state);
        return;
    }

    if (*state == GATE_STATE_CLOSING && gate_obstacle_active()) {
        gate_keeper_stop(state, "optical obstacle");
    }
}

static void gate_keeper_task(void *arg)
{
    gate_state_t state = GATE_STATE_IDLE;
    gate_keeper_command_t command;

    (void) arg;

    ESP_ERROR_CHECK(gate_pwm_set_idle());
    gate_keeper_refresh_status(state);
    ESP_LOGI(TAG, "Gate keeper task started");

    for (;;) {
        if (xQueueReceive(s_gate_command_queue, &command, GATE_KEEPER_POLL_TICKS) == pdTRUE) {
            gate_keeper_handle_command(&state, command);
        }

        gate_keeper_update_motion(&state);
    }
}

esp_err_t gate_keeper_init(void)
{
    if (s_gate_command_queue != NULL) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(gate_keeper_gpio_init(), TAG, "Failed to initialize gate inputs");

    s_gate_command_queue = xQueueCreate(GATE_KEEPER_QUEUE_LENGTH, sizeof(gate_keeper_command_t));
    if (s_gate_command_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create gate command queue");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(
            gate_keeper_task,
            "gate_keeper",
            GATE_KEEPER_TASK_STACK_SIZE,
            NULL,
            GATE_KEEPER_TASK_PRIORITY,
            &s_gate_keeper_task_handle) != pdPASS) {
        vQueueDelete(s_gate_command_queue);
        s_gate_command_queue = NULL;
        s_gate_keeper_task_handle = NULL;
        ESP_LOGE(TAG, "Failed to create gate keeper task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "Gate inputs configured: open=%d close=%d obstacle=%d",
             GATE_END_OPEN_GPIO,
             GATE_END_CLOSE_GPIO,
             GATE_OBSTACLE_GPIO);

    return ESP_OK;
}

esp_err_t gate_keeper_send_command(gate_keeper_command_t command)
{
    if (s_gate_command_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(s_gate_command_queue, &command, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

gate_keeper_status_t gate_keeper_get_status(void)
{
    return s_gate_status;
}
