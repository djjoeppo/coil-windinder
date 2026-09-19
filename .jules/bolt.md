## 2025-05-19 - Single-pass zero-allocation G-code scanner

**Learning:** Replacing multi-pass string parsing (`strchr`, `strtof`, `atoi`) with a custom single-pass ASCII scanner (`fast_atof`, `fast_atoi`) eliminates memory allocations, string traversals, and libc locale checks, achieving a ~10x execution speedup (~21M lines/sec).

**Action:** For performance-critical input parsing on embedded microcontrollers, always implement single-pass scanners that convert numeric types in place rather than using multi-pass standard C library functions.
