## 2025-02-13 - Fast single-pass G-code scanner vs multi-pass strchr/strtof

**Learning:** Replacing multi-pass string manipulations (`strchr`, `atoi`, `strtof`, `toupper`, `isspace`) with a single-pass zero-allocation scanner using custom inline ASCII space/case converters and fast number parsers (`fast_atof`, `fast_atoi`) yields a ~9.1x speedup in parsing throughput (~0.178s vs ~1.619s for 10M operations). This drastically reduces core CPU execution time during high-rate G-code streaming over UART.
**Action:** When parsing short structured ASCII protocols like G-code in embedded firmware, avoid standard library float conversion and repeated `strchr` searches in favor of single-pass index/pointer scanning.
