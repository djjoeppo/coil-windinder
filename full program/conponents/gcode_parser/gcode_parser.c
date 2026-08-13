#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static inline int is_ascii_space(unsigned char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f');
}

static inline unsigned char to_ascii_upper(unsigned char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line || line[0] == '\0') return false;

    memset(cmd, 0, sizeof(gcode_command_t));
    
    char clean_line[130]; // Slightly larger than input line to be safe
    int j = 0;
    for (int i = 0; line[i] && j < 128; i++) {
        unsigned char c = (unsigned char)line[i];
        if (!is_ascii_space(c)) {
            clean_line[j++] = to_ascii_upper(c);
        }
    }
    clean_line[j] = '\0';

    if (clean_line[0] == 'G' || clean_line[0] == 'M') {
        cmd->command = clean_line[0];
        
        char* endptr = NULL;
        cmd->code = (int)strtol(clean_line + 1, &endptr, 10);
        
        char* p = endptr;
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
