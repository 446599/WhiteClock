#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>
#include <stdbool.h>
#include "clock_core.h"
void board_init(void);
uint32_t board_millis(void);
void board_delay(uint32_t ms);
uint8_t board_key(void);
bool board_epd_busy(void);
void board_epd_reset(bool high);
/* begin retains bytes until poll returns 1/-1; no caller stack buffers. */
bool board_epd_begin(bool data, const uint8_t *bytes, unsigned length);
int board_epd_poll(void);
bool board_rtc_init(void);
bool board_rtc_read(uint8_t *h, uint8_t *m, uint8_t *s);
bool board_rtc_set(uint8_t h, uint8_t m);
bool board_rtc_read_time(ClockTime *time);
bool board_rtc_save_start(const ClockTime *time, bool date_only);
int board_rtc_save_poll(void);
bool board_rtc_save(const ClockTime *time, bool date_only);
extern volatile uint32_t key_overflows;
#endif
