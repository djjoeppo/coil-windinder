#include "gcode_parser.h"
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Original
static float get_value_orig(const char *s, char key, bool *found) {
  const char *p = strchr(s, key);
  if (p) {
    *found = true;
    return strtof(p + 1, NULL);
  }
  *found = false;
  return 0.0f;
}

bool gcode_parse_line_orig(const char *line, gcode_command_t *cmd) {
  if (!line || strlen(line) == 0)
    return false;

  memset(cmd, 0, sizeof(gcode_command_t));

  char clean_line[130];
  int j = 0;
  for (int i = 0; line[i] && j < 128; i++) {
    if (!isspace((int)line[i])) {
      clean_line[j++] = toupper((int)line[i]);
    }
  }
  clean_line[j] = '\0';

  if (clean_line[0] == 'G' || clean_line[0] == 'M') {
    cmd->command = clean_line[0];
    cmd->code = atoi(clean_line + 1);

    cmd->x = get_value_orig(clean_line, 'X', &cmd->has_x);
    cmd->y = get_value_orig(clean_line, 'Y', &cmd->has_y);
    cmd->z = get_value_orig(clean_line, 'Z', &cmd->has_z);
    cmd->a = get_value_orig(clean_line, 'A', &cmd->has_a);
    cmd->f = get_value_orig(clean_line, 'F', &cmd->has_f);

    return true;
  }

  return false;
}

// Optimized
static inline bool is_ascii_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline char to_ascii_upper(char c) {
  if (c >= 'a' && c <= 'z')
    return c - ('a' - 'A');
  return c;
}

static inline int fast_atoi(const char **p) {
  int val = 0;
  while (**p >= '0' && **p <= '9') {
    val = val * 10 + (**p - '0');
    (*p)++;
  }
  return val;
}

static inline float fast_atof(const char **p) {
  float sign = 1.0f;
  if (**p == '-') {
    sign = -1.0f;
    (*p)++;
  } else if (**p == '+') {
    (*p)++;
  }

  float val = 0.0f;
  while (**p >= '0' && **p <= '9') {
    val = val * 10.0f + (**p - '0');
    (*p)++;
  }

  if (**p == '.') {
    (*p)++;
    float factor = 0.1f;
    while (**p >= '0' && **p <= '9') {
      val += (**p - '0') * factor;
      factor *= 0.1f;
      (*p)++;
    }
  }

  return sign * val;
}

bool gcode_parse_line_opt(const char *line, gcode_command_t *cmd) {
  if (!line)
    return false;

  const char *p = line;
  while (*p && is_ascii_space(*p))
    p++;
  if (*p == '\0')
    return false;

  char cmd_char = to_ascii_upper(*p);
  if (cmd_char != 'G' && cmd_char != 'M')
    return false;
  p++;

  memset(cmd, 0, sizeof(gcode_command_t));
  cmd->command = cmd_char;

  while (*p && is_ascii_space(*p))
    p++;
  if (*p >= '0' && *p <= '9') {
    cmd->code = fast_atoi(&p);
  } else {
    return false;
  }

  while (*p) {
    if (is_ascii_space(*p)) {
      p++;
      continue;
    }
    if (*p == ';')
      break; // Comment

    char key = to_ascii_upper(*p);
    p++;

    while (*p && is_ascii_space(*p))
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

int main() {
  const char *lines[] = {"G1 X120.5 Y-45.2 Z1.25 A180.0 F3000",
                         "G0 X0 Y0 Z0 A0",
                         "M114",
                         "G1 X50.25 F1500",
                         "G28",
                         "M402",
                         "  g1   x-12.345   y67.89   z0.001   f500  "};
  int num_lines = sizeof(lines) / sizeof(lines[0]);
  gcode_command_t cmd1, cmd2;

  // Verify correctness first
  for (int i = 0; i < num_lines; i++) {
    bool r1 = gcode_parse_line_orig(lines[i], &cmd1);
    bool r2 = gcode_parse_line_opt(lines[i], &cmd2);
    if (r1 != r2 || cmd1.command != cmd2.command || cmd1.code != cmd2.code ||
        cmd1.has_x != cmd2.has_x ||
        (cmd1.has_x && (fabsf(cmd1.x - cmd2.x) > 0.0001f)) ||
        cmd1.has_y != cmd2.has_y ||
        (cmd1.has_y && (fabsf(cmd1.y - cmd2.y) > 0.0001f)) ||
        cmd1.has_z != cmd2.has_z ||
        (cmd1.has_z && (fabsf(cmd1.z - cmd2.z) > 0.0001f)) ||
        cmd1.has_a != cmd2.has_a ||
        (cmd1.has_a && (fabsf(cmd1.a - cmd2.a) > 0.0001f)) ||
        cmd1.has_f != cmd2.has_f ||
        (cmd1.has_f && (fabsf(cmd1.f - cmd2.f) > 0.0001f))) {
      printf("Mismatch on line: %s\n", lines[i]);
      return 1;
    }
  }
  printf("Verification succeeded: outputs match perfectly!\n");

  int iterations = 1000000;
  clock_t start = clock();
  for (int i = 0; i < iterations; i++) {
    gcode_parse_line_orig(lines[i % num_lines], &cmd1);
  }
  clock_t mid = clock();
  for (int i = 0; i < iterations; i++) {
    gcode_parse_line_opt(lines[i % num_lines], &cmd2);
  }
  clock_t end = clock();

  double t_orig = ((double)(mid - start)) / CLOCKS_PER_SEC;
  double t_opt = ((double)(end - mid)) / CLOCKS_PER_SEC;

  printf("Original: %f s (%.2f ns/op)\n", t_orig, (t_orig / iterations) * 1e9);
  printf("Optimized: %f s (%.2f ns/op)\n", t_opt, (t_opt / iterations) * 1e9);
  printf("Speedup: %.2fx faster!\n", t_orig / t_opt);
  return 0;
}
