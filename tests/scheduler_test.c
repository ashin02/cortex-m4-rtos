#include <assert.h>
#include <stdbool.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "aerortos/critical_section.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"

#define TEST_STACK_WORDS 64U

extern uint32_t aerortos_host_basepri;
extern uint32_t aerortos_host_pendsv_requests;
extern bool aerortos_host_handler_mode;

static void test_entry(void *argument) { (void)argument; }

static aerortos_task_t *create_task(uint32_t *stack, uint8_t priority)
{
    const aerortos_task_config_t config = {
        .entry = test_entry, .argument = NULL, .stack = stack,
        .stack_size_bytes = TEST_STACK_WORDS * sizeof(uint32_t),
        .priority = priority
    };
    aerortos_task_t *task = NULL;

    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_OK);
    return task;
}

static void dispatch_pending(void)
{
    assert(aerortos_host_pendsv_requests != 0U);
    aerortos_host_pendsv_requests = 0U;
    assert(aerortos_scheduler_switch_context() != NULL);
    assert(aerortos_scheduler_invariants_hold());
}

static void test_priority_fifo_block_and_wakeup(void)
{
    alignas(8) uint32_t stacks[5][TEST_STACK_WORDS];
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *equal_a;
    aerortos_task_t *equal_b;
    aerortos_task_t *low;

    aerortos_task_system_init();
    aerortos_host_basepri = 0U;
    aerortos_host_pendsv_requests = 0U;
    aerortos_host_handler_mode = false;
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    high = create_task(stacks[1], 2U);
    equal_a = create_task(stacks[2], 5U);
    equal_b = create_task(stacks[3], 5U);
    low = create_task(stacks[4], 20U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(equal_a) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(equal_b) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_ERROR_DUPLICATE);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_scheduler_current() == high);

    assert(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == equal_a);
    assert(aerortos_scheduler_yield() == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == equal_b);
    assert(equal_a->state == AERORTOS_TASK_READY);

    assert(aerortos_scheduler_make_ready(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_host_pendsv_requests == 1U);
    dispatch_pending();
    assert(aerortos_scheduler_current() == high);
    assert(equal_b->state == AERORTOS_TASK_READY);
    assert(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == equal_a);
}

static void test_idle_and_nested_basepri(void)
{
    alignas(8) uint32_t stacks[2][TEST_STACK_WORDS];
    aerortos_task_t *idle;
    aerortos_task_t *worker;
    aerortos_critical_state_t outer;
    aerortos_critical_state_t inner;

    aerortos_task_system_init();
    aerortos_host_basepri = 0U;
    aerortos_host_pendsv_requests = 0U;
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    worker = create_task(stacks[1], 8U);
    worker->state = AERORTOS_TASK_BLOCKED;
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_scheduler_current() == idle);

    outer = aerortos_critical_enter();
    assert(outer == 0U);
    inner = aerortos_critical_enter();
    assert(inner == AERORTOS_KERNEL_BASEPRI);
    assert(aerortos_scheduler_make_ready(worker) == AERORTOS_SCHEDULER_OK);
    aerortos_critical_exit(inner);
    assert(aerortos_host_basepri == AERORTOS_KERNEL_BASEPRI);
    aerortos_critical_exit(outer);
    assert(aerortos_host_basepri == 0U);
    dispatch_pending();
    assert(aerortos_scheduler_current() == worker);
    assert(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == idle);
}

static void test_handler_boundaries(void)
{
    aerortos_host_handler_mode = true;
    assert(aerortos_scheduler_yield() == AERORTOS_SCHEDULER_ERROR_STATE);
    assert(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_ERROR_STATE);
    aerortos_host_handler_mode = false;
}

static void test_basepri_prior_state_semantics(void)
{
    aerortos_critical_state_t saved;

    aerortos_host_basepri = UINT32_C(0x20);
    saved = aerortos_critical_enter();
    assert(saved == UINT32_C(0x20));
    assert(aerortos_host_basepri == UINT32_C(0x20));
    aerortos_critical_exit(saved);
    assert(aerortos_host_basepri == UINT32_C(0x20));

    aerortos_host_basepri = UINT32_C(0x80);
    saved = aerortos_critical_enter();
    assert(saved == UINT32_C(0x80));
    assert(aerortos_host_basepri == AERORTOS_KERNEL_BASEPRI);
    aerortos_critical_exit(saved);
    assert(aerortos_host_basepri == UINT32_C(0x80));
    aerortos_host_basepri = 0U;
}

int main(void)
{
    test_priority_fifo_block_and_wakeup();
    test_idle_and_nested_basepri();
    test_handler_boundaries();
    test_basepri_prior_state_semantics();
    puts("scheduler_test: PASS");
    return 0;
}
