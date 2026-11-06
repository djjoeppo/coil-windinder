#include "gcode_parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

int main() {
  const char *lines[] = {"G1 X120.5 Y-45.2 Z1.25 A180.0 F3000",
                         "G0 X0 Y0 Z0 A0",
                         "M114",
                         "G1 X50.25 F1500",
                         "G28",
                         "M402",
                         "  g1   x-12.345   y67.89   z0.001   f500  "};
  int num_lines = sizeof(lines) / sizeof(lines[0]);
  gcode_command_t cmd;

  clock_t start = clock();
  int iterations = 1000000;
  for (int i = 0; i < iterations; i++) {
    gcode_parse_line(lines[i % num_lines], &cmd);
  }
  clock_t end = clock();
  double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Iterations: %d, Time: %f seconds, Per parse: %.2f ns\n", iterations,
         cpu_time_used, (cpu_time_used / iterations) * 1e9);
  return 0;
}
