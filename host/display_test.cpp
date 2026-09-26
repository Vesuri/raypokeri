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
    check(f.indices[64+31]==1 && f.indices[64+32]==0 && f.indices[64+36]==4 && f.indices[64+55]==4 && f.indices[64+63]==4,"odd interleaved window retains the complete final fetch");
    reg(0x92,0x0201);f=compose(v);
    check(f.indices[64+15]==1 && f.indices[64+16]==0 && f.indices[64+20]==4 && f.indices[64+31]==4 && f.indices[64+32]==1,"even window width has no delay");
    reg(0x84,0x0407);reg(0x92,0x0102);f=compose(v);
    check(f.indices[64]==4 && f.indices[64+23]==4 && f.indices[64+24]==1,"delayed window clips at left edge using local source coordinates");
    reg(0x84,1);reg(0x92,1);
    reg(6,0x7c00);f=compose(v);check(f.height==6 && f.indices[32]==2,"DSP bit is not base-screen enable");
    reg(6,0x6c00);f=compose(v);check(f.height==6 && f.indices[0]==0 && f.indices[32]==2,"blank upper retains height");
    reg(6,0x4c00);f=compose(v);check(f.height==4 && f.indices[0]==2,"disabled upper removes its height");
    reg(4,0x28);check(compose(v).indices.empty(),"stopped display emits no frame");
    // Real register geometry, synthetic artwork: eight padding pixels then
    // an 88-pixel card. The last half-fetch contains its right border.
    reg(4,0xcd28);reg(6,0xc300);reg(0x84,0x0947);reg(0x8a,2);
    reg(0x88,0x0600);reg(0x94,6);reg(0x96,1);reg(0x92,0x0f0a);
    reg(0xca,152);reg(0xce,0x1000);reg(0xda,152);reg(0xde,0x2000);
    for(unsigned i=0;i<152*2;++i)v.frame[0x1000+i]=0x2222;
    for(unsigned i=0;i<24;++i)v.frame[0x2000+i]=i<2?0:i==23?0xffff:0x4444;
    f=compose(v);
    check(f.indices[63]==2 && f.indices[64]==0 && f.indices[71]==0 && f.indices[72]==4,"card padding and left alignment");
    check(f.indices[151]==4 && f.indices[152]==4 && f.indices[156]==15 && f.indices[159]==15 && f.indices[160]==2,"moving card retains its right border and stops after the last fetch");
    reg(0x92,0x4b0a);f=compose(v);
    check(f.indices[543]==2 && f.indices[544]==0 && f.indices[552]==4 && f.indices[575]==4,"window clips at the frame right edge");
    reg(0x92,0x0f0b);f=compose(v);
    check(f.indices[47]==2 && f.indices[48]==0 && f.indices[140]==15 && f.indices[143]==15 && f.indices[144]==2,"even width retains its position and full fetch count");
    puts("PASS: display packing, screen strides, split enable/blanking, window priority, display stop");return 0;
}catch(const std::exception &e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
