#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "aerortos/scheduler.h"
#include "aerortos/task.h"

#define TEST_STACK_WORDS 64U
#define SWITCH_COUNT 1000U

extern uint32_t aerortos_host_pendsv_requests;

static void test_entry(void *argument) { (void)argument; }

static aerortos_task_t *create_task(uint32_t *stack, uint8_t priority)
{
    const aerortos_task_config_t config = {
        .entry = test_entry, .argument = stack, .stack = stack,
        .stack_size_bytes = TEST_STACK_WORDS * sizeof(uint32_t),
        .priority = priority
    };
    aerortos_task_t *task = NULL;

    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_OK);
    return task;
}

int main(void)
{
    alignas(8) uint32_t stacks[3][TEST_STACK_WORDS];
    aerortos_task_t *idle;
    aerortos_task_t *first;
    aerortos_task_t *second;
    uint32_t *first_psp;
    uint32_t *second_psp;
    size_t index;

    aerortos_task_system_init();
    aerortos_host_pendsv_requests = 0U;
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    first = create_task(stacks[1], 4U);
    second = create_task(stacks[2], 4U);
    first_psp = first->saved_psp;
    second_psp = second->saved_psp;
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(first) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(second) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());

    for (index = 0U; index < SWITCH_COUNT; ++index) {
        aerortos_task_t *expected = ((index & 1U) == 0U) ? second : first;

        assert(aerortos_scheduler_yield() == AERORTOS_SCHEDULER_OK);
        aerortos_host_pendsv_requests = 0U;
        assert(aerortos_scheduler_switch_context() == expected);
        assert(aerortos_scheduler_invariants_hold());
        assert(first->saved_psp == first_psp);
        assert(second->saved_psp == second_psp);
        assert(aerortos_task_stack_guard_intact(first));
        assert(aerortos_task_stack_guard_intact(second));
    }
    puts("context_switch_test: PASS (portable handoff; ARM path unverified)");
    return 0;
}
