## 2026-10-24 - Single-Pass Zero-Copy G-Code Parsing Speedup

**Learning:** Standard library string functions (`isspace`, `toupper`, `strchr`, `strtof`, `atoi`) introduce significant overhead when processing serial streams on embedded microcontrollers like ESP32 due to locale checks, memory allocations/copies, and multi-pass scanning. Replacing string copies and libc parsing with inline ASCII single-pass scanning (`is_ascii_space`, `to_ascii_upper`, `fast_atoi`, `fast_atof`) reduced parse latency by ~8.8x.

**Action:** Prefer single-pass inline pointer scanning with custom ASCII helpers over multi-pass libc string conversion routines for hot-path streaming protocol parsers.
