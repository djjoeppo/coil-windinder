#include "gcode_parser.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void test_gcode_cases(void) {
  gcode_command_t cmd;

  // Test standard linear motion
  assert(gcode_parse_line("G1 X12.34 Y56.78 Z9.01 A180.0 F3000", &cmd));
  assert(cmd.command == 'G' && cmd.code == 1);
  assert(cmd.has_x && fabs(cmd.x - 12.34f) < 0.001f);
  assert(cmd.has_y && fabs(cmd.y - 56.78f) < 0.001f);
  assert(cmd.has_z && fabs(cmd.z - 9.01f) < 0.001f);
  assert(cmd.has_a && fabs(cmd.a - 180.0f) < 0.001f);
  assert(cmd.has_f && fabs(cmd.f - 3000.0f) < 0.001f);

  // Test lowercase and negative coordinates
  assert(gcode_parse_line("g0 x-10.5 y+20.25 z-0.5 a0", &cmd));
  assert(cmd.command == 'G' && cmd.code == 0);
  assert(cmd.has_x && fabs(cmd.x - (-10.5f)) < 0.001f);
  assert(cmd.has_y && fabs(cmd.y - 20.25f) < 0.001f);
  assert(cmd.has_z && fabs(cmd.z - (-0.5f)) < 0.001f);
  assert(cmd.has_a && fabs(cmd.a - 0.0f) < 0.001f);
  assert(!cmd.has_f);

  // Test M commands
  assert(gcode_parse_line("M114", &cmd));
  assert(cmd.command == 'M' && cmd.code == 114);
  assert(!cmd.has_x && !cmd.has_y && !cmd.has_z && !cmd.has_a && !cmd.has_f);

  assert(gcode_parse_line("m402", &cmd));
  assert(cmd.command == 'M' && cmd.code == 402);

  assert(gcode_parse_line("M401", &cmd));
  assert(cmd.command == 'M' && cmd.code == 401);

  assert(gcode_parse_line("M400", &cmd));
  assert(cmd.command == 'M' && cmd.code == 400);

  assert(gcode_parse_line("M30", &cmd));
  assert(cmd.command == 'M' && cmd.code == 30);

  // Test G-commands: homing, modes, dwell
  assert(gcode_parse_line("G28", &cmd));
  assert(cmd.command == 'G' && cmd.code == 28);

  assert(gcode_parse_line("G90", &cmd));
  assert(cmd.command == 'G' && cmd.code == 90);

  assert(gcode_parse_line("G91", &cmd));
  assert(cmd.command == 'G' && cmd.code == 91);

  assert(gcode_parse_line("G20", &cmd));
  assert(cmd.command == 'G' && cmd.code == 20);

  assert(gcode_parse_line("G21", &cmd));
  assert(cmd.command == 'G' && cmd.code == 21);

  assert(gcode_parse_line("G4 F250", &cmd));
  assert(cmd.command == 'G' && cmd.code == 4);
  assert(cmd.has_f && fabs(cmd.f - 250.0f) < 0.001f);

  // Test whitespace variations
  assert(gcode_parse_line("   g1    z10.5   f500   ", &cmd));
  assert(cmd.command == 'G' && cmd.code == 1);
  assert(cmd.has_z && fabs(cmd.z - 10.5f) < 0.001f);
  assert(cmd.has_f && fabs(cmd.f - 500.0f) < 0.001f);
  assert(!cmd.has_x && !cmd.has_y && !cmd.has_a);

  // Test compact format without spaces
  assert(gcode_parse_line("G1X10Y20Z30A40F50", &cmd));
  assert(cmd.command == 'G' && cmd.code == 1);
  assert(cmd.has_x && fabs(cmd.x - 10.0f) < 0.001f);
  assert(cmd.has_y && fabs(cmd.y - 20.0f) < 0.001f);
  assert(cmd.has_z && fabs(cmd.z - 30.0f) < 0.001f);
  assert(cmd.has_a && fabs(cmd.a - 40.0f) < 0.001f);
  assert(cmd.has_f && fabs(cmd.f - 50.0f) < 0.001f);

  // Test invalid lines
  assert(!gcode_parse_line(NULL, &cmd));
  assert(!gcode_parse_line("", &cmd));
  assert(!gcode_parse_line("   ", &cmd));
  assert(!gcode_parse_line("X10 Y20", &cmd));
  assert(!gcode_parse_line("HELLO WORLD", &cmd));

  printf("All functional test cases passed successfully!\n");
}

static void benchmark_parser(void) {
  const char *sample_lines[] = {"G1 X12.34 Y56.78 Z9.01 A180.0 F3000",
                                "g0 x-10.5 y+20.25 z0 a0",
                                "M114",
                                "G1 X100.5 F1500",
                                "M402",
                                "G28",
                                "  g1  z10.5  f500  ",
                                "M401"};
  int num_lines = sizeof(sample_lines) / sizeof(sample_lines[0]);
  gcode_command_t cmd;
  long iterations = 500000;

  clock_t start = clock();
  for (long i = 0; i < iterations; i++) {
    for (int j = 0; j < num_lines; j++) {
      gcode_parse_line(sample_lines[j], &cmd);
    }
  }
  clock_t end = clock();
  double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Benchmark: %ld lines parsed in %.4f seconds (%.2f ns/line)\n",
         iterations * num_lines, cpu_time,
         (cpu_time * 1e9) / (iterations * num_lines));
}

int main(void) {
  test_gcode_cases();
  benchmark_parser();
  return 0;
}
