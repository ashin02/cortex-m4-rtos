#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "aerortos/port.h"
#include "aerortos/task.h"

#define TEST_STACK_WORDS 64U

static void test_entry(void *argument)
{
    (void)argument;
}

static aerortos_task_config_t make_config(
    uint32_t *stack,
    size_t stack_words,
    uint8_t priority,
    void *argument)
{
    const aerortos_task_config_t config = {
        .entry = test_entry,
        .argument = argument,
        .stack = stack,
        .stack_size_bytes = stack_words * sizeof(uint32_t),
        .priority = priority
    };

    return config;
}

static void test_valid_task_and_initial_frame(void)
{
    alignas(AERORTOS_TASK_STACK_ALIGNMENT) uint32_t stack[TEST_STACK_WORDS];
    uint32_t argument_value = UINT32_C(0x12345678);
    aerortos_task_config_t config =
        make_config(stack, TEST_STACK_WORDS, 3U, &argument_value);
    aerortos_task_t *task = NULL;
    const aerortos_software_frame_t *software_frame;
    const aerortos_hardware_frame_t *hardware_frame;
    const uint32_t *software_words;
    size_t index;
    size_t unused_words = 0U;

    aerortos_task_system_init();
    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_OK);
    assert(task != NULL);
    assert(aerortos_task_count() == 1U);
    assert(task->state == AERORTOS_TASK_READY);
    assert(task->base_priority == 3U);
    assert(task->effective_priority == 3U);
    assert(task->stack_low == stack);
    assert(task->stack_high == &stack[TEST_STACK_WORDS]);
    assert(task->stack_words == TEST_STACK_WORDS);
    assert(task->entry == test_entry);
    assert(task->argument == &argument_value);
    assert(task->id == 1U);
    assert(task->slot == 0U);
    assert(((uintptr_t)task->saved_psp &
            (AERORTOS_TASK_STACK_ALIGNMENT - 1U)) == 0U);
    assert(task->saved_psp ==
           &stack[TEST_STACK_WORDS - AERORTOS_INITIAL_FRAME_WORDS]);

    software_frame = (const aerortos_software_frame_t *)(const void *)task->saved_psp;
    hardware_frame = (const aerortos_hardware_frame_t *)(const void *)(
        &task->saved_psp[sizeof(*software_frame) / sizeof(uint32_t)]);
    software_words = (const uint32_t *)(const void *)software_frame;

    for (index = 0U; index < 8U; ++index) {
        assert(software_words[index] == 0U);
    }
    assert(hardware_frame->r0 == (uint32_t)(uintptr_t)&argument_value);
    assert(hardware_frame->r1 == 0U);
    assert(hardware_frame->r2 == 0U);
    assert(hardware_frame->r3 == 0U);
    assert(hardware_frame->r12 == 0U);
    assert(hardware_frame->lr ==
           (((uint32_t)(uintptr_t)aerortos_task_return_trap) | 1U));
    assert(hardware_frame->pc ==
           (((uint32_t)(uintptr_t)test_entry) & ~1U));
    assert(hardware_frame->xpsr == AERORTOS_INITIAL_XPSR);
    assert((hardware_frame->xpsr & UINT32_C(0x01000000)) != 0U);

    assert(aerortos_task_stack_guard_intact(task));
    assert(aerortos_task_stack_unused_words(task, &unused_words) ==
           AERORTOS_TASK_OK);
    assert(unused_words == (TEST_STACK_WORDS -
                            AERORTOS_TASK_STACK_GUARD_WORDS -
                            AERORTOS_INITIAL_FRAME_WORDS));
}

static void test_invalid_creation(void)
{
    alignas(AERORTOS_TASK_STACK_ALIGNMENT) uint32_t stack[TEST_STACK_WORDS];
    aerortos_task_config_t config = make_config(stack, TEST_STACK_WORDS, 0U, NULL);
    aerortos_task_t *task = NULL;
    uint8_t *stack_bytes = (uint8_t *)(void *)stack;

    aerortos_task_system_init();
    assert(aerortos_task_create(&config, NULL) == AERORTOS_TASK_ERROR_NULL);
    assert(aerortos_task_create(NULL, &task) == AERORTOS_TASK_ERROR_NULL);

    config.stack = NULL;
    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_ERROR_NULL);
    config = make_config(stack, TEST_STACK_WORDS, 0U, NULL);
    config.entry = NULL;
    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_ERROR_ENTRY);

    config = make_config((uint32_t *)(void *)&stack_bytes[4],
                         TEST_STACK_WORDS - 1U, 0U, NULL);
    assert(aerortos_task_create(&config, &task) ==
           AERORTOS_TASK_ERROR_STACK_ALIGNMENT);

    config = make_config(stack,
                         (AERORTOS_TASK_MIN_STACK_BYTES / sizeof(uint32_t)) - 1U,
                         0U, NULL);
    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_ERROR_STACK_SIZE);
    config = make_config(stack, TEST_STACK_WORDS, 0U, NULL);
    config.stack_size_bytes -= sizeof(uint32_t);
    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_ERROR_STACK_SIZE);

    config = make_config(stack, TEST_STACK_WORDS,
                         (uint8_t)AERORTOS_PRIORITY_COUNT, NULL);
    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_ERROR_PRIORITY);
    assert(aerortos_task_count() == 0U);
}

static void test_duplicate_and_capacity_handling(void)
{
    alignas(AERORTOS_TASK_STACK_ALIGNMENT)
        uint32_t stacks[AERORTOS_TASK_CAPACITY + 1U][TEST_STACK_WORDS];
    aerortos_task_t *task = NULL;
    aerortos_task_config_t config;
    size_t index;

    aerortos_task_system_init();
    config = make_config(stacks[0], TEST_STACK_WORDS, 0U, NULL);
    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_OK);
    assert(aerortos_task_create(&config, &task) ==
           AERORTOS_TASK_ERROR_DUPLICATE_STACK);

    config = make_config(&stacks[0][2], TEST_STACK_WORDS - 2U, 1U, NULL);
    assert(aerortos_task_create(&config, &task) ==
           AERORTOS_TASK_ERROR_DUPLICATE_STACK);

    for (index = 1U; index < AERORTOS_TASK_CAPACITY; ++index) {
        config = make_config(stacks[index], TEST_STACK_WORDS,
                             (uint8_t)index, NULL);
        assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_OK);
    }
    assert(aerortos_task_count() == AERORTOS_TASK_CAPACITY);

    config = make_config(stacks[AERORTOS_TASK_CAPACITY], TEST_STACK_WORDS, 0U, NULL);
    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_ERROR_CAPACITY);
    assert(task == NULL);
}

static void test_lifecycle_release_and_guard(void)
{
    alignas(AERORTOS_TASK_STACK_ALIGNMENT) uint32_t stack[TEST_STACK_WORDS];
    aerortos_task_config_t config = make_config(stack, TEST_STACK_WORDS, 4U, NULL);
    aerortos_task_t *task = NULL;
    aerortos_task_t foreign_task = {0};
    size_t unused_words = 0U;

    aerortos_task_system_init();
    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_OK);
    assert(aerortos_task_release(task) == AERORTOS_TASK_ERROR_INVALID_TASK);
    assert(aerortos_task_transition(task, AERORTOS_TASK_RUNNING) == AERORTOS_TASK_OK);
    assert(aerortos_task_transition(task, AERORTOS_TASK_BLOCKED) == AERORTOS_TASK_OK);
    assert(aerortos_task_transition(task, AERORTOS_TASK_SUSPENDED) == AERORTOS_TASK_OK);
    assert(aerortos_task_transition(task, AERORTOS_TASK_READY) == AERORTOS_TASK_OK);
    assert(aerortos_task_transition(task, AERORTOS_TASK_READY) ==
           AERORTOS_TASK_ERROR_INVALID_TRANSITION);
    assert(aerortos_task_transition(task, AERORTOS_TASK_TERMINATED) ==
           AERORTOS_TASK_OK);
    assert(aerortos_task_transition(task, AERORTOS_TASK_READY) ==
           AERORTOS_TASK_ERROR_INVALID_TRANSITION);
    assert(aerortos_task_release(task) == AERORTOS_TASK_OK);
    assert(aerortos_task_count() == 0U);
    assert(aerortos_task_transition(task, AERORTOS_TASK_READY) ==
           AERORTOS_TASK_ERROR_INVALID_TASK);
    assert(aerortos_task_transition(&foreign_task, AERORTOS_TASK_READY) ==
           AERORTOS_TASK_ERROR_INVALID_TASK);

    assert(aerortos_task_create(&config, &task) == AERORTOS_TASK_OK);
    stack[0] = 0U;
    assert(!aerortos_task_stack_guard_intact(task));
    assert(aerortos_task_stack_unused_words(task, &unused_words) ==
           AERORTOS_TASK_ERROR_STACK_GUARD);
    assert(aerortos_task_transition(task, AERORTOS_TASK_RUNNING) ==
           AERORTOS_TASK_ERROR_STACK_GUARD);
}

int main(void)
{
    test_valid_task_and_initial_frame();
    test_invalid_creation();
    test_duplicate_and_capacity_handling();
    test_lifecycle_release_and_guard();
    puts("task_test: PASS");
    return 0;
}
