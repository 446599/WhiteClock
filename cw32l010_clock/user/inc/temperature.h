#ifndef TEMPERATURE_H
#define TEMPERATURE_H
#include <stdbool.h>
#include <stdint.h>
/* Output in 0.1 Celsius. Uses factory trim and measured VDD via internal Vref. */
bool temperature_convert(uint16_t ts,uint16_t vref,uint8_t t0,uint16_t trim,int16_t *c10);
void temperature_init(void);
void temperature_poll(uint32_t now);
bool temperature_read(int16_t *c10);
#endif
