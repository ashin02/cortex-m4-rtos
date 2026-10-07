/*
 * AeroRT - Cortex-M4 exception and fault hooks
 *
 * Purpose:
 *   Provide architecture-owned exception entry points and a minimal diagnostic
 *   bridge while leaving recovery policy to higher layers.
 *
 * Responsibilities:
 *   SVC dispatch/startup support, exception configuration, and bounded fault-state
 *   capture for HardFault, MemManage, BusFault, and UsageFault.
 *
 * Expected dependencies:
 *   Port startup services, Cortex-M fault/status registers, and a future crash
 *   record sink with static storage.
 *
 * TODO:
 *   [ ] Decode supported SVC operations and reject calls from invalid contexts.
 *   [ ] Enable configurable faults and define exception priorities coherently.
 *   [ ] Capture MSP/PSP selection, stacked registers, fault status/address registers,
 *       and reset reason with minimal, recursion-safe handler code.
 *   [ ] Add a future versioned crash-record format that survives reset when the
 *       linker/startup design supplies suitable reserved memory.
 *   [ ] Hand fault facts to system policy without running logging or recovery logic
 *       inside exception context.
 *   [ ] Define terminal behavior for corrupt-stack and nested-fault cases.
 *
 * Non-goals:
 *   Flight failsafe decisions, verbose UART output, or context-switch policy.
 *
 * Validation:
 *   Inject each configurable fault, verify captured frames/status, test handler-mode
 *   faults, and inspect behavior when PSP contents are invalid.
 */
