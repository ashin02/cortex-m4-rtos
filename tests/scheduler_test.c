/*
 * AeroRT - Scheduler test plan
 *
 * Purpose:
 *   Verify fixed-priority preemption, per-priority FIFO queues, and bitmap selection.
 *
 * Responsibilities:
 *   Construct deterministic task-state scenarios and check the selected task plus all
 *   ready-queue/bitmap invariants after every operation.
 *
 * Expected dependencies:
 *   Host-side scheduler seams where possible and on-target preemption tests for the
 *   architecture handoff; statically allocated synthetic TCBs.
 *
 * TODO:
 *   [ ] Setup tasks at every boundary priority and multiple tasks at one priority.
 *   [ ] Verify strict highest-priority ordering and immediate preemption on unblock.
 *   [ ] Verify FIFO order among equal priorities with no critical-path time slicing.
 *   [ ] Exercise enqueue, dequeue, block, unblock, suspend, and effective-priority move.
 *   [ ] Check each ready_bitmap transition and constant-time selection result.
 *   [ ] Verify idle selection only when no non-idle task is READY.
 *   [ ] Measure decision/switch-request behavior under deliberate ready-queue churn.
 *
 * Non-goals:
 *   Benchmark claims before hardware measurement or testing peripheral drivers.
 *
 * Validation:
 *   Fail on wrong selection/order, stale bitmap bits, duplicate/lost TCB linkage,
 *   delayed preemption, illegal state transition, or missing idle fallback.
 */
