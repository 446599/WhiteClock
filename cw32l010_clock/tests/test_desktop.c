#include "clock_core.h"
#include "temperature.h"
#include <assert.h>
#include <stdio.h>
static void theme_selection(void)
{
    ClockTime time={26,10,20,12,34,0};
    ClockSettings settings=clock_defaults();
    for (unsigned theme=0;theme<THEME_COUNT;++theme) {
        settings.theme=theme;
        ClockUI ui;clock_ui_init(&ui,&time,&settings);
        assert(clock_ui_key(&ui,KEY_OK)==ACTION_NONE && ui.page==PAGE_MENU);
        for (unsigned i=0;i<4u;++i) clock_ui_key(&ui,KEY_NEXT);
        clock_ui_key(&ui,KEY_OK);
        assert(ui.page==PAGE_THEME && ui.cursor==theme);
        for (unsigned i=1;i<=THEME_COUNT;++i) {
            assert(clock_ui_key(&ui,KEY_NEXT)==ACTION_NONE);
            assert(ui.cursor==(theme+i)%THEME_COUNT);
            assert(clock_ui_face(&ui).settings.theme==ui.cursor);
            assert(ui.settings.theme==theme);
        }
        clock_ui_key(&ui,KEY_NEXT);
        unsigned next=(theme+1u)%THEME_COUNT;
        assert(clock_ui_key(&ui,KEY_REPEAT)==ACTION_NONE);
        assert(ui.page==PAGE_THEME && ui.settings.theme==theme);
        clock_ui_key(&ui,KEY_BACK);
        assert(ui.page==PAGE_MENU && ui.settings.theme==theme);
        ui.cursor=4;clock_ui_key(&ui,KEY_OK);
        assert(ui.cursor==theme);
        clock_ui_key(&ui,KEY_NEXT);
        assert(clock_ui_face(&ui).settings.theme==next);
        assert(clock_ui_key(&ui,KEY_OK)==ACTION_SAVE_SETTINGS);
        assert(ui.page==PAGE_MENU && ui.settings.theme==next);
        clock_ui_saved(&ui,true);clock_ui_key(&ui,KEY_BACK);
        assert(ui.page==PAGE_HOME && clock_ui_face(&ui).settings.theme==next);
    }
}
static void focus_shortcut(void)
{
    ClockTime time={26,10,20,12,34,0};
    ClockSettings settings=clock_defaults();
    ClockUI ui;clock_ui_init(&ui,&time,&settings);
    clock_ui_key(&ui,KEY_NEXT);
    assert(ui.page==PAGE_FOCUS && ui.focus_state==FOCUS_IDLE);
    assert(ui.focus_remaining_ms==25u*60000u);
    clock_ui_key(&ui,KEY_BACK);
    assert(ui.page==PAGE_HOME);
    clock_ui_key(&ui,KEY_NEXT);clock_ui_key(&ui,KEY_OK);
    clock_ui_tick(&ui,1000);
    uint32_t remaining=ui.focus_remaining_ms;
    clock_ui_key(&ui,KEY_BACK);clock_ui_key(&ui,KEY_NEXT);
    assert(ui.page==PAGE_FOCUS && ui.focus_state==FOCUS_RUNNING);
    assert(ui.focus_remaining_ms==remaining);
    clock_ui_key(&ui,KEY_BACK);clock_ui_key(&ui,KEY_OK);
    for (unsigned i=0;i<5u;++i) clock_ui_key(&ui,KEY_NEXT);
    clock_ui_key(&ui,KEY_OK);
    assert(ui.page==PAGE_FOCUS);
    clock_ui_key(&ui,KEY_BACK);assert(ui.page==PAGE_MENU);
    ui.page=PAGE_HOME;ui.focus_state=FOCUS_RUNNING;ui.focus_remaining_ms=1;
    clock_ui_tick(&ui,1001);
    assert(ui.focus_state==FOCUS_DONE && ui.page==PAGE_FOCUS);
    assert(ui.focus_alert_remaining==FOCUS_ALERT_FLASHES);
    clock_ui_tick(&ui,1002);assert(ui.focus_alert_remaining==FOCUS_ALERT_FLASHES);
    clock_ui_key(&ui,KEY_OK);
    assert(!ui.focus_alert_remaining && ui.focus_state==FOCUS_IDLE);
    ui.page=PAGE_DATE;ui.focus_state=FOCUS_RUNNING;ui.focus_remaining_ms=1;
    ClockTime draft=ui.draft;
    clock_ui_tick(&ui,1003);
    assert(ui.page==PAGE_DATE && ui.draft.year==draft.year && ui.draft.month==draft.month && ui.draft.day==draft.day);
    assert(ui.focus_alert_remaining==FOCUS_ALERT_FLASHES);
    clock_ui_key(&ui,KEY_BACK);assert(!ui.focus_alert_remaining);
}
static void version_is_display_only(void)
{
    ClockTime time={26,10,20,12,34,0};
    ClockSettings settings=clock_defaults();
    ClockUI ui;clock_ui_init(&ui,&time,&settings);
    clock_ui_key(&ui,KEY_OK);
    for (unsigned lap=0;lap<3u;++lap) {
        for (unsigned item=0;item<6u;++item) {
            assert(ui.page==PAGE_MENU && ui.cursor==item);
            assert(clock_ui_key(&ui,KEY_REPEAT)==ACTION_NONE && ui.cursor==item);
            assert(clock_ui_key(&ui,KEY_NEXT)==ACTION_NONE);
        }
        assert(ui.cursor==0u);
    }
    /* Even a stale cursor pointing at the version must have no action. */
    ui.cursor=6;
    ClockFace before=clock_ui_face(&ui);
    assert(clock_ui_key(&ui,KEY_OK)==ACTION_NONE);
    assert(clock_ui_key(&ui,KEY_REPEAT)==ACTION_NONE);
    ClockFace after=clock_ui_face(&ui);
    assert(clock_face_equal(&before,&after));
    clock_ui_key(&ui,KEY_BACK);assert(ui.page==PAGE_HOME);
}
int main(void)
{
    theme_selection();
    focus_shortcut();
    version_is_display_only();
    int16_t t=123;
    assert(temperature_convert(700,1200,50,700,&t) && t==250);
    assert(temperature_convert(710,1200,50,700,&t) && t==289);
    assert(!temperature_convert(710,0,50,700,&t));
    assert(!temperature_convert(710,1200,255,700,&t));
    assert(!temperature_convert(4095,1200,50,700,&t));
    assert(!temperature_convert(100,1200,50,700,&t));
    assert(!temperature_convert(700,1200,50,0,&t));
    ClockTime time={26,10,4,12,0,0};ClockSettings settings=clock_defaults();ClockUI ui;
    clock_ui_init(&ui,&time,&settings);ui.page=PAGE_FOCUS;
    assert(clock_ui_face(&ui).focus_minutes==25);
    clock_ui_key(&ui,KEY_OK);assert(ui.focus_state==FOCUS_RUNNING);
    clock_ui_tick(&ui,1000);assert(ui.focus_remaining_ms==1499000);
    clock_ui_key(&ui,KEY_OK);assert(ui.focus_state==FOCUS_PAUSED);
    clock_ui_tick(&ui,5000);assert(ui.focus_remaining_ms==1499000);
    clock_ui_key(&ui,KEY_OK);clock_ui_tick(&ui,66000);
    assert(clock_ui_face(&ui).focus_minutes==24);
    ui.page=PAGE_HOME;clock_ui_tick(&ui,1504000);
    assert(ui.focus_state==FOCUS_DONE && ui.page==PAGE_FOCUS);
    clock_ui_key(&ui,KEY_OK);assert(ui.focus_state==FOCUS_IDLE);
    clock_ui_key(&ui,KEY_NEXT);assert(clock_ui_face(&ui).focus_minutes==45);
    ui.focus_tick=UINT32_MAX-9u;ui.focus_state=FOCUS_RUNNING;ui.focus_remaining_ms=100;
    clock_ui_tick(&ui,10);assert(ui.focus_remaining_ms==80);
    ui.page=PAGE_DATE;clock_ui_tick(&ui,90);
    assert(ui.focus_state==FOCUS_DONE && ui.page==PAGE_DATE);
    puts("PASS: six selectable settings, read-only version, all-theme preview/cycle/cancel/save, temperature and focus timer");
}
