#include "clock_theme.h"
#include <assert.h>
#include <stdio.h>

static unsigned count,maximum,cases;
static void record(unsigned x,unsigned y,unsigned a,unsigned b,unsigned kind,unsigned shade)
{
    unsigned w,h;
    if (kind==DRAW_THEME_GLYPH) {
        clock_theme_dimensions(a,b,&w,&h);assert(h<64u);
    } else if (kind==DRAW_THEME_BACKGROUND) { w=296;h=152; }
    else { assert(kind==0u);w=a;h=b; }
    if (!(x+w<=296u && y+h<=152u))
        fprintf(stderr,"out of bounds: kind=%u x=%u y=%u w=%u h=%u\n",kind,x,y,w,h);
    assert(shade==0u || shade==3u);
    assert(x+w<=296u && y+h<=152u);
    ++count;
}
static void native_text(unsigned x,unsigned y,const char *s,unsigned shade)
{
    (void)shade;
    unsigned chars=0;
    for (const unsigned char *p=(const unsigned char *)s;*p;++p) if ((*p&192u)!=128u) ++chars;
    assert(x+chars*16u<=296u && y+16u<=152u);
    count+=chars;
}
int main(void)
{
    ClockFace face={0};face.temperature_valid=1;
    static const int temperatures[]={-128,-40,-9,0,26,99,127};
    static const unsigned hours[]={0,1,8,12,14,23},minutes[]={0,8,36,59};
    for (unsigned theme=0;theme<THEME_COUNT;++theme)
        for (unsigned format=0;format<2;++format)
            for (unsigned month=1;month<=12;++month)
                for (unsigned day=1;day<=clock_month_days(24,month);day+=7)
                    for (unsigned h=0;h<sizeof hours/sizeof hours[0];++h)
                        for (unsigned m=0;m<sizeof minutes/sizeof minutes[0];++m)
                            for (unsigned t=0;t<sizeof temperatures/sizeof temperatures[0];++t) {
                                face.time=(ClockTime){24,month,day,hours[h],minutes[m],0};
                                face.settings.theme=theme;face.settings.hour12=format;
                                face.temperature_c=temperatures[t];count=0;
                                clock_theme_prepare(&face,record,native_text);
                                assert(count<=86u); /* Room for selection and error overlays. */
                                if (count>maximum) maximum=count;
                                ++cases;
                            }
    printf("PASS: %u live theme layouts fit 296x152; max %u draw operations; 12/24h, all weekdays/months, signed temperature\n",cases,maximum);
}
