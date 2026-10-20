/******************************************************************************
* Include files                                                              *
******************************************************************************/
#include "system_cw32l010.h"
#include "cw32l010_sysctrl.h"

/* #define USER_VECT_TAB_ADDRESS */

#if defined(USER_VECT_TAB_ADDRESS)
/*!< Uncomment the following line if you need to relocate your vector Table
     in Sram else user remap will be done in Flash. */
/* #define VECT_TAB_SRAM */
#if defined(VECT_TAB_SRAM)
#define VECT_TAB_BASE_ADDRESS   0x20000000U       /*!< Vector Table base address field.
                                                     This value must be a multiple of 0x200. */
#define VECT_TAB_OFFSET         0x00000000U     /*!< Vector Table base offset field.
                                                     This value must be a multiple of 0x200. */
#else
#define VECT_TAB_BASE_ADDRESS   0x00000000U      /*!< Vector Table base address field.
                                                     This value must be a multiple of 0x200. */
#define VECT_TAB_OFFSET         0x00000000U     /*!< Vector Table base offset field.
                                                     This value must be a multiple of 0x200. */
#endif /* VECT_TAB_SRAM */
#endif /* USER_VECT_TAB_ADDRESS */
/******************************************************************************
 ** System Clock Frequency (Core Clock) Variable according CMSIS
 ******************************************************************************/

uint32_t SystemCoreClock = 4000000;

void SystemCoreClockUpdate(void) // Update SystemCoreClock variable
{
    static const uint32_t hclk_div[] = {
        1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u
    };
    uint32_t div = CW_SYSCTRL->HSI_f.DIV;
    uint32_t hclk = (CW_SYSCTRL->CR0 & SYSCTRL_CR0_HCLKPRS_Msk) >>
        SYSCTRL_CR0_HCLKPRS_Pos;

    /* DIV=0 encodes divide-by-16; codes 1..15 encode their value. */
    div = div ? div : 16u;
    if (hclk >= (sizeof hclk_div / sizeof hclk_div[0])) hclk = 0u;
    SystemCoreClock = HSIOSC_VALUE / div / hclk_div[hclk];
}

static void SystemClock24MHz(void)
{
    uint32_t cr0;

    /* The reference manual requires the FLASH clock before changing WAIT. */
    __SYSCTRL_FLASH_CLK_ENABLE();
    CW_FLASH->CR2 = 0x5A5A0000U | SYSCTRL_FLASHWAIT_24M;

    /* Keep the bus clocks at the full HSI rate for SPI and SysTick. */
    cr0 = CW_SYSCTRL->CR0;
    cr0 &= ~(SYSCTRL_CR0_HCLKPRS_Msk | SYSCTRL_CR0_PCLKPRS_Msk);
    CW_SYSCTRL->CR0 = SYSCTRL_KEY | (cr0 & 0xffu);

    /* HSIOSC is factory-trimmed to 48 MHz; DIV=2 gives a 24 MHz HSI. */
    CW_SYSCTRL->HSI_f.DIV = 2u;
}

/**
 ******************************************************************************
 ** \brief  Setup the microcontroller system. Initialize the System and update
 ** the SystemCoreClock variable.
 **
 ** \param  none
 ** \return none
 ******************************************************************************/
void SystemInit(void)
{
    /* Configure the Vector Table location add offset address ------------------*/
#if defined(USER_VECT_TAB_ADDRESS)
    SCB->VTOR = VECT_TAB_BASE_ADDRESS | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal SRAM */
#endif /* USER_VECT_TAB_ADDRESS */

    //Load TrimCode in this.
    CW_SYSCTRL->HSI_f.TRIM = *((volatile uint16_t *)SYSCTRL_HSI_TRIMCODEADDR);
    CW_SYSCTRL->LSI_f.TRIM = *((volatile uint16_t *)SYSCTRL_LSI_TRIMCODEADDR);

    SystemClock24MHz();
    SystemCoreClockUpdate();

    //Init Hide thing.
    //...
}

void FirmwareDelay(uint32_t DlyCnt)
{
    volatile uint32_t thisCnt = DlyCnt;
    while ( thisCnt-- )
    {
        ;
    }
}
