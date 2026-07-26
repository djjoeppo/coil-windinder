#ifndef STEPPER_H
#define STEPPER_H

#include <stdbool.h>

void stepper_init_axis(int axis_idx, int step_gpio, int dir_gpio, bool invert_dir);
void stepper_set_direction(int axis_idx, bool positive);

#endif // STEPPER_H
