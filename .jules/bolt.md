## 2025-03-28 - Zero-Allocation Single-Pass G-Code Parser Optimization
**Learning:** Standard C library string/float parsing functions like `strchr` and `strtof` introduce severe multi-pass overhead and locale checking latency in serial stream parsers. Replacing them with a single-pass ASCII scanner using `fast_atoi` and `fast_atof` (with exact fractional accumulation via integer division) yields an ~8x-9x parsing speedup.
**Action:** Use single-pass zero-allocation parsing for streaming inputs in critical paths on embedded systems.
