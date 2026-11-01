#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "gcode_parser.h"

int main(void) {
    gcode_command_t cmd;

    // Test empty & whitespace inputs
    assert(!gcode_parse_line(NULL, &cmd));
    assert(!gcode_parse_line("", &cmd));
    assert(!gcode_parse_line("   ", &cmd));
    assert(!gcode_parse_line("\t\r\n", &cmd));
    assert(!gcode_parse_line("INVALID", &cmd));
    assert(!gcode_parse_line("X10 Y20", &cmd));

    // Test standard G0 / G1 moves with multiple axes and feeds
    assert(gcode_parse_line("G1 X12.34 Y-56.78 Z0.50 A180.0 F1200", &cmd));
    assert(cmd.command == 'G' && cmd.code == 1);
    assert(cmd.has_x && fabs(cmd.x - 12.34f) < 0.001f);
    assert(cmd.has_y && fabs(cmd.y - (-56.78f)) < 0.001f);
    assert(cmd.has_z && fabs(cmd.z - 0.50f) < 0.001f);
    assert(cmd.has_a && fabs(cmd.a - 180.0f) < 0.001f);
    assert(cmd.has_f && fabs(cmd.f - 1200.0f) < 0.001f);

    // Test lowercase and extra padding/spaces
    assert(gcode_parse_line("   g0   x0 y0 z10   ", &cmd));
    assert(cmd.command == 'G' && cmd.code == 0);
    assert(cmd.has_x && fabs(cmd.x - 0.0f) < 0.001f);
    assert(cmd.has_y && fabs(cmd.y - 0.0f) < 0.001f);
    assert(cmd.has_z && fabs(cmd.z - 10.0f) < 0.001f);
    assert(!cmd.has_a);
    assert(!cmd.has_f);

    // Test G28 homing
    assert(gcode_parse_line("G28", &cmd));
    assert(cmd.command == 'G' && cmd.code == 28);
    assert(!cmd.has_x && !cmd.has_y && !cmd.has_z && !cmd.has_a && !cmd.has_f);

    // Test M114 position query
    assert(gcode_parse_line("M114", &cmd));
    assert(cmd.command == 'M' && cmd.code == 114);

    // Test M401 force mode command with Z axis
    assert(gcode_parse_line("M401 Z2.50", &cmd));
    assert(cmd.command == 'M' && cmd.code == 401);
    assert(cmd.has_z && fabs(cmd.z - 2.50f) < 0.001f);

    // Test positive signed values and fractional floats
    assert(gcode_parse_line("G1 X+10.5 Y-0.05", &cmd));
    assert(cmd.has_x && fabs(cmd.x - 10.5f) < 0.001f);
    assert(cmd.has_y && fabs(cmd.y - (-0.05f)) < 0.001f);

    // Test zero values
    assert(gcode_parse_line("G1 X0.0 Y0 Z0 A0 F0", &cmd));
    assert(cmd.has_x && fabs(cmd.x - 0.0f) < 0.001f);
    assert(cmd.has_y && fabs(cmd.y - 0.0f) < 0.001f);
    assert(cmd.has_z && fabs(cmd.z - 0.0f) < 0.001f);
    assert(cmd.has_a && fabs(cmd.a - 0.0f) < 0.001f);
    assert(cmd.has_f && fabs(cmd.f - 0.0f) < 0.001f);

    // Test single axis move
    assert(gcode_parse_line("G1 X100", &cmd));
    assert(cmd.has_x && fabs(cmd.x - 100.0f) < 0.001f);
    assert(!cmd.has_y && !cmd.has_z && !cmd.has_a && !cmd.has_f);

    printf("All G-code parser unit tests passed successfully!\n");
    return 0;
}
