// Independent due-edge simulation for startup batching, including unaligned kicks.
#include "../src/native/StartupBudget.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
static unsigned reference(unsigned cabinet,uint32_t system,uint32_t input,uint64_t age,
                          uint64_t warning,uint64_t reset,bool warn,bool resets){
    if(cabinet>=10 || system>=8000000 || input>=8000000)return 1;
    for(unsigned tick=1;;++tick){
        bool edge=false;
        const uint64_t next=age+8000;
        if(warn && age<warning && next>=warning)edge=true;
        if(resets && next>=reset)edge=true;
        age=next;
        const uint64_t sys=uint64_t(system)+uint64_t(8000)*100;
        const uint64_t in=uint64_t(input)+uint64_t(8000)*50;
        if(sys/8000000 || in/8000000)edge=true;
        system=uint32_t(sys%8000000);input=uint32_t(in%8000000);
        if(++cabinet==10 || edge)return tick;
    }
}
int main(){
    uint32_t rng=7;unsigned cases=0;
    auto next=[&](){rng=rng*1664525u+1013904223u;return rng;};
    auto check=[&](unsigned cabinet,uint32_t system,uint32_t input,uint64_t age,
                   uint64_t warning,uint64_t reset,bool warn,bool resets){
        unsigned n=pokeri::startupQuietTicks(cabinet,system,input,age,warning,reset,warn,resets);
        assert(n==reference(cabinet,system,input,age,warning,reset,warn,resets));
        assert(n>=1 && n<=10);++cases;
    };
    for(unsigned cabinet=0;cabinet<10;++cabinet)for(unsigned sys=0;sys<10;++sys)
    for(unsigned input=0;input<20;++input)for(unsigned delta=0;delta<8001;delta+=1){
        check(cabinet,sys*800000,input*400000,800000-delta,800000,800800,true,true);
    }
    for(unsigned trial=0;trial<100000;++trial){
        uint64_t age=(uint64_t(next())<<32)|next();
        uint64_t warning=age+(next()%100000),reset=warning+(next()%16001);
        check(next()%12,next()%8000001,next()%8000001,age,warning,reset,next()&1,next()&1);
    }
    check(0,0,0,UINT64_MAX-3999,1000,5000,true,false);
    std::printf("PASS: %u startup quiet horizons; periodic phases, unaligned kicks, warning/reset, cabinet cuts and fallback bounds\n",cases);
}
