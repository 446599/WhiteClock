#include "board.h"
#include "epd.h"
#include "epd_lut.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t tick, busy_until;
static uint8_t cmd, area[7], old_wire[5624], new_wire[5624], commands[128];
static unsigned old_bytes, new_bytes, command_count, areas, resets, sleep_payloads;
static uint8_t data_interval;
static unsigned border_writes;
static uint8_t lut_wire[5][56];
static unsigned lut_writes;
static bool stuck, fail_io, active, tx_data, partial_mode;
static const uint8_t *tx;
static uint8_t held[56];
static unsigned tx_length, tx_polls;

uint32_t board_millis(void) { return tick; }
bool board_epd_busy(void) { return stuck || tick < busy_until; }
void board_epd_reset(bool high) { if (!high) partial_mode=false; ++resets; }
bool board_epd_begin(bool data,const uint8_t *p,unsigned n)
{
    assert(!active && n && n<=sizeof held);
    if (fail_io) return false;
    active=true; tx_data=data; tx=p; tx_length=n;
    tx_polls=0;
    memcpy(held,p,n);
    return true;
}
int board_epd_poll(void)
{
    assert(active && !memcmp(tx,held,tx_length));
    if (fail_io) { active=false; return -1; }
    if (++tx_polls<3u) return 0;
    active=false;
    if (!tx_data) {
        cmd=*tx;
        assert(command_count<sizeof commands);
        commands[command_count++]=cmd;
        if (cmd==0x90 || cmd==0x91) assert(!partial_mode);
        if (cmd==0x91) partial_mode=true;
        if (cmd==0x92) partial_mode=false;
        if (cmd==0x10) old_bytes=0;
        if (cmd==0x13) new_bytes=0;
        if (cmd==0x12) {
            unsigned n=partial_mode ? (area[1]-area[0]+1u)/8u : 19u;
            unsigned h=partial_mode ? (area[4]*256u+area[5])-(area[2]*256u+area[3])+1u : 296u;
            assert(old_bytes==n*h && new_bytes==n*h);
            busy_until=tick+20u;
        }
        if (cmd==0x92 || cmd==0x02 || cmd==0x07) assert(!board_epd_busy() || stuck);
        if (cmd==0x02) busy_until=tick+25u;
    } else if (cmd==0x00) {
        /* UC8251 PSR is one byte: register LUT, monochrome, factory scan direction. */
        assert(tx_length==1u && held[0]==0xf3u);
    } else if (cmd==0x50) {
        assert(tx_length==1u && (held[0]&0x0fu)==7u);
        data_interval=held[0];
        ++border_writes;
    } else if (cmd==0x61) {
        assert(tx_length==3u && held[0]==152u && held[1]==1u && held[2]==40u);
    } else if (cmd>=0x20 && cmd<=0x24) {
        assert(tx_length==56u); /* Seven eight-byte UC8251 monochrome LUT groups. */
        memcpy(lut_wire[cmd-0x20],tx,tx_length); ++lut_writes;
    } else if (cmd==0x90) {
        assert(tx_length==7u); memcpy(area,tx,7); ++areas;
        assert(!(area[0]&7u) && (area[1]&7u)==7u && area[6]==1u);
    } else if (cmd==0x10 || cmd==0x13) {
        unsigned *count=cmd==0x10 ? &old_bytes : &new_bytes;
        uint8_t *wire=cmd==0x10 ? old_wire : new_wire;
        assert(*count+tx_length<=5624);
        memcpy(wire+*count,tx,tx_length); *count+=tx_length;
    } else if (cmd==0x07) {
        assert(tx_length==1u && held[0]==0xa5u && (!board_epd_busy() || stuck));
        ++sleep_payloads;
    }
    return 1;
}
static void clear_wire(void)
{
    assert(!epd_active() && !active);
    old_bytes=new_bytes=command_count=areas=resets=sleep_payloads=0;
    data_interval=0xffu;
    border_writes=0;
    lut_writes=0;
    partial_mode=false;
}
static int finish(void)
{
    for (unsigned i=0;i<22000;++i) {
        ++tick;
        int result=epd_poll();
        if (result) {
            if (result>0) assert(epd_metrics.total_ms==epd_metrics.setup_ms+epd_metrics.old_ms+
                epd_metrics.new_ms+epd_metrics.busy_ms+epd_metrics.finish_ms);
            if (result>0) assert(sleep_payloads==1u);
            return result;
        }
    }
    assert(!"EPD did not finish"); return 0;
}
static void assert_image(const ClockFace *f,const uint8_t *wire,unsigned bytes,bool partial)
{
    unsigned x0=partial ? area[0] : 0u, x1=partial ? area[1] : 151u;
    unsigned y0=partial ? area[2]*256u+area[3] : 0u;
    unsigned y1=partial ? area[4]*256u+area[5] : 295u;
    unsigned n=(x1-x0+1u)/8u, offset=0;
    /* Transport encoding alone does not establish the selected physical LUT. */
    assert(((data_interval>>4)&3u)<=1u);
    assert(bytes==n*(y1-y0+1u));
    for (unsigned y=y0;y<=y1;++y) {
        uint8_t row[19]; clock_render_row(f,y,row);
        for (unsigned i=0;i<n;++i)
            assert((uint8_t)~wire[offset++]==row[x0/8u+i]);
    }
}
static ClockFace face(uint8_t page,uint8_t cursor)
{
    ClockUI ui;
    ClockTime t={26,10,4,12,34,0};
    ClockSettings s=clock_defaults();
    clock_ui_init(&ui,&t,&s);
    ui.page=page; ui.cursor=cursor;
    return clock_ui_face(&ui);
}
static void assert_luts(bool fast)
{
    assert(lut_writes==5u);
    assert(!memcmp(lut_wire,fast && EPD_USE_FAST_LUT ? epd_fast_lut : epd_full_lut,sizeof lut_wire));
}
static void assert_border(uint8_t expected)
{
    assert(border_writes==1u && data_interval==expected);
}
static uint8_t pixel_lut(uint8_t cdi,unsigned old_bit,unsigned new_bit)
{
    /* UC8251d p27: DDX[0] reverses both OLD and NEW before LUT selection. */
    unsigned ddx=(cdi>>4)&3u;
    assert(ddx<2u);
    return 0x21u+(old_bit^(ddx&1u))+2u*(new_bit^(ddx&1u));
}
static uint8_t border_lut(uint8_t cdi)
{
    unsigned vbd=cdi>>6;
    if (vbd==0u || vbd==3u) return 0;
    return ((vbd==1u)^((cdi>>4)&1u)) ? 0x22u : 0x23u;
}
static void assert_pixel_luts(bool fast)
{
    assert((data_interval&0x3fu)==0x07u);
    /* Renderer: white=1, black=0. The board wire boundary complements both. */
    for (unsigned old_black=0;old_black<2u;++old_black)
        for (unsigned new_black=0;new_black<2u;++new_black) {
            unsigned old_bit=(!old_black)^!!EPD_PANEL_INVERT_DATA;
            unsigned new_bit=(!new_black)^!!EPD_PANEL_INVERT_DATA;
            assert(pixel_lut(data_interval,old_bit,new_bit)==0x21u+old_black+2u*new_black);
        }
    /* Only VBD changes in the full-waveform white-border comparison. */
    if ((fast && EPD_USE_FAST_LUT) || EPD_FULL_BORDER_DATA==0x47u)
        assert(border_lut(data_interval)==0x22u);
    else assert(border_lut(data_interval)==0x23u);
}
enum { AREA_COMMAND, AREA_DATA, ENTER_COMMAND, OLD_COMMAND, OLD_DATA, OLD_READY, NEW_READY };
static bool at_retarget_point(unsigned point)
{
    if (point==AREA_COMMAND) return active && !tx_data && held[0]==0x90;
    if (point==AREA_DATA) return active && tx_data && cmd==0x90;
    if (point==ENTER_COMMAND) return active && !tx_data && held[0]==0x91;
    if (point==OLD_COMMAND) return active && !tx_data && held[0]==0x10;
    if (point==OLD_DATA) return active && tx_data && cmd==0x10;
    if (point==OLD_READY) return old_bytes && !active && !new_bytes;
    unsigned n=EPD_USE_PARTIAL_WINDOW ? (area[1]-area[0]+1u)/8u : 19u;
    unsigned h=EPD_USE_PARTIAL_WINDOW ? (area[4]*256u+area[5])-(area[2]*256u+area[3])+1u : 296u;
    return old_bytes==n*h && !active && !new_bytes;
}
static void retarget_at(unsigned point,const ClockFace *old,const ClockFace *next,
                        const ClockFace *latest)
{
    clear_wire();
    assert(epd_start(old,next,true));
    unsigned polls=0;
    while (!at_retarget_point(point)) { assert(++polls<3000); ++tick; assert(epd_poll()==0); }
    assert(epd_retarget(latest));
    while (!new_bytes) { assert(++polls<6000); ++tick; assert(epd_poll()==0); }
    assert(!epd_retarget(next));
    assert(finish()==1 && !partial_mode && epd_metrics.refreshed);
    assert_image(old,old_wire,old_bytes,EPD_USE_PARTIAL_WINDOW);
    assert_image(latest,new_wire,new_bytes,EPD_USE_PARTIAL_WINDOW);
}
static void cancel_at(unsigned point,const ClockFace *old,const ClockFace *next)
{
    clear_wire();
    assert(epd_start(old,next,true));
    unsigned polls=0;
    while (!at_retarget_point(point)) { assert(++polls<3000); ++tick; assert(epd_poll()==0); }
    assert(epd_retarget(old));
    assert(finish()==1 && !partial_mode);
    if (EPD_USE_PARTIAL_WINDOW) {
        assert(!epd_metrics.refreshed && !new_bytes);
        for (unsigned i=0;i<command_count;++i) assert(commands[i]!=0x12);
    } else {
        /* Full-frame OLD no longer depends on retargets; finish it without restarting. */
        assert(epd_metrics.refreshed && !areas);
        assert_image(old,old_wire,old_bytes,false);
        assert_image(old,new_wire,new_bytes,false);
    }
}
static void retarget_full(const ClockFace *old,const ClockFace *next,bool fast)
{
    clear_wire();
    ClockFace latest=*next;
    assert(epd_start(old,next,fast));
    unsigned accepted=0;
    for (unsigned polls=0;polls<5000u && !new_bytes;++polls) {
        ++tick;
        if (polls%150u==0u) {
            latest.cursor=accepted%2u;
            latest.time.minute=accepted%60u;
            assert(epd_retarget(&latest)); ++accepted;
        }
        assert(epd_poll()==0);
    }
    assert(accepted>1u && new_bytes && !epd_retarget(next));
    assert(finish()==1 && !areas && !partial_mode && epd_metrics.refreshed);
    unsigned old_commands=0;
    for (unsigned i=0;i<command_count;++i) if (commands[i]==0x10) ++old_commands;
    assert(old_commands==1u && epd_metrics.image_bytes==11248u);
    assert_luts(fast && old);
    assert_image(old,old_wire,old_bytes,false);
    assert_image(&latest,new_wire,new_bytes,false);
}
static void failure_and_recovery(const ClockFace *old,const ClockFace *next,unsigned point)
{
    clear_wire();
    assert(epd_start(old,next,true));
    unsigned polls=0;
    if (point==0u || point==1u) {
        while (!(active && tx_data && cmd==(point ? 0x13 : 0x10))) {
            assert(++polls<5000u); ++tick; assert(epd_poll()==0);
        }
        fail_io=true; ++tick; assert(epd_poll()==0); fail_io=false;
    } else {
        uint8_t stop=point==2u ? 0x12 : 0x02;
        while (!command_count || commands[command_count-1u]!=stop) {
            assert(++polls<5000u); ++tick; assert(epd_poll()==0);
        }
        stuck=true;
    }
    assert(finish()==-1);
    stuck=false;
    /* Keep the failed controller state until the recovery's hardware reset. */
    old_bytes=new_bytes=command_count=areas=sleep_payloads=lut_writes=0;
    assert(epd_start(0,next,false)); assert(finish()==1 && !partial_mode);
    assert_image(0,old_wire,old_bytes,false);
    assert_image(next,new_wire,new_bytes,false);
}
int main(void)
{
    ClockFace old=face(PAGE_MENU,0), next=face(PAGE_MENU,1);
    assert(epd_start(&old,&next,true)); assert(finish()==1);
    assert_luts(true);
    assert_border(EPD_USE_FAST_LUT ? EPD_FAST_BORDER_DATA : EPD_FULL_BORDER_DATA);
    assert_pixel_luts(true);
    assert(areas==(EPD_USE_PARTIAL_WINDOW ? 1u : 0u));
    assert(EPD_USE_PARTIAL_WINDOW ? old_bytes<5624u : old_bytes==5624u);
    assert(new_bytes==old_bytes);
    assert_image(&old,old_wire,old_bytes,EPD_USE_PARTIAL_WINDOW);
    assert_image(&next,new_wire,new_bytes,EPD_USE_PARTIAL_WINDOW);
    unsigned enter=0,refresh=0,exit=0,off=0;
    for (unsigned i=0;i<command_count;++i) {
        if(commands[i]==0x91) enter=i;
        if(commands[i]==0x12) refresh=i;
        if(commands[i]==0x92) exit=i;
        if(commands[i]==0x02) off=i;
    }
    if (EPD_USE_PARTIAL_WINDOW) assert(enter<refresh && refresh<exit && exit<off);
    else for (unsigned i=0;i<command_count;++i)
        assert(commands[i]!=0x90 && commands[i]!=0x91 && commands[i]!=0x92);
    assert(refresh<off && commands[off+1]==0x07);
    printf("menu cursor 0 -> 1: %u image bytes (baseline 11248)\n",old_bytes+new_bytes);
    assert(epd_metrics.image_bytes==old_bytes+new_bytes && epd_metrics.refreshed==1u);

    for (unsigned page=PAGE_HOME;page<=PAGE_TIME;page+=PAGE_TIME) {
        clear_wire();
        ClockFace a=face(page,1), b=a; ++b.time.minute;
        assert(epd_start(&a,&b,true)); assert(finish()==1);
        assert_image(&a,old_wire,old_bytes,EPD_USE_PARTIAL_WINDOW);
        assert_image(&b,new_wire,new_bytes,EPD_USE_PARTIAL_WINDOW);
        printf("%s minute 34 -> 35: %u image bytes (baseline 11248)\n",
               page==PAGE_HOME ? "home" : "editor",epd_metrics.image_bytes);
    }

    ClockFace latest=face(PAGE_TIME,1);
    for (unsigned point=EPD_USE_PARTIAL_WINDOW ? AREA_COMMAND : OLD_COMMAND;point<=NEW_READY;++point) {
        retarget_at(point,&old,&next,&latest);
        cancel_at(point,&old,&next);
    }
    clear_wire();
    assert(epd_start(&old,&next,true));
    while (!at_retarget_point(OLD_DATA)) { ++tick; assert(epd_poll()==0); }
    for (unsigned i=0;i<5;++i) {
        latest.cursor=i%2u;
        assert(epd_retarget(&latest)); ++tick; assert(epd_poll()==0);
    }
    assert(finish()==1);
    assert_image(&old,old_wire,old_bytes,EPD_USE_PARTIAL_WINDOW);
    assert_image(&latest,new_wire,new_bytes,EPD_USE_PARTIAL_WINDOW);

    retarget_full(&old,&latest,false);
    retarget_full(0,&latest,false);
    if (!EPD_USE_PARTIAL_WINDOW) retarget_full(&old,&latest,true);

    clear_wire(); latest=old; ++latest.time.minute;
    assert(epd_start(&old,&latest,true)); assert(finish()==1);
    assert(!old_bytes && !new_bytes && !epd_metrics.refreshed && !areas);
    /* A scene with no changed draw operations must not drive a waveform. */
    for (unsigned i=0;i<command_count;++i) assert(commands[i]!=0x12);

    /* Timer alerts require five real full-waveform updates of an unchanged page. */
    for (unsigned flash=0;flash<FOCUS_ALERT_FLASHES;++flash) {
        clear_wire();
        assert(epd_start(&old,&old,false));assert(finish()==1);
        assert(epd_metrics.refreshed && !areas && old_bytes==5624u && new_bytes==5624u);
        assert_luts(false);assert_border(EPD_FULL_BORDER_DATA);
        assert_image(&old,old_wire,old_bytes,false);
        assert_image(&old,new_wire,new_bytes,false);
        unsigned refreshes=0;
        for (unsigned i=0;i<command_count;++i) refreshes+=commands[i]==0x12;
        assert(refreshes==1u);
    }

    clear_wire();
    assert(epd_start(0,&next,true)); assert(finish()==1);
    assert_luts(false);
    assert_border(EPD_FULL_BORDER_DATA);
    assert_pixel_luts(false);
    assert(!areas && old_bytes==5624 && new_bytes==5624);
    assert_image(0,old_wire,old_bytes,false);
    assert_image(&next,new_wire,new_bytes,false);
    clear_wire(); next.inverted=1;
    assert(epd_start(&old,&next,true)); assert(finish()==1);
    assert_luts(false);
    assert(!areas && old_bytes==5624 && new_bytes==5624);

    next=face(PAGE_MENU,1);
    for (unsigned point=0;point<4u;++point) failure_and_recovery(&old,&next,point);

    clear_wire(); stuck=true;
    assert(epd_start(&old,&old,true)); assert(finish()==-1); stuck=false;
    clear_wire(); fail_io=true;
    assert(epd_start(0,&old,false)); assert(finish()==-1); fail_io=false;
    puts("PASS: UC8251 PSR, resolution, DDX polarity, LUT lengths, factory window, retarget and recovery");
}
