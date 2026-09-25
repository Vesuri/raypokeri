#ifndef POKERI_PAULA_PERIODS_H
#define POKERI_PAULA_PERIODS_H
#include <stdint.h>
namespace pokeri {
// floor(3546895 * 16 * AY_period / (1000000 * loop_length)).
// Advance quotient/remainder instead of doing 4095 software wide divisions.
inline void paulaPeriods(uint16_t *periods){
    uint32_t period=0,remainder=0;
    for(unsigned p=1;p<4096;++p){
        period+=28;remainder+=750320;
        if(remainder>=2000000){remainder-=2000000;++period;}
        uint32_t value=p>2300?period>>2:period;
        periods[p]=uint16_t(value<124?124:value);
    }
    periods[0]=periods[1];
}
}
#endif
