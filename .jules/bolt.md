## 2025-05-18 - Optimized G-code Parser Latency with Single-Pass Zero-Allocation Scanner

**Learning:** `gcode_parse_line()` was previously using `memset` and `strchr` multiple times per line to find axis parameters (`X`, `Y`, `Z`, `A`, `F`), accompanied by libc `strtof` overhead. A zero-allocation single-pass scanner with custom `fast_atof` and ASCII helpers bypasses libc locale checking and eliminates intermediate `clean_line` string copying.

**Action:** Replace multi-pass string search routines in real-time parser tasks with single-pass zero-allocation linear scans and fast locale-free conversion helpers.
