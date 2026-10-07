/*
 * AeroRT - Deadline and periodic-job accounting
 *
 * Purpose:
 *   Make release, deadline, miss, and overrun behavior observable and explicit.
 *
 * Responsibilities:
 *   Derive absolute deadlines from releases, check completion time, update fixed
 *   counters, and support the flight-oriented stale-release recovery policy.
 *
 * Expected dependencies:
 *   Task timing fields, monotonic kernel time, periodic timer services, scheduler
 *   release hooks, and monitoring event hooks.
 *
 * TODO:
 *   [ ] Validate and store period and relative deadline task parameters.
 *   [ ] On release, set absolute_deadline = release + relative_deadline and update
 *       release counters using wraparound-safe arithmetic.
 *   [ ] Compare recorded finish time with absolute deadline and count every miss.
 *   [ ] Define overrun as a prior job still active at its next release and count it.
 *   [ ] Skip obsolete periodic releases after an overrun instead of replaying stale
 *       flight-control jobs back-to-back; resume at the next relevant phase point.
 *   [ ] Make saturation/overflow behavior of diagnostic counters explicit.
 *   [ ] Emit lightweight monitoring hooks without distorting the measured path.
 *
 * Non-goals:
 *   A scheduler algorithm, control-law recovery, or fabricated timing results.
 *
 * Validation:
 *   Test boundary completion times, multiple missed periods, counter behavior,
 *   phase recovery, and wraparound cases with a deterministic simulated clock.
 */
