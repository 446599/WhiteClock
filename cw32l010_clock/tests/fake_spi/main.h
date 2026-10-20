#ifndef FAKE_SPI_MAIN_H
#define FAKE_SPI_MAIN_H
#include <stdint.h>
typedef struct { uint32_t ISR; } FakeSPI;
typedef struct { uint32_t IDR, ISR, ICR, RISEIE, FALLIE, FILTER; } FakeGPIO;
extern FakeSPI fake_spi;
extern FakeGPIO fake_gpio;
extern unsigned fake_cs;
FakeSPI *fake_spi_device(void);
#define CW_SPI fake_spi_device()
#define CW_GPIOB (&fake_gpio)
#define CW_GPIOA (&fake_gpio)
#define GPIO_PIN_2 (1u<<2)
#define GPIO_PIN_3 (1u<<3)
#define SPI_FLAG_TXE 1u
#define SPI_FLAG_BUSY 2u
void SPI_SendData(FakeSPI *spi,uint8_t data);
#define __WFI() ((void)0)
#define PA01_GETVALUE() 1
#define PA02_SETHIGH() ((void)0)
#define PA02_SETLOW() ((void)0)
#define PA03_SETHIGH() (fake_cs=1)
#define PA03_SETLOW() (fake_cs=0)
#define PA04_SETHIGH() ((void)0)
#define PA04_SETLOW() ((void)0)
#define __SYSCTRL_GPIOA_CLK_ENABLE() ((void)0)
#define __SYSCTRL_GPIOB_CLK_ENABLE() ((void)0)
#define __SYSCTRL_SPI_CLK_ENABLE() ((void)0)
#define EPD_RST_PIN 4
#define EPD_CS_PIN 8
#define EPD_DC_PIN 16
#define EPD_SPI_SCK_PIN 32
#define EPD_SPI_MOSI_PIN 64
#define EPD_BUSY_PIN 2
#define GPIO_MODE_OUTPUT_PP 1
#define GPIO_MODE_INPUT 2
#define GPIO_MODE_INPUT_PULLUP 3
typedef struct { unsigned Pins,Mode,IT; } GPIO_InitTypeDef;
#define GPIO_IT_RISING 0x81u
#define GPIO_IT_FALLING 0x82u
#define GPIO_FLTCLK_HCLK8 0x00020000u
#define GPIOB_INTFLAG_CLR(mask) ((void)(mask))
#define GPIOB_IRQn 6
#define NVIC_ClearPendingIRQ(x) ((void)(x))
#define NVIC_SetPriority(x,p) ((void)(x),(void)(p))
#define NVIC_EnableIRQ(x) ((void)(x))
#define GPIO_ConfigFilter(a,b,c) ((void)(a),(void)(b),(void)(c))
#define GPIO_Init(a,b) ((void)(a),(void)(b))
#define PA05_AFx_SPI1SCK() ((void)0)
#define PA06_AFx_SPI1MOSI() ((void)0)
typedef struct { int a,b,c,d,e,f,g,h,i; } SPI_InitTypeDef;
#define SPI_Direction_1Line_TxOnly 0
#define SPI_Mode_Master 0
#define SPI_DataSize_8b 0
#define SPI_CPOL_Low 0
#define SPI_CPHA_1Edge 0
#define SPI_NSS_Soft 0
#define SPI_BaudRatePrescaler_4 0
#define SPI_BaudRatePrescaler_8 0
#define SPI_FirstBit_MSB 0
#define SPI_Speed_Low 0
#define SPI_Init(a,b) ((void)(a),(void)(b))
#define SPI_NSSInternalSoftwareConfig(a,b) ((void)(a),(void)(b))
#define SPI_NSSInternalSoft_Set 0
#define SPI_Cmd(a,b) ((void)(a),(void)(b))
#define ENABLE 1
#define SystemCoreClock 48000000u
#define SysTick_Config(x) ((void)(x))
#endif
