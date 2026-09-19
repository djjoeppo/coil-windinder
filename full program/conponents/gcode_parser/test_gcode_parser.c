#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "gcode_parser.h"

int main(void) {
    printf("Running gcode_parser unit test and benchmark...\n");

    struct test_case {
        const char* input;
        bool expected_res;
        char expected_cmd;
        int expected_code;
        bool has_x; float x;
        bool has_y; float y;
        bool has_z; float z;
        bool has_a; float a;
        bool has_f; float f;
    } tests[] = {
        { "G1 X10.5 Y20.3 Z-5.0 A180.0 F1200", true, 'G', 1, true, 10.5f, true, 20.3f, true, -5.0f, true, 180.0f, true, 1200.0f },
        { "G0 X0 Y0", true, 'G', 0, true, 0.0f, true, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f },
        { "M114", true, 'M', 114, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f },
        { "G28", true, 'G', 28, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f },
        { "  g1  x 10.5   y -20.3  f 500.5  ", true, 'G', 1, true, 10.5f, true, -20.3f, false, 0.0f, false, 0.0f, true, 500.5f },
        { "M402", true, 'M', 402, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f },
        { "G1 X-12.345 Y+67.89 Z0.001 A180 F300.5", true, 'G', 1, true, -12.345f, true, 67.89f, true, 0.001f, true, 180.0f, true, 300.5f },
        { "G1 X.5 Y-.25 Z+.75", true, 'G', 1, true, 0.5f, true, -0.25f, true, 0.75f, false, 0.0f, false, 0.0f },
        { "G1 X10.5 Y20.3 ; line comment", true, 'G', 1, true, 10.5f, true, 20.3f, false, 0.0f, false, 0.0f, false, 0.0f },
        { "; comment line", false, '\0', 0, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f },
        { "", false, '\0', 0, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f, false, 0.0f },
    };

    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;

    for (int i = 0; i < num_tests; i++) {
        gcode_command_t cmd = {0};
        bool res = gcode_parse_line(tests[i].input, &cmd);

        if (res != tests[i].expected_res) {
            printf("FAIL test %d: expected result %d got %d\n", i, tests[i].expected_res, res);
            return 1;
        }

        if (res) {
            if (cmd.command != tests[i].expected_cmd || cmd.code != tests[i].expected_code ||
                cmd.has_x != tests[i].has_x || (tests[i].has_x && fabs(cmd.x - tests[i].x) > 1e-4f) ||
                cmd.has_y != tests[i].has_y || (tests[i].has_y && fabs(cmd.y - tests[i].y) > 1e-4f) ||
                cmd.has_z != tests[i].has_z || (tests[i].has_z && fabs(cmd.z - tests[i].z) > 1e-4f) ||
                cmd.has_a != tests[i].has_a || (tests[i].has_a && fabs(cmd.a - tests[i].a) > 1e-4f) ||
                cmd.has_f != tests[i].has_f || (tests[i].has_f && fabs(cmd.f - tests[i].f) > 1e-4f)) {
                printf("FAIL test %d ('%s'): Mismatch in parsed fields!\n", i, tests[i].input);
                printf("Got: cmd=%c code=%d X=(%d,%.3f) Y=(%d,%.3f) Z=(%d,%.3f) A=(%d,%.3f) F=(%d,%.3f)\n",
                       cmd.command, cmd.code, cmd.has_x, cmd.x, cmd.has_y, cmd.y, cmd.has_z, cmd.z, cmd.has_a, cmd.a, cmd.has_f, cmd.f);
                return 1;
            }
        }
        passed++;
    }

    printf("ALL %d CORRECTNESS TESTS PASSED SUCCESSFULLY!\n", passed);

    // Benchmark execution
    const int iterations = 1000000;
    gcode_command_t dummy;
    clock_t start = clock();
    for (int k = 0; k < iterations; k++) {
        gcode_parse_line(tests[k % 8].input, &dummy);
    }
    clock_t end = clock();
    double time_taken = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Benchmark: %d lines parsed in %.4f seconds (%.2f lines/sec)\n",
           iterations, time_taken, iterations / time_taken);

    return 0;
}
