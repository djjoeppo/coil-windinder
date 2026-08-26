#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Helper to quickly check for whitespace without libc/locale overhead
static inline bool is_ascii_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Helper to convert lowercase letter to uppercase
static inline char to_ascii_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 32);
    }
    return c;
}

// Fast inline string-to-float parser for G-code numeric parameters (bypasses libc locale overhead)
static inline float fast_atof(const char *p, const char **endptr) {
    const char *start = p;
    float sign = 1.0f;
    if (*p == '-') {
        sign = -1.0f;
        p++;
    } else if (*p == '+') {
        p++;
    }

    bool has_digits = false;
    float val = 0.0f;
    while (*p >= '0' && *p <= '9') {
        has_digits = true;
        val = val * 10.0f + (*p - '0');
        p++;
    }

    if (*p == '.') {
        p++;
        long frac = 0;
        long divisor = 1;
        while (*p >= '0' && *p <= '9') {
            has_digits = true;
            frac = frac * 10 + (*p - '0');
            divisor *= 10;
            p++;
        }
        val += (float)frac / (float)divisor;
    }

    if (!has_digits) {
        if (endptr) *endptr = start; // Reset endptr if no valid digits parsed
        return 0.0f;
    }

    if (endptr) *endptr = p;
    return val * sign;
}

/**
 * ⚡ Bolt Optimization: Single-pass zero-allocation G-code line parser.
 *
 * Scans the G-code string in a single O(N) pass, parsing G/M command codes and axis/feed parameters
 * directly without intermediate buffer allocation or repeated strchr/strtof scans.
 */
bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    if (!line) return false;

    // Fast-forward leading spaces
    const char *p = line;
    while (*p && is_ascii_space(*p)) {
        p++;
    }

    if (*p == '\0') return false;

    char first_letter = to_ascii_upper(*p);
    if (first_letter != 'G' && first_letter != 'M') {
        return false;
    }

    memset(cmd, 0, sizeof(gcode_command_t));
    cmd->command = first_letter;
    p++;

    // Skip whitespace between command letter and code if any (e.g. "G 1")
    while (*p && is_ascii_space(*p)) {
        p++;
    }

    // Parse command code integer (e.g., G0, G1, M114)
    int code_val = 0;
    while (*p >= '0' && *p <= '9') {
        code_val = code_val * 10 + (*p - '0');
        p++;
    }
    cmd->code = code_val;

    // Single-pass scan for parameters (X, Y, Z, A, F)
    while (*p) {
        // Skip whitespace
        while (*p && is_ascii_space(*p)) {
            p++;
        }
        if (*p == '\0') break;

        char key = to_ascii_upper(*p);
        p++;

        // Parse float value after parameter letter
        if ((key >= 'A' && key <= 'Z')) {
            // Skip spaces between key letter and number if any
            while (*p && is_ascii_space(*p)) {
                p++;
            }

            const char *next_p = p;
            float val = fast_atof(p, &next_p);

            if (next_p > p) { // Number was parsed successfully
                switch (key) {
                    case 'X': cmd->x = val; cmd->has_x = true; break;
                    case 'Y': cmd->y = val; cmd->has_y = true; break;
                    case 'Z': cmd->z = val; cmd->has_z = true; break;
                    case 'A': cmd->a = val; cmd->has_a = true; break;
                    case 'F': cmd->f = val; cmd->has_f = true; break;
                    default: break; // Ignore unsupported parameter letters
                }
                p = next_p;
            } else {
                p = next_p;
            }
        }
    }

    return true;
}
