#ifndef POKERI_AY_ENVELOPE_H
#define POKERI_AY_ENVELOPE_H
#include <cstdint>
namespace pokeri {
// AY envelope control for the 8 MHz CPU / 1 MHz AY live profile. Calls are
// bounded to one PAL frame (160000 cycles). No oscillator/sample iteration.
struct AyEnvelope {
    uint32_t phase=0,count=0;
    uint8_t step=15,attack=0;
    bool hold=false;
    void restart(unsigned shape){count=0;step=15;attack=(shape&4)?15:0;hold=false;}
    unsigned level()const{return step^attack;}
    void tick(uint32_t cycles,unsigned period,unsigned shape){
        phase+=cycles;uint32_t steps=phase>>6;phase&=63;
        if(hold || !steps)return;
        period*=2;if(!period)period=1;
        // A period rewrite can put the old counter above the new limit. The
        // reference resets it on the next divider edge; it does not catch up.
        uint32_t wait=count<period?period-count:1;
        if(steps<wait){count+=steps;return;}
        steps-=wait;uint32_t advances=1;
        if(steps<period)count=steps;
        else {
#ifdef __m68k__
            uint16_t divisor=period;
            __asm volatile("divu.w %1,%0" : "+d"(steps) : "d"(divisor) : "cc");
            advances+=uint16_t(steps);count=steps>>16;
#else
            advances+=steps/period;count=steps%period;
#endif
        }
        while(advances){
            unsigned n=step+1;
            if(advances<n){step-=advances;break;}
            advances-=n;
            if(!(shape&8)){hold=true;attack=0;step=0;count=0;break;}
            if(shape&1){if(shape&2)attack^=15;hold=true;step=0;count=0;break;}
            if(shape&2)attack^=15;
            step=15;if(advances>=32)advances&=31;
        }
    }
};
}
#endif
