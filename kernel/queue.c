/*
 * AeroRT - Fixed-capacity queues
 *
 * Purpose:
 *   Provide deterministic message transport using caller-owned storage.
 *
 * Responsibilities:
 *   Generic ring-buffer mechanics, send/receive operations, bounded blocking,
 *   timeouts, and explicit full/empty results. Application freshness policy is
 *   selected by callers rather than hidden in the queue implementation.
 *
 * Expected dependencies:
 *   Scheduler blocking/wakeup services, kernel timeouts, critical sections, and
 *   an ISR-safe wake/reschedule interface where explicitly supported.
 *
 * TODO:
 *   [ ] Initialize element size/capacity and caller-provided aligned storage.
 *   [ ] Implement bounded index/count updates and copying for send and receive.
 *   [ ] Specify nonblocking, timed, and indefinite-wait APIs and result codes.
 *   [ ] Track waiting senders and receivers in deterministic priority/FIFO order.
 *   [ ] Make timeout-vs-operation races atomic and remove waiters exactly once.
 *   [ ] Add narrowly scoped ISR-safe operations with deferred PendSV requests.
 *   [ ] Return queue-full explicitly; do not silently overwrite by default.
 *   [ ] Let flight code choose latest-value/drop/error behavior by data class.
 *   [ ] Require all queue control blocks and payload buffers to be static.
 *
 * Non-goals:
 *   Dynamic allocation or deciding sensor, command, and telemetry loss policy.
 *
 * Validation:
 *   Test wraparound, capacity edges, waiter ordering, timeout races, ISR wakeups,
 *   and explicit full behavior with fixed test storage.
 */
