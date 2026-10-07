/*
 * AeroRT - Flight controller
 *
 * Purpose:
 *   Define the fixed-frequency rate/attitude control interface and its timing and
 *   safety contracts without choosing or implementing a control law yet.
 *
 * Responsibilities:
 *   Consume coherent estimates/setpoints, produce bounded control efforts, expose
 *   reset/mode behavior, and make deadline/runtime assumptions observable.
 *
 * Expected dependencies:
 *   Estimator outputs, validated command/setpoint data, safety state, mixer input
 *   types, and monitoring hooks. The enclosing flight task supplies scheduling.
 *
 * TODO:
 *   [ ] Define rate/attitude command, state, output, unit, and axis conventions.
 *   [ ] Specify the required execution frequency, maximum input age, deadline, and
 *       behavior when a release is skipped after overrun.
 *   [ ] Add placeholders for PID or equivalent state/configuration and update API.
 *   [ ] Specify integrator reset/anti-windup/saturation behavior for future work.
 *   [ ] Reject invalid estimates/commands and route faults to safety logic.
 *   [ ] Bracket execution for observed-WCET and deadline monitoring.
 *
 * Non-goals:
 *   Actual PID/filter math, PWM register access, or RTOS ready-queue manipulation.
 *
 * Validation:
 *   Later test step responses, saturation/fault boundaries, deterministic replay,
 *   fixed-rate timing, and runtime/deadline behavior on target hardware.
 */
