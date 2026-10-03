#ifndef SETTINGS_H
#define SETTINGS_H
#include "clock_core.h"
ClockSettings settings_load(void);
bool settings_save(const ClockSettings *settings);
/* Storage backend, also substituted by power-loss tests. */
const uint8_t *settings_flash_read(uint32_t address);
bool settings_flash_erase(uint32_t address);
bool settings_flash_program(uint32_t address,uint8_t value);
#endif
