#include "epd.h"
#include "board.h"
#include "epd_lut.h"

/* Only the display task owns SPI and these immutable in-flight buffers. */
static struct {
    ClockFace old, next;
    ClockWindow window;
    uint32_t since, idle_since, started, stage_since;
    unsigned row, init_index;
    uint8_t state, after_wait, phase, command, data[PANEL_ROW_BYTES];
    bool previous, fast, failed, settling, io_active, frozen, windowed, restart, old_started, entered;
} update;
volatile EpdMetrics epd_metrics;
enum { IDLE, RESET_LOW, RESET_HIGH, RESET_WAIT, INIT, POWER, OLD, NEW,
       REFRESH, OFF, SLEEP, WAIT, AREA, ENTER, EXIT, RESTART_EXIT };
static int transfer(bool data,const uint8_t *bytes,unsigned length)
{
    if (!update.io_active) {
        if (!board_epd_begin(data,bytes,length)) return -1;
        update.io_active=true;
        return 0;
    }
    int result=board_epd_poll();
    if (result) update.io_active=false;
    return result;
}
static int command(uint8_t c)
{
    update.command=c;
    return transfer(false,&update.command,1);
}
static int initialize(void)
{
    static uint8_t border_data;
    static const struct { uint8_t command,length,data[5]; } regs[]={
        /* UC8251 PSR has one byte; retain the factory scan direction and LUTs. */
        {0x00,1,{0xf3}}, {0x01,5,{0x03,0x00,0x3f,0x3f,0x03}},
        {0x06,3,{0x17,0x17,0x17}}, {0x61,3,{0x98,0x01,0x28}},
        {0x30,1,{0x1a}}, {0x60,1,{0x22}}, {0x82,1,{0x13}},
        {0x03,1,{0x10}}, {0x50,1,{0}}
    };
    unsigned count=sizeof regs/sizeof regs[0], i=update.init_index;
    uint8_t c=i<count ? regs[i].command : 0x20u+i-count;
    bool fast_waveform=update.fast && EPD_USE_FAST_LUT;
    const uint8_t *data=i<count ? regs[i].data :
        fast_waveform ? epd_fast_lut[i-count] : epd_full_lut[i-count];
    unsigned length=i<count ? regs[i].length : 56u;
    if (i==count-1u) {
        /* Border comparisons must not change the pixel polarity or LUT bytes. */
        border_data=fast_waveform ? EPD_FAST_BORDER_DATA : EPD_FULL_BORDER_DATA;
        data=&border_data;
    }
    int result=update.phase ? transfer(true,data,length) : command(c);
    if (result<=0) return result;
    if (!update.phase) update.phase=1;
    else { update.phase=0; if (++update.init_index==count+5u) return 1; }
    return 0;
}
static void wait_for(uint8_t next)
{
    update.state=WAIT; update.after_wait=next;
    update.since=board_millis(); update.settling=false;
}
bool epd_active(void) { return update.state!=IDLE; }
bool epd_work_ready(void)
{
    return update.state>=INIT && update.state!=WAIT;
}
bool epd_start(const ClockFace *previous,const ClockFace *next,bool fast)
{
    if (epd_active() || !next) return false;
    update.next=*next; update.previous=previous!=0;
    if (previous) update.old=*previous;
    update.fast=fast && previous && previous->inverted==next->inverted;
    update.failed=update.io_active=update.frozen=update.restart=update.old_started=false;
    update.windowed=update.entered=false;
    update.window=(ClockWindow){0,PANEL_WIDTH-1u,0,PANEL_HEIGHT-1u};
    epd_metrics=(EpdMetrics){0};
    update.row=update.init_index=update.phase=0;
    update.state=RESET_LOW; update.since=board_millis();
    update.started=update.stage_since=update.since;
    board_epd_reset(true); return true;
}
bool epd_retarget(const ClockFace *next)
{
    /* Once NEW starts, the frame is immutable until the waveform completes. */
    if (!next || !epd_active() || update.frozen || update.failed ||
        (update.fast && next->inverted!=update.old.inverted)) return false;
    update.next=*next;
    /* Only cropped windows depend on the new scene; full-frame OLD stays valid. */
    if (EPD_USE_PARTIAL_WINDOW && update.fast &&
        (update.state==AREA || update.state==ENTER || update.state==OLD || update.state==NEW))
        update.restart=true;
    return true;
}
static void restart_if_ready(void)
{
    if (!update.restart || update.io_active || update.failed || update.frozen ||
        update.state==RESTART_EXIT || (update.state==AREA && update.phase)) return;
    /* Finish a pending 0x90 parameter block before issuing another command.
       An entered window must be exited even if the next upload is full-screen. */
    update.restart=false; update.phase=0;
    update.state=update.entered ? RESTART_EXIT : AREA;
}
/* 0 pending, 1 window ready, 2 unchanged scene, -1 transfer failed. */
static int window(void)
{
    if (!update.phase && !update.io_active) {
        ClockWindow changed;
        if (update.fast && !clock_changed_window(&update.old,&update.next,&changed)) return 2;
        update.windowed=EPD_USE_PARTIAL_WINDOW && update.fast &&
            (changed.x0 || changed.x1!=PANEL_WIDTH-1u || changed.y0 || changed.y1!=PANEL_HEIGHT-1u);
        update.window=update.windowed ? changed : (ClockWindow){0,PANEL_WIDTH-1u,0,PANEL_HEIGHT-1u};
        update.row=update.window.y0;
        if (!update.windowed) return 1;
        update.data[0]=changed.x0; update.data[1]=changed.x1;
        update.data[2]=changed.y0>>8; update.data[3]=changed.y0;
        update.data[4]=changed.y1>>8; update.data[5]=changed.y1;
        update.data[6]=1; /* Factory 0x33a0: 0x90 + seven bytes, then 0x91. */
    }
    int result=update.phase ? transfer(true,update.data,7) : command(0x90);
    if (result==1) { if (update.phase) { update.phase=0; return 1; } update.phase=1; }
    return result<0 ? -1 : 0;
}
static int rows(void)
{
    unsigned length=(update.window.x1-update.window.x0+1u)/8u;
    if (!update.phase) {
        int result=command(update.state==OLD ? 0x10 : 0x13);
        if (result==1) update.phase=1;
        return result<0 ? -1 : 0;
    }
    if (!update.io_active) {
        const ClockFace *face=update.state==NEW ? &update.next : update.previous ? &update.old : 0;
        uint32_t before=board_millis();
        clock_render_row_span(face,update.row,update.window.x0/8u,length,update.data);
        uint32_t elapsed=board_millis()-before;
        if (elapsed>epd_metrics.max_render_ms) epd_metrics.max_render_ms=elapsed;
        ++epd_metrics.rendered_rows;
        /* Board polarity correction belongs ONLY at the wire boundary, for both planes.
           UI luminance and scheduled dark mode remain independent. */
        if (EPD_PANEL_INVERT_DATA)
            for (unsigned i=0;i<length;++i) update.data[i]^=0xffu;
    }
    int result=transfer(true,update.data,length);
    if (result<=0) return result;
    epd_metrics.image_bytes+=length;
    if (++update.row>update.window.y1) { update.row=update.window.y0;update.phase=0;return 1; }
    return 0;
}
int epd_poll(void)
{
    uint32_t now=board_millis();
    int result=0;
    restart_if_ready();
    switch (update.state) {
    case IDLE: return 0;
    case RESET_LOW: case RESET_HIGH: case RESET_WAIT:
        if (now-update.since<10u) return 0;
        if (update.state==RESET_WAIT) { wait_for(INIT); break; }
        board_epd_reset(update.state==RESET_HIGH);
        ++update.state; update.since=now; break;
    case WAIT:
        if (now-update.since<1u) return 0;
        if (now-update.since>=10000u) {
            update.failed=true;
            update.state=update.after_wait==SLEEP ? SLEEP : OFF;
        } else if (board_epd_busy()) update.settling=false;
        else if (!update.settling) { update.settling=true; update.idle_since=now; }
        else if (now-update.idle_since>=10u) {
            update.state=update.after_wait;
            if (update.state==OFF || update.state==EXIT) {
                epd_metrics.busy_ms=now-update.stage_since; update.stage_since=now;
            }
        }
        break;
    case INIT: result=initialize(); if(result==1) update.state=POWER; break;
    case POWER: result=command(0x04); if(result==1) wait_for(AREA); break;
    case AREA:
        result=window();
        if (result==1) update.state=update.windowed ? ENTER : OLD;
        else if (result==2) {
            if (update.old_started) epd_metrics.old_ms+=now-update.stage_since;
            else epd_metrics.setup_ms=now-update.started;
            update.frozen=true; update.stage_since=now; update.state=OFF;
        }
        break;
    case ENTER:
        result=command(0x91);
        if(result==1) { update.entered=true; update.state=OLD; }
        break;
    case RESTART_EXIT:
        result=command(0x92);
        if(result==1) { update.entered=false; update.state=AREA; }
        break;
    case OLD: case NEW:
        if (update.state==OLD && !update.old_started) {
            update.old_started=true;
            epd_metrics.setup_ms=now-update.started; update.stage_since=now;
        }
        if (update.state==NEW) update.frozen=true;
        result=rows();
        if(result==1) {
            if (update.state==OLD) { epd_metrics.old_ms+=board_millis()-update.stage_since; update.stage_since=board_millis(); }
            ++update.state;
        }
        break;
    case REFRESH:
        result=command(0x12);
        if(result==1) {
            epd_metrics.refreshed=1;
            epd_metrics.new_ms=board_millis()-update.stage_since; update.stage_since=board_millis();
            wait_for(update.windowed ? EXIT : OFF);
        }
        break;
    case EXIT: result=command(0x92); if(result==1) update.state=OFF; break;
    case OFF:
        result=command(0x02);
        if(result==1) wait_for(SLEEP);
        else if(result<0) { update.failed=true;update.state=SLEEP;result=0; }
        break;
    case SLEEP: {
        static const uint8_t sleep=0xa5;
        result=update.phase ? transfer(true,&sleep,1) : command(0x07);
        if (result<0 || (result==1 && update.phase)) {
            update.state=IDLE;update.phase=0;
            epd_metrics.finish_ms=board_millis()-update.stage_since;
            epd_metrics.total_ms=board_millis()-update.started;
            return result>0 && !update.failed ? 1 : -1;
        }
        if (result==1) update.phase=1;
        break;
    }
    default: result=-1; break;
    }
    if (result<0) { update.failed=true; update.phase=0;update.state=OFF; }
    restart_if_ready();
    return 0;
}
