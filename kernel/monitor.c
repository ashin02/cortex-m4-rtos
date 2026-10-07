/*
 * AeroRT - Runtime instrumentation and statistics
 *
 * Purpose:
 *   Collect bounded evidence about real-time behavior without claiming unmeasured
 *   performance. Measurement overhead must itself be characterized.
 *
 * Responsibilities:
 *   Aggregate task/runtime, latency, jitter, utilization, deadline, and stack data;
 *   provide snapshots for lower-priority reporting.
 *
 * Expected dependencies:
 *   Cortex-M DWT cycle-counter hooks, scheduler/timer/deadline event hooks, task
 *   metadata, and optional telemetry consumers. No UART driver calls in hot hooks.
 *
 * TODO:
 *   [ ] Define DWT timestamp conversion using an explicitly known core frequency.
 *   [ ] Instrument task runtime and maximum observed runtime/WCET proxy.
 *   [ ] Record context-switch duration, ISR-to-task wakeup latency, periodic release
 *       jitter, critical-section duration, and sensor-to-actuator latency.
 *   [ ] Accumulate CPU utilization with well-defined idle/accounting boundaries.
 *   [ ] Expose stack high-water and deadline miss/overrun statistics.
 *   [ ] Use bounded counters/histograms or trace buffers suitable for p99, p99.9,
 *       and maximum analysis while documenting overflow behavior.
 *   [ ] Provide coherent snapshots for future low-priority UART output.
 *   [ ] Never populate benchmark documentation with invented or simulated numbers.
 *
 * Non-goals:
 *   Printing from interrupt context or treating averages as worst-case guarantees.
 *
 * Validation:
 *   Calibrate hook overhead, cross-check GPIO traces with a logic analyzer, force
 *   counter limits, and compare accounting totals with known synthetic workloads.
 */
