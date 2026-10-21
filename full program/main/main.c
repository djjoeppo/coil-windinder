#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
    // Install driver with longer timeout and check for errors
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
}

void app_main(void) {
    // 0. Delay and log to give developer time and avoid early WDT trip
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Starting Windmachine Classic ESP32 Firmware...");

    // 1. Hardware Init
    uart_init();
    vTaskDelay(pdMS_TO_TICKS(50));

    hx711_config_t hx_cfg = {
        .sck_pin = CONFIG_HX711_SCK_GPIO,
        .dt_pin = CONFIG_HX711_DT_GPIO,
        .offset = 0,
        .divider = 34355.26f
    };
    hx711_init(&hx_cfg);
    
    // Initial tare with delay to allow sensor power-up
    vTaskDelay(pdMS_TO_TICKS(500));
    hx711_tare(15);
    vTaskDelay(pdMS_TO_TICKS(50));

    stepper_init_axis(0, CONFIG_X_STEP_GPIO, CONFIG_X_DIR_GPIO, X_INVERT_DIR);
    stepper_init_axis(1, CONFIG_Y_STEP_GPIO, CONFIG_Y_DIR_GPIO, Y_INVERT_DIR);
    stepper_init_axis(2, CONFIG_Z_STEP_GPIO, CONFIG_Z_DIR_GPIO, Z_INVERT_DIR);
    stepper_init_axis(3, CONFIG_A_STEP_GPIO, CONFIG_A_DIR_GPIO, A_INVERT_DIR);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 2. Start Background Tasks
    // HX711 Update on Core 0
    xTaskCreatePinnedToCore(hx711_update_task, "hx711_task", 4096, NULL, 5, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Motion task on Core 1
    xTaskCreatePinnedToCore(motion_control_task, "motion_task", 8192, NULL, 20, NULL, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
    char line[128];
    int line_idx = 0;

    printf("\n--- Windmachine ESP32 READY ---\n");

    while (1) {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, 10 / portTICK_PERIOD_MS);
        if (len > 0) {
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

                                            for(int i=0; i<4; i++) planned_position[i] += move.steps[i];

                                            // === HOOGWAARDIGE STREAMING LOGICA ===
                                            // Als de buffer vol zit, wachten we hier net zo lang tot er door een 
                                            // voltooide motorbeweging weer een plekje vrijkomt.
                                            while (!motion_add_move(&move)) {
                                                vTaskDelay(pdMS_TO_TICKS(10)); // Wacht 10ms en probeer opnieuw
                                            }

                                            // Beweging met succes toegevoegd! Nu mag de PC de volgende sturen.
                                            uart_write_bytes(UART_NUM, "ok\n", 3);
                                        }
                                        break;
                                    }
                                    case 4: {
                                        // G4: Wachten (Dwell) op basis van de F parameter (in milliseconden)
                                        int wait_ms = cmd.has_f ? (int)cmd.f : 1000; 
                                        vTaskDelay(pdMS_TO_TICKS(wait_ms));
                                        uart_write_bytes(UART_NUM, "ok\n", 3);
                                        break;
                                    }
                                    case 20: {
                                        // G20: Inch modus (We accepteren de code maar blijven in MM werken)
                                        uart_write_bytes(UART_NUM, "WARNING: Inches niet ondersteund, blijft in MM!\nok\n", 51); 
                                        break;
                                    }
                                    case 21: {
                                        // G21: Millimeter modus (Standaard)
                                        uart_write_bytes(UART_NUM, "Modus: Millimeters actief\nok\n", 29); 
                                        break;
                                    }
                                    case 28: {
                                        // Start de homing procedure
                                        motion_start_homing(); 
                                        
                                        // Geef de achtergrondtaak heel even de tijd om de motoren te starten
                                        vTaskDelay(pdMS_TO_TICKS(150));
                                        
                                        // Wacht zolang de machine nog bezig is (niet idle is) met bewegen/homen
                                        while (!motion_is_idle()) {
                                            vTaskDelay(pdMS_TO_TICKS(50));
                                        }
                                        
                                        // Nu pas resetten we de posities en geven we het 'ok' signaal
                                        for(int i=0; i<4; i++) planned_position[i] = 0;
                                        uart_write_bytes(UART_NUM, "ok\n", 3); 
                                        break;
                                    }
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
                                    case 30: {
                                        // M30: Einde van het programma, herstel standaard modi
                                        absolute_mode = true;
                                        force_mode = false;
                                        uart_write_bytes(UART_NUM, "Programma Beeindigd. Systeem gereset.\nok\n", 41);
                                        break;
                                    }
                                    case 114: {
                                        char msg[128];
                                        float weight = hx711_get_weight();
                                        int l = snprintf(msg, sizeof(msg), "X:%.2f Y:%.2f Z:%.2f A:%.2f W:%.3f\nok\n",
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
        } else {
            // No UART data, give some time to IDLE task to feed WDT
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}