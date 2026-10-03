#include "temperature.h"
bool temperature_convert(uint16_t ts,uint16_t vref,uint8_t t0,uint16_t trim,int16_t *c10)
{
    if (!c10 || !ts || ts>=4095u || !vref || vref>=4095u ||
        t0==255u || !trim || trim>=4095u) return false;
    uint32_t vdd=(1200u*4095u+vref/2u)/vref;
    if (vdd<1600u || vdd>5500u) return false;
    int32_t mv=(1200u*ts+vref/2u)/vref;
    /* SDK: T=(T0>>1)+0.095765*(Vdd*ADC-TRIM*4.095).
       In deci-C: delta_mV * 3.92157675. Fixed point avoids soft-float. */
    int32_t delta=(mv-(int32_t)trim)*39216;
    int32_t value=(t0>>1)*10+(delta+(delta>=0 ? 5000 : -5000))/10000;
    if (value < -400 || value > 850) return false;
    *c10=(int16_t)value;
    return true;
}
