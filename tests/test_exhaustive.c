#include "gcode_parser.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int main() {
  gcode_command_t cmd;

  // Normal move
  assert(gcode_parse_line("G1 X12.34 Y56.78 Z90.12 A34.56 F1200", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(cmd.has_x && fabsf(cmd.x - 12.34f) < 0.0001f);
  assert(cmd.has_y && fabsf(cmd.y - 56.78f) < 0.0001f);
  assert(cmd.has_z && fabsf(cmd.z - 90.12f) < 0.0001f);
  assert(cmd.has_a && fabsf(cmd.a - 34.56f) < 0.0001f);
  assert(cmd.has_f && fabsf(cmd.f - 1200.0f) < 0.0001f);

  // Negative values
  assert(gcode_parse_line("G0 X-10.5 Y-0.25 Z-100", &cmd));
  assert(cmd.command == 'G');
  assert(cmd.code == 0);
  assert(cmd.has_x && fabsf(cmd.x - (-10.5f)) < 0.0001f);
  assert(cmd.has_y && fabsf(cmd.y - (-0.25f)) < 0.0001f);
  assert(cmd.has_z && fabsf(cmd.z - (-100.0f)) < 0.0001f);
  assert(!cmd.has_a && !cmd.has_f);

  // Integer values without decimal point
  assert(gcode_parse_line("G1 X10 Y20 Z30", &cmd));
  assert(cmd.has_x && fabsf(cmd.x - 10.0f) < 0.0001f);
  assert(cmd.has_y && fabsf(cmd.y - 20.0f) < 0.0001f);
  assert(cmd.has_z && fabsf(cmd.z - 30.0f) < 0.0001f);

  // Leading '+' signs
  assert(gcode_parse_line("G1 X+5.5", &cmd));
  assert(cmd.has_x && fabsf(cmd.x - 5.5f) < 0.0001f);

  // M commands
  assert(gcode_parse_line("M114", &cmd));
  assert(cmd.command == 'M' && cmd.code == 114);

  assert(gcode_parse_line("M400", &cmd));
  assert(cmd.command == 'M' && cmd.code == 400);

  // Comments
  assert(gcode_parse_line("G1 X10 ; this is a comment", &cmd));
  assert(cmd.has_x && fabsf(cmd.x - 10.0f) < 0.0001f);

  // Spaces in unusual places
  assert(gcode_parse_line("  g  1   x  12.5   y  -3.2  ", &cmd));
  assert(cmd.command == 'G' && cmd.code == 1);
  assert(cmd.has_x && fabsf(cmd.x - 12.5f) < 0.0001f);
  assert(cmd.has_y && fabsf(cmd.y - (-3.2f)) < 0.0001f);

  // Invalid lines
  assert(!gcode_parse_line(NULL, &cmd));
  assert(!gcode_parse_line("", &cmd));
  assert(!gcode_parse_line("   ", &cmd));
  assert(!gcode_parse_line("T0", &cmd));
  assert(!gcode_parse_line("G", &cmd));
  assert(!gcode_parse_line("G X10", &cmd));

  printf("All exhaustive tests passed!\n");
  return 0;
}
