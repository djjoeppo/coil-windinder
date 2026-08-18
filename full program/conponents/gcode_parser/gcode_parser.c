#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Fast inline helpers to avoid ctype locale lookup overhead
static inline bool is_ascii_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline char to_ascii_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (c - (char)32) : c;
}

// Fast float parser for standard G-code floating numbers to avoid strtof overhead
static float fast_atof(const char* p, char** endptr) {
    while (is_ascii_space(*p)) p++;

    float sign = 1.0f;
    if (*p == '-') {
        sign = -1.0f;
        p++;
    } else if (*p == '+') {
        p++;
    }

    float val = 0.0f;
    bool has_digits = false;

    while (*p >= '0' && *p <= '9') {
        val = val * 10.0f + (float)(*p - '0');
        has_digits = true;
        p++;
    }

    if (*p == '.') {
        p++;
        float factor = 0.1f;
        while (*p >= '0' && *p <= '9') {
            val += (float)(*p - '0') * factor;
            factor *= 0.1f;
            has_digits = true;
            p++;
        }
    }

    if (!has_digits) {
        if (endptr) *endptr = (char*)p;
        return 0.0f;
    }

    if (endptr) *endptr = (char*)p;
    return val * sign;
}

/**
 * Single-pass G-code line parser optimized for latency & throughput.
 * Bypasses string copying, locale overhead, and redundant scans.
 */
bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line) return false;

    const char* p = line;
    while (is_ascii_space(*p)) p++;

    char first = to_ascii_upper(*p);
    if (first != 'G' && first != 'M') return false;

    memset(cmd, 0, sizeof(gcode_command_t));
    cmd->command = first;
    p++;

    // Parse integer command code
    int code = 0;
    while (*p >= '0' && *p <= '9') {
        code = code * 10 + (*p - '0');
        p++;
    }
    cmd->code = code;

    // Single pass parameter scanning (X, Y, Z, A, F)
    while (*p) {
        while (is_ascii_space(*p)) p++;
        if (*p == '\0' || *p == ';') break;

        char key = to_ascii_upper(*p);
        p++;

        while (is_ascii_space(*p)) p++;
        if (*p == '\0') break;

        char* endptr;
        float val = fast_atof(p, &endptr);
        if (p == endptr) {
            if (*p != '\0') p++;
            continue;
        }
        p = endptr;

        switch (key) {
            case 'X': cmd->x = val; cmd->has_x = true; break;
            case 'Y': cmd->y = val; cmd->has_y = true; break;
            case 'Z': cmd->z = val; cmd->has_z = true; break;
            case 'A': cmd->a = val; cmd->has_a = true; break;
            case 'F': cmd->f = val; cmd->has_f = true; break;
            default: break;
        }
    }

    return true;
}
