/*
 * AeroRT - Flight state estimator
 *
 * Purpose:
 *   Define the application interface for converting validated, timestamped sensor
 *   observations into an estimated vehicle state.
 *
 * Responsibilities:
 *   Validate inputs, update estimator state, publish coherent outputs, and bracket
 *   execution with timing-budget instrumentation.
 *
 * Expected dependencies:
 *   Flight sensor data types, calibration/configuration, monotonic sample time, and
 *   monitoring hooks. It must not depend on scheduler internals or hardware registers.
 *
 * TODO:
 *   [ ] Define estimator input/output/state types and explicit units/reference frames.
 *   [ ] Specify initialization, reset, validity, and numerical-failure contracts.
 *   [ ] Add a state-update entry point driven by fresh IMU samples and timestamps.
 *   [ ] Reserve a replaceable attitude/state-estimation algorithm implementation.
 *   [ ] Record runtime against an assigned budget without logging in the hot path.
 *   [ ] Define behavior for stale, missing, out-of-order, or implausible input.
 *
 * Non-goals:
 *   Choosing an algorithm now, acquiring sensors, or implementing RTOS services.
 *
 * Validation:
 *   Later use recorded sensor vectors, numerical edge cases, validity transitions,
 *   and on-target runtime measurements with an independently checked reference.
 */
