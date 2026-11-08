#include <assert.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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
static aerortos_mutex_t *classic_mutex;
static aerortos_task_t *classic_high;
static aerortos_task_t *classic_low;

static void test_entry(void *argument) { (void)argument; }

static aerortos_task_t *create_task(uint32_t *stack, uint8_t priority)
{
    const aerortos_task_config_t config = {
        .entry = test_entry,
        .argument = NULL,
        .stack = stack,
        .stack_size_bytes = TEST_STACK_WORDS * sizeof(uint32_t),
        .priority = priority,
        .period_ticks = 0U,
        .relative_deadline_ticks = 0U,
        .initial_release_delay_ticks = 0U
    };
    aerortos_task_t *task = NULL;

    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_OK);
    return task;
}

static void reset_host(void)
{
    aerortos_task_system_init();
    aerortos_time_system_init(0U);
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

static void block_and_dispatch(void)
{
    assert(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
}

static void escape_mutex_block(void)
{
    longjmp(block_escape, 1);
}

static void escape_queue_block(void)
{
    longjmp(block_escape, 1);
}

static void test_ownership_and_invalid_operations(void)
{
    alignas(8) uint32_t stacks[3][TEST_STACK_WORDS];
    aerortos_mutex_t mutex = {0};
    aerortos_task_t *idle;
    aerortos_task_t *owner;
    aerortos_task_t *other;

    reset_host();
    assert(aerortos_mutex_init(NULL) == AERORTOS_MUTEX_ERROR_NULL);
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    owner = create_task(stacks[1], 4U);
    other = create_task(stacks[2], 9U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(other) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(owner) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_mutex_try_lock(&mutex) == AERORTOS_MUTEX_ERROR_STATE);
    assert(aerortos_mutex_init(&mutex) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_try_lock(&mutex) == AERORTOS_MUTEX_OK);
    assert(mutex.owner == owner);
    assert(owner->owned_mutex_count == 1U);
    assert(aerortos_mutex_try_lock(&mutex) ==
           AERORTOS_MUTEX_ERROR_RECURSIVE);
    assert(aerortos_mutex_lock(&mutex) == AERORTOS_MUTEX_ERROR_RECURSIVE);
    assert(aerortos_mutex_invariants_hold(&mutex));

    block_and_dispatch();
    assert(aerortos_scheduler_current() == other);
    assert(aerortos_mutex_try_lock(&mutex) == AERORTOS_MUTEX_ERROR_BUSY);
    assert(aerortos_mutex_unlock(&mutex) ==
           AERORTOS_MUTEX_ERROR_NOT_OWNER);
    aerortos_host_handler_mode = true;
    assert(aerortos_mutex_try_lock(&mutex) == AERORTOS_MUTEX_ERROR_CONTEXT);
    assert(aerortos_mutex_lock(&mutex) == AERORTOS_MUTEX_ERROR_CONTEXT);
    assert(aerortos_mutex_unlock(&mutex) == AERORTOS_MUTEX_ERROR_CONTEXT);
    aerortos_host_handler_mode = false;
    assert(aerortos_scheduler_make_ready(owner) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_mutex_unlock(&mutex) == AERORTOS_MUTEX_OK);
    assert(mutex.owner == NULL);
    assert(owner->owned_mutex_count == 0U);
    assert(aerortos_mutex_invariants_hold(&mutex));
}

static void classic_block_hook(void)
{
    assert(aerortos_scheduler_current() == classic_high);
    assert(classic_high->state == AERORTOS_TASK_BLOCKED);
    assert(classic_mutex->wait_head == classic_high);
    assert(classic_low->effective_priority == classic_high->base_priority);
    dispatch_pending();
    assert(aerortos_scheduler_current() == classic_low);
    assert(aerortos_mutex_unlock(classic_mutex) == AERORTOS_MUTEX_OK);
    assert(classic_low->effective_priority == classic_low->base_priority);
    assert(classic_mutex->owner == classic_high);
    dispatch_pending();
    assert(aerortos_scheduler_current() == classic_high);
}

static void test_classic_priority_inheritance(void)
{
    alignas(8) uint32_t stacks[4][TEST_STACK_WORDS];
    aerortos_mutex_t mutex = {0};
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *medium;
    aerortos_task_t *low;

    reset_host();
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    high = create_task(stacks[1], 2U);
    medium = create_task(stacks[2], 5U);
    low = create_task(stacks[3], 10U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(medium) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    block_and_dispatch();
    block_and_dispatch();
    assert(aerortos_scheduler_current() == low);
    assert(aerortos_mutex_init(&mutex) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_lock(&mutex) == AERORTOS_MUTEX_OK);
    assert(aerortos_scheduler_make_ready(medium) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_make_ready(high) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == high);

    classic_mutex = &mutex;
    classic_high = high;
    classic_low = low;
    aerortos_host_mutex_block_callback = classic_block_hook;
    assert(aerortos_mutex_lock(&mutex) == AERORTOS_MUTEX_OK);
    assert(mutex.owner == high);
    assert(low->effective_priority == low->base_priority);
    assert(medium->state == AERORTOS_TASK_READY);
    assert(aerortos_mutex_unlock(&mutex) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_invariants_hold(&mutex));
}

static void test_ownership_capacity(void)
{
    alignas(8) uint32_t stacks[2][TEST_STACK_WORDS];
    aerortos_mutex_t mutexes[AERORTOS_MUTEX_MAX_OWNED + 1U];
    aerortos_task_t *idle;
    aerortos_task_t *owner;
    size_t index;

    reset_host();
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    owner = create_task(stacks[1], 4U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(owner) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    for (index = 0U; index < (AERORTOS_MUTEX_MAX_OWNED + 1U); ++index) {
        assert(aerortos_mutex_init(&mutexes[index]) == AERORTOS_MUTEX_OK);
    }
    for (index = 0U; index < AERORTOS_MUTEX_MAX_OWNED; ++index) {
        assert(aerortos_mutex_lock(&mutexes[index]) == AERORTOS_MUTEX_OK);
    }
    assert(owner->owned_mutex_count == AERORTOS_MUTEX_MAX_OWNED);
    assert(aerortos_mutex_try_lock(&mutexes[AERORTOS_MUTEX_MAX_OWNED]) ==
           AERORTOS_MUTEX_ERROR_CAPACITY);
    for (index = AERORTOS_MUTEX_MAX_OWNED; index > 0U; --index) {
        assert(aerortos_mutex_unlock(&mutexes[index - 1U]) ==
               AERORTOS_MUTEX_OK);
    }
    assert(owner->owned_mutex_count == 0U);
}

static void test_equal_priority_waiter_fifo(void)
{
    alignas(8) static uint32_t stacks[4][TEST_STACK_WORDS];
    static aerortos_mutex_t mutex;
    aerortos_task_t *idle;
    aerortos_task_t *first;
    aerortos_task_t *second;
    aerortos_task_t *owner;

    reset_host();
    mutex = (aerortos_mutex_t){0};
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    first = create_task(stacks[1], 5U);
    second = create_task(stacks[2], 5U);
    owner = create_task(stacks[3], 10U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(owner) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(first) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(second) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    block_and_dispatch();
    block_and_dispatch();
    assert(aerortos_scheduler_current() == owner);
    assert(aerortos_mutex_init(&mutex) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_lock(&mutex) == AERORTOS_MUTEX_OK);
    aerortos_host_mutex_block_callback = escape_mutex_block;

    assert(aerortos_scheduler_make_ready(first) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&mutex);
        assert(false);
    }
    dispatch_pending();
    assert(aerortos_scheduler_current() == owner);
    assert(aerortos_scheduler_make_ready(second) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_yield() == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == second);
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&mutex);
        assert(false);
    }
    dispatch_pending();
    assert(aerortos_scheduler_current() == owner);
    assert(mutex.wait_head == first);
    assert(mutex.wait_tail == second);
    assert(mutex.waiter_count == 2U);
    assert(aerortos_mutex_invariants_hold(&mutex));
    assert(aerortos_mutex_unlock(&mutex) == AERORTOS_MUTEX_OK);
    assert(mutex.owner == first);
}

static void test_multiple_owned_mutex_restoration(void)
{
    alignas(8) static uint32_t stacks[4][TEST_STACK_WORDS];
    static aerortos_mutex_t first;
    static aerortos_mutex_t second;
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *medium;
    aerortos_task_t *low;

    reset_host();
    first = (aerortos_mutex_t){0};
    second = (aerortos_mutex_t){0};
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    high = create_task(stacks[1], 2U);
    medium = create_task(stacks[2], 5U);
    low = create_task(stacks[3], 10U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(medium) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    block_and_dispatch();
    block_and_dispatch();
    assert(aerortos_mutex_init(&first) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_init(&second) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_lock(&first) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_lock(&second) == AERORTOS_MUTEX_OK);
    assert(low->owned_mutex_count == 2U);

    assert(aerortos_scheduler_make_ready(medium) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    aerortos_host_mutex_block_callback = escape_mutex_block;
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&second);
        assert(false);
    }
    dispatch_pending();
    assert(low->effective_priority == 5U);

    assert(aerortos_scheduler_make_ready(high) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&first);
        assert(false);
    }
    dispatch_pending();
    assert(aerortos_scheduler_current() == low);
    assert(low->effective_priority == 2U);
    assert(aerortos_mutex_unlock(&first) == AERORTOS_MUTEX_OK);
    assert(low->effective_priority == 5U);
    dispatch_pending();
    assert(aerortos_scheduler_current() == high);
    assert(aerortos_mutex_unlock(&first) == AERORTOS_MUTEX_OK);
    block_and_dispatch();
    assert(aerortos_scheduler_current() == low);
    assert(aerortos_mutex_unlock(&second) == AERORTOS_MUTEX_OK);
    assert(low->effective_priority == low->base_priority);
    dispatch_pending();
    assert(aerortos_scheduler_current() == medium);
    assert(aerortos_mutex_unlock(&second) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_invariants_hold(&first));
    assert(aerortos_mutex_invariants_hold(&second));
}

static void test_transitive_inheritance(void)
{
    alignas(8) static uint32_t stacks[4][TEST_STACK_WORDS];
    static aerortos_mutex_t first;
    static aerortos_mutex_t second;
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *middle;
    aerortos_task_t *low;

    reset_host();
    first = (aerortos_mutex_t){0};
    second = (aerortos_mutex_t){0};
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    high = create_task(stacks[1], 2U);
    middle = create_task(stacks[2], 6U);
    low = create_task(stacks[3], 10U);
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
    assert(aerortos_scheduler_current() == low);
    aerortos_host_mutex_block_callback = escape_mutex_block;
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&second);
        assert(false);
    }
    dispatch_pending();
    assert(aerortos_scheduler_current() != low);
    assert(aerortos_scheduler_make_ready(middle) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_make_ready(high) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&first);
        assert(false);
    }
    dispatch_pending();
    assert(aerortos_scheduler_current() == middle);
    assert(low->effective_priority == 2U);
    assert(middle->effective_priority == 2U);
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

static void test_queue_waiter_reordering(void)
{
    alignas(8) static uint32_t stacks[5][TEST_STACK_WORDS];
    static aerortos_mutex_t mutex;
    static aerortos_queue_t queue;
    static uint32_t storage[1];
    uint32_t value = 123U;
    uint32_t output = 0U;
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *medium;
    aerortos_task_t *low;
    aerortos_task_t *producer;

    reset_host();
    mutex = (aerortos_mutex_t){0};
    queue = (aerortos_queue_t){0};
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    high = create_task(stacks[1], 2U);
    medium = create_task(stacks[2], 5U);
    low = create_task(stacks[3], 10U);
    producer = create_task(stacks[4], 15U);
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
    assert(aerortos_scheduler_current() == producer);
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
    assert(aerortos_scheduler_current() == producer);
    assert(low->effective_priority == high->base_priority);
    assert(queue.receive_wait_head == low);
    assert(queue.receive_wait_tail == medium);
    assert(aerortos_queue_invariants_hold(&queue));
    assert(aerortos_queue_try_send(&queue, &value) == AERORTOS_QUEUE_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == low);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == value);
    assert(aerortos_mutex_unlock(&mutex) == AERORTOS_MUTEX_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == high);
    assert(aerortos_mutex_unlock(&mutex) == AERORTOS_MUTEX_OK);
    value = 456U;
    assert(aerortos_queue_try_send(&queue, &value) == AERORTOS_QUEUE_OK);
    block_and_dispatch();
    assert(aerortos_scheduler_current() == medium);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == value);
    assert(aerortos_queue_invariants_hold(&queue));
    assert(aerortos_mutex_invariants_hold(&mutex));
}

static void test_deadlock_detection(void)
{
    alignas(8) static uint32_t stacks[3][TEST_STACK_WORDS];
    static aerortos_mutex_t first;
    static aerortos_mutex_t second;
    aerortos_task_t *idle;
    aerortos_task_t *first_task;
    aerortos_task_t *second_task;

    reset_host();
    first = (aerortos_mutex_t){0};
    second = (aerortos_mutex_t){0};
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    first_task = create_task(stacks[1], 5U);
    second_task = create_task(stacks[2], 10U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(second_task) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(first_task) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_mutex_init(&first) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_init(&second) == AERORTOS_MUTEX_OK);
    assert(aerortos_mutex_lock(&first) == AERORTOS_MUTEX_OK);
    block_and_dispatch();
    assert(aerortos_mutex_lock(&second) == AERORTOS_MUTEX_OK);
    assert(aerortos_scheduler_make_ready(first_task) == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    aerortos_host_mutex_block_callback = escape_mutex_block;
    if (setjmp(block_escape) == 0) {
        (void)aerortos_mutex_lock(&second);
        assert(false);
    }
    dispatch_pending();
    assert(aerortos_scheduler_current() == second_task);
    assert(aerortos_mutex_lock(&first) == AERORTOS_MUTEX_ERROR_DEADLOCK);
    assert(aerortos_mutex_invariants_hold(&first));
    assert(aerortos_mutex_invariants_hold(&second));
}

int main(void)
{
    test_ownership_and_invalid_operations();
    test_classic_priority_inheritance();
    test_ownership_capacity();
    test_equal_priority_waiter_fifo();
    test_multiple_owned_mutex_restoration();
    test_transitive_inheritance();
    test_queue_waiter_reordering();
    test_deadlock_detection();
    puts("mutex_test: PASS");
    return 0;
}
