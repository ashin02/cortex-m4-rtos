#include <stdbool.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/critical_section.h"
#include "aerortos/cortex_m4.h"
#include "aerortos/platform.h"
#include "aerortos/port.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"

#define M5_TEST_STACK_WORDS 128U
#define M5_TEST_TRACE_CAPACITY 64U
#define M5_TEST_COMPLETE_MAGIC UINT32_C(0x4D355041)

volatile uint32_t aerortos_m5_hw_pass_count;
volatile uint32_t aerortos_m5_hw_fail_count;
volatile uint32_t aerortos_m5_hw_switch_count;
volatile uint32_t aerortos_m5_hw_preemption_count;
volatile uint32_t aerortos_m5_hw_idle_count;
volatile uint32_t aerortos_m5_hw_irq_count;
volatile uint32_t aerortos_m5_hw_outer_basepri;
volatile uint32_t aerortos_m5_hw_inner_basepri;
volatile uint32_t aerortos_m5_hw_masked_basepri;
volatile uint32_t aerortos_m5_hw_complete;
volatile uint32_t aerortos_m5_hw_trace_count;
volatile uint32_t aerortos_m5_hw_trace[M5_TEST_TRACE_CAPACITY];

static aerortos_task_t *idle_task;
static aerortos_task_t *high_task;
static aerortos_task_t *task_a;
static aerortos_task_t *task_b;
static volatile uint32_t task_a_count;
static volatile uint32_t task_b_count;
static volatile uint32_t test_phase;
extern uint32_t _estack;
extern uint32_t _stack_limit;

alignas(8) static uint32_t idle_stack[M5_TEST_STACK_WORDS];
alignas(8) static uint32_t high_stack[M5_TEST_STACK_WORDS];
alignas(8) static uint32_t task_a_stack[M5_TEST_STACK_WORDS];
alignas(8) static uint32_t task_b_stack[M5_TEST_STACK_WORDS];

int main(void);
bool aerortos_m5_register_probe(void);
void aerortos_m5_hw_test_complete_marker(void);
static void check(bool condition);

static uintptr_t read_psp(void)
{
    uintptr_t value;

    __asm volatile ("mrs %0, psp" : "=r" (value));
    return value;
}

static uintptr_t read_msp(void)
{
    uintptr_t value;

    __asm volatile ("mrs %0, msp" : "=r" (value));
    return value;
}

static uint32_t read_control(void)
{
    uint32_t value;

    __asm volatile ("mrs %0, control" : "=r" (value));
    return value;
}

static uint32_t read_basepri(void)
{
    uint32_t value;

    __asm volatile ("mrs %0, basepri" : "=r" (value));
    return value;
}

void aerortos_m5_hw_test_complete_marker(void)
{
    __asm volatile ("" ::: "memory");
}

void SysTick_Handler(void)
{
    const uintptr_t msp = read_msp();

    ++aerortos_m5_hw_irq_count;
    check((msp >= (uintptr_t)&_stack_limit) && (msp < (uintptr_t)&_estack) &&
          ((msp & 7U) == 0U));
    check(aerortos_scheduler_make_ready(high_task) == AERORTOS_SCHEDULER_OK);
}

static void trace(uint32_t value)
{
    const uint32_t index = aerortos_m5_hw_trace_count;

    if (index < M5_TEST_TRACE_CAPACITY) {
        aerortos_m5_hw_trace[index] = value;
        aerortos_m5_hw_trace_count = index + 1U;
    }
}

static void check(bool condition)
{
    if (condition) {
        ++aerortos_m5_hw_pass_count;
    } else {
        ++aerortos_m5_hw_fail_count;
    }
}

static void high_entry(void *argument)
{
    (void)argument;
    trace(UINT32_C(0x10000001));
    check(aerortos_scheduler_current() == high_task);
    check(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);

    trace(UINT32_C(0x10000002));
    ++aerortos_m5_hw_preemption_count;
    check(aerortos_scheduler_current() == high_task);
    check(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);
    for (;;) {
    }
}

static void task_a_entry(void *argument)
{
    (void)argument;
    trace(UINT32_C(0xA0000001));
    check(aerortos_m5_register_probe());

    for (;;) {
        ++task_a_count;
        ++aerortos_m5_hw_switch_count;
        if (task_a_count == 16U) {
            const aerortos_critical_state_t outer = aerortos_critical_enter();
            const aerortos_critical_state_t inner = aerortos_critical_enter();

            aerortos_m5_hw_outer_basepri = outer;
            aerortos_m5_hw_inner_basepri = inner;
            aerortos_m5_hw_masked_basepri = read_basepri();
            check(outer == 0U);
            check(inner == AERORTOS_KERNEL_BASEPRI);
            check(aerortos_m5_hw_masked_basepri == AERORTOS_KERNEL_BASEPRI);
            AERORTOS_SCB->ICSR = AERORTOS_SCB_ICSR_PENDSTSET_Msk;
            check(aerortos_m5_hw_irq_count == 0U);
            check(aerortos_scheduler_current() == task_a);
            aerortos_critical_exit(inner);
            check(aerortos_m5_hw_irq_count == 0U);
            check(aerortos_scheduler_current() == task_a);
            aerortos_critical_exit(outer);
            check(aerortos_m5_hw_irq_count == 1U);
            check(aerortos_m5_hw_preemption_count == 1U);
        }
        if (task_a_count == 64U) {
            test_phase = 1U;
            check(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);

            trace(UINT32_C(0xA0000002));
            check(aerortos_m5_hw_idle_count == 1U);
            check(task_a_count >= 64U);
            check(task_b_count >= 64U);
            check(aerortos_task_stack_guard_intact(task_a));
            check(aerortos_task_stack_guard_intact(task_b));
            check(aerortos_scheduler_invariants_hold());
            check((read_control() & UINT32_C(0x2)) != 0U);
            check((read_psp() >= (uintptr_t)task_a->stack_low) &&
                  (read_psp() < (uintptr_t)task_a->stack_high) &&
                  ((read_psp() & 7U) == 0U));
            check((((uintptr_t)task_a->saved_psp) & 7U) == 0U);
            check((task_a->saved_psp >= task_a->stack_low) &&
                  (task_a->saved_psp < task_a->stack_high));
            aerortos_m5_hw_complete = M5_TEST_COMPLETE_MAGIC;
            aerortos_m5_hw_test_complete_marker();
        }
        check(aerortos_scheduler_yield() == AERORTOS_SCHEDULER_OK);
    }
}

static void task_b_entry(void *argument)
{
    (void)argument;
    trace(UINT32_C(0xB0000001));
    for (;;) {
        ++task_b_count;
        ++aerortos_m5_hw_switch_count;
        if ((test_phase == 1U) && (task_b_count >= 64U)) {
            test_phase = 2U;
            check(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);
        }
        check(aerortos_scheduler_yield() == AERORTOS_SCHEDULER_OK);
    }
}

static void idle_entry(void *argument)
{
    (void)argument;
    for (;;) {
        if (test_phase == 2U) {
            test_phase = 3U;
            ++aerortos_m5_hw_idle_count;
            trace(UINT32_C(0x1D1E0001));
            check(aerortos_scheduler_make_ready(task_a) ==
                  AERORTOS_SCHEDULER_OK);
        }
        aerortos_wait_for_interrupt();
    }
}

static aerortos_task_t *create_task(
    aerortos_task_entry_t entry,
    uint32_t *stack,
    uint8_t priority)
{
    aerortos_task_config_t config;
    aerortos_task_t *task = NULL;

    config.entry = entry;
    config.argument = NULL;
    config.stack = stack;
    config.stack_size_bytes = M5_TEST_STACK_WORDS * sizeof(uint32_t);
    config.priority = priority;
    config.period_ticks = 0U;
    config.relative_deadline_ticks = 0U;
    config.initial_release_delay_ticks = 0U;

    if (aerortos_task_create(&config, &task) != AERORTOS_TASK_OK) {
        ++aerortos_m5_hw_fail_count;
        platform_panic(UINT32_C(0x4D350001));
    }
    return task;
}

int main(void)
{
    aerortos_task_system_init();
    idle_task = create_task(idle_entry, idle_stack, AERORTOS_IDLE_PRIORITY);
    high_task = create_task(high_entry, high_stack, 2U);
    task_a = create_task(task_a_entry, task_a_stack, 8U);
    task_b = create_task(task_b_entry, task_b_stack, 8U);
    aerortos_scheduler_init(idle_task);
    check(aerortos_scheduler_add(task_a) == AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(task_b) == AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(high_task) == AERORTOS_SCHEDULER_OK);
    aerortos_port_init();
    AERORTOS_SCB_SHPR3 = (AERORTOS_SCB_SHPR3 & UINT32_C(0x00FFFFFF)) |
                         (AERORTOS_KERNEL_BASEPRI << 24);
    aerortos_scheduler_start();
}
