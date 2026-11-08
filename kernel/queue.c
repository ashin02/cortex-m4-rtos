#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/critical_section.h"
#include "aerortos/port.h"
#include "aerortos/queue.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#if defined(AERORTOS_ENABLE_TIMING)
#include "aerortos/monitor.h"
#endif

#define AERORTOS_QUEUE_SIGNATURE UINT32_C(0x41525137)

typedef struct {
    aerortos_task_t **head;
    aerortos_task_t **tail;
    size_t *count;
} wait_list_t;

#if defined(AERORTOS_HOST_TEST)
extern void aerortos_host_queue_block_hook(void);
#endif

static bool queue_is_initialized(const aerortos_queue_t *queue)
{
    return (queue != NULL) &&
           (queue->signature == AERORTOS_QUEUE_SIGNATURE) &&
           (queue->storage != NULL) && (queue->element_size != 0U) &&
           (queue->capacity != 0U) &&
           (queue->capacity <= (SIZE_MAX / queue->element_size));
}

static wait_list_t send_wait_list(aerortos_queue_t *queue)
{
    wait_list_t list = {
        .head = &queue->send_wait_head,
        .tail = &queue->send_wait_tail,
        .count = &queue->waiting_senders
    };

    return list;
}

static wait_list_t receive_wait_list(aerortos_queue_t *queue)
{
    wait_list_t list = {
        .head = &queue->receive_wait_head,
        .tail = &queue->receive_wait_tail,
        .count = &queue->waiting_receivers
    };

    return list;
}

static void wait_enqueue_locked(wait_list_t list, aerortos_task_t *task)
{
    aerortos_task_t *position = *list.head;

    while ((position != NULL) &&
           (position->effective_priority <= task->effective_priority)) {
        position = position->ready_next;
    }

    if (position == NULL) {
        task->ready_next = NULL;
        task->ready_previous = *list.tail;
        if (*list.tail != NULL) {
            (*list.tail)->ready_next = task;
        } else {
            *list.head = task;
        }
        *list.tail = task;
    } else {
        task->ready_next = position;
        task->ready_previous = position->ready_previous;
        if (position->ready_previous != NULL) {
            position->ready_previous->ready_next = task;
        } else {
            *list.head = task;
        }
        position->ready_previous = task;
    }
    ++(*list.count);
}

static void wait_remove_locked(wait_list_t list, aerortos_task_t *task)
{
    if (task->ready_previous != NULL) {
        task->ready_previous->ready_next = task->ready_next;
    } else {
        *list.head = task->ready_next;
    }
    if (task->ready_next != NULL) {
        task->ready_next->ready_previous = task->ready_previous;
    } else {
        *list.tail = task->ready_previous;
    }
    task->ready_next = NULL;
    task->ready_previous = NULL;
    --(*list.count);
}

static void wake_first_locked(wait_list_t list)
{
    aerortos_task_t *task = *list.head;

    if (task == NULL) {
        return;
    }
    wait_remove_locked(list, task);
    task->wait_object = NULL;
    task->wait_kind = (uint8_t)AERORTOS_WAIT_NONE;
#if defined(AERORTOS_ENABLE_TIMING)
    aerortos_monitor_sync_wake(task);
#endif
    (void)aerortos_scheduler_make_ready(task);
}

static void copy_bytes(uint8_t *destination, const uint8_t *source, size_t length)
{
    size_t index;

    for (index = 0U; index < length; ++index) {
        destination[index] = source[index];
    }
}

static void push_locked(aerortos_queue_t *queue, const void *message)
{
    uint8_t *destination = &queue->storage[queue->tail * queue->element_size];

    copy_bytes(destination, (const uint8_t *)message, queue->element_size);
    ++queue->tail;
    if (queue->tail == queue->capacity) {
        queue->tail = 0U;
    }
    ++queue->count;
    wake_first_locked(receive_wait_list(queue));
}

static void pop_locked(aerortos_queue_t *queue, void *message_out)
{
    const uint8_t *source = &queue->storage[queue->head * queue->element_size];

    copy_bytes((uint8_t *)message_out, source, queue->element_size);
    ++queue->head;
    if (queue->head == queue->capacity) {
        queue->head = 0U;
    }
    --queue->count;
    wake_first_locked(send_wait_list(queue));
}

static aerortos_queue_result_t block_current_locked(
    aerortos_queue_t *queue,
    wait_list_t list,
    aerortos_wait_kind_t wait_kind)
{
    aerortos_task_t *current = aerortos_scheduler_current();

    if ((current == NULL) || (current->state != AERORTOS_TASK_RUNNING) ||
        (current->wait_object != NULL) || (current->ready_next != NULL) ||
        (current->ready_previous != NULL)) {
        return AERORTOS_QUEUE_ERROR_STATE;
    }

    current->wait_object = queue;
    current->wait_kind = (uint8_t)wait_kind;
    wait_enqueue_locked(list, current);
    if (aerortos_scheduler_block_current() != AERORTOS_SCHEDULER_OK) {
        wait_remove_locked(list, current);
        current->wait_object = NULL;
        current->wait_kind = (uint8_t)AERORTOS_WAIT_NONE;
        return AERORTOS_QUEUE_ERROR_STATE;
    }
    return AERORTOS_QUEUE_OK;
}

aerortos_queue_result_t aerortos_queue_init(
    aerortos_queue_t *queue,
    void *storage,
    size_t capacity,
    size_t element_size)
{
    if ((queue == NULL) || (storage == NULL)) {
        return AERORTOS_QUEUE_ERROR_NULL;
    }
    if (capacity == 0U) {
        return AERORTOS_QUEUE_ERROR_CAPACITY;
    }
    if (element_size == 0U) {
        return AERORTOS_QUEUE_ERROR_ELEMENT_SIZE;
    }
    if (capacity > (SIZE_MAX / element_size)) {
        return AERORTOS_QUEUE_ERROR_STORAGE_SIZE;
    }

    queue->signature = 0U;
    queue->storage = (uint8_t *)storage;
    queue->element_size = element_size;
    queue->capacity = capacity;
    queue->head = 0U;
    queue->tail = 0U;
    queue->count = 0U;
    queue->send_wait_head = NULL;
    queue->send_wait_tail = NULL;
    queue->receive_wait_head = NULL;
    queue->receive_wait_tail = NULL;
    queue->waiting_senders = 0U;
    queue->waiting_receivers = 0U;
    queue->signature = AERORTOS_QUEUE_SIGNATURE;
    return AERORTOS_QUEUE_OK;
}

aerortos_queue_result_t aerortos_queue_try_send(
    aerortos_queue_t *queue,
    const void *message)
{
    aerortos_critical_state_t critical_state;

    if ((queue == NULL) || (message == NULL)) {
        return AERORTOS_QUEUE_ERROR_NULL;
    }
    critical_state = aerortos_critical_enter();
    if (!queue_is_initialized(queue)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_QUEUE_ERROR_STATE;
    }
    if (queue->count == queue->capacity) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_QUEUE_ERROR_FULL;
    }
    push_locked(queue, message);
    aerortos_critical_exit(critical_state);
    return AERORTOS_QUEUE_OK;
}

aerortos_queue_result_t aerortos_queue_try_receive(
    aerortos_queue_t *queue,
    void *message_out)
{
    aerortos_critical_state_t critical_state;

    if ((queue == NULL) || (message_out == NULL)) {
        return AERORTOS_QUEUE_ERROR_NULL;
    }
    critical_state = aerortos_critical_enter();
    if (!queue_is_initialized(queue)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_QUEUE_ERROR_STATE;
    }
    if (queue->count == 0U) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_QUEUE_ERROR_EMPTY;
    }
    pop_locked(queue, message_out);
    aerortos_critical_exit(critical_state);
    return AERORTOS_QUEUE_OK;
}

aerortos_queue_result_t aerortos_queue_send(
    aerortos_queue_t *queue,
    const void *message)
{
    if ((queue == NULL) || (message == NULL)) {
        return AERORTOS_QUEUE_ERROR_NULL;
    }
    if (aerortos_port_in_handler_mode()) {
        return AERORTOS_QUEUE_ERROR_CONTEXT;
    }

    for (;;) {
        aerortos_critical_state_t critical_state = aerortos_critical_enter();
        aerortos_queue_result_t result;

        if (!queue_is_initialized(queue)) {
            aerortos_critical_exit(critical_state);
            return AERORTOS_QUEUE_ERROR_STATE;
        }
        if (queue->count < queue->capacity) {
            push_locked(queue, message);
            aerortos_critical_exit(critical_state);
            return AERORTOS_QUEUE_OK;
        }
        result = block_current_locked(queue, send_wait_list(queue),
                                      AERORTOS_WAIT_QUEUE_SEND);
        aerortos_critical_exit(critical_state);
        if (result != AERORTOS_QUEUE_OK) {
            return result;
        }
#if defined(AERORTOS_HOST_TEST)
        aerortos_host_queue_block_hook();
#endif
    }
}

aerortos_queue_result_t aerortos_queue_receive(
    aerortos_queue_t *queue,
    void *message_out)
{
    if ((queue == NULL) || (message_out == NULL)) {
        return AERORTOS_QUEUE_ERROR_NULL;
    }
    if (aerortos_port_in_handler_mode()) {
        return AERORTOS_QUEUE_ERROR_CONTEXT;
    }

    for (;;) {
        aerortos_critical_state_t critical_state = aerortos_critical_enter();
        aerortos_queue_result_t result;

        if (!queue_is_initialized(queue)) {
            aerortos_critical_exit(critical_state);
            return AERORTOS_QUEUE_ERROR_STATE;
        }
        if (queue->count != 0U) {
            pop_locked(queue, message_out);
            aerortos_critical_exit(critical_state);
            return AERORTOS_QUEUE_OK;
        }
        result = block_current_locked(queue, receive_wait_list(queue),
                                      AERORTOS_WAIT_QUEUE_RECEIVE);
        aerortos_critical_exit(critical_state);
        if (result != AERORTOS_QUEUE_OK) {
            return result;
        }
#if defined(AERORTOS_HOST_TEST)
        aerortos_host_queue_block_hook();
#endif
    }
}

aerortos_queue_result_t aerortos_queue_count(
    const aerortos_queue_t *queue,
    size_t *count_out)
{
    aerortos_critical_state_t critical_state;

    if ((queue == NULL) || (count_out == NULL)) {
        return AERORTOS_QUEUE_ERROR_NULL;
    }
    critical_state = aerortos_critical_enter();
    if (!queue_is_initialized(queue)) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_QUEUE_ERROR_STATE;
    }
    *count_out = queue->count;
    aerortos_critical_exit(critical_state);
    return AERORTOS_QUEUE_OK;
}

static bool invariants_hold_locked(const aerortos_queue_t *queue)
{
    const aerortos_task_t *task;
    const aerortos_task_t *previous;
    size_t observed;
    size_t ring_distance;

    if (!queue_is_initialized(queue) || (queue->head >= queue->capacity) ||
        (queue->tail >= queue->capacity) || (queue->count > queue->capacity)) {
        return false;
    }
    if (queue->tail >= queue->head) {
        ring_distance = queue->tail - queue->head;
    } else {
        ring_distance = (queue->capacity - queue->head) + queue->tail;
    }
    if (((queue->count == queue->capacity) &&
         (queue->head != queue->tail)) ||
        ((queue->count < queue->capacity) &&
         (ring_distance != queue->count))) {
        return false;
    }

    task = queue->send_wait_head;
    previous = NULL;
    observed = 0U;
    while (task != NULL) {
        if ((task->ready_previous != previous) ||
            (task->state != AERORTOS_TASK_BLOCKED) ||
            (task->wait_object != queue) ||
            (task->wait_kind != (uint8_t)AERORTOS_WAIT_QUEUE_SEND) ||
            ((previous != NULL) &&
             (previous->effective_priority > task->effective_priority)) ||
            (++observed > AERORTOS_TASK_CAPACITY)) {
            return false;
        }
        previous = task;
        task = task->ready_next;
    }
    if ((previous != queue->send_wait_tail) ||
        (observed != queue->waiting_senders)) {
        return false;
    }

    task = queue->receive_wait_head;
    previous = NULL;
    observed = 0U;
    while (task != NULL) {
        if ((task->ready_previous != previous) ||
            (task->state != AERORTOS_TASK_BLOCKED) ||
            (task->wait_object != queue) ||
            (task->wait_kind != (uint8_t)AERORTOS_WAIT_QUEUE_RECEIVE) ||
            ((previous != NULL) &&
             (previous->effective_priority > task->effective_priority)) ||
            (++observed > AERORTOS_TASK_CAPACITY)) {
            return false;
        }
        previous = task;
        task = task->ready_next;
    }
    return (previous == queue->receive_wait_tail) &&
           (observed == queue->waiting_receivers) &&
           ((queue->send_wait_head == NULL) ==
            (queue->send_wait_tail == NULL)) &&
           ((queue->receive_wait_head == NULL) ==
            (queue->receive_wait_tail == NULL));
}

bool aerortos_queue_invariants_hold(const aerortos_queue_t *queue)
{
    aerortos_critical_state_t critical_state;
    bool valid;

    if (queue == NULL) {
        return false;
    }
    critical_state = aerortos_critical_enter();
    valid = invariants_hold_locked(queue);
    aerortos_critical_exit(critical_state);
    return valid;
}

aerortos_queue_result_t aerortos_queue_reorder_waiter(
    aerortos_task_t *task,
    uint8_t priority)
{
    aerortos_critical_state_t critical_state;
    aerortos_queue_t *queue;
    wait_list_t list;

    if (task == NULL) {
        return AERORTOS_QUEUE_ERROR_NULL;
    }
    critical_state = aerortos_critical_enter();
    queue = (aerortos_queue_t *)task->wait_object;
    if ((task->state != AERORTOS_TASK_BLOCKED) ||
        !queue_is_initialized(queue) ||
        ((task->wait_kind != (uint8_t)AERORTOS_WAIT_QUEUE_SEND) &&
         (task->wait_kind != (uint8_t)AERORTOS_WAIT_QUEUE_RECEIVE))) {
        aerortos_critical_exit(critical_state);
        return AERORTOS_QUEUE_ERROR_STATE;
    }
    list = (task->wait_kind == (uint8_t)AERORTOS_WAIT_QUEUE_SEND) ?
           send_wait_list(queue) : receive_wait_list(queue);
    wait_remove_locked(list, task);
    if (aerortos_scheduler_set_effective_priority(task, priority) !=
        AERORTOS_SCHEDULER_OK) {
        wait_enqueue_locked(list, task);
        aerortos_critical_exit(critical_state);
        return AERORTOS_QUEUE_ERROR_STATE;
    }
    wait_enqueue_locked(list, task);
    aerortos_critical_exit(critical_state);
    return AERORTOS_QUEUE_OK;
}
