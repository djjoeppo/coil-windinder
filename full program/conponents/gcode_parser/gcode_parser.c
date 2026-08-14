#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Extremely fast custom space checking to bypass standard locale-dependent functions
static inline int is_ascii_space(unsigned char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f');
}

// Extremely fast custom lowercase to uppercase ASCII conversion
static inline unsigned char to_ascii_upper(unsigned char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line || line[0] == '\0') return false;

    memset(cmd, 0, sizeof(gcode_command_t));
    
    char clean_line[130];
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
        
        // Fast inline integer parser to replace standard atoi()
        int code = 0;
        int i = 1;
        while (clean_line[i] >= '0' && clean_line[i] <= '9') {
            code = code * 10 + (clean_line[i] - '0');
            i++;
        }
        cmd->code = code;
        
        // Single-pass parameter scanning to replace 5 separate strchr() scans
        for (; clean_line[i] != '\0'; i++) {
            char c = clean_line[i];
            char* endptr;
            if (c == 'X') {
                cmd->x = strtof(clean_line + i + 1, &endptr);
                cmd->has_x = true;
                i = endptr - clean_line - 1;
            } else if (c == 'Y') {
                cmd->y = strtof(clean_line + i + 1, &endptr);
                cmd->has_y = true;
                i = endptr - clean_line - 1;
            } else if (c == 'Z') {
                cmd->z = strtof(clean_line + i + 1, &endptr);
                cmd->has_z = true;
                i = endptr - clean_line - 1;
            } else if (c == 'A') {
                cmd->a = strtof(clean_line + i + 1, &endptr);
                cmd->has_a = true;
                i = endptr - clean_line - 1;
            } else if (c == 'F') {
                cmd->f = strtof(clean_line + i + 1, &endptr);
                cmd->has_f = true;
                i = endptr - clean_line - 1;
            }
        }
        return true;
    }

    return false;
}
