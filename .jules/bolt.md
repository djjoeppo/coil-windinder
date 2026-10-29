## 2025-05-18 - Single-Pass Zero-Copy G-Code Scanning

**Learning:** Standard C string routines (`strchr`, `strtof`, `atoi`) and intermediate string buffer copying inside real-time stream loops cause major performance degradation (~7.7x overhead) and unnecessary stack allocations on ESP32 microcontrollers.

**Action:** Replace multi-pass string searches and libc float conversion functions in serial stream command handlers with single-pass zero-copy scanners and inline ASCII conversion functions (`fast_atof`/`fast_atoi`).
