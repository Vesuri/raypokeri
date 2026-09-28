#include "native/StartupBudget.h"
#include "native/DelayBudget.h"
#include <cassert>
#include <initializer_list>
#include <cstdint>
#include <cstdio>
int main(){
    unsigned scales=0,budgets=0;
    for(unsigned ratio=1;ratio<=80;++ratio)for(unsigned cycles=0;cycles<0x100000;++cycles){
        assert(pokeri::startupWorkCycles(cycles,false,ratio)==(uint64_t(cycles)*ratio)/16);
        assert(pokeri::startupWorkCycles(cycles,true,ratio)==cycles);++scales;
    }
    for(unsigned phase=0;phase<8000;++phase)for(unsigned counter:{0u,1u,2u,3u,7424u,65535u}){
        unsigned available=pokeri::startupDelayAvailable(phase);
        unsigned steps=pokeri::delaySteps(counter,available),n=counter?counter:65536;
        unsigned cycles=(steps/2)*14+(steps&1?4:0)-(steps==2*n?2:0);
        assert(steps && steps<=2*n && cycles<=available);
        assert(phase+cycles<=8003);++budgets;
    }
    std::printf("PASS: %u calibrated startup scales and %u next-edge delay budgets\n",scales,budgets);
}
