#ifndef AERORTOS_MUTEX_H
#define AERORTOS_MUTEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/task.h"

#define AERORTOS_MUTEX_MAX_OWNED AERORTOS_TASK_CAPACITY

typedef enum {
    AERORTOS_MUTEX_OK = 0,
    AERORTOS_MUTEX_ERROR_NULL,
    AERORTOS_MUTEX_ERROR_STATE,
    AERORTOS_MUTEX_ERROR_CONTEXT,
    AERORTOS_MUTEX_ERROR_RECURSIVE,
    AERORTOS_MUTEX_ERROR_NOT_OWNER,
    AERORTOS_MUTEX_ERROR_BUSY,
    AERORTOS_MUTEX_ERROR_CAPACITY,
    AERORTOS_MUTEX_ERROR_DEADLOCK
} aerortos_mutex_result_t;

typedef struct aerortos_mutex aerortos_mutex_t;

struct aerortos_mutex {
    aerortos_task_t *owner;
    aerortos_task_t *wait_head;
    aerortos_task_t *wait_tail;
    aerortos_mutex_t *owner_next;
    aerortos_mutex_t *owner_previous;
    size_t waiter_count;
    uint32_t signature;
};

/*
 * Mutex storage remains caller-owned for its full initialized lifetime. Mutexes
 * are non-recursive, use indefinite waits, and permit at most eight owned mutexes
 * per task. All operations are Thread-mode-only; unlock is owner-only.
 *
 * Wait-list insertion/reordering, owned-list scans, deadlock inspection, and PI
 * propagation are bounded by the eight-task system capacity. There are no timed
 * waits, cancellation, deletion, or safe reinitialization while owned/waited-on.
 */
aerortos_mutex_result_t aerortos_mutex_init(aerortos_mutex_t *mutex);
aerortos_mutex_result_t aerortos_mutex_try_lock(aerortos_mutex_t *mutex);
aerortos_mutex_result_t aerortos_mutex_lock(aerortos_mutex_t *mutex);
aerortos_mutex_result_t aerortos_mutex_unlock(aerortos_mutex_t *mutex);
bool aerortos_mutex_invariants_hold(const aerortos_mutex_t *mutex);

#endif
