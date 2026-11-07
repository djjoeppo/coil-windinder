#include "gcode_parser.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool float_eq(float a, float b) { return fabsf(a - b) < 0.001f; }

static void test_g0_g1_command(void) {
  gcode_command_t cmd;
  bool ok = gcode_parse_line("G1 X12.34 Y56.78 Z90.12 A34.56 F3000", &cmd);
  assert(ok);
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(cmd.has_x && float_eq(cmd.x, 12.34f));
  assert(cmd.has_y && float_eq(cmd.y, 56.78f));
  assert(cmd.has_z && float_eq(cmd.z, 90.12f));
  assert(cmd.has_a && float_eq(cmd.a, 34.56f));
  assert(cmd.has_f && float_eq(cmd.f, 3000.0f));
}

static void test_negative_values(void) {
  gcode_command_t cmd;
  bool ok = gcode_parse_line("G1 X-15.5 Y-22.1 Z-0.5 A-180.0 F1500.5", &cmd);
  assert(ok);
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(cmd.has_x && float_eq(cmd.x, -15.5f));
  assert(cmd.has_y && float_eq(cmd.y, -22.1f));
  assert(cmd.has_z && float_eq(cmd.z, -0.5f));
  assert(cmd.has_a && float_eq(cmd.a, -180.0f));
  assert(cmd.has_f && float_eq(cmd.f, 1500.5f));
}

static void test_m_commands(void) {
  gcode_command_t cmd;
  bool ok = gcode_parse_line("M114", &cmd);
  assert(ok);
  assert(cmd.command == 'M');
  assert(cmd.code == 114);
  assert(!cmd.has_x && !cmd.has_y && !cmd.has_z && !cmd.has_a && !cmd.has_f);

  ok = gcode_parse_line("M402", &cmd);
  assert(ok);
  assert(cmd.command == 'M');
  assert(cmd.code == 402);
}

static void test_whitespace_and_lowercase(void) {
  gcode_command_t cmd;
  bool ok = gcode_parse_line("   g0   x10.5   y20.25   f1200   ", &cmd);
  assert(ok);
  assert(cmd.command == 'G');
  assert(cmd.code == 0);
  assert(cmd.has_x && float_eq(cmd.x, 10.5f));
  assert(cmd.has_y && float_eq(cmd.y, 20.25f));
  assert(!cmd.has_z);
  assert(!cmd.has_a);
  assert(cmd.has_f && float_eq(cmd.f, 1200.0f));
}

static void test_invalid_input(void) {
  gcode_command_t cmd;
  assert(!gcode_parse_line(NULL, &cmd));
  assert(!gcode_parse_line("", &cmd));
  assert(!gcode_parse_line("    ", &cmd));
  assert(!gcode_parse_line("INVALID GCODE", &cmd));
  assert(!gcode_parse_line("XYZ 123", &cmd));
}

int main(void) {
  test_g0_g1_command();
  test_negative_values();
  test_m_commands();
  test_whitespace_and_lowercase();
  test_invalid_input();
  printf("All G-code parser tests passed!\n");
  return 0;
}
