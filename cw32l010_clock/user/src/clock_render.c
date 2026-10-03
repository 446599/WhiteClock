#include "clock_core.h"
#include "ui_font.h"

enum { DRAW_RECT, DRAW_SMALL, DRAW_LARGE, DRAW_MEDIUM };
typedef struct { uint16_t x,y,a,b; uint8_t kind,shade; } DrawOp;
static DrawOp ops[96];
static unsigned op_count;
static ClockFace cached;
static bool cache_valid;

static void add(unsigned x,unsigned y,unsigned a,unsigned b,unsigned kind,unsigned shade)
{
    if (op_count < sizeof ops/sizeof ops[0]) ops[op_count++]=(DrawOp){x,y,a,b,kind,shade};
}
static void rect(unsigned x,unsigned y,unsigned w,unsigned h,unsigned shade) { add(x,y,w,h,DRAW_RECT,shade); }
static uint16_t next_char(const char **p)
{
    const uint8_t *s=(const uint8_t *)*p;
    uint16_t c;
    if (*s<128) { c=*s; *p+=1; }
    else { c=((s[0]&15u)<<12)|((s[1]&63u)<<6)|(s[2]&63u); *p+=3; }
    return c;
}
static unsigned text_width(const char *s)
{
    unsigned n=0;
    while (*s) n+=next_char(&s)<128 ? 8u : 16u;
    return n;
}
static void text(unsigned x,unsigned y,const char *s,unsigned shade)
{
    while (*s) {
        uint16_t c=next_char(&s);
        for (unsigned i=0;i<SMALL_GLYPH_COUNT;++i)
            if (small_glyphs[i].code==c) { add(x,y,i,0,DRAW_SMALL,shade); break; }
        x+=c<128 ? 8u : 16u;
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
static void home(const ClockFace *f)
{
    const ClockTime *t=&f->time;
    char date[32];date_text(date,t);
    const char *period=f->settings.hour12 ? (t->hour<12 ? "上午" : "下午") : "24小时制";
    if (f->settings.theme==0) {
        text(16,12,date,0);text(232,12,weekdays[clock_weekday(t)],0);
        rect(16,34,264,1,2);
        time_digits(t,f->settings.hour12,47,49,41,true,0);
        rect(16,116,264,1,2);
        text(16,126,period,0);text(216,126,"时光留白",0);
    } else if (f->settings.theme==1) {
        text(16,10,date,0);text(280-text_width(period),10,period,0);
        unsigned h=clock_display_hour(t->hour,f->settings.hour12);
        const uint8_t d[]={h/10u,h%10u,t->minute/10u,t->minute%10u};
        static const uint8_t xs[]={18,76,170,228};
        for (unsigned i=0;i<4;++i) {
            rect(xs[i]+2,41,50,71,1);rect(xs[i],38,50,71,2);
            rect(xs[i],38,50,1,0);rect(xs[i],108,50,1,0);
            add(xs[i]+6,47,d[i],0,DRAW_LARGE,0);
            rect(xs[i],73,4,2,1);rect(xs[i]+46,73,4,2,1);
        }
        rect(145,60,5,5,0);rect(145,83,5,5,0);
        centered(128,weekdays[clock_weekday(t)],0);
    } else {
        rect(12,12,88,128,2);rect(12,12,88,25,0);
        char month[]="00月";two(month,t->month);text(40,16,month,3);
        add(18,48,t->day/10u,0,DRAW_LARGE,0);add(56,48,t->day%10u,0,DRAW_LARGE,0);
        text(32,115,weekdays[clock_weekday(t)],0);
        char year[]="2000年";two(year+2,t->year);text(122,17,year,0);
        text(282-text_width(period),17,period,0);
        time_digits(t,f->settings.hour12,124,60,28,false,0);
        rect(122,108,160,1,1);
        text(122,121,"日有所得",0);
    }
}
static void footer(const char *s) { rect(12,122,272,1,2);centered(131,s,0); }
static void title(const char *s) { text(14,9,s,0);rect(12,30,272,1,1); }
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
    if (f->page==PAGE_HOME || f->page==PAGE_THEME) {
        home(f);
        if (f->page==PAGE_THEME) {
            rect(0,120,296,32,3);
            text(12,130,themes[f->settings.theme],0);text(92,130,"K1切换 K2保存",0);
        }
    } else if (f->page==PAGE_MENU) {
        title("设置");
        static const char *items[]={"时间设置","日期设置","时间制式","定时反色","显示主题","保存返回"};
        unsigned first=f->cursor<4 ? 0 : 2;
        for (unsigned i=first;i<first+4;++i) choice(39+(i-first)*21,items[i],i==f->cursor);
        footer("K1下一项  K2确认  长按K1返回");
    } else if (f->page==PAGE_FORMAT) {
        title("时间制式");
        choice(46,"24小时制",f->cursor==0);choice(79,"12小时制  上午/下午",f->cursor==1);
        footer("K1切换  K2保存  长按K1取消");
    } else if (f->page==PAGE_INVERT) invert_page(f);
    else editor(f);
    if (f->error) {
        rect(0,122,296,30,3);
        centered(131,f->error==1 ? "时钟异常" : "保存失败  请重新尝试",0);
    }
}
static void dimensions(const DrawOp *o,unsigned *w,unsigned *h)
{
    if (o->kind==DRAW_RECT) { *w=o->a;*h=o->b; }
    else if(o->kind==DRAW_SMALL) { *w=small_glyphs[o->a].width;*h=16; }
    else if(o->kind==DRAW_LARGE) { *w=38;*h=52; }
    else { *w=24;*h=32; }
}
static bool ink(const DrawOp *o,unsigned x,unsigned y)
{
    if (o->kind==DRAW_RECT) return true;
    if (o->kind==DRAW_SMALL) return (small_glyphs[o->a].rows[y] & (0x8000u>>x))!=0;
    const uint8_t *data=o->kind==DRAW_LARGE ? large_digits[o->a] : medium_digits[o->a];
    unsigned stride=o->kind==DRAW_LARGE?5u:3u;
    return (data[y*stride+x/8u]&(0x80u>>(x%8u)))!=0;
}
uint8_t clock_shade(const ClockFace *f,unsigned x,unsigned y)
{
    if (!f || x>=296 || y>=152) return 3;
    prepare(f);uint8_t value=3;
    for (unsigned i=0;i<op_count;++i) {
        DrawOp *o=&ops[i];unsigned w,h;dimensions(o,&w,&h);
        if (x>=o->x && x<o->x+w && y>=o->y && y<o->y+h && ink(o,x-o->x,y-o->y)) value=o->shade;
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
            if (ink(o,x-o->x,y-o->y)) shades[y]=o->shade;
    }
    if (f->inverted) for (unsigned y=0;y<152;++y) shades[y]=3u-shades[y];
}
