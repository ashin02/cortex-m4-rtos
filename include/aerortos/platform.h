#ifndef AERORTOS_PLATFORM_H
#define AERORTOS_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

extern uint32_t SystemCoreClock;
extern volatile uint32_t aerortos_fault_code;

bool system_clock_init(void);
bool systick_heartbeat_init(void);
uint32_t bsp_millis(void);
void gpio_led_init(void);
void gpio_led_toggle(void);
void platform_panic(uint32_t code) __attribute__((noreturn));

void Reset_Handler(void) __attribute__((noreturn));
void Default_Handler(void) __attribute__((noreturn));
void NMI_Handler(void) __attribute__((noreturn));
void HardFault_Handler(void) __attribute__((noreturn));
void MemManage_Handler(void) __attribute__((noreturn));
void BusFault_Handler(void) __attribute__((noreturn));
void UsageFault_Handler(void) __attribute__((noreturn));
void SysTick_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);

#endif
