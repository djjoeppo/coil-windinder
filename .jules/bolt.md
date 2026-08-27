## 2024-08-27 - Single-Pass G-code Parsing in C Firmware
**Learning:** Multi-pass string scanning using `strchr`, `toupper()`, and `strtof()` introduces significant CPU overhead and string allocations/copies in UART stream parsers. Replacing it with a single-pass ASCII scanner and custom inline float parsing (`fast_atof`) achieves a ~7.5x speedup without memory allocation.
**Action:** Prefer single-pass inline ASCII parsers for streaming line-based commands (like G-code) in embedded microcontrollers instead of relying on standard libc string utilities.
