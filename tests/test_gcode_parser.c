#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "gcode_parser.h"

int main() {
    printf("Running G-code parser unit tests...\n");

    // Test 1: Standard G1 linear move with multiple axes and feedrate
    {
        gcode_command_t cmd;
        bool ok = gcode_parse_line("G1 X120.50 Y45.20 Z-2.35 A90.00 F3000", &cmd);
        assert(ok);
        assert(cmd.command == 'G');
        assert(cmd.code == 1);
        assert(cmd.has_x && fabsf(cmd.x - 120.50f) < 0.001f);
        assert(cmd.has_y && fabsf(cmd.y - 45.20f) < 0.001f);
        assert(cmd.has_z && fabsf(cmd.z - (-2.35f)) < 0.001f);
        assert(cmd.has_a && fabsf(cmd.a - 90.00f) < 0.001f);
        assert(cmd.has_f && fabsf(cmd.f - 3000.0f) < 0.001f);
    }

    // Test 2: Lowercase and leading/trailing whitespace
    {
        gcode_command_t cmd;
        bool ok = gcode_parse_line("   g0  x10.0   y20.0   ", &cmd);
        assert(ok);
        assert(cmd.command == 'G');
        assert(cmd.code == 0);
        assert(cmd.has_x && fabsf(cmd.x - 10.0f) < 0.001f);
        assert(cmd.has_y && fabsf(cmd.y - 20.0f) < 0.001f);
        assert(!cmd.has_z);
        assert(!cmd.has_a);
        assert(!cmd.has_f);
    }

    // Test 3: M-codes without coordinates (e.g. M114, M401, M402)
    {
        gcode_command_t cmd;
        bool ok = gcode_parse_line("M114", &cmd);
        assert(ok);
        assert(cmd.command == 'M');
        assert(cmd.code == 114);
        assert(!cmd.has_x && !cmd.has_y && !cmd.has_z && !cmd.has_a && !cmd.has_f);

        ok = gcode_parse_line("m401", &cmd);
        assert(ok);
        assert(cmd.command == 'M');
        assert(cmd.code == 401);
    }

    // Test 4: Negative coordinates and explicit positive signs
    {
        gcode_command_t cmd;
        bool ok = gcode_parse_line("G1 X-0.5 Y+12.75 Z-15.000 A+360.5 F500", &cmd);
        assert(ok);
        assert(cmd.command == 'G');
        assert(cmd.code == 1);
        assert(cmd.has_x && fabsf(cmd.x - (-0.5f)) < 0.001f);
        assert(cmd.has_y && fabsf(cmd.y - 12.75f) < 0.001f);
        assert(cmd.has_z && fabsf(cmd.z - (-15.0f)) < 0.001f);
        assert(cmd.has_a && fabsf(cmd.a - 360.5f) < 0.001f);
        assert(cmd.has_f && fabsf(cmd.f - 500.0f) < 0.001f);
    }

    // Test 5: Invalid inputs
    {
        gcode_command_t cmd;
        assert(!gcode_parse_line(NULL, &cmd));
        assert(!gcode_parse_line("", &cmd));
        assert(!gcode_parse_line("   \n\r  ", &cmd));
        assert(!gcode_parse_line("UNKNOWN 123", &cmd));
        assert(!gcode_parse_line("N10 G1 X10", &cmd)); // Non G/M prefix
    }

    printf("All G-code parser unit tests passed successfully!\n");
    return 0;
}
