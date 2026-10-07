/*
 * AeroRT - Flight IMU acquisition
 *
 * Purpose:
 *   Coordinate timestamped IMU samples from data-ready event through SPI DMA to
 *   the high-priority flight path without doing heavy work in interrupts.
 *
 * Responsibilities:
 *   Own IMU protocol/state, initiate acquisition, validate completed samples, and
 *   publish the freshest sample with explicit ownership and age metadata.
 *
 * Expected dependencies:
 *   BSP GPIO/EXTI and SPI-DMA interfaces, monotonic timestamps, and a direct kernel
 *   notification to the flight task. No kernel internals are accessed directly.
 *
 * TODO:
 *   [ ] Define static buffers and the IMU sample/status/timestamp interface.
 *   [ ] On data-ready, timestamp minimally and initiate a bounded SPI DMA transfer.
 *   [ ] On completion, transfer buffer ownership and notify the flight task.
 *   [ ] Keep protocol parsing, calibration, validation, and conversion in task context.
 *   [ ] Define latest-value/double-buffer behavior when samples arrive faster than
 *       consumers; count discarded or stale samples explicitly.
 *   [ ] Detect missing data-ready events, DMA errors, corrupt samples, and excessive age.
 *
 * Non-goals:
 *   Estimation/control algorithms or heavy processing in EXTI/DMA ISRs.
 *
 * Validation:
 *   Test timestamps, buffer ownership, sample freshness, burst/overrun behavior,
 *   errors, and data-ready-to-task latency under interference.
 */
