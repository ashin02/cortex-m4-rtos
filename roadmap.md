# AeroRTOS Engineering Roadmap

## 1. Evidence baseline

Audit date: 2026-11-05. Audited commit: `3dd24e5` (`Initial commit`).

The initial repository contained only comment/TODO scaffolds. M1 now supplies a
freestanding ARM GCC build, STM32F446RE linker layout, vector/reset startup,
minimal device/core definitions, terminal fault handlers, 16 MHz HSI setup, and an
LD2 heartbeat image. The image builds and passes static binutils inspection, but no
hardware, probe, OpenOCD, or ARM GDB client was available for target execution.
M2 adds bounded static TCBs and initial integer frames. M3-M5 add fixed-priority
FIFO ready queues, a bitmap selector, nested BASEPRI protection, SVC first launch,
and PendSV context switching. The isolated M5 acceptance image passes on the
physical STM32F446RE, including repeated debugger-reset runs. M6 now implements
periodic timing and deadline tracking and its isolated acceptance image passes on
the physical STM32F446RE, including a debugger-reset rerun. M7 bounded message
queues also pass their isolated physical acceptance test and reset rerun. M8 bounded
priority-inheritance mutexes pass their isolated physical acceptance test and an
identical debugger-reset rerun. Broader multiple-ownership, transitive-PI, and M7
wait-list-reordering coverage remains separately supported by host/sanitizer tests.
At the implementation level, every milestone M1 through M8 is present. The
M5-M8 isolated hardware images exercise the integrated M1-M8 startup, task,
scheduler, interrupt, timing, queue, and mutex paths; M1-M4 do not have separate
standalone hardware acceptance records.
M9 supplies a deterministic integrated host regression and an isolated mixed-feature
target image. The physical STM32F446RE image passed with 69 checks, zero failures,
all completion flags, expected counters and trace, and an identical debugger-reset
rerun. Host-only scenarios remain identified separately below.
M10's original instrumented firmware also passed physical STM32F446RE acceptance
and a debugger-reset rerun with calibrated DWT measurements. A subsequent bounded-
statistics improvement preserves all observations in aggregate statistics after raw
storage fills. The corrected revision passed physical acceptance with 80 checks,
zero failures, and identical debugger-reset reproduction.

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
| Build/startup/target configuration | IMPLEMENTED / UNVERIFIED | ELF/BIN/map checks and M5 target reset/vector/debug execution pass; standalone M1 heartbeat acceptance remains unrecorded |
| TCB, stacks, task lifecycle | IMPLEMENTED / UNVERIFIED | Static lifecycle/frame host tests and target ABI checks pass; M5 proves task-frame exception return and guards, but full M2 acceptance remains separately tracked |
| Bitmap scheduler and idle | IMPLEMENTED / UNVERIFIED | Host FIFO/priority/invariant tests plus M5 target dispatch, preemption, and idle fallback pass; M3 remains separately tracked |
| Critical sections/IRQ policy | IMPLEMENTED / UNVERIFIED | Host prior-state tests and M5 target nested `0/0x40/0x40` behavior pass; urgent-IRQ liveness and masked-duration evidence remain |
| SVC/PendSV and PSP/MSP | COMPLETE | Physical STM32F446RE: 130 switches, priority/idle/IRQ cases, valid PSP/MSP/CONTROL, zero failures, repeated debugger-reset pass |
| Periodic time/deadlines | COMPLETE | Physical STM32F446RE M6 acceptance image: 49 passes, zero failures, 21 SysTick IRQs, one preemption, 17 idle entries, one deliberate high-priority deadline miss, and verified 4 ms/5 ms releases across tick wrap; debugger-reset rerun matched |
| Bounded message queues | COMPLETE | Physical STM32F446RE M7 acceptance image: 40 passes, zero failures, one preemption, one blocked send, two blocked receives, expected nine-event trace, empty queue/no waiters, and identical debugger-reset rerun |
| PI mutexes | COMPLETE | Physical STM32F446RE M8 acceptance image: completion magic `0x4D385041`, 43 passes, zero failures, one boost/restoration/block/preemption, all completion flags, empty mutex ownership/waiters, restored base priorities, expected nine-event trace, and identical debugger-reset rerun; multiple ownership, transitive PI, and M7 wait-list reordering remain host/sanitizer evidence |
| Integrated regression | COMPLETE | Eight host and sanitizer binaries pass after M10 additions; physical M9 passed 69/0 with three preemptions, two blocks/wakeups/queue operations/releases, one PI boost/restoration, all flags, and identical reset trace |
| DWT timing instrumentation | COMPLETE — HARDWARE VERIFIED | Original M10 firmware supplies the verified PendSV/dispatch/periodic/synchronization and 9-cycle DWT-overhead baseline; the corrected aggregate-statistics revision passed 80/0 twice with 104 critical observations summarized across a 64-sample raw buffer |
| Tests and measurements | PARTIAL | Host/sanitizer/static suites and isolated M5-M10 physical acceptance records pass; M10 observed maxima are not WCET and M11/M12 workload/stress evidence remains absent |
| Flight-like workload | MISSING | `flight/*.c` are comments only |
| Public finalization | BLOCKED | Requires completed evidence and explicit README authorization |

M0 and M5-M10 are complete. M1-M4 are implemented and exercised through the later
hardware images, but retain their separately recorded standalone verification
limitations. M10 and its aggregate-statistics revision are hardware verified. M11
and later remain out of scope.

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

- Queues use fixed-capacity caller storage and explicit full/empty results. M7 has
  indefinite blocking and deliberately defers timed waits/cancellation.
  Copy cost is bounded by configured element size, not O(1) for arbitrary messages.
- Mutexes are task-context only. M8 covers the low/medium/high case, transitive
  donation, and restoration across multiple owned mutexes. ED14 records the bounded
  policy selected for recursion, waiter ordering, ownership, chains, and cycles.
- PI mitigates inversion; it does not prove an application blocking bound without
  critical-section and acquisition-chain analysis.

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
  EXC_RETURN value is not stored per task under the integer-only policy; M5 uses the
  fixed thread/PSP basic-frame return value.
- A project-owned offset header fixes ARM offsets for assembly. Cross-compilation
  asserts `saved_psp=0`, `stack_low=4`, `stack_high=8`, `entry=12`, `argument=16`,
  and target TCB size 256 bytes after the M6 timing fields were appended. The five
  assembly-visible offsets remain unchanged. Both software and hardware frames are
  asserted at 32 bytes. Host pointer widths may differ, so target-only offsets are
  asserted only in the 32-bit build.
- TCB pointers are valid only while their slot is allocated. `id` is diagnostic,
  not a generation-safe handle; callers must not retain a TCB pointer after release.
- M2 lifecycle operations are deliberately single-threaded and do not update ready
  queues or enter critical sections. M3-M5 scheduler APIs now own runnable state/list
  changes after registration and use M4 critical sections.
- Guard checks occur on lifecycle transitions; fill scanning is an explicit bounded
  diagnostic proportional to configured stack size and must stay outside future hot
  scheduler/critical paths. Guards/high-water patterns detect some overwrites but do
  not replace MPU protection or on-target stack-bound validation.

### ED11 — M3-M5 dispatch and exception handoff (implemented; M5 target-verified)

- Priorities 0-30 are application priorities and 31 is reserved for the idle task;
  lower numbers are more urgent. Ready bitmap bit `31-priority` makes `CLZ` return
  the highest READY priority. Each priority owns an intrusive FIFO queue.
- A RUNNING application task remains selected when no READY task strictly outranks
  it. A higher-priority READY task causes the old RUNNING task to be changed to READY
  and appended exactly once. Yield changes the caller to READY before PendSV so it
  joins its FIFO tail; block removes it from eligibility; idle is never queued.
- `aerortos_current_task` continues to name the executing task until PendSV has saved
  R4-R11 and PSP. Only then does `aerortos_scheduler_switch_context` perform the
  RUNNING/READY handoff. This prevents the scheduler from losing the outgoing PSP.
- Scheduler mutation uses BASEPRI `0x40` with saved prior-state nesting. SVC is
  configured at `0x20`; PendSV at implemented-lowest `0xF0`. IRQs encoded `0x40` or
  less urgent may call the bounded wake API; more urgent IRQs must not call
  kernel APIs. Yield and block reject Handler mode.
- SVC restores the selected task's software frame and exception-returns with
  `0xFFFFFFFD`. PendSV relies on hardware for R0-R3/R12/LR/PC/xPSR, saves/restores
  R4-R11, and stores the resulting aligned PSP at TCB offset zero. The C/assembly
  offset assertions remain authoritative.
- The policy remains integer-only and soft-float. No FP context is saved and FPU use
  by tasks remains prohibited. Task return still enters the terminal return trap.
- `aerortos_scheduler_add` is startup registration. Direct M2 lifecycle transitions
  are not safe after scheduler ownership begins; M5 uses scheduler yield/block/wake
  for runnable-state changes.

### ED12 — M6 periodic release and deadline policy (implemented; target-verified)

- SysTick is the deterministic kernel time source at exactly 1 kHz from the 16 MHz
  HSI. It runs at encoded priority `0x40`, equal to the kernel BASEPRI threshold;
  SVC remains `0x20` and PendSV `0xF0`. The strong vector handler advances time and
  uses only the existing ISR-safe ready operation.
- Time is unsigned 32-bit milliseconds. Period, deadline, and initial-delay inputs
  are nonzero where required and limited to `INT32_MAX`; signed-difference ordering
  is valid over that declared half-range and across counter wrap.
- Periodic registration stores an absolute phase. Every due phase advances with
  `next_release += period`, never `now + period`, so handler or dispatch delay cannot
  accumulate drift. Simultaneous due tasks are processed in fixed registration order;
  fixed-priority/FIFO ready queues determine execution order.
- Each task has at most one active job. `aerortos_periodic_wait_next` records
  completion and blocks the calling Thread-mode task until the next phase. If a phase
  occurs while its prior job is active, the new job is not queued: one overrun and
  one skipped-release event are recorded, the old job continues, and the absolute
  phase advances. There is no backlog or catch-up burst.
- An active job misses once when tick time becomes strictly later than its absolute
  release-plus-relative-deadline. Completion on the deadline tick is on time. A miss
  does not kill the job; after completion, the next unoccupied phase releases it
  normally. Counters saturate at `UINT32_MAX`.
- SysTick accounts one execution tick to the RUNNING active periodic job and scans a
  static array of at most eight registered tasks. Dispatch remains ED1 O(1); interrupt
  work is O(8), a compile-time bound. Each TCB contains an overwrite-on-full ring of
  eight release/start/completion/miss/overrun records. Jitter and execution values
  have tick resolution and do not replace M10 calibrated measurements.

### ED13 — M7 bounded queue policy (implemented; target-verified)

- Queue control blocks and payload arrays are caller-owned and fixed for their full
  lifetime. Initialization rejects null storage, zero capacity/element size, and a
  capacity-times-element-size overflow. No heap, hidden payload storage, overwrite,
  queue deletion, or reinitialization with waiters is supported.
- `aerortos_queue_try_send/receive` return explicit full/empty results and never
  block. They are the only queue operations allowed from an ISR, and then only at a
  kernel-aware encoded priority `0x40` or less urgent. Blocking send/receive reject
  Handler mode and wait indefinitely; timed waits and cancellation remain deferred.
- The ring is FIFO, including capacity-one and index wrap. A successful receive wakes
  one blocked sender; a successful send wakes one blocked receiver. Waiters are
  ordered by effective priority and FIFO within equal priority. Wake removes the
  intrusive node and clears `wait_object` before making the task READY, so a PendSV
  request may safely preempt the caller after BASEPRI restoration.
- Queue state inspection, condition check, waiter insertion/removal, payload copy,
  and scheduler wake/block transition occur under the nested BASEPRI mechanism. This
  closes the check-before-block lost-wakeup window. A woken task rechecks the queue,
  so intervening consumers/producers cannot cause underflow or overflow.
- Ring index/count work is O(1). Payload copying is O(element size) and occurs while
  BASEPRI is raised; applications must bound element size accordingly. Ordered wait
  insertion is O(number of waiters), bounded by the eight-task system capacity;
  removal/wake is O(1). Input/output buffers must not overlap queue payload storage.

### ED14 — M8 bounded priority-inheritance mutex policy (implemented; target-validated)

- Mutex objects are caller-owned, explicitly initialized, non-recursive, and valid
  for their full use lifetime. There is no allocation, deletion, timed locking, or
  cancellation. Reinitializing an owned mutex or one with waiters is unsupported.
- `aerortos_mutex_lock`, `try_lock`, and owner-only `unlock` are Thread-mode APIs;
  mutex operations are rejected in Handler mode. Contended blocking lock atomically
  enqueues and blocks under BASEPRI. Unlock transfers ownership to the highest-
  effective-priority waiter and requests PendSV when the new owner outranks the
  running task.
- Waiters are ordered by effective priority with FIFO preservation for equal
  priorities. Each TCB retains immutable base priority and mutable effective
  priority. A priority change repositions a READY task in its ready queue or a
  BLOCKED task in its mutex or M7 queue wait list.
- Effective priority is the highest donation from the heads of all owned-mutex wait
  lists, otherwise base priority. Recalculation after waiter arrival or unlock
  propagates through mutex-owner chains, so multiple ownership and transitive PI are
  supported. Acquisition cycles are rejected as deadlock instead of blocking.
- Bounds follow the eight-task kernel: at most eight owned mutexes per task, eight
  waiters per list, and eight links in a propagation/deadlock walk. Ready selection
  remains O(1); ordered insertion/reordering, ownership scans, restoration, and
  transitive propagation are O(8), with nested propagation bounded by eight tasks.
  These are fixed system bounds, not unbounded asymptotic operations.

### ED15 — M9 deterministic integration regression (implemented; target-verified)

- Add one host executable dedicated to cross-milestone interactions while retaining
  every M1-M8 unit/regression binary. Schedules are explicit and repeatable; no
  randomized concurrency or timing-dependent host sleeps are used.
- Host scenarios cover M5+M6 wrap/preemption/deadline behavior, M6+M7 queue-blocked
  periodic overrun/skipped release, M7+M8 queue waiter reordering after donation,
  M5+M8 multiple ownership/restoration, and a combined M6+M7+M8 queue/PI handoff.
  Existing suites remain authoritative for invalid arguments/states, task and mutex
  capacity, deadlock rejection, FIFO ordering, stack guards, and handler boundaries.
- The isolated target schedule uses explicit high/consumer/low completion flags.
  The high periodic task never spins while lower-priority work is unfinished: after
  its first job it blocks in `aerortos_periodic_wait_next`, allowing the consumer and
  low task to finish and block before the second release validates and publishes.
- Completion magic is gated by the exact assertion count, zero failures, all flags,
  exact counters, empty queue/wait lists, released mutex ownership, restored base
  priorities, scheduler/queue/mutex/stack invariants, PSP Thread mode, unmasked
  BASEPRI, and the exact trace. Cross-build/static inspection is not physical-board
  validation; only recorded GDB results may close M9.

### ED16 — M10 opt-in DWT measurement (implemented and accepted)

- `AERORTOS_ENABLE_TIMING` keeps hot-path instrumentation out of normal M1-M9
  images. The M10 image enables DWT CYCCNT only after `SystemCoreClock` matches the
  expected 16 MHz HSI configuration and verifies that the counter advances.
- Raw unsigned cycle subtraction is valid across one 32-bit wrap. At 16 MHz the
  unambiguous interval is less than about 268 seconds; each measured interval is far
  shorter. Integer microsecond conversion uses 64-bit intermediate arithmetic.
- Six fixed-capacity series retain at most 64 raw samples. Observation count,
  64-bit total, minimum, maximum, and integer mean continue across raw-buffer
  saturation; retained and dropped counts remain separate. Total, observation
  count, and dropped count clamp at their integer maxima and set explicit saturation
  flags rather than wrapping. Thirty-two periodic records retain task ID, scheduled
  tick, actual release tick/cycle, and execution-start cycle. There is no allocation,
  printing, or UART activity.
- PendSV instrumentation is assembly-local and preserves the integer context: the
  start read occurs at handler entry before software save, and the end hook occurs
  after R4-R11/PSP restoration but before EXC_RETURN. It therefore measures the
  instrumented PendSV path, excluding hardware exception entry/return and the first
  SVC launch; it is not full event-to-task latency.
- READY and synchronization timestamps are recorded at enqueue/wake. Their end is
  the pre-EXC_RETURN hook, a close proxy for execution start that excludes hardware
  exception return instructions. Periodic records keep scheduled, actual-release,
  and start boundaries distinct. Outermost critical duration ends immediately before
  restoring prior BASEPRI. M6 deadline policy is unchanged; M10 mirrors miss totals.

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
  Cortex-M cross-build. The M2-era assertion validated the 56-byte TCB; the current
  M6 TCB is 256 bytes with unchanged assembly-visible offsets, and frame/offset ABI
  assertions still pass. Target objects reference no allocation functions.
- **Historical integrated artifact:** the M2-era ELF retained a non-running smoke
  task and measured text 1,748 bytes, data 8 bytes, and BSS 720 bytes. The current
  M6 default artifact supersedes it and launches real PSP tasks.
- **Architecture cross-check:** Arm DUI 0553 confirms the basic exception-frame order,
  xPSR.T restoration on exception return, stack-alignment mechanism, and
  `0xFFFFFFFD` as the non-floating-point Thread/PSP EXC_RETURN used by M5.
- **Later evidence:** M5 now proves Cortex-M task-frame exception unstack/return,
  aligned PSP use, and task-entry execution on STM32F446RE. M2 remains separately
  IMPLEMENTED / UNVERIFIED pending its own complete debugger-inspection record.

#### M3 — Bitmap scheduler and idle task — IMPLEMENTED / UNVERIFIED

- **Objective:** implement portable decisions before architecture preemption.
- **Relevant files:** `kernel/scheduler.c`, task state helpers,
  `tests/scheduler_test.c`.
- **Decisions/tradeoffs:** ED11 fixes priority/bit mapping, intrusive FIFO queues,
  reserved idle priority, no task scan, and no periodic time slicing.
- **Acceptance:** correct highest READY task; ED1-scoped O(1) operations; bitmap and
  queue emptiness always agree; FIFO ties, boundary priorities, block/unblock, and
  priority moves never lose or duplicate a task.
- **Verification evidence obtained (2026-11-07):** `scheduler_test` passes strict
  priority, higher-priority wake preemption, duplicate rejection, equal-priority FIFO
  yield, block, idle fallback/preemption, Handler-mode boundaries, nested host
  BASEPRI state, and ready-list/bitmap invariants. `context_switch_test` performs
  1,000 alternating handoffs with invariant, guard, and saved-PSP checks. ASan/UBSan
  passes both suites. This is portable policy evidence, not target dispatch evidence.
- **Target evidence obtained through M5:** the physical-board trace confirms FIFO
  peer order, higher-priority wake/preemption, block, idle fallback, resumed task,
  and zero scheduler-invariant failures. M3 remains separately IMPLEMENTED /
  UNVERIFIED because its full boundary-priority and priority-move acceptance record
  is broader than the M5 workload.

#### M4 — BASEPRI critical sections and IRQ contract — IMPLEMENTED / UNVERIFIED

- **Objective:** prove the atomicity mechanism required by preemption.
- **Relevant files:** `arch/cortex_m4/critical_section.c`, `port.c`, BSP IRQ setup,
  new host seams and target tests.
- **Decisions/tradeoffs:** four implemented priority bits, BASEPRI threshold `0x40`,
  saved-state nesting using `BASEPRI_MAX`, exact prior-state restore, DSB/ISB
  boundaries, SVC at `0x20`, and PendSV at `0xF0`. More urgent non-kernel-aware IRQs
  may run but may not call kernel APIs.
- **Acceptance:** nesting restores prior mask; urgent non-kernel IRQs remain live;
  kernel-aware IRQs cannot race scheduler state; misuse is detected; maximum masked
  duration is observable.
- **Verification evidence obtained (2026-11-07):** host seams prove nesting does not
  weaken the threshold and restores zero/nonzero prior values. The isolated target
  test pends kernel-aware SysTick while two BASEPRI levels are active, checks that it
  remains deferred until the outer restore, wakes a higher-priority task from the
  ISR, and expects PendSV tail-chain preemption.
- **Hardware failure observed after the SysTick-vector fix (2026-11-07):** GDB in
  privileged Thread/PSP mode reported outer saved BASEPRI `0`, inner saved BASEPRI
  `0`, and live BASEPRI `0`. Disassembly showed `movs r0,#128; mrs r0,BASEPRI; msr
  BASEPRI_MAX,r0`. The inline-assembly `"=r"` output allowed GCC to overlap the
  saved-output and threshold input; `mrs` destroyed the threshold before the write.
- **Fix after the failed run:** the saved-output constraint is early-clobber
  `"=&r"`, forcing distinct registers. The unexplained M5 `0x80` threshold drift was
  also reconciled with the earlier M4 contract: BASEPRI is restored to shifted
  priority `0x40`, SVC is `0x20`, the test SysTick is `0x40`, and PendSV is `0xF0`.
  Target static assertions enforce ordering/alignment. `make verify` disassembles the
  actual ARM object and rejects overlap; GCC 14.2.1 now emits `movs r3,#64; mrs
  r0,BASEPRI; msr BASEPRI_MAX,r3`. The target test records and checks outer `0`, inner
  `0x40`, and live masked BASEPRI `0x40`.
- **Target evidence now obtained:** the corrected M5 physical-board run passed the
  nested BASEPRI deferral, one kernel-aware IRQ wake, and one resulting priority
  preemption with outer `0`, inner `0x40`, live masked `0x40`, and zero failures.
  M4 remains IMPLEMENTED / UNVERIFIED because its broader acceptance also requires
  urgent non-kernel IRQ liveness/misuse evidence and observable masked duration.

#### M5 — SVC/PendSV launch and preemption — COMPLETE

- **Objective:** launch and preempt independent PSP-backed tasks.
- **Relevant files:** `arch/cortex_m4/{port.c,exception.c,context_switch.S}`,
  task/scheduler integration, `tests/context_switch_test.c`.
- **Decisions/tradeoffs:** ED11 defines the save-before-select handoff, strict
  higher-priority preemption, FIFO voluntary yield, MSP handlers, PSP tasks, fixed
  basic-frame EXC_RETURN, and integer-only context policy.
- **Acceptance:** valid first exception return; repeated voluntary/interrupt switches
  preserve R4-R11 and hardware state; PSP is aligned/in-bounds; MSP serves handlers;
  higher-priority READY tasks preempt; nested IRQ and task return are safe.
- **Verification evidence obtained (2026-11-07):** warnings-as-errors default and
  isolated target images link under `-mcpu=cortex-m4 -mthumb -mfloat-abi=soft`.
  `make verify` confirms the vector resolves strong SVC/PendSV/SysTick symbols, no
  undefined symbols, target TCB offset assertions, and a soft-float ELF. Handler disassembly
  shows SVC restoring R4-R11/PSP and both paths loading `0xFFFFFFFD`; PendSV saves
  R4-R11 below the hardware frame, stores PSP at TCB offset zero, calls the bounded
  selector on MSP, and restores the selected PSP. All three host tests and all three
  ASan/UBSan binaries pass.
- **Isolated target image:** `make m5-hardware-test` emits
  `build/m5_hw_test.{elf,bin,map}` (text 4,112 bytes, data 4 bytes, BSS 3,100 bytes
  with GCC 14.2.1). It exposes `aerortos_m5_hw_{pass_count,fail_count,switch_count,
  preemption_count,idle_count,irq_count,complete,trace_count,trace}` and a completion
  breakpoint. It covers first SVC entry, R4-R11 patterns, repeated FIFO switches,
  blocking, ISR higher-priority wake, nested BASEPRI deferral, idle fallback/wake,
  guards, PSP bounds/alignment, and scheduler invariants.
- **Hardware failure observed (2026-11-07):** the first STM32F446RE run proved SVC
  launch reached `task_a_entry`, then exception 15 entered `Default_Handler` and
  panicked with `0xFFFFFFFF`; CFSR/HFSR remained zero. VTOR was reset value zero and
  its aliased SysTick vector contained `0x08000625`, the Thumb address of
  `Default_Handler`; the linked ELF had no `SysTick_Handler`. Root cause was the
  startup object's `.thumb_set` weak SysTick alias binding its own vector relocation,
  which allowed the strong target-test C handler to be garbage-collected.
- **Fix after the failed run:** SysTick is no longer weak-aliased in startup, so both
  default and isolated images must supply a real handler or fail to link. Reset now
  writes `_vector_table` (`0x08000000`) to VTOR with DSB/ISB. Build checks require a
  strong `SysTick_Handler` distinct from `Default_Handler`. In the current rebuilt
  target, the vector slot is `0x08000595`, resolving to `SysTick_Handler` at
  `0x08000594` rather than `Default_Handler` at `0x080006CC`.
- **Critical-section debugging fix:** a subsequent board run exposed overlapping GCC
  operands in `aerortos_critical_enter`: `"=r"` allowed `mrs` to overwrite the
  threshold before `msr BASEPRI_MAX`. The output is now early-clobber `"=&r"`; the
  M4 ceiling is consistently `0x40`, and actual-object disassembly is regression
  checked. The corrected image records outer/inner/live BASEPRI for target assertions.
- **Physical STM32F446RE verification passed (2026-11-07):** completion magic was
  `0x4D355041`; 157 checks passed with zero failures; 130 context switches completed;
  one higher-priority preemption, one idle transition, and one kernel-aware IRQ event
  occurred. BASEPRI was outer `0`, inner `0x40`, masked `0x40`. At completion,
  CONTROL was `0x2`, PSP `0x200003F0`, and MSP `0x2001FFD0`. The execution trace was
  `0x10000001, 0xA0000001, 0xB0000001, 0x10000002, 0x1D1E0001,
  0xA0000002`, demonstrating first high task, FIFO peers, higher-priority wake,
  idle fallback, and resumed task completion in the expected order. A debugger reset
  and complete rerun again produced 157 passes and zero failures.
- **Completion verdict:** M5 acceptance is met for integer-only SVC launch, repeated
  voluntary PendSV switching, ISR-triggered preemption, FIFO yield, blocking, idle
  fallback, register/stack/PSP/MSP integrity, nested BASEPRI handoff, and repeated
  reset execution on the declared board. M5 is COMPLETE.
- **Remaining limitations:** this is functional verification, not calibrated latency
  or WCET evidence. Floating-point task contexts remain prohibited; graceful task
  return is not implemented (return enters the terminal trap); no periodic scheduler,
  timeout, IPC, mutex, or M6 behavior is included. The repeated run was a debugger
  reset, not documented cold-power-cycle coverage.

## Phase 2 — RTOS Features

#### M6 — Periodic scheduling and deadline tracking — COMPLETE

- **Objective:** provide a deterministic 1 kHz time base, phase-correct periodic
  release/blocking, preemption, and bounded deadline/overrun statistics.
- **Relevant files:** `include/aerortos/{task,timer,deadline}.h`,
  `bsp/stm32f446/timer.c`, `kernel/{timer,deadline}.c`, scheduler/task hooks,
  `tests/deadline_test.c`, and `tests/target/m6_periodic_test.c`.
- **Decisions/tradeoffs:** ED12 defines the time width/wrap horizon, fixed eight-task
  scan, simultaneous-release ordering, one-active-job/no-backlog overrun policy,
  strict-after deadline boundary, counter saturation, and bounded trace overwrite.
  General timed waits/timeouts are intentionally deferred to their owning IPC work;
  M6 adds only periodic wait-until-next-release.
- **Implementation:** task configuration accepts period, relative deadline, and
  initial release delay. Scheduler registration binds periodic TCBs to the fixed
  timer table. SysTick advances time, accounts execution, checks deadlines, advances
  every due absolute phase, wakes eligible blocked jobs through the ISR-safe ready
  path, and lets PendSV preempt when a higher-priority release becomes READY.
- **Host verification (2026-11-07):** strict `deadline_test` passes simultaneous
  release and priority order, higher-priority preemption request, periodic completion/
  block and idle fallback, releases at ticks 2/7/12 without drift, exact-deadline
  success, one deliberate miss, skipped-release overrun and recovery, execution-tick
  accounting, delayed-dispatch jitter, and wrap from `UINT32_MAX` to zero. Task tests
  reject inconsistent/zero/out-of-half-range timing configurations. Existing task,
  scheduler, and 1,000-handoff context-switch regressions still pass. ASan/UBSan pass
  all four host binaries with leak checking disabled as documented.
- **Cross-build verification (2026-11-07):** the default and isolated M5/M6 images
  build warning-free for Cortex-M4 soft-float. `make verify` confirms a strong
  `SysTick_Handler`, SVC/PendSV symbols, no linked undefined symbols, startup/vector
  layout, and the corrected BASEPRI operand separation. `make m6-hardware-test`
  confirms the M6 SysTick vector does not alias `Default_Handler`, has no undefined
  symbols, and disassembles the handler calling the bounded kernel tick path.
- **Isolated target acceptance image:** starts near 32-bit wrap and exports completion
  magic `0x4D365041`, PASS/FAIL counts, IRQ/preemption/idle counts, a 32-word execution
  trace, per-task release timestamp arrays, and TCB timing rings. Four tasks cover
  idle, low period-4 work, simultaneous high/peer period-5 release, actual higher-
  priority preemption, R4-R11 preservation, a deliberate deadline miss, recovery,
  phase spacing, wrap, guards, CONTROL.SPSEL, and scheduler invariants.
- **Hardware validation (2026-11-07):** the isolated image completed on the physical
  NUCLEO-F446RE with completion magic `0x4D365041`, 49 passes, zero failures, 21
  SysTick IRQs, one preemption, 17 idle entries, and one high-priority deadline miss.
  It verified periodic releases at 4 ms and 5 ms intervals across 32-bit tick
  wraparound. A debugger-reset rerun again produced 49 passes, zero failures, and
  the same one-miss count. This closes M6's target acceptance criteria.
- **Coverage qualification:** overrun and skipped-release paths remain supported by
  the recorded host-test evidence; they were not exercised in this hardware run.
- **Exact target procedure:** build with `make clean && make m6-hardware-test`, flash
  with `make flash-m6-test`, run `make openocd` in one terminal, and run
  `make gdb-m6-test GDB=gdb-multiarch` in another. In GDB use `continue`, then inspect
  `p/x aerortos_m6_hw_complete`, `p aerortos_m6_hw_pass_count`,
  `p aerortos_m6_hw_fail_count`, `p aerortos_m6_hw_irq_count`,
  `p aerortos_m6_hw_preemption_count`, `p aerortos_m6_hw_idle_count`,
  `p aerortos_m6_hw_trace_count`, `x/32wx aerortos_m6_hw_trace`, the three
  `aerortos_m6_*_releases` arrays, and the three application TCBs. Use
  `monitor reset halt`, reapply the completion breakpoint, and `continue` for the
  required debugger-reset rerun.

#### M7 — Bounded message queues — COMPLETE

- **Objective:** implement bounded IPC with correct blocking and visible overload.
- **Relevant files:** `include/aerortos/queue.h`, `kernel/queue.c`, existing scheduler
  block/wake APIs, `tests/queue_test.c`, and `tests/target/m7_queue_test.c`.
- **Decisions/tradeoffs:** ED13 defines caller storage, capacity/element bounds,
  explicit try results, indefinite task waits, priority/FIFO waiter order, the narrow
  ISR-safe try subset, and O(element-size) copying under BASEPRI. Timed waits,
  cancellation, deletion, overwrite/latest-value policy, and M8 PI are not included.
- **Implementation:** a generic byte-copy ring supports fixed-size messages and
  capacity-one/wrap cases. Full senders and empty receivers atomically join separate
  priority/FIFO wait lists and block through the existing scheduler. The opposite
  operation removes one waiter before making it READY, which requests PendSV when it
  outranks the running task. All queue state, payload copies, and wait-list changes
  are protected by nested BASEPRI critical sections; blocking APIs reject Handler
  mode while nonblocking try APIs follow the existing kernel-aware ISR contract.
- **Host verification (2026-11-07):** `queue_test` passes FIFO/wrap, capacity-one,
  full/empty, count/invariant diagnostics, invalid configuration/pointers, Handler-
  mode blocking rejection, ISR-style try operations, blocked sender and receiver
  wakeups, higher-priority preemption, equal-priority interleaving, and two concurrent
  receivers woken in priority order. All M1-M6 host regressions still pass. All five
  ASan/UBSan binaries pass with leak detection disabled as documented.
- **Cross-build/static verification (2026-11-07):** `make clean && make verify`,
  `make m5-hardware-test`, `make m6-hardware-test`, and
  `make m7-hardware-test` pass warnings-as-errors. The default ELF remains soft-float
  with strong SVC/PendSV/SysTick handlers, valid BASEPRI operand separation, and no
  undefined symbols. The corrected isolated M7 ELF is 6,108 bytes text, 8 bytes data,
  and 4,456 bytes BSS; its queue object has no malloc/free/memcpy/memset references.
- **Isolated target acceptance image:** completion magic is `0x4D375041`. A
  capacity-one queue forces one high-priority blocking send and two high-priority
  blocking receives. The low-priority peer frees/fills the queue and must be preempted
  by each wake. The image checks FIFO payloads, full handling, wait-list and scheduler
  invariants, empty completion state, stack guards, PSP Thread mode, restored BASEPRI,
  and exports PASS/FAIL, preemption/block counters, and a 16-word trace.
- **Hardware verification (2026-11-07):** after the test-only completion handshake
  fix, the physical NUCLEO-F446RE reached magic `0x4D375041` with exactly 40 passes,
  zero failures, one preemption, one blocked send, two blocked receives, all nine
  expected trace events, and a final queue count and sender/receiver waiter counts
  of zero. A debugger-reset rerun produced identical results. High cannot publish
  completion until Low records its final check; completion is gated on the handshake,
  exact pass count, and zero failures. Kernel queue behavior was unchanged by the
  synchronization fix.
- **Exact target procedure:** run `make clean && make m7-hardware-test`, then
  `make flash-m7-test`. Run `make openocd` in one terminal and
  `make gdb-m7-test GDB=gdb-multiarch` in another. Continue to
  `aerortos_m7_hw_test_complete_marker`, then inspect
  `aerortos_m7_hw_{complete,pass_count,fail_count,preemption_count,
  blocked_send_count,blocked_receive_count,trace_count,trace}` and
  `aerortos_m7_queue`, plus `low_final_check_complete`. Use `monitor reset halt`,
  retain/reapply the completion breakpoint, `continue`, and capture the same evidence
  from the reset rerun.

#### M8 — Priority-inheritance mutexes — COMPLETE

- **Objective:** mitigate inversion within the documented bounded PI scope.
- **Relevant files:** `include/aerortos/mutex.h`, TCB wait/ownership fields,
  scheduler effective-priority repositioning, M7 queue waiter reordering,
  `kernel/mutex.c`, `tests/mutex_test.c`, and `tests/target/m8_mutex_test.c`.
- **Decisions/tradeoffs:** ED14 selects caller-owned non-recursive mutexes,
  task-context-only operations, priority/FIFO waiters, owner-only unlock, an
  eight-mutex ownership limit, eight-task propagation bound, explicit cycle
  rejection, and no timeout/cancellation/deletion support.
- **Implementation:** each task keeps separate base/effective priorities and an
  intrusive owned-mutex list. Blocking donation recalculates the owner's best
  inherited priority, repositions READY, mutex-blocked, or queue-blocked tasks, and
  propagates through mutex-owner chains. Unlock removes the released mutex before
  restoration, transfers ownership to the highest-priority waiter, clears its wait
  membership exactly once, and wakes it through the scheduler under BASEPRI.
- **Host verification (2026-11-07):** all six M1-M8 host binaries pass. `mutex_test`
  covers initialization/context/ownership errors, recursive lock and non-owner
  unlock rejection, the eight-mutex ownership bound, equal-priority FIFO waiters,
  classic low/medium/high inversion, preemptive ownership transfer, restoration
  across multiple owned mutexes, transitive donation, ready/wait-list repositioning,
  M7 queue waiter reordering, cycle rejection, and scheduler/queue/mutex invariants.
  All six ASan/UBSan binaries pass with the documented leak check disabled.
- **Cross-build/static verification (2026-11-07):** clean `make verify` and isolated
  M5, M6, M7, and M8 target builds pass warnings-as-errors. The default ELF is 4,088
  bytes text, 8 bytes data, and 3,596 bytes BSS. The M8 ELF is 6,956 bytes text,
  8 bytes data, and 5,664 bytes BSS; it has strong SVC/PendSV/SysTick handlers, no
  undefined symbols, and the mutex object has no malloc/free/memcpy/memset references.
- **Isolated target image:** the low-priority task owns the mutex, then releases
  medium and high. High blocks and donates priority 2 to Low; Low must run ahead of
  Medium, transfer ownership, restore priority 10, and be preempted by High. High,
  Medium, and Low each set an explicit completion flag and block instead of spinning.
  Only after Low's post-unlock checks does it wake High to execute the final gate.
  Completion requires exactly 43 passing checks, zero failures, all three flags, and
  the exact nine-event trace, preventing the M7 early-completion failure mode.
- **Hardware verification (2026-11-07):** the isolated M8 image passed on the
  physical STM32F446RE with completion magic `0x4D385041`, 43/43 passing checks,
  zero failures, and exactly one priority boost, restoration, blocking event, and
  preemption. High, Medium, and Low completion flags were all `1`; the mutex owner
  was null with zero waiters; High/Medium/Low effective priorities were restored to
  their bases (`2`, `5`, `10`); and all owned-mutex counts were zero. All nine trace
  events matched `A8000001, B8000001, C8000001, A8000002, C8000002, A8000003,
  B8000002, C8000003, A8000004`. A debugger-reset rerun produced identical
  completion, pass/fail, boost/restoration, and trace results.
- **Evidence scope:** this board acceptance covers the isolated low/medium/high
  inversion, donation, restoration, ownership transfer, and preemption scenario.
  Multiple mutex ownership, transitive PI, and M7 queue wait-list reordering remain
  host/sanitizer validation, as recorded above; they are not claimed as physical
  board coverage.
- **Exact target procedure:** run `make clean`, `make m8-hardware-test`, then
  `make flash-m8-test`. Run `make openocd` in one terminal and
  `make gdb-m8-test GDB=gdb-multiarch` in another. At
  `aerortos_m8_hw_test_complete_marker`, inspect
  `aerortos_m8_hw_{complete,pass_count,expected_pass_count,fail_count,boost_count,
  restoration_count,block_count,preemption_count,trace_count,trace}`;
  `high_complete`, `medium_complete`, `low_complete`; `aerortos_m8_mutex`; and the
  base/effective priorities, owned counts, states, and stack guards of the three
  application TCBs. The recorded physical run and its debugger-reset rerun met these
  acceptance checks; retain this procedure for reproducibility.

## Phase 3 — Verification

Phase 3 expands the tests introduced in M1-M8; it does not postpone them.

#### M9 — Integrated correctness and regression — COMPLETE

- **Objective:** make invariants and failure behavior continuously reproducible.
- **Relevant files:** `tests/integration_test.c`,
  `tests/target/m9_integration_test.c`, all prior host/target tests, and `Makefile`.
- **Coverage matrix:**

  | Interaction | Deterministic coverage |
  |---|---|
  | M5 + M6 | Higher-priority periodic release preempts a low task across 32-bit wrap; deadline miss is counted strictly after its deadline; completion blocks and the next phase wakes/preempts with zero jitter |
  | M6 + M7 | An active periodic task blocks on an empty queue across its next phase; exactly one overrun/skipped release is recorded without a duplicate wake, then queue send wakes it to consume and complete |
  | M7 + M8 | A mutex donation changes the effective priority of a queue-blocked owner and moves it ahead of another receiver; queue, mutex, scheduler, wait-object, and wait-kind invariants are checked |
  | M5 + M8 | Preemption and blocking exercise two owned mutexes, independent donations, partial then full restoration, ownership handoff, and a separate transitive owner chain; the retained M8 suite also covers deadlock rejection |
  | M6 + M7 + M8 | A periodic high task blocks on a mutex owned by a queue-blocked low task; donation reorders/wakes the low task, queue traffic completes, unlock hands ownership to high, priorities restore, and the periodic job completes |
  | Regression/fault edges | Existing M1-M8 suites retain invalid state/context, task/mutex capacity, deadlock-cycle rejection, stack guards, FIFO/bitmap checks, and handler boundaries; M9 also rechecks nested prior-BASEPRI restoration |

- **Target scenario:** time begins at `UINT32_MAX-2`. The consumer (priority 5)
  blocks on a one-element queue; low (priority 10) owns the mutex; periodic high
  (priority 2, period 5, deadline 4, initial delay 10) releases at tick 7 after wrap,
  blocks on the mutex, and boosts low. Low sends one value, wakes the consumer, and
  transfers the mutex. High completes job one and blocks until tick 12. Consumer and
  low set their completion flags and block. High's second release verifies final
  state and is the only publisher of completion magic.
- **Static target acceptance contract:** completion magic `0x4D395041`; 69 passes;
  zero failures; high/consumer/low flags all one; counters are three preemptions,
  two blocks, two wakeups, two queue operations, one PI boost, one restoration, and
  two periodic releases. The exact eight-event trace is
  `B9000001, C9000001, A9000001, C9000002, A9000002, B9000002, C9000003,
  A9000003`.
- **Verification obtained (2026-11-07):** `make test` passes all seven binaries;
  `make test-sanitize` passes all seven with ASan/UBSan and leak detection disabled.
  `make clean && make verify` passes the warning-as-error Cortex-M4 soft-float build,
  all host tests, vector/link/static symbol checks, no-undefined-symbol checks, and
  the actual-object BASEPRI assembly check. `make m5-hardware-test
  m6-hardware-test m7-hardware-test m8-hardware-test m9-hardware-test` passes every
  isolated cross-build/static check using Arm GCC 14.2.1. The M9 ELF is 9,280 bytes
  text, 8 bytes data, and 6,240 bytes BSS; its strong SysTick handler and SVC/PendSV
  symbols are present, and queue/mutex objects reference no allocation or implicit
  memory routines. No kernel defect was found; no kernel source changed.
- **Physical acceptance passed (2026-11-07):** STM32F446RE completion magic was
  `0x4D395041`, with 69 passes, zero failures, three preemptions, two blocking
  events, two wakeups, two queue operations, one PI boost, one restoration, two
  periodic releases, eight trace entries, and all high/consumer/low completion
  flags equal to one. The queue was empty with no waiters and the mutex was unowned
  with no waiters. The trace was `B9000001, C9000001, A9000001, C9000002,
  A9000002, B9000002, C9000003, A9000003`. Completion magic, 69/0 counts, and the
  trace reproduced identically after debugger reset. This physical evidence covers
  only the isolated M9 target scenario; wrap/deadline/overrun combinations,
  multi-mutex restoration, transitive PI, deadlock, and queue reorder breadth remain
  host/sanitizer evidence as identified in the coverage matrix.

#### M10 — Latency, jitter, and deadline measurement — COMPLETE — HARDWARE VERIFIED

- **Objective:** measure the distinct real-time quantities in resume bullet five.
- **Relevant files:** `include/aerortos/{cycle_counter,monitor}.h`,
  `arch/cortex_m4/{cycle_counter.c,context_switch.S,critical_section.c}`,
  `kernel/{monitor,scheduler,timer,deadline,queue,mutex}.c`, `tests/timing_test.c`,
  `tests/target/m10_timing_test.c`, and `Makefile`.
- **Metric boundaries:**

  | Metric | Start | End / interpretation |
  |---|---|---|
  | Instrumented PendSV cycles | First DWT read at `PendSV_Handler` entry | DWT read after software restore/PSP update, before EXC_RETURN; includes hook entry and scheduler, excludes hardware exception entry/return |
  | READY-to-running dispatch | Scheduler changes/enqueues task as READY | Same pre-EXC_RETURN hook for selected task; includes pending/interference and PendSV work, excludes exception return |
  | Synchronization wake | Queue or mutex removes/clears waiter immediately before scheduler wake | Same selected-task boundary; tagged subset of dispatch latency |
  | Periodic release-to-start | SysTick observes phase and records actual release cycle | Same selected-task boundary; record separately retains scheduled tick and actual release tick/cycle |
  | Critical-section duration | First/outermost monitored critical entry after masking | Immediately before outermost prior-BASEPRI restoration; nested regions collapse into the outer sample |
  | Deadline statistics | Existing M6 strict-after-deadline observation | Existing per-task counters remain authoritative; monitor mirrors total observed misses without policy changes |

- **Benchmark:** six participants exercise 16 equal-priority voluntary yields, a
  higher-priority mutex wake/preemption, queue receiver wake, eight 6 ms periodic
  releases, and one controlled deadline miss. Thirty-two back-to-back DWT samples
  calibrate timestamp-read overhead. Each participant sets an explicit flag and
  increments a protected completion count; the high controller blocks and is woken
  only by the final participant before validating and publishing.
- **Acceptance contract:** completion magic `0x4D313041`, 80 passes, zero
  failures, six participant flags/count, 16 yields, two queue operations, one mutex
  handoff, eight periodic jobs, 32 overhead samples, at least eight periodic raw
  records, nonempty metric series, exactly one mirrored deadline miss, empty queue,
  unowned mutex, intact stacks, and scheduler/queue/mutex/monitor invariants.
- **Physical acceptance (original firmware, 2026-11-08):** STM32F446RE completed
  with magic `0x4D313041`, 80 passes, zero failures, six participants, 16 voluntary
  yields, two queue operations, one mutex handoff, eight periodic jobs, and one
  deliberate deadline miss. All six completion flags were set. The configured and
  validated core clock was 16 MHz and the measured DWT timestamp-read overhead was
  nine cycles. Completion values were identical after debugger reset.
- **Original verified timing baseline:** the following PendSV, dispatch, periodic,
  synchronization, and DWT-overhead measurements belong to the accepted original
  firmware. They were not repeated on the final aggregate-statistics firmware.

  | Metric | Samples | Mean | Observed maximum |
  |---|---:|---:|---:|
  | Instrumented PendSV software overhead | 42 | 27.93 µs | 30.69 µs |
  | READY-to-dispatch | 28 | 79.66 µs | 219.69 µs |
  | Periodic release-to-dispatch | 8 | 55.33 µs | 55.38 µs |
  | Synchronization wake-to-dispatch | 2 | 143.75 µs | 231.38 µs |
  | Outermost critical-section duration | 64 retained | 13.33 µs | 57.25 µs |

  The boundaries are exactly those in the metric table above. The PendSV number is
  software handler time to the pre-EXC_RETURN hook, not full switch-to-resume
  latency. Dispatch metrics also stop before hardware exception return. The
  critical series filled its 64-entry raw buffer and dropped 40 later observations;
  the reported original mean and maximum therefore describe the 64 retained samples,
  not all 104 observations. All figures include instrumentation effects except that
  timestamp-read overhead is reported separately. They are observations from this
  firmware/workload/debug configuration, not proven WCET bounds.
- **Post-acceptance monitor revision (2026-11-08):** raw storage remains 64 samples,
  but aggregates now cover every valid observation after raw saturation. Separate
  observation/retained/dropped counts and explicit saturating total/count behavior
  prevent wraparound. Host tests cover the 65th sample's effect on aggregates,
  64-bit total and 32-bit counter saturation, DWT wrap subtraction, and full reset.
- **Revised-firmware hardware finding (2026-11-08):** the first board run reached
  79/80 because the target-only acceptance check required the final count of 104
  critical observations while only 101 had occurred at that point in `high_entry`.
  The stable completion marker subsequently showed the correct final state: 104
  observations, 64 retained, and 40 dropped. No kernel, instrumentation, or
  statistics defect was present. The test now uses its actual assertion-time state
  to require a full raw buffer, at least one dropped observation, exact
  `observation = retained + dropped` consistency, nonzero total, mean within
  minimum/maximum via division-free total bounds, and no arithmetic-saturation
  flags. This remains one assertion, preserving the 80-check contract. The
  division-free form also preserves the freestanding no-runtime link contract.
  GDB showed 101 observations at the premature assertion and 104 at completion.
- **Final corrected-firmware acceptance (2026-11-08):** STM32F446RE completed with
  magic `0x4D313041`, 80 passes, zero failures, six completed participants, and one
  deliberate deadline miss. The critical-section series contained 104 observations,
  64 retained raw samples, 40 dropped raw samples, 23,161 total cycles, an 80-cycle
  minimum, a 916-cycle maximum, and zero saturation flags. At the verified 16 MHz
  core clock this is 5.00 µs minimum, 13.92 µs aggregate mean, and 57.25 µs observed
  maximum. All 104 observations contribute to count, total, minimum, maximum, and
  mean even though bounded raw storage retains only the first 64; `dropped` describes
  raw-sample loss, not aggregate-statistics loss. Completion magic, 80/0 counts, and
  these results reproduced identically after debugger reset.
- **Automated verification of the revision (2026-11-08):** all eight M1-M10 host
  binaries pass normally and with ASan/UBSan. `make clean && make verify` passes the
  warning-as-error default ARM build, host suite, ELF/vector/symbol checks, and
  actual-object BASEPRI check. Isolated M5, M6, M7, M8, M9, and revised M10 target
  builds/static checks all pass. The normal uninstrumented image remains 4,088 bytes
  text, 8 bytes data, and 3,596 bytes BSS. The acceptance-fixed M10 image is 10,984
  bytes text, 8 bytes data, and 11,480 bytes BSS; it has no undefined symbols or
  monitor references to allocation/printf, and its PendSV disassembly retains the expected
  PSP save/restore and pre-EXC_RETURN monitor hook. These are static/cross-build
  results complement the final physical validation above.

| Metric | Start | End |
|---|---|---|
| Context-switch execution | PendSV save-path entry | Restore/pre-exception-return boundary |
| Preemption/dispatch latency | Event makes higher-priority task READY | Pre-exception-return measurement hook |
| Periodic release jitter | Scheduled phase plus actual ISR release | Pre-exception-return measurement hook |
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
| Deterministic preemptive STM32 RTOS with synchronization | M1-M9, M11-M12 | M5 preemption, M7 queues, M8 PI, and integrated M9 pass repeated STM32 debugger-reset acceptance; broader multi-mutex/transitive/deadlock coverage remains host/sanitizer-only | Bounded integrated kernel/workload passes later target stress. **Core integrated behavior is supported; M12 stress remains.** |
| PendSV/PSP context switching across preemption | M2, M4-M5, M9-M10 | M5 physical target test passes 130 switches and context checks; accepted M10 firmware measured 42 instrumented PendSV samples with 27.93 µs mean and 30.69 µs observed maximum | Functional switching and the scoped instrumented measurement are supported; the maximum is not WCET. |
| Fixed-priority bitmap ready queues with scoped O(1) dispatch | M3-M5, M9 | Host suites plus physical FIFO/preemption/idle trace pass | ED1 operations and invariants are proven; broader integrated regression remains. **Functionally supported.** |
| Bounded queues and PI mutexes | M7-M9, M12 | M7/M8 isolated images and integrated M9 queue/PI handoff pass repeated physical acceptance; multiple ownership, transitive propagation, and deadlock breadth remain host/sanitizer-only | Capacity/race/PI tests, traces, and limits are complete. **Integrated core paths are hardware-verified; stress remains pending.** |
| Measured switch latency, jitter, and deadline misses under stress | M6, M10-M12 | Original M10 firmware supplies the repeated calibrated PendSV/dispatch/periodic/synchronization baseline; corrected final firmware verifies 104-observation critical aggregates and one deadline miss with an identical reset rerun | M10 synthetic benchmark evidence is complete; representative M11 workloads and M12 stress/WCET analysis remain. |

“Deterministic” requires bounded resources/paths, explicit exhaustion, no runtime
allocation, analyzed masking/PI scope, and repeatable worst-case-oriented tests.
“Predictable” requires proven scheduler policy/invariants and stable declared stress
results. “Real-time” means specified workloads meet declared deadlines with disclosed
evidence and overload behavior, not a universal certification guarantee.

## 6. Immediate direction

M1-M10 are implemented. M9 and M10 are complete with repeated physical STM32F446RE
debugger-reset evidence; M10's original baseline and final aggregate-statistics
evidence remain distinguished above. Scoped host-only scenarios remain identified
separately. M11 and later milestones remain unstarted and out of this change's scope.
