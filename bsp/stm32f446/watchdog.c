/*
 * AeroRT - STM32F446 independent watchdog support
 *
 * Purpose:
 *   Provide IWDG configuration and a narrowly scoped refresh primitive; health
 *   policy decides whether refresh is permitted.
 *
 * Responsibilities:
 *   Configure a documented watchdog window/timeout, expose refresh, and preserve
 *   reset-cause facts for later diagnostics.
 *
 * Expected dependencies:
 *   STM32 RCC/IWDG registers, low-speed clock assumptions, startup reset-cause
 *   capture, and the flight health monitor as the sole normal refresh authority.
 *
 * TODO:
 *   [ ] Derive watchdog timing bounds from measured/allowed LSI variation.
 *   [ ] Configure and start IWDG only after safe startup conditions are defined.
 *   [ ] Expose a minimal refresh call gated by health logic, not a periodic driver task.
 *   [ ] Ensure no ISR or unrelated task can feed the watchdog unconditionally.
 *   [ ] Capture watchdog reset cause early and route it to future fault records.
 *   [ ] Define debugger and fault-handler behavior without weakening production safety.
 *
 * Non-goals:
 *   Deciding system health or masking stalled critical components with blind feeds.
 *
 * Validation:
 *   Verify healthy refresh, intentionally withheld refresh/reset, timing tolerance,
 *   reset-cause reporting, and failure of each required heartbeat in turn.
 */
