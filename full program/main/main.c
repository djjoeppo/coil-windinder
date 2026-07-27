#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"

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
static bool inches_mode = false;

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

static void telemetry_task(void *pvParameters) {
    while (1) {
        float weight = hx711_get_weight();
        float turns = (float)motion_get_position(3) / 1280.0f; // 1280 steps per full rotation of A-axis
        char msg[48];
        int len = snprintf(msg, sizeof(msg), "W:%.3f T:%.2f\n", weight, turns);
        uart_write_bytes(UART_NUM, msg, len);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
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
        .dt_pin = CONFIG_HX711_DOUT_GPIO,
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

    // Initialize the physical button pin (GPIO 18) with internal pullup enabled
    gpio_config_t button_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << CONFIG_BUTTON_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1
    };
    gpio_config(&button_conf);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 2. Start Background Tasks
    // HX711 Update on Core 0
    xTaskCreatePinnedToCore(hx711_update_task, "hx711_task", 4096, NULL, 5, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Telemetry task on Core 0
    xTaskCreatePinnedToCore(telemetry_task, "telemetry_task", 2048, NULL, 4, NULL, 0);
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
                                        // Wait for previous movements or homing to complete
                                        while (!motion_is_idle()) {
                                            vTaskDelay(pdMS_TO_TICKS(10));
                                        }
                                        if (force_mode && cmd.has_z) {
                                            motion_start_force_move(cmd.z);
                                            planned_position[2] = motion_get_position(2);
                                            uart_write_bytes(UART_NUM, "ok\n", 3);
                                        } else {
                                            float scale = inches_mode ? 25.4f : 1.0f;
                                            move_t move = {0};
                                            long target_steps[4];

                                            if (absolute_mode) {
                                                target_steps[0] = cmd.has_x ? (long)(cmd.x * scale * X_STEPS_PER_MM)  : planned_position[0];
                                                target_steps[1] = cmd.has_y ? (long)(cmd.y * scale * Y_STEPS_PER_MM)  : planned_position[1];
                                                target_steps[2] = cmd.has_z ? (long)(cmd.z * scale * Z_STEPS_PER_MM)  : planned_position[2];
                                                target_steps[3] = cmd.has_a ? (long)(cmd.a * A_STEPS_PER_DEG) : planned_position[3];
                                            } else {
                                                target_steps[0] = cmd.has_x ? (planned_position[0] + (long)(cmd.x * scale * X_STEPS_PER_MM))  : planned_position[0];
                                                target_steps[1] = cmd.has_y ? (planned_position[1] + (long)(cmd.y * scale * Y_STEPS_PER_MM))  : planned_position[1];
                                                target_steps[2] = cmd.has_z ? (planned_position[2] + (long)(cmd.z * scale * Z_STEPS_PER_MM))  : planned_position[2];
                                                target_steps[3] = cmd.has_a ? (planned_position[3] + (long)(cmd.a * A_STEPS_PER_DEG)) : planned_position[3];
                                            }

                                            // Enforce soft limits
                                            if (target_steps[0] < 0) target_steps[0] = 0;
                                            if (target_steps[0] > (long)(X_MAX_TRAVEL * X_STEPS_PER_MM)) {
                                                target_steps[0] = (long)(X_MAX_TRAVEL * X_STEPS_PER_MM);
                                            }

                                            if (target_steps[1] < 0) target_steps[1] = 0;
                                            if (target_steps[1] > (long)(Y_MAX_TRAVEL * Y_STEPS_PER_MM)) {
                                                target_steps[1] = (long)(Y_MAX_TRAVEL * Y_STEPS_PER_MM);
                                            }

                                            if (target_steps[2] < 0) target_steps[2] = 0;
                                            if (target_steps[2] > (long)(Z_MAX_TRAVEL * Z_STEPS_PER_MM)) {
                                                target_steps[2] = (long)(Z_MAX_TRAVEL * Z_STEPS_PER_MM);
                                            }

                                            for (int i = 0; i < 4; i++) {
                                                move.steps[i] = target_steps[i] - planned_position[i];
                                            }
                                            
                                            move.feed = cmd.has_f ? (cmd.f * scale) : DEFAULT_FEED;

                                            for(int i=0; i<4; i++) planned_position[i] = target_steps[i];

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
                                        // G20: Inch modus
                                        inches_mode = true;
                                        uart_write_bytes(UART_NUM, "Modus: Inches actief\nok\n", 28);
                                        break;
                                    }
                                    case 21: {
                                        // G21: Millimeter modus (Standaard)
                                        inches_mode = false;
                                        uart_write_bytes(UART_NUM, "Modus: Millimeters actief\nok\n", 29); 
                                        break;
                                    }
                                    case 28: {
                                        // Wait for previous movements to complete
                                        while (!motion_is_idle()) {
                                            vTaskDelay(pdMS_TO_TICKS(10));
                                        }
                                        // Start de homing procedure
                                        motion_start_homing(); 
                                        
                                        // Geef de achtergrondtaak heel even de tijd om de motoren te starten
                                        vTaskDelay(pdMS_TO_TICKS(150));
                                        
                                        // Wacht zolang de machine nog bezig is, maar lees ondertussen de UART voor noodstops!
                                        while (!motion_is_idle()) {
                                            int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, 10 / portTICK_PERIOD_MS);
                                            if (len > 0) {
                                                for (int i = 0; i < len; i++) {
                                                    char c = data[i];
                                                    if (c == '\n' || c == '\r') {
                                                        line[line_idx] = '\0';
                                                        if (line_idx > 0) {
                                                            gcode_command_t e_cmd;
                                                            if (gcode_parse_line(line, &e_cmd) && e_cmd.command == 'M' && (e_cmd.code == 112 || e_cmd.code == 410)) {
                                                                motion_stop();
                                                                uart_write_bytes(UART_NUM, "Emergency Stop Activated! Homing Aborted.\nok\n", 47);
                                                                line_idx = 0;
                                                                goto homing_aborted;
                                                            }
                                                        }
                                                        line_idx = 0;
                                                    } else if (line_idx < sizeof(line) - 1) {
                                                        line[line_idx++] = (char)c;
                                                    }
                                                }
                                            }
                                            vTaskDelay(pdMS_TO_TICKS(10));
                                        }

                                        // Nu pas resetten we de posities en geven we het 'ok' signaal
                                        for(int i=0; i<4; i++) planned_position[i] = 0;
                                        uart_write_bytes(UART_NUM, "ok\n", 3); 
                                    homing_aborted:
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
                                    case 119: {
                                        char msg[160];
                                        int l = snprintf(msg, sizeof(msg), "X_limit: %s  Y_limit: %s  Z_limit: %s  A_limit: %s\nok\n",
                                            motion_get_limit_triggered(0) ? "TRIGGERED" : "open",
                                            motion_get_limit_triggered(1) ? "TRIGGERED" : "open",
                                            motion_get_limit_triggered(2) ? "TRIGGERED" : "open",
                                            motion_get_limit_triggered(3) ? "TRIGGERED" : "open");
                                        uart_write_bytes(UART_NUM, msg, l);
                                        break;
                                    }
                                }
                            } else if (cmd.command == 'M') {
                                switch(cmd.code) {
                                    case 0: {
                                        // M0: Pauzeer en wacht tot de fysieke knop op GPIO 18 wordt ingedrukt
                                        uart_write_bytes(UART_NUM, "Wachten op knopindruk...\n", 25);

                                        // Wacht eerst tot alle lopende motorbewegingen voltooid zijn
                                        while (!motion_is_idle()) {
                                            vTaskDelay(pdMS_TO_TICKS(10));
                                        }

                                        // Wacht tot de knop is ingedrukt (GPIO 18 gaat LOW)
                                        while (gpio_get_level(CONFIG_BUTTON_GPIO) == 1) {
                                            vTaskDelay(pdMS_TO_TICKS(50));
                                        }

                                        // Wacht tot de knop is losgelaten (GPIO 18 gaat terug HIGH) om dubbel-trigger te voorkomen
                                        while (gpio_get_level(CONFIG_BUTTON_GPIO) == 0) {
                                            vTaskDelay(pdMS_TO_TICKS(50));
                                        }

                                        uart_write_bytes(UART_NUM, "Knop ingedrukt! Hervat programma.\nok\n", 38);
                                        break;
                                    }
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
                                        int l = snprintf(msg, sizeof(msg), "X:%.2f Y:%.2f Z:%.2f A:%.2f LOADCELL: %.3f kg\nok\n", 
                                            (float)motion_get_position(0)/X_STEPS_PER_MM,
                                            (float)motion_get_position(1)/Y_STEPS_PER_MM,
                                            (float)motion_get_position(2)/Z_STEPS_PER_MM,
                                            (float)motion_get_position(3)/A_STEPS_PER_DEG,
                                            weight);
                                        uart_write_bytes(UART_NUM, msg, l);
                                        break;
                                    }
                                    case 112:
                                    case 410: {
                                        motion_stop();
                                        uart_write_bytes(UART_NUM, "Emergency Stop Activated! System Halted.\nok\n", 45);
                                        break;
                                    }
                                    case 400: {
                                        while (!motion_is_idle()) {
                                            vTaskDelay(pdMS_TO_TICKS(10));
                                        }
                                        force_mode = false;
                                        uart_write_bytes(UART_NUM, "ok\n", 3);
                                        break;
                                    }
                                    case 401: {
                                        while (!motion_is_idle()) {
                                            vTaskDelay(pdMS_TO_TICKS(10));
                                        }
                                        force_mode = true;
                                        uart_write_bytes(UART_NUM, "ok\n", 3);
                                        break;
                                    }
                                    case 402: {
                                        while (!motion_is_idle()) {
                                            vTaskDelay(pdMS_TO_TICKS(10));
                                        }
                                        hx711_tare(15);
                                        uart_write_bytes(UART_NUM, "ok\n", 3);
                                        break;
                                    }
                                    case 403: {
                                        if (cmd.has_z && cmd.z > 0.0f) {
                                            motion_set_tension_lock(true, cmd.z);
                                            char msg[64];
                                            int len = snprintf(msg, sizeof(msg), "Tension Lock active target: %.2f kg\nok\n", cmd.z);
                                            uart_write_bytes(UART_NUM, msg, len);
                                        } else {
                                            motion_set_tension_lock(false, 0.0f);
                                            uart_write_bytes(UART_NUM, "Tension Lock disabled\nok\n", 26);
                                        }
                                        break;
                                    }
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