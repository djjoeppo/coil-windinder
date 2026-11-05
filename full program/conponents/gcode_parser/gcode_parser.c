#include "gcode_parser.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Fast ASCII whitespace check to bypass locale lookup overhead
static inline bool is_ascii_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' ||
         c == '\f';
}

// Fast ASCII uppercase converter
static inline char to_ascii_upper(char c) {
  return (c >= 'a' && c <= 'z') ? (char)(c - ('a' - 'A')) : c;
}

// Fast non-allocating integer scanner
static inline int fast_atoi(const char **p) {
  const char *s = *p;
  while (*s && is_ascii_space(*s)) {
    s++;
  }
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

// Fast non-allocating float scanner bypassing libc strtof/locale overhead
static inline float fast_atof(const char **p) {
  const char *s = *p;
  while (*s && is_ascii_space(*s)) {
    s++;
  }
  float sign = 1.0f;
  if (*s == '-') {
    sign = -1.0f;
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
    double frac = 0.1;
    while (*s >= '0' && *s <= '9') {
      val += (*s - '0') * frac;
      frac *= 0.1;
      s++;
    }
  }
  *p = s;
  return (float)(sign * val);
}

/**
 * Optimized G-code line parser:
 * - Single-pass, zero-copy scanning directly on the input string.
 * - Eliminates clean_line intermediate buffer and copy loop.
 * - Eliminates 5x strchr scans across the line.
 * - Replaces strtof / atoi with fast inline numeric scanners.
 */
bool gcode_parse_line(const char *line, gcode_command_t *cmd) {
  if (!line) {
    return false;
  }

  const char *p = line;
  while (*p && is_ascii_space(*p)) {
    p++;
  }
  if (*p == '\0') {
    return false;
  }

  char c = to_ascii_upper(*p);
  if (c != 'G' && c != 'M') {
    return false;
  }

  memset(cmd, 0, sizeof(gcode_command_t));
  cmd->command = c;
  p++;

  cmd->code = fast_atoi(&p);

  while (*p) {
    while (*p && is_ascii_space(*p)) {
      p++;
    }
    if (*p == '\0') {
      break;
    }

    char key = to_ascii_upper(*p);
    p++;
    float val = fast_atof(&p);
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
      break;
    }
  }
  return true;
}
