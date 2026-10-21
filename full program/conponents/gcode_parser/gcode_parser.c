#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/**
 * Optimized G-Code Parser
 * Performs a single-pass O(N) scan to extract command keys X, Y, Z, A, F.
 * This completely eliminates 5 redundant scans of the entire line with strchr(),
 * and only invokes strtof() when a valid parameter key is actually encountered,
 * significantly reducing Core 0 CPU utilization during G-code processing.
 */
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
        
        // Single-pass parser to extract keys 'X', 'Y', 'Z', 'A', 'F'
        for (int i = 1; clean_line[i] != '\0'; ) {
            char key = clean_line[i];
            if (key == 'X' || key == 'Y' || key == 'Z' || key == 'A' || key == 'F') {
                char* endptr;
                float val = strtof(&clean_line[i + 1], &endptr);

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

                if (endptr > &clean_line[i + 1]) {
                    i = endptr - clean_line; // Jump past the parsed float
                } else {
                    i++;
                }
            } else {
                i++;
            }
        }
        return true;
    }

    return false;
}
