/*
 * AeroRT - STM32F446 UART DMA transport
 *
 * Purpose:
 *   Support asynchronous telemetry/debug traffic without blocking critical work.
 *
 * Responsibilities:
 *   UART and DMA setup, static TX/RX buffering, transfer initiation/completion, and
 *   bounded error reporting suitable for a low-priority consumer.
 *
 * Expected dependencies:
 *   BSP clocks/GPIO, STM32 UART/DMA/NVIC registers, and application-owned static
 *   buffers/queues with explicit overload policy.
 *
 * TODO:
 *   [ ] Define baud/framing, pins, DMA mappings, and exact clock-derived divisors.
 *   [ ] Implement asynchronous TX/RX state machines over statically supplied buffers.
 *   [ ] Keep completion/error ISRs to flag handling, metadata capture, and notification.
 *   [ ] Specify DMA/UART overrun, framing, dropped-data, and cancellation results.
 *   [ ] Ensure telemetry backpressure degrades or drops low-priority output instead
 *       of blocking the flight-control task.
 *   [ ] Analyze IRQ/DMA priority and bus contention under telemetry flood.
 *
 * Non-goals:
 *   Formatting telemetry in ISR context or guaranteeing delivery during overload.
 *
 * Validation:
 *   Exercise continuous TX/RX, wrap/buffer boundaries, injected errors, saturation,
 *   and interference while measuring flight-task timing.
 */
