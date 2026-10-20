#ifndef CLOCK_CORE_H
#define CLOCK_CORE_H
#include <stdint.h>
#include <stdbool.h>
#define PANEL_WIDTH 152u
#define PANEL_HEIGHT 296u
#define PANEL_ROW_BYTES 19u
#define UI_WIDTH 296u
#define UI_HEIGHT 152u
#define THEME_COUNT 7u
#define MENU_ITEM_COUNT 6u
#define FOCUS_ALERT_FLASHES 5u

typedef struct { uint8_t year, month, day, hour, minute, second; } ClockTime;
typedef struct {
    uint8_t theme, hour12, invert_enabled, start_hour, start_minute, end_hour, end_minute;
} ClockSettings;
enum { PAGE_HOME, PAGE_MENU, PAGE_TIME, PAGE_DATE, PAGE_FORMAT, PAGE_INVERT, PAGE_THEME, PAGE_FOCUS };
enum { KEY_NONE, KEY_NEXT, KEY_BACK, KEY_OK, KEY_REPEAT };
enum { FOCUS_IDLE, FOCUS_RUNNING, FOCUS_PAUSED, FOCUS_DONE };
enum { ACTION_NONE, ACTION_SET_CLOCK, ACTION_SAVE_SETTINGS };
typedef struct {
    ClockTime time;
    ClockSettings settings;
    uint8_t page, cursor, error, inverted;
    int8_t temperature_c;
    uint8_t temperature_valid, focus_minutes, focus_state, focus_progress;
} ClockFace;
typedef struct {
    uint32_t changed_at, pressed_at, repeated_at;
    bool raw, stable, long_sent;
} ClockButton;
typedef struct {
    ClockTime live, draft;
    ClockSettings settings, draft_settings;
    uint8_t page, cursor, error;
    int8_t temperature_c;
    uint8_t temperature_valid, focus_state, focus_preset, focus_return_page, focus_alert_remaining;
    uint32_t focus_remaining_ms, focus_tick;
} ClockUI;

bool clock_decode(uint32_t bcd, uint8_t *h, uint8_t *m, uint8_t *s);
uint32_t clock_encode(uint8_t h, uint8_t m, uint8_t s);
bool clock_date_decode(uint32_t bcd, ClockTime *time);
uint32_t clock_date_encode(const ClockTime *time);
bool clock_time_valid(const ClockTime *time);
uint8_t clock_month_days(uint8_t year, uint8_t month);
uint8_t clock_weekday(const ClockTime *time); /* Sunday = 0 */
uint8_t clock_display_hour(uint8_t hour, bool hour12);
bool clock_inverted(const ClockSettings *settings, const ClockTime *time);
ClockSettings clock_defaults(void);
bool clock_settings_valid(const ClockSettings *settings);
/* SET uses is_set=true (press confirms); BACK uses false. */
uint8_t clock_button_sample(ClockButton *button, bool pressed, bool is_set, uint32_t now);
void clock_ui_init(ClockUI *ui, const ClockTime *time, const ClockSettings *settings);
uint8_t clock_ui_key(ClockUI *ui, uint8_t key);
void clock_ui_saved(ClockUI *ui, bool success);
ClockFace clock_ui_face(const ClockUI *ui);
void clock_ui_tick(ClockUI *ui, uint32_t now);
bool clock_face_equal(const ClockFace *a, const ClockFace *b);
/* Four luminance levels: 0 black, 1 dark gray, 2 light gray, 3 white. */
uint8_t clock_shade(const ClockFace *face, unsigned x, unsigned y);
void clock_render_column(const ClockFace *face, unsigned x, uint8_t shades[152]);
void clock_render_column_span(const ClockFace *face,unsigned x,unsigned first,
                              unsigned end,uint8_t shades[152]);
bool clock_dither(uint8_t shade, unsigned x, unsigned y);
void clock_render_row(const ClockFace *face, unsigned y, uint8_t row[PANEL_ROW_BYTES]);
/* Inclusive physical panel coordinates; x is aligned to complete bytes. */
typedef struct { uint16_t x0,x1,y0,y1; } ClockWindow;
bool clock_changed_window(const ClockFace *old,const ClockFace *next,ClockWindow *window);
void clock_render_row_span(const ClockFace *face,unsigned y,unsigned first_byte,
                           unsigned byte_count,uint8_t *row);
#endif
