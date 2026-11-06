#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/port.h"
#include "aerortos/task.h"

static aerortos_task_t task_pool[AERORTOS_TASK_CAPACITY];
static uint32_t next_task_id = 1U;
static size_t active_task_count;

static void clear_task(aerortos_task_t *task)
{
    uint8_t *bytes = (uint8_t *)(void *)task;
    size_t index;

    for (index = 0U; index < sizeof(*task); ++index) {
        bytes[index] = 0U;
    }
}

static bool task_pointer_is_owned(const aerortos_task_t *task)
{
    const uintptr_t pool_start = (uintptr_t)&task_pool[0];
    const uintptr_t pool_end = (uintptr_t)&task_pool[AERORTOS_TASK_CAPACITY];
    const uintptr_t candidate = (uintptr_t)task;

    return (task != NULL) && (candidate >= pool_start) && (candidate < pool_end) &&
           (((candidate - pool_start) % sizeof(task_pool[0])) == 0U);
}

static bool stack_ranges_overlap(
    uintptr_t first_start,
    uintptr_t first_end,
    uintptr_t second_start,
    uintptr_t second_end)
{
    return (first_start < second_end) && (second_start < first_end);
}

static bool transition_is_valid(
    aerortos_task_state_t current,
    aerortos_task_state_t next)
{
    switch (current) {
    case AERORTOS_TASK_READY:
        return (next == AERORTOS_TASK_RUNNING) ||
               (next == AERORTOS_TASK_BLOCKED) ||
               (next == AERORTOS_TASK_SUSPENDED) ||
               (next == AERORTOS_TASK_TERMINATED);
    case AERORTOS_TASK_RUNNING:
        return (next == AERORTOS_TASK_READY) ||
               (next == AERORTOS_TASK_BLOCKED) ||
               (next == AERORTOS_TASK_SUSPENDED) ||
               (next == AERORTOS_TASK_TERMINATED);
    case AERORTOS_TASK_BLOCKED:
        return (next == AERORTOS_TASK_READY) ||
               (next == AERORTOS_TASK_SUSPENDED) ||
               (next == AERORTOS_TASK_TERMINATED);
    case AERORTOS_TASK_SUSPENDED:
        return (next == AERORTOS_TASK_READY) ||
               (next == AERORTOS_TASK_TERMINATED);
    case AERORTOS_TASK_UNUSED:
    case AERORTOS_TASK_TERMINATED:
    default:
        return false;
    }
}

void aerortos_task_system_init(void)
{
    size_t index;

    for (index = 0U; index < AERORTOS_TASK_CAPACITY; ++index) {
        clear_task(&task_pool[index]);
    }
    next_task_id = 1U;
    active_task_count = 0U;
}

aerortos_task_result_t aerortos_task_create(
    const aerortos_task_config_t *config,
    aerortos_task_t **task_out)
{
    uintptr_t stack_start;
    uintptr_t stack_end;
    size_t stack_words;
    size_t index;
    aerortos_task_t *task = NULL;

    if (task_out == NULL) {
        return AERORTOS_TASK_ERROR_NULL;
    }
    *task_out = NULL;
    if ((config == NULL) || (config->stack == NULL)) {
        return AERORTOS_TASK_ERROR_NULL;
    }
    if (config->entry == NULL) {
        return AERORTOS_TASK_ERROR_ENTRY;
    }
    stack_start = (uintptr_t)config->stack;
    if ((stack_start & (AERORTOS_TASK_STACK_ALIGNMENT - 1U)) != 0U) {
        return AERORTOS_TASK_ERROR_STACK_ALIGNMENT;
    }
    if ((config->stack_size_bytes < AERORTOS_TASK_MIN_STACK_BYTES) ||
        ((config->stack_size_bytes & (AERORTOS_TASK_STACK_ALIGNMENT - 1U)) != 0U)) {
        return AERORTOS_TASK_ERROR_STACK_SIZE;
    }
    if (stack_start > (UINTPTR_MAX - config->stack_size_bytes)) {
        return AERORTOS_TASK_ERROR_STACK_SIZE;
    }
    if (config->priority >= AERORTOS_PRIORITY_COUNT) {
        return AERORTOS_TASK_ERROR_PRIORITY;
    }

    stack_end = stack_start + config->stack_size_bytes;
    for (index = 0U; index < AERORTOS_TASK_CAPACITY; ++index) {
        const aerortos_task_t *existing = &task_pool[index];

        if (existing->state == AERORTOS_TASK_UNUSED) {
            if (task == NULL) {
                task = &task_pool[index];
            }
            continue;
        }
        if (stack_ranges_overlap(stack_start, stack_end,
                                 (uintptr_t)existing->stack_low,
                                 (uintptr_t)existing->stack_high)) {
            return AERORTOS_TASK_ERROR_DUPLICATE_STACK;
        }
    }
    if (task == NULL) {
        return AERORTOS_TASK_ERROR_CAPACITY;
    }

    stack_words = config->stack_size_bytes / sizeof(uint32_t);
    for (index = 0U; index < stack_words; ++index) {
        config->stack[index] = AERORTOS_TASK_STACK_FILL_WORD;
    }
    for (index = 0U; index < AERORTOS_TASK_STACK_GUARD_WORDS; ++index) {
        config->stack[index] = AERORTOS_TASK_STACK_GUARD_WORD;
    }

    clear_task(task);
    task->stack_low = config->stack;
    task->stack_high = &config->stack[stack_words];
    task->entry = config->entry;
    task->argument = config->argument;
    task->stack_words = stack_words;
    task->base_priority = config->priority;
    task->effective_priority = config->priority;
    task->state = AERORTOS_TASK_READY;
    task->slot = (uint8_t)(task - &task_pool[0]);
    task->id = next_task_id;

    if (!aerortos_port_build_initial_frame(task->stack_low, task->stack_words,
                                           task->entry, task->argument,
                                           &task->saved_psp)) {
        clear_task(task);
        return AERORTOS_TASK_ERROR_STACK_SIZE;
    }

    ++next_task_id;
    if (next_task_id == 0U) {
        next_task_id = 1U;
    }
    ++active_task_count;
    *task_out = task;
    return AERORTOS_TASK_OK;
}

aerortos_task_result_t aerortos_task_transition(
    aerortos_task_t *task,
    aerortos_task_state_t next_state)
{
    aerortos_task_state_t current_state;

    if (!task_pointer_is_owned(task) || (task->state == AERORTOS_TASK_UNUSED)) {
        return AERORTOS_TASK_ERROR_INVALID_TASK;
    }
    if (!aerortos_task_stack_guard_intact(task)) {
        return AERORTOS_TASK_ERROR_STACK_GUARD;
    }

    current_state = (aerortos_task_state_t)task->state;
    if (!transition_is_valid(current_state, next_state)) {
        return AERORTOS_TASK_ERROR_INVALID_TRANSITION;
    }

    task->state = (uint8_t)next_state;
    if (next_state != AERORTOS_TASK_BLOCKED) {
        task->wait_object = NULL;
        task->wake_tick = 0U;
    }
    return AERORTOS_TASK_OK;
}

aerortos_task_result_t aerortos_task_release(aerortos_task_t *task)
{
    if (!task_pointer_is_owned(task) ||
        (task->state != AERORTOS_TASK_TERMINATED)) {
        return AERORTOS_TASK_ERROR_INVALID_TASK;
    }

    clear_task(task);
    --active_task_count;
    return AERORTOS_TASK_OK;
}

size_t aerortos_task_count(void)
{
    return active_task_count;
}

bool aerortos_task_stack_guard_intact(const aerortos_task_t *task)
{
    size_t index;

    if (!task_pointer_is_owned(task) || (task->state == AERORTOS_TASK_UNUSED) ||
        (task->stack_low == NULL)) {
        return false;
    }

    for (index = 0U; index < AERORTOS_TASK_STACK_GUARD_WORDS; ++index) {
        if (task->stack_low[index] != AERORTOS_TASK_STACK_GUARD_WORD) {
            return false;
        }
    }
    return true;
}

aerortos_task_result_t aerortos_task_stack_unused_words(
    const aerortos_task_t *task,
    size_t *unused_words_out)
{
    size_t index;
    size_t unused_words = 0U;

    if (unused_words_out == NULL) {
        return AERORTOS_TASK_ERROR_NULL;
    }
    *unused_words_out = 0U;
    if (!task_pointer_is_owned(task) || (task->state == AERORTOS_TASK_UNUSED)) {
        return AERORTOS_TASK_ERROR_INVALID_TASK;
    }
    if (!aerortos_task_stack_guard_intact(task)) {
        return AERORTOS_TASK_ERROR_STACK_GUARD;
    }

    for (index = AERORTOS_TASK_STACK_GUARD_WORDS;
         index < task->stack_words;
         ++index) {
        if (task->stack_low[index] != AERORTOS_TASK_STACK_FILL_WORD) {
            break;
        }
        ++unused_words;
    }
    *unused_words_out = unused_words;
    return AERORTOS_TASK_OK;
}
