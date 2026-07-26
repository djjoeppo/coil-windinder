#include "motion_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "machine_settings.h"
#include "stepper.h"
#include "hx711.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "MOTION_CONTROL";

typedef enum { IDLE, MOVING, FORCE_SEEKING } motion_state_t;
typedef enum { HOME_IDLE, HOME_FAST_SEEK, HOME_BACKOFF, HOME_SLOW_SEEK } home_state_t;

static RingbufHandle_t move_buffer = NULL;
static volatile motion_state_t state = IDLE;
static volatile home_state_t home_state = HOME_IDLE;
static volatile int homing_axis = -1;

static volatile long current_position[4] = {0, 0, 0, 0};
static volatile long step_count[4] = {0, 0, 0, 0};
static volatile long error_acc[4] = {0, 0, 0, 0};
static volatile int move_dir[4] = {0, 0, 0, 0};
static volatile bool do_step[4] = {false, false, false, false};

static volatile long dominant_steps_total = 0;
static volatile long dominant_steps_remaining = 0;

static float current_speed = 0;
static float target_speed = DEFAULT_FEED;
static float accel = DEFAULT_ACCEL;

static gptimer_handle_t gptimer = NULL;
static volatile bool timer_running = false;
static portMUX_TYPE motion_mux = portMUX_INITIALIZER_UNLOCKED;

static int step_gpios[4] = { CONFIG_X_STEP_GPIO, CONFIG_Y_STEP_GPIO, CONFIG_Z_STEP_GPIO, CONFIG_A_STEP_GPIO };
static int limit_gpios[4] = { CONFIG_X_LIMIT_GPIO, CONFIG_Y_LIMIT_GPIO, CONFIG_Z_LIMIT_GPIO, CONFIG_A_LIMIT_GPIO };
static bool axis_is_nc[4] = { false, false, true, true };

static bool IRAM_ATTR is_limit_triggered(int axis) {
    if (axis < 0 || axis >= 4) return false;
    bool raw = gpio_get_level(limit_gpios[axis]) == 0;
    return axis_is_nc[axis] ? raw : !raw;
}

static bool IRAM_ATTR motion_timer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    if (state != MOVING || dominant_steps_remaining <= 0) {
        state = IDLE;
        return false;
    }

    if (homing_axis >= 0 && home_state != HOME_BACKOFF) {
        if (is_limit_triggered(homing_axis)) {
            state = IDLE;
            dominant_steps_remaining = 0;
            return false;
        }
    }

    static bool phase = false;
    phase = !phase;

    if (phase) {
        for (int i = 0; i < 4; i++) {
            if (do_step[i]) gpio_set_level(step_gpios[i], 1);
        }
    } else {
        for (int i = 0; i < 4; i++) {
            if (do_step[i]) {
                gpio_set_level(step_gpios[i], 0);
                current_position[i] += move_dir[i];
            }
            if (dominant_steps_remaining > 1) {
                error_acc[i] -= step_count[i];
                if (error_acc[i] < 0) {
                    do_step[i] = true;
                    error_acc[i] += dominant_steps_total;
                } else {
                    do_step[i] = false;
                }
            }
        }
        dominant_steps_remaining--;
    }
    return false;
}

void motion_init(void) {
    if (move_buffer == NULL) {
        move_buffer = xRingbufferCreate(sizeof(move_t) * 16, RINGBUF_TYPE_NOSPLIT);
    }

    for (int i = 0; i < 4; i++) {
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = (1ULL << limit_gpios[i]),
            .pull_up_en = 1,
        };
        gpio_config(&io_conf);
    }

    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t cbs = { .on_alarm = motion_timer_cb };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
}

static void motion_update_speed(float speed_hz) {
    if (speed_hz < 10.0f) speed_hz = 10.0f;
    uint64_t alarm_val = 1000000 / (uint64_t)(speed_hz * 2);
    if (alarm_val < 20) alarm_val = 20;

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = alarm_val,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
    
    if (!timer_running) {
        ESP_ERROR_CHECK(gptimer_start(gptimer));
        timer_running = true;
    }
}

void motion_start_move(move_t *move) {
    portENTER_CRITICAL(&motion_mux);
    dominant_steps_total = 0;
    for (int i = 0; i < 4; i++) {
        step_count[i] = labs(move->steps[i]);
        if (step_count[i] > dominant_steps_total) dominant_steps_total = step_count[i];
        move_dir[i] = (move->steps[i] > 0) ? 1 : -1;
        stepper_set_direction(i, move->steps[i] > 0);
        error_acc[i] = dominant_steps_total / 2;
        do_step[i] = (step_count[i] > 0 && error_acc[i] < step_count[i]); 
    }
    if (dominant_steps_total == 0) { 
        state = IDLE; 
        portEXIT_CRITICAL(&motion_mux);
        return; 
    }
    dominant_steps_remaining = dominant_steps_total;
    target_speed = move->feed;
    current_speed = (home_state == HOME_IDLE) ? 0 : target_speed;
    state = MOVING;
    portEXIT_CRITICAL(&motion_mux);
    motion_update_speed(current_speed > 0 ? current_speed : 100);
}

bool motion_add_move(move_t *move) {
    if (move_buffer == NULL) return false;
    return xRingbufferSend(move_buffer, move, sizeof(move_t), pdMS_TO_TICKS(10));
}

static void start_homing_axis(int axis) {
    if (axis < 0) {
        homing_axis = -1; home_state = HOME_IDLE;
        ESP_LOGI(TAG, "Homing complete.");
        return;
    }
    homing_axis = axis;
    move_t m = {0};
    long dir = (axis == 3) ? 1 : -1;
    if (is_limit_triggered(axis)) {
        home_state = HOME_BACKOFF;
        m.steps[axis] = -dir * HOME_BACKOFF_STEPS;
        m.feed = HOME_SLOW_SPEED;
    } else {
        home_state = HOME_FAST_SEEK;
        m.steps[axis] = dir * 1000000;
        m.feed = HOME_FAST_SPEED;
    }
    motion_start_move(&m);
}

void motion_start_homing(void) {
    start_homing_axis(2);
}

long motion_get_position(int axis) {
    if (axis < 0 || axis >= 4) return 0;
    long pos;
    portENTER_CRITICAL(&motion_mux);
    pos = current_position[axis];
    portEXIT_CRITICAL(&motion_mux);
    return pos;
}

bool motion_is_idle(void) {
    return state == IDLE && home_state == HOME_IDLE;
}

void motion_start_force_move(float target_weight_kg) {
    portENTER_CRITICAL(&motion_mux);
    state = FORCE_SEEKING;
    portEXIT_CRITICAL(&motion_mux);

    ESP_LOGI(TAG, "Starting Force Seek to %.2f kg", target_weight_kg);
    
    int stability_count = 0;
    bool slow_phase = false;

    while (stability_count < 20) {
        float current_weight = hx711_get_weight();
        if (!slow_phase && current_weight >= 1.0f) {
            slow_phase = true;
            ESP_LOGI(TAG, "Force Seek: entering slow phase");
        }

        float diff = target_weight_kg - current_weight;
        if (fabs(diff) <= 0.05f) {
            stability_count++;
        } else {
            stability_count = 0;
            int steps = slow_phase ? 5 : 40;
            bool move_down = (diff > 0);
            
            portENTER_CRITICAL(&motion_mux);
            long pos_z = current_position[2];
            portEXIT_CRITICAL(&motion_mux);

            long next_pos = pos_z + (move_down ? steps : -steps);
            if (next_pos < 0 || next_pos > (long)(Z_MAX_TRAVEL * Z_STEPS_PER_MM)) {
                ESP_LOGW(TAG, "Force Seek: Soft limit reached!");
                break;
            }

            stepper_set_direction(2, move_down);
            for(int s=0; s<steps; s++) {
                gpio_set_level(step_gpios[2], 1);
                esp_rom_delay_us(500);
                gpio_set_level(step_gpios[2], 0);
                esp_rom_delay_us(500);
                portENTER_CRITICAL(&motion_mux);
                current_position[2] += (move_down ? 1 : -1);
                portEXIT_CRITICAL(&motion_mux);
            }
        }
        if (slow_phase) vTaskDelay(pdMS_TO_TICKS(50));
        else vTaskDelay(pdMS_TO_TICKS(5));

        if (current_weight > OVERLOAD_THRESHOLD_KG) {
            ESP_LOGE(TAG, "Force Seek: EMERGENCY OVERLOAD!");
            break;
        }
    }
    portENTER_CRITICAL(&motion_mux);
    state = IDLE;
    portEXIT_CRITICAL(&motion_mux);
}

void motion_control_task(void *pvParameters) {
    ESP_LOGI(TAG, "Motion Task started on Core %d", xPortGetCoreID());
    
    motion_init();

    uint64_t last_time = esp_timer_get_time();
    while(1) {
        if (state == FORCE_SEEKING) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        uint64_t now = esp_timer_get_time();
        float dt = (now - last_time) / 1000000.0f;
        last_time = now;

        // VLOEIENDE LOOK-AHEAD LOGICA: 
        // Als we bijna klaar zijn met de huidige beweging (bijv. minder dan 5 stappen over)
        // OF als we al stilstaan (IDLE), én we zijn niet aan het homen:
        if (home_state == HOME_IDLE) {
            bool near_end = (state == MOVING && dominant_steps_remaining < 5);
            bool is_idle = (state == IDLE);
            
            if (is_idle || near_end) {
                size_t item_size;
                // Kijk of er al een volgende beweging klaarstaat in de buffer
                move_t *next_move = (move_t *)xRingbufferReceive(move_buffer, &item_size, 0);
                if (next_move) {
                    float saved_speed = current_speed; // Onthoud de huidige vaart!
                    
                    motion_start_move(next_move);
                    
                    // Schakel de snelheid vloeiend over in plaats van te resetten naar 0
                    portENTER_CRITICAL(&motion_mux);
                    if (saved_speed > 10.0f) {
                        current_speed = saved_speed; 
                    }
                    portEXIT_CRITICAL(&motion_mux);
                    
                    vRingbufferReturnItem(move_buffer, (void *)next_move);
                }
            }
        }

        if (state == MOVING) {
            if (home_state == HOME_IDLE) {
                if (current_speed < target_speed) {
                    current_speed += accel * dt;
                    if (current_speed > target_speed) current_speed = target_speed;
                } else if (current_speed > target_speed) {
                    current_speed -= accel * dt;
                    if (current_speed < target_speed) current_speed = target_speed;
                }
                motion_update_speed(current_speed);
            }
        } else {
            // Dit deel wordt nu alleen nog aangesproken voor de Homing (G28) cyclus
            if (home_state != HOME_IDLE) {
                vTaskDelay(pdMS_TO_TICKS(150));
                if (home_state == HOME_FAST_SEEK) {
                    home_state = HOME_BACKOFF;
                    move_t mb = {0};
                    mb.steps[homing_axis] = (homing_axis == 3 ? -1 : 1) * HOME_BACKOFF_STEPS;
                    mb.feed = HOME_SLOW_SPEED;
                    motion_start_move(&mb);
                } else if (home_state == HOME_BACKOFF) {
                    home_state = HOME_SLOW_SEEK;
                    move_t ms = {0};
                    ms.steps[homing_axis] = (homing_axis == 3 ? 1 : -1) * 100000;
                    ms.feed = HOME_SLOW_SPEED;
                    motion_start_move(&ms);
                } else if (home_state == HOME_SLOW_SEEK) {
                    portENTER_CRITICAL(&motion_mux);
                    current_position[homing_axis] = 0;
                    portEXIT_CRITICAL(&motion_mux);
                    
                    if (homing_axis == 2) {
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        hx711_tare(15);
                        start_homing_axis(1);
                    } else if (homing_axis == 1) start_homing_axis(0);
                    else if (homing_axis == 0) start_homing_axis(3);
                    else start_homing_axis(-1);
                }
            } else {
                // Als er écht niks in de buffer zit en we staan stil, zetten we de timer netjes uit
                if (timer_running) {
                    ESP_ERROR_CHECK(gptimer_stop(gptimer));
                    timer_running = false;
                }
            }
        }

        if (hx711_get_weight() > OVERLOAD_THRESHOLD_KG) {
            portENTER_CRITICAL(&motion_mux);
            state = IDLE;
            portEXIT_CRITICAL(&motion_mux);
            if (timer_running) {
                gptimer_stop(gptimer);
                timer_running = false;
            }
            ESP_LOGE(TAG, "OVERLOAD PROTECT!");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}