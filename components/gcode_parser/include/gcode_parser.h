#ifndef GCODE_PARSER_H
#define GCODE_PARSER_H

#include <stdbool.h>

typedef struct {
    char command;
    int code;
    float x, y, z, a, f;
    bool has_x, has_y, has_z, has_a, has_f;
} gcode_command_t;

bool gcode_parse_line(const char* line, gcode_command_t* cmd);

#endif // GCODE_PARSER_H
