#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include "gcode_parser.h"

static void test_gcode_commands(void) {
    gcode_command_t cmd;

    // Test G1 command with all parameters
    bool ok = gcode_parse_line("G1 X12.34 Y56.78 Z10.00 A180.0 F1200", &cmd);
    assert(ok);
    assert(cmd.command == 'G');
    assert(cmd.code == 1);
    assert(cmd.has_x && fabsf(cmd.x - 12.34f) < 0.001f);
    assert(cmd.has_y && fabsf(cmd.y - 56.78f) < 0.001f);
    assert(cmd.has_z && fabsf(cmd.z - 10.00f) < 0.001f);
    assert(cmd.has_a && fabsf(cmd.a - 180.0f) < 0.001f);
    assert(cmd.has_f && fabsf(cmd.f - 1200.0f) < 0.001f);

    // Test lowercase and negative numbers
    ok = gcode_parse_line("g1 x-15.2 y+0.3 z-1.0 a0 f100", &cmd);
    assert(ok);
    assert(cmd.command == 'G');
    assert(cmd.code == 1);
    assert(cmd.has_x && fabsf(cmd.x - (-15.2f)) < 0.001f);
    assert(cmd.has_y && fabsf(cmd.y - 0.3f) < 0.001f);
    assert(cmd.has_z && fabsf(cmd.z - (-1.0f)) < 0.001f);
    assert(cmd.has_a && fabsf(cmd.a - 0.0f) < 0.001f);
    assert(cmd.has_f && fabsf(cmd.f - 100.0f) < 0.001f);

    // Test M command
    ok = gcode_parse_line("M114", &cmd);
    assert(ok);
    assert(cmd.command == 'M');
    assert(cmd.code == 114);
    assert(!cmd.has_x && !cmd.has_y && !cmd.has_z && !cmd.has_a && !cmd.has_f);

    // Test comments
    ok = gcode_parse_line("G28 ; Home all axes", &cmd);
    assert(ok);
    assert(cmd.command == 'G');
    assert(cmd.code == 28);

    // Test invalid line
    ok = gcode_parse_line("INVALID", &cmd);
    assert(!ok);

    printf("All unit tests passed successfully!\n");
}

static void run_benchmark(void) {
    const char *lines[] = {
        "G1 X12.34 Y56.78 Z10.00 A180.0 F1200",
        "G0 X0 Y0",
        "M114",
        "G21",
        "G90",
        "g1 x100.50 y200.25 z5.00 a360.0 f2400.0",
        "G1 X-15.2 Y+0.3 Z-1.0 A0 F100"
    };
    int num_lines = sizeof(lines) / sizeof(lines[0]);
    gcode_command_t cmd;

    int iterations = 1000000;
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        gcode_parse_line(lines[i % num_lines], &cmd);
    }
    clock_t end = clock();

    double total_time = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Benchmark: %d parses completed in %.4f seconds (%.2f ops/sec)\n",
           iterations, total_time, iterations / total_time);
}

int main(void) {
    test_gcode_commands();
    run_benchmark();
    return 0;
}
