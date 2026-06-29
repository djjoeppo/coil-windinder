#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static float get_value(const char* s, char key, bool* found) {
    const char* p = strchr(s, key);
    if (p) {
        *found = true;
        return strtof(p + 1, NULL);
    }
    *found = false;
    return 0.0f;
}

bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line || strlen(line) == 0) return false;

    memset(cmd, 0, sizeof(gcode_command_t));

    char clean_line[130]; // Slightly larger than input line to be safe
    int j = 0;
    for (int i = 0; line[i] && j < 128; i++) {
        if (!isspace((int)line[i])) {
            clean_line[j++] = toupper((int)line[i]);
        }
    }
    clean_line[j] = '\0';

    if (clean_line[0] == 'G' || clean_line[0] == 'M') {
        cmd->command = clean_line[0];
        cmd->code = atoi(clean_line + 1);

        cmd->x = get_value(clean_line, 'X', &cmd->has_x);
        cmd->y = get_value(clean_line, 'Y', &cmd->has_y);
        cmd->z = get_value(clean_line, 'Z', &cmd->has_z);
        cmd->a = get_value(clean_line, 'A', &cmd->has_a);
        cmd->f = get_value(clean_line, 'F', &cmd->has_f);

        return true;
    }

    return false;
}
