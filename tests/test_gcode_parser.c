#include "gcode_parser.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define EPSILON 0.0001f

static bool float_eq(float a, float b) { return fabsf(a - b) < EPSILON; }

static void run_unit_tests(void) {
  gcode_command_t cmd;

  // Test 1: Empty & NULL
  assert(!gcode_parse_line(NULL, &cmd));
  assert(!gcode_parse_line("", &cmd));
  assert(!gcode_parse_line("   ", &cmd));

  // Test 2: G1 standard command
  assert(gcode_parse_line("G1 X12.34 Y56.78 Z90.12 A34.56 F1500", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(cmd.has_x && float_eq(cmd.x, 12.34f));
  assert(cmd.has_y && float_eq(cmd.y, 56.78f));
  assert(cmd.has_z && float_eq(cmd.z, 90.12f));
  assert(cmd.has_a && float_eq(cmd.a, 34.56f));
  assert(cmd.has_f && float_eq(cmd.f, 1500.0f));

  // Test 3: Lowercase and leading/trailing/embedded spaces
  assert(gcode_parse_line("  g1   x-10.5   y+20.25   f500  ", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(cmd.has_x && float_eq(cmd.x, -10.5f));
  assert(cmd.has_y && float_eq(cmd.y, 20.25f));
  assert(!cmd.has_z);
  assert(!cmd.has_a);
  assert(cmd.has_f && float_eq(cmd.f, 500.0f));

  // Test 4: M-codes
  assert(gcode_parse_line("M114", &cmd));
  assert(cmd.command == 'M');
  assert(cmd.code == 114);
  assert(!cmd.has_x && !cmd.has_y && !cmd.has_z && !cmd.has_a && !cmd.has_f);

  assert(gcode_parse_line("m401", &cmd));
  assert(cmd.command == 'M');
  assert(cmd.code == 401);

  // Test 5: G0 zero coordinates
  assert(gcode_parse_line("G0 X0 Y0 Z0 A0", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 0);
  assert(cmd.has_x && float_eq(cmd.x, 0.0f));
  assert(cmd.has_y && float_eq(cmd.y, 0.0f));
  assert(cmd.has_z && float_eq(cmd.z, 0.0f));
  assert(cmd.has_a && float_eq(cmd.a, 0.0f));
  assert(!cmd.has_f);

  // Test 6: Decimal values with leading/trailing dot or no dot
  assert(gcode_parse_line("G1 X.5 Y10. Z-0.75", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(cmd.has_x && float_eq(cmd.x, 0.5f));
  assert(cmd.has_y && float_eq(cmd.y, 10.0f));
  assert(cmd.has_z && float_eq(cmd.z, -0.75f));

  // Test 7: Non G/M line
  assert(!gcode_parse_line("UNKNOWN 123", &cmd));
  assert(!gcode_parse_line("12345", &cmd));

  printf("All unit tests passed successfully!\n");
}

static void run_benchmark(void) {
  const char *lines[] = {"G1 X12.34 Y56.78 Z90.12 A34.56 F1500",
                         "G0 X0 Y0 Z0 A0",
                         "M114",
                         "G28",
                         "G1 X-10.5 Y20.25 F3000",
                         "M401",
                         "G1 Z5.0",
                         "  g1  x100.5   y-200.75   f500  "};
  int num_lines = sizeof(lines) / sizeof(lines[0]);
  gcode_command_t cmd;

  long iterations = 500000;
  clock_t start = clock();
  for (long i = 0; i < iterations; i++) {
    for (int j = 0; j < num_lines; j++) {
      gcode_parse_line(lines[j], &cmd);
    }
  }
  clock_t end = clock();
  double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Benchmark: %ld parses in %.4f s (%.2f ns/parse)\n",
         iterations * num_lines, cpu_time,
         (cpu_time / (iterations * num_lines)) * 1e9);
}

int main(void) {
  run_unit_tests();
  run_benchmark();
  return 0;
}
