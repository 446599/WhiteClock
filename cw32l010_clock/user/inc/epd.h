#ifndef EPD_H
#define EPD_H
#include "clock_core.h"
bool epd_start(const ClockFace *previous,const ClockFace *next,bool fast);
/* 0 busy, 1 successful completion, -1 failed completion. */
int epd_poll(void);
bool epd_active(void);
#endif
