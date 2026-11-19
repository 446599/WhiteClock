#ifndef CLOCK_POWER_H
#define CLOCK_POWER_H
#include <stdbool.h>
#include <stdint.h>
void power_idle(bool allow_deep);
extern volatile uint32_t power_deep_entries, power_sleep_ms, power_clock_failures;
#endif
