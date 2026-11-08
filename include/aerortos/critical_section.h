#ifndef AERORTOS_CRITICAL_SECTION_H
#define AERORTOS_CRITICAL_SECTION_H

#include <stdint.h>

#define AERORTOS_NVIC_PRIORITY_BITS 4U
#define AERORTOS_SVC_PRIORITY UINT32_C(0x20)
#define AERORTOS_KERNEL_BASEPRI UINT32_C(0x40)
#define AERORTOS_PENDSV_PRIORITY UINT32_C(0xF0)

typedef uint32_t aerortos_critical_state_t;

aerortos_critical_state_t aerortos_critical_enter(void);
void aerortos_critical_exit(aerortos_critical_state_t previous_basepri);

#endif
