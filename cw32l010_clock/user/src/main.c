#include "main.h"
#include "board.h"
#include "clock_core.h"
#include "epd.h"
#include "settings.h"
#include "temperature.h"
volatile uint32_t clock_stage, epd_successes, epd_failures, rtc_failures;
volatile uint32_t main_loop_max_ms, epd_poll_max_ms;
volatile uint32_t clock_ui_state, clock_key_events, clock_last_key_ms;
int main(void)
{
    board_init(); clock_stage=1;
    ClockTime time={26,10,4,12,0,0};
    bool rtc_ok=board_rtc_init() && board_rtc_read_time(&time);
    ClockSettings settings=settings_load();
    ClockUI ui; clock_ui_init(&ui,&time,&settings); ui.error=!rtc_ok;
    temperature_init();ui.focus_tick=board_millis();
    ClockFace displayed={0}, pending={0};
    bool have_display=false, dirty_settings=false, saving=false, pending_fast=false, pending_alert=false;
    uint32_t poll=board_millis()-250u, changed=board_millis(), failed_at=0;
    uint32_t last_loop=board_millis();
    uint8_t fast_count=0, saving_page=0; bool retry=false;
    clock_stage=2;
    while(1) {
        uint32_t now=board_millis(), elapsed=now-last_loop;
        last_loop=now;
        if (elapsed>main_loop_max_ms) main_loop_max_ms=elapsed;
        clock_ui_tick(&ui,now);
        /* Input first; EPD, ADC and RTC operations all yield back here. */
        uint8_t key;
        while((key=board_key())!=KEY_NONE) {
            ++clock_key_events; clock_last_key_ms=board_millis();
            pending_alert=false;
            uint8_t action=clock_ui_key(&ui,key); changed=board_millis();
            if(action==ACTION_SET_CLOCK && !saving) {
                saving=board_rtc_save_start(&ui.draft,ui.page==PAGE_DATE);
                saving_page=ui.page;ui.error=saving ? 3 : 2;
            }
            if(action==ACTION_SAVE_SETTINGS) dirty_settings=true;
        }
        if (saving) {
            int result=board_rtc_save_poll();
            if (result) {
                saving=false;
                if (result>0) board_rtc_read_time(&ui.live); else ++rtc_failures;
                if (ui.page==saving_page) clock_ui_saved(&ui,result>0);
                else if (result<0) ui.error=2;
            }
        }
        now=board_millis();
        if(now-poll>=250u && !saving) {
            poll=now;
            if(board_rtc_read_time(&time)) { ui.live=time; if(ui.error==1) ui.error=0; }
            else { ++rtc_failures; if(!ui.error) ui.error=1; }
        }
        temperature_poll(now);
        int16_t c10;
        ui.temperature_valid=temperature_read(&c10);
        if (ui.temperature_valid) ui.temperature_c=(c10+(c10>=0?5:-5))/10;
        if(dirty_settings && now-changed>=2000u && !epd_active() && !saving) {
            ui.error=settings_save(&ui.settings) ? 0 : 2;
            dirty_settings=false;
        }
        uint32_t before=board_millis();
        int result=epd_poll();
        elapsed=board_millis()-before;
        if (elapsed>epd_poll_max_ms) epd_poll_max_ms=elapsed;
        if(result) {
            clock_stage=4;
            if(result>0) {
                displayed=pending; have_display=true; ++epd_successes; retry=false;
                if (epd_metrics.refreshed) fast_count=pending_fast ? fast_count+1u : 0u;
                if (pending_alert && epd_metrics.refreshed && ui.focus_alert_remaining)
                    --ui.focus_alert_remaining;
            }
            else { have_display=false; ++epd_failures; failed_at=board_millis(); retry=true; }
            pending_alert=false;
        }
        ClockFace face=clock_ui_face(&ui);
        now=board_millis();
        if (epd_active() && !clock_face_equal(&face,&pending) && epd_retarget(&face)) pending=face;
        if(!epd_active() && (!retry || now-failed_at>=60000u) && now-changed>=5u &&
           (ui.focus_alert_remaining || !have_display || !clock_face_equal(&face,&displayed))) {
            /* Full waveforms must run even when the completed page is unchanged. */
            bool alert=ui.focus_alert_remaining!=0;
            bool fast=!alert && have_display && fast_count<7u && face.inverted==displayed.inverted;
            pending=face;
            if(epd_start(have_display ? &displayed : 0,&pending,fast)) {
                pending_fast=fast; pending_alert=alert; clock_stage=3;
            }
        }
        /* Read-only diagnostics: UI, pending and displayed page/cursor pairs. */
        clock_ui_state=ui.page | ((uint32_t)ui.cursor<<4) |
            ((uint32_t)pending.page<<8) | ((uint32_t)pending.cursor<<12) |
            ((uint32_t)displayed.page<<16) | ((uint32_t)displayed.cursor<<20) |
            ((uint32_t)epd_active()<<24) | ((uint32_t)have_display<<25) | ((uint32_t)retry<<26);
        /* Do not insert a mandatory 1ms sleep between every SPI byte/row. */
        if (!epd_work_ready()) __WFI();
    }
}
void assert_failed(uint8_t *file,uint32_t line)
{ (void)file; (void)line; clock_stage=0xff; while(1) __WFI(); }
