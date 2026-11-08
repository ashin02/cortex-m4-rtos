#ifndef AERORTOS_DEADLINE_H
#define AERORTOS_DEADLINE_H

#include <stdint.h>

#include "aerortos/task.h"

void aerortos_deadline_release(
    aerortos_task_t *task,
    uint32_t scheduled_tick,
    uint32_t actual_tick);
void aerortos_deadline_check(aerortos_task_t *task, uint32_t now);
void aerortos_deadline_overrun(
    aerortos_task_t *task,
    uint32_t scheduled_tick,
    uint32_t now);
void aerortos_deadline_dispatch(aerortos_task_t *task, uint32_t now);
void aerortos_deadline_account_execution(aerortos_task_t *task);
void aerortos_deadline_complete(aerortos_task_t *task, uint32_t now);

#endif
