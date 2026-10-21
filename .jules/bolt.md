## 2025-10-21 - Single-pass Zero-copy G-code Scanner
**Learning:** Legacy multi-pass string parsing (`strchr` per axis, `strtof`, `atoi`, heap/stack copies) in stream-heavy embedded G-code loops creates significant execution latency and locale overhead.
**Action:** Use a single-pass zero-copy scanner with custom inline ASCII utilities (`is_ascii_space`, `to_ascii_upper`) and fast inline integer/float parsers (`fast_atoi`, `fast_atof`) to achieve an ~8.3x speedup.
