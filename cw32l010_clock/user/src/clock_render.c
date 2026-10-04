#include "clock_core.h"
#include "ui_font.h"

enum { DRAW_RECT, DRAW_SMALL, DRAW_LARGE, DRAW_MEDIUM,
       DRAW_ROUND, DRAW_FLAP, DRAW_FLIP_DIGIT };
typedef struct { uint16_t x,y,a,b; uint8_t kind,shade; } DrawOp;
static DrawOp ops[112];
static unsigned op_count;
static ClockFace cached;
static bool cache_valid;
#ifdef CLOCK_RENDER_TEST
static bool overflowed;
#endif

static void add(unsigned x,unsigned y,unsigned a,unsigned b,unsigned kind,unsigned shade)
{
    if (op_count < sizeof ops/sizeof ops[0]) ops[op_count++]=(DrawOp){x,y,a,b,kind,shade};
#ifdef CLOCK_RENDER_TEST
    else overflowed=true;
#endif
}
static void rect(unsigned x,unsigned y,unsigned w,unsigned h,unsigned shade) { add(x,y,w,h,DRAW_RECT,shade); }
static void rounded(unsigned x,unsigned y,unsigned w,unsigned h,unsigned shade)
{ add(x,y,w,h,DRAW_ROUND,shade); }
#ifdef CLOCK_RENDER_TEST
/* Tests inspect the real list budget, without reserving diagnostic RAM in firmware. */
bool clock_render_overflowed(void) { return overflowed; }
unsigned clock_render_op_count(void) { return op_count; }
#endif
static uint16_t next_char(const char **p)
{
    const uint8_t *s=(const uint8_t *)*p;
    uint16_t c;
    if (*s<128) { c=*s; *p+=1; }
    else if (*s<224u) { c=((s[0]&31u)<<6)|(s[1]&63u); *p+=2; }
    else { c=((s[0]&15u)<<12)|((s[1]&63u)<<6)|(s[2]&63u); *p+=3; }
    return c;
}
static unsigned text_width(const char *s)
{
    unsigned n=0;
    while (*s) n+=next_char(&s)<256 ? 8u : 16u;
    return n;
}
static void text(unsigned x,unsigned y,const char *s,unsigned shade)
{
    while (*s) {
        uint16_t c=next_char(&s);
        for (unsigned i=0;i<SMALL_GLYPH_COUNT;++i)
            if (small_glyphs[i].code==c) { add(x,y,i,0,DRAW_SMALL,shade); break; }
        x+=c<256 ? 8u : 16u;
    }
}
static void centered(unsigned y,const char *s,unsigned shade) { text((296u-text_width(s))/2u,y,s,shade); }
static void two(char *s,unsigned n) { s[0]='0'+n/10u; s[1]='0'+n%10u; }
static void date_text(char s[32],const ClockTime *t)
{
    const char pattern[]="2000年00月00日";
    for (unsigned i=0;i<sizeof pattern;++i) s[i]=pattern[i];
    two(s+2,t->year);two(s+7,t->month);two(s+12,t->day);
}
static const char *weekdays[]={"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};
static const char *themes[]={"留白","翻页","日历"};
static void time_digits(const ClockTime *t,bool hour12,unsigned x,unsigned y,unsigned step,bool large,unsigned shade)
{
    unsigned h=clock_display_hour(t->hour,hour12);
    const uint8_t digits[]={h/10u,h%10u,10u,t->minute/10u,t->minute%10u};
    for (unsigned i=0;i<5;++i) add(x+i*step,y,digits[i],0,large?DRAW_LARGE:DRAW_MEDIUM,shade);
}
static void outline(unsigned x,unsigned y,unsigned w,unsigned h)
{
    rect(x,y,w,1,0);rect(x,y+h-1,w,1,0);
    rect(x,y,1,h,0);rect(x+w-1,y,1,h,0);
}
static void temperature(const ClockFace *f,unsigned x,unsigned y)
{
    char value[]="芯温 --°C";
    if (f->temperature_valid) {
        int t=f->temperature_c;
        if (t<0) { text(x,y,"芯温",0); text(x+40,y,"-",0); t=-t;
            char n[]="00°C";two(n,(unsigned)t);text(x+48,y,n,0);return; }
        two(value+7,(unsigned)t);
    }
    text(x,y,value,0);
}
/* Original 16x28 geometric numerals, drawn on an integer grid. The flip theme
 * doubles each bit to a 2x2 block (36x60 cell including a 2px margin).
 * Solid glyph ink is never dithered; only the card material and shadow are. */
static const uint16_t flip_digit_rows[10][28] = {
    { /* 0 */
        0x0000, 0x07f0, 0x0ff8, 0x3ffc, 0x7ffe, 0xfe1f, 0xf80f, 0xf00f, 0xf00f, 0xf00f, 0xf00f, 0xf00f, 0xf00f, 0xf00f,
        0xf00f, 0xf00f, 0xf00f, 0xf00f, 0xf00f, 0xf00f, 0xf00f, 0xf00f, 0xf81f, 0x7ffe, 0x3ffc, 0x1ff8, 0x0ff0, 0x0000
    },
    { /* 1 */
        0x0000, 0x0080, 0x01f0, 0x03f0, 0x0ff0, 0x1ff0, 0x0ff0, 0x0ef0, 0x04f0, 0x00f0, 0x00f0, 0x00f0, 0x00f0, 0x00f0,
        0x00f0, 0x00f0, 0x00f0, 0x00f0, 0x00f0, 0x00f0, 0x00f0, 0x00f0, 0x00f0, 0x00f0, 0x1ffe, 0x1ffe, 0x1ffe, 0x1ffe
    },
    { /* 2 */
        0x0000, 0x0ff0, 0x1ff8, 0x7ffc, 0xfffe, 0xf81f, 0xf00f, 0x000f, 0x000f, 0x001f, 0x001f, 0x003e, 0x007e, 0x007c,
        0x00f8, 0x01f0, 0x03f0, 0x03e0, 0x07c0, 0x0f80, 0x1f80, 0x1f00, 0x3e00, 0x7c00, 0x7ffe, 0x7ffe, 0x3ffe, 0x3ffe
    },
    { /* 3 */
        0x0000, 0x3ff0, 0x3ff8, 0x3ffc, 0x3ffe, 0x001f, 0x000f, 0x000f, 0x000f, 0x000f, 0x001f, 0x03fe, 0x03fc, 0x03f8,
        0x03fc, 0x003e, 0x001f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x001f, 0x3ffe, 0x3ffc, 0x3ff8, 0x3ff0, 0x0000
    },
    { /* 4 */
        0x0000, 0x0040, 0x00fc, 0x00fc, 0x01fc, 0x01fc, 0x03fc, 0x03fc, 0x07fc, 0x07bc, 0x0fbc, 0x0f3c, 0x1f3c, 0x1e3c,
        0x3e3c, 0x3c3c, 0x7ffe, 0x3ffe, 0x3ffe, 0x3ffe, 0x003c, 0x003c, 0x003c, 0x003c, 0x003c, 0x003c, 0x003c, 0x0000
    },
    { /* 5 */
        0x3ffe, 0x3ffe, 0x7ffe, 0x7ffe, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7ff0, 0x7ff8,
        0x3ffc, 0x3ffe, 0x001f, 0x000f, 0x000f, 0x000f, 0x000f, 0x000f, 0x001f, 0x3ffe, 0x3ffc, 0x3ff8, 0x3ff0, 0x0000
    },
    { /* 6 */
        0x03f8, 0x07f8, 0x0ff8, 0x1ff8, 0x1f00, 0x3e00, 0x7e00, 0x7c00, 0x7800, 0x7800, 0x7800, 0x7800, 0x7fe0, 0x7ff0,
        0x7ff8, 0x7ffc, 0x783e, 0x781e, 0x781e, 0x781e, 0x781e, 0x781e, 0x781e, 0x7c3e, 0x3ffc, 0x1ff8, 0x0ff0, 0x07e0
    },
    { /* 7 */
        0x0000, 0x7ffe, 0x7fff, 0x7fff, 0x7fff, 0x000f, 0x001f, 0x001e, 0x001e, 0x003e, 0x003c, 0x003c, 0x007c, 0x0078,
        0x00f8, 0x00f0, 0x00f0, 0x01f0, 0x01e0, 0x01e0, 0x03e0, 0x03c0, 0x03c0, 0x07c0, 0x0780, 0x0080, 0x0000, 0x0000
    },
    { /* 8 */
        0x0000, 0x0ff0, 0x1ff8, 0x3ffc, 0x7ffe, 0xf81f, 0xf00f, 0xf00f, 0xf00f, 0xf00f, 0xf81f, 0x7ffe, 0x3ffc, 0x1ff8,
        0x3ffc, 0x7ffe, 0xf81f, 0xf00f, 0xf00f, 0xf00f, 0xf00f, 0xf00f, 0xf81f, 0x7ffe, 0x3ffc, 0x1ff8, 0x0ff0, 0x0000
    },
    { /* 9 */
        0x07e0, 0x0ff0, 0x1ff8, 0x3ffc, 0x7c3e, 0x781e, 0x781e, 0x781e, 0x781e, 0x781e, 0x781e, 0x7c1e, 0x3ffe, 0x1ffe,
        0x0ffe, 0x07fe, 0x001e, 0x001e, 0x001e, 0x001e, 0x003e, 0x007e, 0x007c, 0x00f8, 0x3ff8, 0x3ff0, 0x3fe0, 0x3fc0
    },
};

static void flip_card(unsigned x,unsigned value)
{
    const unsigned y=31, w=126, h=78;
    /* A fixed spatial texture: old/new frames reconstruct identical shadows. */
    rounded(x+3,y+5,w,h,2);             /* dispersed drop shadow */
    rounded(x,y+2,w,h,0);               /* stacked lower leaf */
    add(x,y,w,h,DRAW_FLAP,1);           /* charcoal upper leaf, black lower */
    rect(x+6,y+1,w-12,1,2);             /* bevel, kept away from the corners */
    rect(x+6,y+h-2,w-12,1,1);
    add(x+21,y+9,value/10u,0,DRAW_FLIP_DIGIT,3);
    add(x+69,y+9,value%10u,0,DRAW_FLIP_DIGIT,3);
    rect(x+1,y+39,w-2,1,0);             /* physical split, not a gray numeral */
    for (unsigned side=0;side<2;++side) {
        unsigned hx=x+(side?w-4:0);
        rounded(hx,y+34,4,11,0);        /* hinge/axle */
        rect(hx+1,y+36,2,5,2);
        rect(hx+1,y+36,1,3,3);
    }
}
static void flip_home(const ClockFace *f)
{
    char date[32];date_text(date,&f->time);
    text(14,8,date,0);text(234,8,weekdays[clock_weekday(&f->time)],0);
    flip_card(12,clock_display_hour(f->time.hour,f->settings.hour12));
    flip_card(158,f->time.minute);
    rounded(146,57,4,4,0);rounded(146,80,4,4,0);
    if (f->focus_state==FOCUS_RUNNING || f->focus_state==FOCUS_PAUSED) {
        char left[]="专注 00分";two(left+7,f->focus_minutes);text(14,120,left,0);
        if (f->focus_state==FOCUS_PAUSED) text(110,120,"暂停",0);
    } else text(14,120,f->settings.hour12 ? (f->time.hour<12 ? "上午" : "下午") : "24小时制",0);
    temperature(f,202,120);
    /* The ornament view omits persistent button help; theme selection keeps it. */
    rect(14,143,268,2,2);
    rect(14,143,(f->time.hour*60u+f->time.minute)*268u/1440u,2,0);
}
static void home(const ClockFace *f)
{
    if (f->settings.theme==1) { flip_home(f);return; }
    const ClockTime *t=&f->time;
    char date[32];date_text(date,t);
    const char *period=f->settings.hour12 ? (t->hour<12 ? "上午" : "下午") : "24小时制";
    text(14,10,date,0);text(234,10,weekdays[clock_weekday(t)],0);
    rect(14,32,268,1,0);
    if (f->settings.theme==0) {
        time_digits(t,f->settings.hour12,47,41,41,true,0);
    } else {
        rect(14,39,88,60,0);
        add(20,43,t->day/10u,0,DRAW_LARGE,3);add(58,43,t->day%10u,0,DRAW_LARGE,3);
        time_digits(t,f->settings.hour12,125,42,28,false,0);
        text(127,81,"今日值得专注",0);
    }
    if (f->focus_state==FOCUS_RUNNING || f->focus_state==FOCUS_PAUSED) {
        char left[]="专注 00分";two(left+7,f->focus_minutes);text(14,106,left,0);
    } else text(14,106,period,0);
    temperature(f,202,106);
    /* Daily progress is black/white only, advanced once per displayed minute. */
    rect(14,127,268,1,0);rect(14,126,(t->hour*60u+t->minute)*268u/1440u,3,0);
    centered(133,"K1设置   K2主题",0);
}
static void focus_page(const ClockFace *f)
{
    text(14,10,"专注计时",0);temperature(f,202,10);rect(14,32,268,1,0);
    add(81,42,f->focus_minutes/10u,0,DRAW_LARGE,0);
    add(122,42,f->focus_minutes%10u,0,DRAW_LARGE,0);
    text(174, 74,"分钟",0);
    const char *status=f->focus_state==FOCUS_RUNNING ? "保持专注" :
        f->focus_state==FOCUS_PAUSED ? "已暂停" : f->focus_state==FOCUS_DONE ? "已完成  休息一下" : "选择时长后开始";
    centered(103,status,0);
    outline(14,123,268,5);rect(15,124,266u*f->focus_progress/100u,3,0);
    centered(133,f->focus_state==FOCUS_RUNNING ? "K1重置 K2暂停 长按K1返回" :
        f->focus_state==FOCUS_PAUSED ? "K1重置 K2继续 长按K1返回" :
        f->focus_state==FOCUS_DONE ? "K2确认完成 长按K1返回" : "K1时长 K2开始 长按K1返回",0);
}
static void footer(const char *s) { rect(12,122,272,1,0);centered(131,s,0); }
static void title(const char *s) { text(14,9,s,0);rect(12,30,272,1,0); }
static void choice(unsigned y,const char *s,bool selected)
{
    if (selected) rect(12,y-2,272,21,0);
    text(22,y,s,selected?3:0);
}
static void editor(const ClockFace *f)
{
    title(f->page==PAGE_TIME ? "时间设置" : "日期设置");
    if (f->page==PAGE_TIME) {
        time_digits(&f->time,false,79,51,28,false,0);
        if (f->cursor<2) rect(f->cursor==0?79:163,89,52,3,0);
        if (f->cursor<2) centered(99,f->cursor==0?"小时":"分钟",0);
        else choice(98,"保存时间",true);
    } else {
        char date[32];date_text(date,&f->time);centered(51,date,0);
        centered(80,weekdays[clock_weekday(&f->time)],0);
        static const uint8_t x[]={92,140,172},w[]={32,16,16};
        if (f->cursor<3) rect(x[f->cursor],72,w[f->cursor],2,0);
        else choice(99,"保存日期",true);
    }
    footer("K1下一项  K2加一  长按K1取消");
}
static void invert_page(const ClockFace *f)
{
    title("定时反色");
    text(18,39,"启用",0);text(70,39,f->settings.invert_enabled?"开":"关",0);
    if (f->cursor==0) rect(68,58,20,2,0);
    char start[]="00:00",end[]="00:00";
    two(start,f->settings.start_hour);two(start+3,f->settings.start_minute);
    two(end,f->settings.end_hour);two(end+3,f->settings.end_minute);
    text(18,70,"开始",0);text(62,70,start,0);
    text(154,70,"结束",0);text(198,70,end,0);
    static const uint8_t x[]={0,62,86,198,222};
    if (f->cursor>=1 && f->cursor<=4) rect(x[f->cursor],90,16,2,0);
    bool all=f->settings.start_hour==f->settings.end_hour && f->settings.start_minute==f->settings.end_minute;
    if (f->cursor==5) choice(99,"保存设置",true);
    else centered(99,all?"全天反色":"按24小时制设置",0);
    footer("K1下一项  K2修改  长按K1取消");
}
static void prepare(const ClockFace *f)
{
    if (cache_valid && clock_face_equal(&cached,f)) return;
    cached=*f;cache_valid=true;op_count=0;
#ifdef CLOCK_RENDER_TEST
    overflowed=false;
#endif
    if (f->page==PAGE_HOME || f->page==PAGE_THEME) {
        home(f);
        if (f->page==PAGE_THEME) {
            rect(0,120,296,32,3);
            text(12,130,themes[f->settings.theme],0);text(92,130,"K1切换 K2保存",0);
        }
    } else if (f->page==PAGE_MENU) {
        title("设置");
        static const char *items[]={"时间设置","日期设置","时间制式","定时反色","显示主题","专注计时","保存返回"};
        unsigned first=f->cursor<4 ? 0 : 3;
        for (unsigned i=first;i<first+4;++i) choice(39+(i-first)*21,items[i],i==f->cursor);
        footer("K1下一项  K2确认  长按K1返回");
    } else if (f->page==PAGE_FOCUS) focus_page(f);
    else if (f->page==PAGE_FORMAT) {
        title("时间制式");
        choice(46,"24小时制",f->cursor==0);choice(79,"12小时制  上午/下午",f->cursor==1);
        footer("K1切换  K2保存  长按K1取消");
    } else if (f->page==PAGE_INVERT) invert_page(f);
    else editor(f);
    if (f->error) {
        rect(0,122,296,30,3);
        centered(131,f->error==1 ? "时钟异常" : f->error==3 ? "正在保存  可长按返回" : "保存失败  请重新尝试",0);
    }
}
static void dimensions(const DrawOp *o,unsigned *w,unsigned *h)
{
    if (o->kind==DRAW_RECT || o->kind==DRAW_ROUND || o->kind==DRAW_FLAP) { *w=o->a;*h=o->b; }
    else if(o->kind==DRAW_FLIP_DIGIT) { *w=36;*h=60; }
    else if(o->kind==DRAW_SMALL) { *w=small_glyphs[o->a].width;*h=16; }
    else if(o->kind==DRAW_LARGE) { *w=38;*h=52; }
    else { *w=24;*h=32; }
}
#ifdef CLOCK_RENDER_TEST
bool clock_render_bounds_valid(void)
{
    for (unsigned i=0;i<op_count;++i) {
        unsigned w,h;dimensions(&ops[i],&w,&h);
        if ((unsigned)ops[i].x+w>UI_WIDTH || (unsigned)ops[i].y+h>UI_HEIGHT) return false;
    }
    return true;
}
#endif
static bool ink(const DrawOp *o,unsigned x,unsigned y)
{
    if (o->kind==DRAW_RECT) return true;
    if (o->kind==DRAW_ROUND || o->kind==DRAW_FLAP) {
        /* Four stepped corner pixels, clamped for the small hinges and colon. */
        unsigned r=o->a<8 || o->b<8 ? 1u : 4u;
        unsigned edge=y<o->b-1u-y ? y : o->b-1u-y;
        static const uint8_t inset[]={4,2,1,1};
        unsigned cut=edge<r ? (r==1 ? 1u : inset[edge]) : 0u;
        return x>=cut && x<o->a-cut;
    }
    if (o->kind==DRAW_FLIP_DIGIT) {
        if (o->a>=10 || x<2 || x>=34 || y<2 || y>=58) return false;
        return (flip_digit_rows[o->a][(y-2)/2] & (0x8000u>>((x-2)/2)))!=0;
    }
    if (o->kind==DRAW_SMALL) return (small_glyphs[o->a].rows[y] & (0x8000u>>x))!=0;
    const uint8_t *data=o->kind==DRAW_LARGE ? large_digits[o->a] : medium_digits[o->a];
    unsigned stride=o->kind==DRAW_LARGE?5u:3u;
    return (data[y*stride+x/8u]&(0x80u>>(x%8u)))!=0;
}
static uint8_t op_shade(const DrawOp *o,unsigned y)
{
    return o->kind==DRAW_FLAP && y>=o->b/2 ? 0u : o->shade;
}
uint8_t clock_shade(const ClockFace *f,unsigned x,unsigned y)
{
    if (!f || x>=296 || y>=152) return 3;
    prepare(f);uint8_t value=3;
    for (unsigned i=0;i<op_count;++i) {
        DrawOp *o=&ops[i];unsigned w,h;dimensions(o,&w,&h);
        if (x>=o->x && x<o->x+w && y>=o->y && y<o->y+h && ink(o,x-o->x,y-o->y)) value=op_shade(o,y-o->y);
    }
    return f->inverted?3u-value:value;
}
void clock_render_column(const ClockFace *f,unsigned x,uint8_t shades[152])
{
    for (unsigned y=0;y<152;++y) shades[y]=3;
    if (!f || x>=296) return;
    prepare(f);
    for (unsigned i=0;i<op_count;++i) {
        DrawOp *o=&ops[i];unsigned w,h;dimensions(o,&w,&h);
        if (x<o->x || x>=o->x+w) continue;
        for (unsigned y=o->y;y<o->y+h && y<152;++y)
            if (ink(o,x-o->x,y-o->y)) shades[y]=op_shade(o,y-o->y);
    }
    if (f->inverted) for (unsigned y=0;y<152;++y) shades[y]=3u-shades[y];
}
