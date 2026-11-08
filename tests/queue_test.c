#include <assert.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "aerortos/queue.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"

#define TEST_STACK_WORDS 96U

extern uint32_t aerortos_host_basepri;
extern uint32_t aerortos_host_pendsv_requests;
extern bool aerortos_host_handler_mode;
extern void (*aerortos_host_queue_block_callback)(void);

static aerortos_queue_t *hook_queue;
static aerortos_task_t *hook_blocked_task;
static aerortos_task_t *hook_helper_task;
static uint32_t hook_value;
static jmp_buf block_escape;

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
}

static void dispatch_pending(void)
{
    assert(aerortos_host_pendsv_requests != 0U);
    aerortos_host_pendsv_requests = 0U;
    assert(aerortos_scheduler_switch_context() != NULL);
    assert(aerortos_scheduler_invariants_hold());
}

static void test_invalid_arguments_and_context(void)
{
    aerortos_queue_t queue = {0};
    uint8_t storage[8];
    uint32_t value = 1U;
    size_t count = 99U;

    reset_host();
    assert(aerortos_queue_init(NULL, storage, 1U, sizeof(value)) ==
           AERORTOS_QUEUE_ERROR_NULL);
    assert(aerortos_queue_init(&queue, NULL, 1U, sizeof(value)) ==
           AERORTOS_QUEUE_ERROR_NULL);
    assert(aerortos_queue_init(&queue, storage, 0U, sizeof(value)) ==
           AERORTOS_QUEUE_ERROR_CAPACITY);
    assert(aerortos_queue_init(&queue, storage, 1U, 0U) ==
           AERORTOS_QUEUE_ERROR_ELEMENT_SIZE);
    assert(aerortos_queue_init(&queue, storage, SIZE_MAX, 2U) ==
           AERORTOS_QUEUE_ERROR_STORAGE_SIZE);
    assert(aerortos_queue_try_send(&queue, &value) ==
           AERORTOS_QUEUE_ERROR_STATE);
    assert(aerortos_queue_try_send(NULL, &value) == AERORTOS_QUEUE_ERROR_NULL);
    assert(aerortos_queue_try_send(&queue, NULL) == AERORTOS_QUEUE_ERROR_NULL);
    assert(aerortos_queue_try_receive(NULL, &value) ==
           AERORTOS_QUEUE_ERROR_NULL);
    assert(aerortos_queue_try_receive(&queue, NULL) ==
           AERORTOS_QUEUE_ERROR_NULL);
    assert(aerortos_queue_count(NULL, &count) == AERORTOS_QUEUE_ERROR_NULL);
    assert(aerortos_queue_count(&queue, NULL) == AERORTOS_QUEUE_ERROR_NULL);

    assert(aerortos_queue_init(&queue, storage, 2U, sizeof(value)) ==
           AERORTOS_QUEUE_OK);
    aerortos_host_handler_mode = true;
    assert(aerortos_queue_send(&queue, &value) ==
           AERORTOS_QUEUE_ERROR_CONTEXT);
    assert(aerortos_queue_receive(&queue, &value) ==
           AERORTOS_QUEUE_ERROR_CONTEXT);
    assert(aerortos_queue_try_send(&queue, &value) == AERORTOS_QUEUE_OK);
    assert(aerortos_queue_try_receive(&queue, &value) == AERORTOS_QUEUE_OK);
    aerortos_host_handler_mode = false;
    assert(aerortos_queue_invariants_hold(&queue));
}

static void test_fifo_capacity_and_wrap(void)
{
    aerortos_queue_t queue = {0};
    uint32_t storage[3];
    uint32_t output;
    uint32_t values[] = {11U, 22U, 33U, 44U};
    size_t count;

    reset_host();
    assert(aerortos_queue_init(&queue, storage, 3U, sizeof(storage[0])) ==
           AERORTOS_QUEUE_OK);
    assert(aerortos_queue_try_receive(&queue, &output) ==
           AERORTOS_QUEUE_ERROR_EMPTY);
    assert(aerortos_queue_try_send(&queue, &values[0]) == AERORTOS_QUEUE_OK);
    assert(aerortos_queue_try_send(&queue, &values[1]) == AERORTOS_QUEUE_OK);
    assert(aerortos_queue_try_send(&queue, &values[2]) == AERORTOS_QUEUE_OK);
    assert(aerortos_queue_try_send(&queue, &values[3]) ==
           AERORTOS_QUEUE_ERROR_FULL);
    assert(aerortos_queue_count(&queue, &count) == AERORTOS_QUEUE_OK);
    assert(count == 3U);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == values[0]);
    assert(aerortos_queue_try_send(&queue, &values[3]) == AERORTOS_QUEUE_OK);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == values[1]);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == values[2]);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == values[3]);
    assert(aerortos_queue_try_receive(&queue, &output) ==
           AERORTOS_QUEUE_ERROR_EMPTY);
    assert(aerortos_queue_invariants_hold(&queue));

    assert(aerortos_queue_init(&queue, storage, 1U, sizeof(storage[0])) ==
           AERORTOS_QUEUE_OK);
    assert(aerortos_queue_try_send(&queue, &values[0]) == AERORTOS_QUEUE_OK);
    assert(aerortos_queue_try_send(&queue, &values[1]) ==
           AERORTOS_QUEUE_ERROR_FULL);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == values[0]);
}

static void receive_block_hook(void)
{
    assert(aerortos_scheduler_current() == hook_blocked_task);
    assert(hook_blocked_task->state == AERORTOS_TASK_BLOCKED);
    assert(hook_blocked_task->wait_object == hook_queue);
    assert(hook_queue->waiting_receivers == 1U);
    assert(aerortos_queue_invariants_hold(hook_queue));
    dispatch_pending();
    assert(aerortos_scheduler_current() == hook_helper_task);
    assert(aerortos_queue_try_send(hook_queue, &hook_value) ==
           AERORTOS_QUEUE_OK);
    assert(hook_blocked_task->state == AERORTOS_TASK_READY);
    assert(hook_blocked_task->wait_object == NULL);
    assert(hook_queue->waiting_receivers == 0U);
    assert(aerortos_host_pendsv_requests != 0U);
    dispatch_pending();
    assert(aerortos_scheduler_current() == hook_blocked_task);
}

static void send_block_hook(void)
{
    uint32_t output = 0U;

    assert(aerortos_scheduler_current() == hook_blocked_task);
    assert(hook_blocked_task->state == AERORTOS_TASK_BLOCKED);
    assert(hook_queue->waiting_senders == 1U);
    assert(aerortos_queue_invariants_hold(hook_queue));
    dispatch_pending();
    assert(aerortos_scheduler_current() == hook_helper_task);
    assert(aerortos_queue_try_receive(hook_queue, &output) ==
           AERORTOS_QUEUE_OK);
    assert(output == 77U);
    assert(hook_blocked_task->state == AERORTOS_TASK_READY);
    assert(aerortos_host_pendsv_requests != 0U);
    dispatch_pending();
    assert(aerortos_scheduler_current() == hook_blocked_task);
}

static void escape_block_hook(void)
{
    longjmp(block_escape, 1);
}

static void test_blocking_wakeup_and_priority_preemption(void)
{
    alignas(8) uint32_t stacks[3][TEST_STACK_WORDS];
    aerortos_queue_t queue = {0};
    uint32_t storage[1];
    uint32_t output = 0U;
    uint32_t initial = 77U;
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *low;

    reset_host();
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    high = create_task(stacks[1], 2U);
    low = create_task(stacks[2], 12U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_scheduler_current() == high);
    assert(aerortos_queue_init(&queue, storage, 1U, sizeof(storage[0])) ==
           AERORTOS_QUEUE_OK);

    hook_queue = &queue;
    hook_blocked_task = high;
    hook_helper_task = low;
    hook_value = 1234U;
    aerortos_host_queue_block_callback = receive_block_hook;
    assert(aerortos_queue_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == hook_value);
    assert(aerortos_scheduler_current() == high);
    assert(aerortos_queue_invariants_hold(&queue));

    assert(aerortos_queue_try_send(&queue, &initial) == AERORTOS_QUEUE_OK);
    hook_value = 4321U;
    aerortos_host_queue_block_callback = send_block_hook;
    assert(aerortos_queue_send(&queue, &hook_value) == AERORTOS_QUEUE_OK);
    assert(aerortos_scheduler_current() == high);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == hook_value);
    assert(queue.waiting_senders == 0U);
    assert(queue.waiting_receivers == 0U);
    assert(aerortos_queue_invariants_hold(&queue));
    aerortos_host_queue_block_callback = NULL;
}

static void test_interleaved_tasks_and_fifo(void)
{
    alignas(8) uint32_t stacks[3][TEST_STACK_WORDS];
    aerortos_queue_t queue = {0};
    uint16_t storage[4];
    uint16_t output;
    uint16_t first = 10U;
    uint16_t second = 20U;
    uint16_t third = 30U;
    aerortos_task_t *idle;
    aerortos_task_t *task_a;
    aerortos_task_t *task_b;

    reset_host();
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    task_a = create_task(stacks[1], 6U);
    task_b = create_task(stacks[2], 6U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(task_a) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(task_b) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_queue_init(&queue, storage, 4U, sizeof(storage[0])) ==
           AERORTOS_QUEUE_OK);

    assert(aerortos_scheduler_current() == task_a);
    assert(aerortos_queue_try_send(&queue, &first) == AERORTOS_QUEUE_OK);
    assert(aerortos_scheduler_yield() == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == task_b);
    assert(aerortos_queue_try_send(&queue, &second) == AERORTOS_QUEUE_OK);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == first);
    assert(aerortos_scheduler_yield() == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == task_a);
    assert(aerortos_queue_try_send(&queue, &third) == AERORTOS_QUEUE_OK);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == second);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == third);
    assert(aerortos_queue_invariants_hold(&queue));
}

static void test_multiple_receiver_priority_order(void)
{
    alignas(8) static uint32_t stacks[4][TEST_STACK_WORDS];
    static aerortos_queue_t queue;
    static uint32_t storage[1];
    uint32_t output = 0U;
    uint32_t first = 91U;
    uint32_t second = 92U;
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *medium;
    aerortos_task_t *low;

    reset_host();
    queue = (aerortos_queue_t){0};
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    high = create_task(stacks[1], 2U);
    medium = create_task(stacks[2], 5U);
    low = create_task(stacks[3], 12U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(medium) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_queue_init(&queue, storage, 1U, sizeof(storage[0])) ==
           AERORTOS_QUEUE_OK);
    aerortos_host_queue_block_callback = escape_block_hook;

    if (setjmp(block_escape) == 0) {
        (void)aerortos_queue_receive(&queue, &output);
        assert(false);
    }
    assert(high->state == AERORTOS_TASK_BLOCKED);
    dispatch_pending();
    assert(aerortos_scheduler_current() == medium);
    if (setjmp(block_escape) == 0) {
        (void)aerortos_queue_receive(&queue, &output);
        assert(false);
    }
    assert(medium->state == AERORTOS_TASK_BLOCKED);
    dispatch_pending();
    assert(aerortos_scheduler_current() == low);
    assert(queue.waiting_receivers == 2U);
    assert(queue.receive_wait_head == high);
    assert(queue.receive_wait_tail == medium);
    assert(aerortos_queue_invariants_hold(&queue));

    assert(aerortos_queue_try_send(&queue, &first) == AERORTOS_QUEUE_OK);
    assert(high->state == AERORTOS_TASK_READY);
    assert(medium->state == AERORTOS_TASK_BLOCKED);
    dispatch_pending();
    assert(aerortos_scheduler_current() == high);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == first);
    assert(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == low);

    assert(aerortos_queue_try_send(&queue, &second) == AERORTOS_QUEUE_OK);
    assert(medium->state == AERORTOS_TASK_READY);
    dispatch_pending();
    assert(aerortos_scheduler_current() == medium);
    assert(aerortos_queue_try_receive(&queue, &output) == AERORTOS_QUEUE_OK);
    assert(output == second);
    assert(queue.waiting_receivers == 0U);
    assert(aerortos_queue_invariants_hold(&queue));
    aerortos_host_queue_block_callback = NULL;
}

int main(void)
{
    test_invalid_arguments_and_context();
    test_fifo_capacity_and_wrap();
    test_blocking_wakeup_and_priority_preemption();
    test_interleaved_tasks_and_fifo();
    test_multiple_receiver_priority_order();
    puts("queue_test: PASS");
    return 0;
}
