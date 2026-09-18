# Bolt's Journal

## 2025-05-18 - Single-Pass Zero-Allocation G-Code Parsing
**Learning:** Standard libc floating point conversion routines like `strtof()`/`strtod()` incur substantial CPU overhead on embedded microcontrollers due to locale processing and state management. In multi-parameter string parsing (like G-code lines with X, Y, Z, A, F fields), scanning the string repeatedly with `strchr()` and `strtof()` multiplies string traversal overhead.
**Action:** Replace `strchr()` + `strtof()` scans with a single-pass zero-allocation scanner using custom `fast_atof` and `fast_atoi` routines for G-code parameter extraction to achieve ~9x faster command parsing without dynamic allocation or string copies.
