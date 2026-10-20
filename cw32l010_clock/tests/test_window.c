#include "clock_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned cases;
static void check_window(const ClockFace *old,const ClockFace *next)
{
    ClockWindow w;
    bool changed=clock_changed_window(old,next,&w);
    if (changed) {
        assert(w.x0<=w.x1 && w.x1<PANEL_WIDTH && !(w.x0&7u) && (w.x1&7u)==7u);
        assert(w.y0<=w.y1 && w.y1<PANEL_HEIGHT);
    }
    uint8_t before[PANEL_HEIGHT][PANEL_ROW_BYTES], after[PANEL_HEIGHT][PANEL_ROW_BYTES];
    for (unsigned y=0;y<PANEL_HEIGHT;++y) clock_render_row(old,y,before[y]);
    for (unsigned y=0;y<PANEL_HEIGHT;++y) clock_render_row(next,y,after[y]);
    for (unsigned y=0;y<PANEL_HEIGHT;++y) {
        for (unsigned x=0;x<PANEL_WIDTH;++x)
            if ((before[y][x/8u]^after[y][x/8u])&(0x80u>>(x%8u)))
                assert(changed && x>=w.x0 && x<=w.x1 && y>=w.y0 && y<=w.y1);
        unsigned first=y%PANEL_ROW_BYTES, n=1u+(y/19u)%(PANEL_ROW_BYTES-first);
        uint8_t span[PANEL_ROW_BYTES+2u];
        memset(span,0x5a,sizeof span);
        clock_render_row_span(next,y,first,n,span+1);
        assert(span[0]==0x5a && span[n+1u]==0x5a);
        assert(!memcmp(span+1,after[y]+first,n));
    }
    ++cases;
}
int main(void)
{
    ClockUI ui;
    ClockTime t={26,10,4,12,34,0};
    ClockSettings s=clock_defaults();
    clock_ui_init(&ui,&t,&s);
    ClockFace base=clock_ui_face(&ui);
    ClockWindow w;
    assert(!clock_changed_window(&base,&base,&w));
    assert(clock_changed_window(0,&base,&w));
    assert(!w.x0 && w.x1==151u && !w.y0 && w.y1==295u);
    for (unsigned theme=0;theme<THEME_COUNT;++theme)
        for (unsigned page=PAGE_HOME;page<=PAGE_FOCUS;++page)
            for (unsigned cursor=0;cursor<7u;++cursor) {
                ClockFace old=base;
                old.settings.theme=theme; old.page=page; old.cursor=cursor;
                old.focus_minutes=25; old.focus_state=FOCUS_RUNNING;
                old.temperature_valid=1; old.temperature_c=24;
                ClockFace next=old;
                next.cursor=(cursor+1u)%7u; check_window(&old,&next);
                next=old; next.page=(page+1u)%(PAGE_FOCUS+1u); check_window(&old,&next);
                next=old; next.time=(ClockTime){27,1,31,23,59,0}; check_window(&old,&next);
                next=old; next.settings.theme=(theme+1u)%THEME_COUNT;
                next.settings.hour12=1; next.settings.invert_enabled=1;
                next.settings.start_hour=18; next.settings.end_minute=30;
                check_window(&old,&next);
                next=old; next.error=2;
                check_window(&old,&next); check_window(&next,&old);
                next=old; next.inverted=1; check_window(&old,&next);
                next=old; next.focus_minutes=0; next.focus_state=FOCUS_DONE;
                next.focus_progress=100; next.temperature_c=-9;
                check_window(&old,&next);
                next=old; next.focus_state=FOCUS_PAUSED; next.temperature_valid=0;
                check_window(&old,&next);
            }
    printf("PASS: %u scene changes, every changed pixel inside window, span/full equivalence\n",cases);
}
