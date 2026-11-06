#include "gcode_parser.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int main() {
  gcode_command_t cmd;

  // Empty / null tests
  assert(!gcode_parse_line(NULL, &cmd));
  assert(!gcode_parse_line("", &cmd));
  assert(!gcode_parse_line("   ", &cmd));
  assert(!gcode_parse_line("XYZ", &cmd));

  // Valid commands
  assert(gcode_parse_line("G1 X10 Y20.5", &cmd));
  assert(cmd.command == 'G' && cmd.code == 1);
  assert(cmd.has_x && fabsf(cmd.x - 10.0f) < 0.0001f);
  assert(cmd.has_y && fabsf(cmd.y - 20.5f) < 0.0001f);
  assert(!cmd.has_z && !cmd.has_a && !cmd.has_f);

  // Negative numbers and spaces
  assert(gcode_parse_line("  g0  x-12.34  y+5.6   z -0.05  a 360  f 2000  ",
                          &cmd));
  assert(cmd.command == 'G' && cmd.code == 0);
  assert(cmd.has_x && fabsf(cmd.x - (-12.34f)) < 0.0001f);
  assert(cmd.has_y && fabsf(cmd.y - 5.6f) < 0.0001f);
  assert(cmd.has_z && fabsf(cmd.z - (-0.05f)) < 0.0001f);
  assert(cmd.has_a && fabsf(cmd.a - 360.0f) < 0.0001f);
  assert(cmd.has_f && fabsf(cmd.f - 2000.0f) < 0.0001f);

  // M codes
  assert(gcode_parse_line("M114", &cmd));
  assert(cmd.command == 'M' && cmd.code == 114);

  assert(gcode_parse_line("M402", &cmd));
  assert(cmd.command == 'M' && cmd.code == 402);

  // Line with comments
  assert(gcode_parse_line("G1 X15 ; move to 15", &cmd));
  assert(cmd.command == 'G' && cmd.code == 1);
  assert(cmd.has_x && fabsf(cmd.x - 15.0f) < 0.0001f);

  printf("All edge case tests passed!\n");
  return 0;
}
