## 2025-02-18 - Single-Pass Zero-Allocation G-Code Scanner
**Learning:** Standard C string operations (`strchr`, `strlen`, `strtof`, `memset`) in G-code command parsers add massive overhead due to multi-pass iteration, stack allocations, and libc locale conversion logic. Replacing them with a single-pass zero-allocation pointer scanner with custom inline ASCII parsing functions yields ~8.8x parsing throughput.
**Action:** Use single-pass inline ASCII pointer walking instead of libc `strchr`/`strtof` for hot stream parsing.
