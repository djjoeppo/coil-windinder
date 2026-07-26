#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    long steps[4];
    float feed;
} move_t;

void motion_init(void);
bool motion_add_move(move_t *move);
void motion_start_homing(void);
void motion_start_force_move(float target_weight_kg);
void motion_set_absolute(bool absolute);
bool motion_is_idle(void);
long motion_get_position(int axis);

// Task functions
void motion_control_task(void *pvParameters);

#endif // MOTION_CONTROL_H
