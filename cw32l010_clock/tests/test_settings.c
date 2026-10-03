#include "settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t flash[1024];
static int budget=-1;
static unsigned erases;
const uint8_t *settings_flash_read(uint32_t address)
{ assert(address>=0xfa00 && address<0xfe00); return flash+(address-0xfa00); }
static bool permit(void) { if(budget<0)return true; if(!budget)return false; --budget;return true; }
bool settings_flash_erase(uint32_t a)
{ assert(a==0xfa00 || a==0xfc00);if(!permit())return false;memset(flash+(a-0xfa00),255,512);++erases;return true; }
bool settings_flash_program(uint32_t a,uint8_t v)
{ if(!permit())return false;unsigned i=a-0xfa00;assert(i<1024);assert((flash[i]&v)==v);flash[i]=v;return true; }
static void equal(ClockSettings a,ClockSettings b) { assert(!memcmp(&a,&b,sizeof a)); }
int main(void)
{
    memset(flash,255,sizeof flash); ClockSettings a=clock_defaults(),b=a,c=a;
    equal(settings_load(),a);assert(settings_save(&a));assert(!erases);
    a.theme=1;assert(settings_save(&a));equal(settings_load(),a);
    uint8_t saved[1024];memcpy(saved,flash,sizeof saved);b.theme=2;
    for(int i=0;i<21;++i) {
        memcpy(flash,saved,sizeof flash);budget=i;
        assert(!settings_save(&b));equal(settings_load(),a);
    }
    budget=-1;assert(settings_save(&b));equal(settings_load(),b);
    flash[512+8]^=1;equal(settings_load(),a); /* CRC rejects corrupt newer record. */
    assert(settings_save(&b));equal(settings_load(),b);
    unsigned count=erases;assert(settings_save(&b));assert(erases==count);
    c.hour12=1;assert(settings_save(&c));equal(settings_load(),c);
    c.theme=99;assert(!settings_save(&c));
    puts("PASS: settings defaults, unchanged-write suppression, dual-page recovery at every write boundary, CRC rejection");
}
