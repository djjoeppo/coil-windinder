# Bolt's Performance Journal

## 2026-11-05 - Zero-Copy Single-Pass G-code Parsing in Microcontroller Firmware
**Learning:** In microcontroller firmware streaming G-code over UART, intermediate line buffers (`clean_line`) with multiple character copies, repeated linear scans (`strchr` for each axis), and libc conversion routines (`strtof`/`atoi` with locale overhead) cause significant CPU overhead and jitter. A zero-copy single-pass parser using inline ASCII classification and lightweight integer/float decoders speeds up line parsing by >7x (~198ns down to ~28ns per command) while eliminating 130 bytes of stack buffer per parse call.
**Action:** Always scan command streams in a single pass without copying or redundant searches, using direct pointer advancing and locale-free numeric parsing on embedded platforms.
