#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/cycle_counter.h"
#include "aerortos/monitor.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"

typedef struct {
    uint32_t ready_cycle;
    uint32_t sync_cycle;
    uint32_t release_cycle;
    uint32_t scheduled_tick;
    uint32_t actual_tick;
    bool ready_pending;
    bool sync_pending;
    bool periodic_pending;
} task_measurement_state_t;

aerortos_monitor_data_t aerortos_monitor_data;
volatile uint32_t aerortos_monitor_pendsv_start_cycle;

static task_measurement_state_t task_states[AERORTOS_TASK_CAPACITY];
static uint32_t critical_start_cycle;
static uint32_t critical_depth;

void aerortos_measurement_series_init(aerortos_measurement_series_t *series)
{
    size_t index;

    if (series == NULL) {
        return;
    }
    for (index = 0U; index < AERORTOS_MEASUREMENT_CAPACITY; ++index) {
        series->samples[index] = 0U;
    }
    series->total_cycles = 0U;
    series->minimum_cycles = UINT32_MAX;
    series->maximum_cycles = 0U;
    series->observation_count = 0U;
    series->retained_count = 0U;
    series->dropped_count = 0U;
    series->saturation_flags = 0U;
}

bool aerortos_measurement_record(
    aerortos_measurement_series_t *series,
    uint32_t cycles)
{
    if (series == NULL) {
        return false;
    }

    if (series->observation_count != UINT32_MAX) {
        ++series->observation_count;
    } else {
        series->saturation_flags |= AERORTOS_MEASUREMENT_COUNT_SATURATED;
    }
    if (series->total_cycles <= (UINT64_MAX - cycles)) {
        series->total_cycles += cycles;
    } else {
        series->total_cycles = UINT64_MAX;
        series->saturation_flags |= AERORTOS_MEASUREMENT_TOTAL_SATURATED;
    }
    if (cycles < series->minimum_cycles) {
        series->minimum_cycles = cycles;
    }
    if (cycles > series->maximum_cycles) {
        series->maximum_cycles = cycles;
    }

    if (series->retained_count < AERORTOS_MEASUREMENT_CAPACITY) {
        series->samples[series->retained_count] = cycles;
        ++series->retained_count;
        return true;
    }
    if (series->dropped_count != UINT32_MAX) {
        ++series->dropped_count;
    } else {
        series->saturation_flags |= AERORTOS_MEASUREMENT_DROPPED_SATURATED;
    }
    return false;
}

uint32_t aerortos_measurement_mean_cycles(
    const aerortos_measurement_series_t *series)
{
    uint64_t mean;

    if ((series == NULL) || (series->observation_count == 0U)) {
        return 0U;
    }
    mean = series->total_cycles / series->observation_count;
    return mean > UINT32_MAX ? UINT32_MAX : (uint32_t)mean;
}

bool aerortos_monitor_init(uint32_t expected_core_clock_hz)
{
    size_t index;

    for (index = 0U; index < AERORTOS_METRIC_COUNT; ++index) {
        aerortos_measurement_series_init(&aerortos_monitor_data.series[index]);
    }
    for (index = 0U; index < AERORTOS_PERIODIC_MEASUREMENT_CAPACITY; ++index) {
        aerortos_monitor_data.periodic[index] =
            (aerortos_periodic_measurement_t){0};
    }
    for (index = 0U; index < AERORTOS_TASK_CAPACITY; ++index) {
        task_states[index] = (task_measurement_state_t){0};
    }
    aerortos_monitor_data.periodic_count = 0U;
    aerortos_monitor_data.periodic_dropped = 0U;
    aerortos_monitor_data.core_clock_hz = expected_core_clock_hz;
    aerortos_monitor_data.instrumentation_overhead_cycles = 0U;
    aerortos_monitor_data.deadline_miss_total = 0U;
    aerortos_monitor_pendsv_start_cycle = 0U;
    critical_start_cycle = 0U;
    critical_depth = 0U;
    return aerortos_cycle_counter_init(expected_core_clock_hz);
}

bool aerortos_monitor_calibrate_overhead(uint32_t sample_count)
{
    aerortos_measurement_series_t *series =
        &aerortos_monitor_data.series[
            AERORTOS_METRIC_INSTRUMENTATION_OVERHEAD_CYCLES];
    uint32_t index;

    if (!aerortos_cycle_counter_is_ready() || (sample_count == 0U) ||
        (sample_count > AERORTOS_MEASUREMENT_CAPACITY)) {
        return false;
    }
    aerortos_measurement_series_init(series);
    for (index = 0U; index < sample_count; ++index) {
        const uint32_t start = aerortos_cycle_counter_now();
        const uint32_t end = aerortos_cycle_counter_now();

        (void)aerortos_measurement_record(
            series, aerortos_cycle_counter_elapsed(start, end));
    }
    aerortos_monitor_data.instrumentation_overhead_cycles =
        series->minimum_cycles;
    return true;
}

static task_measurement_state_t *state_for(aerortos_task_t *task)
{
    if ((task == NULL) || (task->slot >= AERORTOS_TASK_CAPACITY)) {
        return NULL;
    }
    return &task_states[task->slot];
}

void aerortos_monitor_task_ready(aerortos_task_t *task)
{
    task_measurement_state_t *state = state_for(task);

    if ((state != NULL) && aerortos_cycle_counter_is_ready()) {
        state->ready_cycle = aerortos_cycle_counter_now();
        state->ready_pending = true;
    }
}

void aerortos_monitor_sync_wake(aerortos_task_t *task)
{
    task_measurement_state_t *state = state_for(task);

    if ((state != NULL) && aerortos_cycle_counter_is_ready()) {
        state->sync_cycle = aerortos_cycle_counter_now();
        state->sync_pending = true;
    }
}

void aerortos_monitor_periodic_release(
    aerortos_task_t *task,
    uint32_t scheduled_tick,
    uint32_t actual_tick)
{
    task_measurement_state_t *state = state_for(task);

    if ((state != NULL) && aerortos_cycle_counter_is_ready()) {
        state->release_cycle = aerortos_cycle_counter_now();
        state->scheduled_tick = scheduled_tick;
        state->actual_tick = actual_tick;
        state->periodic_pending = true;
    }
}

void aerortos_monitor_deadline_miss(void)
{
    if (aerortos_monitor_data.deadline_miss_total != UINT32_MAX) {
        ++aerortos_monitor_data.deadline_miss_total;
    }
}

void aerortos_monitor_critical_enter(uint32_t previous_basepri)
{
    (void)previous_basepri;
    if (!aerortos_cycle_counter_is_ready()) {
        return;
    }
    if (critical_depth == 0U) {
        critical_start_cycle = aerortos_cycle_counter_now();
    }
    ++critical_depth;
}

void aerortos_monitor_critical_exit(uint32_t previous_basepri)
{
    (void)previous_basepri;
    if (!aerortos_cycle_counter_is_ready()) {
        return;
    }
    if (critical_depth == 0U) {
        return;
    }
    --critical_depth;
    if (critical_depth == 0U) {
        const uint32_t end = aerortos_cycle_counter_now();

        (void)aerortos_measurement_record(
            &aerortos_monitor_data.series[
                AERORTOS_METRIC_CRITICAL_SECTION_CYCLES],
            aerortos_cycle_counter_elapsed(critical_start_cycle, end));
    }
}

void aerortos_monitor_pendsv_end(void)
{
    aerortos_task_t *task = aerortos_scheduler_current();
    task_measurement_state_t *state = state_for(task);
    const uint32_t now = aerortos_cycle_counter_now();

    (void)aerortos_measurement_record(
        &aerortos_monitor_data.series[AERORTOS_METRIC_PENDSV_CYCLES],
        aerortos_cycle_counter_elapsed(aerortos_monitor_pendsv_start_cycle, now));
    if (state == NULL) {
        return;
    }
    if (state->ready_pending) {
        (void)aerortos_measurement_record(
            &aerortos_monitor_data.series[AERORTOS_METRIC_DISPATCH_CYCLES],
            aerortos_cycle_counter_elapsed(state->ready_cycle, now));
        state->ready_pending = false;
    }
    if (state->sync_pending) {
        (void)aerortos_measurement_record(
            &aerortos_monitor_data.series[AERORTOS_METRIC_SYNC_WAKE_CYCLES],
            aerortos_cycle_counter_elapsed(state->sync_cycle, now));
        state->sync_pending = false;
    }
    if (state->periodic_pending) {
        (void)aerortos_measurement_record(
            &aerortos_monitor_data.series[
                AERORTOS_METRIC_PERIODIC_RELEASE_TO_START_CYCLES],
            aerortos_cycle_counter_elapsed(state->release_cycle, now));
        if (aerortos_monitor_data.periodic_count <
            AERORTOS_PERIODIC_MEASUREMENT_CAPACITY) {
            aerortos_periodic_measurement_t *measurement =
                &aerortos_monitor_data.periodic[
                    aerortos_monitor_data.periodic_count];

            measurement->task_id = task->id;
            measurement->scheduled_tick = state->scheduled_tick;
            measurement->actual_release_tick = state->actual_tick;
            measurement->release_cycle = state->release_cycle;
            measurement->execution_start_cycle = now;
            ++aerortos_monitor_data.periodic_count;
        } else if (aerortos_monitor_data.periodic_dropped != UINT32_MAX) {
            ++aerortos_monitor_data.periodic_dropped;
        }
        state->periodic_pending = false;
    }
}

bool aerortos_monitor_invariants_hold(void)
{
    size_t index;

    if ((aerortos_monitor_data.core_clock_hz == 0U) ||
        (aerortos_monitor_data.periodic_count >
         AERORTOS_PERIODIC_MEASUREMENT_CAPACITY)) {
        return false;
    }
    for (index = 0U; index < AERORTOS_METRIC_COUNT; ++index) {
        const aerortos_measurement_series_t *series =
            &aerortos_monitor_data.series[index];

        if ((series->retained_count > AERORTOS_MEASUREMENT_CAPACITY) ||
            (series->retained_count > series->observation_count) ||
            ((series->saturation_flags &
              ~AERORTOS_MEASUREMENT_SATURATION_MASK) != 0U) ||
            ((series->observation_count == 0U) &&
             ((series->retained_count != 0U) ||
              (series->dropped_count != 0U) ||
              (series->total_cycles != 0U) ||
              (series->minimum_cycles != UINT32_MAX) ||
              (series->maximum_cycles != 0U))) ||
            ((series->observation_count != 0U) &&
             (series->minimum_cycles > series->maximum_cycles)) ||
            ((series->retained_count < AERORTOS_MEASUREMENT_CAPACITY) &&
             (series->dropped_count != 0U)) ||
            (((series->saturation_flags &
               (AERORTOS_MEASUREMENT_COUNT_SATURATED |
                AERORTOS_MEASUREMENT_DROPPED_SATURATED)) == 0U) &&
             ((uint64_t)series->retained_count + series->dropped_count !=
              series->observation_count))) {
            return false;
        }
    }
    return true;
}
