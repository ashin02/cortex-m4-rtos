/*
 * AeroRT - Task management
 *
 * Purpose:
 *   Own the portable task lifecycle and the statically allocated Task Control
 *   Block (TCB) contract. Architecture code may construct the initial machine
 *   frame through a narrow port interface, but task policy belongs here.
 *
 * Responsibilities:
 *   - Initialize caller-provided TCBs and stack buffers without malloc/free.
 *   - Track saved stack pointer, stack bounds, state, base/effective priority,
 *     wake/release/deadline fields, statistics, and kernel-list linkage.
 *   - Validate and perform READY/RUNNING/BLOCKED/SUSPENDED transitions.
 *   - Maintain blocking/unblocking bookkeeping and stack-measurement hooks.
 *
 * Expected dependencies:
 *   Portable scheduler/timer APIs and a Cortex-M port routine that builds the
 *   initial exception frame. It must not depend on STM32 peripheral code.
 *
 * TODO:
 *   [ ] Define the fixed-capacity TCB representation and public task API.
 *   [ ] Accept statically supplied, correctly aligned stacks and reject invalid
 *       sizes, priorities, entry points, or duplicate task registration.
 *   [ ] Fill unused stack memory with a known pattern for later high-water scans.
 *   [ ] Ask the port to construct the initial R0-R3/R12/LR/PC/xPSR frame and
 *       software-saved R4-R11 area, including required 8-byte alignment.
 *   [ ] Initialize base and effective priorities and all timing/statistic fields.
 *   [ ] Specify legal task-state transitions and atomic blocking/unblocking rules.
 *   [ ] Preserve wait-object and timeout linkage without ambiguous ownership.
 *   [ ] Expose bounded stack high-water measurement hooks outside critical paths.
 *   [ ] Guarantee that task creation and later operation require no heap access.
 *
 * Non-goals:
 *   Scheduling decisions, Cortex-M register manipulation, and flight behavior.
 *
 * Validation:
 *   Unit-test TCB initialization and state transitions; inspect synthetic initial
 *   frames; use guard regions and high-water scans on hardware later.
 */
