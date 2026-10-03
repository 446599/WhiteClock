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
int main(void)
{
    ClockTime t={26,10,4,14,36,0};ClockSettings s=clock_defaults();
    ClockUI ui;clock_ui_init(&ui,&t,&s);char path[100];
    for(unsigned theme=0;theme<3;++theme){
        ui.settings.theme=theme;
        ClockFace f=clock_ui_face(&ui);
        snprintf(path,sizeof path,"design/theme-%u.pgm",theme+1);output(path,&f,0);
        snprintf(path,sizeof path,"design/theme-%u-dither.pgm",theme+1);output(path,&f,1);
        f.inverted=1;snprintf(path,sizeof path,"design/theme-%u-night.pgm",theme+1);output(path,&f,0);
    }
    for(unsigned page=1;page<=6;++page){
        ui.page=page;ui.cursor=page==PAGE_INVERT?2:0;ui.draft_settings=ui.settings;
        ClockFace f=clock_ui_face(&ui);
        snprintf(path,sizeof path,"design/menu-%u.pgm",page);output(path,&f,0);
    }
    ui.page=PAGE_HOME;ui.settings.hour12=1;ui.settings.theme=0;
    ClockFace f=clock_ui_face(&ui);output("design/12-hour.pgm",&f,0);
}
