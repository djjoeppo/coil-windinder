#ifndef HX711_H
#define HX711_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int sck_pin;
    int dt_pin;
    long offset;
    float divider;
} hx711_config_t;

void hx711_init(hx711_config_t *config);
long hx711_read_raw(void);
void hx711_tare(int samples);
float hx711_get_weight(void);
float hx711_get_instant_weight(void);
float hx711_get_direct_stable_weight(void);
bool hx711_is_online(void);

// Task for background updates
void hx711_update_task(void *pvParameters);

#endif // HX711_H
