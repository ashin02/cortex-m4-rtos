/*
 * AeroRT - Actuator mixer
 *
 * Purpose:
 *   Convert controller efforts into a complete, bounded actuator-command frame.
 *
 * Responsibilities:
 *   Apply a vehicle-specific allocation matrix, handle saturation coherently, and
 *   pass only safety-approved values to the PWM abstraction.
 *
 * Expected dependencies:
 *   Controller output types, vehicle configuration, safety state, and BSP PWM API;
 *   no direct timer registers or kernel internals.
 *
 * TODO:
 *   [ ] Define actuator count, ordering, normalized units, and configuration format.
 *   [ ] Add a replaceable control-allocation/mixing entry point.
 *   [ ] Specify desaturation/authority priorities and report saturation diagnostics.
 *   [ ] Enforce finite values and safe command bounds before BSP publication.
 *   [ ] Apply disarmed/failsafe command selection in coordination with safety policy.
 *   [ ] Publish all channel updates as one coherent frame at a deterministic point.
 *
 * Non-goals:
 *   Controller algorithms, arming policy, or hardware timer configuration.
 *
 * Validation:
 *   Later test axis/sign mapping, saturation corners, invalid values, safe states,
 *   coherent PWM handoff, and execution time.
 */
