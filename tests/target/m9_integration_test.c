#include <stdbool.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/cortex_m4.h"
#include "aerortos/mutex.h"
#include "aerortos/platform.h"
#include "aerortos/port.h"
#include "aerortos/queue.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"

#define M9_STACK_WORDS 224U
#define M9_COMPLETE_MAGIC UINT32_C(0x4D395041)
#define M9_EXPECTED_PASS_COUNT 69U
#define M9_EXPECTED_TRACE_COUNT 8U
#define M9_TRACE_CAPACITY 16U

volatile uint32_t aerortos_m9_hw_pass_count;
volatile uint32_t aerortos_m9_hw_fail_count;
volatile uint32_t aerortos_m9_hw_complete;
volatile uint32_t aerortos_m9_hw_expected_pass_count =
    M9_EXPECTED_PASS_COUNT;
volatile uint32_t aerortos_m9_hw_preemption_count;
volatile uint32_t aerortos_m9_hw_block_count;
volatile uint32_t aerortos_m9_hw_wakeup_count;
volatile uint32_t aerortos_m9_hw_queue_operation_count;
volatile uint32_t aerortos_m9_hw_pi_boost_count;
volatile uint32_t aerortos_m9_hw_pi_restoration_count;
volatile uint32_t aerortos_m9_hw_periodic_release_count;
volatile uint32_t aerortos_m9_hw_trace_count;
volatile uint32_t aerortos_m9_hw_trace[M9_TRACE_CAPACITY];

volatile uint32_t aerortos_m9_high_complete;
volatile uint32_t aerortos_m9_consumer_complete;
volatile uint32_t aerortos_m9_low_complete;

aerortos_queue_t aerortos_m9_queue;
aerortos_mutex_t aerortos_m9_mutex;
aerortos_task_t *aerortos_m9_idle_task;
aerortos_task_t *aerortos_m9_high_task;
aerortos_task_t *aerortos_m9_consumer_task;
aerortos_task_t *aerortos_m9_low_task;

alignas(8) static uint32_t idle_stack[M9_STACK_WORDS];
alignas(8) static uint32_t high_stack[M9_STACK_WORDS];
alignas(8) static uint32_t consumer_stack[M9_STACK_WORDS];
alignas(8) static uint32_t low_stack[M9_STACK_WORDS];
static uint32_t queue_storage[1];

static volatile uint32_t low_unlock_in_progress;

int main(void);
void aerortos_m9_hw_test_complete_marker(void);

static void check(bool condition)
{
    if (condition) {
        ++aerortos_m9_hw_pass_count;
    } else {
        ++aerortos_m9_hw_fail_count;
    }
}

static void trace(uint32_t value)
{
    const uint32_t index = aerortos_m9_hw_trace_count;

    if (index < M9_TRACE_CAPACITY) {
        aerortos_m9_hw_trace[index] = value;
        aerortos_m9_hw_trace_count = index + 1U;
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
        ++aerortos_m9_hw_fail_count;
        platform_panic(code);
    }
}

void aerortos_m9_hw_test_complete_marker(void)
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

static void consumer_entry(void *argument)
{
    uint32_t value = 0U;
    size_t count = 1U;

    (void)argument;
    trace(UINT32_C(0xB9000001));
    check(aerortos_queue_count(&aerortos_m9_queue, &count) ==
          AERORTOS_QUEUE_OK && count == 0U);
    ++aerortos_m9_hw_block_count;
    check(aerortos_queue_receive(&aerortos_m9_queue, &value) ==
          AERORTOS_QUEUE_OK);
    trace(UINT32_C(0xB9000002));
    ++aerortos_m9_hw_queue_operation_count;
    check(value == UINT32_C(0x4D390091));
    check(aerortos_m9_consumer_task->wait_kind ==
          (uint8_t)AERORTOS_WAIT_NONE);
    check(aerortos_queue_invariants_hold(&aerortos_m9_queue));
    aerortos_m9_consumer_complete = 1U;
    sync_block_or_panic(UINT32_C(0x4D390006));
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

static void low_entry(void *argument)
{
    const uint32_t value = UINT32_C(0x4D390091);

    (void)argument;
    check(aerortos_mutex_lock(&aerortos_m9_mutex) == AERORTOS_MUTEX_OK);
    check(aerortos_m9_mutex.owner == aerortos_m9_low_task);
    trace(UINT32_C(0xC9000001));

    while (aerortos_m9_mutex.waiter_count == 0U) {
        aerortos_wait_for_interrupt();
    }

    trace(UINT32_C(0xC9000002));
    check(aerortos_m9_mutex.waiter_count == 1U);
    check(aerortos_m9_high_task->wait_kind ==
          (uint8_t)AERORTOS_WAIT_MUTEX);
    check(aerortos_m9_low_task->effective_priority ==
          aerortos_m9_high_task->base_priority);
    ++aerortos_m9_hw_pi_boost_count;
    check(aerortos_m9_queue.waiting_receivers == 1U);
    check(aerortos_queue_send(&aerortos_m9_queue, &value) ==
          AERORTOS_QUEUE_OK);
    ++aerortos_m9_hw_queue_operation_count;
    ++aerortos_m9_hw_wakeup_count;
    check(aerortos_m9_consumer_task->state == AERORTOS_TASK_READY);
    check(aerortos_queue_invariants_hold(&aerortos_m9_queue));

    low_unlock_in_progress = 1U;
    check(aerortos_mutex_unlock(&aerortos_m9_mutex) == AERORTOS_MUTEX_OK);
    low_unlock_in_progress = 0U;
    trace(UINT32_C(0xC9000003));
    check(aerortos_m9_low_task->effective_priority ==
          aerortos_m9_low_task->base_priority);
    check(aerortos_m9_low_task->owned_mutex_count == 0U);
    check(aerortos_scheduler_invariants_hold());
    ++aerortos_m9_hw_pi_restoration_count;
    aerortos_m9_low_complete = 1U;
    sync_block_or_panic(UINT32_C(0x4D390007));
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

static void high_entry(void *argument)
{
    static const uint32_t expected_trace[M9_EXPECTED_TRACE_COUNT] = {
        UINT32_C(0xB9000001), UINT32_C(0xC9000001),
        UINT32_C(0xA9000001), UINT32_C(0xC9000002),
        UINT32_C(0xA9000002), UINT32_C(0xB9000002),
        UINT32_C(0xC9000003), UINT32_C(0xA9000003)
    };
    size_t queue_count = 1U;
    size_t index;
    uint32_t phase = 0U;

    (void)argument;
    for (;;) {
        ++phase;
        ++aerortos_m9_hw_periodic_release_count;
        ++aerortos_m9_hw_preemption_count;
        if (phase == 1U) {
            trace(UINT32_C(0xA9000001));
            check(aerortos_m9_high_task->release_count == 1U);
            check(aerortos_m9_high_task->release_tick == 7U);
            check(aerortos_m9_mutex.owner == aerortos_m9_low_task);
            ++aerortos_m9_hw_block_count;
            check(aerortos_mutex_lock(&aerortos_m9_mutex) ==
                  AERORTOS_MUTEX_OK);
            trace(UINT32_C(0xA9000002));
            ++aerortos_m9_hw_wakeup_count;
            ++aerortos_m9_hw_preemption_count;
            check(low_unlock_in_progress != 0U);
            check(aerortos_m9_mutex.owner == aerortos_m9_high_task);
            check(aerortos_m9_low_task->effective_priority ==
                  aerortos_m9_low_task->base_priority);
            check(aerortos_m9_high_task->effective_priority ==
                  aerortos_m9_high_task->base_priority);
            check(aerortos_mutex_unlock(&aerortos_m9_mutex) ==
                  AERORTOS_MUTEX_OK);
            check(aerortos_m9_mutex.owner == NULL);
            check(aerortos_periodic_wait_next() == AERORTOS_TIMER_OK);
        } else if (phase == 2U) {
            trace(UINT32_C(0xA9000003));
            check(aerortos_m9_high_task->release_count == 2U);
            check(aerortos_m9_high_task->release_tick == 12U);
            check(aerortos_m9_high_task->last_release_jitter_ticks == 0);
            check((aerortos_m9_high_complete == 0U) &&
                  (aerortos_m9_consumer_complete == 1U) &&
                  (aerortos_m9_low_complete == 1U));
            check(aerortos_m9_hw_preemption_count == 3U);
            check(aerortos_m9_hw_block_count == 2U);
            check(aerortos_m9_hw_wakeup_count == 2U);
            check(aerortos_m9_hw_queue_operation_count == 2U);
            check(aerortos_m9_hw_pi_boost_count == 1U);
            check(aerortos_m9_hw_pi_restoration_count == 1U);
            check(aerortos_m9_hw_periodic_release_count == 2U);
            check(aerortos_queue_count(&aerortos_m9_queue, &queue_count) ==
                  AERORTOS_QUEUE_OK);
            check(queue_count == 0U);
            check((aerortos_m9_queue.waiting_senders == 0U) &&
                  (aerortos_m9_queue.waiting_receivers == 0U));
            check(aerortos_m9_mutex.owner == NULL);
            check(aerortos_queue_invariants_hold(&aerortos_m9_queue));
            check(aerortos_mutex_invariants_hold(&aerortos_m9_mutex));
            check(aerortos_scheduler_invariants_hold());
            check(aerortos_task_stack_guard_intact(aerortos_m9_high_task));
            check(aerortos_task_stack_guard_intact(
                aerortos_m9_consumer_task));
            check(aerortos_task_stack_guard_intact(aerortos_m9_low_task));
            check((aerortos_m9_consumer_task->state ==
                   AERORTOS_TASK_BLOCKED) &&
                  (aerortos_m9_low_task->state == AERORTOS_TASK_BLOCKED));
            check((aerortos_m9_high_task->effective_priority ==
                   aerortos_m9_high_task->base_priority) &&
                  (aerortos_m9_low_task->effective_priority ==
                   aerortos_m9_low_task->base_priority));
            check((read_control() & UINT32_C(0x2)) != 0U);
            check(read_basepri() == 0U);
            check(aerortos_m9_hw_trace_count == M9_EXPECTED_TRACE_COUNT);
            for (index = 0U; index < M9_EXPECTED_TRACE_COUNT; ++index) {
                check(aerortos_m9_hw_trace[index] == expected_trace[index]);
            }
            aerortos_m9_high_complete = 1U;
            if ((aerortos_m9_hw_pass_count == M9_EXPECTED_PASS_COUNT) &&
                (aerortos_m9_hw_fail_count == 0U) &&
                (aerortos_m9_hw_trace_count == M9_EXPECTED_TRACE_COUNT) &&
                (aerortos_m9_high_complete == 1U) &&
                (aerortos_m9_consumer_complete == 1U) &&
                (aerortos_m9_low_complete == 1U)) {
                aerortos_m9_hw_complete = M9_COMPLETE_MAGIC;
            }
            aerortos_m9_hw_test_complete_marker();
            sync_block_or_panic(UINT32_C(0x4D390008));
        } else {
            ++aerortos_m9_hw_fail_count;
            platform_panic(UINT32_C(0x4D390009));
        }
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
    config.stack_size_bytes = M9_STACK_WORDS * sizeof(uint32_t);
    config.priority = priority;
    config.period_ticks = period;
    config.relative_deadline_ticks = deadline;
    config.initial_release_delay_ticks = initial_delay;
    if (aerortos_task_create(&config, &task) != AERORTOS_TASK_OK) {
        ++aerortos_m9_hw_fail_count;
        platform_panic(UINT32_C(0x4D390001));
    }
    return task;
}

int main(void)
{
    if (!system_clock_init()) {
        platform_panic(UINT32_C(0x4D390002));
    }
    aerortos_task_system_init();
    aerortos_time_system_init(UINT32_MAX - 2U);
    check(aerortos_queue_init(&aerortos_m9_queue, queue_storage, 1U,
                              sizeof(queue_storage[0])) == AERORTOS_QUEUE_OK);
    check(aerortos_mutex_init(&aerortos_m9_mutex) == AERORTOS_MUTEX_OK);
    aerortos_m9_idle_task = create_task(
        idle_entry, idle_stack, AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    aerortos_m9_high_task = create_task(
        high_entry, high_stack, 2U, 5U, 4U, 10U);
    aerortos_m9_consumer_task = create_task(
        consumer_entry, consumer_stack, 5U, 0U, 0U, 0U);
    aerortos_m9_low_task = create_task(
        low_entry, low_stack, 10U, 0U, 0U, 0U);
    aerortos_scheduler_init(aerortos_m9_idle_task);
    check(aerortos_scheduler_add(aerortos_m9_low_task) ==
          AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m9_consumer_task) ==
          AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m9_high_task) ==
          AERORTOS_SCHEDULER_OK);
    aerortos_port_init();
    check(aerortos_systick_init());
    aerortos_scheduler_start();
}
