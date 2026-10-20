#ifndef BOARD_H
#define BOARD_H
#include <stdint.h>
#include <stdbool.h>
#include "clock_core.h"
/* Match the enclosure labels, active low. */
#define BUTTON_SET_PIN (1u << 3)  /* PB3 */
#define BUTTON_BACK_PIN (1u << 2) /* PB2 */
void board_init(void);
uint32_t board_millis(void);
void board_delay(uint32_t ms);
uint8_t board_key(void);
/* Called by GPIOB_IRQHandler with the active-low button edge mask. */
void board_key_gpio_irq(uint32_t pins);
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
