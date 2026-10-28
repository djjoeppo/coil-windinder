#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "../full program/conponents/gcode_parser/include/gcode_parser.h"

static bool float_eq(float a, float b) {
    return fabsf(a - b) < 0.00001f;
}

int main(void) {
    printf("Running G-Code Parser Unit Tests & Performance Benchmark...\n");

    struct TestCase {
        const char *line;
        bool expected_success;
        char expected_cmd;
        int expected_code;
        bool has_x; float x;
        bool has_y; float y;
        bool has_z; float z;
        bool has_a; float a;
        bool has_f; float f;
    } test_cases[] = {
        {"G1 X10.5 Y-20.25 Z0.0 A90 F1200", true, 'G', 1, true, 10.5f, true, -20.25f, true, 0.0f, true, 90.0f, true, 1200.0f},
        {"g0 x0 y0 z10", true, 'G', 0, true, 0.0f, true, 0.0f, true, 10.0f, false, 0.0f, false, 0.0f},
        {"M114", true, 'M', 114, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f},
        {"  G28  ", true, 'G', 28, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f},
        {"m402", true, 'M', 402, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f},
        {"G1 X+123.456 Y-78.90 Z.123 A45 F1500.0", true, 'G', 1, true, 123.456f, true, -78.90f, true, 0.123f, true, 45.0f, true, 1500.0f},
        {"  G1 (inline comment)  X = 10.0 (arg comment)  Y = -20.0  ; comment", true, 'G', 1, true, 10.0f, true, -20.0f, false, 0.0f, false, 0.0f, false, 0.0f},
        {"(Full line comment) ", false, 0, 0, false, 0, false, 0, false, 0, false, 0, false, 0},
        {"; full line comment", false, 0, 0, false, 0, false, 0, false, 0, false, 0, false, 0},
        {"", false, 0, 0, false, 0, false, 0, false, 0, false, 0, false, 0},
        {"   ", false, 0, 0, false, 0, false, 0, false, 0, false, 0, false, 0},
        {"INVALID COMMAND", false, 0, 0, false, 0, false, 0, false, 0, false, 0, false, 0}
    };

    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    int passed = 0;

    for (int i = 0; i < num_tests; i++) {
        gcode_command_t cmd;
        bool res = gcode_parse_line(test_cases[i].line, &cmd);

        if (res != test_cases[i].expected_success) {
            printf("[FAIL] Case %d ('%s'): expected_res=%d, got=%d\n", i, test_cases[i].line, test_cases[i].expected_success, res);
            continue;
        }

        if (res) {
            if (cmd.command != test_cases[i].expected_cmd ||
                cmd.code != test_cases[i].expected_code ||
                cmd.has_x != test_cases[i].has_x || (cmd.has_x && !float_eq(cmd.x, test_cases[i].x)) ||
                cmd.has_y != test_cases[i].has_y || (cmd.has_y && !float_eq(cmd.y, test_cases[i].y)) ||
                cmd.has_z != test_cases[i].has_z || (cmd.has_z && !float_eq(cmd.z, test_cases[i].z)) ||
                cmd.has_a != test_cases[i].has_a || (cmd.has_a && !float_eq(cmd.a, test_cases[i].a)) ||
                cmd.has_f != test_cases[i].has_f || (cmd.has_f && !float_eq(cmd.f, test_cases[i].f))) {
                printf("[FAIL] Case %d ('%s'): parameter mismatch\n", i, test_cases[i].line);
                printf("  Got: cmd=%c %d X:%.3f(%d) Y:%.3f(%d) Z:%.3f(%d) A:%.3f(%d) F:%.3f(%d)\n",
                       cmd.command, cmd.code, cmd.x, cmd.has_x, cmd.y, cmd.has_y, cmd.z, cmd.has_z, cmd.a, cmd.has_a, cmd.f, cmd.has_f);
                continue;
            }
        }
        passed++;
    }

    printf("Unit tests result: %d / %d passed!\n", passed, num_tests);

    if (passed == num_tests) {
        // Benchmark
        const char *sample_line = "G1 X123.456 Y-78.90 Z0.123 A45.0 F1500.0";
        gcode_command_t cmd;
        int iterations = 2000000;

        clock_t start = clock();
        for (int i = 0; i < iterations; i++) {
            gcode_parse_line(sample_line, &cmd);
        }
        clock_t end = clock();

        double elapsed_sec = (double)(end - start) / CLOCKS_PER_SEC;
        double ns_per_op = (elapsed_sec / iterations) * 1e9;

        printf("Benchmark: %d parses in %.4f sec (%.2f ns/op, %.2f ops/sec)\n",
               iterations, elapsed_sec, ns_per_op, iterations / elapsed_sec);
        return 0;
    }

    return 1;
}
