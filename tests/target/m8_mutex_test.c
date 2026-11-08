#include <stdbool.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/cortex_m4.h"
#include "aerortos/critical_section.h"
#include "aerortos/mutex.h"
#include "aerortos/platform.h"
#include "aerortos/port.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"

#define M8_STACK_WORDS 192U
#define M8_COMPLETE_MAGIC UINT32_C(0x4D385041)
#define M8_EXPECTED_PASS_COUNT 43U
#define M8_EXPECTED_TRACE_COUNT 9U
#define M8_TRACE_CAPACITY 16U

volatile uint32_t aerortos_m8_hw_pass_count;
volatile uint32_t aerortos_m8_hw_fail_count;
volatile uint32_t aerortos_m8_hw_complete;
volatile uint32_t aerortos_m8_hw_expected_pass_count =
    M8_EXPECTED_PASS_COUNT;
volatile uint32_t aerortos_m8_hw_boost_count;
volatile uint32_t aerortos_m8_hw_restoration_count;
volatile uint32_t aerortos_m8_hw_block_count;
volatile uint32_t aerortos_m8_hw_preemption_count;
volatile uint32_t aerortos_m8_hw_trace_count;
volatile uint32_t aerortos_m8_hw_trace[M8_TRACE_CAPACITY];

aerortos_mutex_t aerortos_m8_mutex;
aerortos_task_t *aerortos_m8_idle_task;
aerortos_task_t *aerortos_m8_high_task;
aerortos_task_t *aerortos_m8_medium_task;
aerortos_task_t *aerortos_m8_low_task;

alignas(8) static uint32_t idle_stack[M8_STACK_WORDS];
alignas(8) static uint32_t high_stack[M8_STACK_WORDS];
alignas(8) static uint32_t medium_stack[M8_STACK_WORDS];
alignas(8) static uint32_t low_stack[M8_STACK_WORDS];

static volatile uint32_t high_complete;
static volatile uint32_t medium_complete;
static volatile uint32_t low_complete;
static volatile uint32_t low_unlock_in_progress;
static volatile uint32_t medium_run_count;

int main(void);
void SysTick_Handler(void);
void aerortos_m8_hw_test_complete_marker(void);

static void check(bool condition)
{
    if (condition) {
        ++aerortos_m8_hw_pass_count;
    } else {
        ++aerortos_m8_hw_fail_count;
    }
}

static void trace(uint32_t value)
{
    const uint32_t index = aerortos_m8_hw_trace_count;

    if (index < M8_TRACE_CAPACITY) {
        aerortos_m8_hw_trace[index] = value;
        aerortos_m8_hw_trace_count = index + 1U;
    }
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

static void sync_block_or_panic(uint32_t code)
{
    if (aerortos_scheduler_block_current() != AERORTOS_SCHEDULER_OK) {
        ++aerortos_m8_hw_fail_count;
        platform_panic(code);
    }
}

static void sync_wake_or_panic(aerortos_task_t *task, uint32_t code)
{
    if (aerortos_scheduler_make_ready(task) != AERORTOS_SCHEDULER_OK) {
        ++aerortos_m8_hw_fail_count;
        platform_panic(code);
    }
}

void SysTick_Handler(void)
{
    __asm volatile ("" ::: "memory");
}

void aerortos_m8_hw_test_complete_marker(void)
{
    __asm volatile ("" ::: "memory");
}

static void idle_entry(void *argument)
{
    (void)argument;
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

static void high_entry(void *argument)
{
    (void)argument;
    trace(UINT32_C(0xA8000001));
    check(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);

    trace(UINT32_C(0xA8000002));
    check(aerortos_m8_mutex.owner == aerortos_m8_low_task);
    ++aerortos_m8_hw_block_count;
    check(aerortos_mutex_lock(&aerortos_m8_mutex) == AERORTOS_MUTEX_OK);
    trace(UINT32_C(0xA8000003));
    check(low_unlock_in_progress != 0U);
    ++aerortos_m8_hw_preemption_count;
    check(aerortos_m8_mutex.owner == aerortos_m8_high_task);
    check(aerortos_m8_high_task->effective_priority ==
          aerortos_m8_high_task->base_priority);
    check(aerortos_mutex_unlock(&aerortos_m8_mutex) == AERORTOS_MUTEX_OK);
    high_complete = 1U;
    sync_block_or_panic(UINT32_C(0x4D380004));

    trace(UINT32_C(0xA8000004));
    check(high_complete == 1U);
    check(medium_complete == 1U);
    check(low_complete == 1U);
    check(aerortos_m8_hw_boost_count == 1U);
    check(aerortos_m8_hw_restoration_count == 1U);
    check(aerortos_m8_hw_block_count == 1U);
    check(aerortos_m8_hw_preemption_count == 1U);
    check(aerortos_m8_mutex.owner == NULL);
    check(aerortos_mutex_invariants_hold(&aerortos_m8_mutex));
    check(aerortos_scheduler_invariants_hold());
    check(aerortos_task_stack_guard_intact(aerortos_m8_low_task));
    check(aerortos_task_stack_guard_intact(aerortos_m8_medium_task));
    check(aerortos_task_stack_guard_intact(aerortos_m8_high_task));
    check((read_control() & UINT32_C(0x2)) != 0U);
    check(read_basepri() == 0U);

    if ((aerortos_m8_hw_pass_count == M8_EXPECTED_PASS_COUNT) &&
        (aerortos_m8_hw_fail_count == 0U) &&
        (aerortos_m8_hw_trace_count == M8_EXPECTED_TRACE_COUNT) &&
        (high_complete != 0U) && (medium_complete != 0U) &&
        (low_complete != 0U)) {
        aerortos_m8_hw_complete = M8_COMPLETE_MAGIC;
    }
    aerortos_m8_hw_test_complete_marker();
    sync_block_or_panic(UINT32_C(0x4D380005));
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

static void medium_entry(void *argument)
{
    (void)argument;
    trace(UINT32_C(0xB8000001));
    check(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);

    trace(UINT32_C(0xB8000002));
    ++medium_run_count;
    check(high_complete == 1U);
    check(aerortos_m8_low_task->effective_priority ==
          aerortos_m8_low_task->base_priority);
    check(aerortos_m8_mutex.owner == NULL);
    medium_complete = 1U;
    sync_block_or_panic(UINT32_C(0x4D380006));
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

static void low_entry(void *argument)
{
    aerortos_critical_state_t critical_state;

    (void)argument;
    check(aerortos_mutex_lock(&aerortos_m8_mutex) == AERORTOS_MUTEX_OK);
    check(aerortos_m8_mutex.owner == aerortos_m8_low_task);
    trace(UINT32_C(0xC8000001));

    critical_state = aerortos_critical_enter();
    check(aerortos_scheduler_make_ready(aerortos_m8_medium_task) ==
          AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_make_ready(aerortos_m8_high_task) ==
          AERORTOS_SCHEDULER_OK);
    aerortos_critical_exit(critical_state);

    trace(UINT32_C(0xC8000002));
    check(medium_run_count == 0U);
    check(aerortos_m8_low_task->effective_priority ==
          aerortos_m8_high_task->base_priority);
    ++aerortos_m8_hw_boost_count;
    check(aerortos_m8_mutex.wait_head == aerortos_m8_high_task);
    check(aerortos_mutex_invariants_hold(&aerortos_m8_mutex));
    low_unlock_in_progress = 1U;
    check(aerortos_mutex_unlock(&aerortos_m8_mutex) == AERORTOS_MUTEX_OK);
    low_unlock_in_progress = 0U;

    trace(UINT32_C(0xC8000003));
    check(aerortos_m8_low_task->effective_priority ==
          aerortos_m8_low_task->base_priority);
    check(aerortos_m8_low_task->owned_mutex_count == 0U);
    check(aerortos_m8_mutex.owner == NULL);
    check(aerortos_scheduler_invariants_hold());
    ++aerortos_m8_hw_restoration_count;
    low_complete = 1U;
    sync_wake_or_panic(aerortos_m8_high_task, UINT32_C(0x4D380007));
    for (;;) {
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
    config.stack_size_bytes = M8_STACK_WORDS * sizeof(uint32_t);
    config.priority = priority;
    config.period_ticks = 0U;
    config.relative_deadline_ticks = 0U;
    config.initial_release_delay_ticks = 0U;
    if (aerortos_task_create(&config, &task) != AERORTOS_TASK_OK) {
        ++aerortos_m8_hw_fail_count;
        platform_panic(UINT32_C(0x4D380001));
    }
    return task;
}

int main(void)
{
    if (!system_clock_init()) {
        platform_panic(UINT32_C(0x4D380002));
    }
    aerortos_task_system_init();
    aerortos_time_system_init(0U);
    check(aerortos_mutex_init(&aerortos_m8_mutex) == AERORTOS_MUTEX_OK);
    aerortos_m8_idle_task = create_task(
        idle_entry, idle_stack, AERORTOS_IDLE_PRIORITY);
    aerortos_m8_high_task = create_task(high_entry, high_stack, 2U);
    aerortos_m8_medium_task = create_task(medium_entry, medium_stack, 5U);
    aerortos_m8_low_task = create_task(low_entry, low_stack, 10U);
    aerortos_scheduler_init(aerortos_m8_idle_task);
    check(aerortos_scheduler_add(aerortos_m8_low_task) ==
          AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m8_medium_task) ==
          AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m8_high_task) ==
          AERORTOS_SCHEDULER_OK);
    aerortos_port_init();
    aerortos_scheduler_start();
}
