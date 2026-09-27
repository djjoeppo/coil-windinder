## 2025-05-10 - Zero-Allocation Single-Pass G-Code Parsing
**Learning:** Standard C functions like `strchr` and `strtof` require multiple string scans and handle locale overhead, creating parsing latency in high-throughput G-code stream processing on embedded targets. A custom single-pass scanner with fast inlined ASCII/numeric converters eliminates memory copies, stack allocations, and string re-traversals.
**Action:** Replace multi-pass standard string functions in embedded command parsers with single-pass pointer scanners and `double`-based division for exact floating point conversion.
