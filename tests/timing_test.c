#include <assert.h>
#include <stdbool.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>

#include "aerortos/critical_section.h"
#include "aerortos/cycle_counter.h"
#include "aerortos/monitor.h"
#include "aerortos/platform.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"

#define TEST_STACK_WORDS 96U
#define TEST_CLOCK_HZ UINT32_C(16000000)

extern bool aerortos_host_dwt_available;
extern uint32_t aerortos_host_cycle_counter;
extern uint32_t aerortos_host_cycle_counter_step;
extern uint32_t aerortos_host_pendsv_requests;

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

static void test_counter_and_conversion(void)
{
    uint64_t microseconds = 0U;
    uint32_t first;
    uint32_t second;

    SystemCoreClock = TEST_CLOCK_HZ;
    aerortos_host_dwt_available = true;
    aerortos_host_cycle_counter = 100U;
    aerortos_host_cycle_counter_step = 7U;
    assert(!aerortos_cycle_counter_init(TEST_CLOCK_HZ / 2U));
    aerortos_host_dwt_available = false;
    assert(!aerortos_cycle_counter_init(TEST_CLOCK_HZ));
    aerortos_host_dwt_available = true;
    assert(aerortos_cycle_counter_init(TEST_CLOCK_HZ));
    first = aerortos_cycle_counter_now();
    second = aerortos_cycle_counter_now();
    assert(second > first);
    assert(aerortos_cycle_counter_elapsed(UINT32_MAX - 3U, 4U) == 8U);
    assert(aerortos_cycles_to_microseconds(16000U, TEST_CLOCK_HZ,
                                           &microseconds));
    assert(microseconds == 1000U);
    assert(!aerortos_cycles_to_microseconds(1U, 0U, &microseconds));
    assert(!aerortos_cycles_to_microseconds(1U, TEST_CLOCK_HZ, NULL));
}

static void test_statistics_and_bounds(void)
{
    aerortos_measurement_series_t series;
    uint32_t index;

    aerortos_measurement_series_init(&series);
    assert(series.observation_count == 0U);
    assert(series.retained_count == 0U);
    assert(series.dropped_count == 0U);
    assert(series.minimum_cycles == UINT32_MAX);
    assert(aerortos_measurement_record(&series, 5U));
    assert(aerortos_measurement_record(&series, 1U));
    assert(aerortos_measurement_record(&series, 3U));
    assert(series.minimum_cycles == 1U);
    assert(series.maximum_cycles == 5U);
    assert(aerortos_measurement_mean_cycles(&series) == 3U);
    aerortos_measurement_series_init(&series);
    for (index = 0U; index < AERORTOS_MEASUREMENT_CAPACITY; ++index) {
        assert(aerortos_measurement_record(&series, index));
    }
    assert(!aerortos_measurement_record(&series, 99U));
    assert(series.observation_count == AERORTOS_MEASUREMENT_CAPACITY + 1U);
    assert(series.retained_count == AERORTOS_MEASUREMENT_CAPACITY);
    assert(series.dropped_count == 1U);
    assert(series.total_cycles == UINT64_C(2115));
    assert(series.minimum_cycles == 0U);
    assert(series.maximum_cycles == 99U);
    assert(aerortos_measurement_mean_cycles(&series) == 32U);

    series.total_cycles = UINT64_MAX - 2U;
    series.observation_count = UINT32_MAX;
    series.dropped_count = UINT32_MAX;
    assert(!aerortos_measurement_record(&series, 5U));
    assert(series.total_cycles == UINT64_MAX);
    assert(series.observation_count == UINT32_MAX);
    assert(series.dropped_count == UINT32_MAX);
    assert((series.saturation_flags &
            AERORTOS_MEASUREMENT_TOTAL_SATURATED) != 0U);
    assert((series.saturation_flags &
            AERORTOS_MEASUREMENT_COUNT_SATURATED) != 0U);
    assert((series.saturation_flags &
            AERORTOS_MEASUREMENT_DROPPED_SATURATED) != 0U);
    assert(aerortos_measurement_mean_cycles(&series) == UINT32_MAX);

    aerortos_measurement_series_init(&series);
    assert(series.total_cycles == 0U);
    assert(series.observation_count == 0U);
    assert(series.retained_count == 0U);
    assert(series.dropped_count == 0U);
    assert(series.saturation_flags == 0U);
    assert(series.minimum_cycles == UINT32_MAX);
    assert(series.maximum_cycles == 0U);
}

static void test_monitor_hooks(void)
{
    alignas(8) uint32_t stacks[3][TEST_STACK_WORDS];
    aerortos_task_t *idle;
    aerortos_task_t *high;
    aerortos_task_t *low;
    aerortos_critical_state_t outer;
    aerortos_critical_state_t inner;
    uint32_t index;

    aerortos_task_system_init();
    aerortos_time_system_init(0U);
    SystemCoreClock = TEST_CLOCK_HZ;
    aerortos_host_cycle_counter = UINT32_MAX - 20U;
    aerortos_host_cycle_counter_step = 3U;
    assert(aerortos_monitor_init(TEST_CLOCK_HZ));
    assert(aerortos_monitor_calibrate_overhead(8U));
    assert(aerortos_monitor_data.instrumentation_overhead_cycles == 3U);
    idle = create_task(stacks[0], AERORTOS_IDLE_PRIORITY);
    high = create_task(stacks[1], 2U);
    low = create_task(stacks[2], 10U);
    aerortos_scheduler_init(idle);
    assert(aerortos_scheduler_add(low) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_add(high) == AERORTOS_SCHEDULER_OK);
    assert(aerortos_scheduler_prepare_start());
    assert(aerortos_scheduler_block_current() == AERORTOS_SCHEDULER_OK);
    aerortos_host_pendsv_requests = 0U;
    assert(aerortos_scheduler_switch_context() == low);
    assert(aerortos_scheduler_make_ready(high) == AERORTOS_SCHEDULER_OK);
    aerortos_monitor_sync_wake(high);
    aerortos_monitor_periodic_release(high, 10U, 11U);
    aerortos_monitor_pendsv_start_cycle = aerortos_cycle_counter_now();
    assert(aerortos_scheduler_switch_context() == high);
    aerortos_monitor_pendsv_end();
    assert(aerortos_monitor_data.series[
        AERORTOS_METRIC_PENDSV_CYCLES].observation_count == 1U);
    assert(aerortos_monitor_data.series[
        AERORTOS_METRIC_DISPATCH_CYCLES].observation_count == 1U);
    assert(aerortos_monitor_data.series[
        AERORTOS_METRIC_SYNC_WAKE_CYCLES].observation_count == 1U);
    assert(aerortos_monitor_data.periodic_count == 1U);
    assert(aerortos_monitor_data.periodic[0].scheduled_tick == 10U);
    assert(aerortos_monitor_data.periodic[0].actual_release_tick == 11U);
    for (index = 0U;
         index < (AERORTOS_PERIODIC_MEASUREMENT_CAPACITY + 3U);
         ++index) {
        aerortos_monitor_periodic_release(high, index, index);
        aerortos_monitor_pendsv_start_cycle = aerortos_cycle_counter_now();
        aerortos_monitor_pendsv_end();
    }
    assert(aerortos_monitor_data.periodic_count ==
           AERORTOS_PERIODIC_MEASUREMENT_CAPACITY);
    assert(aerortos_monitor_data.periodic_dropped == 4U);

    outer = aerortos_critical_enter();
    inner = aerortos_critical_enter();
    aerortos_critical_exit(inner);
    aerortos_critical_exit(outer);
    assert(aerortos_monitor_data.series[
        AERORTOS_METRIC_CRITICAL_SECTION_CYCLES].observation_count != 0U);
    aerortos_monitor_deadline_miss();
    assert(aerortos_monitor_data.deadline_miss_total == 1U);
    assert(aerortos_monitor_invariants_hold());
}

int main(void)
{
    test_counter_and_conversion();
    test_statistics_and_bounds();
    test_monitor_hooks();
    puts("timing_test: PASS");
    return 0;
}
