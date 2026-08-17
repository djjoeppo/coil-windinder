#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Fast inline helpers to avoid libc locale/function call overhead in embedded command parser
static inline bool is_ascii_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline char to_ascii_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

/*
 * Optimized single-pass O(N) G-code parser.
 * Eliminates intermediate string buffer copying, multiple strchr full-line scans,
 * and redundant character normalization passes.
 */
bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line) return false;

    const char* p = line;

    // Skip leading whitespace
    while (*p && is_ascii_space(*p)) {
        p++;
    }

    char first_char = to_ascii_upper(*p);
    if (first_char != 'G' && first_char != 'M') {
        return false;
    }

    memset(cmd, 0, sizeof(gcode_command_t));
    cmd->command = first_char;
    p++; // Skip 'G' or 'M'

    // Fast inline base-10 integer parser for command code (e.g. 0, 1, 28, 114)
    int code = 0;
    bool has_code = false;
    while (*p >= '0' && *p <= '9') {
        code = code * 10 + (*p - '0');
        has_code = true;
        p++;
    }
    if (!has_code) {
        return false;
    }
    cmd->code = code;

    // Single pass scan for command parameters (X, Y, Z, A, F)
    while (*p) {
        // Skip whitespace
        while (*p && is_ascii_space(*p)) {
            p++;
        }

        if (!*p || *p == ';') break; // End of line or start of G-code comment

        char key = to_ascii_upper(*p);
        p++;

        char* endptr = NULL;
        float val = strtof(p, &endptr);

        if (endptr == p) {
            // Could not parse a float; skip invalid character to prevent infinite loop
            continue;
        }

        p = endptr;

        switch (key) {
            case 'X':
                cmd->x = val;
                cmd->has_x = true;
                break;
            case 'Y':
                cmd->y = val;
                cmd->has_y = true;
                break;
            case 'Z':
                cmd->z = val;
                cmd->has_z = true;
                break;
            case 'A':
                cmd->a = val;
                cmd->has_a = true;
                break;
            case 'F':
                cmd->f = val;
                cmd->has_f = true;
                break;
            default:
                // Other parameters (e.g. S, P, E) are ignored but parsed safely
                break;
        }
    }

    return true;
}
