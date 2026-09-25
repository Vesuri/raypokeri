#include "../src/board/Display.h"
#include <cstdio>
#include <stdexcept>
static void check(bool b,const char *message){if(!b)throw std::runtime_error(message);}
int main() try {
    pokeri::Hd63484 v;
    auto reg=[&](unsigned a,unsigned n){v.control[a]=n>>8;v.control[a+1]=n;};
    reg(2,0x200);reg(4,0x4028);reg(6,0xff00);reg(0x84,1);reg(0x92,1);
    reg(0x8c,2);reg(0x8a,3);reg(0x8e,1);reg(0x94,1);reg(0x96,2);
    for(unsigned s=0;s<4;++s){reg(0xc2+8*s,5);reg(0xc6+8*s,0x20+s*0x20);
        for(unsigned i=0;i<32;++i)v.frame[0x20+s*0x20+i]=(s+1)*0x1111;}
    v.frame[0x20]=0x4321;v.frame[0x80]=0;
    auto f=compose(v);
    check(f.width==16 && f.height==6,"split geometry, interleaved pixels per cycle");
    check(f.indices[0]==1 && f.indices[1]==2 && f.indices[3]==4,"LSB-first pixel packing");
    check(f.indices[16]==0 && f.indices[24]==4 && f.indices[32]==4 && f.indices[40]==4,"window priority including color zero and local stride");
    check(f.indices[80]==3,"lower screen starts at its own SAR");
    reg(0x84,7);reg(0x92,0x0202);f=compose(v);
    check(f.indices[64+31]==1 && f.indices[64+32]==0 && f.indices[64+36]==4 && f.indices[64+55]==4 && f.indices[64+56]==1,"odd interleaved window width delays start two cycles and retains width");
    reg(0x92,0x0201);f=compose(v);
    check(f.indices[64+15]==1 && f.indices[64+16]==0 && f.indices[64+20]==4 && f.indices[64+31]==4 && f.indices[64+32]==1,"even window width has no delay");
    reg(0x84,0x0407);reg(0x92,0x0102);f=compose(v);
    check(f.indices[64]==4 && f.indices[64+15]==4 && f.indices[64+16]==1,"delayed window clips at left edge using local source coordinates");
    reg(0x84,1);reg(0x92,1);
    reg(6,0x7c00);f=compose(v);check(f.height==6 && f.indices[32]==2,"DSP bit is not base-screen enable");
    reg(6,0x6c00);f=compose(v);check(f.height==6 && f.indices[0]==0 && f.indices[32]==2,"blank upper retains height");
    reg(6,0x4c00);f=compose(v);check(f.height==4 && f.indices[0]==2,"disabled upper removes its height");
    reg(4,0x28);check(compose(v).indices.empty(),"stopped display emits no frame");
    puts("PASS: display packing, screen strides, split enable/blanking, window priority, display stop");return 0;
}catch(const std::exception &e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
