#include "clock_core.h"
#include "clock_theme.h"
#include <stdio.h>
#include <time.h>
#ifdef CLOCK_THEME_PROFILE_TEST
#include <assert.h>
#endif

static double seconds(void)
{
    struct timespec value;
    timespec_get(&value,TIME_UTC);
    return (double)value.tv_sec+(double)value.tv_nsec/1e9;
}
int main(void)
{
    ClockTime time={26,10,20,14,36,0};
    ClockSettings settings=clock_defaults();
    ClockUI ui;clock_ui_init(&ui,&time,&settings);
    ui.temperature_valid=1;ui.temperature_c=26;
    puts("[");
    for (unsigned theme=0;theme<THEME_COUNT;++theme)
        for (unsigned page=0;page<2u;++page) {
            ui.page=page ? PAGE_THEME : PAGE_HOME;
            ui.cursor=ui.settings.theme=theme;
            ClockFace face=clock_ui_face(&ui);
            clock_theme_profile_reset();
            uint32_t hash=2166136261u;
            double start=seconds();
            for (unsigned y=0;y<PANEL_HEIGHT;++y) {
                uint8_t row[PANEL_ROW_BYTES];clock_render_row(&face,y,row);
                for (unsigned x=0;x<PANEL_ROW_BYTES;++x) hash=(hash^row[x])*16777619u;
            }
#ifdef CLOCK_THEME_PROFILE_TEST
            /* Count work, not host timing: keep the panel path free of scalar decoding. */
            static const uint32_t expected[THEME_COUNT][2]={
                {1886478175u,2125598410u},{182723871u,3860187215u},
                {2966466620u,2313087551u},{595287330u,3533632886u},
                {827365769u,2085343046u},{2981359632u,2124644266u},
                {745383174u,3918995945u}
            };
            assert(hash==expected[theme][page]);
            assert(clock_theme_profile.pixel_calls==0u);
            assert(clock_theme_profile.decoded_blocks<600u);
#endif
            printf("%s{\"theme\":%u,\"page\":%u,\"hash\":%u,\"host_ms\":%.3f,\"decoded_blocks\":%u,\"pixel_calls\":%u,\"glyph_columns\":%u}",
                theme || page ? ",\n" : "",theme,ui.page,hash,(seconds()-start)*1000.0,
                clock_theme_profile.decoded_blocks,clock_theme_profile.pixel_calls,clock_theme_profile.glyph_columns);
        }
    puts("\n]");
    return 0;
}
