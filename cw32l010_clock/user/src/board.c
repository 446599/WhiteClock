#include "main.h"
#include "board.h"
#include "clock_core.h"

static volatile uint32_t millis;
static ClockButton buttons[2];
static volatile uint8_t keys[64], key_head, key_tail;
volatile uint32_t key_overflows;

void SysTick_Handler(void)
{
    ++millis;
    if (millis % 5u) return;
    for (unsigned i = 0; i < 2u; ++i) {
        uint8_t key = clock_button_sample(&buttons[i],
            (CW_GPIOB->IDR & (GPIO_PIN_2 << i)) == 0, i == 1u, millis);
        if (key) {
            uint8_t next = (uint8_t)((key_head + 1u) % sizeof keys);
            if (next == key_tail) ++key_overflows;
            else { keys[key_head] = key; key_head = next; }
        }
    }
}

uint8_t board_key(void)
{
    if (key_tail == key_head) return KEY_NONE;
    uint8_t result = keys[key_tail];
    key_tail = (uint8_t)((key_tail + 1u) % sizeof keys);
    return result;
}
uint32_t board_millis(void) { return millis; }
void board_delay(uint32_t ms)
{
    uint32_t start = millis;
    while (millis - start < ms) __WFI();
}
bool board_epd_busy(void) { return PA01_GETVALUE() == 0; }
void board_epd_reset(bool high) { if (high) PA02_SETHIGH(); else PA02_SETLOW(); }

static struct {
    const uint8_t *bytes;
    unsigned length, offset;
    uint32_t started;
    bool active;
} spi_tx;
bool board_epd_begin(bool data,const uint8_t *bytes,unsigned length)
{
    if (spi_tx.active || !bytes || !length) return false;
    spi_tx.bytes=bytes;spi_tx.length=length;spi_tx.offset=0;
    spi_tx.started=millis;spi_tx.active=true;
    if (data) PA04_SETHIGH(); else PA04_SETLOW();
    PA03_SETLOW();
    return true;
}
int board_epd_poll(void)
{
    if (!spi_tx.active) return 0;
    /* TXE/BUSY are sampled, never spun on. Even broken SPI cannot stall keys. */
    if (millis-spi_tx.started>=5u) {
        PA03_SETHIGH();spi_tx.active=false;return -1;
    }
    for (unsigned budget=0;budget<8u && spi_tx.offset<spi_tx.length;++budget) {
        if (!(CW_SPI->ISR & SPI_FLAG_TXE)) return 0;
        SPI_SendData(CW_SPI,spi_tx.bytes[spi_tx.offset++]);
    }
    if (spi_tx.offset==spi_tx.length && (CW_SPI->ISR & SPI_FLAG_TXE) &&
        !(CW_SPI->ISR & SPI_FLAG_BUSY)) {
        PA03_SETHIGH();spi_tx.active=false;return 1;
    }
    return 0;
}

void board_init(void)
{
    __SYSCTRL_GPIOA_CLK_ENABLE();
    __SYSCTRL_GPIOB_CLK_ENABLE();
    __SYSCTRL_SPI_CLK_ENABLE();
    PA02_SETHIGH(); PA03_SETHIGH(); PA04_SETHIGH();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pins = EPD_RST_PIN | EPD_CS_PIN | EPD_DC_PIN | EPD_SPI_SCK_PIN | EPD_SPI_MOSI_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Init(CW_GPIOA, &gpio);
    gpio.Pins = EPD_BUSY_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    GPIO_Init(CW_GPIOA, &gpio);
    gpio.Pins = BUTTON_SET_PIN | BUTTON_UP_PIN;
    gpio.Mode = GPIO_MODE_INPUT_PULLUP;
    GPIO_Init(CW_GPIOB, &gpio);
    PA05_AFx_SPI1SCK(); PA06_AFx_SPI1MOSI();
    SPI_InitTypeDef spi = {
        SPI_Direction_1Line_TxOnly, SPI_Mode_Master, SPI_DataSize_8b,
        SPI_CPOL_Low, SPI_CPHA_1Edge, SPI_NSS_Soft, SPI_BaudRatePrescaler_8,
        SPI_FirstBit_MSB, SPI_Speed_Low
    };
    SPI_Init(CW_SPI, &spi);
    SPI_NSSInternalSoftwareConfig(CW_SPI, SPI_NSSInternalSoft_Set);
    SPI_Cmd(CW_SPI, ENABLE);
    SysTick_Config(SystemCoreClock / 1000u);
}
