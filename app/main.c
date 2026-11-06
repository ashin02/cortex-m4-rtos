#include <stdalign.h>
#include <stdint.h>

#include "aerortos/cortex_m4.h"
#include "aerortos/platform.h"
#include "aerortos/task.h"

enum {
    CLOCK_INIT_FAILURE = 0xC10C0001UL,
    SYSTICK_INIT_FAILURE = 0x51C70001UL,
    TASK_INIT_FAILURE = 0x7A5C0002UL,
    M2_SMOKE_STACK_WORDS = 64UL,
    HEARTBEAT_HALF_PERIOD_MS = 500UL
};

aerortos_task_t *aerortos_m2_smoke_task;
alignas(AERORTOS_TASK_STACK_ALIGNMENT)
    uint32_t aerortos_m2_smoke_stack[M2_SMOKE_STACK_WORDS];

int main(void);

static void m2_smoke_task_entry(void *argument)
{
    (void)argument;
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

int main(void)
{
    const aerortos_task_config_t smoke_task_config = {
        .entry = m2_smoke_task_entry,
        .argument = NULL,
        .stack = aerortos_m2_smoke_stack,
        .stack_size_bytes = sizeof(aerortos_m2_smoke_stack),
        .priority = 0U
    };
    uint32_t previous_toggle;

    if (!system_clock_init()) {
        platform_panic(CLOCK_INIT_FAILURE);
    }

    aerortos_task_system_init();
    if (aerortos_task_create(&smoke_task_config, &aerortos_m2_smoke_task) !=
        AERORTOS_TASK_OK) {
        platform_panic(TASK_INIT_FAILURE);
    }

    gpio_led_init();
    if (!systick_heartbeat_init()) {
        platform_panic(SYSTICK_INIT_FAILURE);
    }

    previous_toggle = bsp_millis();
    for (;;) {
        const uint32_t now = bsp_millis();

        if ((uint32_t)(now - previous_toggle) >= HEARTBEAT_HALF_PERIOD_MS) {
            previous_toggle += HEARTBEAT_HALF_PERIOD_MS;
            gpio_led_toggle();
        }
        aerortos_wait_for_interrupt();
    }
}
