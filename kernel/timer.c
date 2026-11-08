#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/critical_section.h"
#include "aerortos/deadline.h"
#include "aerortos/port.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"
#if defined(AERORTOS_ENABLE_TIMING)
#include "aerortos/monitor.h"
#endif

static volatile uint32_t tick_count;
static aerortos_task_t *periodic_tasks[AERORTOS_TASK_CAPACITY];
static size_t periodic_task_count;

void aerortos_time_system_init(uint32_t initial_tick)
{
    size_t index;

    tick_count = initial_tick;
    periodic_task_count = 0U;
    for (index = 0U; index < AERORTOS_TASK_CAPACITY; ++index) {
        periodic_tasks[index] = NULL;
    }
}

uint32_t aerortos_tick_now(void)
{
    return tick_count;
}

bool aerortos_tick_after(uint32_t first, uint32_t second)
{
    return (int32_t)(first - second) > 0;
}

bool aerortos_tick_after_or_equal(uint32_t first, uint32_t second)
{
    return (int32_t)(first - second) >= 0;
}

aerortos_timer_result_t aerortos_timer_register_task(aerortos_task_t *task)
{
    uint32_t scheduled_tick;
    size_t index;

    if (task == NULL) {
        return AERORTOS_TIMER_ERROR_NULL;
    }
    if (task->period_ticks == 0U) {
        return AERORTOS_TIMER_ERROR_NOT_PERIODIC;
    }
    for (index = 0U; index < periodic_task_count; ++index) {
        if (periodic_tasks[index] == task) {
            return AERORTOS_TIMER_ERROR_DUPLICATE;
        }
    }
    if (periodic_task_count >= AERORTOS_TASK_CAPACITY) {
        return AERORTOS_TIMER_ERROR_CAPACITY;
    }

    periodic_tasks[periodic_task_count] = task;
    ++periodic_task_count;
    scheduled_tick = tick_count + task->initial_release_delay_ticks;
    if (task->initial_release_delay_ticks == 0U) {
        aerortos_deadline_release(task, scheduled_tick, tick_count);
#if defined(AERORTOS_ENABLE_TIMING)
        aerortos_monitor_periodic_release(task, scheduled_tick, tick_count);
#endif
        task->next_release_tick = scheduled_tick + task->period_ticks;
    } else {
        task->next_release_tick = scheduled_tick;
        task->state = AERORTOS_TASK_BLOCKED;
    }
    return AERORTOS_TIMER_OK;
}

void aerortos_timer_tick_isr(void)
{
    aerortos_task_t *current;
    uint32_t now;
    size_t index;

    ++tick_count;
    now = tick_count;
    current = aerortos_scheduler_current();
    aerortos_deadline_account_execution(current);

    for (index = 0U; index < periodic_task_count; ++index) {
        aerortos_task_t *task = periodic_tasks[index];

        aerortos_deadline_check(task, now);
        if (aerortos_tick_after_or_equal(now, task->next_release_tick)) {
            const uint32_t scheduled_tick = task->next_release_tick;

            task->next_release_tick += task->period_ticks;
            if (task->job_active != 0U) {
                aerortos_deadline_overrun(task, scheduled_tick, now);
            } else if (task->state == AERORTOS_TASK_BLOCKED) {
                aerortos_deadline_release(task, scheduled_tick, now);
#if defined(AERORTOS_ENABLE_TIMING)
                aerortos_monitor_periodic_release(task, scheduled_tick, now);
#endif
                (void)aerortos_scheduler_make_ready(task);
            }
        }
    }
}

aerortos_timer_result_t aerortos_periodic_wait_next(void)
{
    aerortos_critical_state_t critical_state;
    aerortos_task_t *current;
    aerortos_scheduler_result_t scheduler_result;

    if (aerortos_port_in_handler_mode()) {
        return AERORTOS_TIMER_ERROR_CONTEXT;
    }
    critical_state = aerortos_critical_enter();
    current = aerortos_scheduler_current();
    if ((current == NULL) || (current->period_ticks == 0U)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_TIMER_ERROR_NOT_PERIODIC;
    }
    if ((current->state != AERORTOS_TASK_RUNNING) ||
        (current->job_active == 0U)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_TIMER_ERROR_STATE;
    }

    aerortos_deadline_complete(current, tick_count);
    scheduler_result = aerortos_scheduler_block_current();
    aerortos_critical_exit(critical_state);
    return (scheduler_result == AERORTOS_SCHEDULER_OK) ?
           AERORTOS_TIMER_OK : AERORTOS_TIMER_ERROR_STATE;
}
