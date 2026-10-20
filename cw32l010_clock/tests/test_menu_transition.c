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

enum { STARTUP, STABLE, UPLOAD_OLD, UPLOAD_NEW, WAVEFORM, CASES };
static jmp_buf done;
static unsigned scenario, theme, dark, calls, tick, sent_at, menu_at;
static unsigned old_bytes, new_bytes, refreshes;
static bool sent, active, data, advance;
static uint8_t command, payload[56], old_wire[5624], new_wire[5624], physical[5624], menu[5624];
static unsigned length;
static ClockTime rtc;

void board_init(void)
{
    calls=tick=sent_at=menu_at=old_bytes=new_bytes=refreshes=0;
    sent=active=data=advance=false; command=0;
    rtc=(ClockTime){26,10,20,14,36,0};
    ClockSettings settings=clock_defaults();settings.theme=theme;settings.invert_enabled=dark;
    settings.start_hour=settings.end_hour=0;settings.start_minute=settings.end_minute=0;
    ClockUI ui;clock_ui_init(&ui,&rtc,&settings);ui.page=PAGE_MENU;
    ClockFace face=clock_ui_face(&ui);
    for (unsigned y=0;y<PANEL_HEIGHT;++y) {
        clock_render_row(&face,y,menu+y*PANEL_ROW_BYTES);
        if (EPD_PANEL_INVERT_DATA)
            for (unsigned x=0;x<PANEL_ROW_BYTES;++x) menu[y*PANEL_ROW_BYTES+x]^=0xffu;
    }
    clock_stage=epd_successes=epd_failures=rtc_failures=0;
}
uint32_t board_millis(void)
{
    if (++calls%8u==0u) ++tick;
    assert(tick<30000u);
    if (menu_at && !epd_active() && tick-menu_at>100u) {
        assert(sent && !epd_failures && !memcmp(physical,menu,sizeof menu));
        longjmp(done,1);
    }
    return tick;
}
uint8_t board_key(void)
{
    bool ready=scenario==STARTUP ? tick>=15u : scenario==STABLE ? epd_successes && tick>3000u :
        epd_successes && (scenario==UPLOAD_OLD ? command==0x10 && old_bytes>100u :
        scenario==UPLOAD_NEW ? command==0x13 && new_bytes>100u : command==0x12);
    if (!sent && ready) { sent=true;sent_at=tick;return KEY_OK; }
    return KEY_NONE;
}
bool board_rtc_init(void) { return true; }
bool board_rtc_read_time(ClockTime *time)
{
    if (scenario>=UPLOAD_OLD && epd_successes && !advance) { ++rtc.minute;advance=true; }
    *time=rtc;return true;
}
bool board_rtc_save_start(const ClockTime *time,bool date_only)
{ (void)time;(void)date_only;assert(0);return false; }
int board_rtc_save_poll(void) { assert(0);return 0; }
ClockSettings settings_load(void)
{
    ClockSettings settings=clock_defaults();settings.theme=theme;settings.invert_enabled=dark;
    settings.start_hour=settings.end_hour=0;settings.start_minute=settings.end_minute=0;
    return settings;
}
bool settings_save(const ClockSettings *settings) { (void)settings;assert(0);return false; }
void temperature_init(void) {}
void temperature_poll(uint32_t now) { (void)now; }
bool temperature_read(int16_t *c10) { (void)c10;return false; }
bool board_epd_busy(void) { return false; }
void board_epd_reset(bool high) { (void)high; }
bool board_epd_begin(bool is_data,const uint8_t *bytes,unsigned count)
{
    assert(!active && count<=sizeof payload);
    active=true;data=is_data;length=count;memcpy(payload,bytes,count);return true;
}
int board_epd_poll(void)
{
    assert(active);active=false;
    if (!data) {
        command=payload[0];
        if (command==0x10) old_bytes=0;
        if (command==0x13) new_bytes=0;
        if (command==0x12) {
            assert(old_bytes==sizeof old_wire && new_bytes==sizeof new_wire);
            if (refreshes) assert(!memcmp(old_wire,physical,sizeof physical));
            memcpy(physical,new_wire,sizeof physical);++refreshes;
            if (sent && !memcmp(physical,menu,sizeof menu)) menu_at=tick;
        }
    } else if (command==0x10 || command==0x13) {
        unsigned *count=command==0x10 ? &old_bytes : &new_bytes;
        uint8_t *wire=command==0x10 ? old_wire : new_wire;
        assert(*count+length<=sizeof old_wire);
        memcpy(wire+*count,payload,length);*count+=length;
    }
    return 1;
}
void test_wait(void) { ++tick; }
int main(void)
{
    for (theme=0;theme<THEME_COUNT;++theme)
        for (dark=0;dark<2u;++dark)
            for (scenario=0;scenario<CASES;++scenario) {
                if (!setjmp(done)) firmware_main();
                assert(menu_at>=sent_at && menu_at-sent_at<10000u);
            }
    puts("PASS: one SET enters the menu without BACK (70 real main/EPD/wire transitions)");
    return 0;
}
