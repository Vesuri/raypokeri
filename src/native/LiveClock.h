#ifndef POKERI_LIVE_CLOCK_H
#define POKERI_LIVE_CLOCK_H
#include "board/WordMath.h"
namespace pokeri {
// PAL E-clock ticks -> nominal 8 MHz board cycles. 361/32 differs from
// 8,000,000/709,379 by +0.034%. Only a native 16x16 multiply is needed.
inline uint32_t boardClockCycles(uint16_t ticks){return wordProduct(ticks,361)>>5;}
// Wall time may use only recent guest throughput. Credit and delayed wall
// time are bounded to 1..3 PAL frames (one by default). Each grant still keeps
// at most one frame of board ticks outstanding; catch-up cannot be unbounded.
struct LiveClock {
    static constexpr uint32_t frameCycles=160000;
    uint32_t credit=0,debt=0,frame=0,discardedWall=0,limited=0;
    // The tightest paired boot phase permits about 1.74; keep >12.5% margin.
    uint16_t ratioSixteenths=24,windowFrames=1;
    void reset(uint32_t now){credit=debt=0;frame=now;}
    __attribute__((always_inline))
    uint32_t grant(uint32_t cycles,bool reference,uint32_t now,uint32_t queued){
        const uint32_t limit=windowFrames==1?160000:windowFrames==2?320000:480000;
        // A repeated boundary cannot spend credit without wall debt.
        if(now==frame && !debt && (!cycles || credit==limit))return 0;
        uint32_t frames=now-frame;frame=now;
        if(frames){
            // Saturate before multiplication, including after uint32 wrap.
            discardedWall+=frames>windowFrames?frames-windowFrames:0;
            if(frames>=windowFrames)debt=limit;
            else {uint32_t add=frames==1?160000:320000;debt=add>=limit-debt?limit:debt+add;}
        }
        uint32_t add=cycles;
        if(!reference && cycles){
            // Caller intervals fit 20 ms normally. Saturating first also
            // makes long/overflow-recovery intervals safe without wide math.
            // Exact saturated forms of the calibrated boot/play ratios.
            if(ratioSixteenths==64)add=cycles>=(limit>>2)?limit:cycles<<2;
            else if(ratioSixteenths==24)add=cycles>=(windowFrames==1?106667u:windowFrames==2?213334u:320000u)?limit:cycles+(cycles>>1);
            else if(cycles>=limit*16)add=limit;
            else add=(wordProduct(uint16_t(cycles),ratioSixteenths)>>4)+
                     (wordProduct(uint16_t(cycles>>16),ratioSixteenths)<<12);
        }
        credit=add>=limit-credit?limit:credit+add;
        uint32_t available=queued>=frameCycles?0:frameCycles-queued;
        uint32_t use=credit<debt?credit:debt;
        if(use>available)use=available;
        if(debt && !credit)++limited;
        credit-=use;debt-=use;return use;
    }
    __attribute__((always_inline)) inline uint32_t batchContribution(
        uint32_t cycles,bool reference,uint32_t limit,uint32_t &available,
        uint32_t &remainingCredit,uint32_t &remainingDebt){
        if(!remainingDebt && (!cycles || remainingCredit==limit))return 0;
        uint32_t add=cycles;
        if(!reference && cycles){
            if(ratioSixteenths==64)add=cycles>=(limit>>2)?limit:cycles<<2;
            else if(ratioSixteenths==24)add=cycles>=(windowFrames==1?106667u:windowFrames==2?213334u:320000u)?limit:cycles+(cycles>>1);
            else if(cycles>=limit*16)add=limit;
            else add=(wordProduct(uint16_t(cycles),ratioSixteenths)>>4)+
                     (wordProduct(uint16_t(cycles>>16),ratioSixteenths)<<12);
        }
        remainingCredit=add>=limit-remainingCredit?limit:remainingCredit+add;
        uint32_t use=remainingCredit<remainingDebt?remainingCredit:remainingDebt;
        if(use>available)use=available;
        if(remainingDebt && !remainingCredit)++limited;
        remainingCredit-=use;remainingDebt-=use;available-=use;return use;
    }
    // One paused guest interval has up to three ordered contributions. Wall
    // time cannot change while the caller holds the interrupt mask. Publish
    // it once, then preserve each contribution's saturation/spending boundary:
    // summing the inputs first loses credit when a prior grant spent debt.
    __attribute__((always_inline)) inline uint32_t grantBatch(uint32_t guest,uint32_t nominal,uint32_t raw,bool running,
                        uint32_t now,uint32_t queued){
        if(!guest && !nominal && !running)return 0;
        const uint32_t limit=windowFrames==1?160000:windowFrames==2?320000:480000;
        uint32_t frames=now-frame;frame=now;
        uint32_t remainingDebt=debt,remainingCredit=credit;
        if(frames){
            discardedWall+=frames>windowFrames?frames-windowFrames:0;
            if(frames>=windowFrames)remainingDebt=limit;
            else {uint32_t add=frames==1?160000:320000;
                remainingDebt=add>=limit-remainingDebt?limit:remainingDebt+add;}
        }
        uint32_t available=queued>=frameCycles?0:frameCycles-queued,total=0;
        if(guest)total+=batchContribution(guest,false,limit,available,remainingCredit,remainingDebt);
        if(nominal)total+=batchContribution(nominal,true,limit,available,remainingCredit,remainingDebt);
        if(running)total+=batchContribution(raw,false,limit,available,remainingCredit,remainingDebt);
        credit=remainingCredit;debt=remainingDebt;return total;
    }

};
}
#endif
