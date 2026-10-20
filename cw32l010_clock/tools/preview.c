#include "clock_core.h"
#include <stdio.h>
#include <stdlib.h>
static void output(const char *path,ClockFace *f,int dither)
{
    FILE *out=fopen(path,"wb");if(!out)exit(1);
    fprintf(out,"P5\n296 152\n255\n");
    for(unsigned y=0;y<152;++y)for(unsigned x=0;x<296;++x){
        unsigned v=clock_shade(f,x,y);
        fputc(dither?(clock_dither(v,x,y)?0:255):v*85,out);
    }
    fclose(out);
}
int main(int argc,char **argv)
{
    const char *dir=argc>1 ? argv[1] : "design";
    ClockTime t={26,10,20,14,36,0};ClockSettings s=clock_defaults();
    ClockUI ui;clock_ui_init(&ui,&t,&s);ui.temperature_c=26;ui.temperature_valid=1;char path[1024];
    for(unsigned theme=0;theme<THEME_COUNT;++theme){
        ui.settings.theme=theme;
        ClockFace f=clock_ui_face(&ui);
        snprintf(path,sizeof path,"%s/theme-%u.pgm",dir,theme+1);output(path,&f,0);
        snprintf(path,sizeof path,"%s/theme-%u-dither.pgm",dir,theme+1);output(path,&f,1);
        f.inverted=1;snprintf(path,sizeof path,"%s/theme-%u-night.pgm",dir,theme+1);output(path,&f,0);
        ui.page=PAGE_THEME;ui.cursor=theme;
        f=clock_ui_face(&ui);
        snprintf(path,sizeof path,"%s/theme-%u-select.pgm",dir,theme+1);output(path,&f,0);
        ui.page=PAGE_HOME;
    }
    for(unsigned page=1;page<=PAGE_FOCUS;++page){
        ui.page=page;ui.cursor=page==PAGE_INVERT?2:0;ui.draft_settings=ui.settings;
        ClockFace f=clock_ui_face(&ui);
        snprintf(path,sizeof path,"%s/menu-%u.pgm",dir,page);output(path,&f,0);
    }
    ui.page=PAGE_MENU;ui.cursor=MENU_ITEM_COUNT-1u;
    ClockFace last=clock_ui_face(&ui);
    snprintf(path,sizeof path,"%s/menu-last.pgm",dir);output(path,&last,0);
    last.inverted=!last.inverted;
    snprintf(path,sizeof path,"%s/menu-last-night.pgm",dir);output(path,&last,0);
    ui.page=PAGE_FOCUS;ui.focus_state=FOCUS_PAUSED;
    ClockFace paused=clock_ui_face(&ui);
    snprintf(path,sizeof path,"%s/focus-paused.pgm",dir);output(path,&paused,0);
    ui.focus_state=FOCUS_DONE;ui.focus_remaining_ms=0;
    ClockFace done=clock_ui_face(&ui);
    snprintf(path,sizeof path,"%s/focus-done.pgm",dir);output(path,&done,0);
    ui.page=PAGE_HOME;ui.settings.hour12=1;ui.settings.theme=0;
    ClockFace f=clock_ui_face(&ui);
    snprintf(path,sizeof path,"%s/12-hour.pgm",dir);output(path,&f,0);
}
