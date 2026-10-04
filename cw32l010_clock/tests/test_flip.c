#include "clock_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void render(const ClockFace *f,uint8_t out[PANEL_HEIGHT][PANEL_ROW_BYTES])
{
    for (unsigned row=0;row<PANEL_HEIGHT;++row) clock_render_row(f,row,out[row]);
    assert(!clock_render_overflowed());
    assert(clock_render_bounds_valid());
    assert(clock_render_op_count()<=112u);
}
static void check_mapping(const ClockFace *f)
{
    for (unsigned r=0;r<PANEL_HEIGHT;++r) {
        uint8_t row[PANEL_ROW_BYTES];clock_render_row(f,r,row);
        for (unsigned y=0;y<UI_HEIGHT;++y) {
            unsigned x=UI_WIDTH-1u-r;
            uint8_t shade=clock_shade(f,x,y);
            assert(shade<=3u);
            bool ink=clock_dither_pixel(shade,x,y,f->inverted);
            assert(((row[y/8u]>>(7u-y%8u))&1u)==!ink);
        }
    }
    assert(!clock_render_overflowed());
    assert(clock_render_bounds_valid());
}
int main(void)
{
    static const unsigned coverage[]={16,14,2,0};
    for (unsigned s=0;s<4;++s) {
        unsigned black=0;
        for (unsigned y=0;y<4;++y) for (unsigned x=0;x<4;++x) black+=clock_dither(s,x,y);
        assert(black==coverage[s]);
        for (unsigned y=0;y<64;++y) for (unsigned x=0;x<64;++x) {
            assert(clock_dither(s,x,y)==clock_dither(s,x+4,y+8));
            assert(clock_dither_pixel(3u-s,x,y,true)==!clock_dither_pixel(s,x,y,false));
        }
    }
    assert(!clock_dither(255,0,0));
    assert(!clock_dither_pixel(255,0,0,true));
    ClockTime t={26,10,4,14,36,0};ClockSettings s=clock_defaults();s.theme=1;
    ClockUI ui;clock_ui_init(&ui,&t,&s);ui.temperature_c=26;ui.temperature_valid=1;
    ClockFace f=clock_ui_face(&ui),night=f;night.inverted=1;
    static uint8_t day[PANEL_HEIGHT][PANEL_ROW_BYTES],dark[PANEL_HEIGHT][PANEL_ROW_BYTES];
    render(&f,day);render(&night,dark);
    for (unsigned r=0;r<PANEL_HEIGHT;++r) for (unsigned b=0;b<PANEL_ROW_BYTES;++b)
        assert((unsigned)(day[r][b]^dark[r][b])==255u);
    check_mapping(&f);check_mapping(&night);
    /* Glyph ink is pure white on both leaves, except the deliberate 1px split. */
    unsigned solid=0,gray=0;
    for (unsigned y=42;y<98;++y) for (unsigned x=35;x<67;++x) {
        uint8_t shade=clock_shade(&f,x,y);
        if (shade==3) {
            ++solid;assert(!clock_dither_pixel(shade,x,y,false));
        } else if (shade==1) ++gray;
    }
    assert(solid>300 && gray>100);
    for (unsigned x=17;x<133;++x) assert(clock_shade(&f,x,70)==0);
    /* Cache switching must never change a reconstructed OLD frame. */
    for (unsigned r=0;r<PANEL_HEIGHT;++r) {
        uint8_t row[19],other[19];
        clock_render_row(&night,r,other);clock_render_row(&f,r,row);
        assert(!memcmp(row,day[r],sizeof row));
    }
    /* Digit cells (including 12am/12pm and leading zero) for every minute. */
    for (unsigned fmt=0;fmt<2;++fmt) for (unsigned m=0;m<1440;++m) {
        ui.settings.hour12=fmt;ui.live.hour=m/60u;ui.live.minute=m%60u;
        f=clock_ui_face(&ui);
        for (unsigned x=33;x<270;x+=23) {
            uint8_t column[152];clock_render_column(&f,x,column);
            for (unsigned y=0;y<152;++y) assert(column[y]<=3);
        }
        assert(!clock_render_overflowed());
        assert(clock_render_bounds_valid());
    }
    /* Error overlays, selected theme, pause text, maximum and missing temperatures. */
    for (unsigned page=0;page<=PAGE_FOCUS;++page) for (unsigned error=0;error<4;++error)
        for (unsigned state=0;state<4;++state) for (unsigned theme=0;theme<THEME_COUNT;++theme) {
            ui.page=page;ui.cursor=page==PAGE_THEME?theme:0;
            ui.settings.theme=theme;ui.error=error;ui.focus_state=state;
            ui.temperature_c=state==0 ? -40 : 99;ui.temperature_valid=state!=1;
            f=clock_ui_face(&ui);
            for (unsigned x=0;x<296;x+=5) {
                uint8_t column[152];clock_render_column(&f,x,column);
            }
            assert(!clock_render_overflowed());
            assert(clock_render_bounds_valid());
        }
    /* Full-width 2099 date, negative temperature, and theme selector stay in bounds. */
    ui.page=PAGE_HOME;ui.error=0;ui.settings.theme=1;ui.live=(ClockTime){99,12,31,23,59,0};
    ui.temperature_c=-40;ui.temperature_valid=1;ui.focus_state=FOCUS_PAUSED;
    f=clock_ui_face(&ui);check_mapping(&f);
    uint8_t empty[19];clock_render_row(0,0,empty);
    for (unsigned i=0;i<19;++i) assert(empty[i]==255);
    clock_render_row(&f,PANEL_HEIGHT,empty);
    for (unsigned i=0;i<19;++i) assert(empty[i]==255);
    puts("PASS: flip theme, 4x4 coverage/phase, exact night complement, opaque digits, all minute cells, overlays and 112-op budget");
}
