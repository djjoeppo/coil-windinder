#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "gcode_parser.h"

int main(void) {
  gcode_command_t cmd;

  // Correctness checks
  assert(gcode_parse_line("G1 X12.34 Y56.78 Z-1.23 A90 F1500", &cmd));
  assert(cmd.command == 'G' && cmd.code == 1);
  assert(cmd.has_x && fabsf(cmd.x - 12.34f) < 1e-4);
  assert(cmd.has_y && fabsf(cmd.y - 56.78f) < 1e-4);
  assert(cmd.has_z && fabsf(cmd.z - (-1.23f)) < 1e-4);
  assert(cmd.has_a && fabsf(cmd.a - 90.0f) < 1e-4);
  assert(cmd.has_f && fabsf(cmd.f - 1500.0f) < 1e-4);

  assert(gcode_parse_line("  g0   x-5.5  f3000 ", &cmd));
  assert(cmd.command == 'G' && cmd.code == 0);
  assert(cmd.has_x && fabsf(cmd.x - (-5.5f)) < 1e-4);
  assert(cmd.has_f && fabsf(cmd.f - 3000.0f) < 1e-4);
  assert(!cmd.has_y && !cmd.has_z && !cmd.has_a);

  assert(gcode_parse_line("M114", &cmd));
  assert(cmd.command == 'M' && cmd.code == 114);

  assert(!gcode_parse_line("", &cmd));
  assert(!gcode_parse_line("   ", &cmd));
  assert(!gcode_parse_line("INVALID", &cmd));

  printf("All correctness tests passed!\n");

  // Benchmark
  const char *test_lines[] = {"G1 X12.34 Y56.78 Z-1.23 A90 F1500",
                              "  g0   x-5.5  f3000 ", "M114", "G28",
                              "G1 X100.5 Y200.75 Z300.125 A45.0 F1200"};
  int num_lines = sizeof(test_lines) / sizeof(test_lines[0]);

  int iterations = 1000000;
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  for (int i = 0; i < iterations; i++) {
    gcode_parse_line(test_lines[i % num_lines], &cmd);
  }

  clock_gettime(CLOCK_MONOTONIC, &end);
  double elapsed_sec =
      (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
  double ns_per_call = (elapsed_sec / iterations) * 1e9;

  printf("Benchmark: %d iterations in %.4f s (%.2f ns/op)\n", iterations,
         elapsed_sec, ns_per_call);

  return 0;
}
