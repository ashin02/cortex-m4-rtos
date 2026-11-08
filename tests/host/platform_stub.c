#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "aerortos/platform.h"

uint32_t SystemCoreClock;
volatile uint32_t aerortos_fault_code;
uint32_t aerortos_host_basepri;
uint32_t aerortos_host_pendsv_requests;
bool aerortos_host_handler_mode;
void (*aerortos_host_queue_block_callback)(void);
void (*aerortos_host_mutex_block_callback)(void);

void aerortos_host_queue_block_hook(void);
void aerortos_host_mutex_block_hook(void);

void aerortos_host_queue_block_hook(void)
{
    if (aerortos_host_queue_block_callback == NULL) {
        abort();
    }
    aerortos_host_queue_block_callback();
}

void aerortos_host_mutex_block_hook(void)
{
    if (aerortos_host_mutex_block_callback == NULL) {
        abort();
    }
    aerortos_host_mutex_block_callback();
}

void platform_panic(uint32_t code)
{
    aerortos_fault_code = code;
    abort();
}
