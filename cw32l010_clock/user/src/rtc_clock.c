#include "main.h"
#include "board.h"
#include "clock_core.h"

static void unlock(void) { CW_RTC->KEY = 0xca; CW_RTC->KEY = 0x53; }
static void lock(void) { CW_RTC->KEY = 0xca; CW_RTC->KEY = 0x55; }

static bool wait_cr1(uint32_t mask, bool set)
{
    uint32_t start = board_millis();
    while (((CW_RTC->CR1 & mask) != 0) != set) {
        if (board_millis() - start >= 1200u) return false;
        board_delay(1u);
    }
    return true;
}

static bool source_ready(void)
{
    uint32_t source = CW_RTC->CR1 & RTC_CR1_SOURCE_Msk;
    if (source == RTC_RTCCLK_FROM_LSE) return CW_SYSCTRL->LSE_f.STABLE != 0;
    if (source == RTC_RTCCLK_FROM_LSI) return CW_SYSCTRL->LSI_f.STABLE != 0;
    return false;
}

bool board_rtc_read_time(ClockTime *t)
{
    if (!CW_RTC->CR0_f.START || !CW_RTC->CR0_f.H24 || !source_ready()) return false;
    for (unsigned retry=0; retry<16u; ++retry) {
        uint32_t time=CW_RTC->TIME, date=CW_RTC->DATE;
        if (time==CW_RTC->TIME && date==CW_RTC->DATE)
            return clock_decode(time,&t->hour,&t->minute,&t->second) && clock_date_decode(date,t);
    }
    return false;
}
bool board_rtc_read(uint8_t *h,uint8_t *m,uint8_t *s)
{
    ClockTime t;
    if (!board_rtc_read_time(&t)) return false;
    *h=t.hour; *m=t.minute; *s=t.second; return true;
}

bool board_rtc_init(void)
{
    __SYSCTRL_RTC_CLK_ENABLE();
    uint32_t source = CW_RTC->CR1 & RTC_CR1_SOURCE_Msk;
    bool running = CW_RTC->CR0_f.START != 0;
    /* This board's original firmware uses an external 32768 Hz crystal.
       0x0A24 is its measured writable LSE configuration (driver level 4). */
    CW_GPIOB->ANALOG |= GPIO_PIN_0 | GPIO_PIN_1;
    if (!(running && source == RTC_RTCCLK_FROM_LSE)) CW_SYSCTRL->LSE = 0x0a24u;
    CW_SYSCTRL->CR1 |= SYSCTRL_KEY | SYSCTRL_CR1_LSEEN_Msk;
    uint32_t start = board_millis();
    while (!CW_SYSCTRL->LSE_f.STABLE) {
        if (board_millis() - start >= 3000u) return false;
        board_delay(1u);
    }

    uint8_t h, m, s;
    if (running && source == RTC_RTCCLK_FROM_LSE && board_rtc_read(&h, &m, &s)) {
        unlock();
        CW_RTC->IER = 0; /* Polling, no inherited alarm interrupts. */
        lock();
        return true;
    }

    /* Power-on: 32768 Hz LSE / 16384 = 2 Hz. */
    unlock();
    CW_RTC->CR0 = 0x08; /* stop, 24-hour format */
    CW_RTC->CR1 = RTC_RTCCLK_FROM_LSE;
    CW_RTC->PSC = 16383u;
    CW_RTC->COMPCFR1 = 0;
    CW_RTC->CR2 = 0;
    CW_RTC->IER = 0;
    CW_RTC->DATE = 0x00261004u; /* 2026-10-04, Sunday */
    CW_RTC->TIME = clock_encode(12, 0, 0);
    CW_RTC->CR0 = 0x88;
    lock();
    return board_rtc_read(&h, &m, &s);
}

bool board_rtc_save(const ClockTime *t, bool date_only)
{
    if (!clock_time_valid(t) || !CW_RTC->CR0_f.START || !source_ready()) return false;
    if (!wait_cr1(RTC_CR1_WAIT_Msk, false)) return false;
    unlock();
    CW_RTC->CR1_f.ACCESS=1;
    if (!wait_cr1(RTC_CR1_WINDOW_Msk,true)) {
        CW_RTC->CR1_f.ACCESS=0; lock(); return false;
    }
    if (date_only) CW_RTC->DATE=clock_date_encode(t);
    else CW_RTC->TIME=clock_encode(t->hour,t->minute,0);
    CW_RTC->CR1_f.ACCESS=0; lock();
    if (!wait_cr1(RTC_CR1_WAIT_Msk,false)) return false;
    ClockTime got;
    if (!board_rtc_read_time(&got)) return false;
    return date_only ? (got.year==t->year && got.month==t->month && got.day==t->day) :
        (got.hour==t->hour && got.minute==t->minute && got.second<2u);
}
bool board_rtc_set(uint8_t h,uint8_t m)
{
    ClockTime t;
    if (!board_rtc_read_time(&t)) return false;
    t.hour=h; t.minute=m;
    return board_rtc_save(&t,false);
}
