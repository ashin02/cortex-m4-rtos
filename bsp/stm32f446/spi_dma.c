/*
 * AeroRT - STM32F446 SPI DMA transport
 *
 * Purpose:
 *   Provide deterministic asynchronous transfers suitable for IMU acquisition.
 *
 * Responsibilities:
 *   Configure SPI/DMA, start bounded transfers, acknowledge completion/error IRQs,
 *   and notify a known task with minimal ISR work.
 *
 * Expected dependencies:
 *   BSP clocks/GPIO, STM32 SPI/DMA/NVIC registers, arch IRQ constraints, and kernel
 *   direct-notification primitives through an application registration boundary.
 *
 * TODO:
 *   [ ] Select pins, SPI mode/rate, DMA streams/channels, and static transfer buffers.
 *   [ ] Define ownership and busy/error results; never allocate per transfer.
 *   [ ] Start IMU-oriented full-duplex/read transactions with bounded setup work.
 *   [ ] In DMA completion ISR, clear flags, capture status/timestamp, publish buffer
 *       ownership, notify the flight task, and request rescheduling if needed.
 *   [ ] Handle transfer, FIFO, overrun, and peripheral errors without ISR logging.
 *   [ ] Assign DMA/IRQ priorities consistent with kernel-aware interrupt rules and
 *       analyze interference with other DMA users.
 *
 * Non-goals:
 *   IMU protocol interpretation, estimator work, or heavy ISR processing.
 *
 * Validation:
 *   Test buffer ownership, busy/error paths, IRQ latency, sustained transfers,
 *   competing DMA traffic, and notification timing on hardware.
 */
