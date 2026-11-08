#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/deadline.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"

static uint32_t saturating_increment(uint32_t value)
{
    return (value == UINT32_MAX) ? value : value + 1U;
}

static void record_event(
    aerortos_task_t *task,
    aerortos_timing_event_type_t type,
    uint32_t tick,
    uint32_t scheduled_tick,
    int32_t value)
{
    aerortos_timing_event_t *event;
    uint8_t index;

    index = task->timing_trace_write;
    event = &task->timing_trace[index];
    event->tick = tick;
    event->scheduled_tick = scheduled_tick;
    event->value = value;
    event->type = (uint8_t)type;
    event->reserved[0] = 0U;
    event->reserved[1] = 0U;
    event->reserved[2] = 0U;
    task->timing_trace_write =
        (uint8_t)((index + 1U) % AERORTOS_TIMING_TRACE_CAPACITY);
    if (task->timing_trace_count < AERORTOS_TIMING_TRACE_CAPACITY) {
        ++task->timing_trace_count;
    }
}

void aerortos_deadline_release(
    aerortos_task_t *task,
    uint32_t scheduled_tick,
    uint32_t actual_tick)
{
    const int32_t service_jitter = (int32_t)(actual_tick - scheduled_tick);

    task->release_tick = scheduled_tick;
    task->absolute_deadline = scheduled_tick + task->relative_deadline_ticks;
    task->release_count = saturating_increment(task->release_count);
    task->current_job_execution_ticks = 0U;
    task->job_active = 1U;
    task->job_started = 0U;
    task->deadline_miss_recorded = 0U;
    record_event(task, AERORTOS_TIMING_EVENT_RELEASE, actual_tick,
                 scheduled_tick, service_jitter);
}

void aerortos_deadline_check(aerortos_task_t *task, uint32_t now)
{
    if ((task->job_active != 0U) &&
        (task->deadline_miss_recorded == 0U) &&
        aerortos_tick_after(now, task->absolute_deadline)) {
        task->deadline_miss_recorded = 1U;
        task->deadline_miss_count =
            saturating_increment(task->deadline_miss_count);
        record_event(task, AERORTOS_TIMING_EVENT_DEADLINE_MISS, now,
                     task->absolute_deadline,
                     (int32_t)(now - task->absolute_deadline));
    }
}

void aerortos_deadline_overrun(
    aerortos_task_t *task,
    uint32_t scheduled_tick,
    uint32_t now)
{
    task->overrun_count = saturating_increment(task->overrun_count);
    task->skipped_release_count =
        saturating_increment(task->skipped_release_count);
    record_event(task, AERORTOS_TIMING_EVENT_OVERRUN, now,
                 scheduled_tick, 0);
}

void aerortos_deadline_dispatch(aerortos_task_t *task, uint32_t now)
{
    int32_t jitter;

    if ((task == NULL) || (task->period_ticks == 0U) ||
        (task->job_active == 0U) || (task->job_started != 0U)) {
        return;
    }
    jitter = (int32_t)(now - task->release_tick);
    task->job_started = 1U;
    task->last_start_tick = now;
    task->last_release_jitter_ticks = jitter;
    if (jitter > task->max_release_jitter_ticks) {
        task->max_release_jitter_ticks = jitter;
    }
    record_event(task, AERORTOS_TIMING_EVENT_START, now,
                 task->release_tick, jitter);
}

void aerortos_deadline_account_execution(aerortos_task_t *task)
{
    if ((task == NULL) || (task->period_ticks == 0U) ||
        (task->job_active == 0U) || (task->job_started == 0U) ||
        (task->state != AERORTOS_TASK_RUNNING)) {
        return;
    }
    task->current_job_execution_ticks =
        saturating_increment(task->current_job_execution_ticks);
    task->total_execution_ticks =
        saturating_increment(task->total_execution_ticks);
}

void aerortos_deadline_complete(aerortos_task_t *task, uint32_t now)
{
    aerortos_deadline_check(task, now);
    task->completion_count = saturating_increment(task->completion_count);
    task->last_finish_tick = now;
    task->last_job_execution_ticks = task->current_job_execution_ticks;
    task->job_active = 0U;
    task->job_started = 0U;
    record_event(task, AERORTOS_TIMING_EVENT_COMPLETE, now,
                 task->absolute_deadline,
                 (task->last_job_execution_ticks > (uint32_t)INT32_MAX) ?
                 INT32_MAX : (int32_t)task->last_job_execution_ticks);
}
