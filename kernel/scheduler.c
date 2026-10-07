/*
 * AeroRT - Fixed-priority scheduler
 *
 * Purpose:
 *   Define portable, deterministic scheduling policy for runnable tasks.
 *
 * Responsibilities:
 *   - Maintain one FIFO ready queue per priority and a ready bitmap.
 *   - Select and dispatch the highest-priority READY task in O(1) time.
 *   - Decide preemption and preserve same-priority FIFO ordering.
 *
 * Expected dependencies:
 *   Task state/TCB interfaces, bounded critical sections, and the Cortex-M port's
 *   PendSV request hook. No timer registers or other STM32 details belong here.
 *
 * TODO:
 *   [ ] Define a fixed priority range where lower numeric values mean higher
 *       logical priority, and document the bitmap-bit mapping unambiguously.
 *   [ ] Maintain intrusive per-priority FIFO queues with bounded enqueue/dequeue.
 *   [ ] Set and clear ready_bitmap bits exactly when queues cross empty/non-empty.
 *   [ ] Use CLZ/CTZ or an equivalent constant-time operation, never a task scan,
 *       to find the highest-priority non-empty ready queue.
 *   [ ] Request PendSV when a newly READY task outranks the running task.
 *   [ ] Coordinate RUNNING/READY transitions with the context-switch mechanism.
 *   [ ] Requeue a task correctly when priority inheritance changes its effective
 *       priority, including while READY or blocked on a synchronization object.
 *   [ ] Define a permanently available lowest-priority idle task and its hooks.
 *   [ ] Avoid periodic round-robin time slicing in the hard real-time path.
 *
 * Non-goals:
 *   Context save/restore, hardware timer programming, or application priorities.
 *
 * Validation:
 *   Test strict priority, immediate preemption, FIFO ties, bitmap invariants,
 *   priority changes, and idle selection across every boundary priority.
 */
