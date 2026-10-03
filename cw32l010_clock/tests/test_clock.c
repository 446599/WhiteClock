#include "clock_core.h"
#include "epd.h"
#include "epd_lut.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t tick;
static bool busy, fail_write;
static uint8_t cmd;
static unsigned bytes_old,bytes_new,luts,refreshes,offs,sleeps;
static bool fast;
uint32_t board_millis(void) { return tick; }
void board_delay(uint32_t ms) { (void)ms; assert(!"blocking delay in EPD"); }
bool board_epd_busy(void) { return busy; }
void board_epd_reset(bool high) { (void)high; }
bool board_epd_write(bool data,const uint8_t *p,unsigned n)
{
    if(fail_write) return false;
    if(!data) { assert(n==1); cmd=*p; if(cmd==0x12) ++refreshes; if(cmd==2) ++offs; if(cmd==7) ++sleeps; }
    else {
        if(cmd==0x10) bytes_old+=n;
        if(cmd==0x13) bytes_new+=n;
        if(cmd>=0x20 && cmd<=0x24) { assert(n==56); assert(!memcmp(p,fast ? epd_fast_lut[cmd-0x20] : epd_full_lut[cmd-0x20],56)); ++luts; }
    }
    return true;
}
static int finish(unsigned limit)
{
    for(unsigned i=0;i<limit;++i) { ++tick; uint32_t before=tick; int r=epd_poll(); assert(tick==before); if(r) return r; }
    assert(!"EPD did not finish"); return 0;
}
int main(void)
{
    for(unsigned seconds=0;seconds<86400;++seconds) {
        uint8_t h,m,s;
        assert(clock_decode(clock_encode(seconds/3600,seconds/60%60,seconds%60),&h,&m,&s));
        assert(h*3600u+m*60u+s==seconds);
    }
    ClockTime t={0,1,1,23,59,59}; unsigned weekday=6,days=0;
    for(unsigned y=0;y<100;++y) for(unsigned m=1;m<=12;++m)
        for(unsigned d=1;d<=clock_month_days(y,m);++d) {
            t.year=y;t.month=m;t.day=d; ClockTime back=t;
            assert(clock_time_valid(&t)); assert(clock_weekday(&t)==weekday);
            assert(clock_date_decode(clock_date_encode(&t),&back)); assert(!memcmp(&back,&t,sizeof t));
            weekday=(weekday+1)%7; ++days;
        }
    assert(days==36525); assert(clock_display_hour(0,true)==12 && clock_display_hour(12,true)==12);
    assert(clock_display_hour(23,true)==11 && clock_display_hour(23,false)==23);
    ClockSettings s=clock_defaults(); s.invert_enabled=1;
    t.hour=22;t.minute=0;assert(clock_inverted(&s,&t));
    t.hour=6;t.minute=59;assert(clock_inverted(&s,&t));
    t.hour=7;t.minute=0;assert(!clock_inverted(&s,&t));
    t.hour=21;t.minute=59;assert(!clock_inverted(&s,&t));
    s.end_hour=22;assert(clock_inverted(&s,&t));
    ClockButton b={0};
    assert(clock_button_sample(&b,true,false,0)==KEY_NONE);
    assert(clock_button_sample(&b,false,false,10)==KEY_NONE);
    assert(clock_button_sample(&b,true,false,20)==KEY_NONE);
    assert(clock_button_sample(&b,true,false,40)==KEY_NONE);
    assert(clock_button_sample(&b,true,false,740)==KEY_BACK);
    assert(clock_button_sample(&b,false,false,750)==KEY_NONE);
    assert(clock_button_sample(&b,false,false,770)==KEY_NONE);
    memset(&b,0,sizeof b);
    assert(clock_button_sample(&b,true,true,0)==KEY_NONE);
    assert(clock_button_sample(&b,true,true,20)==KEY_OK);
    assert(clock_button_sample(&b,true,true,520)==KEY_REPEAT);
    t=(ClockTime){24,1,31,12,34,0}; s=clock_defaults(); ClockUI u; clock_ui_init(&u,&t,&s);
    clock_ui_key(&u,KEY_NEXT); assert(u.page==PAGE_MENU);
    clock_ui_key(&u,KEY_REPEAT); assert(u.page==PAGE_MENU);
    clock_ui_key(&u,KEY_NEXT); clock_ui_key(&u,KEY_OK); assert(u.page==PAGE_DATE);
    clock_ui_key(&u,KEY_NEXT); clock_ui_key(&u,KEY_OK); assert(u.draft.month==2 && u.draft.day==29);
    clock_ui_key(&u,KEY_BACK); assert(u.live.month==1);
    u.page=PAGE_TIME;u.draft=t;u.cursor=2;
    assert(clock_ui_key(&u,KEY_REPEAT)==ACTION_NONE);
    assert(clock_ui_key(&u,KEY_OK)==ACTION_SET_CLOCK);
    clock_ui_saved(&u,false); assert(u.page==PAGE_TIME && u.error==2);
    clock_ui_saved(&u,true); assert(u.page==PAGE_MENU && !u.error);
    u.page=PAGE_HOME; ClockFace f=clock_ui_face(&u);
    for(unsigned theme=0;theme<THEME_COUNT;++theme) for(unsigned page=0;page<=PAGE_THEME;++page) {
        f.settings.theme=theme; f.page=page;
        for(unsigned y=0;y<PANEL_HEIGHT;++y) {
            uint8_t row[19];clock_render_row(&f,y,row);
            for(unsigned x=0;x<PANEL_WIDTH;++x) {
                unsigned lx=295-y,ly=x; uint8_t shade=clock_shade(&f,lx,ly); assert(shade<4);
                assert(((row[x/8]>>(7-x%8))&1u)==!clock_dither(shade,lx,ly));
            }
        }
    }
    f.page=PAGE_HOME; assert(epd_start(0,&f,false)); assert(!epd_start(0,&f,false));
    assert(finish(3000)==1); assert(bytes_old==5624 && bytes_new==5624 && luts==5 && refreshes==1 && offs==1 && sleeps==1);
    fast=true; assert(epd_start(&f,&f,true)); assert(finish(3000)==1);
    busy=true; assert(epd_start(&f,&f,true)); assert(finish(21000)==-1); busy=false;
    fail_write=true; assert(epd_start(0,&f,false)); assert(finish(1000)==-1); fail_write=false;
    assert(!epd_active());
    puts("PASS: century dates, weekday, schedules, keys, UI drafts, all-theme rotation, bounded full/fast refresh and failures");
}
