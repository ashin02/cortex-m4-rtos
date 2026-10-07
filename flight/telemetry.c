/*
 * AeroRT - Low-priority telemetry
 *
 * Purpose:
 *   Export bounded observability without delaying flight-critical execution.
 *
 * Responsibilities:
 *   Snapshot application/kernel statistics, format selected records outside critical
 *   paths, and submit them asynchronously to UART DMA with explicit drop behavior.
 *
 * Expected dependencies:
 *   Read-only monitor/health snapshots, UART-DMA API, static buffers/queues, and a
 *   low-priority periodic or event-driven task context.
 *
 * TODO:
 *   [ ] Define versioned telemetry records and bounded serialization costs.
 *   [ ] Read coherent runtime, deadline, stack, sensor, and health statistics.
 *   [ ] Use statically allocated buffers with nonblocking UART-DMA submission.
 *   [ ] Drop, coalesce, or reduce low-priority output under overload and count losses.
 *   [ ] Ensure telemetry saturation cannot hold a mutex/resource needed by flight work.
 *   [ ] Rate-limit debug output and keep all formatting outside ISR/kernel hooks.
 *
 * Non-goals:
 *   Reliable delivery at the expense of flight timing or invented benchmark reports.
 *
 * Validation:
 *   Saturate output and DMA, verify loss counters and nonblocking behavior, validate
 *   record snapshots, and measure interference with the control path.
 */
