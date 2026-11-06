# AeroRTOS Engineering Roadmap

## 1. Evidence baseline

Audit date: 2026-11-05. Audited commit: `3dd24e5` (`Initial commit`).

The initial repository contained only comment/TODO scaffolds. M1 now supplies a
freestanding ARM GCC build, STM32F446RE linker layout, vector/reset startup,
minimal device/core definitions, terminal fault handlers, 16 MHz HSI setup, and an
LD2 heartbeat image. The image builds and passes static binutils inspection, but no
hardware, probe, OpenOCD, or ARM GDB client was available for target execution.
M2 now adds a bounded static TCB pool, caller-owned guarded stacks, explicit task
states, an initial integer Cortex-M frame builder, target ABI assertions, and an
executable host test. Neither M1 nor M2 has on-target execution evidence. Scheduler,
preemption, synchronization, and other RTOS source remains comment-only.

Status terms:

- **COMPLETE:** implemented and verified against recorded acceptance criteria.
- **IMPLEMENTED / UNVERIFIED:** code exists; verification is insufficient.
- **PARTIAL:** useful implementation exists, but required behavior is absent.
- **MISSING:** no implementation exists.
- **BLOCKED:** cannot proceed until a stated dependency or authorization is resolved.

Evidence strength: source inspection < linked cross-build < executable host test <
STM32F446 target test < repeated calibrated target measurement. Compilation is not
runtime correctness; host simulation cannot prove Cortex-M exception behavior; an
observed maximum is not a mathematical WCET bound.

| Area | Status | Evidence and missing work |
|---|---|---|
| Build/startup/target configuration | IMPLEMENTED / UNVERIFIED | ELF/BIN/map and binutils checks pass; target reset/heartbeat/debug execution remains unverified |
| TCB, stacks, task lifecycle | IMPLEMENTED / UNVERIFIED | Static pool, lifecycle, guards, initial frames, host tests, and target ABI asserts pass; debugger inspection/exception return remain pending |
| Bitmap scheduler and idle | MISSING | `kernel/scheduler.c` has no queues, bitmap, or operations |
| Critical sections/IRQ policy | MISSING | `arch/cortex_m4/critical_section.c` has BASEPRI TODOs only |
| SVC/PendSV and PSP/MSP | MISSING | `port.c`, `exception.c`, and `context_switch.S` contain no executable code |
| Periodic time/timeouts/deadlines | MISSING | Kernel/BSP timer files are comments only |
| Bounded queues and PI mutexes | MISSING | `queue.c` and `mutex.c` are comments only |
| Tests and measurements | PARTIAL | `make test` executes M2 host tests and `make verify` checks M1 artifacts; target task-frame inspection and all later RTOS tests remain missing |
| Flight-like workload | MISSING | `flight/*.c` are comments only |
| Public finalization | BLOCKED | Requires completed evidence and explicit README authorization |

M0 is complete. M1 and M2 are implemented but remain hardware-unverified; neither
may be marked COMPLETE from host/build evidence alone. No scheduler or preemption
feature is implemented.

## 2. Scope and final requirements

The project emphasizes skills relevant to embedded platform software: C and ARM
assembly, Cortex-M bring-up and exceptions, interrupt concurrency, deterministic
resource management, hardware debugging/instrumentation, automated regression,
fault analysis, and reproducible evidence.

The five final resume requirements are:

1. Built a deterministic RTOS for STM32 with preemptive scheduling and
   synchronization for flight-control tasks.
2. Implemented context switching with PendSV and PSP stacks to preserve task
   contexts across preemption.
3. Designed a fixed-priority scheduler with ready queues and bitmap selection for
   predictable, O(1) task dispatch.
4. Added message queues and priority-inheritance mutexes to bound memory use and
   mitigate priority inversion.
5. Validated real-time behavior by measuring context-switch latency, jitter, and
   deadline misses under stress.

Out of scope unless a required test needs it: CAN, Ethernet, UDS, a production
bootloader, a complete flight-control stack, production safety certification, and
broad FreeRTOS feature parity.

## 3. Engineering decisions

These are proposed/reconstructed decisions, not implementation evidence. Historical
alternatives were not recorded in the repository.

### ED1 — Scheduler policy and O(1) scope

- Use fixed priorities, one intrusive FIFO ready queue per priority, a fixed-width
  ready bitmap, FIFO equal-priority ordering, and a lowest-priority idle task.
- O(1) means fixed-width highest-priority selection and ready-queue head/tail
  enqueue/dequeue. It excludes timeout management, message copying, priority-
  inheritance propagation, ISR work, and context save/restore.
- Tradeoffs: starvation is possible, priorities need analysis, and initial support
  does not require equal-priority time slicing.
- Planned location: `kernel/scheduler.c`. Evidence: none.

### ED2 — Static allocation and explicit bounds

- Use caller-provided stacks/queue storage and fixed capacities for tasks,
  priorities, timers, waiters, mutexes, and traces. Do not use a runtime kernel heap.
- Capacity exhaustion must return explicit errors. Verify RAM use with the link map,
  boundary tests, and stack high-water evidence.
- Tradeoff: fixed limits may reserve unused memory but make resource behavior auditable.

### ED3 — Critical sections before preemption

- Implement and validate nested BASEPRI protection before enabling interrupt-driven
  preemption. Reserve PRIMASK for narrowly justified startup/fault paths.
- Derive shifted BASEPRI values from implemented NVIC priority bits. High-urgency,
  non-kernel-aware ISRs must never call kernel APIs.
- Tradeoff: priority encodings and barriers are subtle, but scheduler/list atomicity
  cannot depend on an unverified mask policy.

### ED4 — Cortex-M switching model

- Use SVC/exception return for first launch, per-task PSP in thread mode, MSP in
  handlers, and lowest-appropriate-priority PendSV for switching.
- Preserve AAPCS, 8-byte alignment, Thumb xPSR, EXC_RETURN, and verified C/assembly
  TCB offsets. PendSV defers switching until more urgent ISR work completes.
- Planned location: `arch/cortex_m4/{port.c,exception.c,context_switch.S}`.

### ED5 — FPU policy gate

- Before any floating-point task is enabled, choose and document: prohibit FP;
  eagerly preserve supported FP state; or conditionally preserve it using
  EXC_RETURN/FPCA with lazy stacking deliberately configured.
- Initial choice: prohibit task FP through the integer-only context-switch milestone.
  Add conditional support only if the final workload requires it.
- No FP compiler output/task use is permitted until build flags, FPCCR behavior,
  S16-S31 preservation, costs, and active/inactive-context tests are recorded.

### ED6 — Periodic tick first

- Start with a documented periodic tick, wrap-safe absolute time, and periodic
  release using `next_release += period` to prevent phase drift.
- Tickless/one-shot scheduling is optional after the tick-based kernel is correct
  and measured; it is not required for the resume claims.
- Tradeoff: periodic interrupt overhead and tick granularity in exchange for simpler
  timeout/release correctness. Reconcile existing TIM2/TIM5 tickless comments during
  M6 rather than treating them as an implemented commitment.

### ED7 — Queue and priority-inheritance scope

- Queues use fixed-capacity caller storage and explicit full/empty/timeout results.
  Copy cost is bounded by configured element size, not O(1) for arbitrary messages.
- Mutexes are task-context only. PI must cover the low/medium/high case and restore
  effective priority across multiple owned mutexes.
- M8 must decide recursion, waiter order, owned-mutex and chain limits, timeout
  propagation, and cycle/deadlock behavior before coding. PI mitigates inversion;
  it neither prevents deadlock nor proves a blocking bound without critical-section
  and chain analysis.

### ED8 — Tests accompany implementation

- Add executable tests in every milestone. Phase 3 consolidates regression,
  adversarial interleavings, long runs, and measurement evidence; it does not defer
  basic testing until the end.
- Host-test portable task/scheduler/queue logic. Verify startup, stacks, BASEPRI,
  exceptions, IRQ nesting, and timing on STM32F446 hardware.
- Final timing uses calibrated DWT timestamps and selected GPIO markers with raw
  samples, exact firmware/configuration, tool versions, and reproducible analysis.

### ED9 — M1 platform baseline (implemented, hardware-unverified)

- Build with GNU Arm Embedded GCC as freestanding Thumb-2 for Cortex-M4 using the
  soft-float ABI. This enforces the ED5 integer-only baseline and avoids a C library,
  HAL, heap, or implicit runtime startup.
- Use project-owned minimal CMSIS-compatible core/device register definitions because
  CMSIS device headers were not installed. The surface is intentionally limited to
  SCB/VTOR, SysTick, RCC, Flash, and GPIOA; replace it with pinned vendor CMSIS only
  if the dependency and version are made reproducible.
- Link STM32F446RET6 Flash at `0x08000000` for 512 KiB and contiguous SRAM1/SRAM2 at
  `0x20000000` for 128 KiB. Reserve 2 KiB below initial MSP and assert vector size,
  vector origin, stack alignment, and static-RAM separation at link time.
- Use the reset-default 16 MHz internal HSI with AHB/APB divide-by-one. This is slower
  than the MCU maximum but avoids board-oscillator/PLL assumptions during bring-up.
  Clock waits are bounded and failure enters `platform_panic`.
- Use a 1 kHz SysTick only for the M1 LED heartbeat. It is BSP smoke-test code, not
  the future RTOS tick or scheduling behavior. LD2 is PA5 and toggles every 500 ms.
- Unhandled exceptions disable interrupts, expose `aerortos_fault_code`, break only
  when a debugger is attached, and otherwise remain in a terminal WFI loop. Detailed
  stacked-frame/fault-status capture remains later work.

### ED10 — M2 static task and initial-frame contract (implemented, target-unverified)

- The kernel owns eight fixed TCB slots; callers retain ownership and lifetime of
  their stack buffers. Priorities are 0-31, with lower numbers reserved by the future
  scheduler as higher logical priority. No heap or hidden stack allocation is used.
- `aerortos_task_create` rejects null entry/stack/output, non-8-byte-aligned stacks,
  stack sizes below 128 bytes or not divisible by 8, invalid priorities, overlapping
  active stack ranges, and exhausted slots. Multiple tasks may share an entry function
  only when their stack storage is distinct.
- Creation fills the stack, writes two low-address guard words, constructs an initial
  16-word integer context, and enters `READY`. A slot is reusable only after a legal
  transition to `TERMINATED` followed by explicit release. `task_system_init` is a
  pre-scheduling reset operation, not safe concurrent lifecycle API.
- From low to high address, `saved_psp` references R4-R11 followed by the hardware
  R0-R3/R12/LR/PC/xPSR frame. R0 carries the task argument, stacked PC is halfword-
  aligned with xPSR Thumb bit set, and LR points to a terminal task-return trap. The
  EXC_RETURN value is not stored per task under the integer-only policy; M5 must use
  the fixed thread/PSP basic-frame return value.
- A project-owned offset header fixes ARM offsets for assembly. Cross-compilation
  asserts `saved_psp=0`, `stack_low=4`, `stack_high=8`, `entry=12`, `argument=16`,
  and target TCB size 56 bytes. Both software and hardware frames are asserted at
  32 bytes. Host pointer widths may differ, so target-only offsets are asserted only
  in the 32-bit build.
- TCB pointers are valid only while their slot is allocated. `id` is diagnostic,
  not a generation-safe handle; callers must not retain a TCB pointer after release.
- M2 lifecycle operations are deliberately single-threaded and do not update ready
  queues or enter critical sections. M3 must centralize scheduler-owned state/list
  changes; M4 supplies the interrupt-safe atomicity mechanism before preemption.
- Guard checks occur on lifecycle transitions; fill scanning is an explicit bounded
  diagnostic proportional to configured stack size and must stay outside future hot
  scheduler/critical paths. Guards/high-water patterns detect some overwrites but do
  not replace MPU protection or on-target stack-bound validation.

## 4. Milestone plan

### Completed history

#### M0 — Architecture scaffold and repository audit — COMPLETE

- **Objective:** establish module boundaries and an honest baseline.
- **Relevant files:** all tracked files, `AGENTS.md`, and `roadmap.md`.
- **Decisions/tradeoffs:** preserve contracts as requirements, not functionality;
  focus the final scope on the five resume claims.
- **Acceptance:** every tracked source/test inspected; missing code/evidence recorded;
  only allowed documentation changed.
- **Evidence:** source/TODO/non-comment scan, file inventory, empty-unit compiler/
  assembler check, and symbol check on 2026-11-05.

## Phase 1 — Kernel Foundation

Phase exit: static integer-only tasks preempt correctly on hardware, with scheduler
invariants tested and BASEPRI validated before interrupt-driven preemption.

#### M1 — Reproducible build, startup, and hardware smoke test — IMPLEMENTED / UNVERIFIED

- **Objective:** build, link, flash, and debug a minimal STM32F446RET6 image.
- **Relevant files:** `Makefile`, `linker/stm32f446re.ld`,
  `startup/startup_stm32f446re.S`, `include/aerortos/*.h`, `app/main.c`,
  `bsp/stm32f446/{clock,gpio}.c`, `arch/cortex_m4/exception.c`, `.gitignore`.
- **Decisions/tradeoffs:** ED9 records the minimal project-owned register surface,
  soft-float/no-libc build, HSI clock, complete vector table, terminal faults, and
  heartbeat-only SysTick. OpenOCD commands target ST-Link and `stm32f4x`.
- **Acceptance:** warnings-as-errors build emits ELF/BIN/map; vector table, initial
  MSP, flash/RAM, reset/C initialization, and fault loop are correct; reset reaches
  a GPIO/debugger-visible marker on the declared board.
- **Verification evidence obtained (2026-11-05):** with
  `arm-none-eabi-gcc 14.2.1 20241119`, `make clean && make verify` completed under
  warnings-as-errors. The historical M1-only outputs were ELF 160,380 bytes (debug
  symbols), raw BIN 1,048 bytes, and map 14,101 bytes. Allocated image was text 1,044
  bytes, data 4 bytes, and BSS 8 bytes; M2 integration supersedes these sizes below.
- **M1 binary inspection obtained:** ELF32 little-endian ARM v7E-M, Thumb-2, soft-float;
  `.isr_vector` is 0x1C4 bytes at `0x08000000`; first word is initial MSP
  `0x20020000`; second word and ELF entry are Thumb `Reset_Handler` at `0x080003D5`;
  `.data` VMA is `0x20000000` with Flash LMA `0x08000414`; `.bss` begins
  `0x20000004`; no undefined symbols; GNU stack is non-executable. Linker assertions
  enforce 113 vector entries, vector origin, 8-byte MSP alignment, and 2 KiB MSP
  reserve. Stack-use reports show at most 8 static bytes in current C functions.
- **Primary-document cross-check:** ST DS10693/RM0390 confirm STM32F446RE has 512
  KiB Flash, 128 KiB SRAM, a 16 MHz internal HSI, and Flash beginning at
  `0x08000000`; the ST MB1136 NUCLEO-64 schematic connects green LD2 through the
  PA5 net. These checks validate configuration intent, not board execution.
- **Outstanding acceptance evidence:** install OpenOCD and an ARM-capable GDB, connect
  NUCLEO-F446RE/ST-Link, flash with `make flash`, observe LD2 toggling at 1 Hz, halt at
  `Reset_Handler`, confirm MSP/PC and `.data`/`.bss`, and reset/re-run successfully.
  Until these are recorded, M1 must not be marked COMPLETE.

#### M2 — Static TCB, stacks, and initial frames — IMPLEMENTED / UNVERIFIED

- **Objective:** define bounded task resources and valid initial contexts.
- **Relevant files:** `include/aerortos/{task,port,tcb_offsets}.h`, `kernel/task.c`,
  `arch/cortex_m4/port.c`, `tests/task_test.c`, `tests/host/platform_stub.c`, `Makefile`.
- **Decisions/tradeoffs:** ED10 records eight slots, 32 priorities, caller-owned
  stacks, lifecycle/release rules, two-word guards, fill scanning, frame ordering,
  return trap, fixed integer context, and target ABI offsets. The 128-byte minimum is
  a structural minimum, not a recommended application-task stack size.
- **Acceptance:** no heap; invalid input, duplicate task, misaligned/small stack, and
  capacity exhaustion fail explicitly; initial frame is AAPCS-correct and 8-byte aligned.
- **Verification evidence obtained (2026-11-05):** `make test` builds with the native
  C11 compiler under warnings-as-errors and reports `task_test: PASS`. Cases cover a
  valid TCB/frame, R4-R11 layout, R0 argument, return LR, aligned/Thumb PC, xPSR Thumb
  bit, aligned saved PSP, fill count, null/entry/alignment/size/priority failures,
  duplicate/overlapping stacks, all eight slots plus exhaustion, legal/illegal states,
  termination/release/reuse, released/foreign slot rejection, and guard corruption.
- **Additional evidence obtained:** ASan/UBSan at `-O1` passes with a non-PIE host
  binary and leak detection disabled because LeakSanitizer cannot operate in this
  container. `make clean && make verify` passes the host suite and strict soft-float
  Cortex-M cross-build. Target static assertions validate the 56-byte TCB and frame/
  offset ABI; target objects reference no allocation functions.
- **Integrated target artifact:** the current ELF retains the non-running smoke task,
  frame builder, and task pool for debugger inspection. It contains text 1,748 bytes,
  data 8 bytes, and BSS 720 bytes; raw BIN is 1,756 bytes. The vector still begins at
  `0x08000000`, initial MSP remains `0x20020000`, and current Thumb reset entry is
  `0x08000681`. Symbols expose `aerortos_m2_smoke_stack` at `0x20000008`, the smoke
  TCB pointer at `0x20000108`, and the static pool beginning at `0x20000118`.
- **Architecture cross-check:** Arm DUI 0553 confirms the basic exception-frame order,
  xPSR.T restoration on exception return, stack-alignment mechanism, and
  `0xFFFFFFFD` as the non-floating-point Thread/PSP EXC_RETURN for future M5 use.
- **Outstanding acceptance evidence:** inspect `aerortos_m2_smoke_task` and its frame
  through an STM32F446 debugger and eventually prove exception unstack/return in M5.
  The smoke task is constructed but deliberately never launched. M2 remains
  IMPLEMENTED / UNVERIFIED because host address projection cannot prove Cortex-M
  PSP, exception-return, or task-entry execution.

#### M3 — Bitmap scheduler and idle task — MISSING

- **Objective:** implement portable decisions before architecture preemption.
- **Relevant files:** `kernel/scheduler.c`, task state helpers,
  `tests/scheduler_test.c`.
- **Decisions/tradeoffs:** fixed priority/bit mapping, intrusive FIFO queues, idle at
  lowest priority, no task scan or initial time slicing.
- **Acceptance:** correct highest READY task; ED1-scoped O(1) operations; bitmap and
  queue emptiness always agree; FIFO ties, boundary priorities, block/unblock, and
  priority moves never lose or duplicate a task.
- **Verification evidence:** deterministic and seeded randomized host invariant tests
  added now, target cross-build, and selection code/disassembly review.

#### M4 — BASEPRI critical sections and IRQ contract — MISSING

- **Objective:** prove the atomicity mechanism required by preemption.
- **Relevant files:** `arch/cortex_m4/critical_section.c`, `port.c`, BSP IRQ setup,
  new host seams and target tests.
- **Decisions/tradeoffs:** implemented NVIC bits/threshold, nesting that never weakens
  masking, exact prior-state restore, justified barriers, kernel-aware IRQ band.
- **Acceptance:** nesting restores prior mask; urgent non-kernel IRQs remain live;
  kernel-aware IRQs cannot race scheduler state; misuse is detected; maximum masked
  duration is observable.
- **Verification evidence:** host mask-state tests plus on-target nested-priority IRQ
  traces and duration measurements. Preemption remains disabled until these pass.

#### M5 — SVC/PendSV launch and preemption — MISSING

- **Objective:** launch and preempt independent PSP-backed tasks.
- **Relevant files:** `arch/cortex_m4/{port.c,exception.c,context_switch.S}`,
  task/scheduler integration, `tests/context_switch_test.c`.
- **Decisions/tradeoffs:** MSP handlers, lowest-appropriate PendSV priority, verified
  C/assembly ABI, integer-only contexts until ED5 is completed.
- **Acceptance:** valid first exception return; repeated voluntary/interrupt switches
  preserve R4-R11 and hardware state; PSP is aligned/in-bounds; MSP serves handlers;
  higher-priority READY tasks preempt; nested IRQ and task return are safe.
- **Verification evidence:** on-target register-pattern stress, guard/PSP/MSP asserts,
  frame inspection, nested-IRQ cases, GPIO/DWT traces, and M1-M4 regression.

## Phase 2 — RTOS Features

#### M6 — Periodic scheduling, timeouts, and deadlines — MISSING

- **Objective:** provide correct periodic release and bounded timeout behavior.
- **Relevant files:** `bsp/stm32f446/timer.c`, `kernel/{timer,deadline}.c`, scheduler/
  task hooks, `tests/deadline_test.c`, new timer tests.
- **Decisions/tradeoffs:** periodic tick per ED6; define frequency, time width/wrap
  horizon, equal-expiry order, capacity, and deadline boundary; absolute releases.
- **Acceptance:** wrap comparisons and due ordering are correct; cancel/signal has one
  winner; phase does not drift; misses/overruns count once; no stale catch-up burst.
- **Verification evidence:** fake-clock wrap/race tests added now; target tick/release
  tests, long-run phase check, and deliberate miss/overrun cases.

#### M7 — Bounded message queues — MISSING

- **Objective:** implement bounded IPC with correct blocking and visible overload.
- **Relevant files:** `kernel/queue.c`, scheduler/timer APIs,
  `tests/queue_overflow_test.c`.
- **Decisions/tradeoffs:** caller storage, fixed element/capacity, try/timed/blocking
  APIs, deterministic waiter order, narrow ISR-safe subset, external freshness policy.
- **Acceptance:** capacity-one/wrap work; no overwrite/out-of-bounds access; all
  results explicit; timeout race removes once; higher-priority wake preempts; copy and
  memory bounds documented.
- **Verification evidence:** host edge/invariant/forced-interleaving tests added now,
  memory-safety tooling where feasible, target task/ISR handoff and DWT timing.

#### M8 — Priority-inheritance mutexes — MISSING

- **Objective:** mitigate inversion within a documented bounded PI scope.
- **Relevant files:** `kernel/mutex.c`, task/scheduler/timer integration,
  `tests/priority_inversion_test.c`.
- **Decisions/tradeoffs:** finalize ED7 recursion, ordering, ownership/chain limits,
  timeouts, cycles, and restoration before coding; mutexes remain forbidden in ISRs.
- **Acceptance:** low owner inherits high and runs before medium; READY owner requeues
  atomically; transfer/unlock is correct; timeout, several waiters, multiple owned
  mutexes, and supported chains restore the highest remaining/base priority.
- **Verification evidence:** exhaustive host state/order tests added now; target low/
  medium/high traces; controlled with/without-PI timing and documented blocking limits.

## Phase 3 — Verification

Phase 3 expands the tests introduced in M1-M8; it does not postpone them.

#### M9 — Integrated correctness and regression — MISSING

- **Objective:** make invariants and failure behavior continuously reproducible.
- **Relevant files:** all `tests/*.c`, kernel modules, `exception.c`, new build/CI scripts.
- **Decisions/tradeoffs:** fast portable host suite plus separate target exception/IRQ
  suite; deterministic schedules and recorded seeds over opaque random stress.
- **Acceptance:** one command runs clean build/host suite; target suite covers states,
  bitmap, wakeup races, exhaustion, stack guards, faults, wrap, queue contention, PI
  restoration, and long-running invariant checks; failures preserve evidence.
- **Verification evidence:** logs with commit/tools/seeds/configuration, fault records,
  and repeated target regression runs.

#### M10 — Latency, jitter, and deadline measurement — MISSING

- **Objective:** measure the distinct real-time quantities in resume bullet five.
- **Relevant files:** `arch/cortex_m4/port.c`, `bsp/stm32f446/gpio.c`,
  `kernel/monitor.c`, timing/overload tests, new analysis scripts/results location.
- **Decisions/tradeoffs:** calibrated DWT timestamps, atomic GPIO corroboration, fixed
  trace/histogram capacity, explicit overflow and instrumentation costs.
- **Acceptance:** separately define/record context-switch execution, interrupt-to-task
  dispatch, signed periodic release jitter, end-to-end response, and deadline totals/
  misses; overhead/wrap are characterized; raw data is exportable.
- **Verification evidence:** DWT cross-check with timer/logic analyzer; multiple cold
  boots; recorded board/probe/clocks/compiler/flags/IRQ priorities; sample counts, raw
  data, analysis version, drops, and justified min/median/tail/max summaries.

| Metric | Start | End |
|---|---|---|
| Context-switch execution | PendSV save-path entry | Restore/exception-return boundary |
| Preemption/dispatch latency | Event makes higher-priority task READY | First task instruction/marker |
| Periodic release jitter | Scheduled absolute release | First execution; signed difference |
| End-to-end response | Defined input/release | Defined output/completion |
| Deadline miss | Completion exceeds absolute deadline | Count once with totals retained |

These metrics are not interchangeable and observed distributions are not hard-real-
time guarantees.

## Phase 4 — Finalization

#### M11 — Minimal flight-like workload — MISSING

- **Objective:** exercise the RTOS with a credible, deliberately small workload.
- **Relevant files:** smallest useful `flight/*.c` subset and static workload config.
- **Decisions/tradeoffs:** synthetic/replayed sensor-control stages with declared
  periods, priorities, deadlines, loads, and IPC instead of a complete flight stack.
- **Acceptance:** exercises releases, preemption, queues, PI, bounded memory,
  deadlines, and overload recovery without implying a production controller.
- **Verification evidence:** reproducible replay/config, link-map RAM budget, stack
  high-water, regression result, and baseline timing run.

#### M12 — Hardware stress validation — MISSING

- **Objective:** validate correctness and timing under CPU, IRQ, and IPC contention.
- **Relevant files:** `tests/overload_test.c`, Phase 3 instrumentation, new stress
  generators and run manifests.
- **Decisions/tradeoffs:** stress CPU/ready churn, queue occupancy/timeouts, mutex
  blocking, valid nested IRQs, and deliberate overload. Add DMA/UART stress only if
  already required; add no vehicle protocols.
- **Acceptance:** repeated representative runs have no unexplained fault, lost wakeup,
  resource overrun, or invariant failure; declared deadlines meet preselected limits;
  overload produces correct miss/overrun/overflow behavior.
- **Verification evidence:** multiple cold boots, raw traces, exact ELF/map/config,
  workload/priority manifest, calibrated overhead, miss rate, and reproducible analysis.

#### M13 — Portfolio and README finalization — BLOCKED

- **Objective:** publish only claims supported by implementation and evidence.
- **Relevant files:** `README.md`, evidence links, final resume wording.
- **Decisions/tradeoffs:** reconcile `AeroRT`/`AeroRTOS`, repair stale `docs/` links,
  and state limitations without expanding scope.
- **Acceptance:** every target bullet traces to code, tests, target results, and raw
  measurement evidence; no unsupported hard-real-time/performance claim remains.
- **Verification evidence:** independent clean build/test/evidence reproduction and
  final traceability review.
- **Blocker:** incomplete implementation/validation and no explicit authorization to
  modify README after completion.

## 5. Evidence protocol and resume traceability

Final measurement runs record commit, ELF/map, board/probe, clocks, wait states,
compiler/flags, debug state, priorities, workload, capacities, duration, warm-up,
sample count, and instrumentation. Calibrate DWT/GPIO overhead, predeclare acceptance
limits, run unloaded/individual/combined/overload cases, repeat across cold boots,
retain raw machine-readable samples, and provide a reproducible analysis command.

| Final requirement | Required milestones | Current evidence | Completion criterion / verdict |
|---|---|---|---|
| Deterministic preemptive STM32 RTOS with synchronization | M1-M9, M11-M12 | Design comments only | Bounded kernel/workload passes target regression and stress. **Not defensible.** |
| PendSV/PSP context switching across preemption | M2, M4-M5, M9-M10 | TODO contracts only | Register/stack/nested-IRQ target tests and switch measurement pass. **Not defensible.** |
| Fixed-priority bitmap ready queues with scoped O(1) dispatch | M3-M5, M9 | Proposed contract only | ED1 operations and invariants are proven; target preemption works. **Not defensible.** |
| Bounded queues and PI mutexes | M7-M9, M12 | Proposed contracts only | Capacity/race/PI tests, traces, and limits are complete. **Not defensible.** |
| Measured switch latency, jitter, and deadline misses under stress | M6, M10-M12 | No samples | Repeated calibrated raw target evidence meets the protocol. **Not defensible.** |

“Deterministic” requires bounded resources/paths, explicit exhaustion, no runtime
allocation, analyzed masking/PI scope, and repeatable worst-case-oriented tests.
“Predictable” requires proven scheduler policy/invariants and stable declared stress
results. “Real-time” means specified workloads meet declared deadlines with disclosed
evidence and overload behavior, not a universal certification guarantee.

## 6. Immediate direction

Before M3, close the available hardware evidence for M1 and M2: flash the exact ELF,
verify heartbeat/reset/MSP, and use the debugger to inspect a synthetic TCB and its
aligned initial frame. Do not mark either milestone COMPLETE from host/build evidence.
M3 may then add only the portable bitmap scheduler and its tests; BASEPRI and
preemption remain M4/M5 work.
