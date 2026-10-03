#include "clock_core.h"
#include "temperature.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
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
    puts("PASS: fixed-point calibrated chip temperature, invalid ADC/OTP rejection, focus pause/resume/presets/completion and tick rollover");
}
