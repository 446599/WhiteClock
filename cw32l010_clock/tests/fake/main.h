#ifndef TEST_MAIN_H
#define TEST_MAIN_H
#include <stdint.h>
typedef struct {
    uint32_t KEY;
    union { uint32_t CR0; struct { uint32_t interval:3, H24:1, reserved:3, START:1; } CR0_f; };
    union { uint32_t CR1; struct { uint32_t ACCESS:1, WINDOW:1, WAIT:1, reserved:5, SOURCE:3; } CR1_f; };
    uint32_t CR2, COMPCFR1, DATE, TIME, IER, PSC;
} FakeRTC;
typedef struct { uint32_t CR1, LSE; struct { uint32_t STABLE; } LSE_f, LSI_f; } FakeSYSCTRL;
typedef struct { uint32_t ANALOG; } FakeGPIO;
extern FakeRTC test_rtc;
extern FakeSYSCTRL test_sysctrl;
extern FakeGPIO test_gpio;
#define CW_RTC (&test_rtc)
#define CW_SYSCTRL (&test_sysctrl)
#define CW_GPIOB (&test_gpio)
#define RTC_CR1_SOURCE_Msk (7u << 8)
#define RTC_CR1_WAIT_Msk 4u
#define RTC_CR1_WINDOW_Msk 2u
#define RTC_RTCCLK_FROM_LSE 0u
#define RTC_RTCCLK_FROM_LSI 0x200u
#define SYSCTRL_KEY 0x5a5a0000u
#define SYSCTRL_CR1_LSEEN_Msk 0x10u
#define GPIO_PIN_0 1u
#define GPIO_PIN_1 2u
#define __SYSCTRL_RTC_CLK_ENABLE() ((void)0)
int SYSCTRL_LSI_Enable(void);
#endif
