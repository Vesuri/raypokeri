#include "native/LiveClock.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace pokeri;
// Wide host oracle for the pre-optimization policy, independent of word math
// and the common-ratio / repeated-boundary shortcuts.
static uint32_t referenceGrant(LiveClock &c,uint32_t cycles,bool reference,uint32_t now,uint32_t queued){
    uint32_t frames=now-c.frame;c.frame=now;
    if(frames){c.discardedWall+=frames>1?frames-1:0;c.debt=160000;}
    uint32_t add=cycles;
    if(!reference && cycles)add=cycles>=2560000?160000:uint32_t(uint64_t(cycles)*c.ratioSixteenths/16);
    c.credit=add>=160000-c.credit?160000:c.credit+add;
    uint32_t use=c.credit<c.debt?c.credit:c.debt;
    uint32_t available=queued>=160000?0:160000-queued;
    if(use>available)use=available;
    if(c.debt && !c.credit)++c.limited;
    c.credit-=use;c.debt-=use;return use;
}
static void equivalentGrants(){
    uint32_t seed=0x89675432;
    auto random=[&](){return seed=seed*1664525+1013904223;};
    const unsigned values[]={0,1,3,40000,65535,65536,106666,106667,160000,2559999,2560000,0xffffffffu};
    for(unsigned n=0;n<2000000;++n){
        LiveClock a;
        a.credit=random()%160001;a.debt=n&1?0:random()%160001;
        a.frame=random();a.limited=random();a.discardedWall=random();
        a.ratioSixteenths=n%3==0?64:n%3==1?24:random()%81;
        LiveClock b=a;
        for(unsigned k=0;k<4;++k){
            unsigned now=a.frame+(random()%8==0?random()%5:0),cycles=k&1?values[random()%12]:random();
            bool reference=random()&1;unsigned queued=random()%480001;
            assert(a.grant(cycles,reference,now,queued)==referenceGrant(b,cycles,reference,now,queued));
            assert(a.credit==b.credit && a.debt==b.debt && a.frame==b.frame &&
                   a.limited==b.limited && a.discardedWall==b.discardedWall);
        }
    }
    puts("PASS 8000000 exact clock state transitions against the prior policy");
}
int main(){
    equivalentGrants();
    for(unsigned ticks=0;ticks<=65535;++ticks){
        uint32_t got=boardClockCycles(ticks);
        assert(got==(uint64_t(ticks)*361>>5));
        double exact=ticks*8000000.0/709379;
        assert(got<=exact*1.00034 && got+1>=exact);
    }
    for(unsigned ratio: {1u,8u,16u,24u,32u,37u,64u,80u}){
        LiveClock clock;clock.ratioSixteenths=ratio;clock.reset(0);
        uint64_t guest=0,board=0;
        for(unsigned frame=1;frame<=3000;++frame){
            // Alternating idle and services which exceed the guest budget.
            unsigned cycles=frame%100<50?100000:10000;
            guest+=cycles;
            auto advanced=clock.grant(cycles,false,frame,0);board+=advanced;
            assert(advanced<=160000 && clock.credit<=160000 && clock.debt<=160000);
            assert(board<=guest*ratio/16 && board<=uint64_t(frame)*160000);
        }
        // Stalled service: no runaway wall debt or catch-up interrupt burst.
        assert(clock.grant(0,false,1000000,160000)==0);
        assert(clock.debt==160000);
        assert(clock.grant(0,false,1000000,160000)==0);
    }
    // VBI entry pauses/charges the guest before the callback publishes the
    // new frame. Its return trace must poll with zero new guest cycles.
    LiveClock boundary;boundary.reset(0);
    assert(boundary.grant(120000,false,0,0)==0);
    assert(boundary.grant(0,false,1,0)==160000);
    assert(boundary.grant(0,false,1,0)==0); // no duplicated frame/debt
    // Pending board ticks can block credit spending. Draining the queue does
    // not require fabricating another guest interval to spend existing credit.
    assert(boundary.grant(120000,false,2,160000)==0);
    assert(boundary.grant(0,false,2,80000)==80000);
    assert(boundary.grant(0,false,2,0)==80000);
    assert(boundary.grant(0,false,2,0)==0);
    LiveClock exact;exact.ratioSixteenths=37;exact.reset(0);
    assert(exact.grant(34,true,1,0)==34); // audited polls are never multiplied
    assert(exact.grant(26,true,1,0)==26);
    LiveClock wrap;wrap.reset(0xffffffffu);assert(wrap.grant(160000,true,0,0)==160000);
    LiveClock saturated;saturated.reset(0);assert(saturated.grant(0xffffffffu,false,1,0)==160000);
    puts("PASS live clock units, throughput limits, bounded stalls, reference polls and frame wrap");
}
