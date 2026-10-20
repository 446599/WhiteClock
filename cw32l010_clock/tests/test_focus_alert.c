#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "epd.h"
#include "settings.h"
#include "temperature.h"

#define main firmware_main
#include "../user/src/main.c"
#undef main

enum { NORMAL, BACKGROUND, EDITOR, CANCEL_OK, CANCEL_NEXT, CANCEL_BACK, FAILURE, NO_WAVEFORM, ALREADY_BUSY, CASE_COUNT };
static jmp_buf done;
static struct {
    uint32_t tick, started, failed_at;
    unsigned scenario, phase, attempts, flashes, loops, quiet;
    uint8_t key;
    bool active, frozen, valid, expired, cancelled, alert;
    ClockFace pending, displayed;
} test;
volatile EpdMetrics epd_metrics;

void board_init(void) {}
uint32_t board_millis(void) { return test.tick; }
uint8_t board_key(void) { uint8_t key=test.key;test.key=KEY_NONE;return key; }
bool board_rtc_init(void) { return true; }
bool board_rtc_read_time(ClockTime *time) { *time=(ClockTime){26,10,20,12,34,0};return true; }
bool board_rtc_save_start(const ClockTime *time,bool date_only)
{ (void)time;(void)date_only;assert(!"Unexpected RTC save");return false; }
int board_rtc_save_poll(void) { assert(!"Unexpected RTC save poll");return 0; }
ClockSettings settings_load(void) { return clock_defaults(); }
bool settings_save(const ClockSettings *settings)
{ (void)settings;assert(!"Unexpected settings save");return false; }
void temperature_init(void) {}
void temperature_poll(uint32_t now) { (void)now; }
bool temperature_read(int16_t *c10) { (void)c10;return false; }
bool epd_active(void) { return test.active; }
bool epd_work_ready(void) { return false; }
bool epd_start(const ClockFace *previous,const ClockFace *next,bool fast)
{
    assert(!test.active && (previous!=0)==test.valid);
    if (previous) assert(clock_face_equal(previous,&test.displayed));
    test.alert=test.expired && !test.cancelled;
    if (test.alert) {
        assert(!fast);
        assert(next->page==(test.scenario==EDITOR ? PAGE_TIME : PAGE_FOCUS));
        if (next->page==PAGE_FOCUS) assert(next->focus_state==FOCUS_DONE);
        else assert(next->time.hour==12 && next->time.minute==34);
        if (test.failed_at) assert(test.tick-test.failed_at>=60000u);
        assert(++test.attempts<=FOCUS_ALERT_FLASHES+1u);
    }
    test.pending=*next;test.active=true;test.frozen=false;test.started=test.tick;
    epd_metrics=(EpdMetrics){0};
    return true;
}
bool epd_retarget(const ClockFace *next)
{
    assert(test.active);
    if (test.frozen) return false;
    test.pending=*next;return true;
}
int epd_poll(void)
{
    if (!test.active) return 0;
    if (test.tick-test.started>=10u) test.frozen=true;
    if (test.tick-test.started<40u) return 0;
    test.active=false;epd_metrics.refreshed=1;
    if (test.alert && test.scenario==FAILURE && test.attempts==1u) {
        test.valid=false;test.failed_at=test.tick;return -1;
    }
    if (test.alert && test.scenario==NO_WAVEFORM && test.attempts==1u) epd_metrics.refreshed=0;
    if (test.alert && epd_metrics.refreshed) ++test.flashes;
    test.valid=true;test.displayed=test.pending;return 1;
}
static void expire(void) { test.tick+=25u*60000u;test.expired=true;test.phase=3; }
void test_wait(void)
{
    assert(++test.loops<20000u);
    if (!test.active) {
        if (test.phase==0u && test.valid) { test.key=KEY_NEXT;test.phase=1; }
        else if (test.phase==1u && test.displayed.page==PAGE_FOCUS) {
            assert(test.displayed.focus_state==FOCUS_IDLE);
            test.key=KEY_OK;test.phase=2;
        } else if (test.phase==2u && test.displayed.focus_state==FOCUS_RUNNING) {
            if (test.scenario==BACKGROUND || test.scenario==EDITOR) { test.key=KEY_BACK;test.phase=4; }
            else expire();
        } else if (test.phase==4u && test.displayed.page==PAGE_HOME) {
            if (test.scenario==BACKGROUND) expire();
            else { test.key=KEY_OK;test.phase=5; }
        } else if (test.phase==5u && test.displayed.page==PAGE_MENU) { test.key=KEY_OK;test.phase=6; }
        else if (test.phase==6u && test.displayed.page==PAGE_TIME) expire();
    }
    if (test.scenario==ALREADY_BUSY && test.phase==2u && test.active && test.frozen) expire();
    bool cancel=test.scenario>=CANCEL_OK && test.scenario<=CANCEL_BACK;
    if (cancel && !test.cancelled && test.attempts==2u && test.active && test.frozen) {
        static const uint8_t keys[]={KEY_OK,KEY_NEXT,KEY_BACK};
        test.key=keys[test.scenario-CANCEL_OK];test.cancelled=true;
    }
    unsigned expected=cancel ? 2u : FOCUS_ALERT_FLASHES;
    if (test.phase==3u && !test.active && test.flashes==expected && (!cancel || test.cancelled)) {
        if (++test.quiet==200u) {
            assert(test.attempts==expected+((test.scenario==FAILURE || test.scenario==NO_WAVEFORM) ? 1u : 0u));
            assert(epd_failures==(test.scenario==FAILURE ? 1u : 0u));
            assert(!rtc_failures);
            longjmp(done,1);
        }
    }
    test.tick+=10u;
}
int main(void)
{
    for (unsigned scenario=0;scenario<CASE_COUNT;++scenario) {
        memset(&test,0,sizeof test);test.scenario=scenario;
        epd_successes=epd_failures=rtc_failures=main_loop_max_ms=epd_poll_max_ms=0;
        if (!setjmp(done)) firmware_main();
    }
    puts("PASS: real main, five full timer flashes, background/editor expiry, in-flight expiry, cancellation, retry and no-waveform accounting (9 scenarios)");
    return 0;
}
