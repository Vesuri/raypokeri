#ifndef POKERI_STARTUP_BUDGET_H
#define POKERI_STARTUP_BUDGET_H
#include "board/WordMath.h"
namespace pokeri {
// Retain the existing calibrated guest-work scale, without PAL debt/credit.
// Caller supplies bounded CIA intervals (at most 20 bits) and a ratio <= 80.
// Nominal hook/delay cycles are already reference cycles and are never scaled.
inline uint32_t startupWorkCycles(uint32_t cycles,bool reference,uint16_t ratio){
    if(reference)return cycles;
    return (wordProduct(uint16_t(cycles),ratio)>>4)+
           (wordProduct(uint16_t(cycles>>16),ratio)<<12);
}
// Supported model's finest timed boundary is the 1 kHz serial peer. A SUBQ
// cannot be split: permit its four cycles when only 1..3 cycles remain.
inline unsigned startupDelayAvailable(unsigned phase){
    unsigned available=8000-phase;
    return available<4?4:available;
}
}
#endif
