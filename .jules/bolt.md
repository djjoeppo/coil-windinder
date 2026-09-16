## 2025-05-18 - Single-pass zero-allocation G-code parsing

**Learning:** Replacing multi-pass string searching (`strchr`), string buffer copying, and `strtof`/`atoi` with a single-pass scanner using inline ASCII conversion (`is_ascii_space`, `to_ascii_upper`) and fast custom number parsing (`fast_atof`, `fast_atoi`) eliminates libc locale lookup overhead and redundant character scans, achieving a ~9.4x performance improvement for streaming G-code parsing.
**Action:** Use single-pass pointer scanning and custom inline number parsing instead of multi-pass libc string functions (`strchr`, `strtof`) for packet and command parsing in hot streaming execution paths.
