#ifndef TEST_POWER_MAIN_H
#define TEST_POWER_MAIN_H
#include <stdint.h>
typedef struct { uint32_t CTRL,LOAD,VAL; } TestSysTick;
typedef struct { uint32_t SCR; } TestSCB;
typedef struct { uint32_t APBEN1,CR2; } TestSYSCTRL;
extern TestSysTick test_tick;
extern TestSCB test_scb;
extern TestSYSCTRL test_sys;
extern uint32_t test_mask;
void test_wfi(void);
void test_disable_irq(void);
void test_set_mask(uint32_t mask);
#define SysTick (&test_tick)
#define SCB (&test_scb)
#define CW_SYSCTRL (&test_sys)
#define __get_PRIMASK() test_mask
#define __disable_irq() test_disable_irq()
#define __set_PRIMASK(x) test_set_mask(x)
#define __WFI() test_wfi()
#define __DSB() ((void)0)
#define __ISB() ((void)0)
#define SYSCTRL_KEY 0x5a5a0000u
#define SYSCTRL_APB1_PERIPH_SPI (1u<<2)
#define SYSCTRL_APB1_PERIPH_ADC (1u<<0)
#define SYSCTRL_CR2_WAKEUPCLK_Msk 8u
#define SCB_SCR_SLEEPONEXIT_Msk 2u
#define SCB_SCR_SLEEPDEEP_Msk 4u
#endif
