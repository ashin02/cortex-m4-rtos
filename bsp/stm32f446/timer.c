/*
 * AeroRT - STM32F446 high-resolution timers
 *
 * Purpose:
 *   Provide the hardware side of the monotonic-clock and tickless next-event API.
 *
 * Responsibilities:
 *   TIM2 free-running microsecond-scale time, TIM5 one-shot/compare wake events,
 *   minimal IRQ acknowledgement, and handoff to the kernel timer subsystem.
 *
 * Expected dependencies:
 *   BSP clock frequencies, STM32 timer/NVIC registers, arch interrupt rules, and a
 *   narrow portable kernel timer callback.
 *
 * TODO:
 *   [ ] Configure TIM2 as a free-running monotonic counter, likely near 1 MHz, only
 *       after deriving an exact prescaler from the verified timer clock.
 *   [ ] Extend or formally constrain the timestamp range and handle counter wrap.
 *   [ ] Configure TIM5 compare/one-shot operation for the next kernel event.
 *   [ ] Program past/near-future events without losing wakeups to compare races.
 *   [ ] In the IRQ, clear only relevant flags, capture time, and invoke a bounded
 *       kernel timer handoff that may request PendSV.
 *   [ ] Define timer IRQ priority within the kernel-aware interrupt band.
 *
 * Non-goals:
 *   Delayed-task heap management, periodic policy, or blocking inside an ISR.
 *
 * Validation:
 *   Measure monotonicity/resolution, compare latency, wrap behavior, race windows,
 *   and long idle intervals with DWT/GPIO and a logic analyzer.
 */
