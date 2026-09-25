#include "../src/board/PlanarSurface.h"
#include "../src/board/Hd63484.h"
#include <cstdio>
#include <stdexcept>
#include <vector>
static void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
int main()try{
    pokeri::PlanarSurface planar;std::vector<uint16_t> planes(0x40000);
    planar.attach(planes.data(),0x40000);uint32_t random=1;
    // Every packed word and each nibble position must agree with the
    // independent per-pixel accessor, not merely round-trip through a table.
    for(unsigned value=0;value<65536;++value)for(unsigned a=0;a<4;++a){
        planar.writeWord(a,value);
        check(planar.readWord(a)==value,"exhaustive packed word readback differs");
        for(unsigned x=0;x<4;++x)check(planar.pixel4(a,x*4)==((value>>(x*4))&15),"packed conversion changes pixel order");
    }
    std::vector<uint16_t> packed(0x40000);
    for(unsigned a=0;a<packed.size();++a){random=random*1664525+1013904223;packed[a]=random>>16;planar.writeWord(a,packed[a]);}
    for(unsigned a=0;a<packed.size();++a)check(planar.readWord(a)==packed[a],"packed bus readback differs");
    for(unsigned i=0;i<100000;++i){random=random*1664525+1013904223;unsigned a=random&0x3ffff,shift=((random>>18)&3)*4,op=(random>>20)&3,color=(random>>22)&15;
        uint16_t mask=15<<shift,src=color<<shift;
        switch(op){case 0:packed[a]=(packed[a]&~mask)|src;break;case 1:packed[a]|=src;break;case 2:packed[a]&=uint16_t(~mask|src);break;case 3:packed[a]^=src;break;}
        planar.plot4(a,shift,color,op);check(planar.readWord(a)==packed[a],"planar logical operation differs");
        check(planar.pixel4(a,shift)==((packed[a]>>shift)&15),"planar pixel readback differs");
    }
    puts("PASS: direct planar storage matches packed bus readback and 100000 logical pixel operations");return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
