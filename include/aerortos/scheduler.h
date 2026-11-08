#ifndef AERORTOS_SCHEDULER_H
#define AERORTOS_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "aerortos/task.h"

#define AERORTOS_IDLE_PRIORITY (AERORTOS_PRIORITY_COUNT - 1U)
#define AERORTOS_MAX_APPLICATION_PRIORITY (AERORTOS_IDLE_PRIORITY - 1U)

typedef enum {
    AERORTOS_SCHEDULER_OK = 0,
    AERORTOS_SCHEDULER_ERROR_NULL,
    AERORTOS_SCHEDULER_ERROR_STATE,
    AERORTOS_SCHEDULER_ERROR_PRIORITY,
    AERORTOS_SCHEDULER_ERROR_DUPLICATE,
    AERORTOS_SCHEDULER_ERROR_IDLE
} aerortos_scheduler_result_t;

extern aerortos_task_t * volatile aerortos_current_task;

void aerortos_scheduler_init(aerortos_task_t *idle_task);
aerortos_scheduler_result_t aerortos_scheduler_add(aerortos_task_t *task);
bool aerortos_scheduler_prepare_start(void);
void aerortos_scheduler_start(void) __attribute__((noreturn));
aerortos_scheduler_result_t aerortos_scheduler_yield(void);
aerortos_scheduler_result_t aerortos_scheduler_block_current(void);
aerortos_scheduler_result_t aerortos_scheduler_make_ready(aerortos_task_t *task);
aerortos_scheduler_result_t aerortos_scheduler_set_effective_priority(
    aerortos_task_t *task,
    uint8_t priority);
aerortos_task_t *aerortos_scheduler_switch_context(void);
aerortos_task_t *aerortos_scheduler_current(void);
uint32_t aerortos_scheduler_ready_bitmap(void);
bool aerortos_scheduler_invariants_hold(void);

#endif
