# AeroRTOS Codex Guide

## Baseline

AeroRTOS is intended to be a small fixed-priority preemptive RTOS for an ST
NUCLEO-F446RE / STM32F446RET6 / Cortex-M4F (`README.md`, `bsp/stm32f446/`,
`arch/cortex_m4/`). The target is encoded in the M1 build and linker configuration
but is not yet confirmed by a running hardware image.

Audit baseline (2026-11-05, commit `3dd24e5`) found only comment/TODO scaffolds.
M1 provides a buildable startup/heartbeat image, and M2 provides host-tested static
TCBs and initial integer task frames. Neither has been inspected on hardware. The
scheduler, context switch, synchronization, flight, and remaining test files are
still comment-only. Never treat a comment, compile, or roadmap entry as runtime
verification.

## Navigation map

| Path | Responsibility / symbols |
|---|---|
| `Makefile` | Strict soft-float Cortex-M4 build; `all`, `verify`, `flash`, `openocd`, `gdb`, `clean` |
| `linker/stm32f446re.ld` | 512 KiB Flash/128 KiB SRAM layout; vector/MSP/RAM assertions |
| `startup/startup_stm32f446re.S` | `_vector_table`, `Reset_Handler`, `.data` copy, `.bss` clear, weak IRQ defaults |
| `include/aerortos/{cortex_m4,stm32f446re,platform}.h` | Minimal project-owned core/device registers and M1 BSP API |
| `app/main.c` | `main`; initializes clock, one non-running M2 smoke TCB/frame, LD2, heartbeat SysTick |
| `arch/cortex_m4/context_switch.S` | PendSV context save/restore; no handler exists yet |
| `arch/cortex_m4/port.c` | `aerortos_port_build_initial_frame`, `aerortos_task_return_trap`; no launch/PendSV code |
| `arch/cortex_m4/critical_section.c` | Nested BASEPRI protection; no functions yet |
| `arch/cortex_m4/exception.c` | `platform_panic`, default/NMI/HardFault/MemManage/BusFault/UsageFault terminal handlers |
| `include/aerortos/task.h` | `aerortos_task_t`, config/result/state types, capacity/stack contracts, task API |
| `include/aerortos/port.h`, `tcb_offsets.h` | Initial frame types/constants and target C/assembly TCB ABI offsets |
| `kernel/task.c` | Eight-slot static TCB pool, create/transition/release, stack guards/high-water scan |
| `kernel/scheduler.c` | Fixed-priority FIFO ready queues, bitmap selection, idle/preemption policy |
| `kernel/timer.c`, `kernel/deadline.c` | Periodic release, timeouts, deadline/miss accounting |
| `kernel/queue.c`, `kernel/mutex.c` | Bounded queues and priority-inheritance mutexes |
| `kernel/semaphore.c` | Optional semaphore support; not required by current resume scope |
| `kernel/monitor.c` | Runtime, stack, latency, jitter, and deadline instrumentation |
| `bsp/stm32f446/clock.c` | `system_clock_init` (16 MHz HSI), M1-only heartbeat SysTick and `bsp_millis` |
| `bsp/stm32f446/gpio.c` | `gpio_led_init`, `gpio_led_toggle` for NUCLEO LD2 on PA5 |
| `bsp/stm32f446/timer.c` | Future RTOS tick/time base; still comments only |
| `bsp/stm32f446/{spi_dma,uart_dma,pwm,watchdog}.c` | Optional BSP work; implement only when a required test/workload needs it |
| `flight/*.c` | Proposed application modules; final scope is a minimal flight-like workload, not a full controller |
| `tests/task_test.c`, `tests/host/platform_stub.c` | Executable M2 lifecycle, capacity, stack, frame, and guard tests |
| Other `tests/*.c` | Test specifications only; add executable tests with their owning milestone |
| `roadmap.md` | Status, decisions, milestones, risks, and verification evidence |

Update this table with actual public types, entry points, handlers, and commands as
they are implemented. Remove stale navigation facts promptly.

## Architecture boundaries and invariants

Current M1 flow: `initial MSP/vector -> Reset_Handler -> copy .data -> clear .bss ->
main -> 16 MHz HSI -> construct one non-running M2 smoke task -> PA5 output -> 1 kHz
heartbeat-only SysTick -> LD2 toggle at 1 Hz -> WFI`. The smoke TCB/frame exists for
debugger inspection only; no SVC launch, PSP use, scheduling, or PendSV switching
exists yet.

M2 frame layout from low to high address: `R4-R11` software frame, then hardware
`R0-R3, R12, LR, PC, xPSR`; `saved_psp` points at R4. Stacked PC has bit 0 clear,
xPSR has Thumb bit 24 set, and LR is the odd-address task-return trap. The fixed
integer-only exception-return value is deferred to M5 launch/switch code.

- Portable `kernel/` code must not access STM32 registers. `arch/` owns Cortex-M
  mechanisms; `bsp/` owns board peripherals; `flight/` uses public RTOS/BSP APIs.
- Implement and test BASEPRI protection before enabling interrupt-driven
  preemption. Only kernel-aware IRQ priorities may call ISR-safe kernel APIs.
- Preserve AAPCS, 8-byte stack alignment, Thumb xPSR, EXC_RETURN, PSP/MSP roles,
  and the selected FPU-context policy. Bind C/assembly TCB offsets at build time.
- A task/list node has one owner. State, list, and ready-bitmap updates are atomic.
  Signal/timeout races remove and wake a waiter exactly once.
- M2 lifecycle calls are single-threaded and not scheduler-integrated. M3 must make
  the scheduler own READY/RUNNING/list transitions before any concurrent use.
- Use static/caller-owned storage with configured bounds and explicit exhaustion.
  No runtime heap allocation in the kernel.
- O(1) claims cover only fixed-width highest-priority selection and intrusive
  ready-queue enqueue/dequeue. They do not cover message copies, timeout ordering,
  priority-inheritance propagation, or FP-dependent context costs.
- Start with a periodic tick. Tickless/one-shot scheduling is optional only after
  the tick-based kernel is correct and measured.
- Define the FPU save/restore policy before allowing floating-point task code.

## Build, test, and change rules

Commands from the repository root:

- Clean build: `make clean && make all`
- Host task tests: `make test`
- Optional ASan/UBSan host run: `make test-sanitize` (non-PIE; leak detection is
  disabled because it is unsupported in the current container)
- Artifact/binutils inspection: `make verify`
- Flash with ST-Link/OpenOCD: `make flash`
- Debug: run `make openocd`, then `make gdb` in another terminal. Override a missing
  default client with `make gdb GDB=gdb-multiarch`.
- Outputs: `build/aerortos.{elf,bin,map}`; generated objects/dependencies/stack-use
  reports also live under ignored `build/`.

`make flash/openocd/gdb` require locally installed OpenOCD and an ARM-capable GDB;
they were unavailable during M1 build verification. See `roadmap.md` for evidence.

- Before editing: read the relevant roadmap milestone, inspect affected contracts,
  and check `git status` so unrelated user work is preserved.
- Add tests with each implementation milestone. Pure policy/data-structure logic
  should have host tests; exception, interrupt, stack, and timing behavior requires
  STM32F446 hardware tests.
- Minimum verification after a change: warnings-as-errors clean build, relevant
  executable tests, affected on-target checks, and results recorded in `roadmap.md`.
  Compilation alone is never runtime verification.
- Keep ISR, PendSV, and masked sections bounded. Do not allocate, format, log, or
  run application algorithms there.
- Follow existing separation and naming once APIs exist; do not invent parallel
  abstractions without updating this map and the owning roadmap decision.
- Do not add unrelated protocols or a complete flight stack. CAN, Ethernet, and
  UDS are outside the current project scope.

## Documentation ownership

- `AGENTS.md`: concise navigation, boundaries, commands, and stable invariants.
- `roadmap.md`: status, milestones, decisions, tradeoffs, and evidence.
- `README.md`: final public portfolio presentation. **Do not modify, reformat, or
  generate it until implementation/validation are complete and the user explicitly
  authorizes finalization.**
