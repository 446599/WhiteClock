#include "main.h"
#include "board.h"
#include <assert.h>
#include <stdio.h>
FakeSPI fake_spi;
FakeGPIO fake_gpio;
unsigned fake_cs, sent;
static unsigned yield_after_byte;
static unsigned delay_reads, ready_after, status_reads;
void SysTick_Handler(void);
static void advance(unsigned ms)
{
    while (ms--) SysTick_Handler();
}
static void key_level(uint32_t pins,bool pressed)
{
    if (pressed) fake_gpio.IDR &= ~pins;
    else fake_gpio.IDR |= pins;
    board_key_gpio_irq(pins);
}
FakeSPI *fake_spi_device(void)
{
    ++status_reads;
    if (ready_after && !--ready_after) fake_spi.ISR=SPI_FLAG_TXE;
    return &fake_spi;
}
void SPI_SendData(FakeSPI *spi,uint8_t data)
{
    (void)data;++sent;
    if (yield_after_byte || delay_reads) spi->ISR=0;
    ready_after=delay_reads;
}
int main(void)
{
    /* Physical SET is PB3 and must also work when held during startup. */
    fake_gpio.IDR=~GPIO_PIN_3;board_init();uint8_t data[19]={0};
    advance(21);assert(board_key()==KEY_OK);assert(board_key()==KEY_NONE);
    advance(500);assert(board_key()==KEY_REPEAT);assert(board_key()==KEY_NONE);
    key_level(GPIO_PIN_3,false);advance(30);assert(board_key()==KEY_NONE);
    fake_spi.ISR=SPI_FLAG_TXE;
    assert(board_epd_begin(true,data,19));assert(!board_epd_begin(true,data,19));
    assert(board_epd_poll()==0 && sent==8 && !fake_cs);
    assert(board_epd_poll()==0 && sent==16);
    assert(board_epd_poll()==1 && sent==19 && fake_cs);
    /* Short hardware backpressure must finish within a bounded burst,
       not force one expensive main-loop round for every byte. */
    unsigned start=sent; delay_reads=3u; status_reads=0;
    fake_spi.ISR=SPI_FLAG_TXE;assert(board_epd_begin(true,data,3));
    assert(board_epd_poll()==1 && sent==start+3u && fake_cs);
    assert(status_reads<=64u+8u);
    delay_reads=0;
    fake_spi.ISR=0;assert(board_epd_begin(true,data,19));
    uint32_t before=board_millis();
    for(unsigned n=0;n<100;++n) {
        status_reads=0;
        assert(board_epd_poll()==0 && board_millis()==before && status_reads<=64u);
    }
    for(unsigned n=0;n<5;++n) SysTick_Handler();
    assert(board_epd_poll()==-1 && fake_cs);
    /* Physical BACK is PB2: short press advances; long press returns once. */
    key_level(GPIO_PIN_2,true);advance(21);
    assert(board_key()==KEY_NEXT);assert(board_key()==KEY_NONE);
    advance(699);assert(board_key()==KEY_NONE);
    advance(1);assert(!(fake_gpio.IDR&GPIO_PIN_2));
    assert(board_key()==KEY_BACK);assert(board_key()==KEY_NONE);
    advance(2000);assert(!(fake_gpio.IDR&GPIO_PIN_2));assert(board_key()==KEY_NONE);
    key_level(GPIO_PIN_2,false);advance(30);assert(board_key()==KEY_NONE);
    fake_spi.ISR=SPI_FLAG_TXE|SPI_FLAG_BUSY;assert(board_epd_begin(true,data,1));
    assert(board_epd_poll()==0);
    for(unsigned n=0;n<5;++n) SysTick_Handler();
    assert(board_epd_poll()==-1 && fake_cs);
    /* A 56-byte LUT can progress across many cooperative main-loop rounds.
       Five milliseconds from begin must not abort healthy progress. */
    uint8_t lut[56]={0};unsigned prior=sent;yield_after_byte=1;
    fake_spi.ISR=SPI_FLAG_TXE;assert(board_epd_begin(true,lut,sizeof lut));
    for(unsigned n=0;n<sizeof lut;++n) {
        fake_spi.ISR=SPI_FLAG_TXE;
        assert(board_epd_poll()==0 && sent==prior+n+1 && !fake_cs);
        SysTick_Handler();
    }
    fake_spi.ISR=SPI_FLAG_TXE;
    assert(board_epd_poll()==1 && fake_cs);yield_after_byte=0;
    /* Stalling after partial progress still fails after five milliseconds. */
    yield_after_byte=1;fake_spi.ISR=SPI_FLAG_TXE;
    assert(board_epd_begin(true,lut,sizeof lut));assert(board_epd_poll()==0);
    for(unsigned n=0;n<5;++n) SysTick_Handler();
    assert(board_epd_poll()==-1 && fake_cs);yield_after_byte=0;
    /* Slow trickle must also obey the length-scaled overall deadline. */
    yield_after_byte=1;fake_spi.ISR=SPI_FLAG_TXE;
    assert(board_epd_begin(true,lut,sizeof lut));
    for(unsigned n=0;n<17u;++n) {
        fake_spi.ISR=SPI_FLAG_TXE;assert(board_epd_poll()==0);
        for(unsigned ms=0;ms<4u;++ms) SysTick_Handler();
    }
    fake_spi.ISR=SPI_FLAG_TXE;assert(board_epd_poll()==-1 && fake_cs);
    yield_after_byte=0;
    key_level(GPIO_PIN_2|GPIO_PIN_3,true);advance(30);
    assert(board_key()==KEY_OK);assert(board_key()==KEY_NEXT);assert(board_key()==KEY_NONE);
    key_level(GPIO_PIN_2|GPIO_PIN_3,false);advance(30);assert(board_key()==KEY_NONE);
    /* An unrelated edge must not wake either button sampler. */
    fake_gpio.IDR &= ~GPIO_PIN_3;board_key_gpio_irq(1u<<7);advance(30);
    assert(board_key()==KEY_NONE);
    board_key_gpio_irq(GPIO_PIN_3);advance(30);assert(board_key()==KEY_OK);
    key_level(GPIO_PIN_3,false);advance(30);assert(board_key()==KEY_NONE);
    fake_gpio.IDR &= ~GPIO_PIN_2;board_init();advance(30);
    assert(board_key()==KEY_NEXT);
    key_level(GPIO_PIN_2,false);advance(30);assert(board_key()==KEY_NONE);
    puts("PASS: SPI budget/stalls/timeouts, SET=PB3 and BACK=PB2, held-at-boot, short/long/repeat, simultaneous and unrelated edges");
}
