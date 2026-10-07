/*
 * AeroRT - Semaphores
 *
 * Purpose:
 *   Supply bounded binary/counting event and resource-count synchronization.
 *
 * Responsibilities:
 *   Initialize static objects, wait/signal tasks, enforce count limits, integrate
 *   timeouts, and offer a minimal ISR-compatible signal path.
 *
 * Expected dependencies:
 *   Scheduler wait/wakeup primitives, kernel timeout services, and critical
 *   sections. Objects and waiter records must use fixed storage.
 *
 * TODO:
 *   [ ] Define binary and counting initialization with validated maximum counts.
 *   [ ] Specify try-wait, timed wait, signal, overflow, and cancellation results.
 *   [ ] Block and wake tasks with deterministic priority/FIFO waiter ordering.
 *   [ ] Resolve signal/timeout races while keeping scheduler invariants intact.
 *   [ ] Provide ISR signal semantics that do bounded work and request rescheduling
 *       when the awakened task outranks the interrupted task.
 *   [ ] Assert invalid counts, invalid context, and object misuse in debug builds.
 *   [ ] Allocate neither objects nor waiter nodes dynamically.
 *
 * Non-goals:
 *   Ownership or priority inheritance; those semantics belong to mutexes.
 *
 * Validation:
 *   Test binary saturation, counting limits, ordered wakeups, timeouts, ISR-to-task
 *   handoff, and all overflow/error paths.
 */
