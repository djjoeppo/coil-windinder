## 2026-03-02 - Zero-Copy Single-Pass G-Code Parser Optimization
**Learning:** Standard library string scanning (`strchr`), heap buffer creation/copying, and locale-aware float conversion (`strtof`) introduce significant runtime overhead during high-frequency stream processing on embedded targets.
**Action:** Replace multi-pass string searches and libc conversion functions with inline single-pass ASCII scanning and custom `fast_atoi`/`fast_atof` routines when parsing stream protocols like G-code.
