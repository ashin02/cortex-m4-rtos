#include <stdbool.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/cortex_m4.h"
#include "aerortos/platform.h"
#include "aerortos/port.h"
#include "aerortos/queue.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"

#define M7_STACK_WORDS 160U
#define M7_COMPLETE_MAGIC UINT32_C(0x4D375041)
#define M7_EXPECTED_PASS_COUNT 40U
#define M7_TRACE_CAPACITY 16U

volatile uint32_t aerortos_m7_hw_pass_count;
volatile uint32_t aerortos_m7_hw_fail_count;
volatile uint32_t aerortos_m7_hw_complete;
volatile uint32_t aerortos_m7_hw_preemption_count;
volatile uint32_t aerortos_m7_hw_blocked_send_count;
volatile uint32_t aerortos_m7_hw_blocked_receive_count;
volatile uint32_t aerortos_m7_hw_trace_count;
volatile uint32_t aerortos_m7_hw_trace[M7_TRACE_CAPACITY];

aerortos_queue_t aerortos_m7_queue;
aerortos_task_t *aerortos_m7_idle_task;
aerortos_task_t *aerortos_m7_high_task;
aerortos_task_t *aerortos_m7_low_task;

alignas(8) static uint32_t idle_stack[M7_STACK_WORDS];
alignas(8) static uint32_t high_stack[M7_STACK_WORDS];
alignas(8) static uint32_t low_stack[M7_STACK_WORDS];
static uint32_t queue_storage[1];
static volatile uint32_t low_in_queue_operation;
static volatile uint32_t low_final_check_complete;

int main(void);
void SysTick_Handler(void);
void aerortos_m7_hw_test_complete_marker(void);

static void check(bool condition)
{
    if (condition) {
        ++aerortos_m7_hw_pass_count;
    } else {
        ++aerortos_m7_hw_fail_count;
    }
}

static void trace(uint32_t value)
{
    const uint32_t index = aerortos_m7_hw_trace_count;

    if (index < M7_TRACE_CAPACITY) {
        aerortos_m7_hw_trace[index] = value;
        aerortos_m7_hw_trace_count = index + 1U;
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

void SysTick_Handler(void)
{
    __asm volatile ("" ::: "memory");
}

void aerortos_m7_hw_test_complete_marker(void)
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
    uint32_t value;
    uint32_t output = 0U;
    size_t count = 0U;

    (void)argument;
    trace(UINT32_C(0xA7000001));

    value = UINT32_C(0x11111111);
    check(aerortos_queue_try_send(&aerortos_m7_queue, &value) ==
          AERORTOS_QUEUE_OK);
    value = UINT32_C(0x22222222);
    check(aerortos_queue_try_send(&aerortos_m7_queue, &value) ==
          AERORTOS_QUEUE_ERROR_FULL);
    ++aerortos_m7_hw_blocked_send_count;
    check(aerortos_queue_send(&aerortos_m7_queue, &value) ==
          AERORTOS_QUEUE_OK);
    trace(UINT32_C(0xA7000002));
    check(low_in_queue_operation != 0U);
    ++aerortos_m7_hw_preemption_count;
    check(aerortos_queue_count(&aerortos_m7_queue, &count) ==
          AERORTOS_QUEUE_OK);
    check(count == 1U);
    check(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);

    trace(UINT32_C(0xA7000003));
    ++aerortos_m7_hw_blocked_receive_count;
    check(aerortos_queue_receive(&aerortos_m7_queue, &output) ==
          AERORTOS_QUEUE_OK);
    trace(UINT32_C(0xA7000004));
    check(low_in_queue_operation != 0U);
    check(output == UINT32_C(0x33333333));

    ++aerortos_m7_hw_blocked_receive_count;
    check(aerortos_queue_receive(&aerortos_m7_queue, &output) ==
          AERORTOS_QUEUE_OK);
    trace(UINT32_C(0xA7000005));
    check(low_in_queue_operation != 0U);
    check(output == UINT32_C(0x44444444));
    check(aerortos_queue_count(&aerortos_m7_queue, &count) ==
          AERORTOS_QUEUE_OK);
    check(count == 0U);
    check(aerortos_m7_queue.waiting_senders == 0U);
    check(aerortos_m7_queue.waiting_receivers == 0U);
    check(aerortos_queue_invariants_hold(&aerortos_m7_queue));
    check(aerortos_scheduler_invariants_hold());
    check(aerortos_task_stack_guard_intact(aerortos_m7_high_task));
    check(aerortos_task_stack_guard_intact(aerortos_m7_low_task));
    check((read_control() & UINT32_C(0x2)) != 0U);
    check(read_basepri() == 0U);
    check(aerortos_m7_hw_preemption_count == 1U);
    check(aerortos_m7_hw_blocked_send_count == 1U);
    check(aerortos_m7_hw_blocked_receive_count == 2U);
    if (aerortos_scheduler_block_current() != AERORTOS_SCHEDULER_OK) {
        ++aerortos_m7_hw_fail_count;
        platform_panic(UINT32_C(0x4D370004));
    }
    if ((low_final_check_complete != 0U) &&
        (aerortos_m7_hw_pass_count == M7_EXPECTED_PASS_COUNT) &&
        (aerortos_m7_hw_fail_count == 0U)) {
        aerortos_m7_hw_complete = M7_COMPLETE_MAGIC;
    }
    aerortos_m7_hw_test_complete_marker();
    for (;;) {
    }
}

static void low_entry(void *argument)
{
    uint32_t output = 0U;
    uint32_t value;

    (void)argument;
    trace(UINT32_C(0xB7000001));
    check(aerortos_m7_queue.waiting_senders == 1U);
    check(aerortos_queue_invariants_hold(&aerortos_m7_queue));
    low_in_queue_operation = 1U;
    check(aerortos_queue_try_receive(&aerortos_m7_queue, &output) ==
          AERORTOS_QUEUE_OK);
    low_in_queue_operation = 0U;
    trace(UINT32_C(0xB7000002));
    check(output == UINT32_C(0x11111111));
    check(aerortos_queue_try_receive(&aerortos_m7_queue, &output) ==
          AERORTOS_QUEUE_OK);
    check(output == UINT32_C(0x22222222));
    check(aerortos_scheduler_make_ready(aerortos_m7_high_task) ==
          AERORTOS_SCHEDULER_OK);

    trace(UINT32_C(0xB7000003));
    check(aerortos_m7_queue.waiting_receivers == 1U);
    check(aerortos_queue_invariants_hold(&aerortos_m7_queue));
    value = UINT32_C(0x33333333);
    low_in_queue_operation = 1U;
    check(aerortos_queue_try_send(&aerortos_m7_queue, &value) ==
          AERORTOS_QUEUE_OK);
    low_in_queue_operation = 0U;

    trace(UINT32_C(0xB7000004));
    check(aerortos_m7_queue.waiting_receivers == 1U);
    value = UINT32_C(0x44444444);
    low_in_queue_operation = 1U;
    check(aerortos_queue_try_send(&aerortos_m7_queue, &value) ==
          AERORTOS_QUEUE_OK);
    low_in_queue_operation = 0U;
    low_final_check_complete = 1U;
    if (aerortos_scheduler_make_ready(aerortos_m7_high_task) !=
        AERORTOS_SCHEDULER_OK) {
        ++aerortos_m7_hw_fail_count;
        platform_panic(UINT32_C(0x4D370005));
    }
    for (;;) {
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
    config.stack_size_bytes = M7_STACK_WORDS * sizeof(uint32_t);
    config.priority = priority;
    config.period_ticks = 0U;
    config.relative_deadline_ticks = 0U;
    config.initial_release_delay_ticks = 0U;
    if (aerortos_task_create(&config, &task) != AERORTOS_TASK_OK) {
        ++aerortos_m7_hw_fail_count;
        platform_panic(UINT32_C(0x4D370001));
    }
    return task;
}

int main(void)
{
    if (!system_clock_init()) {
        platform_panic(UINT32_C(0x4D370002));
    }
    aerortos_task_system_init();
    aerortos_time_system_init(0U);
    if (aerortos_queue_init(&aerortos_m7_queue, queue_storage, 1U,
                            sizeof(queue_storage[0])) != AERORTOS_QUEUE_OK) {
        platform_panic(UINT32_C(0x4D370003));
    }
    aerortos_m7_idle_task = create_task(
        idle_entry, idle_stack, AERORTOS_IDLE_PRIORITY);
    aerortos_m7_high_task = create_task(high_entry, high_stack, 2U);
    aerortos_m7_low_task = create_task(low_entry, low_stack, 10U);
    aerortos_scheduler_init(aerortos_m7_idle_task);
    check(aerortos_scheduler_add(aerortos_m7_low_task) ==
          AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m7_high_task) ==
          AERORTOS_SCHEDULER_OK);
    aerortos_port_init();
    aerortos_scheduler_start();
}
