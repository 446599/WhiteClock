#include "main.h"
#include "board.h"
#include "clock_core.h"

static volatile uint32_t millis;
/* Index 0 is SET (PB3); index 1 is BACK (PB2). */
static ClockButton buttons[2];
static volatile uint8_t keys[64], key_head, key_tail;
static volatile uint8_t button_watch_mask;
volatile uint32_t key_overflows;

void SysTick_Handler(void)
{
    ++millis;
    for (unsigned i = 0; i < 2u; ++i) {
        uint8_t bit = (uint8_t)(1u << i);
        if (!(button_watch_mask & bit)) continue;
        uint32_t pin = i == 0u ? BUTTON_SET_PIN : BUTTON_BACK_PIN;
        uint8_t key = clock_button_sample(&buttons[i],
            (CW_GPIOB->IDR & pin) == 0, i == 0u, millis);
        if (key) {
            uint8_t next = (uint8_t)((key_head + 1u) % sizeof keys);
            if (next == key_tail) ++key_overflows;
            else { keys[key_head] = key; key_head = next; }
        }
        /* Once a released level has been stable, stop sampling until the
           next GPIO edge. A held key remains watched for long/repeat. */
        if (!buttons[i].raw && !buttons[i].stable) {
            button_watch_mask &= (uint8_t)~bit;
        }
    }
}

void board_key_gpio_irq(uint32_t pins)
{
    uint32_t mask = pins & (BUTTON_SET_PIN | BUTTON_BACK_PIN);
    if (!mask) return;
    if (mask & BUTTON_SET_PIN) button_watch_mask |= 1u << 0;
    if (mask & BUTTON_BACK_PIN) button_watch_mask |= 1u << 1;
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
    uint32_t started, last_progress;
    bool active;
} spi_tx;
bool board_epd_begin(bool data,const uint8_t *bytes,unsigned length)
{
    if (spi_tx.active || !bytes || !length) return false;
    spi_tx.bytes=bytes;spi_tx.length=length;spi_tx.offset=0;
    spi_tx.started=spi_tx.last_progress=millis;spi_tx.active=true;
    if (data) PA04_SETHIGH(); else PA04_SETLOW();
    PA03_SETLOW();
    return true;
}
int board_epd_poll(void)
{
    if (!spi_tx.active) return 0;
    /* Keep every poll bounded. Progress may span many cooperative loop rounds,
       but a stalled byte gets 5ms and total time gets 10ms + 1ms per byte. */
    if (millis-spi_tx.last_progress>=5u ||
        millis-spi_tx.started>=10u+spi_tx.length) {
        PA03_SETHIGH();spi_tx.active=false;return -1;
    }
    /* Absorb short TXE/BUSY gaps without re-running the UI per byte.
       Both register polls and transmitted bytes remain strictly bounded. */
    unsigned budget=0;
    for (unsigned polls=0;polls<64u;++polls) {
        if (budget==8u && spi_tx.offset<spi_tx.length) return 0;
        uint32_t status=CW_SPI->ISR;
        if (spi_tx.offset==spi_tx.length) {
            if ((status & SPI_FLAG_TXE) && !(status & SPI_FLAG_BUSY)) {
                PA03_SETHIGH();spi_tx.active=false;return 1;
            }
        } else if (status & SPI_FLAG_TXE) {
            SPI_SendData(CW_SPI,spi_tx.bytes[spi_tx.offset++]);
            spi_tx.last_progress=millis;
            ++budget;
        }
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
    gpio.Pins = BUTTON_SET_PIN | BUTTON_BACK_PIN;
    gpio.Mode = GPIO_MODE_INPUT_PULLUP;
    gpio.IT = GPIO_IT_RISING | GPIO_IT_FALLING;
    GPIO_Init(CW_GPIOB, &gpio);
    GPIO_ConfigFilter(CW_GPIOB, BUTTON_SET_PIN | BUTTON_BACK_PIN, GPIO_FLTCLK_HCLK8);
    GPIOB_INTFLAG_CLR(BUTTON_SET_PIN | BUTTON_BACK_PIN);
    NVIC_ClearPendingIRQ(GPIOB_IRQn);
    NVIC_SetPriority(GPIOB_IRQn, 0u);
    NVIC_EnableIRQ(GPIOB_IRQn);
    /* Capture a key already held during reset; normal transitions arrive via
       GPIOB_IRQHandler and set the same watch bits. */
    if (!(CW_GPIOB->IDR & BUTTON_SET_PIN)) button_watch_mask |= 1u << 0;
    if (!(CW_GPIOB->IDR & BUTTON_BACK_PIN)) button_watch_mask |= 1u << 1;
    PA05_AFx_SPI1SCK(); PA06_AFx_SPI1MOSI();
    SPI_InitTypeDef spi = {
        SPI_Direction_1Line_TxOnly, SPI_Mode_Master, SPI_DataSize_8b,
        /* 24 MHz HCLK / 4 = 6 MHz SCK; keep the conservative SPI sample mode. */
        SPI_CPOL_Low, SPI_CPHA_1Edge, SPI_NSS_Soft, SPI_BaudRatePrescaler_4,
        SPI_FirstBit_MSB, SPI_Speed_Low
    };
    SPI_Init(CW_SPI, &spi);
    SPI_NSSInternalSoftwareConfig(CW_SPI, SPI_NSSInternalSoft_Set);
    SPI_Cmd(CW_SPI, ENABLE);
    SysTick_Config(SystemCoreClock / 1000u);
}
