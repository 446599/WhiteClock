#ifndef CLOCK_THEME_H
#define CLOCK_THEME_H
#include "clock_core.h"

enum { DRAW_THEME_GLYPH = 4, DRAW_THEME_BACKGROUND = 5 };
typedef void (*ThemeDraw)(unsigned x,unsigned y,unsigned a,unsigned b,unsigned kind,unsigned shade);
typedef void (*ThemeText)(unsigned x,unsigned y,const char *s,unsigned shade);
void clock_theme_prepare(const ClockFace *face,ThemeDraw draw,ThemeText native_text);
void clock_theme_dimensions(unsigned font,unsigned glyph,unsigned *w,unsigned *h);
bool clock_theme_ink(unsigned font,unsigned glyph,unsigned x,unsigned y);
uint64_t clock_theme_mask(unsigned font,unsigned glyph,unsigned x);
uint8_t clock_theme_background(unsigned image,unsigned x,unsigned y);
void clock_theme_background_column(unsigned image,unsigned x,unsigned first,unsigned end,uint8_t *shades);
/* Pack a logical column directly into physical panel bytes. */
void clock_theme_background_row(unsigned image,unsigned x,unsigned first_byte,unsigned count,uint8_t *row,bool inverted);
/* Exposed for host-side lossless resource verification. */
bool clock_theme_asset_pixel(unsigned image,unsigned x,unsigned y);
#ifdef CLOCK_THEME_PROFILE
typedef struct { unsigned decoded_blocks,pixel_calls,glyph_columns; } ClockThemeProfile;
extern ClockThemeProfile clock_theme_profile;
void clock_theme_profile_reset(void);
#endif
#endif
