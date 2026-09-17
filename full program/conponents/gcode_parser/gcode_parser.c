#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Fast ASCII space check avoiding locale overhead
static inline bool is_ascii_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Fast ASCII uppercase conversion
static inline char to_ascii_upper(char c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

// Zero-allocation inline float parser bypassing strtof/locale
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

// Zero-allocation inline int parser bypassing atoi
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

// Optimized single-pass G-code line parser
bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line) return false;

    const char *p = line;
    while (*p && is_ascii_space(*p)) p++;
    if (!*p) return false;

    char c = to_ascii_upper(*p);
    if (c != 'G' && c != 'M') return false;

    memset(cmd, 0, sizeof(gcode_command_t));
    cmd->command = c;
    p++;

    cmd->code = fast_atoi(&p);

    while (*p) {
        while (*p && is_ascii_space(*p)) p++;
        if (!*p || *p == ';' || *p == '(') break;

        char key = to_ascii_upper(*p);
        p++;

        switch (key) {
            case 'X':
                cmd->x = fast_atof(&p);
                cmd->has_x = true;
                break;
            case 'Y':
                cmd->y = fast_atof(&p);
                cmd->has_y = true;
                break;
            case 'Z':
                cmd->z = fast_atof(&p);
                cmd->has_z = true;
                break;
            case 'A':
                cmd->a = fast_atof(&p);
                cmd->has_a = true;
                break;
            case 'F':
                cmd->f = fast_atof(&p);
                cmd->has_f = true;
                break;
            default:
                while (*p && !is_ascii_space(*p)) p++;
                break;
        }
    }

    return true;
}
