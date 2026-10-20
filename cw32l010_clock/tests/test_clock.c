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
static uint8_t wire_old[5624],wire_new[5624];
uint32_t board_millis(void) { return tick; }
void board_delay(uint32_t ms) { (void)ms; assert(!"blocking delay in EPD"); }
bool board_epd_busy(void) { return busy; }
void board_epd_reset(bool high) { (void)high; }
static bool write_complete(bool data,const uint8_t *p,unsigned n)
{
    if(fail_write) return false;
    if(!data) { assert(n==1); cmd=*p; if(cmd==0x12) ++refreshes; if(cmd==2) ++offs; if(cmd==7) ++sleeps; }
    else {
        if(cmd==0x10) { memcpy(wire_old+bytes_old%5624,p,n);bytes_old+=n; }
        if(cmd==0x13) { memcpy(wire_new+bytes_new%5624,p,n);bytes_new+=n; }
        if(cmd>=0x20 && cmd<=0x24) { assert(n==56); assert(!memcmp(p,fast && EPD_USE_FAST_LUT ? epd_fast_lut[cmd-0x20] : epd_full_lut[cmd-0x20],56)); ++luts; }
    }
    return true;
}
static const uint8_t *tx_pointer;
static uint8_t tx_copy[56];
static unsigned tx_length;
static bool tx_data,tx_active;
bool board_epd_begin(bool data,const uint8_t *p,unsigned n)
{
    assert(!tx_active && n && n<=sizeof tx_copy);
    if (fail_write) return false;
    tx_pointer=p;tx_length=n;tx_data=data;tx_active=true;
    memcpy(tx_copy,p,n);
    return true;
}
int board_epd_poll(void)
{
    assert(tx_active);
    assert(!memcmp(tx_pointer,tx_copy,tx_length));
    tx_active=false;
    return write_complete(tx_data,tx_pointer,tx_length) ? 1 : -1;
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
    assert(clock_button_sample(&b,true,false,40)==KEY_NEXT);
    assert(clock_button_sample(&b,true,false,740)==KEY_BACK);
    assert(clock_button_sample(&b,false,false,750)==KEY_NONE);
    assert(clock_button_sample(&b,false,false,770)==KEY_NONE);
    memset(&b,0,sizeof b);
    assert(clock_button_sample(&b,true,true,0)==KEY_NONE);
    assert(clock_button_sample(&b,true,true,20)==KEY_OK);
    assert(clock_button_sample(&b,true,true,520)==KEY_REPEAT);
    memset(&b,0,sizeof b);
    assert(clock_button_sample(&b,true,false,0)==KEY_NONE);
    assert(clock_button_sample(&b,true,false,20)==KEY_NEXT);
    assert(clock_button_sample(&b,false,false,40)==KEY_NONE);
    assert(clock_button_sample(&b,false,false,60)==KEY_NONE);
    memset(&b,0,sizeof b);
    assert(clock_button_sample(&b,true,false,0)==KEY_NONE);
    assert(clock_button_sample(&b,true,false,20)==KEY_NEXT);
    assert(clock_button_sample(&b,true,false,720)==KEY_BACK);
    t=(ClockTime){24,1,31,12,34,0}; s=clock_defaults(); ClockUI u; clock_ui_init(&u,&t,&s);
    clock_ui_key(&u,KEY_OK); assert(u.page==PAGE_MENU);
    clock_ui_key(&u,KEY_REPEAT); assert(u.page==PAGE_MENU);
    clock_ui_key(&u,KEY_NEXT); clock_ui_key(&u,KEY_OK); assert(u.page==PAGE_DATE);
    clock_ui_key(&u,KEY_NEXT); clock_ui_key(&u,KEY_OK); assert(u.draft.month==2 && u.draft.day==29);
    clock_ui_key(&u,KEY_BACK); assert(u.live.month==1);
    u.page=PAGE_TIME;u.draft=t;u.cursor=2;
    assert(clock_ui_key(&u,KEY_REPEAT)==ACTION_NONE);
    assert(clock_ui_key(&u,KEY_OK)==ACTION_SET_CLOCK);
    clock_ui_saved(&u,false); assert(u.page==PAGE_TIME && u.error==2);
    clock_ui_saved(&u,true); assert(u.page==PAGE_MENU && !u.error);
    u.page=PAGE_HOME;clock_ui_key(&u,KEY_BACK);assert(u.page==PAGE_HOME);
    u.page=PAGE_HOME; ClockFace f=clock_ui_face(&u);
    for(unsigned inverted=0;inverted<2u;++inverted)
    for(unsigned theme=0;theme<THEME_COUNT;++theme) for(unsigned page=0;page<=PAGE_FOCUS;++page) {
        f.settings.theme=theme; f.page=page; f.inverted=inverted;
        for(unsigned y=0;y<PANEL_HEIGHT;++y) {
            uint8_t row[19];clock_render_row(&f,y,row);
            for(unsigned x=0;x<PANEL_WIDTH;++x) {
                unsigned lx=295-y,ly=x; uint8_t shade=clock_shade(&f,lx,ly); assert(shade<4);
                assert(((row[x/8]>>(7-x%8))&1u)==!clock_dither(shade,lx,ly));
            }
        }
    }
    /* Empty/invalid rows remain white even when the scene itself is inverted. */
    for (unsigned mode=0;mode<2u;++mode) {
        uint8_t row[PANEL_ROW_BYTES];
        clock_render_row(mode ? &f : 0,mode ? PANEL_HEIGHT : 0,row);
        for (unsigned i=0;i<PANEL_ROW_BYTES;++i) assert(row[i]==0xffu);
    }
    f.inverted=0; f.page=PAGE_HOME; assert(epd_start(0,&f,false)); assert(!epd_start(0,&f,false));
    assert(finish(3000)==1); assert(bytes_old==5624 && bytes_new==5624 && luts==5 && refreshes==1 && offs==1 && sleeps==1);
    for(unsigned i=0;i<5624;++i) assert(wire_old[i]==(EPD_PANEL_INVERT_DATA?0:255));
    for(unsigned y=0;y<PANEL_HEIGHT;++y) {
        uint8_t row[19];clock_render_row(&f,y,row);
        for(unsigned i=0;i<19;++i) assert(wire_new[y*19+i]==(uint8_t)(row[i]^(EPD_PANEL_INVERT_DATA?255:0)));
    }
    fast=true; assert(epd_start(&f,&f,true)); assert(finish(3000)==1);
    ClockFace latest=f;latest.time.minute=(f.time.minute+1u)%60u;
    unsigned before_new=bytes_new;
    /* Verify complete-frame pixels here; test_epd covers windowed fast retargets. */
    fast=false;
    assert(epd_start(&f,&f,false));assert(epd_retarget(&latest));
    while(bytes_new==before_new) { ++tick;assert(epd_poll()==0); }
    assert(!epd_retarget(&f));assert(finish(3000)==1);
    for(unsigned y=0;y<PANEL_HEIGHT;++y) {
        uint8_t row[19];clock_render_row(&latest,y,row);
        for(unsigned i=0;i<19;++i) assert(wire_new[y*19+i]==(uint8_t)(row[i]^(EPD_PANEL_INVERT_DATA?255:0)));
    }
    fast=true; busy=true; assert(epd_start(&f,&f,true)); assert(finish(21000)==-1); busy=false;
    fail_write=true; assert(epd_start(0,&f,false)); assert(finish(1000)==-1); fail_write=false;
    assert(!epd_active());
    puts("PASS: century dates, weekday, schedules, keys, UI drafts, all-theme rotation, bounded full/fast refresh and failures");
}
