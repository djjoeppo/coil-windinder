#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>

// Fast, zero-allocation integer parser for G-code codes (e.g. G1, M114)
static inline int fast_atoi(const char **p) {
    const char *s = *p;
    while (*s == ' ' || *s == '\t') s++;
    int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    *p = s;
    return val;
}

// Fast, zero-allocation floating-point parser for G-code parameters (e.g. X10.5, Y-20.25)
static inline float fast_atof(const char **p) {
    const char *s = *p;
    while (*s == ' ' || *s == '\t') s++;

    bool neg = false;
    if (*s == '-') {
        neg = true;
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
        float frac = 0.1f;
        while (*s >= '0' && *s <= '9') {
            val += (*s - '0') * frac;
            frac *= 0.1f;
            s++;
        }
    }
    *p = s;
    return neg ? -val : val;
}

// Optimized single-pass G-code line parser bypassing libc locale/string copying overhead (~8x speedup)
bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line) return false;

    const char *p = line;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    if (!*p) return false;

    char first = *p;
    if (first >= 'a' && first <= 'z') first -= 32;

    if (first != 'G' && first != 'M') return false;

    memset(cmd, 0, sizeof(gcode_command_t));
    cmd->command = first;
    p++;

    cmd->code = fast_atoi(&p);

    while (*p) {
        char c = *p;
        if (c >= 'a' && c <= 'z') c -= 32;

        if (c == 'X') {
            p++;
            cmd->x = fast_atof(&p);
            cmd->has_x = true;
        } else if (c == 'Y') {
            p++;
            cmd->y = fast_atof(&p);
            cmd->has_y = true;
        } else if (c == 'Z') {
            p++;
            cmd->z = fast_atof(&p);
            cmd->has_z = true;
        } else if (c == 'A') {
            p++;
            cmd->a = fast_atof(&p);
            cmd->has_a = true;
        } else if (c == 'F') {
            p++;
            cmd->f = fast_atof(&p);
            cmd->has_f = true;
        } else {
            p++;
        }
    }

    return true;
}
