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

typedef struct {
    aerortos_task_t *head;
    aerortos_task_t *tail;
} ready_queue_t;

static ready_queue_t ready_queues[AERORTOS_PRIORITY_COUNT];
static uint32_t ready_bitmap;
static aerortos_task_t *idle_task;
aerortos_task_t * volatile aerortos_current_task;

static uint32_t priority_bit(uint8_t priority)
{
    return UINT32_C(1) << (31U - priority);
}

static bool task_is_linked(const aerortos_task_t *task)
{
    const ready_queue_t *queue = &ready_queues[task->effective_priority];

    return (task->ready_previous != NULL) || (task->ready_next != NULL) ||
           (queue->head == task);
}

static void enqueue_locked(aerortos_task_t *task)
{
    ready_queue_t *queue = &ready_queues[task->effective_priority];

    task->ready_next = NULL;
    task->ready_previous = queue->tail;
    if (queue->tail != NULL) {
        queue->tail->ready_next = task;
    } else {
        queue->head = task;
    }
    queue->tail = task;
    ready_bitmap |= priority_bit(task->effective_priority);
}

static void remove_ready_locked(aerortos_task_t *task)
{
    ready_queue_t *queue = &ready_queues[task->effective_priority];

    if (task->ready_previous != NULL) {
        task->ready_previous->ready_next = task->ready_next;
    } else {
        queue->head = task->ready_next;
    }
    if (task->ready_next != NULL) {
        task->ready_next->ready_previous = task->ready_previous;
    } else {
        queue->tail = task->ready_previous;
    }
    if (queue->head == NULL) {
        ready_bitmap &= ~priority_bit(task->effective_priority);
    }
    task->ready_next = NULL;
    task->ready_previous = NULL;
}

static aerortos_task_t *dequeue_highest_locked(void)
{
    uint8_t priority;
    ready_queue_t *queue;
    aerortos_task_t *task;

    if (ready_bitmap == 0U) {
        return NULL;
    }
    priority = (uint8_t)__builtin_clz(ready_bitmap);
    queue = &ready_queues[priority];
    task = queue->head;
    queue->head = task->ready_next;
    if (queue->head != NULL) {
        queue->head->ready_previous = NULL;
    } else {
        queue->tail = NULL;
        ready_bitmap &= ~priority_bit(priority);
    }
    task->ready_next = NULL;
    task->ready_previous = NULL;
    return task;
}

static uint8_t highest_ready_priority_locked(void)
{
    return (uint8_t)__builtin_clz(ready_bitmap);
}

static aerortos_task_t *select_next_locked(void)
{
    aerortos_task_t *current = aerortos_current_task;
    aerortos_task_t *next;

    if ((current != NULL) && (current->state == AERORTOS_TASK_RUNNING)) {
        if (current == idle_task) {
            if (ready_bitmap == 0U) {
                return current;
            }
            current->state = AERORTOS_TASK_READY;
        } else if ((ready_bitmap == 0U) ||
                   (current->effective_priority <= highest_ready_priority_locked())) {
            return current;
        } else {
            current->state = AERORTOS_TASK_READY;
            enqueue_locked(current);
        }
    } else if ((current != NULL) && (current != idle_task) &&
               (current->state == AERORTOS_TASK_READY) &&
               !task_is_linked(current)) {
        enqueue_locked(current);
    }

    next = dequeue_highest_locked();
    if (next == NULL) {
        next = idle_task;
    }
    next->state = AERORTOS_TASK_RUNNING;
    aerortos_current_task = next;
    aerortos_deadline_dispatch(next, aerortos_tick_now());
    return next;
}

void aerortos_scheduler_init(aerortos_task_t *new_idle_task)
{
    size_t index;

    for (index = 0U; index < AERORTOS_PRIORITY_COUNT; ++index) {
        ready_queues[index].head = NULL;
        ready_queues[index].tail = NULL;
    }
    ready_bitmap = 0U;
    aerortos_current_task = NULL;
    idle_task = NULL;
    if ((new_idle_task != NULL) &&
        (new_idle_task->state == AERORTOS_TASK_READY) &&
        (new_idle_task->effective_priority == AERORTOS_IDLE_PRIORITY)) {
        new_idle_task->ready_next = NULL;
        new_idle_task->ready_previous = NULL;
        idle_task = new_idle_task;
    }
}

aerortos_scheduler_result_t aerortos_scheduler_add(aerortos_task_t *task)
{
    aerortos_critical_state_t critical_state;
    aerortos_timer_result_t timer_result;

    if (task == NULL) {
        return AERORTOS_SCHEDULER_ERROR_NULL;
    }
    if (task == idle_task) {
        return AERORTOS_SCHEDULER_ERROR_IDLE;
    }
    if (task->effective_priority > AERORTOS_MAX_APPLICATION_PRIORITY) {
        return AERORTOS_SCHEDULER_ERROR_PRIORITY;
    }
    critical_state = aerortos_critical_enter();
    if (task->state != AERORTOS_TASK_READY) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_SCHEDULER_ERROR_STATE;
    }
    if ((task->scheduler_registered != 0U) || task_is_linked(task) ||
        (task == aerortos_current_task)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_SCHEDULER_ERROR_DUPLICATE;
    }
    if (task->period_ticks != 0U) {
        timer_result = aerortos_timer_register_task(task);
        if (timer_result != AERORTOS_TIMER_OK) {
            aerortos_critical_exit(critical_state);
            return AERORTOS_SCHEDULER_ERROR_STATE;
        }
    }
    task->scheduler_registered = 1U;
    if (task->state == AERORTOS_TASK_READY) {
        enqueue_locked(task);
    }
    aerortos_critical_exit(critical_state);
    return AERORTOS_SCHEDULER_OK;
}

bool aerortos_scheduler_prepare_start(void)
{
    aerortos_critical_state_t critical_state;
    bool prepared = false;

    critical_state = aerortos_critical_enter();
    if ((idle_task != NULL) && (aerortos_current_task == NULL)) {
        (void)select_next_locked();
        prepared = true;
    }
    aerortos_critical_exit(critical_state);
    return prepared;
}

void aerortos_scheduler_start(void)
{
    if (!aerortos_scheduler_prepare_start()) {
        for (;;) {
        }
    }
    aerortos_port_start_first_task();
}

aerortos_scheduler_result_t aerortos_scheduler_yield(void)
{
    aerortos_critical_state_t critical_state;
    aerortos_task_t *current;

    if (aerortos_port_in_handler_mode()) {
        return AERORTOS_SCHEDULER_ERROR_STATE;
    }
    critical_state = aerortos_critical_enter();
    current = aerortos_current_task;
    if ((current == NULL) || (current->state != AERORTOS_TASK_RUNNING)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_SCHEDULER_ERROR_STATE;
    }
    current->state = AERORTOS_TASK_READY;
#if defined(AERORTOS_ENABLE_TIMING)
    aerortos_monitor_task_ready(current);
#endif
    aerortos_port_request_context_switch();
    aerortos_critical_exit(critical_state);
    return AERORTOS_SCHEDULER_OK;
}

aerortos_scheduler_result_t aerortos_scheduler_block_current(void)
{
    aerortos_critical_state_t critical_state;
    aerortos_task_t *current;

    if (aerortos_port_in_handler_mode()) {
        return AERORTOS_SCHEDULER_ERROR_STATE;
    }
    critical_state = aerortos_critical_enter();
    current = aerortos_current_task;
    if ((current == NULL) || (current == idle_task) ||
        (current->state != AERORTOS_TASK_RUNNING)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_SCHEDULER_ERROR_STATE;
    }
    current->state = AERORTOS_TASK_BLOCKED;
    aerortos_port_request_context_switch();
    aerortos_critical_exit(critical_state);
    return AERORTOS_SCHEDULER_OK;
}

aerortos_scheduler_result_t aerortos_scheduler_make_ready(aerortos_task_t *task)
{
    aerortos_critical_state_t critical_state;
    aerortos_task_t *current;
    bool should_preempt;

    if (task == NULL) {
        return AERORTOS_SCHEDULER_ERROR_NULL;
    }
    if ((task == idle_task) ||
        (task->effective_priority > AERORTOS_MAX_APPLICATION_PRIORITY)) {
        return AERORTOS_SCHEDULER_ERROR_PRIORITY;
    }
    critical_state = aerortos_critical_enter();
    if ((task->state != AERORTOS_TASK_BLOCKED) &&
        (task->state != AERORTOS_TASK_SUSPENDED)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_SCHEDULER_ERROR_STATE;
    }
    task->state = AERORTOS_TASK_READY;
    enqueue_locked(task);
#if defined(AERORTOS_ENABLE_TIMING)
    aerortos_monitor_task_ready(task);
#endif
    current = aerortos_current_task;
    should_preempt = (current != NULL) &&
                     ((current == idle_task) ||
                      ((current->state == AERORTOS_TASK_RUNNING) &&
                       (task->effective_priority < current->effective_priority)));
    if (should_preempt) {
        aerortos_port_request_context_switch();
    }
    aerortos_critical_exit(critical_state);
    return AERORTOS_SCHEDULER_OK;
}

aerortos_scheduler_result_t aerortos_scheduler_set_effective_priority(
    aerortos_task_t *task,
    uint8_t priority)
{
    aerortos_critical_state_t critical_state;
    aerortos_task_t *current;
    bool linked;
    bool should_preempt = false;

    if (task == NULL) {
        return AERORTOS_SCHEDULER_ERROR_NULL;
    }
    if (priority > AERORTOS_MAX_APPLICATION_PRIORITY) {
        return AERORTOS_SCHEDULER_ERROR_PRIORITY;
    }
    critical_state = aerortos_critical_enter();
    if ((task == idle_task) || (task->scheduler_registered == 0U) ||
        (task->state == AERORTOS_TASK_UNUSED) ||
        (task->state == AERORTOS_TASK_TERMINATED)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_SCHEDULER_ERROR_STATE;
    }
    if (task->effective_priority == priority) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_SCHEDULER_OK;
    }

    linked = (task->state == AERORTOS_TASK_READY) && task_is_linked(task);
    if (linked) {
        remove_ready_locked(task);
    }
    task->effective_priority = priority;
    if (linked) {
        enqueue_locked(task);
    }

    current = aerortos_current_task;
    if ((current != NULL) && (current->state == AERORTOS_TASK_RUNNING)) {
        if (linked && (task->effective_priority < current->effective_priority)) {
            should_preempt = true;
        } else if ((task == current) && (ready_bitmap != 0U) &&
                   (highest_ready_priority_locked() <
                    current->effective_priority)) {
            should_preempt = true;
        }
    }
    if (should_preempt) {
        aerortos_port_request_context_switch();
    }
    aerortos_critical_exit(critical_state);
    return AERORTOS_SCHEDULER_OK;
}

aerortos_task_t *aerortos_scheduler_switch_context(void)
{
    aerortos_critical_state_t critical_state = aerortos_critical_enter();
    aerortos_task_t *next = select_next_locked();

    aerortos_critical_exit(critical_state);
    return next;
}

aerortos_task_t *aerortos_scheduler_current(void)
{
    return aerortos_current_task;
}

uint32_t aerortos_scheduler_ready_bitmap(void)
{
    return ready_bitmap;
}

bool aerortos_scheduler_invariants_hold(void)
{
    uint32_t observed_bitmap = 0U;
    size_t priority;
    size_t total = 0U;

    if ((idle_task == NULL) ||
        (idle_task->effective_priority != AERORTOS_IDLE_PRIORITY)) {
        return false;
    }
    for (priority = 0U; priority < AERORTOS_PRIORITY_COUNT; ++priority) {
        const ready_queue_t *queue = &ready_queues[priority];
        const aerortos_task_t *previous = NULL;
        const aerortos_task_t *task = queue->head;
        size_t queue_count = 0U;

        if ((queue->head == NULL) != (queue->tail == NULL)) {
            return false;
        }
        while (task != NULL) {
            if ((task->state != AERORTOS_TASK_READY) ||
                (task->effective_priority != priority) ||
                (task->ready_previous != previous) || (task == idle_task) ||
                (++total > AERORTOS_TASK_CAPACITY) ||
                (++queue_count > AERORTOS_TASK_CAPACITY)) {
                return false;
            }
            previous = task;
            task = task->ready_next;
        }
        if (previous != queue->tail) {
            return false;
        }
        if (queue_count != 0U) {
            observed_bitmap |= priority_bit((uint8_t)priority);
        }
    }
    return observed_bitmap == ready_bitmap;
}
