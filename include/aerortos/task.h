#ifndef AERORTOS_TASK_H
#define AERORTOS_TASK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AERORTOS_TASK_CAPACITY 8U
#define AERORTOS_PRIORITY_COUNT 32U
#define AERORTOS_TASK_MIN_STACK_BYTES 128U
#define AERORTOS_TASK_STACK_ALIGNMENT 8U
#define AERORTOS_TASK_STACK_GUARD_WORDS 2U
#define AERORTOS_TASK_STACK_FILL_WORD UINT32_C(0xA5A5A5A5)
#define AERORTOS_TASK_STACK_GUARD_WORD UINT32_C(0xD15EA5E5)

typedef void (*aerortos_task_entry_t)(void *argument);

typedef enum {
    AERORTOS_TASK_UNUSED = 0,
    AERORTOS_TASK_READY,
    AERORTOS_TASK_RUNNING,
    AERORTOS_TASK_BLOCKED,
    AERORTOS_TASK_SUSPENDED,
    AERORTOS_TASK_TERMINATED
} aerortos_task_state_t;

typedef enum {
    AERORTOS_TASK_OK = 0,
    AERORTOS_TASK_ERROR_NULL,
    AERORTOS_TASK_ERROR_ENTRY,
    AERORTOS_TASK_ERROR_STACK_ALIGNMENT,
    AERORTOS_TASK_ERROR_STACK_SIZE,
    AERORTOS_TASK_ERROR_PRIORITY,
    AERORTOS_TASK_ERROR_DUPLICATE_STACK,
    AERORTOS_TASK_ERROR_CAPACITY,
    AERORTOS_TASK_ERROR_INVALID_TASK,
    AERORTOS_TASK_ERROR_INVALID_TRANSITION,
    AERORTOS_TASK_ERROR_STACK_GUARD
} aerortos_task_result_t;

typedef struct aerortos_task aerortos_task_t;

struct aerortos_task {
    uint32_t *saved_psp;
    uint32_t *stack_low;
    uint32_t *stack_high;
    aerortos_task_entry_t entry;
    void *argument;
    aerortos_task_t *ready_next;
    aerortos_task_t *ready_previous;
    void *wait_object;
    uint32_t wake_tick;
    uint32_t release_tick;
    uint32_t absolute_deadline;
    uint32_t id;
    size_t stack_words;
    uint8_t base_priority;
    uint8_t effective_priority;
    uint8_t state;
    uint8_t slot;
};

typedef struct {
    aerortos_task_entry_t entry;
    void *argument;
    uint32_t *stack;
    size_t stack_size_bytes;
    uint8_t priority;
} aerortos_task_config_t;

void aerortos_task_system_init(void);
aerortos_task_result_t aerortos_task_create(
    const aerortos_task_config_t *config,
    aerortos_task_t **task_out);
aerortos_task_result_t aerortos_task_transition(
    aerortos_task_t *task,
    aerortos_task_state_t next_state);
aerortos_task_result_t aerortos_task_release(aerortos_task_t *task);
size_t aerortos_task_count(void);
bool aerortos_task_stack_guard_intact(const aerortos_task_t *task);
aerortos_task_result_t aerortos_task_stack_unused_words(
    const aerortos_task_t *task,
    size_t *unused_words_out);

#endif
