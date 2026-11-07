#include "gcode_parser.h"
#include <stdbool.h>
#include <string.h>

// Performance optimization (Bolt):
// Fast ASCII-only helpers bypass libc locale overhead (isspace, toupper,
// strtof, atoi). Single-pass scanner parses directly from the input string
// without buffer copies or strchr scans.

static inline bool is_ascii_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline char to_ascii_upper(char c) {
  if (c >= 'a' && c <= 'z') {
    return (char)(c - ('a' - 'A'));
  }
  return c;
}

static inline float fast_atof(const char **p) {
  const char *s = *p;
  while (*s == ' ' || *s == '\t') {
    s++;
  }

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
    double frac = 0.1;
    while (*s >= '0' && *s <= '9') {
      val += (*s - '0') * frac;
      frac *= 0.1;
      s++;
    }
  }

  *p = s;
  return (float)(neg ? -val : val);
}

static inline int fast_atoi(const char **p) {
  const char *s = *p;
  while (*s == ' ' || *s == '\t') {
    s++;
  }

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

bool gcode_parse_line(const char *line, gcode_command_t *cmd) {
  if (!line || !*line) {
    return false;
  }

  memset(cmd, 0, sizeof(gcode_command_t));

  const char *p = line;
  while (*p && is_ascii_space(*p)) {
    p++;
  }

  if (*p != 'G' && *p != 'g' && *p != 'M' && *p != 'm') {
    return false;
  }

  cmd->command = to_ascii_upper(*p);
  p++;

  cmd->code = fast_atoi(&p);

  while (*p) {
    while (*p && is_ascii_space(*p)) {
      p++;
    }
    if (!*p) {
      break;
    }

    char c = to_ascii_upper(*p);
    p++;

    switch (c) {
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
      // Skip unknown parameter / token characters until next whitespace
      while (*p && !is_ascii_space(*p)) {
        p++;
      }
      break;
    }
  }

  return true;
}
