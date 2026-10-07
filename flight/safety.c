/*
 * AeroRT - Flight safety state management
 *
 * Purpose:
 *   Own explicit arming, disarming, failsafe, and critical-fault state transitions.
 *
 * Responsibilities:
 *   Validate safety-critical commands, latch faults as required, select safe actuator
 *   behavior, and coordinate health/watchdog decisions at the highest policy level.
 *
 * Expected dependencies:
 *   Validated RC/command input, flight health summaries, mixer/PWM safe-output API,
 *   monotonic time, and static fault/event storage.
 *
 * TODO:
 *   [ ] Define a reviewable safety state machine with guarded transitions.
 *   [ ] Require explicit, fresh, authenticated-as-applicable ARM/DISARM commands;
 *       queue-full or lost-command conditions must be visible failures.
 *   [ ] Define failsafe triggers for stale commands, sensor/driver failure, deadline
 *       faults, flight-task failure, and health-monitor failure.
 *   [ ] Select immediate versus latched responses and safe actuator values per fault.
 *   [ ] Coordinate watchdog gating without allowing safety logic to hide dead tasks.
 *   [ ] Give safety events bounded, high-priority handling without blocking in ISRs.
 *
 * Non-goals:
 *   Low-level drivers, estimator/controller math, or silent command overwrite.
 *
 * Validation:
 *   Exhaustively test state transitions, stale/invalid commands, simultaneous faults,
 *   restart/latching semantics, and observed actuator failsafe timing.
 */
