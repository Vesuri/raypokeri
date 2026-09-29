#include "../src/platform/amiga/PaulaPeriods.h"
#include "../src/board/Board.h"
#include "../src/board/AyEnvelope.h"
#include <cstdio>
#include <stdexcept>
static void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
struct Output:pokeri::AyBackend{
    unsigned count=0,reg=99,value=99;uint32_t cycles=0;
    void write(unsigned r,uint8_t v)override{++count;reg=r;value=v;}
    void tick(uint32_t n)override{cycles+=n;}
};
int main()try{
    uint16_t periods[4096];pokeri::paulaPeriods(periods);
    for(unsigned p=0;p<4096;++p){
        unsigned ay=p?p:1,n=ay>2300?8:2;
        uint32_t expected=uint64_t(3546895)*16*ay/(1000000*n);
        if(expected<124)expected=124;
        if(periods[p]!=expected)throw std::runtime_error("Paula period table changed");
    }

    pokeri::Ay38912 ay;Output output;ay.backend=&output;ay.clockHz=1000000;
    ay.write8(0,1);ay.write8(1,255);check(output.count==1 && output.reg==1 && output.value==15,"backend receives masked register writes");
    check(ay.read8(1)==15,"backend cannot alter CPU-visible readback");
    ay.tick(80000);check(output.cycles==80000 && ay.clockPhase==0,"backend replaces reference PCM work");
    for(unsigned shape=0;shape<16;++shape)for(unsigned initial: {0u,1u,15u,255u,65535u}){
        pokeri::Ay38912 reference;pokeri::AyEnvelope env;reference.clockHz=1000000;
        auto write=[&](unsigned r,unsigned v){reference.write8(0,r);reference.write8(1,v);};
        unsigned period=initial;write(11,period);write(12,period>>8);write(13,shape);env.restart(shape);
        for(unsigned tick=0;tick<140;++tick){
            if(tick==60){period=1;write(11,period);write(12,0);}
            if(tick==90){write(13,shape);env.restart(shape);}
            unsigned cycles=1+(tick*997)%160000;
            reference.tick(cycles);env.tick(cycles,period,shape);
            check(env.level()==(reference.envelopeStep^reference.envelopeAttack),"batched envelope differs from reference");
            check(env.hold==reference.envelopeHold,"envelope hold differs");
        }
    }
    // Double's descending envelope must finish after ten PAL updates even
    // when graphics leave the guest clock at only 100 ms.
    pokeri::AyEnvelope wall,board;wall.restart(9);board.restart(9);
    for(unsigned frame=0;frame<9;++frame)wall.tick(160000,768,9);
    check(wall.level()==1 && !wall.hold,"PAL decay before the last frame");
    wall.tick(160000,768,9);
    for(unsigned frame=0;frame<5;++frame)board.tick(160000,768,9);
    check(wall.level()==0 && wall.hold && board.level()==7,"PAL decay must not inherit guest slowdown");
    puts("PASS: backend masks/readback, renderer replacement, all envelope shapes and period rewrites");return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
