#include <stdbool.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/cortex_m4.h"
#include "aerortos/critical_section.h"
#include "aerortos/platform.h"
#include "aerortos/port.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"

#define M6_STACK_WORDS 160U
#define M6_COMPLETE_MAGIC UINT32_C(0x4D365041)
#define M6_TRACE_CAPACITY 32U
#define M6_START_TICK (UINT32_MAX - 10U)

volatile uint32_t aerortos_m6_hw_pass_count;
volatile uint32_t aerortos_m6_hw_fail_count;
volatile uint32_t aerortos_m6_hw_complete;
volatile uint32_t aerortos_m6_hw_irq_count;
volatile uint32_t aerortos_m6_hw_preemption_count;
volatile uint32_t aerortos_m6_hw_idle_count;
volatile uint32_t aerortos_m6_hw_trace_count;
volatile uint32_t aerortos_m6_hw_trace[M6_TRACE_CAPACITY];
volatile uint32_t aerortos_m6_high_releases[5];
volatile uint32_t aerortos_m6_peer_releases[5];
volatile uint32_t aerortos_m6_low_releases[6];

aerortos_task_t *aerortos_m6_idle_task;
aerortos_task_t *aerortos_m6_high_task;
aerortos_task_t *aerortos_m6_peer_task;
aerortos_task_t *aerortos_m6_low_task;

static volatile uint32_t low_entered;
static volatile uint32_t low_resumed;
static volatile uint32_t tick_started;

alignas(8) static uint32_t idle_stack[M6_STACK_WORDS];
alignas(8) static uint32_t high_stack[M6_STACK_WORDS];
alignas(8) static uint32_t peer_stack[M6_STACK_WORDS];
alignas(8) static uint32_t low_stack[M6_STACK_WORDS];

int main(void);
bool aerortos_m5_register_probe(void);
void aerortos_m6_hw_test_complete_marker(void);

static void check(bool condition)
{
    if (condition) {
        ++aerortos_m6_hw_pass_count;
    } else {
        ++aerortos_m6_hw_fail_count;
    }
}

static void trace(uint32_t value)
{
    const uint32_t index = aerortos_m6_hw_trace_count;

    if (index < M6_TRACE_CAPACITY) {
        aerortos_m6_hw_trace[index] = value;
        aerortos_m6_hw_trace_count = index + 1U;
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

void aerortos_m6_hw_test_complete_marker(void)
{
    __asm volatile ("" ::: "memory");
}

static void idle_entry(void *argument)
{
    (void)argument;
    if (tick_started == 0U) {
        check(aerortos_systick_init());
        tick_started = 1U;
    }
    for (;;) {
        ++aerortos_m6_hw_idle_count;
        aerortos_wait_for_interrupt();
    }
}

static void high_entry(void *argument)
{
    (void)argument;
    for (;;) {
        const uint32_t release_index = aerortos_m6_high_task->release_count - 1U;

        if (release_index < 5U) {
            aerortos_m6_high_releases[release_index] =
                aerortos_m6_high_task->release_tick;
        }
        trace(UINT32_C(0xA1000000) | (release_index + 1U));
        if ((release_index == 0U) && (low_entered != 0U) &&
            (low_resumed == 0U)) {
            ++aerortos_m6_hw_preemption_count;
        }
        if (release_index == 2U) {
            while (!aerortos_tick_after(
                aerortos_tick_now(),
                aerortos_m6_high_task->absolute_deadline)) {
            }
        }
        check(aerortos_periodic_wait_next() == AERORTOS_TIMER_OK);
    }
}

static void peer_entry(void *argument)
{
    (void)argument;
    for (;;) {
        const uint32_t release_index = aerortos_m6_peer_task->release_count - 1U;

        if (release_index < 5U) {
            aerortos_m6_peer_releases[release_index] =
                aerortos_m6_peer_task->release_tick;
        }
        trace(UINT32_C(0xB2000000) | (release_index + 1U));
        check(aerortos_periodic_wait_next() == AERORTOS_TIMER_OK);
    }
}

static void validate_completion(void)
{
    size_t index;

    aerortos_m6_hw_irq_count = aerortos_systick_irq_count;
    check(aerortos_m6_high_task->release_count >= 4U);
    check(aerortos_m6_peer_task->release_count >= 4U);
    check(aerortos_m6_low_task->release_count >= 6U);
    check(aerortos_m6_high_task->deadline_miss_count == 1U);
    check(aerortos_m6_high_task->overrun_count == 0U);
    check(aerortos_m6_high_task->completion_count >= 4U);
    check(aerortos_m6_high_task->total_execution_ticks > 0U);
    check(aerortos_m6_hw_preemption_count == 1U);
    check(aerortos_m6_hw_idle_count > 0U);
    check(aerortos_m6_hw_irq_count > 0U);
    check(aerortos_tick_now() < 100U);
    check((read_control() & UINT32_C(0x2)) != 0U);
    check(read_basepri() == 0U);
    check(((AERORTOS_SCB_SHPR2 >> 24) & UINT32_C(0xFF)) ==
          AERORTOS_SVC_PRIORITY);
    check(((AERORTOS_SCB_SHPR3 >> 16) & UINT32_C(0xFF)) ==
          AERORTOS_PENDSV_PRIORITY);
    check(((AERORTOS_SCB_SHPR3 >> 24) & UINT32_C(0xFF)) ==
          AERORTOS_KERNEL_BASEPRI);
    check(aerortos_scheduler_invariants_hold());
    check(aerortos_task_stack_guard_intact(aerortos_m6_high_task));
    check(aerortos_task_stack_guard_intact(aerortos_m6_peer_task));
    check(aerortos_task_stack_guard_intact(aerortos_m6_low_task));

    for (index = 1U; index < 4U; ++index) {
        check((uint32_t)(aerortos_m6_high_releases[index] -
                         aerortos_m6_high_releases[index - 1U]) == 5U);
        check((uint32_t)(aerortos_m6_peer_releases[index] -
                         aerortos_m6_peer_releases[index - 1U]) == 5U);
    }
    for (index = 1U; index < 6U; ++index) {
        check((uint32_t)(aerortos_m6_low_releases[index] -
                         aerortos_m6_low_releases[index - 1U]) == 4U);
    }
    check(aerortos_m6_high_releases[0] == aerortos_m6_peer_releases[0]);
    check(aerortos_m6_high_task->timing_trace_count <=
          AERORTOS_TIMING_TRACE_CAPACITY);
    aerortos_m6_hw_complete = M6_COMPLETE_MAGIC;
    aerortos_m6_hw_test_complete_marker();
}

static void low_entry(void *argument)
{
    (void)argument;
    check(aerortos_m5_register_probe());
    for (;;) {
        const uint32_t release_index = aerortos_m6_low_task->release_count - 1U;

        if (release_index < 6U) {
            aerortos_m6_low_releases[release_index] =
                aerortos_m6_low_task->release_tick;
        }
        trace(UINT32_C(0xC3000000) | (release_index + 1U));
        if (release_index == 0U) {
            low_entered = 1U;
            while (aerortos_m6_high_task->release_count == 0U) {
            }
            low_resumed = 1U;
            trace(UINT32_C(0xC3FF0001));
        }
        if ((release_index >= 5U) &&
            (aerortos_m6_high_task->completion_count >= 4U) &&
            (aerortos_m6_peer_task->completion_count >= 4U)) {
            validate_completion();
        }
        check(aerortos_periodic_wait_next() == AERORTOS_TIMER_OK);
    }
}

static aerortos_task_t *create_task(
    aerortos_task_entry_t entry,
    uint32_t *stack,
    uint8_t priority,
    uint32_t period,
    uint32_t deadline,
    uint32_t initial_delay)
{
    aerortos_task_config_t config;
    aerortos_task_t *task = NULL;

    config.entry = entry;
    config.argument = NULL;
    config.stack = stack;
    config.stack_size_bytes = M6_STACK_WORDS * sizeof(uint32_t);
    config.priority = priority;
    config.period_ticks = period;
    config.relative_deadline_ticks = deadline;
    config.initial_release_delay_ticks = initial_delay;
    if (aerortos_task_create(&config, &task) != AERORTOS_TASK_OK) {
        ++aerortos_m6_hw_fail_count;
        platform_panic(UINT32_C(0x4D360001));
    }
    return task;
}

int main(void)
{
    if (!system_clock_init()) {
        platform_panic(UINT32_C(0x4D360002));
    }
    aerortos_task_system_init();
    aerortos_time_system_init(M6_START_TICK);
    aerortos_m6_idle_task = create_task(
        idle_entry, idle_stack, AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    aerortos_m6_high_task = create_task(high_entry, high_stack, 2U, 5U, 2U, 2U);
    aerortos_m6_peer_task = create_task(peer_entry, peer_stack, 5U, 5U, 5U, 2U);
    aerortos_m6_low_task = create_task(low_entry, low_stack, 10U, 4U, 4U, 1U);
    aerortos_scheduler_init(aerortos_m6_idle_task);
    check(aerortos_scheduler_add(aerortos_m6_low_task) == AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m6_peer_task) == AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m6_high_task) == AERORTOS_SCHEDULER_OK);
    aerortos_port_init();
    aerortos_scheduler_start();
}
