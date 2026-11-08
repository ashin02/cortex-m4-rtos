#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/cortex_m4.h"
#include "aerortos/platform.h"
#include "aerortos/port.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"

enum {
    CLOCK_INIT_FAILURE = 0xC10C0001UL,
    SYSTICK_INIT_FAILURE = 0x51C70001UL,
    TASK_INIT_FAILURE = 0x7A5C0002UL,
    SCHEDULER_INIT_FAILURE = 0x7A5C0004UL,
    M6_TASK_STACK_WORDS = 96UL,
    HEARTBEAT_HALF_PERIOD_MS = 500UL
};

aerortos_task_t *aerortos_m6_idle_task;
aerortos_task_t *aerortos_m6_heartbeat_task;
aerortos_task_t *aerortos_m6_peer_task;
alignas(AERORTOS_TASK_STACK_ALIGNMENT)
    uint32_t aerortos_m6_idle_stack[M6_TASK_STACK_WORDS];
alignas(AERORTOS_TASK_STACK_ALIGNMENT)
    uint32_t aerortos_m6_heartbeat_stack[M6_TASK_STACK_WORDS];
alignas(AERORTOS_TASK_STACK_ALIGNMENT)
    uint32_t aerortos_m6_peer_stack[M6_TASK_STACK_WORDS];

int main(void);

static void idle_task_entry(void *argument)
{
    (void)argument;
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

static void heartbeat_task_entry(void *argument)
{
    (void)argument;
    for (;;) {
        gpio_led_toggle();
        (void)aerortos_periodic_wait_next();
    }
}

static void peer_task_entry(void *argument)
{
    (void)argument;
    for (;;) {
        (void)aerortos_periodic_wait_next();
    }
}

static aerortos_task_t *create_task(
    aerortos_task_entry_t entry,
    uint32_t *stack,
    size_t stack_bytes,
    uint8_t priority,
    uint32_t period,
    uint32_t deadline,
    uint32_t initial_delay)
{
    aerortos_task_config_t config;
    aerortos_task_t *task = NULL;

    config.entry = entry;
    config.argument = NULL;
    config.stack = stack;
    config.stack_size_bytes = stack_bytes;
    config.priority = priority;
    config.period_ticks = period;
    config.relative_deadline_ticks = deadline;
    config.initial_release_delay_ticks = initial_delay;

    if (aerortos_task_create(&config, &task) != AERORTOS_TASK_OK) {
        platform_panic(TASK_INIT_FAILURE);
    }
    return task;
}

int main(void)
{
    if (!system_clock_init()) {
        platform_panic(CLOCK_INIT_FAILURE);
    }
    aerortos_task_system_init();
    aerortos_time_system_init(0U);
    gpio_led_init();
    aerortos_m6_idle_task = create_task(
        idle_task_entry, aerortos_m6_idle_stack,
        sizeof(aerortos_m6_idle_stack), AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    aerortos_m6_heartbeat_task = create_task(
        heartbeat_task_entry, aerortos_m6_heartbeat_stack,
        sizeof(aerortos_m6_heartbeat_stack), 5U,
        HEARTBEAT_HALF_PERIOD_MS, HEARTBEAT_HALF_PERIOD_MS,
        HEARTBEAT_HALF_PERIOD_MS);
    aerortos_m6_peer_task = create_task(
        peer_task_entry, aerortos_m6_peer_stack,
        sizeof(aerortos_m6_peer_stack), 6U, 100U, 100U, 100U);

    aerortos_scheduler_init(aerortos_m6_idle_task);
    if ((aerortos_scheduler_add(aerortos_m6_heartbeat_task) !=
         AERORTOS_SCHEDULER_OK) ||
        (aerortos_scheduler_add(aerortos_m6_peer_task) !=
         AERORTOS_SCHEDULER_OK)) {
        platform_panic(SCHEDULER_INIT_FAILURE);
    }
    aerortos_port_init();
    if (!aerortos_systick_init()) {
        platform_panic(SYSTICK_INIT_FAILURE);
    }
    aerortos_scheduler_start();
}
