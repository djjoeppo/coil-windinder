# Bolt's Journal - Critical Learnings

## 2025-08-26 - Single-Pass G-Code Parsing Optimization
**Learning:** Standard multi-pass G-code parsing using `strchr` and intermediate string cleanups incurs repeated memory copies and O(K*N) scans. A single-pass scanner that parses command codes and axis parameters directly in O(N) time with zero allocations significantly improves throughput for high-frequency motion streams over UART.
**Action:** Replace `strchr`/`strtof` multi-pass parsing in `gcode_parser.c` with a fast single-pass scanner.
