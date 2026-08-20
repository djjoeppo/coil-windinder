#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static inline bool is_ascii_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline char to_ascii_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (c - ('a' - 'A')) : c;
}

// Fast float parsing bypassing libc locale and strtof overhead
static float fast_atof(const char* p, const char** endptr) {
    while (*p && is_ascii_space(*p)) p++;

    float sign = 1.0f;
    if (*p == '-') {
        sign = -1.0f;
        p++;
    } else if (*p == '+') {
        p++;
    }

    double val = 0.0;
    while (*p >= '0' && *p <= '9') {
        val = val * 10.0 + (*p - '0');
        p++;
    }

    if (*p == '.') {
        p++;
        double frac = 0.0;
        double div = 1.0;
        while (*p >= '0' && *p <= '9') {
            frac = frac * 10.0 + (*p - '0');
            div *= 10.0;
            p++;
        }
        val += frac / div;
    }

    if (endptr) *endptr = p;
    return (float)(sign * val);
}

// Optimized single-pass zero-allocation G-code line parser
bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line) return false;

    // Skip leading whitespace
    const char* p = line;
    while (*p && is_ascii_space(*p)) p++;

    char first = to_ascii_upper(*p);
    if (first != 'G' && first != 'M') return false;

    memset(cmd, 0, sizeof(gcode_command_t));
    cmd->command = first;
    p++;

    cmd->code = (int)fast_atof(p, &p);

    // Single pass scan over key-value parameter pairs
    while (*p) {
        while (*p && is_ascii_space(*p)) p++;
        if (!*p || *p == ';' || *p == '(') break;

        char key = to_ascii_upper(*p);
        p++;

        if (key >= 'A' && key <= 'Z') {
            float val = fast_atof(p, &p);
            switch (key) {
                case 'X': cmd->x = val; cmd->has_x = true; break;
                case 'Y': cmd->y = val; cmd->has_y = true; break;
                case 'Z': cmd->z = val; cmd->has_z = true; break;
                case 'A': cmd->a = val; cmd->has_a = true; break;
                case 'F': cmd->f = val; cmd->has_f = true; break;
                default: break;
            }
        }
    }

    return true;
}
