# Bolt's Performance Journal

Critical learnings only. No routine logs.

## 2025-11-01 - Single-pass zero-copy scanner for G-code stream parsing
**Learning:** In microcontroller command streaming (Core 0 UART loop), repeated scans (`strchr` for every possible coordinate axis) and libc conversions (`strtof`/`atoi` with locale/errno overhead) plus buffer copying (`clean_line`) caused significant CPU overhead (~270 ns per line, or ~3.7M ops/sec). A single-pass scanner using branch-predictable ASCII checks (`to_ascii_upper`, `is_ascii_space`) and fast numeric scanning (`fast_atof`, `fast_atoi`) directly on the input pointer achieves ~30-33 ns per line (>29M ops/sec, ~8.8x speedup) with zero allocations and no buffer copying.
**Action:** When parsing high-throughput streaming protocols or ASCII commands on embedded targets, favor single-pass zero-copy scanning over multi-pass `strchr` + standard libc float parsers.
