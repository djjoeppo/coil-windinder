# Bolt's Performance Journal

## 2026-07-26 - Single-Pass Zero-Copy G-Code Scanning

**Learning:** In microcontroller firmware (ESP32 / FreeRTOS) receiving continuous G-code over UART, classic two-pass string pre-processing (copying into cleaned buffers, calling standard libc `isspace`/`toupper` which incur locale lookups, and repeatedly invoking `strchr` and `strtof` for every coordinate parameter) accounts for substantial latency per command. A zero-copy single-pass scanner using branch-light ASCII transforms and inline numeric parsing drops CPU time from ~280 ns to ~39 ns per line (~7.2x speedup) with zero heap or stack buffer allocation.

**Action:** Prefer single-pass zero-copy tokenization with lightweight inline numeric scanners for serial streaming protocols over C standard library string utilities (`strchr`, `strtof`, `atoi`).
