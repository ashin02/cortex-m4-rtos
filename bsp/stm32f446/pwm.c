/*
 * AeroRT - STM32F446 actuator PWM output
 *
 * Purpose:
 *   Eventually translate validated actuator commands into deterministic, bounded
 *   hardware timer updates.
 *
 * Responsibilities:
 *   PWM timer/channel setup, synchronized command updates, electrical-safe limits,
 *   and a direct failsafe-output primitive.
 *
 * Expected dependencies:
 *   BSP clock/GPIO configuration, board output mapping, STM32 timer registers, and
 *   validated commands from flight mixer/safety interfaces.
 *
 * TODO:
 *   [ ] Select timer channels, output frequency/resolution, polarity, and safe reset
 *       values from actual actuator requirements.
 *   [ ] Convert bounded command units to timer compare values without overflow.
 *   [ ] Use preload/synchronous update behavior to avoid channel-to-channel skew.
 *   [ ] Clamp or reject unsafe values and expose explicit status to safety logic.
 *   [ ] Define immediate and latched failsafe output behavior for fault paths.
 *   [ ] Document when the high-priority flight task may publish a complete frame.
 *
 * Non-goals:
 *   Controller/mixer algorithms, arming decisions, or dynamic memory.
 *
 * Validation:
 *   Scope period, pulse bounds, simultaneous updates, startup/failsafe outputs, and
 *   response timing from flight-task publication to physical pins.
 */
