/*
 * AeroRT - Cortex-M4 context-switch mechanism
 *
 * Purpose:
 *   Eventually provide the assembly-only PendSV path that switches PSP-backed
 *   task contexts while exceptions and kernel startup use MSP.
 *
 * Responsibilities:
 *   Save/restore software-stacked registers, exchange task stack pointers through
 *   the port/scheduler contract, and perform a valid exception return.
 *
 * Expected dependencies:
 *   Stable TCB stack-pointer offset symbols and a small C scheduler handoff ABI.
 *   This file must contain Cortex-M mechanism only, never flight policy.
 *
 * TODO:
 *   [ ] Define PendSV at the lowest appropriate exception priority in port setup.
 *   [ ] Read PSP and save R4-R11 below the hardware exception frame.
 *   [ ] Rely on exception entry for R0-R3, R12, LR, PC, and xPSR preservation.
 *   [ ] Store the current PSP into the current task's TCB via a verified ABI.
 *   [ ] Call/select the next task without violating AAPCS stack/alignment rules.
 *   [ ] Load the next TCB's PSP, restore R4-R11, and return using correct EXC_RETURN.
 *   [ ] Preserve 8-byte stack alignment and document initial-frame layout.
 *   [ ] Define Cortex-M4F policy for FPCA, lazy stacking, S16-S31, and EXC_RETURN;
 *       measure costs before selecting eager or conditional FPU preservation.
 *   [ ] Add assembly-time checks or generated offsets to prevent C/ASM drift.
 *
 * Non-goals:
 *   Scheduling policy, STM32 peripherals, task creation policy, or control logic.
 *
 * Validation:
 *   Later run register-pattern, repeated-switch, PSP/alignment, exception-return,
 *   and optional floating-point preservation tests on STM32F446 hardware.
 */
