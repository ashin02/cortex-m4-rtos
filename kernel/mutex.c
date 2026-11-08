#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/critical_section.h"
#include "aerortos/mutex.h"
#include "aerortos/port.h"
#include "aerortos/queue.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#if defined(AERORTOS_ENABLE_TIMING)
#include "aerortos/monitor.h"
#endif

#define AERORTOS_MUTEX_SIGNATURE UINT32_C(0x41524D38)

#if defined(AERORTOS_HOST_TEST)
extern void aerortos_host_mutex_block_hook(void);
#endif

static bool mutex_is_initialized(const aerortos_mutex_t *mutex)
{
    return (mutex != NULL) &&
           (mutex->signature == AERORTOS_MUTEX_SIGNATURE);
}

static void wait_enqueue_locked(
    aerortos_mutex_t *mutex,
    aerortos_task_t *task)
{
    aerortos_task_t *position = mutex->wait_head;

    while ((position != NULL) &&
           (position->effective_priority <= task->effective_priority)) {
        position = position->ready_next;
    }
    if (position == NULL) {
        task->ready_next = NULL;
        task->ready_previous = mutex->wait_tail;
        if (mutex->wait_tail != NULL) {
            mutex->wait_tail->ready_next = task;
        } else {
            mutex->wait_head = task;
        }
        mutex->wait_tail = task;
    } else {
        task->ready_next = position;
        task->ready_previous = position->ready_previous;
        if (position->ready_previous != NULL) {
            position->ready_previous->ready_next = task;
        } else {
            mutex->wait_head = task;
        }
        position->ready_previous = task;
    }
    ++mutex->waiter_count;
}

static void wait_remove_locked(
    aerortos_mutex_t *mutex,
    aerortos_task_t *task)
{
    if (task->ready_previous != NULL) {
        task->ready_previous->ready_next = task->ready_next;
    } else {
        mutex->wait_head = task->ready_next;
    }
    if (task->ready_next != NULL) {
        task->ready_next->ready_previous = task->ready_previous;
    } else {
        mutex->wait_tail = task->ready_previous;
    }
    task->ready_next = NULL;
    task->ready_previous = NULL;
    --mutex->waiter_count;
}

static aerortos_task_t *wait_dequeue_locked(aerortos_mutex_t *mutex)
{
    aerortos_task_t *task = mutex->wait_head;

    if (task != NULL) {
        wait_remove_locked(mutex, task);
    }
    return task;
}

static void owned_add_locked(
    aerortos_task_t *owner,
    aerortos_mutex_t *mutex)
{
    mutex->owner_previous = NULL;
    mutex->owner_next = owner->owned_mutex_head;
    if (owner->owned_mutex_head != NULL) {
        owner->owned_mutex_head->owner_previous = mutex;
    }
    owner->owned_mutex_head = mutex;
    ++owner->owned_mutex_count;
    mutex->owner = owner;
}

static void owned_remove_locked(
    aerortos_task_t *owner,
    aerortos_mutex_t *mutex)
{
    if (mutex->owner_previous != NULL) {
        mutex->owner_previous->owner_next = mutex->owner_next;
    } else {
        owner->owned_mutex_head = mutex->owner_next;
    }
    if (mutex->owner_next != NULL) {
        mutex->owner_next->owner_previous = mutex->owner_previous;
    }
    mutex->owner_next = NULL;
    mutex->owner_previous = NULL;
    mutex->owner = NULL;
    --owner->owned_mutex_count;
}

static uint8_t desired_priority_locked(const aerortos_task_t *task)
{
    const aerortos_mutex_t *mutex = task->owned_mutex_head;
    uint8_t priority = task->base_priority;
    size_t count = 0U;

    while ((mutex != NULL) && (count < AERORTOS_MUTEX_MAX_OWNED)) {
        if ((mutex->wait_head != NULL) &&
            (mutex->wait_head->effective_priority < priority)) {
            priority = mutex->wait_head->effective_priority;
        }
        mutex = mutex->owner_next;
        ++count;
    }
    return priority;
}

static bool recompute_priority_locked(
    aerortos_task_t *task,
    size_t depth);

static bool set_priority_locked(
    aerortos_task_t *task,
    uint8_t priority,
    size_t depth)
{
    aerortos_mutex_t *wait_mutex = NULL;

    if (depth >= AERORTOS_TASK_CAPACITY) {
        return false;
    }
    if (task->effective_priority == priority) {
        return true;
    }
    if ((task->state == AERORTOS_TASK_BLOCKED) &&
        ((task->wait_kind == (uint8_t)AERORTOS_WAIT_QUEUE_SEND) ||
         (task->wait_kind == (uint8_t)AERORTOS_WAIT_QUEUE_RECEIVE))) {
        return aerortos_queue_reorder_waiter(task, priority) ==
               AERORTOS_QUEUE_OK;
    }
    if ((task->state == AERORTOS_TASK_BLOCKED) &&
        (task->wait_kind == (uint8_t)AERORTOS_WAIT_MUTEX)) {
        wait_mutex = (aerortos_mutex_t *)task->wait_object;
        if (!mutex_is_initialized(wait_mutex)) {
            return false;
        }
        wait_remove_locked(wait_mutex, task);
    }
    if (aerortos_scheduler_set_effective_priority(task, priority) !=
        AERORTOS_SCHEDULER_OK) {
        if (wait_mutex != NULL) {
            wait_enqueue_locked(wait_mutex, task);
        }
        return false;
    }
    if (wait_mutex != NULL) {
        wait_enqueue_locked(wait_mutex, task);
        if (wait_mutex->owner != NULL) {
            return recompute_priority_locked(wait_mutex->owner, depth + 1U);
        }
    }
    return true;
}

static bool recompute_priority_locked(
    aerortos_task_t *task,
    size_t depth)
{
    return set_priority_locked(task, desired_priority_locked(task), depth);
}

static bool would_deadlock_locked(
    const aerortos_task_t *current,
    const aerortos_mutex_t *mutex)
{
    const aerortos_task_t *owner = mutex->owner;
    size_t depth;

    for (depth = 0U; depth < AERORTOS_TASK_CAPACITY; ++depth) {
        const aerortos_mutex_t *wait_mutex;

        if (owner == current) {
            return true;
        }
        if ((owner == NULL) ||
            (owner->wait_kind != (uint8_t)AERORTOS_WAIT_MUTEX)) {
            return false;
        }
        wait_mutex = (const aerortos_mutex_t *)owner->wait_object;
        if (!mutex_is_initialized(wait_mutex)) {
            return true;
        }
        owner = wait_mutex->owner;
    }
    return true;
}

static aerortos_task_t *current_task_locked(void)
{
    aerortos_task_t *current = aerortos_scheduler_current();

    if ((current == NULL) ||
        (current->state != AERORTOS_TASK_RUNNING) ||
        (current->effective_priority > AERORTOS_MAX_APPLICATION_PRIORITY)) {
        return NULL;
    }
    return current;
}

aerortos_mutex_result_t aerortos_mutex_init(aerortos_mutex_t *mutex)
{
    if (mutex == NULL) {
        return AERORTOS_MUTEX_ERROR_NULL;
    }
    mutex->signature = 0U;
    mutex->owner = NULL;
    mutex->wait_head = NULL;
    mutex->wait_tail = NULL;
    mutex->owner_next = NULL;
    mutex->owner_previous = NULL;
    mutex->waiter_count = 0U;
    mutex->signature = AERORTOS_MUTEX_SIGNATURE;
    return AERORTOS_MUTEX_OK;
}

aerortos_mutex_result_t aerortos_mutex_try_lock(aerortos_mutex_t *mutex)
{
    aerortos_critical_state_t critical_state;
    aerortos_task_t *current;
    aerortos_mutex_result_t result;

    if (mutex == NULL) {
        return AERORTOS_MUTEX_ERROR_NULL;
    }
    if (aerortos_port_in_handler_mode()) {
        return AERORTOS_MUTEX_ERROR_CONTEXT;
    }
    critical_state = aerortos_critical_enter();
    current = current_task_locked();
    if (!mutex_is_initialized(mutex) || (current == NULL)) {
        result = AERORTOS_MUTEX_ERROR_STATE;
    } else if (mutex->owner == current) {
        result = AERORTOS_MUTEX_ERROR_RECURSIVE;
    } else if (mutex->owner != NULL) {
        result = AERORTOS_MUTEX_ERROR_BUSY;
    } else if (current->owned_mutex_count >= AERORTOS_MUTEX_MAX_OWNED) {
        result = AERORTOS_MUTEX_ERROR_CAPACITY;
    } else {
        owned_add_locked(current, mutex);
        result = AERORTOS_MUTEX_OK;
    }
    aerortos_critical_exit(critical_state);
    return result;
}

aerortos_mutex_result_t aerortos_mutex_lock(aerortos_mutex_t *mutex)
{
    aerortos_critical_state_t critical_state;
    aerortos_task_t *current;
    aerortos_task_t *owner;

    if (mutex == NULL) {
        return AERORTOS_MUTEX_ERROR_NULL;
    }
    if (aerortos_port_in_handler_mode()) {
        return AERORTOS_MUTEX_ERROR_CONTEXT;
    }
    critical_state = aerortos_critical_enter();
    current = current_task_locked();
    if (!mutex_is_initialized(mutex) || (current == NULL)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_MUTEX_ERROR_STATE;
    }
    if (mutex->owner == NULL) {
        if (current->owned_mutex_count >= AERORTOS_MUTEX_MAX_OWNED) {
            aerortos_critical_exit(critical_state);
            return AERORTOS_MUTEX_ERROR_CAPACITY;
        }
        owned_add_locked(current, mutex);
        aerortos_critical_exit(critical_state);
        return AERORTOS_MUTEX_OK;
    }
    if (mutex->owner == current) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_MUTEX_ERROR_RECURSIVE;
    }
    if (current->owned_mutex_count >= AERORTOS_MUTEX_MAX_OWNED) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_MUTEX_ERROR_CAPACITY;
    }
    if (would_deadlock_locked(current, mutex)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_MUTEX_ERROR_DEADLOCK;
    }

    owner = mutex->owner;
    current->wait_object = mutex;
    current->wait_kind = (uint8_t)AERORTOS_WAIT_MUTEX;
    wait_enqueue_locked(mutex, current);
    if (!recompute_priority_locked(owner, 0U)) {
        wait_remove_locked(mutex, current);
        current->wait_object = NULL;
        current->wait_kind = (uint8_t)AERORTOS_WAIT_NONE;
        aerortos_critical_exit(critical_state);
        return AERORTOS_MUTEX_ERROR_STATE;
    }
    if (aerortos_scheduler_block_current() != AERORTOS_SCHEDULER_OK) {
        wait_remove_locked(mutex, current);
        current->wait_object = NULL;
        current->wait_kind = (uint8_t)AERORTOS_WAIT_NONE;
        (void)recompute_priority_locked(owner, 0U);
        aerortos_critical_exit(critical_state);
        return AERORTOS_MUTEX_ERROR_STATE;
    }
    aerortos_critical_exit(critical_state);

#if defined(AERORTOS_HOST_TEST)
    aerortos_host_mutex_block_hook();
#endif
    critical_state = aerortos_critical_enter();
    current = current_task_locked();
    if (mutex_is_initialized(mutex) && (current != NULL) &&
        (mutex->owner == current) &&
        (current->wait_kind == (uint8_t)AERORTOS_WAIT_NONE)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_MUTEX_OK;
    }
    aerortos_critical_exit(critical_state);
    return AERORTOS_MUTEX_ERROR_STATE;
}

aerortos_mutex_result_t aerortos_mutex_unlock(aerortos_mutex_t *mutex)
{
    aerortos_critical_state_t critical_state;
    aerortos_task_t *current;
    aerortos_task_t *waiter;

    if (mutex == NULL) {
        return AERORTOS_MUTEX_ERROR_NULL;
    }
    if (aerortos_port_in_handler_mode()) {
        return AERORTOS_MUTEX_ERROR_CONTEXT;
    }
    critical_state = aerortos_critical_enter();
    current = current_task_locked();
    if (!mutex_is_initialized(mutex) || (current == NULL)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_MUTEX_ERROR_STATE;
    }
    if (mutex->owner != current) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_MUTEX_ERROR_NOT_OWNER;
    }

    waiter = wait_dequeue_locked(mutex);
    owned_remove_locked(current, mutex);
    if (!recompute_priority_locked(current, 0U)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_MUTEX_ERROR_STATE;
    }
    if (waiter != NULL) {
        waiter->wait_object = NULL;
        waiter->wait_kind = (uint8_t)AERORTOS_WAIT_NONE;
        owned_add_locked(waiter, mutex);
#if defined(AERORTOS_ENABLE_TIMING)
        if (!recompute_priority_locked(waiter, 0U)) {
            aerortos_critical_exit(critical_state);
            return AERORTOS_MUTEX_ERROR_STATE;
        }
        aerortos_monitor_sync_wake(waiter);
        if (aerortos_scheduler_make_ready(waiter) != AERORTOS_SCHEDULER_OK) {
#else
        if (!recompute_priority_locked(waiter, 0U) ||
            (aerortos_scheduler_make_ready(waiter) != AERORTOS_SCHEDULER_OK)) {
#endif
            aerortos_critical_exit(critical_state);
            return AERORTOS_MUTEX_ERROR_STATE;
        }
    }
    aerortos_critical_exit(critical_state);
    return AERORTOS_MUTEX_OK;
}

bool aerortos_mutex_invariants_hold(const aerortos_mutex_t *mutex)
{
    aerortos_critical_state_t critical_state;
    const aerortos_task_t *task;
    const aerortos_task_t *previous = NULL;
    const aerortos_mutex_t *owned;
    const aerortos_mutex_t *owned_previous = NULL;
    size_t waiter_count = 0U;
    size_t owned_count = 0U;
    bool found = false;
    bool valid = true;

    if (mutex == NULL) {
        return false;
    }
    critical_state = aerortos_critical_enter();
    if (!mutex_is_initialized(mutex) ||
        ((mutex->wait_head == NULL) != (mutex->wait_tail == NULL))) {
        valid = false;
    }
    task = mutex->wait_head;
    while (valid && (task != NULL)) {
        if ((task->ready_previous != previous) ||
            (task->state != AERORTOS_TASK_BLOCKED) ||
            (task->wait_object != mutex) ||
            (task->wait_kind != (uint8_t)AERORTOS_WAIT_MUTEX) ||
            ((previous != NULL) &&
             (previous->effective_priority > task->effective_priority)) ||
            (++waiter_count > AERORTOS_TASK_CAPACITY)) {
            valid = false;
            break;
        }
        previous = task;
        task = task->ready_next;
    }
    if (valid && ((previous != mutex->wait_tail) ||
                  (waiter_count != mutex->waiter_count))) {
        valid = false;
    }
    if (valid && (mutex->owner == NULL)) {
        valid = (mutex->waiter_count == 0U) &&
                (mutex->owner_next == NULL) &&
                (mutex->owner_previous == NULL);
    } else if (valid) {
        owned = mutex->owner->owned_mutex_head;
        while ((owned != NULL) &&
               (owned_count < AERORTOS_MUTEX_MAX_OWNED)) {
            if ((owned->owner != mutex->owner) ||
                (owned->owner_previous != owned_previous)) {
                valid = false;
                break;
            }
            if (owned == mutex) {
                found = true;
            }
            owned_previous = owned;
            owned = owned->owner_next;
            ++owned_count;
        }
        if ((owned != NULL) || !found ||
            (owned_count != mutex->owner->owned_mutex_count) ||
            (mutex->owner->effective_priority !=
             desired_priority_locked(mutex->owner))) {
            valid = false;
        }
    }
    aerortos_critical_exit(critical_state);
    return valid;
}
