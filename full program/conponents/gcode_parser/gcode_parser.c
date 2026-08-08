#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static inline bool is_space_fast(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f');
}

static inline char to_upper_fast(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line || strlen(line) == 0) return false;

    memset(cmd, 0, sizeof(gcode_command_t));
    
    char clean_line[130]; // Slightly larger than input line to be safe
    int j = 0;
    for (int i = 0; line[i] && j < 128; i++) {
        if (!is_space_fast(line[i])) {
            clean_line[j++] = to_upper_fast(line[i]);
        }
    }
    clean_line[j] = '\0';

    if (clean_line[0] == 'G' || clean_line[0] == 'M') {
        cmd->command = clean_line[0];
        
        // Explicitly initialize all fields as they might not be present in the line
        cmd->has_x = cmd->has_y = cmd->has_z = cmd->has_a = cmd->has_f = false;
        cmd->x = cmd->y = cmd->z = cmd->a = cmd->f = 0.0f;
        
        char *p = clean_line + 1;
        char *endptr;
        cmd->code = (int)strtol(p, &endptr, 10);
        p = endptr;

        while (*p) {
            char key = *p;
            if (key == 'X' || key == 'Y' || key == 'Z' || key == 'A' || key == 'F') {
                float val = strtof(p + 1, &endptr);
                if (key == 'X') { cmd->x = val; cmd->has_x = true; }
                else if (key == 'Y') { cmd->y = val; cmd->has_y = true; }
                else if (key == 'Z') { cmd->z = val; cmd->has_z = true; }
                else if (key == 'A') { cmd->a = val; cmd->has_a = true; }
                else if (key == 'F') { cmd->f = val; cmd->has_f = true; }
                p = endptr;
            } else {
                p++;
            }
        }
        return true;
    }

    return false;
}
