#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Fast inline helper functions to avoid standard library character classification and float conversion overhead.
static inline bool is_ascii_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline char to_ascii_upper(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 32);
    return c;
}

static inline void skip_ascii_spaces(const char **p) {
    while (is_ascii_space(**p)) (*p)++;
}

static inline float fast_atof(const char **p) {
    skip_ascii_spaces(p);
    const char *s = *p;
    bool neg = false;
    if (*s == '-') {
        neg = true;
        s++;
    } else if (*s == '+') {
        s++;
    }

    double val = 0.0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10.0 + (*s - '0');
        s++;
    }

    if (*s == '.') {
        s++;
        double divisor = 10.0;
        while (*s >= '0' && *s <= '9') {
            val += (*s - '0') / divisor;
            divisor *= 10.0;
            s++;
        }
    }

    *p = s;
    return neg ? (float)(-val) : (float)val;
}

static inline int fast_atoi(const char **p) {
    skip_ascii_spaces(p);
    const char *s = *p;
    bool neg = false;
    if (*s == '-') {
        neg = true;
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
    return neg ? -val : val;
}

/*
 * Optimized single-pass G-code parser.
 * Eliminates intermediate buffer copies, multiple strchr searches, and heavy strtof standard library calls.
 * Handles whitespace robustly, avoids precision loss in float conversion, and skips unrecognized keys.
 */
bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line) return false;

    const char *p = line;
    skip_ascii_spaces(&p);
    if (*p == '\0') return false;

    char first = to_ascii_upper(*p);
    if (first != 'G' && first != 'M') return false;

    memset(cmd, 0, sizeof(gcode_command_t));
    cmd->command = first;
    p++;

    cmd->code = fast_atoi(&p);

    while (*p != '\0') {
        skip_ascii_spaces(&p);
        if (*p == '\0' || *p == ';' || *p == '(') break; // End of command or comment

        char key = to_ascii_upper(*p);
        p++;

        if (key >= '0' && key <= '9') {
            // Unexpected standalone number, skip
            continue;
        }

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
                // Skip value of unrecognized key (e.g., E, S, P)
                fast_atof(&p);
                break;
        }
    }

    return true;
}
