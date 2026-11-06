#include "gcode_parser.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main() {
  gcode_command_t cmd;
  bool ok = gcode_parse_line("G1 X10.5 Y-20.25 Z0.1 A90 F1500", &cmd);
  assert(ok);
  assert(cmd.command == 'G');
  assert(cmd.code == 1);
  assert(cmd.has_x && fabsf(cmd.x - 10.5f) < 0.001f);
  assert(cmd.has_y && fabsf(cmd.y - (-20.25f)) < 0.001f);
  assert(cmd.has_z && fabsf(cmd.z - 0.1f) < 0.001f);
  assert(cmd.has_a && fabsf(cmd.a - 90.0f) < 0.001f);
  assert(cmd.has_f && fabsf(cmd.f - 1500.0f) < 0.001f);
  printf("Test passed!\n");
  return 0;
}
