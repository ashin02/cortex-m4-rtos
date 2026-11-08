#include <assert.h>
#include <stdbool.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"

#define TEST_STACK_WORDS 96U

extern uint32_t aerortos_host_pendsv_requests;
extern bool aerortos_host_handler_mode;

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

static void reset_system(uint32_t initial_tick)
{
    aerortos_task_system_init();
    aerortos_time_system_init(initial_tick);
    aerortos_host_pendsv_requests = 0U;
    aerortos_host_handler_mode = false;
}

static void run_tick(void)
{
    aerortos_host_handler_mode = true;
    aerortos_timer_tick_isr();
    aerortos_host_handler_mode = false;
}

static void dispatch_pending(void)
{
    assert(aerortos_host_pendsv_requests != 0U);
    aerortos_host_pendsv_requests = 0U;
    assert(aerortos_scheduler_switch_context() != NULL);
    assert(aerortos_scheduler_invariants_hold());
}

static void complete_and_dispatch(void)
{
    assert(aerortos_periodic_wait_next() == AERORTOS_TIMER_OK);
    dispatch_pending();
}

static void test_periodic_release_preemption_and_phase(void)
{
    alignas(8) uint32_t stacks[4][TEST_STACK_WORDS];
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *peer;
    aerortos_task_t *low;

    reset_system(0U);
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    high = create_task(stacks[1], 2U, 5U, 4U, 2U);
    peer = create_task(stacks[2], 4U, 5U, 5U, 2U);
    low = create_task(stacks[3], 10U, 20U, 20U, 0U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(peer) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_scheduler_current() == low);

    run_tick();
    assert(aerortos_tick_now() == 1U);
    run_tick();
    assert(high->release_count == 1U);
    assert(peer->release_count == 1U);
    assert(high->release_tick == 2U);
    assert(peer->release_tick == 2U);
    dispatch_pending();
    assert(aerortos_scheduler_current() == high);
    assert(high->last_release_jitter_ticks == 0);
    complete_and_dispatch();
    assert(aerortos_scheduler_current() == peer);
    complete_and_dispatch();
    assert(aerortos_scheduler_current() == low);
    complete_and_dispatch();
    assert(aerortos_scheduler_current() == idle);

    while (aerortos_tick_now() < 7U) {
        run_tick();
    }
    assert(high->release_count == 2U);
    assert(peer->release_count == 2U);
    assert(high->release_tick == 7U);
    assert(high->next_release_tick == 12U);
    dispatch_pending();
    assert(aerortos_scheduler_current() == high);
    assert(aerortos_scheduler_invariants_hold());
}

static void test_deadline_miss_overrun_and_recovery(void)
{
    alignas(8) uint32_t stacks[2][TEST_STACK_WORDS];
    aerortos_task_t *idle;
    aerortos_task_t *periodic;

    reset_system(0U);
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    periodic = create_task(stacks[1], 3U, 4U, 2U, 0U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(periodic) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_scheduler_current() == periodic);
    assert(periodic->absolute_deadline == 2U);

    run_tick();
    run_tick();
    assert(periodic->deadline_miss_count == 0U);
    run_tick();
    assert(periodic->deadline_miss_count == 1U);
    run_tick();
    assert(periodic->overrun_count == 1U);
    assert(periodic->skipped_release_count == 1U);
    assert(periodic->next_release_tick == 8U);
    run_tick();
    complete_and_dispatch();
    assert(periodic->completion_count == 1U);
    assert(periodic->deadline_miss_count == 1U);
    assert(periodic->last_job_execution_ticks == 5U);
    assert(aerortos_scheduler_current() == idle);

    while (aerortos_tick_now() < 8U) {
        run_tick();
    }
    assert(periodic->release_count == 2U);
    assert(periodic->release_tick == 8U);
    assert(periodic->absolute_deadline == 10U);
    dispatch_pending();
    complete_and_dispatch();
    assert(periodic->completion_count == 2U);
    assert(periodic->deadline_miss_count == 1U);

    while (aerortos_tick_now() < 12U) {
        run_tick();
    }
    dispatch_pending();
    complete_and_dispatch();
    assert(periodic->release_count == 3U);
    assert(periodic->timing_trace_count == AERORTOS_TIMING_TRACE_CAPACITY);
    assert(periodic->timing_trace_write < AERORTOS_TIMING_TRACE_CAPACITY);
}

static void test_wraparound(void)
{
    alignas(8) uint32_t stacks[2][TEST_STACK_WORDS];
    aerortos_task_t *idle;
    aerortos_task_t *periodic;

    reset_system(UINT32_MAX - 2U);
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    periodic = create_task(stacks[1], 3U, 4U, 3U, 2U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(periodic) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_scheduler_current() == idle);

    run_tick();
    assert(periodic->release_count == 0U);
    run_tick();
    assert(aerortos_tick_now() == UINT32_MAX);
    assert(periodic->release_count == 1U);
    assert(periodic->release_tick == UINT32_MAX);
    assert(periodic->absolute_deadline == 2U);
    assert(periodic->next_release_tick == 3U);
    dispatch_pending();
    complete_and_dispatch();
    run_tick();
    run_tick();
    run_tick();
    run_tick();
    assert(aerortos_tick_now() == 3U);
    assert(periodic->release_count == 2U);
    assert(periodic->release_tick == 3U);
    assert(aerortos_tick_after(3U, UINT32_MAX));
}

static void test_dispatch_jitter(void)
{
    alignas(8) uint32_t stacks[3][TEST_STACK_WORDS];
    aerortos_task_t *idle;
    aerortos_task_t *blocker;
    aerortos_task_t *periodic;

    reset_system(0U);
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    blocker = create_task(stacks[1], 1U, 0U, 0U, 0U);
    periodic = create_task(stacks[2], 8U, 10U, 8U, 2U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(periodic) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(blocker) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_scheduler_current() == blocker);
    run_tick();
    run_tick();
    run_tick();
    run_tick();
    assert(periodic->release_tick == 2U);
    assert(periodic->job_started == 0U);
    assert(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);
    dispatch_pending();
    assert(aerortos_scheduler_current() == periodic);
    assert(periodic->last_release_jitter_ticks == 2);
    assert(periodic->max_release_jitter_ticks == 2);
}

int main(void)
{
    test_periodic_release_preemption_and_phase();
    test_deadline_miss_overrun_and_recovery();
    test_wraparound();
    test_dispatch_jitter();
    puts("deadline_test: PASS");
    return 0;
}
