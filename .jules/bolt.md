## 2025-05-18 - Single-Pass G-Code Parser Optimization
**Learning:** Standard C string processing functions (`strlen`, `isspace`, `toupper`, `strchr`, `strtof`, `atoi`) create significant overhead in microcontrollers when processing streams of short strings like G-code commands due to multi-pass scans and locale overhead.
**Action:** Replace multi-pass string manipulations in high-frequency string parsing routines with zero-allocation single-pass pointer scanners and specialized inline numeric converters (`fast_atof`, `fast_atoi`).
