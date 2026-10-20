#include "clock_core.h"
#include "ui_font.h"
#include "clock_theme.h"

enum { DRAW_RECT, DRAW_SMALL, DRAW_LARGE, DRAW_MEDIUM };
typedef struct { uint16_t x,y,a,b; uint8_t kind,shade; } DrawOp;
static DrawOp ops[112];
static unsigned op_count;
static ClockFace cached;
static bool cache_valid;
static ClockWindow *damage;
static unsigned previous_ops;
static void include_op(const DrawOp *o);

static void add(unsigned x,unsigned y,unsigned a,unsigned b,unsigned kind,unsigned shade)
{
    if (op_count >= sizeof ops/sizeof ops[0]) return;
    DrawOp next={x,y,a,b,kind,shade}, *old=&ops[op_count];
    if (damage && (op_count>=previous_ops || old->x!=x || old->y!=y ||
        old->a!=a || old->b!=b || old->kind!=kind || old->shade!=shade)) {
        if (op_count<previous_ops) include_op(old);
        include_op(&next);
    }
    ops[op_count++]=next;
}
static void rect(unsigned x,unsigned y,unsigned w,unsigned h,unsigned shade) { add(x,y,w,h,DRAW_RECT,shade); }
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
static const char *themes[THEME_COUNT]={"错版","撕纸","头版","半调","几何","日牌","织纹"};
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
    char value[]="温度 --°C";
    if (f->temperature_valid) {
        int t=f->temperature_c;
        if (t<0) { text(x,y,"温度",0); text(x+40,y,"-",0); t=-t;
            char n[]="00°C";two(n,(unsigned)t);text(x+48,y,n,0);return; }
        two(value+7,(unsigned)t);
    }
    text(x,y,value,0);
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
    centered(133,f->focus_state==FOCUS_RUNNING ? "SET暂停 BACK重置 长按BACK返回" :
        f->focus_state==FOCUS_PAUSED ? "SET继续 BACK重置 长按BACK返回" :
        f->focus_state==FOCUS_DONE ? "SET确认完成 BACK时长 长按BACK返回" : "SET开始 BACK时长 长按BACK返回",0);
}
static void footer(const char *s) { rect(12,122,272,1,0);centered(131,s,0); }
static void title(const char *s) { text(14,9,s,0);rect(12,30,272,1,0); }
static void choice(unsigned y,const char *s,bool selected)
{
    text(30,y,s,0);
    unsigned shade=selected ? 0u : 3u;
    rect(16,y+4,4,8,shade);rect(15,y+5,6,6,shade);rect(14,y+6,8,4,shade);
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
    footer("SET修改  BACK下一项  长按BACK取消");
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
    footer("SET修改  BACK下一项  长按BACK取消");
}
static void prepare(const ClockFace *f)
{
    if (cache_valid && clock_face_equal(&cached,f)) return;
    cached=*f;cache_valid=true;op_count=0;
    if (f->page==PAGE_HOME || f->page==PAGE_THEME) {
        clock_theme_prepare(f,add,text);
        if (f->page==PAGE_THEME) {
            rect(0,120,296,32,3);
            text(12,130,themes[f->settings.theme],0);text(92,130,"SET保存 BACK切换",0);
        }
    } else if (f->page==PAGE_MENU) {
        title("设置");
        static const char *items[]={"时间设置","日期设置","时间制式","定时反色","显示主题","专注计时","WhiteClock V1.0"};
        unsigned first=f->cursor<4 ? 0 : 3;
        for (unsigned i=first;i<first+4;++i) {
            unsigned y=39+(i-first)*21;
            if (i<MENU_ITEM_COUNT) choice(y,items[i],i==f->cursor);
            else text(30,y,items[i],0);
        }
        footer("SET确认  BACK下一项  长按BACK返回");
    } else if (f->page==PAGE_FOCUS) focus_page(f);
    else if (f->page==PAGE_FORMAT) {
        title("时间制式");
        choice(46,"24小时制",f->cursor==0);choice(79,"12小时制  上午/下午",f->cursor==1);
        footer("SET保存  BACK切换  长按BACK取消");
    } else if (f->page==PAGE_INVERT) invert_page(f);
    else editor(f);
    if (f->error) {
        rect(0,122,296,30,3);
        centered(131,f->error==1 ? "时钟异常" : f->error==3 ? "正在保存  可长按返回" : "保存失败  请重新尝试",0);
    }
}
static void dimensions(const DrawOp *o,unsigned *w,unsigned *h)
{
    if (o->kind==DRAW_RECT) { *w=o->a;*h=o->b; }
    else if (o->kind==DRAW_THEME_BACKGROUND) { *w=UI_WIDTH;*h=UI_HEIGHT; }
    else if (o->kind==DRAW_THEME_GLYPH) clock_theme_dimensions(o->a,o->b,w,h);
    else if(o->kind==DRAW_SMALL) { *w=small_glyphs[o->a].width;*h=16; }
    else if(o->kind==DRAW_LARGE) { *w=38;*h=52; }
    else { *w=24;*h=32; }
}
static void include_op(const DrawOp *o)
{
    unsigned w,h; dimensions(o,&w,&h);
    if (!w || !h || o->x>=UI_WIDTH || o->y>=UI_HEIGHT) return;
    unsigned xe=o->x+w<UI_WIDTH ? o->x+w : UI_WIDTH;
    unsigned ye=o->y+h<UI_HEIGHT ? o->y+h : UI_HEIGHT;
    unsigned x0=o->y&~7u, x1=(ye-1u)|7u;
    unsigned y0=UI_WIDTH-xe, y1=UI_WIDTH-1u-o->x;
    if (x0<damage->x0) damage->x0=x0;
    if (x1>damage->x1) damage->x1=x1;
    if (y0<damage->y0) damage->y0=y0;
    if (y1>damage->y1) damage->y1=y1;
}
bool clock_changed_window(const ClockFace *old,const ClockFace *next,ClockWindow *window)
{
    if (!next || !window) return false;
    if (!old || old->inverted!=next->inverted) {
        *window=(ClockWindow){0,PANEL_WIDTH-1u,0,PANEL_HEIGHT-1u};
        return true;
    }
    prepare(old); previous_ops=op_count;
    *window=(ClockWindow){PANEL_WIDTH,0,PANEL_HEIGHT,0};
    /* Compare while replacing the cached scene, avoiding a second draw list
       or two framebuffers on the 4 KiB MCU. Removed ops also damage their area. */
    damage=window; cache_valid=false;
    prepare(next);
    for (unsigned i=op_count;i<previous_ops;++i) include_op(&ops[i]);
    damage=0;
    return window->x0<=window->x1 && window->y0<=window->y1;
}
static bool ink(const DrawOp *o,unsigned x,unsigned y)
{
    if (o->kind==DRAW_RECT) return true;
    if (o->kind==DRAW_THEME_GLYPH) return clock_theme_ink(o->a,o->b,x,y) &&
        (!(o->b&1024u) || (((o->x+x)/2u+(o->y+y)/2u)%2u)==0u);
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
        if (x>=o->x && x<o->x+w && y>=o->y && y<o->y+h) {
            if (o->kind==DRAW_THEME_BACKGROUND) value=clock_theme_background(o->a,x,y);
            else if (ink(o,x-o->x,y-o->y)) value=o->shade;
        }
    }
    return f->inverted?3u-value:value;
}
void clock_render_column_span(const ClockFace *f,unsigned x,unsigned first,
                              unsigned end,uint8_t shades[152])
{
    for (unsigned y=first;y<end;++y) shades[y]=3;
    if (!f || x>=296) return;
    prepare(f);
    for (unsigned i=0;i<op_count;++i) {
        DrawOp *o=&ops[i];unsigned w,h;dimensions(o,&w,&h);
        if (x<o->x || x>=o->x+w) continue;
        unsigned begin=o->y>first ? o->y : first;
        unsigned limit=o->y+h<end ? o->y+h : end;
        if (o->kind==DRAW_THEME_BACKGROUND) {
            clock_theme_background_column(o->a,x,begin,limit,shades);continue;
        }
        if (o->kind==DRAW_THEME_GLYPH) {
            uint64_t mask=clock_theme_mask(o->a,o->b,x-o->x);
            for (unsigned y=begin;y<limit;++y)
                if (((mask>>(y-o->y))&1u) && (!(o->b&1024u) || ((x/2u+y/2u)%2u)==0u)) shades[y]=o->shade;
            continue;
        }
        for (unsigned y=begin;y<limit;++y)
            if (ink(o,x-o->x,y-o->y)) shades[y]=o->shade;
    }
    if (f->inverted) for (unsigned y=first;y<end;++y) shades[y]=3u-shades[y];
}
void clock_render_column(const ClockFace *f,unsigned x,uint8_t shades[152])
{ clock_render_column_span(f,x,0,UI_HEIGHT,shades); }

void clock_render_row_span(const ClockFace *f,unsigned y,unsigned first_byte,
                           unsigned byte_count,uint8_t *row)
{
    if (!row || first_byte>=PANEL_ROW_BYTES || byte_count>PANEL_ROW_BYTES-first_byte) return;
    uint8_t background=f && y<PANEL_HEIGHT && f->inverted ? 0u : 0xffu;
    for (unsigned i=0;i<byte_count;++i) row[i]=background;
    if (!f || y>=PANEL_HEIGHT || !byte_count) return;
    prepare(f);
    unsigned x=UI_WIDTH-1u-y, first=first_byte*8u, end=(first_byte+byte_count)*8u;
    uint8_t tones[4];
    /* A four-pixel Bayer column repeats twice per byte, including in dark mode. */
    for (unsigned shade=0;shade<4u;++shade) {
        uint8_t tone=0;
        for (unsigned bit=0;bit<4u;++bit)
            if (!clock_dither(f->inverted ? 3u-shade : shade,x,bit)) tone|=0x80u>>bit;
        tones[shade]=tone|(tone>>4);
    }
    for (unsigned i=0;i<op_count;++i) {
        const DrawOp *o=&ops[i];unsigned w,h;dimensions(o,&w,&h);
        if (x<o->x || x>=o->x+w) continue;
        unsigned begin=o->y>first ? o->y : first;
        unsigned limit=o->y+h<end ? o->y+h : end;
        uint8_t tone=tones[o->shade];
        if (o->kind==DRAW_THEME_BACKGROUND) {
            clock_theme_background_row(o->a,x,first_byte,byte_count,row,f->inverted);
        } else if (o->kind==DRAW_THEME_GLYPH) {
            uint64_t column=clock_theme_mask(o->a,o->b,x-o->x);
            for (unsigned p=begin;p<limit;++p) {
                if (!((column>>(p-o->y))&1u) || ((o->b&1024u) && ((x/2u+p/2u)%2u)!=0u)) continue;
                unsigned b=p/8u-first_byte;
                uint8_t mask=0x80u>>(p%8u);
                row[b]=(row[b]&~mask)|(tone&mask);
            }
        } else if (o->kind==DRAW_RECT) {
            /* Opaque rectangles replace byte spans; later glyphs remain transparent. */
            for (unsigned p=begin;p<limit;) {
                unsigned stop=(p|7u)+1u;
                if (stop>limit) stop=limit;
                uint8_t mask=(0xffu>>(p&7u)) & (0xffu<<(7u-((stop-1u)&7u)));
                unsigned b=p/8u-first_byte;
                row[b]=(row[b]&~mask)|(tone&mask);
                p=stop;
            }
        } else {
            for (unsigned p=begin;p<limit;++p) {
                if (!ink(o,x-o->x,p-o->y)) continue;
                unsigned b=p/8u-first_byte;
                uint8_t mask=0x80u>>(p&7u);
                row[b]=(row[b]&~mask)|(tone&mask);
            }
        }
    }
}
