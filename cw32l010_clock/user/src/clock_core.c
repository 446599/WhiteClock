#include "clock_core.h"

static bool decode_byte(uint8_t bcd, uint8_t limit, uint8_t *value)
{
    if ((bcd & 15u) > 9u || (bcd >> 4) > 9u) return false;
    *value = (uint8_t)((bcd >> 4) * 10u + (bcd & 15u));
    return *value <= limit;
}
bool clock_decode(uint32_t bcd, uint8_t *h, uint8_t *m, uint8_t *s)
{
    return (bcd & 0xffc08080u) == 0 && decode_byte(bcd >> 16,23,h) &&
        decode_byte(bcd >> 8,59,m) && decode_byte(bcd,59,s);
}
static uint32_t encode_byte(uint8_t n) { return ((n / 10u) << 4) | (n % 10u); }
uint32_t clock_encode(uint8_t h, uint8_t m, uint8_t s)
{
    return (encode_byte(h) << 16) | (encode_byte(m) << 8) | encode_byte(s);
}
uint8_t clock_month_days(uint8_t year, uint8_t month)
{
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12 || year > 99) return 0;
    return days[month-1] + (month == 2 && year % 4 == 0); /* 2000..2099 */
}
bool clock_time_valid(const ClockTime *t)
{
    return t->year <= 99 && t->month >= 1 && t->month <= 12 && t->day >= 1 &&
        t->day <= clock_month_days(t->year,t->month) && t->hour < 24 && t->minute < 60 && t->second < 60;
}
bool clock_date_decode(uint32_t bcd, ClockTime *t)
{
    ClockTime value = *t;
    if ((bcd & 0xf800e0c0u) || (bcd >> 24) > 6 ||
        !decode_byte(bcd >> 16,99,&value.year) || !decode_byte(bcd >> 8,12,&value.month) ||
        !decode_byte(bcd,31,&value.day) || !clock_time_valid(&value)) return false;
    *t = value;
    return true;
}
uint8_t clock_weekday(const ClockTime *t)
{
    static const uint8_t offsets[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (!clock_time_valid(t)) return 0;
    unsigned year = 2000u + t->year;
    if (t->month < 3) --year;
    return (year + year/4u - year/100u + year/400u + offsets[t->month-1] + t->day) % 7u;
}
uint32_t clock_date_encode(const ClockTime *t)
{
    return ((uint32_t)clock_weekday(t) << 24) | (encode_byte(t->year) << 16) |
        (encode_byte(t->month) << 8) | encode_byte(t->day);
}
uint8_t clock_display_hour(uint8_t h, bool hour12) { return hour12 ? (h%12u ? h%12u : 12u) : h; }
ClockSettings clock_defaults(void) { return (ClockSettings){0,0,0,22,0,7,0}; }
bool clock_settings_valid(const ClockSettings *s)
{
    return s->theme < THEME_COUNT && s->hour12 < 2 && s->invert_enabled < 2 &&
        s->start_hour < 24 && s->end_hour < 24 && s->start_minute < 60 && s->end_minute < 60;
}
bool clock_inverted(const ClockSettings *s, const ClockTime *t)
{
    if (!s->invert_enabled) return false;
    unsigned now = t->hour*60u+t->minute, start=s->start_hour*60u+s->start_minute, end=s->end_hour*60u+s->end_minute;
    if (start == end) return true; /* explicitly shown as 全天反色 */
    return start < end ? now >= start && now < end : now >= start || now < end;
}
uint8_t clock_button_sample(ClockButton *b, bool pressed, bool ok, uint32_t now)
{
    if (b->raw != pressed) { b->raw = pressed; b->changed_at = now; }
    if (b->stable != b->raw && now - b->changed_at >= 20u) {
        b->stable = b->raw;
        if (b->stable) {
            b->pressed_at = b->repeated_at = now;
            b->long_sent = false;
            if (ok) return KEY_OK;
        } else if (!ok && !b->long_sent) return KEY_NEXT;
    }
    if (!b->stable || !b->raw) return KEY_NONE;
    if (ok && now-b->pressed_at >= 500u && now-b->repeated_at >= 150u) {
        b->repeated_at = now;
        return KEY_REPEAT;
    }
    if (!ok && !b->long_sent && now-b->pressed_at >= 700u) {
        b->long_sent = true;
        return KEY_BACK;
    }
    return KEY_NONE;
}
void clock_ui_init(ClockUI *u, const ClockTime *t, const ClockSettings *s)
{
    u->live = u->draft = *t;
    u->settings = u->draft_settings = *s;
    u->page = PAGE_HOME; u->cursor = u->error = 0;
}
static void clamp_day(ClockTime *t)
{
    uint8_t max=clock_month_days(t->year,t->month);
    if (t->day > max) t->day=max;
}
uint8_t clock_ui_key(ClockUI *u, uint8_t key)
{
    if (!key) return ACTION_NONE;
    if (key == KEY_BACK) {
        u->cursor=0; u->error=0;
        if (u->page == PAGE_HOME) u->page=PAGE_MENU;
        else u->page=u->page==PAGE_MENU ? PAGE_HOME : PAGE_MENU;
        return ACTION_NONE;
    }
    if (u->page == PAGE_HOME) {
        if (key==KEY_NEXT) { u->page=PAGE_MENU; u->cursor=0; }
        if (key==KEY_OK) { u->settings.theme=(u->settings.theme+1u)%THEME_COUNT; return ACTION_SAVE_SETTINGS; }
    } else if (u->page == PAGE_MENU) {
        if (key==KEY_NEXT) u->cursor=(u->cursor+1u)%6u;
        if (key==KEY_OK) {
            if (u->cursor==5) { u->page=PAGE_HOME; u->cursor=0; return ACTION_SAVE_SETTINGS; }
            static const uint8_t pages[]={PAGE_TIME,PAGE_DATE,PAGE_FORMAT,PAGE_INVERT,PAGE_THEME};
            u->page=pages[u->cursor];
            u->draft=u->live; u->draft_settings=u->settings;
            u->cursor=u->page==PAGE_FORMAT ? u->settings.hour12 : u->page==PAGE_THEME ? u->settings.theme : 0;
            u->error=0;
        }
    } else if (u->page==PAGE_FORMAT || u->page==PAGE_THEME) {
        if (key==KEY_NEXT) u->cursor=(u->cursor+1u)%(u->page==PAGE_THEME ? THEME_COUNT : 2u);
        if (key==KEY_OK) {
            if (u->page==PAGE_THEME) u->settings.theme=u->cursor;
            else u->settings.hour12=u->cursor;
            u->page=PAGE_MENU; u->cursor=0;
            return ACTION_SAVE_SETTINGS;
        }
    } else {
        unsigned fields=u->page==PAGE_TIME ? 2u : u->page==PAGE_DATE ? 3u : 5u;
        if (key==KEY_NEXT) u->cursor=(u->cursor+1u)%(fields+1u);
        if (key==KEY_OK || key==KEY_REPEAT) {
            if (u->cursor==fields) {
                if (key==KEY_REPEAT) return ACTION_NONE;
                if (u->page==PAGE_INVERT) {
                    u->settings=u->draft_settings; u->page=PAGE_MENU; u->cursor=0;
                    return ACTION_SAVE_SETTINGS;
                }
                return ACTION_SET_CLOCK;
            }
            if (u->page==PAGE_TIME) {
                if (u->cursor==0) u->draft.hour=(u->draft.hour+1u)%24u;
                else u->draft.minute=(u->draft.minute+1u)%60u;
            } else if (u->page==PAGE_DATE) {
                if (u->cursor==0) u->draft.year=(u->draft.year+1u)%100u;
                if (u->cursor==1) u->draft.month=u->draft.month%12u+1u;
                if (u->cursor==2) u->draft.day=u->draft.day%clock_month_days(u->draft.year,u->draft.month)+1u;
                clamp_day(&u->draft);
            } else {
                ClockSettings *s=&u->draft_settings;
                if (u->cursor==0 && key==KEY_OK) s->invert_enabled=!s->invert_enabled;
                if (u->cursor==1) s->start_hour=(s->start_hour+1u)%24u;
                if (u->cursor==2) s->start_minute=(s->start_minute+1u)%60u;
                if (u->cursor==3) s->end_hour=(s->end_hour+1u)%24u;
                if (u->cursor==4) s->end_minute=(s->end_minute+1u)%60u;
            }
        }
    }
    return ACTION_NONE;
}
void clock_ui_saved(ClockUI *u, bool success)
{
    u->error=success ? 0 : 2;
    if (success) { u->page=PAGE_MENU; u->cursor=0; }
}
ClockFace clock_ui_face(const ClockUI *u)
{
    ClockFace f;
    f.time=(u->page==PAGE_TIME || u->page==PAGE_DATE) ? u->draft : u->live;
    f.time.second=0;
    f.settings=u->page==PAGE_INVERT ? u->draft_settings : u->settings;
    if (u->page==PAGE_THEME) f.settings.theme=u->cursor;
    f.page=u->page; f.cursor=u->cursor; f.error=u->error;
    f.inverted=clock_inverted(&u->settings,&u->live);
    return f;
}
bool clock_face_equal(const ClockFace *a, const ClockFace *b)
{
    /* These records contain only byte fields; no padding or uninitialized bytes. */
    const uint8_t *aa=(const uint8_t *)a, *bb=(const uint8_t *)b;
    for (unsigned i=0;i<sizeof *a;++i) if (aa[i]!=bb[i]) return false;
    return true;
}
bool clock_dither(uint8_t shade, unsigned x, unsigned y)
{
    /* 3x3 dispersed pattern gives exact 0, 1/3, 2/3, 1 coverage. */
    return shade<3u && (shade==0u || ((x+2u*y)%3u) >= shade);
}
void clock_render_row(const ClockFace *f, unsigned y, uint8_t row[PANEL_ROW_BYTES])
{
    uint8_t shades[152];
    clock_render_column(f,y<PANEL_HEIGHT?295u-y:296u,shades);
    for (unsigned i=0;i<PANEL_ROW_BYTES;++i) {
        uint8_t byte=0xff;
        for (unsigned bit=0;bit<8u;++bit) {
            unsigned x=i*8u+bit;
            /* Rotate 180 degrees from v1, matching the user's actual mounting. */
            if (f && y<PANEL_HEIGHT && clock_dither(shades[x],295u-y,x))
                byte&=(uint8_t)~(0x80u>>bit);
        }
        row[i]=byte;
    }
}
