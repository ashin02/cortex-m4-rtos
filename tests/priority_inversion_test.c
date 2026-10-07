/*
 * AeroRT - Priority-inheritance test plan
 *
 * Purpose:
 *   Verify that mutex priority inheritance bounds the classic low/medium/high task
 *   inversion scenario and restores effective priority correctly afterward.
 *
 * Responsibilities:
 *   Orchestrate ownership/wait relationships, observe scheduling order, and cover
 *   timeout, nested, and chained inheritance cases.
 *
 * Expected dependencies:
 *   Three or more static tasks, mutex/scheduler trace hooks, deterministic barriers,
 *   and DWT/GPIO timing on hardware.
 *
 * TODO:
 *   [ ] Setup low task holding a mutex, high task blocking on it, and medium task READY.
 *   [ ] Document/demonstrate the unbounded-interference sequence conceptually without
 *       PI, then enable PI and verify low inherits high's effective priority.
 *   [ ] Verify the boosted owner runs ahead of medium and releases the mutex to high.
 *   [ ] Confirm restoration to base or the highest remaining inherited priority.
 *   [ ] Cover waiter timeout, several waiters, nested mutexes, and chained inheritance.
 *   [ ] Record blocking duration and scheduling trace; do not fabricate bounds.
 *
 * Non-goals:
 *   Using mutexes in ISRs or recommending them for the hard flight path.
 *
 * Validation:
 *   Fail on medium-task interference during inheritance, bad waiter order, stale boost,
 *   incorrect ready-queue placement, ownership error, or unbounded chain handling.
 */
