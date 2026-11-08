# AeroRTOS Codex Guide

## Baseline

AeroRTOS is intended to be a small fixed-priority preemptive RTOS for an ST
NUCLEO-F446RE / STM32F446RET6 / Cortex-M4F (`README.md`, `bsp/stm32f446/`,
`arch/cortex_m4/`). The target is encoded in the build/linker configuration and is
confirmed by the passing isolated M5 image on a physical NUCLEO-F446RE.

Audit baseline (2026-11-05, commit `3dd24e5`) found only comment/TODO scaffolds.
M1 provides startup support, M2 provides static TCBs and initial integer frames,
M3/M4 provide the bitmap scheduler and BASEPRI critical sections, and M5 provides
SVC launch plus PendSV switching. The isolated M5 image passed twice under debugger
reset with 157 checks, zero failures, and 130 switches on STM32F446RE. M6 implements
a 1 kHz SysTick, periodic release/deadline policy, bounded timing statistics, and an
isolated acceptance image that passed twice on STM32F446RE under debugger reset (49
checks, zero failures, one deliberate deadline miss, and verified 4 ms/5 ms releases
across 32-bit tick wrap). Overrun and skipped-release hardware coverage remains
absent; retain its host-test evidence separately. M7 caller-owned bounded message
queues passed twice on STM32F446RE after the test-only completion handshake fix (40
checks, zero failures, one preemption, one blocked send, two blocked receives, and
the expected nine-event trace). M8 bounded non-recursive priority-inheritance mutexes
passed their isolated physical STM32F446RE acceptance image and debugger-reset rerun
(43 checks, zero failures, one boost/restoration/block/preemption, and the expected
nine-event trace). Multiple mutex ownership, transitive PI, and M7 wait-list
reordering remain host/sanitizer-only evidence. M9 deterministic integrated host
regression and mixed periodic/queue/PI target image passed physically with 69 checks,
zero failures, expected counters/flags/trace, and an identical debugger-reset rerun.
M10 provides opt-in DWT timing, bounded RAM statistics, host mocks, and an isolated
benchmark. Its original firmware passed physical STM32F446RE acceptance twice with
80 checks, zero failures, a validated 16 MHz clock, and 9-cycle DWT-read overhead.
The later aggregate-statistics revision retains only 64 raw samples but summarizes
all observations with saturating counters. The corrected revision passed 80/0 on
STM32F446RE and reproduced identically after debugger reset: 104 critical-section
observations, 64 retained, 40 dropped, 23,161 total cycles, 80/916-cycle minimum/
maximum, and zero saturation flags. Never generalize isolated hardware evidence
beyond its recorded scope.

## Navigation map

| Path | Responsibility / symbols |
|---|---|
| `Makefile` | Strict soft-float Cortex-M4 build; host/sanitizer suites; default plus isolated M5-M10 target images |
| `linker/stm32f446re.ld` | 512 KiB Flash/128 KiB SRAM layout; vector/MSP/RAM assertions |
| `startup/startup_stm32f446re.S` | `_vector_table`, explicit Flash VTOR, C data init, weak unused-IRQ defaults; SysTick must resolve strongly |
| `include/aerortos/{cortex_m4,stm32f446re,platform}.h` | Minimal project-owned core/device registers and M1 BSP API |
| `app/main.c` | Default M6 image: 1 kHz SysTick, idle, heartbeat and peer periodic tasks, SVC scheduler start |
| `arch/cortex_m4/context_switch.S` | `SVC_Handler`, `PendSV_Handler`; PSP R4-R11 restore/save and fixed basic-frame exception return |
| `arch/cortex_m4/port.c` | Initial frame, exception priorities, PendSV request, SVC launch, handler-mode query |
| `arch/cortex_m4/critical_section.c` | `aerortos_critical_enter/exit`; nested prior-BASEPRI restoration |
| `arch/cortex_m4/cycle_counter.c` | Checked DWT CYCCNT initialization, reads, wrap-safe elapsed cycles, and conversion |
| `arch/cortex_m4/exception.c` | `platform_panic`, default/NMI/HardFault/MemManage/BusFault/UsageFault terminal handlers |
| `include/aerortos/task.h` | Static TCB/config API, base/effective priority, wait kind, owned mutexes, periodic counters, and bounded timing-event ring |
| `include/aerortos/scheduler.h` | Scheduler add/start/yield/block/wake/switch and effective-priority reposition APIs |
| `include/aerortos/critical_section.h` | Kernel BASEPRI threshold and saved-state critical-section API |
| `include/aerortos/timer.h`, `deadline.h` | Wrap-safe tick/periodic-wait APIs and internal deadline accounting hooks |
| `include/aerortos/queue.h` | Caller-owned fixed-capacity FIFO, try/blocking APIs, results, and diagnostics |
| `include/aerortos/mutex.h` | Caller-owned non-recursive PI mutex, try/blocking lock, owner unlock, results, and diagnostics |
| `include/aerortos/port.h`, `tcb_offsets.h` | Initial frame types/constants and target C/assembly TCB ABI offsets |
| `kernel/task.c` | Eight-slot static TCB pool, create/transition/release, stack guards/high-water scan |
| `kernel/scheduler.c` | Fixed-priority FIFO queues, bitmap selection, priority repositioning, idle/preemption policy, PendSV C handoff |
| `kernel/timer.c`, `kernel/deadline.c` | 32-bit time, bounded periodic scan/release, deadline/overrun/execution/jitter accounting |
| `kernel/queue.c` | BASEPRI-protected ring buffer and effective-priority/FIFO sender/receiver wait lists, including PI reordering |
| `kernel/mutex.c` | BASEPRI-protected ownership, priority/FIFO waiters, bounded transitive PI, restoration, handoff, and cycle rejection |
| `kernel/semaphore.c` | Optional semaphore support; not required by current resume scope |
| `include/aerortos/monitor.h`, `kernel/monitor.c` | Opt-in bounded raw/aggregate timing metrics and hot-path hooks |
| `bsp/stm32f446/clock.c` | `system_clock_init` (16 MHz HSI) and explicit Flash VTOR setup |
| `bsp/stm32f446/gpio.c` | `gpio_led_init`, `gpio_led_toggle` for NUCLEO LD2 on PA5 |
| `bsp/stm32f446/timer.c` | Exact 1 kHz SysTick configuration/handler and legacy `bsp_millis` wrapper |
| `bsp/stm32f446/{spi_dma,uart_dma,pwm,watchdog}.c` | Optional BSP work; implement only when a required test/workload needs it |
| `flight/*.c` | Proposed application modules; final scope is a minimal flight-like workload, not a full controller |
| `tests/{task,scheduler,context_switch,deadline}_test.c` | Host lifecycle, switching, periodic phase/wrap/miss/overrun/jitter regression |
| `tests/target/m5_*` | Isolated target test, R4-R11 probe, PASS/FAIL counters, trace, ISR wake, idle fallback |
| `tests/target/m6_periodic_test.c` | Isolated wrap/release/preemption/miss/recovery test with GDB counters and traces |
| `tests/queue_test.c`, `tests/target/m7_queue_test.c` | Host queue edge/contention regression and isolated target blocking/preemption image |
| `tests/mutex_test.c`, `tests/target/m8_mutex_test.c` | Host PI/restoration/queue-integration regression and isolated inversion/PI target image |
| `tests/integration_test.c` | M9 deterministic periodic, queue, PI, wrap, overrun, and invariant interaction regression |
| `tests/target/m9_integration_test.c` | Isolated mixed periodic-wrap, queue wake, mutex PI/handoff/restoration image with completion flags, counters, and fixed trace |
| `tests/timing_test.c`, `tests/target/m10_timing_test.c` | DWT/statistics host regression and isolated six-participant timing benchmark |
| Other `tests/*.c` | Later-milestone test specifications only |
| `roadmap.md` | Status, decisions, milestones, risks, and verification evidence |

Update this table with actual public types, entry points, handlers, and commands as
they are implemented. Remove stale navigation facts promptly.

## Architecture boundaries and invariants

Current default flow: `initial MSP/vector -> Reset_Handler -> C init -> main -> 16
MHz HSI -> create idle and periodic tasks -> initialize 1 kHz SysTick -> scheduler
select -> SVC -> Thread/PSP -> task periodic wait -> PendSV -> idle/next task`.
SysTick advances the monotonic tick, accounts the running job, scans at most eight
registered periodic tasks, releases due jobs, and requests PendSV when a released
task outranks the running task. Handlers use MSP; tasks use caller-owned PSP stacks.

Frame layout from low to high address: `R4-R11` software frame, then hardware
`R0-R3, R12, LR, PC, xPSR`; `saved_psp` points at R4. Stacked PC has bit 0 clear,
xPSR has Thumb bit 24 set, and LR is the odd-address task-return trap. SVC/PendSV use
the fixed integer-only Thread/PSP basic-frame return value `0xFFFFFFFD`.

- Portable `kernel/` code must not access STM32 registers. `arch/` owns Cortex-M
  mechanisms; `bsp/` owns board peripherals; `flight/` uses public RTOS/BSP APIs.
- STM32F446 implements four priority bits. BASEPRI `0x40` masks kernel-aware
  interrupts during scheduler mutation; SVC is `0x20`, PendSV is implemented-lowest
  `0xF0`, and only IRQ priorities encoded `0x40` or greater (less urgent) may call
  ISR-safe APIs. `make verify` checks the actual BASEPRI assembly operand separation.
- Preserve AAPCS, 8-byte stack alignment, Thumb xPSR, EXC_RETURN, PSP/MSP roles,
  and the selected FPU-context policy. Bind C/assembly TCB offsets at build time.
- A task/list node has one owner. State, list, and ready-bitmap updates are atomic.
  Signal/timeout races remove and wake a waiter exactly once.
- Direct M2 lifecycle transitions remain pre-scheduling/test-only. Once registered,
  scheduler APIs own READY/RUNNING/BLOCKED transitions and ready-list membership.
- Yield/block are Thread-only. Wake/add are bounded under BASEPRI; an ISR wake may
  pend PendSV, which runs only after higher-priority handler work and masking end.
- Period/deadline/initial-delay values are at most `INT32_MAX` ticks. Wrap-safe
  signed-difference ordering is valid within that half-range; `next_release +=
  period` preserves phase across 32-bit wrap without cumulative drift.
- Each periodic task has at most one active job and no backlog. A task completes its
  job by calling `aerortos_periodic_wait_next()`. If the next phase arrives first,
  that release is skipped and overrun/skipped counters advance; the phase still
  advances. A miss is recorded once when an active job is observed strictly after
  its absolute deadline, so completion at the deadline tick is on time.
- Ready selection remains O(1). Each SysTick performs one fixed-capacity scan of at
  most eight periodic TCB pointers; timing history is an overwrite-on-full ring of
  eight events per task. Execution and jitter values have 1 ms resolution and are
  functional statistics, not M10 calibrated latency measurements.
- Queue control blocks and payload buffers are caller-owned for their full lifetime.
  Ring updates are O(1), copies are O(element size) under BASEPRI, and priority/FIFO
  waiter insertion scans at most eight tasks. Blocking queue calls are Thread-only;
  try operations may run in Thread mode or permitted kernel-aware ISRs. Queue waits
  are indefinite in M7: there are no timed waits, cancellation, or deletion APIs.
- A queue-blocked task uses its intrusive links exclusively in one queue wait list.
  The queue removes and clears the waiter before scheduler wakeup; direct lifecycle
  transitions and queue reinitialization are forbidden while waiters exist.
- Mutex objects are caller-owned, explicitly initialized, non-recursive, and
  task-context only. Unlock is owner-only. A task may own at most eight mutexes;
  waits are indefinite and there is no cancellation, deletion, or timed lock.
- TCB base priority is immutable after creation; effective priority is the minimum
  numeric priority donated by the head waiters of all owned mutexes, or base when
  undonated. Priority/FIFO mutex waiters preserve FIFO within equal effective
  priorities. Priority changes atomically reposition READY, mutex-blocked, and
  queue-blocked tasks under BASEPRI.
- PI propagation, restoration, ownership scans, wait-list insertion/reordering, and
  deadlock-chain inspection are bounded by the eight-task/eight-owned-mutex limits.
  Cyclic acquisition is rejected. PI mitigates inversion but does not establish an
  application blocking-time bound without acquisition/critical-section analysis.
- Use static/caller-owned storage with configured bounds and explicit exhaustion.
  No runtime heap allocation in the kernel.
- O(1) claims cover only fixed-width highest-priority selection and intrusive
  ready-queue enqueue/dequeue. They do not cover message copies, timeout ordering,
  priority-inheritance propagation, or FP-dependent context costs.
- Start with a periodic tick. Tickless/one-shot scheduling is optional only after
  the tick-based kernel is correct and measured.
- Define the FPU save/restore policy before allowing floating-point task code.
- M10 timing is enabled only with `AERORTOS_ENABLE_TIMING`. Raw DWT subtraction is
  valid for intervals shorter than one 32-bit wrap; at 16 MHz that is about 268 s.
  PendSV samples exclude hardware exception entry/return and observed maxima are not
  WCET bounds. Each series retains 64 raw samples while aggregate count/total/min/max
  continue beyond capacity; retained and dropped counts are distinct and integer
  exhaustion saturates with flags. Normal M1-M9 images retain the uninstrumented
  context-switch path.

## Build, test, and change rules

Commands from the repository root:

- Clean build: `make clean && make all`
- Host task tests: `make test`
- Optional ASan/UBSan host run: `make test-sanitize` (non-PIE; leak detection is
  disabled because it is unsupported in the current container)
- Artifact/binutils inspection: `make verify`
- Isolated M5 target build/disassembly: `make m5-hardware-test`
- Flash isolated M5 image: `make flash-m5-test`
- Debug isolated M5 image: run `make openocd`, then `make gdb-m5-test`
- Isolated M6 target build/SysTick disassembly: `make m6-hardware-test`
- Flash isolated M6 image: `make flash-m6-test`
- Debug isolated M6 image: run `make openocd`, then `make gdb-m6-test` (use
  `GDB=gdb-multiarch` when needed)
- Isolated M7 target build/disassembly: `make m7-hardware-test`
- Flash isolated M7 image: `make flash-m7-test`
- Debug isolated M7 image: run `make openocd`, then `make gdb-m7-test` (use
  `GDB=gdb-multiarch` when needed)
- Isolated M8 target build/disassembly: `make m8-hardware-test`
- Flash isolated M8 image: `make flash-m8-test`
- Debug isolated M8 image: run `make openocd`, then `make gdb-m8-test` (use
  `GDB=gdb-multiarch` when needed)
- Isolated M9 target build/static checks: `make m9-hardware-test`
- Flash isolated M9 image: `make flash-m9-test`
- Debug isolated M9 image: run `make openocd`, then `make gdb-m9-test` (use
  `GDB=gdb-multiarch` when needed)
- Isolated M10 timing build/static checks: `make m10-hardware-test`
- Flash isolated M10 image: `make flash-m10-test`
- Debug isolated M10 image: run `make openocd`, then `make gdb-m10-test` (use
  `GDB=gdb-multiarch` when needed)
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
