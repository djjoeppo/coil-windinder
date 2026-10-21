# Bolt's Journal - Critical Learnings

## 2025-02-15 - [G-Code Parser Scan Complexity Reduction]
**Learning:** In resource-constrained embedded systems like the ESP32, repeated string scanning with `strchr` followed by multiple standard library `strtof` calls introduces significant parsing latency and CPU overhead. Transitioning to a single-pass O(N) token scanner reduces the time spent in string scanning linearly, freeing valuable clock cycles on Core 0.
**Action:** Always parse incoming command strings in a single pass (O(N)), extracting keys and values on-the-fly, instead of scanning for keys individually.

## 2025-02-15 - [Feedback Control Loop Timing Alignment]
**Learning:** A critical timing mismatch occurs when a hardware control loop (e.g. Z-axis Force Seek) executes at a high frequency (e.g., 5ms iterations) while the underlying sensor (e.g. HX711) is updated at a lower frequency (e.g., 50ms task delay). This mismatch results in multiple control actions being taken on stale sensor data, causing massive physical overshoot.
**Action:** Align control loop delays with the hardware sensor sampling and filtering rates, and use proportional steps to safely slow down as the target is approached.
