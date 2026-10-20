#include "clock_core.h"
#include "ui_font.h"
#include <assert.h>
#include <stdio.h>

static uint16_t codepoint(const unsigned char **p)
{
    const unsigned char *s=*p;
    if (*s<128u) { *p=s+1; return *s; }
    if (*s<224u) { *p=s+2; return ((s[0]&31u)<<6)|(s[1]&63u); }
    *p=s+3; return ((s[0]&15u)<<12)|((s[1]&63u)<<6)|(s[2]&63u);
}
static unsigned width(const char *label)
{
    const unsigned char *p=(const unsigned char *)label;
    unsigned n=0;
    while (*p) n+=codepoint(&p)<256u ? 8u : 16u;
    return n;
}
static void check_label(ClockFace *face,const char *label,unsigned y,bool centered)
{
    unsigned w=width(label);
    assert(w<=272u && y+16u<=152u);
    unsigned start=centered ? (296u-w)/2u : 92u;
    assert(start+w<=296u);
    for (unsigned row=0;row<16u;++row) {
        uint8_t expected[296]={0};
        const unsigned char *p=(const unsigned char *)label;
        unsigned x=start;
        while (*p) {
            uint16_t c=codepoint(&p);
            const SmallGlyph *glyph=0;
            for (unsigned i=0;i<SMALL_GLYPH_COUNT;++i)
                if (small_glyphs[i].code==c) { glyph=&small_glyphs[i]; break; }
            assert(glyph);
            for (unsigned bit=0;bit<glyph->width;++bit)
                expected[x++]=(glyph->rows[row]>>(15u-bit))&1u;
        }
        for (unsigned col=centered ? 0u : 80u;col<296u;++col) {
            unsigned shade=expected[col] ? 0u : 3u;
            if (face->inverted) shade=3u-shade;
            if (clock_shade(face,col,y+row)!=shade)
                fprintf(stderr,"page=%u theme=%u inverted=%u x=%u y=%u label=%s expected=%u actual=%u\n",
                    face->page,face->settings.theme,face->inverted,col,y+row,label,
                    shade,clock_shade(face,col,y+row));
            assert(clock_shade(face,col,y+row)==shade);
        }
    }
}
static void check_home_hint(ClockFace *face)
{
    /* The collage has its own live outline-font footer; other themes retain
       the native SET/BACK bitmap, including the newspaper's right alignment. */
    if (face->settings.theme==1u) return;
    const unsigned char *p=(const unsigned char *)(face->settings.theme==2u ? "SET/BACK" : "SET / BACK");
    unsigned x=face->settings.theme==2u ? 220u : 108u;
    while (*p) {
        unsigned code=*p++;
        const SmallGlyph *glyph=0;
        for (unsigned i=0;i<SMALL_GLYPH_COUNT;++i)
            if (small_glyphs[i].code==code) { glyph=&small_glyphs[i];break; }
        assert(glyph);
        for (unsigned row=0;row<16u;++row)
            for (unsigned bit=0;bit<8u;++bit)
                if (glyph->rows[row]&(0x8000u>>bit))
                    assert(clock_shade(face,x+bit,133u+row)==(face->inverted ? 3u : 0u));
        x+=8u;
    }
}
static void check_choice(ClockFace *face,const char *label,unsigned y,bool selected)
{
    static const uint8_t dot[]={0x3c,0x7e,0xff,0xff,0xff,0xff,0x7e,0x3c};
    assert(30u+width(label)<=284u);
    for (unsigned row=0;row<21u;++row) {
        uint8_t expected[296]={0};
        if (row>=2u && row<18u) {
            const unsigned char *p=(const unsigned char *)label;
            unsigned x=30;
            while (*p) {
                uint16_t c=codepoint(&p);
                const SmallGlyph *glyph=0;
                for (unsigned i=0;i<SMALL_GLYPH_COUNT;++i)
                    if (small_glyphs[i].code==c) { glyph=&small_glyphs[i];break; }
                assert(glyph);
                for (unsigned bit=0;bit<glyph->width;++bit)
                    expected[x++]=(glyph->rows[row-2u]>>(15u-bit))&1u;
            }
        }
        if (selected && row>=6u && row<14u)
            for (unsigned bit=0;bit<8u;++bit) expected[14u+bit]=(dot[row-6u]>>(7u-bit))&1u;
        for (unsigned x=12;x<284u;++x) {
            unsigned shade=expected[x] ? 0u : 3u;
            if (face->inverted) shade=3u-shade;
            assert(clock_shade(face,x,y-2u+row)==shade);
        }
    }
}
static void check_menu_selection(ClockFace *face)
{
    static const char *items[]={"时间设置","日期设置","时间制式","定时反色","显示主题","专注计时","WhiteClock V1.0"};
    face->page=PAGE_MENU;
    for (unsigned cursor=0;cursor<MENU_ITEM_COUNT;++cursor) {
        face->cursor=cursor;
        unsigned first=cursor<4u ? 0u : 3u;
        for (unsigned i=first;i<first+4u;++i)
            check_choice(face,items[i],39u+(i-first)*21u,i==cursor);
    }
    face->page=PAGE_FORMAT;
    for (unsigned cursor=0;cursor<2u;++cursor) {
        face->cursor=cursor;
        check_choice(face,"24小时制",46,cursor==0u);
        check_choice(face,"12小时制  上午/下午",79,cursor==1u);
    }
    face->page=PAGE_TIME;face->cursor=2;check_choice(face,"保存时间",98,true);
    face->page=PAGE_DATE;face->cursor=3;check_choice(face,"保存日期",99,true);
    face->page=PAGE_INVERT;face->cursor=5;check_choice(face,"保存设置",99,true);
    face->page=PAGE_MENU;face->cursor=0;
    ClockFace next=*face;next.cursor=1;
    ClockWindow window;
    assert(clock_changed_window(face,&next,&window));
    assert(window.y0==274u && window.y1==281u);
    for (unsigned x=0;x<296u;++x)
        for (unsigned y=0;y<152u;++y)
            if (x<14u || x>=22u || !((y>=43u && y<51u)||(y>=64u && y<72u)))
                assert(clock_shade(face,x,y)==clock_shade(&next,x,y));
}
int main(void)
{
    ClockUI ui;
    ClockTime time={26,10,20,12,34,0};
    ClockSettings settings=clock_defaults();
    clock_ui_init(&ui,&time,&settings);
    ClockFace face=clock_ui_face(&ui);
    for (unsigned inverted=0;inverted<2u;++inverted)
        for (unsigned theme=0;theme<THEME_COUNT;++theme) {
            face.inverted=inverted;face.settings.theme=theme;
            check_menu_selection(&face);
            face.page=PAGE_HOME;
            check_home_hint(&face);
            face.page=PAGE_THEME;
            check_label(&face,"SET保存 BACK切换",130,false);
            face.page=PAGE_MENU;
            for (unsigned cursor=0;cursor<MENU_ITEM_COUNT;++cursor) {
                face.cursor=cursor;
                check_label(&face,"SET确认  BACK下一项  长按BACK返回",131,true);
            }
            face.page=PAGE_TIME;
            check_label(&face,"SET修改  BACK下一项  长按BACK取消",131,true);
            face.page=PAGE_DATE;
            check_label(&face,"SET修改  BACK下一项  长按BACK取消",131,true);
            face.page=PAGE_INVERT;
            check_label(&face,"SET修改  BACK下一项  长按BACK取消",131,true);
            face.page=PAGE_FORMAT;
            check_label(&face,"SET保存  BACK切换  长按BACK取消",131,true);
            face.page=PAGE_FOCUS;
            face.focus_minutes=25;
            face.focus_state=FOCUS_IDLE;
            check_label(&face,"SET开始 BACK时长 长按BACK返回",133,true);
            face.focus_state=FOCUS_RUNNING;
            check_label(&face,"SET暂停 BACK重置 长按BACK返回",133,true);
            face.focus_state=FOCUS_PAUSED;
            check_label(&face,"SET继续 BACK重置 长按BACK返回",133,true);
            face.focus_state=FOCUS_DONE;
            check_label(&face,"SET确认完成 BACK时长 长按BACK返回",133,true);
        }
    puts("PASS: dot-only menu selection, unchanged text, seven-theme SET/BACK hints and labels in both polarities");
}
