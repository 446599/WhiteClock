#include "main.h"
#include "board.h"
#include "settings.h"
const uint8_t *settings_flash_read(uint32_t address) { return (const uint8_t *)address; }
static bool ready(void)
{
    uint32_t start=board_millis(); unsigned budget=1000000u;
    while(CW_FLASH->ISR_f.BUSY)
        if(!(--budget) || board_millis()-start>=100u) return false;
    return true;
}
static bool operation(uint32_t address,uint8_t value,bool erase)
{
    if(address<0xfa00u || address>=0xfe00u || (erase && address%512u)) return false;
    __SYSCTRL_FLASH_CLK_ENABLE();
    if(!ready()) return false;
    uint16_t lock=CW_FLASH->PAGELOCK, cr1=CW_FLASH->CR1;
    CW_FLASH->PAGELOCK=0x5a5a0000u|lock|0x8000u;
    CW_FLASH->ICR=0;
    CW_FLASH->CR1=0x5a5a0000u|(cr1&~3u)|(erase ? 2u : 1u);
    *(volatile uint8_t *)address=value;
    bool ok=ready() && !(CW_FLASH->ISR&0x13u);
    CW_FLASH->CR1=0x5a5a0000u|cr1;
    CW_FLASH->PAGELOCK=0x5a5a0000u|lock;
    if(erase) {
        for(unsigned i=0;i<512u;++i) if(*(volatile uint8_t *)(address+i)!=0xff) ok=false;
    } else if(*(volatile uint8_t *)address!=value) ok=false;
    return ok;
}
bool settings_flash_erase(uint32_t address) { return operation(address,0,true); }
bool settings_flash_program(uint32_t address,uint8_t value) { return operation(address,value,false); }
