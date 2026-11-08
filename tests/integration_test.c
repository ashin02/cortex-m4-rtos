#include <assert.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "aerortos/critical_section.h"
#include "aerortos/mutex.h"
#include "aerortos/queue.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"

#define TEST_STACK_WORDS 96U

extern uint32_t aerortos_host_basepri;
extern uint32_t aerortos_host_pendsv_requests;
extern bool aerortos_host_handler_mode;
extern void (*aerortos_host_queue_block_callback)(void);
extern void (*aerortos_host_mutex_block_callback)(void);

static jmp_buf block_escape;
static aerortos_queue_t *mixed_queue;
static aerortos_mutex_t *mixed_mutex;
static aerortos_task_t *mixed_high;
static aerortos_task_t *mixed_low;
static aerortos_task_t *mixed_helper;
static uint32_t mixed_value;

static void test_entry(void *argument) { (void)argument; }

static aerortos_task_t *create_task(
    uint32_t *stack,
    uint8_t priority,
    uint32_t period,
    uint32_t deadline,
    uint32_t initial_delay)
{
    const aerortos_task_config_t config = {
        .entry = test_entry,
        .argument = NULL,
        .stack = stack,
        .stack_size_bytes = TEST_STACK_WORDS * sizeof(uint32_t),
        .priority = priority,
        .period_ticks = period,
        .relative_deadline_ticks = deadline,
        .initial_release_delay_ticks = initial_delay
    };
    aerortos_task_t *task = NULL;

    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_OK);
    return task;
}

static void reset_host(uint32_t initial_tick)
{
    aerortos_task_system_init();
    aerortos_time_system_init(initial_tick);
    aerortos_host_basepri = 0U;
    aerortos_host_pendsv_requests = 0U;
    aerortos_host_handler_mode = false;
    aerortos_host_queue_block_callback = NULL;
    aerortos_host_mutex_block_callback = NULL;
}

static void dispatch_pending(void)
{
    assert(aerortos_host_pendsv_requests != 0U);
    aerortos_host_pendsv_requests = 0U;
    assert(aerortos_scheduler_switch_context() != NULL);
    assert(aerortos_scheduler_invariants_hold());
}

static void run_tick(void)
{
    aerortos_host_handler_mode = true;
    aerortos_timer_tick_isr();
    aerortos_host_handler_mode = false;
}

static void block_and_dispatch(void)
{
    assert(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
}

static void escape_queue_block(void)
{
    longjmp(block_escape, 1);
}

static void escape_mutex_block(void)
{
    longjmp(block_escape, 1);
}

static void test_m5_m6_wrap_preemption_deadline(void)
{
    alignas(8) uint32_t stacks[3][TEST_STACK_WORDS];
    aerortos_task_t *idle;
    aerortos_task_t *periodic;
    aerortos_task_t *low;

    reset_host(UINT32_MAX - 1U);
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    periodic = create_task(stacks[1], 2U, 4U, 2U, 2U);
    low = create_task(stacks[2], 10U, 0U, 0U, 0U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(periodic) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_scheduler_current() == low);

    run_tick();
    assert(aerortos_tick_now() == UINT32_MAX);
    assert(periodic->release_count == 0U);
    run_tick();
    assert(aerortos_tick_now() == 0U);
    assert(periodic->release_count == 1U);
    assert(periodic->release_tick == 0U);
    assert(periodic->absolute_deadline == 2U);
    assert(periodic->next_release_tick == 4U);
    dispatch_pending();
    assert(aerortos_scheduler_current() == periodic);

    run_tick();
    run_tick();
    assert(periodic->deadline_miss_count == 0U);
    run_tick();
    assert(periodic->deadline_miss_count == 1U);
    assert(aerortos_periodic_wait_next() == AERORTOS_TIMER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == low);
    run_tick();
    assert(periodic->release_count == 2U);
    assert(periodic->release_tick == 4U);
    dispatch_pending();
    assert(aerortos_scheduler_current() == periodic);
    assert(periodic->last_release_jitter_ticks == 0);
    assert(aerortos_task_stack_guard_intact(periodic));
}

static void test_m6_m7_periodic_queue_overrun(void)
{
    alignas(8) static uint32_t stacks[3][TEST_STACK_WORDS];
    static aerortos_queue_t queue;
    static uint32_t storage[1];
    uint32_t output = 0U;
    uint32_t value = UINT32_C(0x67190001);
    aerortos_task_t *idle;
    aerortos_task_t *periodic;
    aerortos_task_t *helper;

    reset_host(0U);
    queue = (aerortos_queue_t){0};
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    periodic = create_task(stacks[1], 2U, 3U, 3U, 0U);
    helper = create_task(stacks[2], 10U, 0U, 0U, 0U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(helper) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(periodic) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_queue_init(&queue, storage, 1U, sizeof(storage[0])) ==
           AERORTOS_QUEUE_OK);
    assert(periodic->job_active == 1U);

    aerortos_host_queue_block_callback = escape_queue_block;
    if (setjmp(block_escape) == 0) {
        (void)aerortos_queue_receive(&queue, &output);
        assert(false);
    }
    assert(periodic->state == AERORTOS_TASK_BLOCKED);
    assert(periodic->wait_kind == (uint8_t)AERORTOS_WAIT_QUEUE_RECEIVE);
    dispatch_pending();
    assert(aerortos_scheduler_current() == helper);
    run_tick();
    run_tick();
    run_tick();
    assert(periodic->overrun_count == 1U);
    assert(periodic->skipped_release_count == 1U);
    assert(periodic->release_count == 1U);
    assert(periodic->state == AERORTOS_TASK_BLOCKED);
    assert(queue.waiting_receivers == 1U);
    assert(aerortos_queue_invariants_hold(&queue));

    assert(aerortos_queue_try_send(&queue, &value) == AERORTOS_QUEUE_OK);
    assert(periodic->state == AERORTOS_TASK_READY);
    dispatch_pending();
    assert(aerortos_scheduler_current() == periodic);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == value);
    assert(aerortos_periodic_wait_next() == AERORTOS_TIMER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == helper);
    assert(periodic->completion_count == 1U);
    assert(aerortos_queue_invariants_hold(&queue));
}

static void test_m7_m8_queue_waiter_reordering(void)
{
    alignas(8) static uint32_t stacks[5][TEST_STACK_WORDS];
    static aerortos_mutex_t mutex;
    static aerortos_queue_t queue;
    static uint32_t storage[1];
    uint32_t output = 0U;
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *medium;
    aerortos_task_t *low;
    aerortos_task_t *producer;

    reset_host(0U);
    mutex = (aerortos_mutex_t){0};
    queue = (aerortos_queue_t){0};
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    high = create_task(stacks[1], 2U, 0U, 0U, 0U);
    medium = create_task(stacks[2], 5U, 0U, 0U, 0U);
    low = create_task(stacks[3], 10U, 0U, 0U, 0U);
    producer = create_task(stacks[4], 15U, 0U, 0U, 0U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(producer) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(medium) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    block_and_dispatch();
    block_and_dispatch();
    assert(aerortos_scheduler_current() == low);
    assert(aerortos_mutex_init(&mutex) == AERORTOS_MUTEX_OK);
    assert(aerortos_queue_init(&queue, storage, 1U, sizeof(storage[0])) ==
           AERORTOS_QUEUE_OK);
    assert(aerortos_mutex_lock(&mutex) == AERORTOS_MUTEX_OK);
    aerortos_host_queue_block_callback = escape_queue_block;

    if (setjmp(block_escape) == 0) {
        (void)aerortos_queue_receive(&queue, &output);
        assert(false);
    }
    dispatch_pending();
    assert(aerortos_scheduler_current() == producer);
    assert(aerortos_scheduler_make_ready(medium) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    if (setjmp(block_escape) == 0) {
        (void)aerortos_queue_receive(&queue, &output);
        assert(false);
    }
    dispatch_pending();
    assert(queue.receive_wait_head == medium);
    assert(queue.receive_wait_tail == low);

    assert(aerortos_scheduler_make_ready(high) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    aerortos_host_mutex_block_callback = escape_mutex_block;
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&mutex);
        assert(false);
    }
    dispatch_pending();
    assert(low->effective_priority == high->base_priority);
    assert(queue.receive_wait_head == low);
    assert(queue.receive_wait_tail == medium);
    assert(aerortos_queue_invariants_hold(&queue));
    assert(aerortos_mutex_invariants_hold(&mutex));
    assert(aerortos_scheduler_invariants_hold());
}

static void test_m5_m8_multiple_mutex_restoration(void)
{
    alignas(8) static uint32_t stacks[4][TEST_STACK_WORDS];
    static aerortos_mutex_t first;
    static aerortos_mutex_t second;
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *middle;
    aerortos_task_t *low;

    reset_host(0U);
    first = (aerortos_mutex_t){0};
    second = (aerortos_mutex_t){0};
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    high = create_task(stacks[1], 2U, 0U, 0U, 0U);
    middle = create_task(stacks[2], 6U, 0U, 0U, 0U);
    low = create_task(stacks[3], 10U, 0U, 0U, 0U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(middle) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    block_and_dispatch();
    block_and_dispatch();
    assert(aerortos_mutex_init(&first) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_init(&second) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_lock(&first) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_lock(&second) == AERORTOS_MUTEX_OK);
    assert(low->owned_mutex_count == 2U);

    assert(aerortos_scheduler_make_ready(middle) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    aerortos_host_mutex_block_callback = escape_mutex_block;
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&second);
        assert(false);
    }
    dispatch_pending();
    assert(low->effective_priority == middle->base_priority);
    assert(aerortos_scheduler_make_ready(high) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&first);
        assert(false);
    }
    dispatch_pending();
    assert(low->effective_priority == high->base_priority);
    assert(aerortos_mutex_unlock(&first) == AERORTOS_MUTEX_OK);
    assert(low->effective_priority == middle->base_priority);
    dispatch_pending();
    assert(aerortos_scheduler_current() == high);
    assert(aerortos_mutex_unlock(&first) == AERORTOS_MUTEX_OK);
    block_and_dispatch();
    assert(aerortos_scheduler_current() == low);
    assert(aerortos_mutex_unlock(&second) == AERORTOS_MUTEX_OK);
    assert(low->effective_priority == low->base_priority);
    dispatch_pending();
    assert(aerortos_scheduler_current() == middle);
    assert(aerortos_mutex_unlock(&second) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_invariants_hold(&first));
    assert(aerortos_mutex_invariants_hold(&second));
}

static void test_m5_m8_transitive_pi(void)
{
    alignas(8) static uint32_t stacks[4][TEST_STACK_WORDS];
    static aerortos_mutex_t first;
    static aerortos_mutex_t second;
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *middle;
    aerortos_task_t *low;

    reset_host(0U);
    first = (aerortos_mutex_t){0};
    second = (aerortos_mutex_t){0};
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    high = create_task(stacks[1], 2U, 0U, 0U, 0U);
    middle = create_task(stacks[2], 6U, 0U, 0U, 0U);
    low = create_task(stacks[3], 10U, 0U, 0U, 0U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(middle) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    block_and_dispatch();
    block_and_dispatch();
    assert(aerortos_mutex_init(&first) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_init(&second) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_lock(&first) == AERORTOS_MUTEX_OK);

    assert(aerortos_scheduler_make_ready(middle) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_mutex_lock(&second) == AERORTOS_MUTEX_OK);
    block_and_dispatch();
    aerortos_host_mutex_block_callback = escape_mutex_block;
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&second);
        assert(false);
    }
    dispatch_pending();
    assert(aerortos_scheduler_make_ready(middle) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_make_ready(high) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&first);
        assert(false);
    }
    dispatch_pending();
    assert(aerortos_scheduler_current() == middle);
    assert(low->effective_priority == high->base_priority);
    assert(middle->effective_priority == high->base_priority);
    assert(aerortos_mutex_unlock(&second) == AERORTOS_MUTEX_OK);
    assert(middle->effective_priority == middle->base_priority);
    dispatch_pending();
    assert(aerortos_scheduler_current() == low);
    assert(aerortos_mutex_unlock(&first) == AERORTOS_MUTEX_OK);
    assert(low->effective_priority == low->base_priority);
    dispatch_pending();
    assert(aerortos_scheduler_current() == high);
    assert(aerortos_mutex_invariants_hold(&first));
    assert(aerortos_mutex_invariants_hold(&second));
}

static void mixed_mutex_block_hook(void)
{
    uint32_t output = 0U;

    assert(aerortos_scheduler_current() == mixed_high);
    assert(mixed_high->state == AERORTOS_TASK_BLOCKED);
    assert(mixed_low->effective_priority == mixed_high->base_priority);
    assert(mixed_queue->receive_wait_head == mixed_low);
    dispatch_pending();
    assert(aerortos_scheduler_current() == mixed_helper);
    assert(aerortos_queue_try_send(mixed_queue, &mixed_value) ==
           AERORTOS_QUEUE_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == mixed_low);
    assert(aerortos_queue_try_receive(mixed_queue, &output) ==
           AERORTOS_QUEUE_OK);
    assert(output == mixed_value);
    assert(aerortos_mutex_unlock(mixed_mutex) == AERORTOS_MUTEX_OK);
    assert(mixed_low->effective_priority == mixed_low->base_priority);
    dispatch_pending();
    assert(aerortos_scheduler_current() == mixed_high);
}

static void test_m6_m7_m8_mixed_workload(void)
{
    alignas(8) static uint32_t stacks[4][TEST_STACK_WORDS];
    static aerortos_queue_t queue;
    static aerortos_mutex_t mutex;
    static uint32_t storage[1];
    uint32_t output = 0U;
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *low;
    aerortos_task_t *helper;
    aerortos_critical_state_t outer;
    aerortos_critical_state_t inner;

    reset_host(UINT32_MAX - 2U);
    queue = (aerortos_queue_t){0};
    mutex = (aerortos_mutex_t){0};
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    high = create_task(stacks[1], 2U, 5U, 4U, 0U);
    low = create_task(stacks[2], 10U, 0U, 0U, 0U);
    helper = create_task(stacks[3], 15U, 0U, 0U, 0U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(helper) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_queue_init(&queue, storage, 1U, sizeof(storage[0])) ==
           AERORTOS_QUEUE_OK);
    assert(aerortos_mutex_init(&mutex) == AERORTOS_MUTEX_OK);
    block_and_dispatch();
    assert(aerortos_scheduler_current() == low);
    assert(aerortos_mutex_lock(&mutex) == AERORTOS_MUTEX_OK);
    aerortos_host_queue_block_callback = escape_queue_block;
    if (setjmp(block_escape) == 0) {
        (void)aerortos_queue_receive(&queue, &output);
        assert(false);
    }
    dispatch_pending();
    assert(aerortos_scheduler_current() == helper);
    assert(aerortos_scheduler_make_ready(high) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();

    mixed_queue = &queue;
    mixed_mutex = &mutex;
    mixed_high = high;
    mixed_low = low;
    mixed_helper = helper;
    mixed_value = UINT32_C(0x67980001);
    aerortos_host_mutex_block_callback = mixed_mutex_block_hook;
    assert(aerortos_mutex_lock(&mutex) == AERORTOS_MUTEX_OK);
    assert(mutex.owner == high);
    assert(aerortos_mutex_unlock(&mutex) == AERORTOS_MUTEX_OK);
    assert(aerortos_periodic_wait_next() == AERORTOS_TIMER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == low);
    assert(aerortos_queue_invariants_hold(&queue));
    assert(aerortos_mutex_invariants_hold(&mutex));
    assert(aerortos_scheduler_invariants_hold());

    aerortos_host_basepri = UINT32_C(0x80);
    outer = aerortos_critical_enter();
    inner = aerortos_critical_enter();
    assert(outer == UINT32_C(0x80));
    assert(inner == AERORTOS_KERNEL_BASEPRI);
    assert(aerortos_host_basepri == AERORTOS_KERNEL_BASEPRI);
    aerortos_critical_exit(inner);
    assert(aerortos_host_basepri == AERORTOS_KERNEL_BASEPRI);
    aerortos_critical_exit(outer);
    assert(aerortos_host_basepri == UINT32_C(0x80));
}

int main(void)
{
    test_m5_m6_wrap_preemption_deadline();
    test_m6_m7_periodic_queue_overrun();
    test_m7_m8_queue_waiter_reordering();
    test_m5_m8_multiple_mutex_restoration();
    test_m5_m8_transitive_pi();
    test_m6_m7_m8_mixed_workload();
    puts("integration_test: PASS");
    return 0;
}
