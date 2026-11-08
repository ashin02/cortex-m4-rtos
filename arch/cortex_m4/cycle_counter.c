#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/cycle_counter.h"
#include "aerortos/platform.h"

#define AERORTOS_DEMCR (*(volatile uint32_t *)UINT32_C(0xE000EDFC))
#define AERORTOS_DWT_CTRL (*(volatile uint32_t *)UINT32_C(0xE0001000))
#define AERORTOS_DWT_CYCCNT (*(volatile uint32_t *)UINT32_C(0xE0001004))
#define AERORTOS_DEMCR_TRCENA_Msk (UINT32_C(1) << 24)
#define AERORTOS_DWT_CTRL_CYCCNTENA_Msk (UINT32_C(1) << 0)

static bool counter_ready;

bool aerortos_cycle_counter_init(uint32_t expected_core_clock_hz)
{
    if ((expected_core_clock_hz == 0U) ||
        (SystemCoreClock != expected_core_clock_hz)) {
        counter_ready = false;
        return false;
    }
#if defined(AERORTOS_HOST_TEST)
    extern bool aerortos_host_dwt_available;

    counter_ready = aerortos_host_dwt_available;
#else
    uint32_t before;
    uint32_t after;

    AERORTOS_DEMCR |= AERORTOS_DEMCR_TRCENA_Msk;
    AERORTOS_DWT_CYCCNT = 0U;
    AERORTOS_DWT_CTRL |= AERORTOS_DWT_CTRL_CYCCNTENA_Msk;
    __asm volatile ("dsb\n isb" ::: "memory");
    before = AERORTOS_DWT_CYCCNT;
    __asm volatile ("nop\n nop\n nop\n nop" ::: "memory");
    after = AERORTOS_DWT_CYCCNT;
    counter_ready =
        ((AERORTOS_DEMCR & AERORTOS_DEMCR_TRCENA_Msk) != 0U) &&
        ((AERORTOS_DWT_CTRL & AERORTOS_DWT_CTRL_CYCCNTENA_Msk) != 0U) &&
        (after != before);
#endif
    return counter_ready;
}

bool aerortos_cycle_counter_is_ready(void)
{
    return counter_ready;
}

uint32_t aerortos_cycle_counter_now(void)
{
#if defined(AERORTOS_HOST_TEST)
    extern uint32_t aerortos_host_cycle_counter;
    extern uint32_t aerortos_host_cycle_counter_step;
    const uint32_t value = aerortos_host_cycle_counter;

    aerortos_host_cycle_counter += aerortos_host_cycle_counter_step;
    return value;
#else
    return AERORTOS_DWT_CYCCNT;
#endif
}

uint32_t aerortos_cycle_counter_elapsed(uint32_t start, uint32_t end)
{
    return end - start;
}

bool aerortos_cycles_to_microseconds(
    uint32_t cycles,
    uint32_t core_clock_hz,
    uint64_t *microseconds_out)
{
    if ((core_clock_hz == 0U) || (microseconds_out == NULL)) {
        return false;
    }
    *microseconds_out =
        ((uint64_t)cycles * UINT64_C(1000000)) / core_clock_hz;
    return true;
}
