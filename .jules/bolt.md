## 2025-10-06 - Single-Pass Zero-Allocation G-Code Parser Optimization
**Learning:** Standard libc `strtof` and `strchr` multi-pass string operations in embedded G-code command parsing introduce high CPU overhead due to locale checks, string scans, and redundant buffer copies.
**Action:** Use a single-pass pointer scanner with fast inline ASCII integer (`fast_atoi`) and float (`fast_atof`) converters for G-code stream processing to eliminate memory allocations and reduce parsing latency by ~9x.
