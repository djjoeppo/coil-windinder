#include "gcode_parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

int main(void) {
  const char *test_lines[] = {"G1 X12.34 Y56.78 Z90.12 A34.56 F3000",
                              "G0 X0 Y0 Z0",
                              "M114",
                              "G28",
                              "G1 X-15.5 Y-22.1 Z0.5 F1500.5",
                              "  g1   x100.25   y200.75   f500  ",
                              "M401",
                              "M402",
                              "G90",
                              "G91"};
  const int num_lines = sizeof(test_lines) / sizeof(test_lines[0]);
  const int iterations = 1000000;
  gcode_command_t cmd;

  clock_t start = clock();
  for (int iter = 0; iter < iterations; iter++) {
    for (int i = 0; i < num_lines; i++) {
      gcode_parse_line(test_lines[i], &cmd);
    }
  }
  clock_t end = clock();
  double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
  double total_lines = (double)iterations * num_lines;
  printf(
      "Parsed %.0f lines in: %.4f seconds (%.2f ns per line, %.2f lines/sec)\n",
      total_lines, time_spent, (time_spent / total_lines) * 1e9,
      total_lines / time_spent);
  return 0;
}
