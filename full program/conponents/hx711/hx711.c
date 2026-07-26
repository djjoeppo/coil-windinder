#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "hx711.h"
#include <math.h>

static hx711_config_t hx_cfg;
static volatile float filtered_weight = 0.0f;
static volatile bool is_online = false;
static int64_t last_read_time = 0;

// Mutex for hardware access to prevent overlapping bit-bangs
static SemaphoreHandle_t hx711_mutex = NULL;

// Spinlock for thread-safe access to shared variables
static portMUX_TYPE hx711_vars_mux = portMUX_INITIALIZER_UNLOCKED;

// Loadcell Multi-Point Calibration Profile
typedef struct {
    float raw_kg;       // Raw calculated weight (using standard divider)
    float actual_kg;    // Actual calibrated weight measured with reference weights
} cal_point_t;

#define CAL_POINTS_NUM 5
static const cal_point_t cal_table[CAL_POINTS_NUM] = {
    { 0.0f,   0.0f },
    { 5.0f,   5.0f },
    { 10.0f,  10.02f },  // Example of micro-corrections for loadcell non-linearity
    { 20.0f,  19.95f },
    { 30.0f,  30.0f }
};

static float hx711_apply_calibration(float raw_kg) {
    if (raw_kg <= cal_table[0].raw_kg) {
        return raw_kg; // Below first point, return raw
    }
    if (raw_kg >= cal_table[CAL_POINTS_NUM - 1].raw_kg) {
        // Extrapolate above last point
        int last = CAL_POINTS_NUM - 1;
        float ratio = (raw_kg - cal_table[last - 1].raw_kg) / (cal_table[last].raw_kg - cal_table[last - 1].raw_kg);
        return cal_table[last - 1].actual_kg + ratio * (cal_table[last].actual_kg - cal_table[last - 1].actual_kg);
    }

    // Find the interval
    for (int i = 0; i < CAL_POINTS_NUM - 1; i++) {
        if (raw_kg >= cal_table[i].raw_kg && raw_kg <= cal_table[i + 1].raw_kg) {
            float ratio = (raw_kg - cal_table[i].raw_kg) / (cal_table[i + 1].raw_kg - cal_table[i].raw_kg);
            return cal_table[i].actual_kg + ratio * (cal_table[i + 1].actual_kg - cal_table[i].actual_kg);
        }
    }
    return raw_kg;
}

void hx711_init(hx711_config_t *config) {
    hx_cfg = *config;

    if (hx711_mutex == NULL) {
        hx711_mutex = xSemaphoreCreateMutex();
    }

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << hx_cfg.sck_pin),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << hx_cfg.dt_pin);
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_set_level(hx_cfg.sck_pin, 0);
}

long hx711_read_raw(void) {
    if (xSemaphoreTake(hx711_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return 0; // Hardware busy
    }

    int timeout = 10; // Reduced from 1000 to prevent system lag when disconnected
    while (gpio_get_level(hx_cfg.dt_pin)) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (--timeout == 0) {
            is_online = false;
            xSemaphoreGive(hx711_mutex);
            return 0;
        }
    }

    uint32_t value = 0;
    for (int i = 0; i < 24; i++) {
        gpio_set_level(hx_cfg.sck_pin, 1);
        esp_rom_delay_us(1);
        value <<= 1;
        if (gpio_get_level(hx_cfg.dt_pin)) {
            value++;
        }
        gpio_set_level(hx_cfg.sck_pin, 0);
        esp_rom_delay_us(1);
    }

    // Gain 128 pulse
    gpio_set_level(hx_cfg.sck_pin, 1);
    esp_rom_delay_us(1);
    gpio_set_level(hx_cfg.sck_pin, 0);
    esp_rom_delay_us(1);

    xSemaphoreGive(hx711_mutex);

    if (value & 0x800000) {
        value |= 0xFF000000;
    }

    is_online = true;
    last_read_time = esp_timer_get_time();
    return (long)value;
}

void hx711_tare(int samples) {
    long sum = 0;
    int valid = 0;
    for (int i = 0; i < samples; i++) {
        long raw = hx711_read_raw();
        if (raw != 0) {
            sum += raw;
            valid++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (valid > 0) {
        hx_cfg.offset = sum / valid;
    }
    
    portENTER_CRITICAL(&hx711_vars_mux);
    filtered_weight = 0.0f;
    portEXIT_CRITICAL(&hx711_vars_mux);
}

float hx711_get_weight(void) {
    if ((esp_timer_get_time() - last_read_time) > 500000) {
        is_online = false;
        return 0.0f;
    }
    
    float w;
    portENTER_CRITICAL(&hx711_vars_mux);
    w = filtered_weight;
    portEXIT_CRITICAL(&hx711_vars_mux);

    if (fabs(w) < 0.05f) return 0.0f; // Deadband filter around 0
    return w;
}

bool hx711_is_online(void) {
    return is_online;
}

void hx711_update_task(void *pvParameters) {
    static float prev_weight = 0;
    while(1) {
        // Hardware check without holding mutex yet
        if (gpio_get_level(hx_cfg.dt_pin) == 0) {
            long raw = hx711_read_raw();
            if (raw != 0 && raw != -8388608) {
                float current = (float)(hx_cfg.offset - raw) / hx_cfg.divider;
                
                // Apply Multi-Point Calibration Profile
                current = hx711_apply_calibration(current);

                if (fabs(current - prev_weight) < 15.0f) {
                     portENTER_CRITICAL(&hx711_vars_mux);
                     filtered_weight = (current * 0.25f) + (filtered_weight * 0.75f);
                     portEXIT_CRITICAL(&hx711_vars_mux);
                }
                prev_weight = current;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
