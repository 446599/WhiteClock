#include "main.h"
#include "board.h"
#include "power.h"

volatile uint32_t power_deep_entries, power_sleep_ms, power_clock_failures;
static bool clock_fault;

void power_idle(bool allow_deep)
{
    if (!allow_deep) { __WFI(); return; }
    uint32_t mask=__get_PRIMASK();
    __disable_irq();
    if (mask) { __set_PRIMASK(mask); return; }
    uint32_t before,after;
    if (clock_fault || !board_sleep_ready() || !board_rtc_sleep_ticks(&before)) {
        __set_PRIMASK(mask); __WFI(); return;
    }
    uint32_t tick=SysTick->CTRL, clocks=CW_SYSCTRL->APBEN1;
    uint32_t config=CW_SYSCTRL->CR2, scr=SCB->SCR;
    SysTick->CTRL=0;
    CW_SYSCTRL->APBEN1=SYSCTRL_KEY | (clocks &
        ~(SYSCTRL_APB1_PERIPH_SPI | SYSCTRL_APB1_PERIPH_ADC));
    /* Keep the pre-sleep HSI divider: a forced 4 MHz wake would break SPI/ticks. */
    CW_SYSCTRL->CR2=SYSCTRL_KEY | (config & ~SYSCTRL_CR2_WAKEUPCLK_Msk);
    SCB->SCR=(scr & ~SCB_SCR_SLEEPONEXIT_Msk) | SCB_SCR_SLEEPDEEP_Msk;
    ++power_deep_entries;
    /* Enabled but masked IRQs still wake WFI; service them only after restoring
       clocks and time. Do not clear pending key/RTC IRQs in this entry window. */
    __DSB(); __WFI(); __ISB();
    SCB->SCR=scr & ~SCB_SCR_SLEEPDEEP_Msk;
    CW_SYSCTRL->CR2=SYSCTRL_KEY | config;
    CW_SYSCTRL->APBEN1=SYSCTRL_KEY | clocks;
    if (board_rtc_sleep_ticks(&after)) {
        uint32_t elapsed=after>=before ? after-before :
            BOARD_RTC_MINUTE_TICKS-before+after;
        power_sleep_ms+=board_sleep_advance(elapsed);
    } else {
        /* A failed sleep clock must not leave timeouts frozen on later sleeps. */
        ++power_clock_failures; clock_fault=true;
    }
    /* Preserve VAL, including its unfinished millisecond and a pending tick. */
    SysTick->CTRL=tick;
    __set_PRIMASK(mask);
}
