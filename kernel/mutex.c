/*
 * AeroRT - Priority-inheriting mutexes
 *
 * Purpose:
 *   Bound priority inversion for task-context mutual exclusion.
 *
 * Responsibilities:
 *   Track ownership and waiters, implement lock/unlock, propagate effective
 *   priorities, and restore priorities as blocking relationships disappear.
 *
 * Expected dependencies:
 *   TCB base/effective priority fields, scheduler requeue operations, timeout
 *   management, and critical sections. Mutex calls are forbidden from ISRs.
 *
 * TODO:
 *   [ ] Define static mutex state, owner, recursion policy, and waiter ordering.
 *   [ ] Implement try-lock/timed lock/unlock result contracts and ownership checks.
 *   [ ] Boost a lower-priority owner when a higher-priority task blocks on it.
 *   [ ] Reposition READY owners whenever their effective priority changes.
 *   [ ] Recompute effective priority after unlock or timeout from all mutexes still
 *       owned, rather than restoring the base priority prematurely.
 *   [ ] Specify bounded handling for nested/chained inheritance and detect cycles.
 *   [ ] Transfer ownership/wake a waiter atomically during unlock.
 *   [ ] Add misuse assertions for ISR use, non-owner unlock, and bad lifecycle.
 *
 * Non-goals:
 *   ISR synchronization, dynamic allocation, or priority-ceiling protocol yet.
 *
 * Validation:
 *   Exercise low/medium/high inversion, nested ownership, waiter timeout, scheduler
 *   requeueing, and correct effective-priority restoration.
 */
