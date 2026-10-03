#include "epd.h"
#include "board.h"
#include "epd_lut.h"

/* Only the display task owns SPI and these immutable in-flight buffers. */
static struct {
    ClockFace old, next;
    uint32_t since, idle_since;
    unsigned row, init_index;
    uint8_t state, after_wait, phase, command, data[PANEL_ROW_BYTES];
    bool previous, fast, failed, settling, io_active, frozen;
} update;
enum { IDLE, RESET_LOW, RESET_HIGH, RESET_WAIT, INIT, POWER, OLD, NEW,
       REFRESH, OFF, SLEEP, WAIT };
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
    static const struct { uint8_t command,length,data[5]; } regs[]={
        {0x00,2,{0xf3,0x0e}}, {0x01,5,{0x03,0x00,0x3f,0x3f,0x03}},
        {0x06,3,{0x17,0x17,0x17}}, {0x61,3,{0x98,0x01,0x28}},
        {0x30,1,{0x1a}}, {0x60,1,{0x22}}, {0x82,1,{0x13}},
        {0x03,1,{0x10}}, {0x50,1,{0x87}}
    };
    unsigned count=sizeof regs/sizeof regs[0], i=update.init_index;
    uint8_t c=i<count ? regs[i].command : 0x20u+i-count;
    const uint8_t *data=i<count ? regs[i].data :
        update.fast ? epd_fast_lut[i-count] : epd_full_lut[i-count];
    unsigned length=i<count ? regs[i].length : 56u;
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
    update.fast=fast && previous; update.failed=update.io_active=update.frozen=false;
    update.row=update.init_index=update.phase=0;
    update.state=RESET_LOW; update.since=board_millis();
    board_epd_reset(true); return true;
}
bool epd_retarget(const ClockFace *next)
{
    /* Once NEW starts, the frame is immutable until the waveform completes. */
    if (!next || !epd_active() || update.frozen || update.failed ||
        (update.fast && next->inverted!=update.old.inverted)) return false;
    update.next=*next;
    return true;
}
static int rows(void)
{
    if (!update.phase) {
        int result=command(update.state==OLD ? 0x10 : 0x13);
        if (result==1) update.phase=1;
        return result<0 ? -1 : 0;
    }
    if (!update.io_active) {
        const ClockFace *face=update.state==NEW ? &update.next : update.previous ? &update.old : 0;
        clock_render_row(face,update.row,update.data);
        /* Board polarity correction belongs ONLY at the wire boundary, for both planes.
           UI luminance and scheduled dark mode remain independent. */
        if (EPD_PANEL_INVERT_DATA)
            for (unsigned i=0;i<PANEL_ROW_BYTES;++i) update.data[i]^=0xffu;
    }
    int result=transfer(true,update.data,sizeof update.data);
    if (result<=0) return result;
    if (++update.row==PANEL_HEIGHT) { update.row=0;update.phase=0;return 1; }
    return 0;
}
int epd_poll(void)
{
    uint32_t now=board_millis();
    int result=0;
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
        else if (now-update.idle_since>=10u) update.state=update.after_wait;
        break;
    case INIT: result=initialize(); if(result==1) update.state=POWER; break;
    case POWER: result=command(0x04); if(result==1) wait_for(OLD); break;
    case OLD: case NEW:
        if (update.state==NEW) update.frozen=true;
        result=rows(); if(result==1) ++update.state; break;
    case REFRESH: result=command(0x12); if(result==1) wait_for(OFF); break;
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
            return result>0 && !update.failed ? 1 : -1;
        }
        if (result==1) update.phase=1;
        break;
    }
    default: result=-1; break;
    }
    if (result<0) { update.failed=true; update.phase=0;update.state=OFF; }
    return 0;
}
