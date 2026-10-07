/*
 * AeroRT - Cortex-M4 kernel port glue
 *
 * Purpose:
 *   Bridge portable kernel requests to Cortex-M exception, stack, and interrupt
 *   facilities without exposing those mechanisms to generic code.
 *
 * Responsibilities:
 *   Kernel startup, first-task launch, PSP/MSP setup, PendSV requests, exception
 *   priorities, and initial-frame construction helpers.
 *
 * Expected dependencies:
 *   CMSIS/core register definitions or minimal project-owned equivalents, context
 *   switch symbols, generic scheduler entry points, and startup vector integration.
 *
 * TODO:
 *   [ ] Define the port API consumed by task, scheduler, and instrumentation code.
 *   [ ] Construct initial task exception/software frames with correct xPSR Thumb
 *       bit, entry argument, return trap, EXC_RETURN, and 8-byte alignment.
 *   [ ] Start the first task through SVC if selected by the final startup design.
 *   [ ] Switch thread mode to PSP while retaining MSP for handlers/kernel startup.
 *   [ ] Provide a bounded helper that sets the PendSV pending bit with barriers.
 *   [ ] Configure PendSV at the lowest appropriate priority and establish SVC and
 *       kernel-aware IRQ priority relationships.
 *   [ ] Document implemented NVIC priority bits, shifted encodings, and BASEPRI
 *       assumptions for STM32F446 without configuring peripherals here.
 *   [ ] Enable and expose DWT cycle-count access through a narrow arch API.
 *
 * Non-goals:
 *   Portable scheduling decisions or STM32 peripheral initialization.
 *
 * Validation:
 *   Inspect initial frames, assert priority encodings, single-step first launch,
 *   and verify PSP/MSP and PendSV behavior under nested interrupts.
 */
