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
    for(unsigned trial=0;trial<10000;++trial){
        random=random*1664525+1013904223;unsigned a=random&255,b=(random>>8)&255;
        unsigned stride=16+((random>>16)&15),width=1+((random>>20)&15),height=1+((random>>24)&7);
        std::vector<bool> occupied(512,false);bool overlap=false;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)occupied[a+y*stride+x]=true;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)overlap|=occupied[b+y*stride+x];
        check(pokeri::PlanarSurface::rectanglesOverlap(a,b,stride,width,height)==overlap,"rectangle overlap differs from pixel occupancy");
    }
    // Independent pixel oracle: every source/destination alignment, clipped
    // edge words, blank windows, multiple rows and source-storage boundaries.
    for(unsigned so=0;so<16;++so)for(unsigned dx=0;dx<16;++dx)
    for(unsigned width: {1u,7u,16u,17u,31u})for(bool visible: {false,true}){
        for(uint32_t source: {so,uint32_t(0x100000-256+so)}){
            std::vector<uint16_t> actual(4*4*5,0xa55a),expected=actual;
            planar.displayRegion(actual.data(),16,4,dx,1,source,64,width,3,visible);
            for(unsigned y=0;y<3;++y)for(unsigned x=0;x<width;++x){
                unsigned bit=source+y*64+x;
                for(unsigned p=0;p<4;++p){
                    unsigned color=visible?((planes[p*0x10000+(bit>>4)]>>(15-(bit&15)))&1):0;
                    unsigned address=(y+1)*16+p*4+((dx+x)>>4);
                    uint16_t mask=uint16_t(0x8000u>>((dx+x)&15));
                    expected[address]=(expected[address]&~mask)|(color?mask:0);
                }
            }
            check(actual==expected,"shifted display rectangle differs from pixel reference");
        }
    }
    puts("PASS: direct planar storage matches packed bus readback and 100000 logical pixel operations");return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
