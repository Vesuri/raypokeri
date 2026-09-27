#ifndef POKERI_DELAY_BUDGET_H
#define POKERI_DELAY_BUDGET_H
#include "board/WordMath.h"
namespace pokeri {
// Number of original SUBQ/BNE instructions that fit an available reference
// cycle budget. Batches start at SUBQ. A zero word counter wraps 65536 times.
inline uint32_t delaySteps(uint16_t counter,uint32_t available){
    if(available<4)return 0;
    // A bounded dividend keeps the 68000's word division sufficient. Splitting
    // a longer wait is harmless; it creates another scheduler boundary.
    if(available>65520)available=65520;
    unsigned whole=wordQuotient(uint16_t(available),14);
    if(counter && counter<=wordQuotient(uint16_t(available+2),14))return unsigned(counter)<<1;
    return whole?whole<<1:1;
}
}
#endif
