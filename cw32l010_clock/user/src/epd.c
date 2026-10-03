#include "epd.h"
#include "board.h"
#include "epd_lut.h"

static struct {
    ClockFace old, next;
    uint32_t since, idle_since;
    unsigned row;
    uint8_t state, after_wait;
    bool previous, fast, failed, settling;
} update;
enum { IDLE, RESET_LOW, RESET_HIGH, RESET_WAIT, INIT, POWER, OLD, NEW,
       REFRESH, OFF, SLEEP, WAIT };
static bool command(uint8_t c) { return board_epd_write(false,&c,1); }
static bool reg(uint8_t c,const uint8_t *d,unsigned n)
{ return command(c) && board_epd_write(true,d,n); }
#define REG(c, ...) do { static const uint8_t d[]={__VA_ARGS__}; \
    if (!reg(c,d,sizeof d)) return false; } while (0)
static bool initialize(void)
{
    REG(0x00,0xf3,0x0e); REG(0x01,0x03,0x00,0x3f,0x3f,0x03);
    REG(0x06,0x17,0x17,0x17); REG(0x61,0x98,0x01,0x28);
    REG(0x30,0x1a); REG(0x60,0x22); REG(0x82,0x13);
    REG(0x03,0x10); REG(0x50,0x87);
    for (unsigned i=0;i<5u;++i)
        if (!reg(0x20u+i,update.fast ? epd_fast_lut[i] : epd_full_lut[i],56)) return false;
    return true;
}
static void wait_for(uint8_t next)
{
    update.state=WAIT; update.after_wait=next;
    update.since=board_millis(); update.settling=false;
}
bool epd_active(void) { return update.state!=IDLE; }
bool epd_start(const ClockFace *previous,const ClockFace *next,bool fast)
{
    if (epd_active() || !next) return false;
    update.next=*next; update.previous=previous!=0;
    if (previous) update.old=*previous;
    update.fast=fast && previous; update.failed=false;
    update.row=0; update.state=RESET_LOW; update.since=board_millis();
    board_epd_reset(true); return true;
}
static bool rows(const ClockFace *face)
{
    uint8_t data[PANEL_ROW_BYTES];
    /* Four rows per poll leaves the main loop free to consume key events. */
    for (unsigned n=0;n<4u && update.row<PANEL_HEIGHT;++n,++update.row) {
        clock_render_row(face,update.row,data);
        if (!board_epd_write(true,data,sizeof data)) return false;
    }
    return true;
}
int epd_poll(void)
{
    uint32_t now=board_millis();
    bool ok=true;
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
    case INIT: ok=initialize(); if(ok) update.state=POWER; break;
    case POWER: ok=command(0x04); if(ok) wait_for(OLD); break;
    case OLD: case NEW:
        if (update.row==0) ok=command(update.state==OLD ? 0x10 : 0x13);
        if (ok) ok=rows(update.state==NEW ? &update.next : update.previous ? &update.old : 0);
        if (ok && update.row==PANEL_HEIGHT) { update.row=0; ++update.state; }
        break;
    case REFRESH: ok=command(0x12); if(ok) wait_for(OFF); break;
    case OFF:
        if (command(0x02)) wait_for(SLEEP);
        else { update.failed=true; update.state=SLEEP; }
        break;
    case SLEEP: {
        const uint8_t sleep=0xa5;
        ok=reg(0x07,&sleep,1); update.state=IDLE;
        return ok && !update.failed ? 1 : -1;
    }
    default: ok=false; break;
    }
    if (!ok) { update.failed=true; update.state=OFF; }
    return 0;
}
