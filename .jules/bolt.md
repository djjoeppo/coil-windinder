# Bolt's Journal - Critical Performance Learnings

## 2026-07-26 - G-Code Parsing Micro-Optimization
**Learning:** Standard library string scanning (`strchr`, `strlen`, `strtof`, `atoi`) on small microcontrollers like the ESP32 introduces significant overhead from repeated scanning and locale-aware standard library floating-point parsing. A single-pass, zero-copy scanner with custom inline ASCII space detection and fast numeric conversion reduces parsing latency from ~270 ns to ~35-40 ns (over 6.5x-7.5x speedup), avoiding intermediate buffer allocations and multiple linear passes over the input string.
**Action:** When parsing high-frequency serial streams or G-code lines on embedded targets, use zero-copy single-pass scanners with simple direct character classification rather than multiple `strchr`/`strtof` passes.
