/*
 * AeroRT - System health supervision
 *
 * Purpose:
 *   Decide whether all required components have made timely, valid progress before
 *   permitting watchdog refresh or reporting the system healthy.
 *
 * Responsibilities:
 *   Track component heartbeats, deadline/overrun evidence, sensor/driver state, and
 *   a coherent fault summary for safety and telemetry.
 *
 * Expected dependencies:
 *   Kernel monitoring snapshots, component heartbeat API, driver/sensor status,
 *   safety policy, monotonic time, and BSP watchdog refresh primitive.
 *
 * TODO:
 *   [ ] Define required components and monotonic heartbeat/progress tokens.
 *   [ ] Detect stale or unchanged heartbeats using per-component time bounds.
 *   [ ] Consume deadline misses, overruns, stack margins, and driver/sensor errors.
 *   [ ] Build a latched/current system fault state with bounded update work.
 *   [ ] Refresh IWDG only after every required condition passes in the current window.
 *   [ ] Ensure a stuck health task cannot keep the watchdog alive accidentally.
 *   [ ] Publish read-only health snapshots to safety and telemetry.
 *
 * Non-goals:
 *   Unconditional watchdog feeding or low-level diagnostic printing.
 *
 * Validation:
 *   Withhold/corrupt each heartbeat and health input, test boundary ages and counter
 *   changes, and confirm watchdog/failsafe behavior under overload.
 */
