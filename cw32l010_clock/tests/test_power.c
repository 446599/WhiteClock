#include "main.h"
#include "board.h"
#include "power.h"
#include <assert.h>
#include <stdio.h>

TestSysTick test_tick;
TestSCB test_scb;
TestSYSCTRL test_sys;
uint32_t test_mask;
static bool ready,valid=true,race,fail_after,key_on_wake;
static unsigned reads,waits,deep_waits,interrupts;
static uint32_t before_phase,after_phase,advanced;

bool board_sleep_ready(void) { return ready; }
bool board_rtc_sleep_ticks(uint32_t *ticks)
{
    ++reads;
    if (!valid || (fail_after && reads==2u)) return false;
    *ticks=reads==1u ? before_phase : after_phase;
    return true;
}
uint32_t board_sleep_advance(uint32_t ticks) { advanced=ticks;return ticks*1000u/BOARD_RTC_HZ; }
void test_disable_irq(void) { test_mask=1; if (race) ready=false; }
void test_set_mask(uint32_t mask)
{
    test_mask=mask;
    if (!mask && key_on_wake) {
        assert(test_tick.CTRL==7u && !(test_scb.SCR&SCB_SCR_SLEEPDEEP_Msk));
        assert((test_sys.APBEN1&0xffffu)==0x3ffu);
        key_on_wake=false;++interrupts;
    }
}
void test_wfi(void)
{
    ++waits;
    if (test_scb.SCR & SCB_SCR_SLEEPDEEP_Msk) {
        ++deep_waits;
        assert(test_mask==1u && test_tick.CTRL==0u);
        assert(test_tick.LOAD==23999u && test_tick.VAL==731u);
        assert(!(test_scb.SCR&SCB_SCR_SLEEPONEXIT_Msk));
        assert(!(test_sys.APBEN1 & (SYSCTRL_APB1_PERIPH_SPI|SYSCTRL_APB1_PERIPH_ADC)));
        assert(!(test_sys.CR2&SYSCTRL_CR2_WAKEUPCLK_Msk));
    }
}
static void setup(void)
{
    test_tick=(TestSysTick){7u,23999u,731u};
    test_scb.SCR=SCB_SCR_SLEEPONEXIT_Msk|16u;
    test_sys=(TestSYSCTRL){0x3ffu,0xc08u};test_mask=0;
    ready=valid=true;race=fail_after=key_on_wake=false;
    reads=waits=deep_waits=0;advanced=0;
    before_phase=100u;after_phase=100u+BOARD_RTC_HZ;
}
static void restored(void)
{
    assert(test_mask==0 && test_tick.CTRL==7u && test_tick.VAL==731u);
    assert(test_tick.LOAD==23999u && test_scb.SCR==(SCB_SCR_SLEEPONEXIT_Msk|16u));
    assert((test_sys.APBEN1&0xffffu)==0x3ffu && (test_sys.CR2&0xffffu)==0xc08u);
}
int main(void)
{
    setup();power_idle(false);assert(waits==1u && !deep_waits && !reads);restored();
    setup();ready=false;power_idle(true);assert(!deep_waits && !reads);restored();
    setup();test_mask=1;power_idle(true);assert(test_mask==1 && !waits && !reads);
    setup();race=true;power_idle(true);assert(!deep_waits && !reads);restored();
    setup();valid=false;power_idle(true);assert(!deep_waits && reads==1u);restored();
    setup();power_idle(true);assert(deep_waits==1u && advanced==BOARD_RTC_HZ);restored();
    setup();before_phase=BOARD_RTC_MINUTE_TICKS-100u;after_phase=200u;
    key_on_wake=true;power_idle(true);assert(advanced==300u && interrupts==1u);restored();
    setup();after_phase=before_phase;power_idle(true);assert(advanced==0u);restored();
    unsigned entries=power_deep_entries;
    uint32_t slept=power_sleep_ms;
    for (unsigned second=0;second<3600u;++second) {
        setup();power_idle(true);assert(advanced==BOARD_RTC_HZ);restored();
    }
    assert(power_deep_entries-entries==3600u && power_sleep_ms-slept==3600000u);
    setup();fail_after=true;power_idle(true);assert(power_clock_failures==1u);restored();
    setup();power_idle(true);assert(!deep_waits && !reads);restored();
    puts("PASS: deep/shallow guards, atomic key wake, clock/IRQ restoration, minute wrap, 3600 RTC wakes and clock-failure fallback");
}
