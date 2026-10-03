#include "main.h"
#include "board.h"
#include "clock_core.h"
#include "epd.h"
#include "settings.h"
volatile uint32_t clock_stage, epd_successes, epd_failures, rtc_failures;
int main(void)
{
    board_init(); clock_stage=1;
    ClockTime time={26,10,4,12,0,0};
    bool rtc_ok=board_rtc_init() && board_rtc_read_time(&time);
    ClockSettings settings=settings_load();
    ClockUI ui; clock_ui_init(&ui,&time,&settings); ui.error=!rtc_ok;
    ClockFace displayed={0}, pending={0};
    bool have_display=false, dirty_settings=false;
    uint32_t poll=board_millis()-250u, changed=board_millis(), failed_at=0;
    uint8_t fast_count=0; bool retry=false;
    clock_stage=2;
    while(1) {
        uint32_t now=board_millis();
        if(now-poll>=250u) {
            poll=now;
            if(board_rtc_read_time(&time)) { ui.live=time; if(ui.error==1) ui.error=0; }
            else { ++rtc_failures; if(!ui.error) ui.error=1; }
        }
        uint8_t key;
        while((key=board_key())!=KEY_NONE) {
            uint8_t action=clock_ui_key(&ui,key); changed=board_millis();
            if(action==ACTION_SET_CLOCK) {
                bool ok=board_rtc_save(&ui.draft,ui.page==PAGE_DATE);
                if(ok) board_rtc_read_time(&ui.live); else ++rtc_failures;
                clock_ui_saved(&ui,ok);
            }
            if(action==ACTION_SAVE_SETTINGS) dirty_settings=true;
        }
        now=board_millis();
        if(dirty_settings && now-changed>=2000u && !epd_active()) {
            ui.error=settings_save(&ui.settings) ? 0 : 2;
            dirty_settings=false;
        }
        int result=epd_poll();
        if(result) {
            clock_stage=4;
            if(result>0) { displayed=pending; have_display=true; ++epd_successes; retry=false; }
            else { have_display=false; ++epd_failures; failed_at=board_millis(); retry=true; }
        }
        ClockFace face=clock_ui_face(&ui);
        now=board_millis();
        if(!epd_active() && (!retry || now-failed_at>=60000u) && now-changed>=80u &&
           (!have_display || !clock_face_equal(&face,&displayed))) {
            bool fast=have_display && fast_count<7u && face.inverted==displayed.inverted;
            pending=face;
            if(epd_start(have_display ? &displayed : 0,&pending,fast)) {
                fast_count=fast ? fast_count+1u : 0u; clock_stage=3;
            }
        }
        __WFI();
    }
}
void assert_failed(uint8_t *file,uint32_t line)
{ (void)file; (void)line; clock_stage=0xff; while(1) __WFI(); }
