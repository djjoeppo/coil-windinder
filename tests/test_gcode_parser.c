#include "gcode_parser.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static bool float_eq(float a, float b) { return fabsf(a - b) < 0.0001f; }

void test_basic_commands() {
  gcode_command_t cmd;

  // G0 with spaces and lowercase
  assert(gcode_parse_line("g0 x10.5 y20.25 z-3.14 f1500", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 0);
  assert(cmd.has_x && float_eq(cmd.x, 10.5f));
  assert(cmd.has_y && float_eq(cmd.y, 20.25f));
  assert(cmd.has_z && float_eq(cmd.z, -3.14f));
  assert(!cmd.has_a);
  assert(cmd.has_f && float_eq(cmd.f, 1500.0f));

  // G1 with A axis
  assert(gcode_parse_line("G1 A360.0 F500", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(!cmd.has_x && !cmd.has_y && !cmd.has_z);
  assert(cmd.has_a && float_eq(cmd.a, 360.0f));
  assert(cmd.has_f && float_eq(cmd.f, 500.0f));

  // G28 homing
  assert(gcode_parse_line("G28", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 28);
  assert(!cmd.has_x && !cmd.has_y && !cmd.has_z && !cmd.has_a && !cmd.has_f);

  // M114
  assert(gcode_parse_line("M114", &cmd));
  assert(cmd.command == 'M');
  assert(cmd.code == 114);

  // Empty or invalid lines
  assert(!gcode_parse_line("", &cmd));
  assert(!gcode_parse_line("   ", &cmd));
  assert(!gcode_parse_line("XYZ", &cmd));

  // Spaces between letters and numbers
  assert(gcode_parse_line("G 1 X 12.34 Y -56.78", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(cmd.has_x && float_eq(cmd.x, 12.34f));
  assert(cmd.has_y && float_eq(cmd.y, -56.78f));

  // Unhandled token in unspaced string (e.g., E5.0 followed by X10.0)
  assert(gcode_parse_line("G1 E5.0 X10.0 Y20.0", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(cmd.has_x && float_eq(cmd.x, 10.0f));
  assert(cmd.has_y && float_eq(cmd.y, 20.0f));

  assert(gcode_parse_line("G1E5.0X10.0Y20.0", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(cmd.has_x && float_eq(cmd.x, 10.0f));
  assert(cmd.has_y && float_eq(cmd.y, 20.0f));

  printf("Unit tests passed successfully.\n");
}

void benchmark_parser() {
  const char *lines[] = {"G1 X123.456 Y78.901 Z-12.34 A90.5 F3000",
                         "G0 X0 Y0 Z0 A0",
                         "G1 X10.5 F1500",
                         "M114",
                         "G28",
                         "g1 x50.25 y100.5 a180 f2000"};
  const int num_lines = sizeof(lines) / sizeof(lines[0]);
  const int iterations = 1000000;

  gcode_command_t cmd;
  clock_t start = clock();
  for (int i = 0; i < iterations; i++) {
    const char *line = lines[i % num_lines];
    gcode_parse_line(line, &cmd);
  }
  clock_t end = clock();
  double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Benchmark: %d iterations in %.4f seconds (%.2f ns/parse)\n",
         iterations, cpu_time_used, (cpu_time_used / iterations) * 1e9);
}

int main() {
  test_basic_commands();
  benchmark_parser();
  return 0;
}
