#ifndef AERORTOS_TIMER_H
#define AERORTOS_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#include "aerortos/task.h"

#define AERORTOS_TICK_HZ 1000U

extern volatile uint32_t aerortos_systick_irq_count;

typedef enum {
    AERORTOS_TIMER_OK = 0,
    AERORTOS_TIMER_ERROR_NULL,
    AERORTOS_TIMER_ERROR_CONTEXT,
    AERORTOS_TIMER_ERROR_NOT_PERIODIC,
    AERORTOS_TIMER_ERROR_STATE,
    AERORTOS_TIMER_ERROR_CAPACITY,
    AERORTOS_TIMER_ERROR_DUPLICATE
} aerortos_timer_result_t;

void aerortos_time_system_init(uint32_t initial_tick);
uint32_t aerortos_tick_now(void);
bool aerortos_tick_after(uint32_t first, uint32_t second);
bool aerortos_tick_after_or_equal(uint32_t first, uint32_t second);
aerortos_timer_result_t aerortos_timer_register_task(aerortos_task_t *task);
void aerortos_timer_tick_isr(void);
aerortos_timer_result_t aerortos_periodic_wait_next(void);
bool aerortos_systick_init(void);

#endif
