#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"

#include "machine_settings.h"
#include "hx711.h"
#include "stepper.h"
#include "gcode_parser.h"
#include "motion_control.h"

static const char *TAG = "MAIN";

#define UART_NUM UART_NUM_0
#define BUF_SIZE 1024

static bool absolute_mode = true;
static bool force_mode = false;

// Virtual position to track planned movements in the buffer
static long planned_position[4] = {0, 0, 0, 0};

void uart_init() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting Windmachine ESP32-S3 Firmware...");

    uart_init();

    hx711_config_t hx_cfg = {
        .sck_pin = CONFIG_HX711_SCK_GPIO,
        .dt_pin = CONFIG_HX711_DT_GPIO,
        .offset = 0,
        .divider = 186.5f
    };
    hx711_init(&hx_cfg);

    vTaskDelay(pdMS_TO_TICKS(500));
    hx711_tare(15);

    stepper_init_axis(0, CONFIG_X_STEP_GPIO, CONFIG_X_DIR_GPIO, X_INVERT_DIR);
    stepper_init_axis(1, CONFIG_Y_STEP_GPIO, CONFIG_Y_DIR_GPIO, Y_INVERT_DIR);
    stepper_init_axis(2, CONFIG_Z_STEP_GPIO, CONFIG_Z_DIR_GPIO, Z_INVERT_DIR);
    stepper_init_axis(3, CONFIG_A_STEP_GPIO, CONFIG_A_DIR_GPIO, A_INVERT_DIR);

    xTaskCreatePinnedToCore(hx711_update_task, "hx711_task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(motion_control_task, "motion_task", 8192, NULL, 20, NULL, 1);

    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    char line[128];
    int line_idx = 0;

    printf("\n--- Windmachine ESP32 READY ---\n");

    while (1) {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
        for (int i = 0; i < len; i++) {
            char c = data[i];
            if (c == '\n' || c == '\r') {
                line[line_idx] = '\0';
                if (line_idx > 0) {
                    gcode_command_t cmd;
                    if (gcode_parse_line(line, &cmd)) {
                        if (cmd.command == 'G') {
                            switch(cmd.code) {
                                case 0:
                                case 1: {
                                    if (force_mode && cmd.has_z) {
                                        motion_start_force_move(cmd.z);
                                        planned_position[2] = motion_get_position(2);
                                        uart_write_bytes(UART_NUM, "ok\n", 3);
                                    } else {
                                        move_t move = {0};
                                        long target_steps[4];

                                        if (absolute_mode) {
                                            target_steps[0] = cmd.has_x ? (long)(cmd.x * X_STEPS_PER_MM)  : planned_position[0];
                                            target_steps[1] = cmd.has_y ? (long)(cmd.y * Y_STEPS_PER_MM)  : planned_position[1];
                                            target_steps[2] = cmd.has_z ? (long)(cmd.z * Z_STEPS_PER_MM)  : planned_position[2];
                                            target_steps[3] = cmd.has_a ? (long)(cmd.a * A_STEPS_PER_DEG) : planned_position[3];

                                            for(int i=0; i<4; i++) {
                                                move.steps[i] = target_steps[i] - planned_position[i];
                                            }
                                        } else {
                                            move.steps[0] = cmd.has_x ? (long)(cmd.x * X_STEPS_PER_MM)  : 0;
                                            move.steps[1] = cmd.has_y ? (long)(cmd.y * Y_STEPS_PER_MM)  : 0;
                                            move.steps[2] = cmd.has_z ? (long)(cmd.z * Z_STEPS_PER_MM)  : 0;
                                            move.steps[3] = cmd.has_a ? (long)(cmd.a * A_STEPS_PER_DEG) : 0;
                                        }

                                        move.feed = cmd.has_f ? cmd.f : DEFAULT_FEED;

                                        // Update planned position BEFORE adding to buffer
                                        for(int i=0; i<4; i++) planned_position[i] += move.steps[i];

                                        if (!motion_add_move(&move)) {
                                            // Revert planned position if buffer is full
                                            for(int i=0; i<4; i++) planned_position[i] -= move.steps[i];
                                            uart_write_bytes(UART_NUM, "ERROR: Buffer vol!\n", 19);
                                        } else {
                                            uart_write_bytes(UART_NUM, "ok\n", 3);
                                        }
                                    }
                                    break;
                                }
                                case 28:
                                    motion_start_homing();
                                    for(int i=0; i<4; i++) planned_position[i] = 0;
                                    uart_write_bytes(UART_NUM, "ok\n", 3);
                                    break;
                                case 90: absolute_mode = true; uart_write_bytes(UART_NUM, "ok\n", 3); break;
                                case 91: absolute_mode = false; uart_write_bytes(UART_NUM, "ok\n", 3); break;
                                default: {
                                    char msg[32];
                                    int l = snprintf(msg, sizeof(msg), "Unknown G%d\n", cmd.code);
                                    uart_write_bytes(UART_NUM, msg, l);
                                    break;
                                }
                            }
                        } else if (cmd.command == 'M') {
                            switch(cmd.code) {
                                case 114: {
                                    char msg[128];
                                    float weight = hx711_get_weight();
                                    int l = snprintf(msg, sizeof(msg), "X:%.2f Y:%.2f Z:%.2f A:%.2f LOADCELL: %.3f kg\nok\n",
                                        (float)motion_get_position(0)/X_STEPS_PER_MM,
                                        (float)motion_get_position(1)/Y_STEPS_PER_MM,
                                        (float)motion_get_position(2)/Z_STEPS_PER_MM,
                                        (float)motion_get_position(3)/A_STEPS_PER_DEG,
                                        weight);
                                    uart_write_bytes(UART_NUM, msg, l);
                                    break;
                                }
                                case 400: force_mode = false; uart_write_bytes(UART_NUM, "ok\n", 3); break;
                                case 401: force_mode = true; uart_write_bytes(UART_NUM, "ok\n", 3); break;
                                case 402: hx711_tare(15); uart_write_bytes(UART_NUM, "ok\n", 3); break;
                                default: {
                                    char msg[32];
                                    int l = snprintf(msg, sizeof(msg), "Unknown M%d\n", cmd.code);
                                    uart_write_bytes(UART_NUM, msg, l);
                                    break;
                                }
                            }
                        }
                    } else {
                        uart_write_bytes(UART_NUM, "Parse error\n", 12);
                    }
                }
                line_idx = 0;
            } else if (line_idx < sizeof(line) - 1) {
                line[line_idx++] = (char)c;
            }
        }
    }
}
