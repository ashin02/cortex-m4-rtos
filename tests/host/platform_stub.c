#include <stdint.h>
#include <stdlib.h>

#include "aerortos/platform.h"

uint32_t SystemCoreClock;
volatile uint32_t aerortos_fault_code;

void platform_panic(uint32_t code)
{
    aerortos_fault_code = code;
    abort();
}
