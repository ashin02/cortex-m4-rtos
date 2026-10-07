/*
 * AeroRT - Cortex-M4 bounded critical sections
 *
 * Purpose:
 *   Protect kernel data from kernel-aware interrupts while allowing more urgent,
 *   non-kernel-aware interrupts to run.
 *
 * Responsibilities:
 *   Enter/exit nested BASEPRI sections, preserve prior masking state, provide needed
 *   barriers, and expose duration instrumentation hooks.
 *
 * Expected dependencies:
 *   Cortex-M special-register intrinsics, a configured kernel-aware IRQ threshold,
 *   and monitor hooks that do not recursively enter critical sections.
 *
 * TODO:
 *   [ ] Define the BASEPRI threshold from the actual implemented NVIC priority bits.
 *   [ ] Save and restore the previous BASEPRI value safely for nested callers.
 *   [ ] Use BASEPRI_MAX or equivalent semantics so nesting never weakens masking.
 *   [ ] Specify caller/ISR rules and assert unbalanced exits in debug builds.
 *   [ ] Place DSB/ISB/compiler barriers only where the architecture and data-sharing
 *       rules require them, with rationale documented.
 *   [ ] Timestamp outermost entry/exit to measure maximum interrupt-mask duration.
 *   [ ] Reserve PRIMASK/global masking for narrowly justified startup/fault cases.
 *
 * Non-goals:
 *   Long computation, peripheral locking, or masking non-kernel-aware urgent IRQs.
 *
 * Validation:
 *   Test nesting and prior-state restoration; use nested hardware interrupts and
 *   GPIO/DWT traces to verify which priorities remain serviceable.
 */
