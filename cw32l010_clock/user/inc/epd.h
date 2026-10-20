#ifndef EPD_H
#define EPD_H
#include "clock_core.h"
#ifndef EPD_PANEL_INVERT_DATA
#define EPD_PANEL_INVERT_DATA 1
#endif
#ifndef EPD_FAST_BORDER_DATA
/* DDX=00 matches black=1 on the wire; VBD=01 selects the B->W border LUT. */
#define EPD_FAST_BORDER_DATA 0x47u
#endif
#ifndef EPD_FULL_BORDER_DATA
/* Match the hardware-accepted WhiteClock border setting in both waveforms. */
#define EPD_FULL_BORDER_DATA 0x47u
#endif
#ifndef EPD_USE_FAST_LUT
#define EPD_USE_FAST_LUT 1
#endif
/* Full-frame fast-waveform diagnostic: cropped windows failed on the device. */
#ifndef EPD_USE_PARTIAL_WINDOW
#define EPD_USE_PARTIAL_WINDOW 0
#endif
#if !EPD_USE_FAST_LUT && EPD_USE_PARTIAL_WINDOW
#error "Full-waveform diagnostic requires full-frame OLD/NEW data"
#endif
typedef struct {
    uint32_t setup_ms,old_ms,new_ms,busy_ms,finish_ms,total_ms;
    uint32_t image_bytes,rendered_rows,max_render_ms;
    uint32_t refreshed;
} EpdMetrics;
extern volatile EpdMetrics epd_metrics;
bool epd_work_ready(void);
bool epd_retarget(const ClockFace *next);
bool epd_start(const ClockFace *previous,const ClockFace *next,bool fast);
/* 0 busy, 1 successful completion, -1 failed completion. */
int epd_poll(void);
bool epd_active(void);
#endif
