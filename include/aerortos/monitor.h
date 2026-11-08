#ifndef AERORTOS_MONITOR_H
#define AERORTOS_MONITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/task.h"

#define AERORTOS_MEASUREMENT_CAPACITY 64U
#define AERORTOS_PERIODIC_MEASUREMENT_CAPACITY 32U
#define AERORTOS_MEASUREMENT_TOTAL_SATURATED (UINT32_C(1) << 0)
#define AERORTOS_MEASUREMENT_COUNT_SATURATED (UINT32_C(1) << 1)
#define AERORTOS_MEASUREMENT_DROPPED_SATURATED (UINT32_C(1) << 2)
#define AERORTOS_MEASUREMENT_SATURATION_MASK \
    (AERORTOS_MEASUREMENT_TOTAL_SATURATED | \
     AERORTOS_MEASUREMENT_COUNT_SATURATED | \
     AERORTOS_MEASUREMENT_DROPPED_SATURATED)

typedef enum {
    AERORTOS_METRIC_PENDSV_CYCLES = 0,
    AERORTOS_METRIC_DISPATCH_CYCLES,
    AERORTOS_METRIC_PERIODIC_RELEASE_TO_START_CYCLES,
    AERORTOS_METRIC_SYNC_WAKE_CYCLES,
    AERORTOS_METRIC_CRITICAL_SECTION_CYCLES,
    AERORTOS_METRIC_INSTRUMENTATION_OVERHEAD_CYCLES,
    AERORTOS_METRIC_COUNT
} aerortos_metric_t;

typedef struct {
    uint32_t samples[AERORTOS_MEASUREMENT_CAPACITY];
    uint64_t total_cycles;
    uint32_t minimum_cycles;
    uint32_t maximum_cycles;
    uint32_t observation_count;
    uint32_t retained_count;
    uint32_t dropped_count;
    uint32_t saturation_flags;
} aerortos_measurement_series_t;

typedef struct {
    uint32_t task_id;
    uint32_t scheduled_tick;
    uint32_t actual_release_tick;
    uint32_t release_cycle;
    uint32_t execution_start_cycle;
} aerortos_periodic_measurement_t;

typedef struct {
    aerortos_measurement_series_t series[AERORTOS_METRIC_COUNT];
    aerortos_periodic_measurement_t
        periodic[AERORTOS_PERIODIC_MEASUREMENT_CAPACITY];
    uint32_t periodic_count;
    uint32_t periodic_dropped;
    uint32_t core_clock_hz;
    uint32_t instrumentation_overhead_cycles;
    uint32_t deadline_miss_total;
} aerortos_monitor_data_t;

extern aerortos_monitor_data_t aerortos_monitor_data;
extern volatile uint32_t aerortos_monitor_pendsv_start_cycle;

void aerortos_measurement_series_init(aerortos_measurement_series_t *series);
bool aerortos_measurement_record(
    aerortos_measurement_series_t *series,
    uint32_t cycles);
uint32_t aerortos_measurement_mean_cycles(
    const aerortos_measurement_series_t *series);
bool aerortos_monitor_init(uint32_t expected_core_clock_hz);
bool aerortos_monitor_calibrate_overhead(uint32_t sample_count);
bool aerortos_monitor_invariants_hold(void);
void aerortos_monitor_task_ready(aerortos_task_t *task);
void aerortos_monitor_sync_wake(aerortos_task_t *task);
void aerortos_monitor_periodic_release(
    aerortos_task_t *task,
    uint32_t scheduled_tick,
    uint32_t actual_tick);
void aerortos_monitor_deadline_miss(void);
void aerortos_monitor_critical_enter(uint32_t previous_basepri);
void aerortos_monitor_critical_exit(uint32_t previous_basepri);
void aerortos_monitor_pendsv_end(void);

#endif
