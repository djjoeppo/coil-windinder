#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Performance-optimized custom macros to replace standard locale-dependent functions
#define IS_SPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n' || (c) == '\v' || (c) == '\f')
#define TO_UPPER(c) (((c) >= 'a' && (c) <= 'z') ? ((c) - 'a' + 'A') : (c))

/**
 * Parses a single G-code or M-code line in a highly efficient single pass.
 * Completely avoids redundant multiple strchr() searches and repeatedly calling strtof().
 */
bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line || *line == '\0') return false;

    memset(cmd, 0, sizeof(gcode_command_t));
    
    char clean_line[130]; // Slightly larger than input line to be safe
    int j = 0;
    for (int i = 0; line[i] && j < 128; i++) {
        char c = line[i];
        if (!IS_SPACE(c)) {
            clean_line[j++] = TO_UPPER(c);
        }
    }
    clean_line[j] = '\0';

    char first = clean_line[0];
    if (first == 'G' || first == 'M') {
        cmd->command = first;
        
        char *p;
        cmd->code = (int)strtol(clean_line + 1, &p, 10);
        
        // Single-pass scanner to parse parameters and their corresponding floating values
        while (*p != '\0') {
            char key = *p;
            char *next_p;
            float val = strtof(p + 1, &next_p);

            // If strtof couldn't parse a number (next_p == p + 1), advance pointer manually
            if (next_p == p + 1) {
                p++;
                continue;
            }

            if (key == 'X') {
                cmd->x = val;
                cmd->has_x = true;
            } else if (key == 'Y') {
                cmd->y = val;
                cmd->has_y = true;
            } else if (key == 'Z') {
                cmd->z = val;
                cmd->has_z = true;
            } else if (key == 'A') {
                cmd->a = val;
                cmd->has_a = true;
            } else if (key == 'F') {
                cmd->f = val;
                cmd->has_f = true;
            }
            p = next_p;
        }
        return true;
    }

    return false;
}
