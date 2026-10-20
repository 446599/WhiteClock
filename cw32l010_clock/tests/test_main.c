#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "board.h"
#include "epd.h"
#include "settings.h"
#include "temperature.h"

#define main firmware_main
#include "../user/src/main.c"
#undef main

enum { ADVANCE_MINUTE=100, SCENARIOS=20 };
static const uint8_t events[SCENARIOS]={
    KEY_NONE, KEY_OK, ADVANCE_MINUTE, KEY_NEXT, ADVANCE_MINUTE,
    KEY_NEXT, ADVANCE_MINUTE, KEY_NEXT, ADVANCE_MINUTE, KEY_NEXT,
    ADVANCE_MINUTE, KEY_NEXT, ADVANCE_MINUTE, KEY_NEXT, ADVANCE_MINUTE,
    ADVANCE_MINUTE, KEY_NONE, KEY_NEXT, KEY_NONE, KEY_NEXT
};
static jmp_buf done;
static uint32_t tick, started, failed_at_tick;
static unsigned starts, completions, rejected;
static uint8_t key;
static bool active, frozen, noop, have_previous, late_sent, scheduled[SCENARIOS];
static ClockFace pending_face, previous_face;
static ClockTime rtc={26,10,4,12,34,0};
volatile EpdMetrics epd_metrics;

void board_init(void) {}
uint32_t board_millis(void) { return tick; }
uint8_t board_key(void) { uint8_t result=key; key=KEY_NONE; return result; }
bool board_rtc_init(void) { return true; }
bool board_rtc_read_time(ClockTime *time) { *time=rtc; return true; }
bool board_rtc_save_start(const ClockTime *time,bool date_only)
{ (void)time; (void)date_only; assert(!"Unexpected RTC save"); return false; }
int board_rtc_save_poll(void) { assert(!"Unexpected RTC save poll"); return 0; }
ClockSettings settings_load(void) { return clock_defaults(); }
bool settings_save(const ClockSettings *settings)
{ (void)settings; assert(!"Unexpected settings save"); return false; }
void temperature_init(void) {}
void temperature_poll(uint32_t now) { (void)now; }
bool temperature_read(int16_t *c10) { (void)c10; return false; }

bool epd_active(void) { return active; }
bool epd_work_ready(void) { return false; }
bool epd_start(const ClockFace *previous,const ClockFace *next,bool fast)
{
    assert(!active && starts==completions && starts<SCENARIOS);
    bool expected_fast=starts!=0u && starts!=14u && starts!=18u;
    assert(fast==expected_fast);
    assert((previous!=0)==have_previous);
    if (previous) assert(clock_face_equal(previous,&previous_face));
    ClockWindow window;
    bool changed=!previous || clock_changed_window(previous,next,&window);
    noop=fast && !changed;
    bool expected_noop=(starts>=2u && starts<=12u && !(starts&1u)) || starts==15u;
    assert(noop==expected_noop);
    if (starts==14u) assert(!changed); /* Seven fast waveforms force a full update. */
    if (starts==16u) assert(next->cursor!=previous->cursor);
    if (starts==18u) assert(tick-failed_at_tick>=60000u);
    pending_face=*next; frozen=false; active=true; started=tick;
    epd_metrics=(EpdMetrics){0}; ++starts;
    return true;
}
bool epd_retarget(const ClockFace *next)
{
    assert(active);
    if (frozen) { ++rejected; return false; }
    pending_face=*next;
    return true;
}
int epd_poll(void)
{
    if (!active) return 0;
    if (tick-started>=10u) frozen=true;
    if (tick-started<40u) return 0;
    unsigned index=starts-1u;
    active=false; ++completions;
    epd_metrics.refreshed=!noop;
    if (index==17u) {
        have_previous=false; failed_at_tick=tick;
        return -1;
    }
    have_previous=true; previous_face=pending_face;
    return 1;
}
void test_wait(void)
{
    assert(tick<100000u && starts<=SCENARIOS);
    if (completions==SCENARIOS) {
        assert(epd_successes==19u && epd_failures==1u && !rtc_failures);
        assert(late_sent && rejected && have_previous);
        longjmp(done,1);
    }
    if (active && starts-1u==15u && tick-started>=10u && !late_sent) {
        assert(frozen);
        key=KEY_NEXT; late_sent=true;
    }
    if (!active && starts==completions && !scheduled[completions]) {
        uint8_t event=events[completions];
        if (event==ADVANCE_MINUTE) ++rtc.minute;
        else key=event;
        scheduled[completions]=true;
    }
    tick+=10u;
}
int main(void)
{
    if (!setjmp(done)) firmware_main();
    puts("PASS: real main, no-op metadata, frozen retarget, seven-fast/full sequencing and failure recovery (20 scenarios)");
    return 0;
}
