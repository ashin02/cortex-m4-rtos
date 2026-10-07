/*
 * AeroRT - Queue overflow and freshness-policy test plan
 *
 * Purpose:
 *   Verify generic queue-full behavior and distinct application policies for sensors,
 *   safety commands, and telemetry.
 *
 * Responsibilities:
 *   Fill fixed queues, trigger producer/consumer races, and prove that each data class
 *   reports or discards data according to an explicit policy.
 *
 * Expected dependencies:
 *   Static queue storage, deterministic producer/consumer tasks or host seams, timeout
 *   controls, application adapters, and diagnostic counters.
 *
 * TODO:
 *   [ ] Setup capacities including one, wraparound, and intentionally full queues.
 *   [ ] Verify generic send reports full and never silently overwrites by default.
 *   [ ] Verify sensor adapter keeps/chooses fresh data and accounts for dropped old data.
 *   [ ] Verify ARM/DISARM/FAILSAFE command overflow is an explicit safety-visible error.
 *   [ ] Verify telemetry can drop/coalesce records without blocking critical tasks.
 *   [ ] Exercise blocked sender/receiver timeouts and ISR producer handoff races.
 *   [ ] Measure bounded operation and wakeup costs at empty/full transitions.
 *
 * Non-goals:
 *   Embedding application overflow policy inside the generic kernel queue.
 *
 * Validation:
 *   Fail on silent command loss, wrong retained sensor sample, missing drop/error count,
 *   memory overwrite, deadlock, incorrect wakeup, or unbounded critical-path blocking.
 */
