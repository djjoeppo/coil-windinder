#include "stepper.h"
#include "driver/gpio.h"

typedef struct {
    int dir_gpio;
    bool invert_dir;
} stepper_ctx_t;

static stepper_ctx_t steppers[4];

void stepper_init_axis(int axis_idx, int step_gpio, int dir_gpio, bool invert_dir) {
    if (axis_idx < 0 || axis_idx >= 4) return;

    steppers[axis_idx].dir_gpio = dir_gpio;
    steppers[axis_idx].invert_dir = invert_dir;

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << step_gpio) | (1ULL << dir_gpio),
    };
    gpio_config(&io_conf);
}

void stepper_set_direction(int axis_idx, bool positive) {
    if (axis_idx < 0 || axis_idx >= 4) return;
    bool level = positive;
    if (steppers[axis_idx].invert_dir) level = !level;
    gpio_set_level(steppers[axis_idx].dir_gpio, level);
}
