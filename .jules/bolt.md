## 2025-02-18 - Fast Single-Pass G-code Parsing over Libc/Strtof Scanner
**Learning:** Standard C string operations (`strchr`, `strtof`, `toupper`, `isspace`) introduce heavy CPU overhead on embedded microcontrollers like ESP32 when processing rapid streams of G-code commands. Copying lines into temporary buffers for uppercase normalization multiplies memory traffic.
**Action:** Use a single-pass zero-copy character scanner with inline fast float/int parsers (`fast_atof`, `fast_atoi`) that skip whitespace and inline comments without modifying or copying the input string.
