#ifndef AERORTOS_CYCLE_COUNTER_H
#define AERORTOS_CYCLE_COUNTER_H

#include <stdbool.h>
#include <stdint.h>

bool aerortos_cycle_counter_init(uint32_t expected_core_clock_hz);
bool aerortos_cycle_counter_is_ready(void);
uint32_t aerortos_cycle_counter_now(void);
uint32_t aerortos_cycle_counter_elapsed(uint32_t start, uint32_t end);
bool aerortos_cycles_to_microseconds(
    uint32_t cycles,
    uint32_t core_clock_hz,
    uint64_t *microseconds_out);

#endif
