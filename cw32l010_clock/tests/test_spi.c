#include "main.h"
#include "board.h"
#include <assert.h>
#include <stdio.h>
FakeSPI fake_spi;
FakeGPIO fake_gpio;
unsigned fake_cs, sent;
void SysTick_Handler(void);
void SPI_SendData(FakeSPI *spi,uint8_t data) { (void)spi;(void)data;++sent; }
int main(void)
{
    fake_gpio.IDR=~0u;board_init();uint8_t data[19]={0};
    fake_spi.ISR=SPI_FLAG_TXE;
    assert(board_epd_begin(true,data,19));assert(!board_epd_begin(true,data,19));
    assert(board_epd_poll()==0 && sent==8 && !fake_cs);
    assert(board_epd_poll()==0 && sent==16);
    assert(board_epd_poll()==1 && sent==19 && fake_cs);
    fake_spi.ISR=0;assert(board_epd_begin(true,data,19));
    uint32_t before=board_millis();
    for(unsigned n=0;n<100;++n) assert(board_epd_poll()==0 && board_millis()==before);
    for(unsigned n=0;n<5;++n) SysTick_Handler();
    assert(board_epd_poll()==-1 && fake_cs);
    fake_gpio.IDR &= ~(1u<<3);
    for(unsigned n=0;n<30;++n) SysTick_Handler();
    assert(board_key()==KEY_OK);
    fake_spi.ISR=SPI_FLAG_TXE|SPI_FLAG_BUSY;assert(board_epd_begin(true,data,1));
    assert(board_epd_poll()==0);
    for(unsigned n=0;n<5;++n) SysTick_Handler();
    assert(board_epd_poll()==-1 && fake_cs);
    puts("PASS: SPI 8-byte poll budget, nonblocking TXE/BUSY stalls, 5ms total timeout, key sampling while SPI fails");
}
