#include "native/IrqCache.h"
#include "board/Board.h"
#include <cassert>
#include <cstdio>
using namespace pokeri;
int main(){
    Board b;IrqCache cache;
    auto check=[&](){assert(cache.level(b,b.video.statusNow())==b.irq());};
    auto write=[&](unsigned a,unsigned v){cache.beforeByte<true>(a);b.write8(a,v);check();};
    auto read=[&](unsigned a){cache.beforeByte<false>(a);auto v=b.read8(a);check();return v;};
    for(unsigned a=0xfb000;a<0xfb024;++a)for(unsigned n=1;n<=4;++n){
        bool changed=false;for(unsigned i=0;i<n;++i)changed|=(a+i>=0xfb002&&a+i<=0xfb003)||(a+i>=0xfb014&&a+i<=0xfb017);
        cache.state=3;cache.beforeAccess(a,n);assert(cache.state==(changed?0:3));
    }
    for(unsigned a=0xfb000;a<0xfb024;++a){
        cache.state=3;cache.beforeByte<true>(a);
        assert(cache.state==((a==0xfb002||a==0xfb015||a==0xfb017)?0:3));
        cache.state=3;cache.beforeByte<false>(a);
        assert(cache.state==((a==0xfb003||a==0xfb014||a==0xfb016)?0:3));
    }
    unsigned seed=0x846cd35;auto random=[&](){return seed=seed*1664525+1013904223;};
    for(unsigned i=0;i<200000;++i){
        cache.invalidate();
        for(unsigned side=0;side<2;++side){b.pia[0].control[side]=random()&63;b.pia[0].flags[side]=random()&0xc0;}
        b.serial[0].control=random();b.serial[0].receive.clear();if(i&1)b.serial[0].receive.push_back(0x55);
        b.video.control[3]=random();b.video.status=random();check();
        // Video mutations must be seen without invalidating peripheral state.
        auto state=cache.state;b.video.control[3]=random();b.video.status=random();b.video.presentationBusy=i&2;check();assert(cache.state==state);
        write(0xfb015,random()&63);write(0xfb017,random()&63);
        auto retained=cache.state;
        write(0xfb014,random());write(0xfb016,random()&0x7f);
        read(0xfb015);read(0xfb017);assert(cache.state==retained);
        read(0xfb014);read(0xfb016); // PIA flags acknowledged when data selected
        write(0xfb002,0x83);write(0xfb002,0x95); // serial reset then RX IRQ enable
        cache.invalidate();b.serial[0].receive.push_back(0x42);check(); // external RX
        retained=cache.state;read(0xfb002);write(0xfb003,random());
        assert(cache.state==retained);
        read(0xfb003); // status, then consume
        cache.invalidate();b.config.systemHz=100;b.config.inputHz=50;b.tick(80000);check();
        cache.invalidate();b.reset();check();
    }
    assert(!b.fault);
    puts("PASS 200000 IRQ-cache transaction sets: PIA acknowledgments/control, ACIA reset/RX/read, timer ticks/reset, and fresh video state");
}
