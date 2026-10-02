#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Fast inline ASCII whitespace check avoiding ctype locale overhead
static inline bool is_ascii_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Fast inline upper-case conversion for ASCII characters
static inline char to_ascii_upper(char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

// Fast float parser advancing pointer in a single pass
static inline float fast_atof(const char **p) {
    const char *s = *p;
    float sign = 1.0f;
    if (*s == '-') {
        sign = -1.0f;
        s++;
    } else if (*s == '+') {
        s++;
    }
    float val = 0.0f;
    while (*s >= '0' && *s <= '9') {
        val = val * 10.0f + (*s - '0');
        s++;
    }
    if (*s == '.') {
        s++;
        float factor = 0.1f;
        while (*s >= '0' && *s <= '9') {
            val += (*s - '0') * factor;
            factor *= 0.1f;
            s++;
        }
    }
    *p = s;
    return sign * val;
}

// Fast integer parser advancing pointer in a single pass
static inline int fast_atoi(const char **p) {
    const char *s = *p;
    int sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    *p = s;
    return sign * val;
}

bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line) return false;

    const char *p = line;
    // Skip leading whitespace
    while (*p && is_ascii_space(*p)) p++;
    if (!*p) return false;

    char first_cmd = to_ascii_upper(*p);
    if (first_cmd != 'G' && first_cmd != 'M') return false;

    p++;
    memset(cmd, 0, sizeof(gcode_command_t));
    cmd->command = first_cmd;

    // Parse command integer code (e.g., 1 for G1 or 114 for M114)
    cmd->code = fast_atoi(&p);

    // Parse axis / feed parameters in a single pass scanner
    while (*p) {
        while (*p && is_ascii_space(*p)) p++;
        if (!*p) break;

        char key = to_ascii_upper(*p);
        p++;

        switch (key) {
            case 'X': cmd->x = fast_atof(&p); cmd->has_x = true; break;
            case 'Y': cmd->y = fast_atof(&p); cmd->has_y = true; break;
            case 'Z': cmd->z = fast_atof(&p); cmd->has_z = true; break;
            case 'A': cmd->a = fast_atof(&p); cmd->has_a = true; break;
            case 'F': cmd->f = fast_atof(&p); cmd->has_f = true; break;
            default:
                // Skip unknown parameters until next space or parameter letter
                while (*p && !is_ascii_space(*p) &&
                       (to_ascii_upper(*p) < 'A' || to_ascii_upper(*p) > 'Z')) {
                    p++;
                }
                break;
        }
    }

    return true;
}
