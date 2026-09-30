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
// Opt-in batching must stop on the same 1 ms grid as ordinary startup.
// Phases are scaled by their fixed 100/50 Hz rates. Unsupported profiles and
// serial/input-file activity are rejected by the caller, before using this.
// A warning age is relative to the latest kick, not to either periodic phase.
inline unsigned startupQuietTicks(unsigned cabinetTicks,uint32_t systemPhase,
                                  uint32_t inputPhase,uint64_t watchdogAge,
                                  uint64_t warning,uint64_t reset,
                                  bool warningEnabled,bool resetEnabled){
    if(cabinetTicks>=10 || systemPhase>=8000000 || inputPhase>=8000000)return 1;
    const unsigned limit=10-cabinetTicks;
    for(unsigned ticks=1;ticks<=limit;++ticks){
        const uint64_t previous=watchdogAge;watchdogAge+=8000;
        systemPhase+=800000;inputPhase+=400000;
        if(systemPhase>=8000000 || inputPhase>=8000000 ||
           (warningEnabled && previous<warning && watchdogAge>=warning) ||
           (resetEnabled && watchdogAge>=reset))return ticks;
    }
    return limit;
}
// Keep the ordinary one-quantum build identical when batching is disabled.
inline unsigned startupDelayAvailable(unsigned phase){
    unsigned available=8000-phase;
    return available<4?4:available;
}
inline unsigned startupDelayAvailable(unsigned phase,unsigned ticks){
    unsigned available=wordProduct(uint16_t(ticks),8000)-phase;
    return available<4?4:available;
}
}
#endif
