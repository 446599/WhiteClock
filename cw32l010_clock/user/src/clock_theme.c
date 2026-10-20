#include "clock_theme.h"
#ifdef CLOCK_THEME_TEST
#include <assert.h>
#endif

typedef struct {
    const uint8_t *metrics;
    uint8_t image,count,cell_w,cell_h,columns;
} ThemeFont;
#include "theme_assets.h"

static uint8_t decoded[2][THEME_DECODE_BYTES];
static const uint8_t *cached_image[2];
static unsigned cached_block[2];
static ThemeDraw emit;
#ifdef CLOCK_THEME_PROFILE
ClockThemeProfile clock_theme_profile;
void clock_theme_profile_reset(void)
{
    clock_theme_profile=(ClockThemeProfile){0};cached_image[0]=cached_image[1]=0;
}
#define PROFILE(field) (++clock_theme_profile.field)
#else
#define PROFILE(field) ((void)0)
#endif

static unsigned u16(const uint8_t *p) { return p[0]|((unsigned)p[1]<<8); }
static int s16(const uint8_t *p) { return (int16_t)u16(p); }
static unsigned block_byte(const uint8_t *p,unsigned offset,unsigned slot)
{
    unsigned block=offset/THEME_DECODE_BYTES;
    if (cached_image[slot]!=p || cached_block[slot]!=block) {
        PROFILE(decoded_blocks);
        unsigned count=u16(p+2),start=u16(p+4+block*2);
        const uint8_t *s=p+4+(count+1)*2+start;
        const uint8_t *end=p+4+(count+1)*2+u16(p+6+block*2);
        unsigned mode=*s++,n=0;
        if (!mode) while (s<end && n<THEME_DECODE_BYTES) decoded[slot][n++]=*s++;
        else {
            while (s<end && n<THEME_DECODE_BYTES) {
                unsigned control=*s++;
                if (control<128u) {
                    unsigned run=control+1u;
                    while (run-- && s<end && n<THEME_DECODE_BYTES) decoded[slot][n++]=*s++;
                } else {
                    unsigned run=257u-control,value=*s++;
                    while (run-- && n<THEME_DECODE_BYTES) decoded[slot][n++]=value;
                }
            }
            unsigned stride=mode-1u;
            if (stride) for (unsigned i=stride;i<n;++i) decoded[slot][i]^=decoded[slot][i-stride];
        }
        cached_image[slot]=p;cached_block[slot]=block;
    }
    return decoded[slot][offset%THEME_DECODE_BYTES];
}
static unsigned raster_byte(const uint8_t *p,unsigned stride,unsigned x,unsigned y,unsigned slot)
{
    if (p[0]==4u) return block_byte(p+5,y*stride+x,slot);
    if (p[0]==7u) return p[5+y*stride+x];
    unsigned count=u16(p+5),tile=(y/8u)*stride+x;
    const uint8_t *map=p+7+count*8u;
    unsigned index=p[0]==5u ? map[tile] : u16(map+tile*2u);
    return p[7+index*8u+y%8u];
}
bool clock_theme_asset_pixel(unsigned image,unsigned x,unsigned y)
{
    PROFILE(pixel_calls);
    if (image>=sizeof theme_images/sizeof theme_images[0]) return false;
    const uint8_t *p=theme_images[image];
    unsigned w=u16(p+1),h=u16(p+3);
    if (x>=w || y>=h) return false;
    unsigned value=raster_byte(p,(h+7u)/8u,y/8u,w-1u-x,image<THEME_BACKGROUND_IMAGES ? 0u : 1u);
    return (value&(0x80u>>(y%8u)))!=0;
}
uint8_t clock_theme_background(unsigned image,unsigned x,unsigned y)
{ return clock_theme_asset_pixel(image,x,y) ? 3u : 0u; }
void clock_theme_background_column(unsigned image,unsigned x,unsigned first,unsigned end,uint8_t *shades)
{
    for (unsigned y=first;y<end;++y) shades[y]=clock_theme_background(image,x,y);
}
void clock_theme_background_row(unsigned image,unsigned x,unsigned first_byte,unsigned count,uint8_t *row,bool inverted)
{
    const uint8_t *p=theme_images[image];
    unsigned stride=(u16(p+3)+7u)/8u,y=u16(p+1)-1u-x;
    for (unsigned i=0;i<count;++i) {
        unsigned value=raster_byte(p,stride,first_byte+i,y,0);
        row[i]=inverted ? value^0xffu : value;
    }
}
static const uint8_t *metric(unsigned font,unsigned index)
{ return theme_fonts[font].metrics+index*10u; }
static unsigned find_glyph(unsigned font,unsigned code)
{
    const ThemeFont *f=&theme_fonts[font];
    for (unsigned i=0;i<f->count;++i) if (u16(metric(font,i))==code) return i;
#ifdef CLOCK_THEME_TEST
    assert(!"Missing live theme glyph");
#endif
    return 0;
}
void clock_theme_dimensions(unsigned font,unsigned glyph,unsigned *w,unsigned *h)
{
    const uint8_t *m=metric(font,glyph&255u);
    unsigned halo=(glyph>>8)&3u;
    *w=m[2]+halo*2u;*h=m[3]+halo*2u;
}
static uint64_t glyph_column(unsigned font,unsigned glyph,int x)
{
    PROFILE(glyph_columns);
    const ThemeFont *f=&theme_fonts[font];
    const uint8_t *m=metric(font,glyph);
    if (x<0 || (unsigned)x>=m[2]) return 0;
    unsigned ax=(glyph%f->columns)*f->cell_w+(unsigned)x;
    unsigned ay=(glyph/f->columns)*f->cell_h;
    const uint8_t *p=theme_images[f->image];
#ifdef CLOCK_THEME_TEST
    assert(!(ay&7u) && m[3]<64u);
#endif
    uint64_t bits=0;
    for (unsigned byte=0;byte<(m[3]+7u)/8u;++byte) {
        unsigned v=raster_byte(p,(u16(p+3)+7u)/8u,ay/8u+byte,u16(p+1)-1u-ax,1);
        v=((v&0x55u)<<1)|((v>>1)&0x55u);
        v=((v&0x33u)<<2)|((v>>2)&0x33u);
        v=((v&0x0fu)<<4)|((v>>4)&0x0fu);
        bits|=(uint64_t)v<<(byte*8u);
    }
    return bits&(((uint64_t)1u<<m[3])-1u);
}
uint64_t clock_theme_mask(unsigned font,unsigned glyph,unsigned x)
{
    unsigned halo=(glyph>>8)&3u;
    uint64_t mask=0;
    for (int dx=-(int)halo;dx<=(int)halo;++dx)
        mask|=glyph_column(font,glyph&255u,(int)x-(int)halo+dx)<<halo;
    uint64_t expanded=mask;
    for (unsigned dy=1;dy<=halo;++dy) expanded|=(mask<<dy)|(mask>>dy);
    return expanded;
}
bool clock_theme_ink(unsigned font,unsigned glyph,unsigned x,unsigned y)
{ return y<64u && ((clock_theme_mask(font,glyph,x)>>y)&1u)!=0; }
static unsigned next_code(const char **s)
{
    const uint8_t *p=(const uint8_t *)*s;
    if (p[0]<128u) { *s+=1;return p[0]; }
    *s+=2;return ((p[0]&31u)<<6)|(p[1]&63u);
}
static int scaled_bearing(unsigned font,const uint8_t *m)
{
    /* Condensed atlases retain source bearings; digits have nonnegative bearings. */
    unsigned scale=100;
    switch (font) {
    case TF_SLANT_69_91:scale=91;break;
    case TF_SERIF_BOLD_66_86:scale=86;break;
    case TF_ITALIC_72_97:scale=97;break;
    case TF_SERIF_69_95:case TF_BOLD_68_95:scale=95;break;
    case TF_BOLD_47_96:scale=96;break;
    default:break;
    }
    return s16(m+4)*(int)scale/100;
}
static void label(unsigned font,const char *value,int anchor,int y,unsigned align,
                  unsigned color,unsigned halo,unsigned pattern)
{
    int left=32767,right=-32767,top=32767;
    unsigned pen=0;
    const char *s=value;
    while (*s) {
        const uint8_t *m=metric(font,find_glyph(font,next_code(&s)));
        int x=(int)((pen+32u)/64u)+scaled_bearing(font,m);
        if (x<left) left=x;
        if (x+(int)m[2]>right) right=x+m[2];
        if (s16(m+6)<top) top=s16(m+6);
        pen+=u16(m+8);
    }
    int origin=anchor-left;
    if (align==1u) origin-=(right-left)/2;
    if (align==2u) origin-=right-left;
    pen=0;s=value;
    while (*s) {
        unsigned glyph=find_glyph(font,next_code(&s));
        const uint8_t *m=metric(font,glyph);
        int x=origin+(int)((pen+32u)/64u)+scaled_bearing(font,m),yy=y+s16(m+6)-top;
        if (halo && x>=(int)halo && yy>=(int)halo)
            emit((unsigned)x-halo,(unsigned)yy-halo,font,glyph|(halo<<8),DRAW_THEME_GLYPH,3u-color);
        if (x>=0 && yy>=0) emit((unsigned)x,(unsigned)yy,font,glyph|pattern,DRAW_THEME_GLYPH,color);
        pen+=u16(m+8);
    }
}
static void copy(char **p,const char *s) { while (*s) *(*p)++=*s++; }
static void two(char *s,unsigned n) { s[0]='0'+n/10u;s[1]='0'+n%10u; }
static void three(char *s,unsigned n) { s[0]='0'+n/100u;two(s+1,n%100u); }
static const char *days[]={"SUNDAY","MONDAY","TUESDAY","WEDNESDAY","THURSDAY","FRIDAY","SATURDAY"};
static const char *months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
void clock_theme_prepare(const ClockFace *f,ThemeDraw draw,ThemeText native_text)
{
    static const uint8_t backgrounds[]={TB_01,TB_03,TB_11,TB_15,TB_16,TB_18,TB_20};
    static const char *weekdays[]={"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};
    emit=draw;
    unsigned theme=f->settings.theme<THEME_COUNT ? f->settings.theme : 0;
    unsigned weekday=clock_weekday(&f->time),month=f->time.month;
    if (month<1u || month>12u) month=1;
    unsigned day_of_year=f->time.day;
    for (unsigned i=1;i<month;++i) day_of_year+=clock_month_days(f->time.year,i);
    char date[]="2000.00.00",time[]="00:00",temp[8],text[64],number[4]="000";
    two(date+2,f->time.year);two(date+5,month);two(date+8,f->time.day);
    two(time,clock_display_hour(f->time.hour,f->settings.hour12));two(time+3,f->time.minute);
    char *p=temp;
    if (f->temperature_valid) {
        int t=f->temperature_c;
        if (t<0) { *p++='-';t=-t; }
        if (t>=100) *p++='1';
        two(p,(unsigned)t%100u);p+=2;
    } else { *p++='-';*p++='-'; }
    copy(&p,"°C");*p=0;
    three(number,day_of_year);
    draw(0,0,backgrounds[theme],0,DRAW_THEME_BACKGROUND,3);
    switch (theme) {
    case 0: {
        char short_day[]={days[weekday][0],days[weekday][1],days[weekday][2],0};
        label(TF_BOLD_12,short_day,280,11,2,0,1,0);
        label(TF_SLANT_69_91,time,151,47,1,0,1,0);
        label(TF_SLANT_69_91,time,148,43,1,0,1,0);
        draw(12,62,30,2,0,0);draw(256,79,27,2,0,0);draw(22,95,13,2,0,0);
        label(TF_BOLD_12,date,14,115,0,0,1,0);
        label(TF_MONO_12,temp,282,115,2,0,1,0);
        break;
    }
    case 1: {
        static const uint8_t fonts[]={TF_SERIF_BOLD_61,TF_BOLD_61,0,TF_ITALIC_61,TF_BOLD_61};
        for (unsigned i=0;i<5;++i) if (i!=2u) {
            char digit[]={time[i],0};
            label(fonts[i],digit,18+(int)i*51,46+(int)(i%2)*3,0,i==1u||i==4u?3u:0u,0,0);
        }
        p=text;copy(&p,date);copy(&p,"  ");copy(&p,temp);copy(&p,"   SET/BACK");*p=0;
        label(TF_MONO_9,text,24,133,0,3,1,0);
        break;
    }
    case 2:
        label(TF_SERIF_10,date,12,40,0,0,0,0);
        p=text;copy(&p,days[weekday]);copy(&p," / NO. ");copy(&p,number);*p=0;
        label(TF_SERIF_10,text,284,40,2,0,0,0);
        label(TF_SERIF_BOLD_66_86,time,13,60,0,0,0,0);
        label(TF_SERIF_BOLD_11,temp,257,73,1,0,0,0);
        p=text;copy(&p,"DAY ");copy(&p,number);copy(&p," / A MOMENT TO KEEP");*p=0;
        label(TF_SERIF_10,text,12,125,0,0,0,0);
        break;
    case 3:
        label(TF_MONO_9,date,12,29,0,0,1,0);
        label(TF_ITALIC_72_97,time,152,51,1,0,0,1024u);
        label(TF_ITALIC_72_97,time,148,46,1,0,2,0);
        p=text;copy(&p,days[weekday]);copy(&p," / ");copy(&p,temp);*p=0;
        label(TF_MONO_10,text,20,118,0,3,1,0);
        break;
    case 4:
        p=text;
        for (unsigned i=0;i<3;++i) { *p++=months[month-1][i];*p++=' '; }
        *p++=' ';*p++='0'+f->time.day/10u;*p++=' ';*p++='0'+f->time.day%10u;*p=0;
        label(TF_SERIF_10,text,148,17,1,0,0,0);
        label(TF_SERIF_69_95,time,148,46,1,0,0,0);
        p=text;
        for (const char *s=days[weekday];*s;++s) { *p++=*s;if (s[1]) *p++=' '; } *p=0;
        label(TF_SERIF_9,text,148,108,1,0,0,0);
        label(TF_SERIF_10,temp,148,123,1,0,0,0);
        break;
    case 5:
        label(TF_BOLD_11,months[month-1],53,15,1,0,1,0);
        two(text,f->time.day);text[2]=0;label(TF_BOLD_48,text,53,49,1,0,1,0);
        for (unsigned i=0;i<4;++i) text[i]=date[i];
        text[4]=0;
        label(TF_MONO_10,text,53,113,1,0,1,0);
        native_text(128,15,weekdays[weekday],0);
        label(TF_BOLD_47_96,time,124,49,0,0,0,0);
        label(TF_MONO_12,temp,128,101,0,0,1,0);
        break;
    default:
        p=text;copy(&p,date);copy(&p,"   ");copy(&p,days[weekday]);*p=0;
        label(TF_MONO_10,text,26,23,0,3,0,0);
        label(TF_BOLD_68_95,time,148,51,1,0,0,0);
        label(TF_MONO_12,temp,272,110,2,0,0,0);
        label(TF_MONO_9,f->settings.hour12 ? (f->time.hour<12 ? "LOCAL / AM" : "LOCAL / PM") : "LOCAL / 24H",23,111,0,0,0,0);
        break;
    }
}
