/*
 * AeroRT - STM32F446 GPIO support
 *
 * Purpose:
 *   Own board pin setup plus fast observability pins for timing measurements.
 *
 * Responsibilities:
 *   Basic mode/alternate-function initialization, optional status LED control, and
 *   low-overhead set/clear/toggle operations for logic-analyzer markers.
 *
 * Expected dependencies:
 *   BSP clock gates, board pin mapping, and STM32 GPIO registers.
 *
 * TODO:
 *   [ ] Define a board-specific pin map and prevent conflicting peripheral claims.
 *   [ ] Configure modes, pulls, speed, type, and alternate functions explicitly.
 *   [ ] Reserve named debug pins for context switch, ISR wakeup, flight-loop, and
 *       critical-section measurements where enough pins exist.
 *   [ ] Implement atomic BSRR-based fast markers with documented call overhead.
 *   [ ] Add optional LED/status output that never delays critical execution.
 *   [ ] Establish safe reset/default levels before changing output modes.
 *
 * Non-goals:
 *   Timing statistics interpretation or application control policy.
 *
 * Validation:
 *   Check pin mux and electrical defaults; scope marker pulse width and correlate
 *   GPIO traces with DWT timestamps.
 */
