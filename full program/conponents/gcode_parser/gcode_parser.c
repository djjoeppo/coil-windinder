#include "gcode_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- BOLT PERFORMANCE OPTIMIZATIONS ---
// 1. Replaced standard locale-dependent isspace() with a fast ASCII-only inline check
//    to eliminate locale/thread-safety check overhead.
static inline bool is_whitespace_ascii(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

// 2. Replaced standard locale-dependent toupper() with a fast ASCII-only inline macro-style logic.
static inline char to_upper_ascii(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

bool gcode_parse_line(const char* line, gcode_command_t* cmd) {
    // Avoid strlen() call to eliminate a redundant pass over the line.
    if (!line || line[0] == '\0') return false;

    memset(cmd, 0, sizeof(gcode_command_t));
    
    char clean_line[130]; // Slightly larger than input line to be safe
    int j = 0;
    for (int i = 0; line[i] && j < 128; i++) {
        char c = line[i];
        if (!is_whitespace_ascii(c)) {
            clean_line[j++] = to_upper_ascii(c);
        }
    }
    clean_line[j] = '\0';

    char first = clean_line[0];
    if (first == 'G' || first == 'M') {
        cmd->command = first;

        // 3. Fast inline integer parser to replace atoi() for the G/M code,
        //    saving library call overhead.
        int code = 0;
        int i = 1;
        while (clean_line[i] >= '0' && clean_line[i] <= '9') {
            code = code * 10 + (clean_line[i] - '0');
            i++;
        }
        cmd->code = code;
        
        // 4. Single-pass parameter scanning to replace 5 separate strchr() scans.
        //    This iterates through clean_line exactly once, registering pointers
        //    for X, Y, Z, A, and F values.
        const char* p_x = NULL;
        const char* p_y = NULL;
        const char* p_z = NULL;
        const char* p_a = NULL;
        const char* p_f = NULL;

        for (int i = 0; clean_line[i]; i++) {
            char c = clean_line[i];
            if (c == 'X') { if (!p_x) p_x = &clean_line[i + 1]; }
            else if (c == 'Y') { if (!p_y) p_y = &clean_line[i + 1]; }
            else if (c == 'Z') { if (!p_z) p_z = &clean_line[i + 1]; }
            else if (c == 'A') { if (!p_a) p_a = &clean_line[i + 1]; }
            else if (c == 'F') { if (!p_f) p_f = &clean_line[i + 1]; }
        }

        if (p_x) { cmd->has_x = true; cmd->x = strtof(p_x, NULL); }
        if (p_y) { cmd->has_y = true; cmd->y = strtof(p_y, NULL); }
        if (p_z) { cmd->has_z = true; cmd->z = strtof(p_z, NULL); }
        if (p_a) { cmd->has_a = true; cmd->a = strtof(p_a, NULL); }
        if (p_f) { cmd->has_f = true; cmd->f = strtof(p_f, NULL); }
        
        return true;
    }

    return false;
}
