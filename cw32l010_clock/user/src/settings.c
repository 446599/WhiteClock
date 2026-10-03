#include "settings.h"
/* Two dedicated pages: keep the previous committed page until its successor
   is verified. Byte 19 is the commit marker and is always programmed last. */
#define PAGE_A 0xfa00u
#define PAGE_B 0xfc00u
#define RECORD_BYTES 20u
static uint32_t checksum(const uint8_t *p)
{
    uint32_t crc=0xffffffffu;
    for(unsigned i=0;i<15u;++i) {
        crc^=p[i];
        for(unsigned b=0;b<8u;++b) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return ~crc;
}
static uint32_t word(const uint8_t *p)
{ return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static bool valid(uint32_t address)
{
    const uint8_t *p=settings_flash_read(address);
    ClockSettings s;
    uint8_t *d=(uint8_t *)&s;
    for(unsigned i=0;i<sizeof s;++i) d[i]=p[8+i];
    return p[19]==0xa5 && word(p)==0x334b4c43u && word(p+15)==checksum(p) && clock_settings_valid(&s);
}
static uint32_t latest(void)
{
    bool a=valid(PAGE_A), b=valid(PAGE_B);
    if(!a) return b ? PAGE_B : 0;
    if(!b) return PAGE_A;
    return (int32_t)(word(settings_flash_read(PAGE_B+4))-word(settings_flash_read(PAGE_A+4)))>0 ? PAGE_B : PAGE_A;
}
ClockSettings settings_load(void)
{
    ClockSettings s=clock_defaults(); uint32_t a=latest();
    if(a) for(unsigned i=0;i<sizeof s;++i) ((uint8_t *)&s)[i]=settings_flash_read(a)[8+i];
    return s;
}
bool settings_save(const ClockSettings *s)
{
    if(!clock_settings_valid(s)) return false;
    ClockSettings old=settings_load(); bool same=true;
    for(unsigned i=0;i<sizeof old;++i) if(((const uint8_t *)&old)[i]!=((const uint8_t *)s)[i]) same=false;
    if(same) return true;
    uint32_t current=latest(), target=current==PAGE_A ? PAGE_B : PAGE_A;
    uint32_t seq=current ? word(settings_flash_read(current+4))+1u : 1u;
    uint8_t record[RECORD_BYTES]={0x43,0x4c,0x4b,0x33};
    for(unsigned i=0;i<4u;++i) record[4+i]=(uint8_t)(seq>>(8u*i));
    for(unsigned i=0;i<sizeof *s;++i) record[8+i]=((const uint8_t *)s)[i];
    uint32_t crc=checksum(record);
    for(unsigned i=0;i<4u;++i) record[15+i]=(uint8_t)(crc>>(8u*i));
    record[19]=0xa5;
    if(!settings_flash_erase(target)) return false;
    for(unsigned i=0;i<RECORD_BYTES;++i)
        if(!settings_flash_program(target+i,record[i])) return false;
    return valid(target);
}
