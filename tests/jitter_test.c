/*
 * AeroRT - Periodic-release jitter test plan
 *
 * Purpose:
 *   Quantify absolute periodic release behavior and interference-induced jitter.
 *
 * Responsibilities:
 *   Generate a known periodic task, timestamp expected and actual releases, and compare
 *   unloaded operation against defined stress configurations.
 *
 * Expected dependencies:
 *   Kernel delay_until/timer services, DWT and monotonic timestamps, optional GPIO,
 *   host-side analysis scripts later, and configurable interference workloads.
 *
 * TODO:
 *   [ ] Setup a periodic task using next += period and record scheduled/actual times.
 *   [ ] Verify phase does not drift when individual releases are delayed.
 *   [ ] Measure signed release jitter, maximum, mean, p99, and p99.9 from real samples.
 *   [ ] Repeat with UART flood, CPU hog, DMA traffic, and nested interrupts.
 *   [ ] Separate timer IRQ latency, wakeup-to-run latency, and measurement overhead.
 *   [ ] Record configuration, firmware revision, clocks, sample count, and raw evidence.
 *
 * Non-goals:
 *   Inventing acceptance limits or publishing measurements not taken on hardware.
 *
 * Validation:
 *   Fail functional checks on phase drift, timestamp/order loss, or wrong release count;
 *   report measured jitter rather than silently converting it to pass/fail.
 */
