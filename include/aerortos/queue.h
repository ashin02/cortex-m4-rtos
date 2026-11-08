#ifndef AERORTOS_QUEUE_H
#define AERORTOS_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/task.h"

typedef enum {
    AERORTOS_QUEUE_OK = 0,
    AERORTOS_QUEUE_ERROR_NULL,
    AERORTOS_QUEUE_ERROR_CAPACITY,
    AERORTOS_QUEUE_ERROR_ELEMENT_SIZE,
    AERORTOS_QUEUE_ERROR_STORAGE_SIZE,
    AERORTOS_QUEUE_ERROR_FULL,
    AERORTOS_QUEUE_ERROR_EMPTY,
    AERORTOS_QUEUE_ERROR_CONTEXT,
    AERORTOS_QUEUE_ERROR_STATE
} aerortos_queue_result_t;

typedef struct {
    uint8_t *storage;
    size_t element_size;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    aerortos_task_t *send_wait_head;
    aerortos_task_t *send_wait_tail;
    aerortos_task_t *receive_wait_head;
    aerortos_task_t *receive_wait_tail;
    size_t waiting_senders;
    size_t waiting_receivers;
    uint32_t signature;
} aerortos_queue_t;

/*
 * Queue control and payload storage remain caller-owned for the queue lifetime.
 * Initialize before concurrent use; capacity * element_size must fit size_t.
 * Payload pointers must address element_size bytes and must not overlap storage.
 * Blocking calls are Thread-mode-only and wait indefinitely. The try calls never
 * block and may be used only by Thread mode or kernel-aware ISRs permitted by the
 * BASEPRI contract.
 */
aerortos_queue_result_t aerortos_queue_init(
    aerortos_queue_t *queue,
    void *storage,
    size_t capacity,
    size_t element_size);
aerortos_queue_result_t aerortos_queue_try_send(
    aerortos_queue_t *queue,
    const void *message);
aerortos_queue_result_t aerortos_queue_try_receive(
    aerortos_queue_t *queue,
    void *message_out);
aerortos_queue_result_t aerortos_queue_send(
    aerortos_queue_t *queue,
    const void *message);
aerortos_queue_result_t aerortos_queue_receive(
    aerortos_queue_t *queue,
    void *message_out);
aerortos_queue_result_t aerortos_queue_count(
    const aerortos_queue_t *queue,
    size_t *count_out);
bool aerortos_queue_invariants_hold(const aerortos_queue_t *queue);

/* Internal PI hook: caller holds or nests the kernel critical section. */
aerortos_queue_result_t aerortos_queue_reorder_waiter(
    aerortos_task_t *task,
    uint8_t priority);

#endif
