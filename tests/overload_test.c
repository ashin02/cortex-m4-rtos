/*
 * AeroRT - Overload behavior test plan
 *
 * Purpose:
 *   Verify predictable flight-critical behavior when CPU, interrupt, DMA, and output
 *   demand exceed normal conditions.
 *
 * Responsibilities:
 *   Apply controlled interference, observe deadline/overrun accounting, and ensure
 *   obsolete periodic releases are skipped rather than replayed in a stale burst.
 *
 * Expected dependencies:
 *   Configurable CPU hog, interrupt/DMA generators, telemetry/logging producers,
 *   deadline monitoring, GPIO/DWT tracing, and the flight-task test double.
 *
 * TODO:
 *   [ ] Setup repeatable load levels and log all clock/priority/load parameters.
 *   [ ] Flood telemetry/logging, run low-priority CPU work, add DMA traffic and nested
 *       interrupts, and combine them in worst-credible scenarios.
 *   [ ] Force a flight-task overrun and verify miss/overrun counters increment once.
 *   [ ] Verify stale releases are discarded and recovery occurs at the next relevant
 *       absolute period without back-to-back catch-up jobs.
 *   [ ] Measure flight deadline, jitter, and sensor-to-actuator behavior under load.
 *   [ ] Verify telemetry degrades before flight-critical work is blocked.
 *
 * Non-goals:
 *   Claiming safety from one workload or using synthetic results as hardware results.
 *
 * Validation:
 *   Fail on stale-job replay, corrupt counters/state, priority inversion, critical-path
 *   blocking by background work, or undocumented load-generator saturation.
 */
