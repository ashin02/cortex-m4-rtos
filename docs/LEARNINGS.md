# Building a Preemptive RTOS from Scratch on Cortex-M4

This guide explains how to reconstruct **Preemptive RTOS for Cortex-M4** in its
actual implementation order, M1 through M10. It is based on the current source,
tests, linker/build configuration, and the recorded STM32F446RE evidence—not merely
on the project summaries. Internal C symbols retain their real `aerortos_` prefix so
that every discussion can be mapped back to code.

Three kinds of statements are kept separate throughout:

- **Implemented behavior** is verified directly in the current source.
- **Verification evidence** is labeled as host execution, cross-build/static
  inspection, or physical STM32F446RE execution.
- **Alternatives and proposals** are technical comparisons, not claims about what
  was historically considered. In particular, ED1–ED16 in
  [roadmap.md](roadmap.md) are reconstructed design explanations, not a historical
  decision log.

There is one concrete documentation discrepancy worth recording up front.
[roadmap.md](roadmap.md) says the current target TCB is 256 bytes, but
the authoritative build ABI constant
[`AERORTOS_TCB_TARGET_SIZE`](include/aerortos/tcb_offsets.h) is **264 bytes**, and
[`port.c`](arch/cortex_m4/port.c) enforces it with `_Static_assert`. This guide uses
264 bytes. The five assembly-visible offsets—`saved_psp=0`, `stack_low=4`,
`stack_high=8`, `entry=12`, and `argument=16`—agree across code and documentation.

---

## 1. Why build a custom RTOS?

### 1.1 What an RTOS adds to bare metal

A minimal bare-metal program often starts as a superloop:

```c
for (;;) {
    read_sensor();
    update_control();
    send_telemetry();
}
```

This is easy to understand, but every operation delays everything after it. If
telemetry stalls, the next sensor update also stalls. Polling wastes CPU time, while
moving too much work into interrupts creates long interrupt latency and difficult
shared-state races.

An RTOS does more than register several functions. It provides machinery to:

1. Give each task an independent stack and saved register context.
2. choose which runnable task should own the one CPU;
3. preempt a less urgent task when a more urgent task becomes READY;
4. make a task BLOCKED while it has nothing useful to do and wake it exactly once;
5. represent periods, deadlines, queues, and mutex ownership as explicit kernel
   state with defined concurrency rules.

The three common execution models differ mainly in when control can change:

| Model | Switch point | Strength | Main risk |
|---|---|---|---|
| Superloop | A function returns | Very small and transparent | One long step delays the whole loop |
| Cooperative scheduler | A task yields or blocks | Simpler switching and fewer races | One faulty task can monopolize the CPU |
| Preemptive RTOS | Also when a higher-priority task becomes READY | Urgent work is insulated from lower-priority work | Context switching and shared-state protection must be exact |

### 1.2 Why fixed priorities and static resources fit this kernel

This kernel uses fixed priorities, where priority 0 is most urgent. It does not solve
an optimization problem at runtime: application analysis assigns importance, and the
scheduler always selects the most urgent READY task. The tradeoff is that poor
priority assignment can starve low-priority work, so response and blocking times
still need application-level analysis.

There is no kernel heap. TCBs come from a fixed eight-entry pool; task stacks and
queue payload buffers are caller-owned; mutex objects are caller-owned. Capacity
exhaustion is explicit. This can reserve memory that is not always used, but makes
memory lifetime and traversal bounds auditable.

The STM32F446RE contains a Cortex-M4F, yet this build uses `-mfloat-abi=soft` and
saves only an integer basic exception frame. Having an FPU in the device does not
make floating-point task contexts automatically safe. Until FP context preservation
is designed and tested, task FP use is prohibited.

The central lesson is that RTOS determinism begins with explicit bounds and state
invariants, not with an unsupported claim that every operation is “fast.”

---

## 2. From source code to executing firmware — M1

Before scheduling can exist, reset must reach valid C code. A bare Cortex-M does not
provide a hosted C runtime: firmware must define the memory map, initial stack,
vector table, global-data initialization, and fault behavior.

### 2.1 Cross-compilation and freestanding C

The [Makefile](Makefile) uses `arm-none-eabi-gcc` and these central options:

```make
CPU_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
CPPFLAGS := -Iinclude -DSTM32F446xx
```

The same [Makefile](Makefile) also sets `-ffreestanding -fno-builtin` for C
compilation and `-nostdlib -T linker/stm32f446re.ld` for linking.

Cross-compilation means the compiler runs on the development machine but emits
Cortex-M4 Thumb-2 instructions. In a *freestanding* C environment there is no
assumed process, operating system, conventional `crt0`, complete libc, or
`main(argc, argv)`. This project supplies the startup behavior it needs and even
uses local byte-copy/clear loops so the final image has no implicit runtime memory
routine dependency.

The build path is:

```text
C and Thumb-2 assembly
        │ compiler / assembler
        v
relocatable object files
        │ linker + linker script
        v
ELF (addresses, symbols, sections, debug data)
        │ objcopy -O binary
        v
BIN (raw image) ──> OpenOCD/ST-Link ──> MCU Flash
```

The ELF is the useful debugging artifact; the BIN is a raw Flash representation.

### 2.2 The linker script is a hardware contract

[stm32f446re.ld](linker/stm32f446re.ld) places 512 KiB of Flash at `0x08000000` and
128 KiB of contiguous SRAM at `0x20000000`. `_estack` is the top of SRAM, with 2 KiB
reserved below it for the initial MSP stack.

The [linker script](linker/stm32f446re.ld), symbols `MEMORY` and `_sidata`, says
exactly where the initialized bytes live before and after reset:

```ld
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
_stack_limit = _estack - 2K;
```

- `.isr_vector`, `.text`, and `.rodata` execute from Flash.
- `.data` has initial bytes in Flash but a runtime address in RAM. `_sidata` is the
  Flash source and `_sdata.._edata` is the RAM destination.
- `.bss` occupies RAM and is zeroed during reset.
- `.noinit` occupies RAM but is deliberately untouched by startup.

Link-time `ASSERT`s require the vector table at the Flash origin, exactly 113 vector
entries (`0x1C4` bytes), an 8-byte-aligned initial MSP, and no overlap between static
RAM and the reserved MSP region. Linker assertions are part of firmware correctness,
not cosmetic build details.

### 2.3 What happens before `main()`

The first two words in
[startup_stm32f446re.S](startup/startup_stm32f446re.S) are:

```asm
_vector_table:
    .word _estack
    .word Reset_Handler
```

On reset, hardware loads the first word into MSP and the second into PC. Then
`Reset_Handler`:

```asm
Reset_Handler:
    ldr r0, =0xE000ED08
    ldr r1, =_vector_table
    str r1, [r0]
    dsb
    isb
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
```

This excerpt from [`Reset_Handler`](startup/startup_stm32f446re.S) programs VTOR
first. The following numbered loops in that function copy words until `r1 == r2`,
then clear `_sbss.._ebss` with zero-filled `r3` before `bl main`. `r0` initially
points to the Flash initialization image and `r1` to its RAM destination.

1. writes `_vector_table` to SCB `VTOR` and executes `DSB; ISB`;
2. copies `_sidata` in Flash to `_sdata.._edata` in RAM;
3. zeros `_sbss.._ebss`;
4. branches with link to `main`;
5. enters `Default_Handler` if `main` ever returns.

No constructor framework, heap initialization, process exit, HAL, or full libc is
needed. Most unused interrupts are weak aliases of `Default_Handler`. SysTick is
intentionally different: a real `SysTick_Handler` must link, preventing a previous
failure where the vector resolved to the default handler.

[`system_clock_init()`](bsp/stm32f446/clock.c) selects the reset-default 16 MHz HSI,
uses divide-by-one AHB/APB settings, disables PLL/HSE, sets zero Flash wait states,
and confirms clock transitions with bounded waits. This is slower than the MCU's
maximum frequency but removes oscillator and PLL assumptions during bring-up.

[`platform_panic()`](arch/cortex_m4/exception.c) disables interrupts, records a
fault code, executes `BKPT` only when a debugger is attached, and otherwise remains
in a terminal `WFI` loop. Detailed stacked-fault capture is not implemented.

### 2.4 Verification and takeaway

M1 passed warning-as-error cross-build and ELF/vector/map/symbol inspection. Later
M5–M10 physical images necessarily traversed the same reset and C initialization
path. However, an independent M1-only LD2 heartbeat acceptance run was never
recorded. “Used successfully by later images” is not the same claim as “standalone
M1 acceptance completed.”

**Takeaway:** `main()` is not the beginning of a bare-metal program. It is the
result produced by the linker layout, vector table, reset assembly, and C storage
initialization.

---

## 3. Tasks, TCBs, and initial stack frames — M2

### 3.1 How several tasks share one CPU

The processor has only one live PC, SP, and register set. Several tasks appear
independent only because the kernel saves one task's state and restores another's.
The TCB (Task Control Block) associates that saved context with scheduling metadata.

[`aerortos_task_t`](include/aerortos/task.h) begins with `saved_psp`, stack bounds,
entry, and argument. It also contains intrusive ready/wait links, immutable base and
mutable effective priority, task state, periodic/deadline counters, an eight-event
timing ring, and mutex ownership data. Its current 32-bit target size is 264 bytes.

Important states are:

- **READY:** eligible to run and represented in scheduler state;
- **RUNNING:** represented by the current CPU context;
- **BLOCKED:** waiting for a release, queue, mutex, or explicit wake and therefore
  ineligible for selection.

Direct M2 lifecycle transitions are startup/test-only. Once registered, the
scheduler must change state and list membership together; changing only the state
byte would corrupt scheduler invariants.

### 3.2 Static ownership and creation checks

[`aerortos_task_create()`](kernel/task.c) selects an unused TCB from
`task_pool[8]`. The caller supplies an 8-byte-aligned stack and keeps it alive. The
function rejects null inputs, an entryless task, stacks smaller than 128 bytes,
misalignment, invalid priority, overlapping active stack ranges, inconsistent
periodic parameters, and pool exhaustion.

Creation fills the stack with `0xA5A5A5A5` and writes two low-address
`0xD15EA5E5` guard words. Guards catch some overwrite cases; fill scanning estimates
unused stack. Neither replaces MPU protection, and the O(stack-size) scan is kept
out of hot scheduling paths.

The preparation step in [`aerortos_task_create()`](kernel/task.c) connects caller
storage to the TCB and the port's initial frame:

```c
for (index = 0U; index < stack_words; ++index) {
    config->stack[index] = AERORTOS_TASK_STACK_FILL_WORD;
}
for (index = 0U; index < AERORTOS_TASK_STACK_GUARD_WORDS; ++index) {
    config->stack[index] = AERORTOS_TASK_STACK_GUARD_WORD;
}

clear_task(task);
task->stack_low = config->stack;
task->stack_high = &config->stack[stack_words];
task->entry = config->entry;
task->argument = config->argument;
```

The function then sets `base_priority`, `effective_priority`, and `state = READY`,
and calls `aerortos_port_build_initial_frame()` to set `saved_psp`. The TCB owns
metadata; `config->stack` remains caller-owned.

### 3.3 Constructing a context that has never run

Cortex-M exception return restores a defined frame from a stack. A new task has no
old frame, so
[`aerortos_port_build_initial_frame()`](arch/cortex_m4/port.c) constructs one that
looks as though the task had previously been preempted.

The decisive lines in
[`aerortos_port_build_initial_frame()`](arch/cortex_m4/port.c) are:

```c
frame_base = &stack_low[stack_words - AERORTOS_INITIAL_FRAME_WORDS];
if ((((uintptr_t)frame_base) & (AERORTOS_TASK_STACK_ALIGNMENT - 1U)) != 0U) {
    return false;
}

software_frame = (aerortos_software_frame_t *)frame_base;
hardware_frame = (aerortos_hardware_frame_t *)(void *)(
    &frame_base[sizeof(*software_frame) / sizeof(uint32_t)]);
```

After zeroing R4–R11, the function writes `argument` to stacked R0, the return
trap to stacked LR, `entry & ~1U` to stacked PC, and `0x01000000` to xPSR. Finally
`*saved_psp_out = frame_base`. The pointer arithmetic places the hardware frame
eight words above the software frame, ready for SVC to consume in that order.

The stack grows toward lower addresses. From low to high address, the initial frame
is:

```text
higher address                                                     stack_high
               +-----------------------+
               | xPSR = 0x01000000     |  Thumb T bit set
               | PC = entry & ~1       |  stacked PC bit 0 clear
               | LR = return_trap | 1  |  task return becomes a panic
               | R12 = 0               |
               | R3  = 0               |
               | R2  = 0               |
               | R1  = 0               |
               | R0  = argument        |  first AAPCS argument
               +-----------------------+  hardware basic frame: 8 words
               | R11 = 0               |
               | R10 = 0               |
               | R9  = 0               |
               | R8  = 0               |
               | R7  = 0               |
               | R6  = 0               |
               | R5  = 0               |
lower address  | R4  = 0               |  <-- saved_psp
               +-----------------------+
               | fill pattern          |
               | guard | guard         |  stack_low

stack growth: toward lower addresses
```

R4–R11 form the software-saved frame; R0–R3, R12, LR, PC, and xPSR form the
hardware exception frame. The complete frame is 16 words (64 bytes) and remains
8-byte aligned. R0 carries the task argument. xPSR bit 24 selects Thumb state. The
stacked PC has bit 0 clear because xPSR carries execution state; the ordinary LR
branch target is made odd for Thumb.

Creating this memory image does **not** execute the task. Execution begins only when
M5 restores the frame and performs an exception return.

Dynamic stack/TCB allocation would be more flexible but add allocator failure,
fragmentation, and lifetime complexity. Shared-stack coroutines can be smaller but
do not naturally preserve arbitrary preempted C call chains. This project's static
design exchanges flexibility for explicit ownership and bounds.

[`task_test.c`](tests/task_test.c) checks the frame words, argument, PC/LR/xPSR,
alignment, capacity, lifecycle, duplicate stacks, and guards on the host. M5 later
proved that hardware could actually unstack the frame. There is no standalone M2
physical acceptance record.

**Takeaway:** a task is not merely a function. It is a function plus an independent
stack, restorable register frame, and scheduling state.

---

## 4. Fixed-priority scheduling — M3

M2 can represent tasks but cannot choose one. M3 needs a predictable mapping from
the READY set to the next task.

### 4.1 One FIFO queue per priority

[`scheduler.c`](kernel/scheduler.c) contains 32 ready queues and a 32-bit bitmap.
Priorities 0–30 are for application tasks; 31 is reserved for idle. Lower numbers
are more urgent. Each TCB embeds `ready_next` and `ready_previous`, so queue nodes
require no allocation. Tail insertion and head removal are O(1), and equal-priority
tasks preserve FIFO order.

The bitmap mapping in [`priority_bit()`](kernel/scheduler.c) is:

```c
static uint32_t priority_bit(uint8_t priority)
{
    return UINT32_C(1) << (31U - priority);
}
```

Priority 0 maps to bit 31 and priority 31 to bit 0. Therefore `CLZ` (count leading
zeros) directly returns the most urgent ready priority.

```text
READY priorities: 2, 5, 10
bitmap bits:      29, 26, 21
CLZ(bitmap) = 2  -> dequeue the priority-2 FIFO head
```

`CLZ` is not a sequential loop over all priorities. It operates on one fixed-width
32-bit value. The code calls `__builtin_clz` only when the bitmap is nonzero.

[`enqueue_locked()` and `dequeue_highest_locked()`](kernel/scheduler.c) couple list
membership to the bitmap. These selected lines are consecutive within each
function; the middle of `dequeue_highest_locked()` is omitted:

```c
queue->tail = task;
ready_bitmap |= priority_bit(task->effective_priority);
```

```c
if (ready_bitmap == 0U) {
    return NULL;
}
priority = (uint8_t)__builtin_clz(ready_bitmap);
queue = &ready_queues[priority];
task = queue->head;
queue->head = task->ready_next;
```

When a removal empties the queue, the implementation clears its bitmap bit. Thus
`CLZ` never selects an empty queue if the queue/bitmap invariant holds.

### 4.2 Selection, yield, block, and wake

[`select_next_locked()`](kernel/scheduler.c) embodies the policy:

```c
} else if ((ready_bitmap == 0U) ||
           (current->effective_priority <= highest_ready_priority_locked())) {
    return current;
} else {
    current->state = AERORTOS_TASK_READY;
    enqueue_locked(current);
}
```

This excerpt is the application-task branch inside `select_next_locked()`; the
preceding `if` handles idle. A lower numeric effective priority wins. If the
current task remains at least as urgent as the best READY task, the function
returns without dequeueing or moving any task. Otherwise it places current at its
priority's FIFO tail before choosing the new head.

- A RUNNING application task continues if no READY task strictly outranks it.
- If a higher-priority task is READY, the current task becomes READY and joins its
  own FIFO tail before the higher-priority head is selected.
- A task that voluntarily yields is already marked READY; during PendSV it is
  appended to its FIFO tail, allowing an equal-priority peer to run.
- A blocked task is not re-enqueued.
- If no application task is runnable, the dedicated idle task runs and executes
  `WFI`. Idle is never put in the regular queues.

Suppose tasks at priorities 2, 5, and 10 are READY. Priority 2 runs. When it blocks,
priority 5 runs; when priority 5 blocks, priority 10 runs. If 5A and 5B are in FIFO
order and 5A yields, 5A moves behind 5B. A new equal-priority task does not
immediately preempt a currently RUNNING task; only a strictly higher priority does.

`aerortos_scheduler_yield()` marks current READY and requests PendSV.
`aerortos_scheduler_block_current()` marks it BLOCKED and requests PendSV.
`aerortos_scheduler_make_ready()` enqueues a blocked/suspended task and requests
PendSV only if it outranks current. A scheduling event does not necessarily imply a
different selected task.

For example, [`aerortos_scheduler_yield()`](kernel/scheduler.c) changes the state
while BASEPRI is raised, and leaves `current` pointing to the outgoing task until
PendSV has saved its PSP:

```c
current->state = AERORTOS_TASK_READY;
#if defined(AERORTOS_ENABLE_TIMING)
    aerortos_monitor_task_ready(current);
#endif
aerortos_port_request_context_switch();
aerortos_critical_exit(critical_state);
```

The `select_next_locked()` branch for a READY but unlinked current task performs
the FIFO tail insertion during the later scheduler handoff. Blocking instead sets
`BLOCKED`; waking calls `enqueue_locked()` immediately under the same mask.

### 4.3 The exact O(1) claim

O(1) covers fixed-width highest-priority selection and intrusive ready-queue
head/tail operations. It does not cover:

- message copying, which is O(element size);
- ordered waiter insertion, bounded by eight tasks;
- priority-inheritance scans and propagation;
- the eight-entry periodic scan each tick;
- stack high-water scanning;
- context save/restore instruction cost.

A linear priority scan would also be bounded at 32 and simpler in some systems.
Round-robin emphasizes fairness rather than urgency. EDF uses dynamic deadlines but
requires a different ordering and analysis model. The bitmap design is a good fit
for small fixed-priority hardware because its selection path is compact and easy to
audit.

Host [scheduler tests](tests/scheduler_test.c) cover priority, FIFO, block/wake,
idle, and bitmap invariants. M5 later exercised FIFO yield, priority preemption, and
idle fallback on hardware, but M3 has no independent target acceptance record.

**Takeaway:** constant-time selection matters, but the deeper requirement is that
task state, list membership, and bitmap always describe the same reality.

---

## 5. Interrupt safety and BASEPRI — M4

### 5.1 Why kernel transitions must be atomic

Imagine Thread mode updating a ready queue. It changes the tail pointer, but before
setting the bitmap, SysTick interrupts and consults the scheduler. The ISR observes
a state that should never exist: a nonempty queue with a clear bitmap. Queue wakeup
has a similar check-then-update risk: without atomicity, a waiter can be missed or
woken twice.

A critical section makes a multi-field transition—state, links, counts, bitmap—look
indivisible to every context allowed to manipulate that state.

### 5.2 PRIMASK versus BASEPRI

`PRIMASK` broadly disables maskable interrupts. This kernel uses the more selective
`BASEPRI`. Cortex-M priority numbering is inverted from ordinary intuition: a
smaller numeric value is more urgent. STM32F446 implements four priority bits, so
encoded values use the high nibble.

```text
SVC       0x20   more urgent than the kernel ceiling
BASEPRI   0x40   masks 0x40 and numerically larger priorities
SysTick   0x40   kernel-aware and deferred in a critical section
PendSV    0xF0   lowest implemented urgency
```

With BASEPRI at `0x40`, interrupts `0x00`–`0x30` may still run, but they must never
call kernel APIs. An ISR that calls an ISR-safe kernel operation must have encoded
priority `0x40` or less urgent (numerically larger), so the kernel can mask it while
shared state is inconsistent.

### 5.3 Nested entry and exact restoration

The target implementation in
[`aerortos_critical_enter()`](arch/cortex_m4/critical_section.c) reads the previous
mask, writes the threshold with `BASEPRI_MAX`, and uses `DSB; ISB`:

```c
__asm volatile (
    "mrs %0, basepri\n"
    "msr basepri_max, %1\n"
    "dsb\n"
    "isb\n"
    : "=&r" (previous)
    : "r" (threshold)
    : "memory");
```

`aerortos_critical_exit()` writes the saved `previous_basepri` with `msr
basepri, %0` and follows it with `dsb` and `isb`. The threshold constant is
`AERORTOS_KERNEL_BASEPRI = 0x40` in
[`critical_section.h`](include/aerortos/critical_section.h); SVC and PendSV
priorities are written to SHPR2/SHPR3 by
[`aerortos_port_init()`](arch/cortex_m4/port.c), and SysTick's `0x40` is written
by [`aerortos_systick_init()`](bsp/stm32f446/timer.c).

The restoration instruction itself in
[`aerortos_critical_exit()`](arch/cortex_m4/critical_section.c) is:

```c
__asm volatile (
    "msr basepri, %0\n"
    "dsb\n"
    "isb\n"
    :
    : "r" (previous_basepri)
    : "memory");
```

The input is the saved value, including zero or an existing stronger mask;
restoring a fixed `0x00` would break a caller's outer critical section.

`BASEPRI_MAX` will not weaken an already stronger mask. The returned old value is
passed to `aerortos_critical_exit()`, which restores it exactly. A normal nested
sequence is therefore outer save 0, live `0x40`; inner save `0x40`, live `0x40`;
inner restore `0x40`; outer restore 0.

A real physical run exposed an inline-assembly register-allocation bug: without an
early-clobber output constraint, GCC could place `previous` and `threshold` in the
same register, so `MRS` destroyed the value later used by `MSR`. The output is now
`"=&r"`, and
[`verify_critical_assembly.sh`](tests/verify_critical_assembly.sh) checks the actual
object disassembly for separate registers. This demonstrates why source inspection
alone cannot validate inline assembly/compiler interaction.

Critical sections must remain short because SysTick and all kernel-aware IRQs are
delayed while they are masked. Queue payload copy occurs under BASEPRI, so message
size directly affects interrupt latency.

M5 hardware acceptance verified nested `0/0x40/0x40` values, deferred SysTick, and
the resulting wake/preemption. The broader M4 acceptance items—especially urgent
non-kernel IRQ liveness and misuse detection—were not independently completed.

**Takeaway:** the goal is not to disable every interrupt. It is to mask precisely
the contexts that are allowed to touch shared kernel state, while preserving the
previous mask across nesting.

---

## 6. SVC, PendSV, and real context switching — M5

The two central questions are: **How does `main()` launch the first task?** and
**How does a running Task A later become Task B?** These paths share exception
return machinery, but only the second has an outgoing task context to save.

### 6.1 Thread/Handler mode, PSP/MSP, and hardware frames

Normal task code runs in Thread mode on its caller-provided PSP (Process Stack
Pointer). Exceptions run in Handler mode on the common MSP (Main Stack Pointer).
After reset, `main()` itself is still Thread mode using MSP.

On exception entry, hardware pushes `R0-R3, R12, LR, PC, xPSR` onto the interrupted
context's active stack. R4–R11 are not in the basic hardware frame, so software must
save them. Handler C calls use MSP and do not consume task PSP space.

`EXC_RETURN` is not an ordinary code address. `0xFFFFFFFD` tells the processor to
return to Thread mode, unstack a non-FP basic frame from PSP, and use PSP thereafter.

### 6.2 First launch: `main()` to SVC

The default [application](app/main.c) performs this sequence:

1. initialize the 16 MHz clock and task/time systems;
2. create idle (priority 31), heartbeat (priority 5, 500 ms period/deadline), and
   peer (priority 6, 100 ms period/deadline) tasks;
3. initialize the scheduler and register application tasks;
4. configure SVC/PendSV in `aerortos_port_init()`;
5. configure the 1 kHz SysTick;
6. call `aerortos_scheduler_start()`.

`aerortos_scheduler_prepare_start()` chooses the first RUNNING task. In the default
image, both periodic tasks initially wait for their delayed first release, so idle
starts. [`aerortos_port_start_first_task()`](arch/cortex_m4/port.c) executes `svc 0`.

```c
void aerortos_port_start_first_task(void)
{
#if defined(AERORTOS_HOST_TEST)
    platform_panic(UINT32_C(0x7A5C0003));
#else
    __asm volatile ("svc 0" ::: "memory");
    __builtin_unreachable();
#endif
}
```

The host branch deliberately cannot simulate an exception return. On target,
`svc 0` moves from Thread/MSP to Handler/MSP. The selected TCB is already in
`aerortos_current_task`.

SVC entry stacks `main()`'s hardware context on MSP. Then
[`SVC_Handler`](arch/cortex_m4/context_switch.S) loads the selected TCB's
`saved_psp`, restores R4–R11, writes PSP so it points at the synthetic hardware
frame, clears BASEPRI, and executes exception return:

The non-error path in [`SVC_Handler`](arch/cortex_m4/context_switch.S) is:

```asm
SVC_Handler:
    ldr r1, =aerortos_current_task
    ldr r1, [r1]
    cbz r1, 1f
    ldr r0, [r1, #AERORTOS_TCB_SAVED_PSP_OFFSET]
    ldmia r0!, {r4-r11}
    msr psp, r0
    movs r0, #0
    msr basepri, r0
    ldr lr, =AERORTOS_EXC_RETURN_THREAD_PSP
    bx lr
```

`r1` is the selected TCB; the null branch reaches `Default_Handler`. `r0` starts
at stacked R4. The `!` advances it eight words to stacked R0, and `msr psp, r0`
sets the PSP for hardware unstacking. Clearing BASEPRI unmasks kernel-aware IRQs.
The assembly constant is defined as `0xFFFFFFFD` just above the handlers.

Hardware then unpacks R0–R3, R12, LR, PC, and xPSR from the task's PSP. The task
begins at its entry with its argument in R0. SVC did not call the task; it returned
through a manufactured exception frame.

### 6.3 Preemption: Task A to Task B

Suppose A is RUNNING and an interrupt or synchronization event makes higher-priority
B READY.

```text
Task A, Thread/PSP-A
    │ event enqueues B and pends PendSV
    v
hardware exception entry
    pushes A's R0-R3,R12,LR,PC,xPSR on PSP-A
    enters Handler mode on MSP
    v
PendSV software save
    pushes A's R4-R11 below that frame
    stores new PSP-A in A.saved_psp
    v
aerortos_scheduler_switch_context() on MSP
    queues A if still runnable, selects B, current = B
    v
PendSV software restore
    restores B's R4-R11 and sets PSP-B to B's hardware frame
    v
EXC_RETURN 0xFFFFFFFD
    hardware restores B's remaining registers from PSP-B
    v
Task B, Thread/PSP-B
```

The non-instrumented switch path in
[`PendSV_Handler`](arch/cortex_m4/context_switch.S) is:

```asm
    mrs r0, psp
    ldr r3, =aerortos_current_task
    ldr r2, [r3]
    cbz r2, 1f
    stmdb r0!, {r4-r11}
    str r0, [r2, #AERORTOS_TCB_SAVED_PSP_OFFSET]

    push {r3, lr}
    bl aerortos_scheduler_switch_context
    pop {r3, lr}
    cbz r0, 1f
    ldr r0, [r0, #AERORTOS_TCB_SAVED_PSP_OFFSET]
    ldmia r0!, {r4-r11}
    msr psp, r0
    ldr lr, =AERORTOS_EXC_RETURN_THREAD_PSP
    bx lr
```

The excerpt omits the conditional timing blocks: a DWT start read before `mrs`
and a monitor callback after `msr psp` when `AERORTOS_ENABLE_TIMING` is set.
`mrs` reads A's PSP after hardware has stacked its basic frame. `stmdb r0!`
decrements the pointer to make room for R4–R11; `str` records that new address in
A's TCB. `push`/`pop` protect LR and `r3` on the handler's MSP around the C call;
they never touch A's PSP. C returns the next TCB in R0. `ldmia r0!` restores B's
R4–R11 and advances to B's hardware frame, which `msr psp` installs for exception
return. The two null branches go to `Default_Handler`.

The C handoff in
[`aerortos_scheduler_switch_context()`](kernel/scheduler.c) is:

```c
aerortos_task_t *aerortos_scheduler_switch_context(void)
{
    aerortos_critical_state_t critical_state = aerortos_critical_enter();
    aerortos_task_t *next = select_next_locked();

    aerortos_critical_exit(critical_state);
    return next;
}
```

This call runs in Handler mode on MSP. `select_next_locked()` changes the current
pointer only after assembly has saved A's PSP. The critical exit restores the
prior mask before assembly installs B's context.

The outgoing PSP must be stored **before** the C scheduler changes
`aerortos_current_task`; otherwise the handler loses which TCB owns the context it
just saved. Wakeup can mark B READY and pend PendSV, but current continues to name A
until A's software frame is safe.

PendSV has the lowest urgency so all more urgent handler work completes first and
switching is deferred while BASEPRI is raised. SVC provides an explicit privileged
first-launch transition; PendSV centralizes deferred subsequent switches.

Task A's stack evolves like this:

```text
before exception:  PSP-A -> current call stack
after entry:       PSP-A -> [R0 R1 R2 R3 R12 LR PC xPSR]
after SW save:     PSP-A -> [R4 R5 R6 R7 R8 R9 R10 R11]
                            [R0 R1 R2 R3 R12 LR PC xPSR]
                            ^ A.saved_psp
```

Restoration reverses those two steps. The initial task-return trap in a synthetic
frame should not be confused with the interrupted LR saved on later exceptions.

### 6.4 Why FPU task context is unsupported

FP support would require a coherent compiler ABI, EXC_RETURN/FPCA handling, lazy
stacking policy, S16–S31 preservation, larger stack budgets, and active/inactive FP
tests. Current code always uses the basic-frame `0xFFFFFFFD` path and saves only
R4–R11. The soft-float build and policy therefore prohibit FP task code.

The host [context-switch test](tests/context_switch_test.c) performs 1,000 portable
scheduler handoffs but cannot execute ARM exception return. The physical M5 image
validated first SVC launch, repeated yield and ISR preemption, idle fallback,
PSP/MSP/CONTROL, guards, and R4–R11 patterns: 157 checks, zero failures, 130
switches, reproduced after debugger reset. This is functional evidence for the
integer path, not latency or WCET evidence.

**Takeaway:** a context switch is not a C call from one task to another. It is a
carefully paired hardware/software frame transfer followed by an exception return
onto a different stack.

---

## 7. Periodic scheduling and deadlines — M6

Scheduling answers “which READY task runs?” but not “when does a periodic job
become READY?” M6 adds a time model, periodic release policy, and bounded deadline
accounting.

### 7.1 READY is different from released

A nonperiodic task can be READY whenever an event wakes it. A periodic task has a
sequence of scheduled phases. Before a phase arrives it remains BLOCKED even if its
priority is high. At release, it becomes READY; it may still wait behind a more
urgent RUNNING task. Thus release time and actual execution start are distinct.

[`aerortos_systick_init()`](bsp/stm32f446/timer.c) derives an exact 1 kHz reload
from the validated 16 MHz core clock. `SysTick_Handler` increments an IRQ counter
and calls [`aerortos_timer_tick_isr()`](kernel/timer.c). Each tick:

```c
void SysTick_Handler(void)
{
    ++aerortos_systick_irq_count;
    aerortos_timer_tick_isr();
}
```

This is the strong vector handler in [`bsp/stm32f446/timer.c`](bsp/stm32f446/timer.c).
Its priority is set to `0x40` by `aerortos_systick_init()`, so it cannot interrupt a
kernel critical section.

1. increments the unsigned 32-bit millisecond tick;
2. accounts one execution tick to a running active periodic job;
3. scans at most eight registered periodic tasks;
4. records newly observed deadline misses;
5. advances due phases and either releases a task or records an overrun;
6. lets `aerortos_scheduler_make_ready()` request PendSV when a released task
   outranks current.

The ready-task selector remains O(1); SysTick work is O(8), a fixed system bound.

### 7.2 Absolute phase prevents drift

Each periodic task stores `next_release_tick`. The due branch in
[`aerortos_timer_tick_isr()`](kernel/timer.c) is:

```c
if (aerortos_tick_after_or_equal(now, task->next_release_tick)) {
    const uint32_t scheduled_tick = task->next_release_tick;

    task->next_release_tick += task->period_ticks;
    if (task->job_active != 0U) {
        aerortos_deadline_overrun(task, scheduled_tick, now);
    } else if (task->state == AERORTOS_TASK_BLOCKED) {
        aerortos_deadline_release(task, scheduled_tick, now);
#if defined(AERORTOS_ENABLE_TIMING)
        aerortos_monitor_periodic_release(task, scheduled_tick, now);
#endif
        (void)aerortos_scheduler_make_ready(task);
    }
}
```

The surrounding loop checks each registered periodic TCB after incrementing
`tick_count`. It checks the active job's deadline before evaluating this branch.
At a due phase, `scheduled_tick` preserves the planned time; `now` is when the ISR
noticed it. One active job causes a skip, while an unoccupied BLOCKED task becomes
READY and may pend PendSV. The phase advances in either case.

It does not use `now + period`. Suppose a 5 ms job should release at 10, 15, 20, …
but the tick handler services the first release at 11. `now + period` would shift all
future phases to 16, 21, …; adding to the old phase preserves 15, 20, … . Handler or
dispatch delay becomes measured jitter rather than permanent phase drift.

The period is the spacing between release phases. The relative deadline is the time
from a scheduled release to the required completion boundary. They need not be
equal. `absolute_deadline = scheduled_release + relative_deadline`.

### 7.3 Release, start, execution, completion, overrun, and miss

[`deadline.c`](kernel/deadline.c) separates these events:

```c
if ((task->job_active != 0U) &&
    (task->deadline_miss_recorded == 0U) &&
    aerortos_tick_after(now, task->absolute_deadline)) {
    task->deadline_miss_recorded = 1U;
    task->deadline_miss_count =
        saturating_increment(task->deadline_miss_count);
```

This opening of [`aerortos_deadline_check()`](kernel/deadline.c) requires an active
job, an unrecorded miss, and a strictly later tick. The rest of the branch records
an event and optionally mirrors it to M10's monitor. Calling the check again on
the same late job cannot increment its miss counter twice.

- `aerortos_deadline_release()` marks one active job, records the scheduled and
  actual release, and computes the absolute deadline.
- `aerortos_deadline_dispatch()` records the first execution start and release
  jitter when the scheduler actually selects the job.
- `aerortos_deadline_account_execution()` increments tick-resolution execution
  totals only while the active job is RUNNING.
- `aerortos_deadline_complete()` records completion when the task calls
  `aerortos_periodic_wait_next()` and then blocks it.
- `aerortos_deadline_check()` counts a miss once when the current tick is **strictly
  later** than the absolute deadline. Completion exactly on the deadline tick is on
  time.

A miss is an observation, not enforcement. The kernel neither kills nor forcibly
aborts a late job.

Completion is initiated by
[`aerortos_periodic_wait_next()`](kernel/timer.c), after it verifies Thread mode,
a RUNNING periodic task, and an active job:

```c
aerortos_deadline_complete(current, tick_count);
scheduler_result = aerortos_scheduler_block_current();
aerortos_critical_exit(critical_state);
return (scheduler_result == AERORTOS_SCHEDULER_OK) ?
       AERORTOS_TIMER_OK : AERORTOS_TIMER_ERROR_STATE;
```

Both completion accounting and the BLOCKED transition occur under the outer
critical section. The next SysTick release later makes the task READY again.

Each task can have at most one active periodic job. If another phase arrives while
the previous job remains active, `aerortos_deadline_overrun()` increments both
overrun and skipped-release counters. It does not enqueue a backlog. The old job
continues and the phase still advances. This avoids a catch-up burst after overload,
at the cost of deliberately dropping work.

### 7.4 Wrap-safe time

Ticks are unsigned 32-bit values. Ordering uses a signed difference:

```c
bool aerortos_tick_after(uint32_t first, uint32_t second)
{
    return (int32_t)(first - second) > 0;
}
```

This is the actual function in [`kernel/timer.c`](kernel/timer.c); its `>= 0`
companion is used for a due release. The half-range configuration limit is needed
because modular ordering is ambiguous for events separated by half a counter wrap.

For intervals no larger than `INT32_MAX`, this remains valid across wrap. For
example, tick 3 is after `UINT32_MAX` because the modular difference is 4. Task
creation rejects period/deadline/delay values outside the declared half-range.

### 7.5 Worked two-task example

Consider High at priority 2, period 5, deadline 2, first release tick 2; and Low at
priority 10, already READY.

```text
tick 0-1: Low runs
tick 2:   High release; High becomes READY; PendSV preempts Low
tick 2:   High first dispatch, release jitter 0
tick 3:   High deliberately continues
tick 4:   now is strictly after deadline 4? No—equal is on time
tick 5:   first observation strictly after deadline -> one miss
           High completes and blocks; Low resumes
tick 7:   next absolute phase (2 + 5), High releases without drift
```

If High were still active at tick 7, that phase would be counted as one overrun and
one skipped release, not as a second queued job. The exact host tests use similar
cases, including a period-4/deadline-2 job whose miss appears at tick 3 and whose
tick-4 release is skipped while the old job remains active.

An overwrite-on-full eight-event ring in each TCB stores release, start, completion,
miss, and overrun history. These values have 1 ms resolution and are functional
statistics, not the calibrated cycle measurements added in M10.

### 7.6 Tick-based versus tickless

A periodic tick is simple: one time source drives release and deadline logic at a
known 1 ms granularity. Its costs are a 1 kHz interrupt even when idle and
quantization to ticks. A tickless design would program the next timer event and save
idle power/interrupt overhead, but must safely reprogram one-shots, account elapsed
time, and resolve simultaneous events and races. It is a plausible future extension,
not current behavior.

The host [deadline tests](tests/deadline_test.c) cover phase preservation,
simultaneous releases, preemption, exact-deadline completion, deliberate miss,
overrun/skipped recovery, jitter, execution accounting, and wrap. The physical M6
image passed 49 checks with zero failures, 21 SysTick IRQs, one preemption, 17 idle
entries, one deliberate miss, and verified 4 ms/5 ms releases across wrap; the reset
rerun matched. Overrun and skipped-release paths were host-tested only, not exercised
in that physical scenario.

**Takeaway:** periodic scheduling needs an explicit time and backlog policy.
Absolute phases prevent drift; one-active-job semantics make overload visible and
bounded rather than silently accumulating work.

---

## 8. Message queues and blocking synchronization — M7

Scheduling alone cannot safely transfer data. A producer and consumer need both a
payload structure and an atomic rule for sleeping and waking.

### 8.1 Caller-owned bounded ring buffers

[`aerortos_queue_t`](include/aerortos/queue.h) stores caller-owned payload storage,
element size, capacity, `head`, `tail`, `count`, and separate sender/receiver wait
lists. Initialization rejects null storage, zero sizes, and
`capacity * element_size` overflow. There is no hidden allocation, overwrite mode,
deletion, or safe reinitialization while waiters exist.

Send copies one fixed-size element at `tail`, advances and wraps the index, and
increments count. Receive copies from `head`, advances and wraps, and decrements
count. The index/count work is O(1); the byte copy is O(element size) and occurs
under BASEPRI. Large elements therefore lengthen masked time.

The producer half in [`push_locked()`](kernel/queue.c) shows the ring update and
wakeup together:

```c
static void push_locked(aerortos_queue_t *queue, const void *message)
{
    uint8_t *destination = &queue->storage[queue->tail * queue->element_size];

    copy_bytes(destination, (const uint8_t *)message, queue->element_size);
    ++queue->tail;
    if (queue->tail == queue->capacity) {
        queue->tail = 0U;
    }
    ++queue->count;
    wake_first_locked(receive_wait_list(queue));
}
```

The consumer counterpart `pop_locked()` copies from `head`, decrements `count`,
and wakes one blocked sender. Neither helper acquires BASEPRI itself: its callers
already hold the queue critical section.

`aerortos_queue_try_send()` and `try_receive()` never block and return explicit
FULL/EMPTY results. They may run in Thread mode or in an ISR that obeys the
kernel-aware priority contract. Blocking `send()`/`receive()` are Thread-only and
wait indefinitely.

### 8.2 Wait lists and lost wakeups

Full senders and empty receivers use distinct intrusive wait lists. Insertion is
ordered by effective priority, with FIFO ties. The condition in
[`wait_enqueue_locked()`](kernel/queue.c) is:

```c
while ((position != NULL) &&
       (position->effective_priority <= task->effective_priority)) {
    position = position->ready_next;
}
```

Because lower numeric values are more urgent, a new equal-priority waiter passes
existing equals and is inserted after them. Insertion scans at most eight tasks;
removing the head is O(1).

The empty-check and block must be atomic. This broken sequence loses a wakeup:

```text
Consumer: checks empty
Producer: sends and sees no waiter
Consumer: joins wait list and sleeps forever despite queued data
```

The real blocking path holds the nested BASEPRI critical section while it checks the
condition, records `wait_object`/`wait_kind`, inserts the waiter, and changes it to
BLOCKED. An opposite operation removes the waiter and clears those fields **before**
calling `aerortos_scheduler_make_ready()`. A resulting PendSV can then run without
seeing stale wait-list ownership.

The full/empty decision in [`aerortos_queue_send()`](kernel/queue.c) is made under
one mask. This is an excerpt from the body of its `for (;;)` loop:

```c
if (queue->count < queue->capacity) {
    push_locked(queue, message);
    aerortos_critical_exit(critical_state);
    return AERORTOS_QUEUE_OK;
}
result = block_current_locked(queue, send_wait_list(queue),
                              AERORTOS_WAIT_QUEUE_SEND);
aerortos_critical_exit(critical_state);
if (result != AERORTOS_QUEUE_OK) {
    return result;
}
```

The receiver uses the same pattern with `count != 0U`. On full, the helper inserts
the current task into the sender wait list and calls
`aerortos_scheduler_block_current()` before the outer critical exit. The task
eventually resumes after PendSV and loops to check capacity again.

The counterpart [`wake_first_locked()`](kernel/queue.c) removes ownership before
the scheduler sees the waiter:

```c
wait_remove_locked(list, task);
task->wait_object = NULL;
task->wait_kind = (uint8_t)AERORTOS_WAIT_NONE;
#if defined(AERORTOS_ENABLE_TIMING)
    aerortos_monitor_sync_wake(task);
#endif
(void)aerortos_scheduler_make_ready(task);
```

The excerpt follows the function's null-head check. Removal clears intrusive
links, and `make_ready()` then changes BLOCKED to READY and updates the bitmap.

Waking grants another chance to compete; it does not reserve the message. Therefore
both blocking APIs are loops. After resumption they reacquire the critical section
and recheck full/empty. Another task may have consumed or filled the queue first.

### 8.3 Concrete producer/consumer walk-through

Use a capacity-one queue, High producer, and Low consumer:

1. High `try_send`s value A; count becomes one.
2. High calls blocking `send(B)`. The queue is full, so High joins the sender list,
   becomes BLOCKED, and PendSV selects Low.
3. Low receives A. `pop_locked()` makes space, removes High from the sender list,
   clears its wait metadata, and makes it READY.
4. Because High outranks Low, PendSV is requested. After Low leaves the critical
   section, High resumes inside its `send()` loop.
5. High rechecks capacity, copies B, and returns successfully.

The receiver direction is symmetric: a successful send wakes one priority/FIFO
receiver. The physical M7 test intentionally forced one blocked send and two blocked
receives in a capacity-one queue.

Queue try operations are only “ISR-safe” under the established priority ceiling;
they are not safe from an arbitrary urgent interrupt. Blocking operations reject
Handler mode because an ISR cannot suspend its call stack as a task waiter.

Host [queue tests](tests/queue_test.c) cover invalid inputs, FIFO/wrap,
capacity-one, full/empty, ISR-style try calls, blocked send/receive, preemption,
equal-priority behavior, and priority-ordered receivers. The physical M7 image
passed 40 checks, zero failures, one preemption, one blocked send, two blocked
receives, and its nine-event trace twice. Later PI-driven queue waiter reordering is
host/sanitizer evidence, not part of this M7 board scenario.

**Takeaway:** blocking IPC is primarily an atomic state-transition problem. A ring
buffer without an atomic check-enqueue-block sequence can still lose wakeups.

---

## 9. Mutexes and priority inversion — M8

### 9.1 The classic Low/Medium/High failure

Let Low have priority 10, Medium 5, and High 2.

```text
Low acquires mutex M
High becomes READY, preempts Low, then blocks trying to acquire M
Medium becomes READY and preempts Low
Medium can run for a long time
High waits indirectly for Medium, although Medium never uses M
```

A normal mutex protects data but does not solve this scheduling inversion. High can
only proceed after Low runs and unlocks, yet fixed priorities let Medium repeatedly
delay Low.

### 9.2 Base and effective priority

Each TCB has an immutable `base_priority` and mutable `effective_priority`. When High
blocks on Low's mutex, High's priority 2 is donated to Low. The scheduler now treats
Low as priority 2, so it runs ahead of Medium, finishes the critical section, and
unlocks. This is priority inheritance (PI).

[`aerortos_mutex_t`](include/aerortos/mutex.h) is caller-owned and non-recursive.
Operations are Thread-only; unlock is owner-only. Waits are indefinite. There is no
deletion, cancellation, or timeout. Each task may own at most eight mutexes.

### 9.3 Lock, donation, handoff, and restoration

[`aerortos_mutex_lock()`](kernel/mutex.c) follows this path:

1. if unowned, add the mutex to current's intrusive owned list;
2. reject recursive acquisition, capacity overflow, or a detected ownership cycle;
3. for contention, set current's wait metadata and insert it in effective-priority/
   FIFO order;
4. recompute the owner's effective priority;
5. reposition any task whose effective priority changed;
6. block the waiter atomically under BASEPRI.

Effective priority is the most urgent of the task's base priority and the head
waiter priorities on **all** mutexes it owns. This matters for multiple ownership.

[`desired_priority_locked()`](kernel/mutex.c) implements that rule:

```c
const aerortos_mutex_t *mutex = task->owned_mutex_head;
uint8_t priority = task->base_priority;
size_t count = 0U;

while ((mutex != NULL) && (count < AERORTOS_MUTEX_MAX_OWNED)) {
    if ((mutex->wait_head != NULL) &&
        (mutex->wait_head->effective_priority < priority)) {
        priority = mutex->wait_head->effective_priority;
    }
    mutex = mutex->owner_next;
    ++count;
}
return priority;
```

Each mutex wait list is priority ordered, so its head supplies that mutex's best
donation. The scan is limited to eight owned mutexes, and the immutable base
priority prevents a released donation from permanently boosting the task.
If Low owns M1 and M2, with priority-5 and priority-2 waiters respectively, Low
inherits 2. After releasing M2 it may still need priority 5 because M1 remains
contended; only after both donations disappear does it return to base 10.

Unlock removes the mutex from the old owner's list before recomputing restoration.
If a waiter exists, the highest-priority/FIFO waiter is removed, made the new owner,
and then made READY. Ownership is transferred before wakeup, so when the blocked
`aerortos_mutex_lock()` call resumes, it can verify that current already owns the
mutex and return `OK`. If the new owner outranks the unlocker, PendSV preempts the
unlocker after BASEPRI restoration.

The relevant beginning of [`aerortos_mutex_unlock()`](kernel/mutex.c) is:

```c
waiter = wait_dequeue_locked(mutex);
owned_remove_locked(current, mutex);
if (!recompute_priority_locked(current, 0U)) {
    aerortos_critical_exit(critical_state);
    return AERORTOS_MUTEX_ERROR_STATE;
}
if (waiter != NULL) {
    waiter->wait_object = NULL;
    waiter->wait_kind = (uint8_t)AERORTOS_WAIT_NONE;
    owned_add_locked(waiter, mutex);
```

The omitted tail recomputes the new owner's effective priority, optionally records
the sync-wake timestamp, calls `aerortos_scheduler_make_ready(waiter)`, and exits
the outer critical section. The order makes the handoff observable as owned before
the new task can resume.

### 9.4 Transitive inheritance and reordering

Suppose High waits on M1 owned by Mid, while Mid waits on M2 owned by Low. High
donates 2 to Mid; Mid's effective priority 2 must then propagate through M2 to Low.
The implementation follows mutex-owner chains with a maximum depth of eight tasks.

The contended branch in [`aerortos_mutex_lock()`](kernel/mutex.c) initiates donation
before blocking the requester:

```c
owner = mutex->owner;
current->wait_object = mutex;
current->wait_kind = (uint8_t)AERORTOS_WAIT_MUTEX;
wait_enqueue_locked(mutex, current);
if (!recompute_priority_locked(owner, 0U)) {
    wait_remove_locked(mutex, current);
    current->wait_object = NULL;
    current->wait_kind = (uint8_t)AERORTOS_WAIT_NONE;
    aerortos_critical_exit(critical_state);
    return AERORTOS_MUTEX_ERROR_STATE;
}
```

The following `aerortos_scheduler_block_current()` changes the requester to
BLOCKED. `set_priority_locked()` reorders a mutex or queue waiter whose effective
priority changes, then recursively recomputes the owner of the mutex it awaits.
Before reaching this excerpt, `would_deadlock_locked()` walks owner/wait links and
returns `AERORTOS_MUTEX_ERROR_DEADLOCK` for a cycle.

A priority change may affect more than the ready queue:

- a READY task is removed from its old priority queue and enqueued at the new one;
- a mutex-blocked task is removed and reinserted in that mutex wait list;
- a queue-blocked task is reordered through
  [`aerortos_queue_reorder_waiter()`](kernel/queue.c).

Without this reordering, the scheduler could know a donated priority while the
wait-list head still reflected stale ordering.

`would_deadlock_locked()` walks the current owner/wait chain. If acquisition would
lead back to the requester, or the bounded walk cannot establish a safe end, lock
returns `AERORTOS_MUTEX_ERROR_DEADLOCK` rather than blocking. This rejects
acquisition cycles; it does not prove that arbitrary application locking order is
otherwise well designed.

The decisive lines in [`would_deadlock_locked()`](kernel/mutex.c) are:

```c
for (depth = 0U; depth < AERORTOS_TASK_CAPACITY; ++depth) {
    const aerortos_mutex_t *wait_mutex;

    if (owner == current) {
        return true;
    }
    if ((owner == NULL) ||
        (owner->wait_kind != (uint8_t)AERORTOS_WAIT_MUTEX)) {
        return false;
    }
```

The remainder follows `owner->wait_object` to the mutex that owner awaits and
then to its owner. Reaching `current` proves a cycle; reaching an owner with no
mutex wait terminates safely. Exhausting the eight-task walk returns `true`.

### 9.5 Alternatives and limits

- A basic mutex is simpler but leaves unbounded priority inversion from unrelated
  medium-priority work.
- Priority inheritance changes priority only when contention occurs and fits dynamic
  ownership, but chain propagation and restoration are more complex.
- Priority ceiling assigns a predefined ceiling to each resource and can simplify
  blocking analysis or prevent some deadlocks, but requires a correct global
  resource/priority configuration.
- Disabling preemption around every shared resource is simple only for extremely
  short sections; long sections directly increase all higher-priority latency and
  do not scale as a general mutex policy.

PI mitigates inversion. It does **not** by itself prove a worst-case blocking-time
bound. Such a proof still needs maximum critical-section durations, resource nesting
and acquisition-chain analysis.

The host [mutex suite](tests/mutex_test.c) covers ownership errors, recursive lock,
capacity, FIFO ties, classic inversion, multiple ownership/restoration, transitive
PI, ready/mutex/queue reordering, and cycle rejection. The physical M8 image covers
only the classic Low/Medium/High case: 43 checks, zero failures, one boost,
restoration, block, and preemption, with the expected nine events twice. Multiple
ownership, transitive PI, and queue waiter reordering remain host/sanitizer-only.

**Takeaway:** PI is not merely changing a number in a TCB. Every ordering structure
that depends on effective priority must be updated atomically, and restoration must
consider all resources still owned.

---

## 10. Integrated behavior and validation — M9

Unit tests can validate each mechanism in isolation while missing interactions. M9
connects periodic release, queue blocking, PI, wakeup, and context switching in a
deterministic sequence.

### 10.1 The physical integrated scenario

[`m9_integration_test.c`](tests/target/m9_integration_test.c) starts time at
`UINT32_MAX - 2` and uses a one-element queue and one mutex:

- High: priority 2, period 5, deadline 4, initial delay 10;
- Consumer: priority 5, nonperiodic;
- Low: priority 10, nonperiodic;
- Idle: priority 31.

The actual sequence is:

The setup in [`m9_integration_test.c`](tests/target/m9_integration_test.c) makes the
schedule reproducible, including tick wrap and the delayed first High release:

```c
aerortos_time_system_init(UINT32_MAX - 2U);
check(aerortos_queue_init(&aerortos_m9_queue, queue_storage, 1U,
                          sizeof(queue_storage[0])) == AERORTOS_QUEUE_OK);
check(aerortos_mutex_init(&aerortos_m9_mutex) == AERORTOS_MUTEX_OK);
```

The following task creation calls give High `(priority=2, period=5,
deadline=4, initial_delay=10)`, Consumer priority 5, and Low priority 10. This
means the first High release is tick 7 after modular wrap, while Consumer and Low
can establish the queue/mutex blocking chain beforehand.

1. Consumer runs first, finds the queue empty, joins the receiver wait list, and
   blocks.
2. Low runs, locks the mutex, and waits in `WFI` for a mutex waiter.
3. Time wraps. High's first absolute release occurs at tick 7 and preempts Low.
4. High attempts the mutex, blocks, and donates effective priority 2 to Low.
5. Boosted Low runs ahead of Consumer. It sends one value, making Consumer READY,
   then unlocks the mutex.
6. Unlock hands ownership to High, restores Low to base priority 10, and wakes High.
   High preempts within Low's unlock sequence.
7. High observes ownership, unlocks, completes its first periodic job, and blocks
   until the next phase.
8. Consumer runs, receives the queued value, records completion, and blocks.
9. Low resumes after its earlier unlock, performs final checks, and blocks.
10. High releases again at tick 12 with zero tick jitter, validates final queue,
    mutex, stack, scheduler, priority, CONTROL, BASEPRI, counter, and trace state,
    then publishes completion.

The eight-event trace is fixed, not timing-dependent. The physical image passed 69
checks with zero failures, three preemptions, two blocks, two wakes, two queue
operations, one PI boost/restoration, and two releases. The reset rerun reproduced
the completion counts and trace.

The target scenario observes deadline state and verifies phase/jitter behavior, but
does not provide the full deadline/overrun interaction breadth. The host
[integration suite](tests/integration_test.c) separately covers a strict-after
deadline miss across wrap, a periodic task blocked on an empty queue across its next
phase (one overrun/skipped release), PI reordering of a queue waiter, multiple-mutex
restoration, transitive PI, and a combined periodic/queue/PI handoff. Those broader
paths are host/sanitizer evidence, not physical M9 coverage.

### 10.2 Why host tests are necessary but insufficient

Host tests are excellent for deterministic data-structure schedules, invalid input,
counter boundaries, cycle rejection, and sanitizer detection of C memory/undefined
behavior. They cannot prove:

- Cortex-M exception entry and `EXC_RETURN` behavior;
- real PSP/MSP and CONTROL selection;
- NVIC priority and BASEPRI masking/tail-chaining;
- real SysTick/vector behavior;
- instruction-cycle timing on STM32.

Conversely, one physical scenario does not cover every legal interleaving. Strong
evidence combines broad deterministic host regression with narrow, observable
hardware acceptance. Cross-compilation and disassembly sit below either runtime
test: they prove an image and instruction pattern exist, not that the behavior ran.

**Takeaway:** integration evidence must state both the interaction it proves and the
nearby interactions it does not. “Runs on hardware” is not a universal correctness
claim.

---

## 11. DWT measurement and benchmark interpretation — M10

Functional correctness does not tell us how many cycles a switch or wake takes. M10
adds opt-in measurement with explicit boundaries.

### 11.1 DWT CYCCNT

The Cortex-M Data Watchpoint and Trace unit contains `DWT_CYCCNT`, a 32-bit cycle
counter. [`aerortos_cycle_counter_init()`](arch/cortex_m4/cycle_counter.c) checks the
expected core clock, enables `DEMCR.TRCENA` and `DWT_CTRL.CYCCNTENA`, zeros the
counter, executes four NOPs, and verifies that the count advances. The target
branch in [`cycle_counter.c`](arch/cortex_m4/cycle_counter.c) is:

```c
AERORTOS_DEMCR |= AERORTOS_DEMCR_TRCENA_Msk;
AERORTOS_DWT_CYCCNT = 0U;
AERORTOS_DWT_CTRL |= AERORTOS_DWT_CTRL_CYCCNTENA_Msk;
__asm volatile ("dsb\n isb" ::: "memory");
before = AERORTOS_DWT_CYCCNT;
__asm volatile ("nop\n nop\n nop\n nop" ::: "memory");
after = AERORTOS_DWT_CYCCNT;
```

The subsequent `counter_ready` expression requires enabled trace/count bits and
`after != before`; the host branch uses an explicit counter mock.

At 16 MHz:

```text
1 cycle = 1 / 16,000,000 s = 0.0625 microseconds
microseconds = cycles / 16
```

Unsigned `end - start` handles one counter wrap correctly, provided the measured
interval is shorter than a full `2^32` cycles—about 268 seconds at 16 MHz. The
measured kernel intervals are much shorter.

Instrumentation is compiled only with `AERORTOS_ENABLE_TIMING`. Normal M1–M9 images
retain the uninstrumented path. This is important: measurement code changes the
firmware being measured.

### 11.2 Measurement boundaries

[`monitor.c`](kernel/monitor.c) defines six bounded series. Their meanings are not
interchangeable. The timed
[`PendSV_Handler`](arch/cortex_m4/context_switch.S) first reads DWT before the
normal save:

```asm
#if defined(AERORTOS_ENABLE_TIMING)
    ldr r1, =0xE0001004
    ldr r1, [r1]
    ldr r2, =aerortos_monitor_pendsv_start_cycle
    str r1, [r2]
#endif
```

After restoring the incoming PSP, it calls the end hook:

```asm
#if defined(AERORTOS_ENABLE_TIMING)
    push {r3, lr}
    bl aerortos_monitor_pendsv_end
    pop {r3, lr}
#endif
```

These excerpts surround the switch shown in Chapter 6. The callback uses MSP
before `bx lr` triggers hardware exception return, which is outside the sample.
The metric boundaries are:

| Metric | Start | End | What is excluded |
|---|---|---|---|
| PendSV software cycles | First DWT read at `PendSV_Handler` entry | After next R4–R11/PSP restore, before `EXC_RETURN` | Hardware exception entry/return, first SVC launch |
| READY-to-dispatch | Task is marked/enqueued READY | Selected-task pre-`EXC_RETURN` hook | Exception return and first task instruction |
| Periodic release-to-dispatch | SysTick records actual release cycle | Same pre-return hook | Scheduled-to-actual tick delay is stored separately; return itself excluded |
| Sync wake-to-dispatch | Queue/mutex removes and clears waiter immediately before wake | Same pre-return hook | Return and first task instruction |
| Outermost critical section | Outermost monitored entry after masking | Immediately before restoring prior BASEPRI | Nested sections are folded into the outer sample |
| Timestamp-read overhead | First of two back-to-back DWT reads | Second read | All hook bookkeeping and scheduler work |

PendSV execution time is therefore not event-to-dispatch latency. READY-to-dispatch
includes pending time and interference before PendSV plus its measured work.
Periodic latency starts at the actual release observation, not the ideal scheduled
phase. End-to-end response would need an application-defined input and output
boundary and is not supplied by these kernel metrics.

### 11.3 Bounded storage and aggregates

Each series retains at most 64 raw samples but also keeps 64-bit total, minimum,
maximum, observation count, retained count, dropped count, and saturation flags.
After raw storage fills, later observations still contribute to aggregates.
Counters/totals saturate with explicit flags instead of silently wrapping. Up to 32
periodic records additionally retain task ID, scheduled tick, actual release tick,
release cycle, and execution-start cycle. The raw-capacity branch in
[`aerortos_measurement_record()`](kernel/monitor.c) is:

```c
if (series->retained_count < AERORTOS_MEASUREMENT_CAPACITY) {
    series->samples[series->retained_count] = cycles;
    ++series->retained_count;
    return true;
}
if (series->dropped_count != UINT32_MAX) {
    ++series->dropped_count;
} else {
    series->saturation_flags |= AERORTOS_MEASUREMENT_DROPPED_SATURATED;
}
return false;
```

Earlier in this same function, observation count, 64-bit total, minimum, and
maximum are updated. Returning `false` means the raw value was not retained;
aggregate statistics still include it. `aerortos_monitor_pendsv_end()` reads one
`now` cycle and records pending READY, sync-wake, and periodic-release intervals
for the selected task against that pre-return endpoint. Critical monitoring
records a start at nesting depth zero and an end when depth returns to zero,
immediately before the old BASEPRI mask is restored.

This revised design distinguishes “raw sample not retained” from “observation lost.”
It also explains why the original distributions were not simply recomputed after
the revision: the other metrics were captured on the accepted **original** timing
firmware and were not rerun on the corrected aggregate-statistics firmware.

### 11.4 Hardware-verified results

The original physical timing firmware ran six participants, 16 equal-priority
yields, a mutex handoff, a queue wake, eight 6 ms periodic jobs, and one deliberate
deadline miss. It passed 80/80 assertions with zero failures and reproduced after
debugger reset. At the verified 16 MHz clock:

| Metric | Samples | Mean | Observed maximum |
|---|---:|---:|---:|
| Instrumented PendSV software interval | 42 | **446.95 cycles / 27.93 µs** | 491 cycles / 30.69 µs |
| READY-to-dispatch | 28 | **1,274.54 cycles / 79.66 µs** | 3,515 cycles / 219.69 µs |
| Periodic release-to-dispatch | 8 | **885.25 cycles / 55.33 µs** | 886 cycles / 55.38 µs |
| Synchronization wake-to-dispatch | 2 | **2,300 cycles / 143.75 µs** | 3,702 cycles / 231.38 µs |

Two synchronization samples are far too few to characterize a distribution. The
original firmware also measured 9 cycles (0.5625 µs) between back-to-back DWT reads.
That is timestamp access overhead only—not the cost of hooks, state updates,
scheduler calls, or all instrumentation.

The corrected revised firmware's critical-section monitor produced:

| Observations | Raw retained / not retained | Total | Minimum | Mean | Observed maximum |
|---:|---:|---:|---:|---:|---:|
| 104 | 64 / 40 | 23,161 cycles | 80 cycles / 5.00 µs | **222.70 cycles / 13.92 µs** | 916 cycles / 57.25 µs |

All 104 observations contributed to total/min/max/mean and no saturation flag was
set. The 64/40 split describes raw retention only. The corrected image again passed
80/80, zero failures, and reproduced identically after debugger reset.

An observed maximum is not WCET. A WCET claim would require a defensible path model,
all relevant cache/flash/bus/interrupt states, bounded interference, and analytical
or exhaustive evidence. These are observed distributions from one firmware,
workload, clock, compiler, and debug procedure, with instrumentation included.

### 11.5 Why FreeRTOS and Zephyr cannot be ranked from these numbers

The README cites RTOSBench means of 223 cycles for FreeRTOS 10.4.4 and 524 cycles
for Zephyr 2.6.0 on a different STM32F429ZIT6, averaged over 1,000 cooperative
yields. RTOSBench measures from yield until next-task execution. This kernel's
446.95-cycle PendSV number starts after hardware exception entry and stops before
exception return and the first resumed instruction. MCU, clock/configuration,
compiler, enabled features, workload, sample count, and instrumentation differ.
The figures are useful boundary examples, not evidence of a performance ranking.

Host [timing tests](tests/timing_test.c) validate DWT wrap arithmetic, conversion,
hook behavior, raw saturation, aggregate continuation, arithmetic saturation, and
reset. Physical M10 supplies the scoped cycle evidence above. Neither establishes
formal WCET.

**Takeaway:** a timing number is meaningful only together with its start, end,
firmware configuration, sample population, and excluded work.

---

## 12. Proposed M11: a minimal flight-like workload

**M11 is not implemented.** The files under [`flight/`](flight/) contain only
comment/TODO scaffolds for IMU, estimator, controller, mixer, safety, health, and
telemetry roles. The following is an educational proposal for exercising the RTOS,
not a claim that this repository contains a flight controller.

### 12.1 What useful evidence would look like

A credible small workload should stress the existing mechanisms with declared
inputs and costs while remaining reproducible. A synthetic or recorded sensor trace
is better than pretending incomplete drivers are a vehicle. It can show that the
kernel schedules a pipeline with bounded resources and visible overload; it cannot
establish control-law quality, airworthiness, or product safety.

One possible seven-task configuration, including idle, is:

| Task | Priority | Period / deadline | Proposed responsibility | Communication |
|---|---:|---:|---|---|
| Safety | 1 | event-driven / 1 ms response target | Consume fault events and select safe state | Small fault queue; no long blocking |
| Sensor ingest | 2 | 1 ms / 1 ms | Read the next replay sample, validate sequence/timestamp | Send fixed integer sample to estimator queue |
| Control | 3 | 4 ms / 4 ms | Consume latest valid estimate and compute bounded integer command | Estimate queue; short output mutex or single-owner queue |
| Estimator | 4 | 2 ms / 2 ms | Update a fixed-point state estimate | Sensor receive, estimate publish |
| Health | 6 | 10 ms / 10 ms | Check progress counters, misses, overruns, and stack margins | Snapshot shared counters under short mutex |
| Telemetry | 12 | 20 ms / 20 ms | Serialize a bounded status record | Nonblocking queue; explicit drop counter |
| Idle | 31 | none | `WFI` | none |

The exact priorities above are illustrative and require response-time analysis. For
example, placing Control above Estimator is defensible only if Control intentionally
uses the most recently completed estimate; if every control job requires the same
phase's estimator output, precedence and release offsets should be redesigned.
This is why a task table is an input to analysis, not proof by itself.

### 12.2 A concrete proposed execution

Use a fixed replay table of integer samples and fixed-point arithmetic only:

```text
t=0 ms  Sensor releases, copies sample 0 to sensor queue
        Estimator wakes, performs a bounded fixed-point update, publishes estimate 0
        Control consumes the latest complete estimate and publishes command 0

t=1 ms  Sensor releases sample 1; Control is not due
t=2 ms  Sensor sample 2 and Estimator's next phase execute by priority
t=4 ms  Sensor, Estimator, and Control are all due
        ready priorities order Sensor -> Control -> Estimator unless phase offsets
        or blocking dependencies deliberately enforce another pipeline order

t=10 ms Health snapshots progress/deadline/stack data
t=20 ms Telemetry formats one bounded record; if output is busy it drops/coalesces
        telemetry rather than delaying flight-critical work
```

This example deliberately exposes a design question: fixed priority alone does not
encode data-flow precedence. Release offsets, queue blocking, or priority assignment
must make the desired sample-to-control relationship explicit.

Queues should carry compact, fixed-size messages such as an integer timestamp,
three signed axes, sequence number, and validity flags. Copying a large floating
structure under BASEPRI would increase masked latency. A shared configuration or
coherent health snapshot may use a PI mutex, but the high-priority path should not
hold it across computation or queue operations.

Because current task contexts are integer-only, all workload math should use scaled
integers/fixed-point with documented units, overflow checks, and saturation. Adding
ordinary `float` code without FPU context support would violate the kernel contract.

### 12.3 Resource and overload budgets

Each task needs a caller-owned stack budget justified by compiler `.su` reports and
on-target high-water measurements with margin. The seven-task proposal fits the
eight-TCB limit, leaving only one spare slot. Queue capacity times element size,
seven TCBs at 264 bytes each, task stacks, monitor arrays, and BSP buffers must be
summed against the link map—not estimated from source line count.

Computation should be reproducibly adjustable, for example by a fixed-count integer
filter loop whose result is checked so the compiler cannot remove it. Workload
levels might target 40%, 70%, and deliberate overload. Under overload the expected
policy is explicit:

- one active periodic job per task;
- skipped releases and overruns increase rather than creating an unbounded backlog;
- telemetry drops are counted;
- safety/health transitions are deterministic;
- no queue count, list, stack guard, or ownership invariant fails.

Useful evidence would include the exact replay/configuration hash, ELF/map and tool
versions, per-task release/start/completion/miss/overrun counters, queue high-water
and drops, mutex blocking/donation events, stack high-water, CPU idle fraction, DWT
response distributions, and repeated reset/cold-boot traces. A sensor-to-command
end-to-end metric needs timestamps at replay ingestion and command publication; it
cannot be inferred from PendSV time.

Such a workload would be credible evidence of flight-control-like scheduling and
resource interaction. It would still be a synthetic RTOS workload, not a complete
controller, validated estimator, device driver stack, or certified flight system.

---

## 13. Proposed M12: hardware stress validation

**M12 is also not implemented.** Stress testing should be designed around failure
modes, not merely “run it for a long time.” The plan below separates the injected
condition, the invariant/metric, and acceptable evidence.

| Scenario | Failure mode it is intended to expose | Observe | Evidence of success |
|---|---|---|---|
| Sustained CPU load at increasing utilization | Starvation, stack growth, deadline collapse, incorrect recovery | Idle time, per-task misses/overruns, guards/high-water, completion progress | Repeated runs preserve invariants; declared load threshold meets deadlines; overload produces only specified counters/policy |
| High-frequency periodic releases | Tick scan/release races, phase drift, duplicate releases | Scheduled/actual phases, release counts, jitter, `next_release`, PendSV count | Exact phase spacing across long runs; no duplicate job/backlog; bounded observed jitter under stated load |
| Priority/preemption churn | Lost context, R4–R11 corruption, ready-list duplication | Register canaries, switch count, bitmap/list invariants, PSP bounds | Millions of switches or a declared duration with no corruption/fault and repeatable trace summaries |
| Queue saturation and contention | Lost wakeups, under/overflow, FIFO/priority violation | Sent/received sequence IDs, occupancy, wait counts, drop/full results | Conservation equation holds; every accepted item received exactly once in policy order; explicit overload only |
| Mutex contention/inversion | Stale donation, premature restoration, bad handoff | Owner, base/effective priorities, wait order, blocking intervals | High waits only for declared critical/resource chain; all priorities and ownership restore; no cycle blocks forever |
| Valid interrupt nesting with BASEPRI | Kernel-aware ISR races or urgent ISR starvation | IRQ entry/order counters, BASEPRI samples, pending/tail-chain behavior | Urgent non-kernel ISR remains live without kernel calls; kernel-aware IRQ defers until restore; state invariants hold |
| Tick wraparound | Signed-order boundary errors, phase discontinuity | Releases/deadlines before and after `UINT32_MAX` | Same spacing and strict deadline semantics across multiple forced wrap scenarios |
| Deliberate deadline violations | Double-counted misses, forced termination, bad recovery | Miss flag/count, active job, skipped releases, next successful job | One miss per active late job; no forced kill; next unoccupied phase recovers correctly |
| Long-running execution | Counter saturation, rare races, memory corruption | Saturation flags, heartbeat/progress tokens, guards, fault code | Declared hours and multiple cold boots/reset reruns with exact binary/config and no unexplained stop or invariant failure |
| Reset repeatability | Hidden `.noinit`/debugger/timing dependence | Completion markers, traces, clock/DWT initialization | Identical acceptance from debugger reset and documented cold power cycles |

### 13.1 How to structure the campaign

Each test image should expose a stable completion/failure marker, immutable workload
manifest, bounded trace, exact assertion count, and summary counters readable through
GDB. Raw high-rate logging inside ISR or masked regions would perturb the system and
violate kernel rules; collect fixed records in RAM and inspect them afterward.

Load should be swept rather than set to one value. For example, run 50%, 75%, 90%,
and deliberate >100% periodic demand while keeping the same priorities and replay.
The expected result changes at overload: meeting all deadlines is no longer the
criterion, but correct miss/skip/drop accounting and continued safety-task progress
are.

Interrupt nesting tests must use legal priorities. An urgent IRQ above `0x40` may
measure a GPIO timestamp or increment independent storage, but must not call kernel
APIs. Kernel-aware test IRQs belong at `0x40` or less urgent and should demonstrate
that BASEPRI defers them. Testing an illegal call from an urgent IRQ without a
runtime priority check may simply corrupt the kernel; it is misuse injection, not a
supported scenario.

### 13.2 Four different kinds of evidence

These categories should not be collapsed:

1. **Correctness testing** asks whether state transitions and outputs satisfy the
   specification for chosen cases.
2. **Performance characterization** records distributions and interference under a
   named firmware/workload.
3. **Stress testing** searches for rare failures and behavior near/beyond capacity.
4. **Formal WCET analysis** attempts a defensible upper bound over every permitted
   path and hardware state.

A 24-hour fault-free run is useful stress evidence but not WCET proof. An observed
916-cycle critical-section maximum is characterization, not a guaranteed ceiling.
Likewise, static compilation proves neither runtime correctness nor timing.

### 13.3 Optional architectural extensions

Several natural extensions are possible, but each changes more than an API:

**FPU context support.** Choose eager or conditional FP save. Conditional support
must inspect FP context state/EXC_RETURN, deliberately configure lazy stacking, save
S16–S31 when required, enlarge stack budgets, update assembly and frame tests, and
measure the active/inactive FP switch costs.

**Tickless scheduling.** Replace the periodic SysTick cadence with a one-shot next
event. The timer layer must find the earliest release/timeout, safely reprogram the
counter around races, account elapsed time, preserve wrap semantics, and redefine
jitter/idle measurements. It reduces idle overhead but substantially complicates
timekeeping.

**Timeouts and cancellation.** Queue/mutex waits currently last indefinitely.
Timeouts need an ordered or otherwise bounded timer structure plus a race protocol:
a signal and timeout can occur together, but the waiter must be removed and made
READY exactly once. PI donation must be recomputed when a mutex waiter times out.

**MPU protection.** Per-task memory regions could turn stack/data corruption into a
fault near its source. This requires region budgeting, alignment constraints,
privilege policy, context-switch MPU programming, fault-frame diagnostics, and an
API model for shared buffers. Guard words alone are much simpler but only detect a
subset of corruption after it occurs.

---

## 14. End-to-end learning synthesis

### 14.1 One complete execution story

1. Reset hardware loads MSP and `Reset_Handler` from the Flash vector table.
2. Startup sets VTOR, copies `.data`, clears `.bss`, and calls `main()`.
3. `main()` selects 16 MHz HSI, initializes static kernel state, constructs task
   stacks/TCBs, registers tasks, configures exception priorities and 1 kHz SysTick,
   and starts the scheduler.
4. The scheduler chooses the highest-priority eligible task. SVC restores its
   synthetic R4–R11 and hardware frame and exception-returns to Thread/PSP.
5. SysTick advances absolute time, accounts execution, observes deadlines, releases
   due periodic jobs, and makes a higher-priority release READY when appropriate.
6. Queue and mutex operations atomically block tasks that cannot proceed. A send,
   receive, or unlock removes a waiter before making it READY. Mutex contention may
   propagate effective priority through owner chains.
7. A higher-priority READY task causes PendSV to run after more urgent handlers and
   after BASEPRI masking ends. Hardware saves volatile state on the outgoing PSP;
   PendSV saves R4–R11, calls the scheduler on MSP, restores the selected PSP and
   R4–R11, then uses `0xFFFFFFFD` to restore the hardware frame.
8. Periodic tasks complete explicitly with `aerortos_periodic_wait_next()`. A late
   job is counted but not killed; an occupied next phase is skipped, not queued.
9. With timing enabled, DWT hooks record specifically bounded intervals into fixed
   raw arrays and continuing aggregates. Those measurements describe the named
   firmware and endpoints, not universal latency or WCET.

### 14.2 The most important tradeoffs

| Choice | What it gains | What it costs |
|---|---|---|
| Fixed priorities + bitmap | Compact O(1) highest-ready selection | Priority analysis and possible starvation |
| Static/caller-owned storage | Explicit lifetime and RAM bounds; no fragmentation | Fixed capacities and possible reserved-but-unused memory |
| BASEPRI ceiling | Urgent independent IRQs remain live | Strict ISR-priority/API contract and subtle encoding |
| SVC first launch + low-priority PendSV | Clean PSP/MSP separation and deferred switching | Architecture-specific assembly/ABI obligations |
| 1 kHz tick | Simple release/deadline model | 1 ms granularity and periodic interrupt overhead |
| One active periodic job | Bounded overload behavior | Work is skipped rather than backlogged |
| Queue copies under mask | Simple atomic fixed-message semantics | O(element-size) masked latency |
| Priority inheritance | Mitigates classic and transitive inversion | Reordering/propagation complexity; no automatic WCET proof |
| Integer-only context | Small fixed switch frame | No supported FP task code |
| Bounded instrumentation | No heap/logging in hot paths | Raw samples can be unretained; hooks perturb timing |

### 14.3 Common misconceptions corrected

**“SVC and PendSV do the same thing.”** SVC launches the first selected task from
`main()`; PendSV saves an outgoing task and switches between established contexts.

**“Tasks and handlers both use PSP.”** Tasks use caller-owned PSP stacks. Handlers
execute on MSP. Exception hardware may stack the interrupted task's frame on PSP,
but handler code itself uses MSP.

**“The scheduler scans 32 priorities.”** The highest READY priority is selected by
`CLZ` on a 32-bit bitmap. Only operations explicitly described as scans—periodic
tasks, waiters, owned mutexes—perform bounded traversal.

**“Every scheduling request switches tasks.”** The current task continues when no
READY task strictly outranks it, except when it voluntarily yields or blocks.

**“A deadline miss stops the task.”** Deadline tracking records one miss strictly
after the absolute deadline. The active job continues until it completes; later
occupied phases are skipped under the one-job policy.

**“Waking a queue receiver gives it the message.”** Wakeup only makes the receiver
eligible. It rechecks the queue after scheduling because another task may have won
the item.

**“Priority inheritance proves bounded blocking.”** PI removes a class of unrelated
medium-priority interference. A numerical blocking bound still requires critical
section and resource-chain analysis.

**“446.95 cycles is full context-switch latency.”** It is the instrumented PendSV
software interval from handler-entry read to the pre-`EXC_RETURN` hook. Hardware
exception entry/return and the first resumed instruction are outside the boundary.

**“Forty dropped samples means forty observations vanished.”** In the revised
monitor, 40 raw values were not retained, but all 104 observations contributed to
aggregate count, total, minimum, maximum, and mean.

---

## 15. Comprehension questions and answers

### 1. Why must `aerortos_current_task` continue to point to Task A after Task B is made READY?

Because PendSV has not yet stored A's software frame and PSP. The handler needs the
outgoing TCB pointer to write `A.saved_psp`. Only after that save is complete may the
scheduler replace current with B.

### 2. If priorities 2, 5, and 10 are READY, which bitmap bits are set and what does `CLZ` return?

Bits 29, 26, and 21 are set by `1 << (31 - priority)`. The leading set bit is bit
29, so `CLZ` returns 2, selecting priority 2.

### 3. Why are R4–R11 below the hardware frame in memory?

Exception entry first pushes the hardware frame while the stack grows downward.
PendSV then uses `STMDB` to push R4–R11 at still lower addresses. `saved_psp` points
to R4, allowing software restore first and hardware exception return second.

### 4. An outer critical section saved BASEPRI 0 and an inner one saved `0x40`. What should each exit restore?

The inner exit restores `0x40`, keeping the outer section protected. The outer exit
restores 0. Unconditionally clearing BASEPRI in the inner exit would break nesting.

### 5. A period-4 task released at tick 0 has deadline 2 and is still active at ticks 2, 3, and 4. What happens?

Tick 2 is exactly the deadline, so no miss yet. Tick 3 is strictly later, so one miss
is recorded. At tick 4 the next phase arrives while the old job is active, so one
overrun and one skipped release are recorded; no second job is queued.

### 6. Why does a blocking queue receive use a loop after wakeup?

Wakeup does not reserve an element. Between wake and execution another receiver may
consume it. Rechecking under BASEPRI prevents underflow and makes the operation
correct under competition.

### 7. Low owns two mutexes. After releasing the mutex with a priority-2 waiter, why might Low restore only to priority 5 rather than base 10?

The other owned mutex may still have a priority-5 head waiter. Effective priority is
recomputed from every owned mutex, so the remaining donation persists.

### 8. Why is an urgent ISR at encoded priority `0x20` forbidden from calling queue try APIs?

BASEPRI `0x40` does not mask that ISR. It could interrupt a kernel state transition
and manipulate the same lists/ring concurrently, violating atomicity. Only
kernel-aware IRQs at `0x40` or numerically larger priorities may use those APIs.

### 9. Can the 916-cycle observed critical-section maximum be used as WCET?

No. It is a maximum observed in one instrumented workload and configuration. It
does not cover every path, interrupt pattern, bus/Flash state, build, or input and
therefore is not a proven upper bound.

### 10. What evidence would be required before enabling `float` in tasks?

A declared FP ABI and lazy/eager stacking policy, correct EXC_RETURN/FPCA handling,
S16–S31 save/restore where required, revised stack budgets and frame ABI, tests for
FP-active and FP-inactive preemption, and measured timing costs. The presence of an
M4F core alone is insufficient.

---

## Source and evidence map

The principal implementation references used by this guide are the
[Makefile](Makefile), [linker script](linker/stm32f446re.ld),
[startup assembly](startup/startup_stm32f446re.S),
[task/frame code](kernel/task.c), [scheduler](kernel/scheduler.c),
[critical sections](arch/cortex_m4/critical_section.c),
[SVC/PendSV assembly](arch/cortex_m4/context_switch.S),
[timer/deadline code](kernel/timer.c), [queue](kernel/queue.c),
[mutex](kernel/mutex.c), [DWT counter](arch/cortex_m4/cycle_counter.c), and
[monitor](kernel/monitor.c). Public contracts live under
[`include/aerortos/`](include/aerortos/).

The broad portable regression is represented by
[task](tests/task_test.c), [scheduler](tests/scheduler_test.c),
[context handoff](tests/context_switch_test.c),
[deadline](tests/deadline_test.c), [queue](tests/queue_test.c),
[mutex](tests/mutex_test.c), [integration](tests/integration_test.c), and
[timing](tests/timing_test.c) tests. Physical acceptance scenarios are the isolated
[M5](tests/target/m5_context_switch_test.c),
[M6](tests/target/m6_periodic_test.c), [M7](tests/target/m7_queue_test.c),
[M8](tests/target/m8_mutex_test.c), [M9](tests/target/m9_integration_test.c), and
[M10](tests/target/m10_timing_test.c) images. Their recorded results, rather than
cross-build alone, are the basis for the hardware claims in this guide.
