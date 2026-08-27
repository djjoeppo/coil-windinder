#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include "gcode_parser.h"

// Reference (old) implementation for correctness comparison
static float get_value_old(const char* s, char key, bool* found) {
    const char* p = strchr(s, key);
    if (p) {
        *found = true;
        return strtof(p + 1, NULL);
    }
    *found = false;
    return 0.0f;
}

static bool gcode_parse_line_old(const char* line, gcode_command_t* cmd) {
    if (!line || strlen(line) == 0) return false;

    memset(cmd, 0, sizeof(gcode_command_t));

    char clean_line[130];
    int j = 0;
    for (int i = 0; line[i] && j < 128; i++) {
        if (!isspace((int)line[i])) {
            clean_line[j++] = toupper((int)line[i]);
        }
    }
    clean_line[j] = '\0';

    if (clean_line[0] == 'G' || clean_line[0] == 'M') {
        cmd->command = clean_line[0];
        cmd->code = atoi(clean_line + 1);

        cmd->x = get_value_old(clean_line, 'X', &cmd->has_x);
        cmd->y = get_value_old(clean_line, 'Y', &cmd->has_y);
        cmd->z = get_value_old(clean_line, 'Z', &cmd->has_z);
        cmd->a = get_value_old(clean_line, 'A', &cmd->has_a);
        cmd->f = get_value_old(clean_line, 'F', &cmd->has_f);

        return true;
    }

    return false;
}

int main(void) {
    const char* sample_lines[] = {
        "G1 X12.34 Y56.78 Z0.5 A90.0 F1200.0",
        "G0 X0 Y0 Z10",
        "M114",
        "G1 X-10.5 Y+20.25 F300",
        "  g1  x100  y200  z300  a45  f500  ",
        "M402",
        "G90",
        "G28",
        "G1 X0.001 Y-0.005 Z12.3456",
    };
    int num_samples = sizeof(sample_lines) / sizeof(sample_lines[0]);

    printf("Running G-code parser correctness test...\n");
    gcode_command_t cmd_old, cmd_new;
    for (int i = 0; i < num_samples; i++) {
        bool r_old = gcode_parse_line_old(sample_lines[i], &cmd_old);
        bool r_new = gcode_parse_line(sample_lines[i], &cmd_new);

        if (r_old != r_new ||
            cmd_old.command != cmd_new.command ||
            cmd_old.code != cmd_new.code ||
            cmd_old.has_x != cmd_new.has_x || (cmd_old.has_x && fabsf(cmd_old.x - cmd_new.x) > 1e-3f) ||
            cmd_old.has_y != cmd_new.has_y || (cmd_old.has_y && fabsf(cmd_old.y - cmd_new.y) > 1e-3f) ||
            cmd_old.has_z != cmd_new.has_z || (cmd_old.has_z && fabsf(cmd_old.z - cmd_new.z) > 1e-3f) ||
            cmd_old.has_a != cmd_new.has_a || (cmd_old.has_a && fabsf(cmd_old.a - cmd_new.a) > 1e-3f) ||
            cmd_old.has_f != cmd_new.has_f || (cmd_old.has_f && fabsf(cmd_old.f - cmd_new.f) > 1e-3f)) {
            printf("[FAIL] Mismatch on sample %d: '%s'\n", i, sample_lines[i]);
            printf("Old: cmd=%c code=%d X=%.3f(has:%d) Y=%.3f(has:%d) Z=%.3f(has:%d) A=%.3f(has:%d) F=%.3f(has:%d)\n",
                   cmd_old.command, cmd_old.code, cmd_old.x, cmd_old.has_x, cmd_old.y, cmd_old.has_y, cmd_old.z, cmd_old.has_z, cmd_old.a, cmd_old.has_a, cmd_old.f, cmd_old.has_f);
            printf("New: cmd=%c code=%d X=%.3f(has:%d) Y=%.3f(has:%d) Z=%.3f(has:%d) A=%.3f(has:%d) F=%.3f(has:%d)\n",
                   cmd_new.command, cmd_new.code, cmd_new.x, cmd_new.has_x, cmd_new.y, cmd_new.has_y, cmd_new.z, cmd_new.has_z, cmd_new.a, cmd_new.has_a, cmd_new.f, cmd_new.has_f);
            return 1;
        }
    }
    printf("[PASS] All parsing correctness tests passed!\n");

    // Benchmark
    const int N = 1000000;
    printf("\nRunning performance benchmark (%d iterations x %d samples)...\n", N, num_samples);

    clock_t start_old = clock();
    for (int i = 0; i < N; i++) {
        for (int s = 0; s < num_samples; s++) {
            gcode_parse_line_old(sample_lines[s], &cmd_old);
        }
    }
    clock_t end_old = clock();

    clock_t start_new = clock();
    for (int i = 0; i < N; i++) {
        for (int s = 0; s < num_samples; s++) {
            gcode_parse_line(sample_lines[s], &cmd_new);
        }
    }
    clock_t end_new = clock();

    double time_old = (double)(end_old - start_old) / CLOCKS_PER_SEC;
    double time_new = (double)(end_new - start_new) / CLOCKS_PER_SEC;

    printf("Old parser total time: %.4f s\n", time_old);
    printf("New parser total time: %.4f s\n", time_new);
    if (time_new > 0.0) {
        printf("Speedup factor: %.2fx\n", time_old / time_new);
    }

    return 0;
}
