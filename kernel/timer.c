/*
 * AeroRT - Kernel time and timeout management
 *
 * Purpose:
 *   Turn a monotonic BSP clock and next-event timer into portable absolute-time,
 *   tickless delays, periodic releases, and synchronization timeouts.
 *
 * Responsibilities:
 *   Maintain a fixed-capacity delayed-event structure, release due tasks, and
 *   program the next event without embedding STM32 register knowledge.
 *
 * Expected dependencies:
 *   BSP monotonic-time/one-shot interfaces, scheduler wakeup services, task timing
 *   metadata, critical sections, and deadline bookkeeping.
 *
 * TODO:
 *   [ ] Define the monotonic time type, resolution contract, and infinity value.
 *   [ ] Implement wraparound-safe comparisons with a documented maximum horizon.
 *   [ ] Provide delay_until(absolute_time) and timeout registration/cancellation.
 *   [ ] Store delayed tasks/events in a statically allocated binary min-heap:
 *       O(1) earliest lookup and bounded O(log N) insertion/removal.
 *   [ ] Advance periodic releases by absolute next += period, never relative sleep.
 *   [ ] Drain all events due at one timestamp in deterministic order.
 *   [ ] Program the BSP next-event timer after every earliest-event change.
 *   [ ] Handle already-due deadlines and hardware-counter wraparound safely.
 *   [ ] Define atomic behavior when an object signal races a timeout.
 *
 * Non-goals:
 *   TIM2/TIM5 configuration or application-level overrun/freshness decisions.
 *
 * Validation:
 *   Test heap invariants/capacity, equal deadlines, cancellation, wrap boundaries,
 *   absolute periodic phase, and next-event reprogramming with a fake clock.
 */
