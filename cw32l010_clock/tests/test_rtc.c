#include "main.h"
#include "board.h"
#include "clock_core.h"
#include <assert.h>
#include <stdio.h>

FakeRTC test_rtc;
FakeSYSCTRL test_sysctrl;
FakeGPIO test_gpio;
static uint32_t tick;
static bool grant_window = true;
uint32_t board_millis(void) { return tick; }
void board_delay(uint32_t ms)
{
    tick += ms;
    test_rtc.CR1_f.WINDOW = grant_window && test_rtc.CR1_f.ACCESS;
}
int main(void)
{
    test_sysctrl.LSE_f.STABLE = 1;
    assert(board_rtc_init());
    assert(test_rtc.PSC == 16383 && test_rtc.CR0_f.H24 && test_rtc.CR0_f.START);
    assert(test_rtc.CR1_f.SOURCE == 0 && test_rtc.KEY == 0x55 && test_sysctrl.LSE == 0x0a24);
    uint8_t h,m,s;
    assert(board_rtc_read(&h,&m,&s) && h == 12 && m == 0 && s == 0);
    assert(board_rtc_set(23,59));
    assert(board_rtc_read(&h,&m,&s) && h == 23 && m == 59 && s == 0);
    assert(!board_rtc_set(24,0));
    assert(test_rtc.TIME == 0x235900);
    test_rtc.TIME = 0x000000; /* hardware midnight rollover value */
    assert(board_rtc_read(&h,&m,&s) && h == 0 && m == 0);
    test_rtc.TIME = 0x006a00;
    assert(!board_rtc_read(&h,&m,&s));
    test_rtc.TIME = 0x123456;
    test_rtc.CR1_f.WINDOW = 0;
    grant_window = false;
    uint32_t before = tick;
    assert(!board_rtc_set(1,2));
    assert(tick-before == 1200 && test_rtc.CR1_f.ACCESS == 0 && test_rtc.KEY == 0x55);
    assert(test_rtc.TIME == 0x123456);
    test_rtc.CR1_f.WAIT = 1;
    before = tick;
    assert(!board_rtc_set(1,2) && tick-before == 1200);
    test_rtc.CR1 = 0; /* retain an already-running LSE clock and calibration */
    test_sysctrl.LSE_f.STABLE = 1;
    test_rtc.PSC = 16383;
    test_rtc.COMPCFR1 = 0x8001;
    assert(board_rtc_init());
    assert(test_rtc.TIME == 0x123456 && test_rtc.PSC == 16383 && test_rtc.COMPCFR1 == 0x8001);
    ClockTime date={24,2,29,1,2,3},got;
    grant_window=true;test_rtc.CR1_f.WINDOW=0;
    assert(board_rtc_save(&date,true));
    assert(test_rtc.TIME==0x123456);
    assert(board_rtc_read_time(&got) && got.year==24 && got.month==2 && got.day==29);
    uint32_t saved_date=test_rtc.DATE;
    assert(board_rtc_set(22,17) && test_rtc.DATE==saved_date);
    date.year=25;assert(!board_rtc_save(&date,true));
    test_sysctrl.LSE_f.STABLE = 0;
    before = tick;
    assert(!board_rtc_init() && tick-before == 3000);
    test_rtc.CR0 = 0;
    before = tick;
    assert(!board_rtc_init());
    assert(tick-before == 3000);
    puts("PASS: RTC defaults/prescaler, retained LSE/time/calibration, 24h validation, save verification, clock/window/WAIT timeouts");
}
