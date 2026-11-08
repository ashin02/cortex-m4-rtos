#include <stdint.h>

#include "aerortos/critical_section.h"

#if defined(AERORTOS_HOST_TEST)
extern uint32_t aerortos_host_basepri;
#endif

aerortos_critical_state_t aerortos_critical_enter(void)
{
#if defined(AERORTOS_HOST_TEST)
    const uint32_t previous = aerortos_host_basepri;

    if ((previous == 0U) || (previous > AERORTOS_KERNEL_BASEPRI)) {
        aerortos_host_basepri = AERORTOS_KERNEL_BASEPRI;
    }
    return previous;
#else
    uint32_t previous;
    const uint32_t threshold = AERORTOS_KERNEL_BASEPRI;

    __asm volatile (
        "mrs %0, basepri\n"
        "msr basepri_max, %1\n"
        "dsb\n"
        "isb\n"
        : "=&r" (previous)
        : "r" (threshold)
        : "memory");
    return previous;
#endif
}

void aerortos_critical_exit(aerortos_critical_state_t previous_basepri)
{
#if defined(AERORTOS_HOST_TEST)
    aerortos_host_basepri = previous_basepri;
#else
    __asm volatile (
        "msr basepri, %0\n"
        "dsb\n"
        "isb\n"
        :
        : "r" (previous_basepri)
        : "memory");
#endif
}
