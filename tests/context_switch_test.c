/*
 * AeroRT - Context-switch test plan
 *
 * Purpose:
 *   Verify that each task retains an independent, valid Cortex-M execution context.
 *
 * Responsibilities:
 *   Exercise repeated switches and detect corruption of registers, stacks, PSP, or
 *   exception-return state; later cover floating-point context explicitly.
 *
 * Expected dependencies:
 *   A hardware test harness, instrumented tasks with static stacks, scheduler/port
 *   test hooks, debugger inspection, and optional GPIO/DWT timestamps.
 *
 * TODO:
 *   [ ] Setup two or more tasks with unique register patterns and guarded stacks.
 *   [ ] Force repeated voluntary and preemptive switches at varied instruction points.
 *   [ ] Verify R4-R11 software preservation and hardware-frame register continuity.
 *   [ ] Verify PSP belongs to the running task, MSP serves exceptions, and both remain
 *       correctly aligned and within bounds.
 *   [ ] Add an optional Cortex-M4F case for active/inactive FP context and lazy stacking.
 *   [ ] Measure switch duration distributions without asserting an invented target.
 *
 * Non-goals:
 *   Implementing the switcher or treating debugger-only success as full validation.
 *
 * Validation:
 *   Fail on any register/guard/stack-pointer mismatch, fault, missed task progress,
 *   invalid exception return, or unexplained measurement outlier.
 */
