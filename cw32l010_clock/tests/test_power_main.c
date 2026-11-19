#include "main.h"
#include "board.h"
#include "epd.h"
#include "settings.h"
#include "temperature.h"
#include "power.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#define main firmware_main
#include "../user/src/main.c"
#undef main

TestSysTick test_tick;
TestSCB test_scb;
TestSYSCTRL test_sys;
uint32_t test_mask;
volatile EpdMetrics epd_metrics;
static jmp_buf done;
static uint32_t ms,real_ms,fraction,started,last_key,focus_started,pause_at,resume_at,completed;
static unsigned phase,scenario,deep,shallow,flashes;
static uint8_t queued,pending_key;
static bool active,valid;
static ClockFace pending,shown;

void board_init(void)
{
    ms=real_ms=fraction=started=last_key=focus_started=pause_at=resume_at=completed=0;
    phase=deep=shallow=flashes=queued=pending_key=test_mask=0;
    active=valid=false;memset(&pending,0,sizeof pending);memset(&shown,0,sizeof shown);
    test_tick=(TestSysTick){7u,23999u,731u};test_scb.SCR=0;
    test_sys=(TestSYSCTRL){0x3ffu,0xc08u};
    power_deep_entries=power_sleep_ms=power_clock_failures=0;
    epd_successes=epd_failures=rtc_failures=clock_key_events=0;
}
uint32_t board_millis(void) { return ms; }
bool board_sleep_ready(void) { return !queued && ms-last_key>=30u; }
bool board_rtc_sleep_ticks(uint32_t *ticks)
{
    *ticks=(uint32_t)(((uint64_t)real_ms*BOARD_RTC_HZ/1000u)%BOARD_RTC_MINUTE_TICKS);
    return true;
}
uint32_t board_sleep_advance(uint32_t ticks)
{
    uint32_t value=ticks*1000u+fraction,elapsed=value/BOARD_RTC_HZ;
    fraction=value%BOARD_RTC_HZ;ms+=elapsed;return elapsed;
}
void test_disable_irq(void) { test_mask=1; }
void test_set_mask(uint32_t mask)
{
    test_mask=mask;
    if (!mask && pending_key) {
        assert(test_tick.CTRL==7u && (test_sys.APBEN1&0xffffu)==0x3ffu);
        queued=pending_key;pending_key=0;last_key=ms;
    }
}
uint8_t board_key(void)
{
    uint8_t key=queued;queued=0;
    if (key==KEY_OK && shown.page==PAGE_FOCUS) {
        if (shown.focus_state==FOCUS_IDLE) focus_started=real_ms;
        if (shown.focus_state==FOCUS_RUNNING) pause_at=real_ms;
        if (shown.focus_state==FOCUS_PAUSED) resume_at=real_ms;
    }
    return key;
}
bool board_rtc_init(void) { return true; }
bool board_rtc_read_time(ClockTime *time)
{
    *time=(ClockTime){26,11,2,12+(real_ms/3600000u),real_ms/60000u%60u,real_ms/1000u%60u};
    return true;
}
bool board_rtc_save_start(const ClockTime *time,bool date_only)
{ (void)time;(void)date_only;assert(0);return false; }
int board_rtc_save_poll(void) { assert(0);return 0; }
ClockSettings settings_load(void) { return clock_defaults(); }
bool settings_save(const ClockSettings *settings) { (void)settings;assert(0);return false; }
void temperature_init(void) {}
void temperature_poll(uint32_t now) { (void)now; }
bool temperature_active(void)
{ return real_ms<20u || (real_ms>=3000u && real_ms<3080u); }
bool temperature_read(int16_t *c10) { (void)c10;return false; }
bool epd_active(void) { return active; }
bool epd_work_ready(void) { return false; }
bool epd_retarget(const ClockFace *face)
{ if (!active || ms-started>=10u) return false;pending=*face;return true; }
bool epd_start(const ClockFace *previous,const ClockFace *next,bool fast)
{
    assert(!active && test_tick.CTRL==7u);
    if (valid) assert(previous && clock_face_equal(previous,&shown));
    if (next->focus_state==FOCUS_DONE) {
        assert(!fast);
        if (!completed) completed=real_ms;
    }
    pending=*next;active=true;started=ms;return true;
}
int epd_poll(void)
{
    if (!active || ms-started<40u) return 0;
    shown=pending;active=false;valid=true;epd_metrics.refreshed=1;
    if (shown.focus_state==FOCUS_DONE) ++flashes;
    return 1;
}
void test_wfi(void)
{
    assert(real_ms<1600000u);
    if (completed && flashes==FOCUS_ALERT_FLASHES && !active) {
        uint32_t expected=focus_started+25u*60000u+(resume_at-pause_at);
        assert(completed>=expected-2u && completed<=expected+1100u);
        assert(!epd_failures && !rtc_failures && !power_clock_failures);
        assert(deep>1000u && deep<1800u && shallow<100000u && power_sleep_ms>1400000u);
        longjmp(done,1);
    }
    uint32_t step=1,event_at=0;uint8_t event=0;
    if (valid && !active) {
        if (phase==0) { event_at=real_ms+123u;event=KEY_NEXT; }
        if (phase==1 && shown.page==PAGE_FOCUS) { event_at=real_ms+1u;event=KEY_OK; }
        if (scenario && phase==2 && focus_started) { event_at=focus_started+60123u;event=KEY_OK; }
        if (scenario && phase==3 && pause_at) { event_at=pause_at+37000u;event=KEY_OK; }
    }
    if (test_scb.SCR & SCB_SCR_SLEEPDEEP_Msk) {
        assert(test_mask==1u && test_tick.CTRL==0 && !active && !temperature_active());
        ++deep;step=1000u-real_ms%1000u;
    } else { assert(!test_mask && test_tick.CTRL==7u);++shallow; }
    if (event && event_at<=real_ms+step) {
        if (event_at>real_ms) step=event_at-real_ms;
        else step=1;
        if (test_mask) pending_key=event;
        else { queued=event;last_key=ms; }
        ++phase;
    }
    real_ms+=step;
    if (!test_mask) ms+=step;
}
int main(void)
{
    for (scenario=0;scenario<2u;++scenario)
        if (!setjmp(done)) firmware_main();
    puts("PASS: real main plus deep-sleep path, fractional key wake, 25-minute focus, pause/resume and five full alerts");
}
