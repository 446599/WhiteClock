#include "clock_core.h"
#include <stdio.h>
#include <stdlib.h>

/* Host-only image buffer. Production rendering still needs just one 19-byte row. */
static uint8_t pixels[UI_HEIGHT][UI_WIDTH];
static void output(const char *path,ClockFace *f,int dither)
{
    FILE *out=fopen(path,"wb");if(!out)exit(1);
    fprintf(out,"P5\n296 152\n255\n");
    if (dither) {
        /* Decode the very same row bytes sent to the EPD, before wire-polarity XOR. */
        for (unsigned r=0;r<PANEL_HEIGHT;++r) {
            uint8_t row[PANEL_ROW_BYTES];clock_render_row(f,r,row);
            for (unsigned y=0;y<UI_HEIGHT;++y)
                pixels[y][UI_WIDTH-1u-r]=(row[y/8]&(0x80u>>(y%8)))?255:0;
        }
    } else {
        /* Reference only: average densities, NOT native panel grayscale. */
        static const uint8_t luminance[]={0,32,223,255};
        for (unsigned y=0;y<UI_HEIGHT;++y) for (unsigned x=0;x<UI_WIDTH;++x)
            pixels[y][x]=luminance[clock_shade(f,x,y)];
    }
    if (fwrite(pixels,1,sizeof pixels,out)!=sizeof pixels || fclose(out)) exit(1);
}
int main(void)
{
    ClockTime t={26,10,4,14,36,0};ClockSettings s=clock_defaults();
    ClockUI ui;clock_ui_init(&ui,&t,&s);ui.temperature_c=26;ui.temperature_valid=1;char path[100];
    for(unsigned theme=0;theme<3;++theme){
        ui.settings.theme=theme;
        ClockFace f=clock_ui_face(&ui);
        snprintf(path,sizeof path,"design/theme-%u.pgm",theme+1);output(path,&f,0);
        snprintf(path,sizeof path,"design/theme-%u-dither.pgm",theme+1);output(path,&f,1);
        f.inverted=1;snprintf(path,sizeof path,"design/theme-%u-night.pgm",theme+1);output(path,&f,0);
        snprintf(path,sizeof path,"design/theme-%u-night-dither.pgm",theme+1);output(path,&f,1);
    }
    for(unsigned page=1;page<=PAGE_FOCUS;++page){
        ui.page=page;ui.cursor=page==PAGE_INVERT?2:0;ui.draft_settings=ui.settings;
        ClockFace f=clock_ui_face(&ui);
        snprintf(path,sizeof path,"design/menu-%u.pgm",page);output(path,&f,1);
    }
    ui.page=PAGE_FOCUS;ui.focus_state=FOCUS_PAUSED;
    ClockFace paused=clock_ui_face(&ui);output("design/focus-paused.pgm",&paused,1);
    ui.focus_state=FOCUS_DONE;ui.focus_remaining_ms=0;
    ClockFace done=clock_ui_face(&ui);output("design/focus-done.pgm",&done,1);
    ui.page=PAGE_HOME;ui.settings.hour12=1;ui.settings.theme=0;
    ClockFace f=clock_ui_face(&ui);output("design/12-hour.pgm",&f,1);
    ui.settings.theme=1;ui.focus_state=FOCUS_IDLE;ui.live.hour=0;ui.live.minute=5;
    f=clock_ui_face(&ui);output("design/flip-midnight.pgm",&f,1);
    ui.live.hour=12;ui.live.minute=0;ui.temperature_c=-8;
    f=clock_ui_face(&ui);output("design/flip-noon-negative.pgm",&f,1);
    ui.live.hour=8;ui.live.minute=9;ui.temperature_valid=0;ui.settings.hour12=0;
    f=clock_ui_face(&ui);output("design/flip-unavailable.pgm",&f,1);
    ui.live.hour=18;ui.live.minute=58;ui.temperature_valid=1;ui.temperature_c=26;
    ui.focus_state=FOCUS_PAUSED;ui.focus_remaining_ms=15u*60000u;
    f=clock_ui_face(&ui);output("design/flip-focus-paused.pgm",&f,1);
    ui.page=PAGE_THEME;ui.cursor=1;
    f=clock_ui_face(&ui);output("design/flip-selector.pgm",&f,1);
}
