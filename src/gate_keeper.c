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

typedef struct {
    gpio_num_t gpio_num;
    uint8_t active_samples;
    uint8_t inactive_samples;
    bool stable_active;
} gate_input_t;

static const char *TAG = "GATE_KEEPER";
static const TickType_t GATE_KEEPER_POLL_TICKS = pdMS_TO_TICKS(20);
static const TickType_t GATE_MOVE_TIMEOUT_TICKS = pdMS_TO_TICKS(20000);
static const TickType_t GATE_OBSTACLE_RETRY_DELAY_TICKS = pdMS_TO_TICKS(5000);
static const UBaseType_t GATE_KEEPER_QUEUE_LENGTH = 8;
static const uint32_t GATE_KEEPER_TASK_STACK_SIZE = 4096;
static const UBaseType_t GATE_KEEPER_TASK_PRIORITY = 5;
static const uint8_t GATE_INPUT_DEBOUNCE_SAMPLES = 3;

static QueueHandle_t s_gate_command_queue;
static TaskHandle_t s_gate_keeper_task_handle;
static gate_keeper_status_t s_gate_status = GATE_KEEPER_STATUS_STOPPED;
static gate_keeper_fault_t s_gate_fault = GATE_KEEPER_FAULT_NONE;
static gate_keeper_fault_t s_last_reported_fault = (gate_keeper_fault_t) -1;
static TickType_t s_move_started_at;
static bool s_obstacle_retry_pending;
static gate_state_t s_obstacle_retry_state = GATE_STATE_IDLE;
static TickType_t s_obstacle_retry_started_at;
static gate_input_t s_open_limit_input = { .gpio_num = (gpio_num_t) GATE_END_OPEN_GPIO };
static gate_input_t s_close_limit_input = { .gpio_num = (gpio_num_t) GATE_END_CLOSE_GPIO };
static gate_input_t s_obstacle_input = { .gpio_num = (gpio_num_t) GATE_OBSTACLE_GPIO };
static gate_input_t s_local_button_input = { .gpio_num = (gpio_num_t) GATE_LOCAL_BUTTON_GPIO };
static gate_keeper_command_t s_local_next_idle_command = GATE_KEEPER_COMMAND_OPEN;

static const char *gate_state_to_string(gate_state_t state)
{
    switch (state) {
        case GATE_STATE_OPENING:
            return "opening";
        case GATE_STATE_CLOSING:
            return "closing";
        case GATE_STATE_IDLE:
        default:
            return "idle";
    }
}

static const char *gate_fault_to_string(gate_keeper_fault_t fault)
{
    switch (fault) {
        case GATE_KEEPER_FAULT_TIMEOUT:
            return "timeout";
        case GATE_KEEPER_FAULT_INVALID_LIMITS:
            return "invalid_limits";
        case GATE_KEEPER_FAULT_OBSTACLE_STOP:
            return "obstacle_detected";
        case GATE_KEEPER_FAULT_NONE:
        default:
            return "none";
    }
}

static const char *gate_fault_to_message(gate_keeper_fault_t fault)
{
    switch (fault) {
        case GATE_KEEPER_FAULT_TIMEOUT:
            return "Gate movement timed out";
        case GATE_KEEPER_FAULT_INVALID_LIMITS:
            return "Both end switches are active";
        case GATE_KEEPER_FAULT_OBSTACLE_STOP:
            return "Obstacle detected; movement stopped and retry is pending";
        case GATE_KEEPER_FAULT_NONE:
        default:
            return "";
    }
}

static void gate_input_prime(gate_input_t *input)
{
    bool raw_active = gpio_get_level(input->gpio_num) == 0;
    input->stable_active = raw_active;
    input->active_samples = raw_active ? GATE_INPUT_DEBOUNCE_SAMPLES : 0;
    input->inactive_samples = raw_active ? 0 : GATE_INPUT_DEBOUNCE_SAMPLES;
}

static void gate_input_update(gate_input_t *input)
{
    bool raw_active = gpio_get_level(input->gpio_num) == 0;

    if (raw_active) {
        if (input->active_samples < GATE_INPUT_DEBOUNCE_SAMPLES) {
            input->active_samples++;
        }
        input->inactive_samples = 0;
        if (input->active_samples >= GATE_INPUT_DEBOUNCE_SAMPLES) {
            input->stable_active = true;
        }
        return;
    }

    if (input->inactive_samples < GATE_INPUT_DEBOUNCE_SAMPLES) {
        input->inactive_samples++;
    }
    input->active_samples = 0;
    if (input->inactive_samples >= GATE_INPUT_DEBOUNCE_SAMPLES) {
        input->stable_active = false;
    }
}

static bool gate_open_limit_active(void)
{
    return s_open_limit_input.stable_active;
}

static bool gate_close_limit_active(void)
{
    return s_close_limit_input.stable_active;
}

static bool gate_obstacle_active(void)
{
    return s_obstacle_input.stable_active;
}

static bool gate_local_button_pressed(void)
{
    return s_local_button_input.stable_active;
}

static bool gate_invalid_limit_state_active(void)
{
    return gate_open_limit_active() && gate_close_limit_active();
}

static void gate_inputs_update(void)
{
    gate_input_update(&s_open_limit_input);
    gate_input_update(&s_close_limit_input);
    gate_input_update(&s_obstacle_input);
    gate_input_update(&s_local_button_input);
}

static void gate_clear_fault(void)
{
    s_gate_fault = GATE_KEEPER_FAULT_NONE;
}

static void gate_set_fault(gate_keeper_fault_t fault, const char *message)
{
    if (s_gate_fault != fault) {
        ESP_LOGW(TAG, "%s", message);
    }
    s_gate_fault = fault;
}

static gate_keeper_status_t gate_keeper_resolve_status(gate_state_t state)
{
    if (gate_invalid_limit_state_active()) {
        return GATE_KEEPER_STATUS_STOPPED;
    }

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
    bool status_changed = new_status != s_gate_status;
    bool fault_changed = s_gate_fault != s_last_reported_fault;

    s_gate_status = new_status;
    if (status_changed || fault_changed) {
        s_last_reported_fault = s_gate_fault;
        mqtt_report_gate_status_change(new_status);
    }
}

static esp_err_t gate_keeper_gpio_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << GATE_END_OPEN_GPIO) |
                        (1ULL << GATE_END_CLOSE_GPIO) |
                        (1ULL << GATE_OBSTACLE_GPIO) |
                        (1ULL << GATE_LOCAL_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&config);
}

static void gate_keeper_start_opening(gate_state_t *state)
{
    if (gate_invalid_limit_state_active()) {
        gate_set_fault(GATE_KEEPER_FAULT_INVALID_LIMITS, "Cannot open: both end switches are active");
        ESP_ERROR_CHECK(gate_pwm_set_idle());
        *state = GATE_STATE_IDLE;
        gate_keeper_refresh_status(*state);
        return;
    }

    if (gate_open_limit_active()) {
        ESP_LOGI(TAG, "Open command ignored, open end switch already active");
        ESP_ERROR_CHECK(gate_pwm_set_idle());
        *state = GATE_STATE_IDLE;
        gate_keeper_refresh_status(*state);
        return;
    }

    gate_clear_fault();
    s_move_started_at = xTaskGetTickCount();
    ESP_ERROR_CHECK(gate_pwm_set_open());
    *state = GATE_STATE_OPENING;
    s_local_next_idle_command = GATE_KEEPER_COMMAND_CLOSE;
    gate_keeper_refresh_status(*state);
    ESP_LOGI(TAG, "Gate opening");
}

static void gate_keeper_start_closing(gate_state_t *state)
{
    if (gate_invalid_limit_state_active()) {
        gate_set_fault(GATE_KEEPER_FAULT_INVALID_LIMITS, "Cannot close: both end switches are active");
        ESP_ERROR_CHECK(gate_pwm_set_idle());
        *state = GATE_STATE_IDLE;
        gate_keeper_refresh_status(*state);
        return;
    }

    if (gate_close_limit_active()) {
        ESP_LOGI(TAG, "Close command ignored, close end switch already active");
        ESP_ERROR_CHECK(gate_pwm_set_idle());
        *state = GATE_STATE_IDLE;
        gate_keeper_refresh_status(*state);
        return;
    }

    gate_clear_fault();
    s_move_started_at = xTaskGetTickCount();
    ESP_ERROR_CHECK(gate_pwm_set_close());
    *state = GATE_STATE_CLOSING;
    s_local_next_idle_command = GATE_KEEPER_COMMAND_OPEN;
    gate_keeper_refresh_status(*state);
    ESP_LOGI(TAG, "Gate closing");
}

static void gate_keeper_stop(gate_state_t *state, const char *reason)
{
    ESP_LOGI(TAG, "Gate stopping: %s", reason);
    ESP_ERROR_CHECK(gate_pwm_set_idle());
    *state = GATE_STATE_IDLE;
    s_move_started_at = 0;
    gate_keeper_refresh_status(*state);
}

static void gate_cancel_obstacle_retry(void)
{
    if (s_obstacle_retry_pending) {
        ESP_LOGI(TAG, "Cancelling pending obstacle retry");
    }

    s_obstacle_retry_pending = false;
    s_obstacle_retry_state = GATE_STATE_IDLE;
    s_obstacle_retry_started_at = 0;
}

static void gate_schedule_obstacle_retry(gate_state_t interrupted_state)
{
    s_obstacle_retry_pending = true;
    s_obstacle_retry_state = interrupted_state;
    s_obstacle_retry_started_at = xTaskGetTickCount();
    ESP_LOGI(TAG,
             "Obstacle retry scheduled in %u ms for %s",
             (unsigned int) (GATE_OBSTACLE_RETRY_DELAY_TICKS * portTICK_PERIOD_MS),
             gate_state_to_string(interrupted_state));
}

static void gate_keeper_handle_command(gate_state_t *state, gate_keeper_command_t command)
{
    gate_cancel_obstacle_retry();

    switch (command) {
        case GATE_KEEPER_COMMAND_OPEN:
            if (*state == GATE_STATE_OPENING) {
                ESP_LOGI(TAG, "Open command ignored, gate is already opening");
                return;
            }
            if (*state == GATE_STATE_CLOSING) {
                ESP_LOGW(TAG, "Open command rejected, gate is closing; stop before reversing direction");
                return;
            }
            gate_keeper_start_opening(state);
            break;
        case GATE_KEEPER_COMMAND_CLOSE:
            if (*state == GATE_STATE_CLOSING) {
                ESP_LOGI(TAG, "Close command ignored, gate is already closing");
                return;
            }
            if (*state == GATE_STATE_OPENING) {
                ESP_LOGW(TAG, "Close command rejected, gate is opening; stop before reversing direction");
                return;
            }
            gate_keeper_start_closing(state);
            break;
        case GATE_KEEPER_COMMAND_STOP:
            gate_clear_fault();
            gate_keeper_stop(state, "stop command");
            break;
        default:
            ESP_LOGW(TAG, "Unknown gate command received: %d", command);
            break;
    }
}

static void gate_keeper_handle_local_button_press(gate_state_t *state)
{
    gate_keeper_command_t command = s_local_next_idle_command;
    gate_keeper_status_t status;

    if (*state == GATE_STATE_OPENING) {
        s_local_next_idle_command = GATE_KEEPER_COMMAND_CLOSE;
        ESP_LOGI(TAG, "Local gate button pressed while opening; stopping gate");
        gate_keeper_handle_command(state, GATE_KEEPER_COMMAND_STOP);
        return;
    }

    if (*state == GATE_STATE_CLOSING) {
        s_local_next_idle_command = GATE_KEEPER_COMMAND_OPEN;
        ESP_LOGI(TAG, "Local gate button pressed while closing; stopping gate");
        gate_keeper_handle_command(state, GATE_KEEPER_COMMAND_STOP);
        return;
    }

    status = gate_keeper_resolve_status(*state);
    if (status == GATE_KEEPER_STATUS_OPEN) {
        command = GATE_KEEPER_COMMAND_CLOSE;
    } else if (status == GATE_KEEPER_STATUS_CLOSED) {
        command = GATE_KEEPER_COMMAND_OPEN;
    }

    ESP_LOGI(TAG, "Local gate button pressed; sending %s command",
             command == GATE_KEEPER_COMMAND_OPEN ? "open" : "close");
    gate_keeper_handle_command(state, command);
}

static void gate_keeper_update_local_button(gate_state_t *state, bool *was_pressed)
{
    bool pressed = gate_local_button_pressed();

    if (pressed && !*was_pressed) {
        gate_keeper_handle_local_button_press(state);
    }

    *was_pressed = pressed;
}

static void gate_keeper_update_motion(gate_state_t *state)
{
    if (gate_invalid_limit_state_active()) {
        gate_cancel_obstacle_retry();
        gate_set_fault(GATE_KEEPER_FAULT_INVALID_LIMITS, "Both end switches are active");
        if (*state != GATE_STATE_IDLE) {
            gate_keeper_stop(state, "invalid end switch combination");
        } else {
            gate_keeper_refresh_status(*state);
        }
        return;
    }

    if (*state == GATE_STATE_OPENING && gate_open_limit_active()) {
        ESP_LOGI(TAG, "Open end switch reached, idling gate");
        ESP_ERROR_CHECK(gate_pwm_set_idle());
        *state = GATE_STATE_IDLE;
        s_move_started_at = 0;
        gate_keeper_refresh_status(*state);
        return;
    }

    if (*state == GATE_STATE_CLOSING && gate_close_limit_active()) {
        ESP_LOGI(TAG, "Close end switch reached, idling gate");
        ESP_ERROR_CHECK(gate_pwm_set_idle());
        *state = GATE_STATE_IDLE;
        s_move_started_at = 0;
        gate_keeper_refresh_status(*state);
        return;
    }
//depends may have to change that
    if ((*state == GATE_STATE_OPENING || *state == GATE_STATE_CLOSING) && gate_obstacle_active()) {
        gate_state_t interrupted_state = *state;
        gate_set_fault(GATE_KEEPER_FAULT_OBSTACLE_STOP, "Obstacle detected while moving");
        gate_schedule_obstacle_retry(interrupted_state);
        gate_keeper_stop(state, "optical obstacle");
        return;
    }

    if ((*state == GATE_STATE_OPENING || *state == GATE_STATE_CLOSING) &&
        s_move_started_at != 0 &&
        (xTaskGetTickCount() - s_move_started_at) >= GATE_MOVE_TIMEOUT_TICKS) {
        gate_set_fault(GATE_KEEPER_FAULT_TIMEOUT, "Gate movement timed out");
        gate_keeper_stop(state, "movement timeout");
    }
}

static void gate_keeper_update_obstacle_retry(gate_state_t *state)
{
    if (!s_obstacle_retry_pending || *state != GATE_STATE_IDLE) {
        return;
    }

    TickType_t now = xTaskGetTickCount();
    if ((now - s_obstacle_retry_started_at) < GATE_OBSTACLE_RETRY_DELAY_TICKS) {
        return;
    }

    if (gate_obstacle_active()) {
        ESP_LOGW(TAG, "Obstacle still active, retry delayed");
        s_obstacle_retry_started_at = now;
        return;
    }

    gate_state_t retry_state = s_obstacle_retry_state;
    gate_cancel_obstacle_retry();
    ESP_LOGI(TAG, "Retrying interrupted gate movement: %s", gate_state_to_string(retry_state));

    if (retry_state == GATE_STATE_OPENING) {
        gate_keeper_start_opening(state);
    } else if (retry_state == GATE_STATE_CLOSING) {
        gate_keeper_start_closing(state);
    }
}

static void gate_keeper_task(void *arg)
{
    gate_state_t state = GATE_STATE_IDLE;
    gate_keeper_command_t command;
    bool local_button_was_pressed = false;

    (void) arg;

    ESP_ERROR_CHECK(gate_pwm_set_idle());
    gate_input_prime(&s_open_limit_input);
    gate_input_prime(&s_close_limit_input);
    gate_input_prime(&s_obstacle_input);
    gate_input_prime(&s_local_button_input);
    local_button_was_pressed = gate_local_button_pressed();
    gate_keeper_refresh_status(state);
    ESP_LOGI(TAG, "Gate keeper task started");

    for (;;) {
        gate_inputs_update();
        gate_keeper_update_local_button(&state, &local_button_was_pressed);

        if (xQueueReceive(s_gate_command_queue, &command, GATE_KEEPER_POLL_TICKS) == pdTRUE) {
            gate_keeper_handle_command(&state, command);
        }

        gate_inputs_update();
        gate_keeper_update_motion(&state);
        gate_keeper_update_obstacle_retry(&state);
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
             "Gate inputs configured: open=%d close=%d obstacle=%d local_button=%d",
             GATE_END_OPEN_GPIO,
             GATE_END_CLOSE_GPIO,
             GATE_OBSTACLE_GPIO,
             GATE_LOCAL_BUTTON_GPIO);

    return ESP_OK;
}

static esp_err_t gate_keeper_validate_command(gate_keeper_command_t command)
{
    gate_keeper_status_t status = gate_keeper_get_status();

    switch (command) {
        case GATE_KEEPER_COMMAND_OPEN:
            if (status == GATE_KEEPER_STATUS_OPEN ||
                status == GATE_KEEPER_STATUS_OPENING ||
                status == GATE_KEEPER_STATUS_CLOSING) {
                return ESP_ERR_INVALID_STATE;
            }
            break;
        case GATE_KEEPER_COMMAND_CLOSE:
            if (status == GATE_KEEPER_STATUS_CLOSED ||
                status == GATE_KEEPER_STATUS_CLOSING ||
                status == GATE_KEEPER_STATUS_OPENING) {
                return ESP_ERR_INVALID_STATE;
            }
            break;
        case GATE_KEEPER_COMMAND_STOP:
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t gate_keeper_send_command(gate_keeper_command_t command)
{
    if (s_gate_command_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(gate_keeper_validate_command(command), TAG, "Command rejected");

    if (xQueueSend(s_gate_command_queue, &command, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

gate_keeper_status_t gate_keeper_get_status(void)
{
    return s_gate_status;
}

gate_keeper_fault_t gate_keeper_get_fault(void)
{
    return s_gate_fault;
}

const char *gate_keeper_get_fault_string(void)
{
    return gate_fault_to_string(s_gate_fault);
}

const char *gate_keeper_get_fault_message(void)
{
    return gate_fault_to_message(s_gate_fault);
}
