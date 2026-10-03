#include "main.h"
#include "temperature.h"
#include "cw32l010_adc.h"

static uint32_t since,sum_ts,sum_ref;
static uint8_t state,samples,t0;
static uint16_t trim;
static int16_t filtered;
static bool valid;
volatile uint32_t temperature_failures;
static void stop(bool failed,uint32_t now)
{
    CW_ADC->CR &= ~(ADC_CR_EN_Msk|ADC_CR_TSEN_Msk|ADC_CR_BGREN_Msk);
    state=0;since=now;
    if (failed) { valid=false;++temperature_failures; }
}
void temperature_init(void)
{
    __SYSCTRL_ADC_CLK_ENABLE();
    CW_ADC->CR=ADC_Clk_Div8|ADC_SqrEns0to1;
    CW_ADC->SQRCFR_f.SQRCH0=ADC_InputTs;
    CW_ADC->SQRCFR_f.SQRCH1=ADC_InputVref1P2;
    CW_ADC->SAMPLE_f.SQRCH0=ADC_SampTime390Clk;
    CW_ADC->SAMPLE_f.SQRCH1=ADC_SampTime390Clk;
    CW_ADC->IER=0;CW_ADC->TRIGGER=0;
    t0=*(const volatile uint8_t *)ADC_TS_T0_ADDRESS;
    trim=*(const volatile uint16_t *)ADC_TS_TRIM_ADDRESS;
    since=0u-60000u;state=0;valid=false;
}
void temperature_poll(uint32_t now)
{
    if (!state) {
        if (now-since<60000u) return;
        CW_ADC->CR |= ADC_CR_EN_Msk|ADC_CR_BGREN_Msk|ADC_CR_TSEN_Msk;
        since=now;state=1;sum_ts=sum_ref=samples=0;
    } else if (state==1) {
        /* Stabilization wait is cooperative; no delay()/BUSY loop. */
        if (now-since<2u) return;
        CW_ADC->ICR=0;
        CW_ADC->START=ADC_START_START_Msk;
        since=now;state=2;
    } else {
        if (CW_ADC->ISR & ADC_IT_EOS) {
            /* Discard first conversion after powering the reference/sensor. */
            if (samples++) { sum_ts+=CW_ADC->RESULT0;sum_ref+=CW_ADC->RESULT1; }
            if (samples==17u) {
                int16_t value;
                if (temperature_convert((sum_ts+8u)/16u,(sum_ref+8u)/16u,t0,trim,&value)) {
                    filtered=valid ? (3*(int32_t)filtered+value)/4 : value;
                    valid=true;stop(false,now);
                } else stop(true,now);
            } else { since=now-2u;state=1; }
        } else if (now-since>=20u) stop(true,now);
    }
}
bool temperature_read(int16_t *c10)
{
    if (!valid || !c10) return false;
    *c10=filtered;return true;
}
