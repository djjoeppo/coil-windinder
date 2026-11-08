#include "gcode_parser.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void test_empty_and_invalid(void) {
  gcode_command_t cmd;
  assert(!gcode_parse_line(NULL, &cmd));
  assert(!gcode_parse_line("", &cmd));
  assert(!gcode_parse_line("   \n", &cmd));
  assert(!gcode_parse_line("INVALID\n", &cmd));
  assert(!gcode_parse_line("X10 Y20\n", &cmd));
}

static void test_g0_g1_move(void) {
  gcode_command_t cmd;
  bool ok = gcode_parse_line("G1 X100.5 Y200.25 Z-15.0 A90.0 F3000\n", &cmd);
  assert(ok);
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(cmd.has_x && fabsf(cmd.x - 100.5f) < 0.0001f);
  assert(cmd.has_y && fabsf(cmd.y - 200.25f) < 0.0001f);
  assert(cmd.has_z && fabsf(cmd.z - (-15.0f)) < 0.0001f);
  assert(cmd.has_a && fabsf(cmd.a - 90.0f) < 0.0001f);
  assert(cmd.has_f && fabsf(cmd.f - 3000.0f) < 0.0001f);

  ok = gcode_parse_line("  g0   x-10.5   z+2.4   f500  \r\n", &cmd);
  assert(ok);
  assert(cmd.command == 'G');
  assert(cmd.code == 0);
  assert(cmd.has_x && fabsf(cmd.x - (-10.5f)) < 0.0001f);
  assert(!cmd.has_y);
  assert(cmd.has_z && fabsf(cmd.z - 2.4f) < 0.0001f);
  assert(!cmd.has_a);
  assert(cmd.has_f && fabsf(cmd.f - 500.0f) < 0.0001f);
}

static void test_machine_commands(void) {
  gcode_command_t cmd;
  assert(gcode_parse_line("G28\n", &cmd) && cmd.command == 'G' &&
         cmd.code == 28);
  assert(gcode_parse_line("G90\n", &cmd) && cmd.command == 'G' &&
         cmd.code == 90);
  assert(gcode_parse_line("G91\n", &cmd) && cmd.command == 'G' &&
         cmd.code == 91);
  assert(gcode_parse_line("G4 F500\n", &cmd) && cmd.command == 'G' &&
         cmd.code == 4 && cmd.has_f && fabsf(cmd.f - 500.0f) < 0.0001f);
  assert(gcode_parse_line("M30\n", &cmd) && cmd.command == 'M' &&
         cmd.code == 30);
  assert(gcode_parse_line("M114\n", &cmd) && cmd.command == 'M' &&
         cmd.code == 114);
  assert(gcode_parse_line("M400\n", &cmd) && cmd.command == 'M' &&
         cmd.code == 400);
  assert(gcode_parse_line("M401\n", &cmd) && cmd.command == 'M' &&
         cmd.code == 401);
  assert(gcode_parse_line("M402\n", &cmd) && cmd.command == 'M' &&
         cmd.code == 402);
}

static void run_benchmark(void) {
  const char *lines[] = {"G1 X100.5 Y200.25 Z-15.0 A90.0 F3000\n",
                         "G0 X0 Y0 Z0 A0\n",
                         "M114\n",
                         "G28\n",
                         "G1 X12.345 Y67.890 F1200\n",
                         "  g1   x-10.5   z+2.4   f500  \n"};
  int num_lines = sizeof(lines) / sizeof(lines[0]);
  int iterations = 1000000;

  gcode_command_t cmd;
  clock_t start = clock();
  volatile long checksum = 0;
  for (int it = 0; it < iterations; it++) {
    for (int i = 0; i < num_lines; i++) {
      if (gcode_parse_line(lines[i], &cmd)) {
        checksum += cmd.command + cmd.code + (long)(cmd.x * 100);
      }
    }
  }
  clock_t end = clock();
  double cpu_time = (double)(end - start) / CLOCKS_PER_SEC;
  printf("Benchmark: parsed %d lines in %.4f seconds (%.2f ns/line, checksum: "
         "%ld)\n",
         num_lines * iterations, cpu_time,
         (cpu_time / (num_lines * iterations)) * 1e9, checksum);
}

int main(void) {
  printf("Running G-code parser tests...\n");
  test_empty_and_invalid();
  test_g0_g1_move();
  test_machine_commands();
  printf("All correctness tests passed!\n");

  run_benchmark();
  return 0;
}
