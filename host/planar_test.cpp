#include "../src/board/PlanarSurface.h"
#include "../src/board/Hd63484.h"
#include <cstdio>
#include <algorithm>
#include <stdexcept>
#include <vector>
static void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
static void planarLines(){
    // Every octant/tie/alignment for lengths up to 16, including physical row
    // aliasing and address-mask wrap. The oracle works in linear pixels and
    // applies each ROP individually; the implementation steps word/mask pairs.
    pokeri::PlanarSurface p;std::vector<uint16_t> actual(1024),expected(1024);
    p.attach(actual.data(),actual.size());
    for(unsigned wordMask:{0u,255u})for(int stride:{0,8})
    for(int ex=-16;ex<=16;++ex)for(int ey=-16;ey<=16;++ey)
    for(unsigned align=0;align<16;++align)for(unsigned op=0;op<4;++op){
        std::fill(actual.begin(),actual.end(),0x596a);expected=actual;
        unsigned pixelMask=(wordMask<<4)|15,first=(128+align)&pixelMask,pixel=first;
        int dx=ex<0?-ex:ex,dy=ey<0?-ey:ey,sx=ex<0?-1:1,sy=ey<0?-1:1;
        int major=dx>dy?dx:dy,minor=dx>dy?dy:dx,error=2*minor-major;
        unsigned color=(align+op)&15;
        for(int n=0;n<major;++n){
            uint16_t mask=uint16_t(0x8000u>>(pixel&15));
            for(unsigned plane=0;plane<4;++plane){
                uint16_t &d=expected[plane*256+(pixel>>4)],v=color&(1<<plane)?mask:0;
                switch(op){case 0:d=(d&~mask)|v;break;case 1:d|=v;break;case 2:d&=uint16_t(~mask|v);break;case 3:d^=v;break;}
            }
            if(error>=0){pixel+=dx>dy?sy*stride*16:sx;error-=2*major;}
            pixel+=dx>dy?sx:sy*stride*16;error+=2*minor;pixel&=pixelMask;
        }
        check(p.line4(first,wordMask,sy*stride,dx,dy,sx,color,op),"planar line refused");
        check(actual==expected,"planar line octant/endpoint/ROP/wrap/alias differs");
    }
}
static void rotatedCopies(){
    pokeri::PlanarSurface p;std::vector<uint16_t> actual(1024),expected(1024),initial(1024);
    p.attach(actual.data(),actual.size());
    for(unsigned i=0;i<initial.size();++i)initial[i]=uint16_t((i*8461)^0xa659);
    for(unsigned so=0;so<16;++so)for(unsigned dest=0;dest<16;++dest)
    for(unsigned width:{1u,7u,16u,17u,31u})for(unsigned height:{1u,3u,17u})
    for(unsigned stride:{64u,67u,76u})for(unsigned op=0;op<4;++op){
        actual=expected=initial;unsigned from=so,to=2048+dest;
        for(unsigned plane=0;plane<4;++plane)for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
            unsigned src=from+(height-1-y)*stride+width-1-x,dst=to+y*stride+x;
            uint16_t mask=uint16_t(0x8000u>>(dst&15)),bits=(initial[plane*256+(src>>4)]>>(15-(src&15)))&1?mask:0;
            uint16_t &v=expected[plane*256+(dst>>4)];
            switch(op){case 0:v=(v&~mask)|bits;break;case 1:v|=bits;break;case 2:v&=uint16_t(~mask|bits);break;case 3:v^=bits;break;}
        }
        check(p.copy180(from,to,stride,width,height,op),"disjoint rotated copy refused");
        check(actual==expected,"rotated copy alignment/edge/ROP differs");
    }
    actual=initial;
    for(unsigned to:{100u,110u})check(!p.copy180(100,to,64,17,3,0),"overlapping rotation must keep sequential fallback");
    check(!p.copy180(4090,0,64,17,3,0) && actual==initial,"invalid rotation must preserve all storage");
}
int main()try{
    planarLines();rotatedCopies();
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
    // Short CPU spans: all alignments/lengths/ROPs, independently plotted into
    // packed words. Check neighbouring bits as well as the requested pixels.
    for(unsigned first=0;first<16;++first)for(unsigned width=1;width<=16;++width)
    for(unsigned op=0;op<4;++op)for(uint16_t color:{uint16_t(0x1234),uint16_t(0xabcd)}){
        uint16_t expected[12],masks[4];pokeri::Surface::colorPlanes4(color,masks);
        for(unsigned a=0;a<12;++a){expected[a]=0x5a69;planar.writeWord(a,expected[a]);}
        for(unsigned x=first;x<first+width;++x){
            unsigned a=x>>2,shift=(x&3)*4;uint16_t mask=15<<shift,src=color&mask;
            switch(op){case 0:expected[a]=(expected[a]&~mask)|src;break;
            case 1:expected[a]|=src;break;case 2:expected[a]&=uint16_t(~mask|src);break;case 3:expected[a]^=src;break;}
        }
        check(planar.span4(first,width,masks,op),"short planar span unexpectedly refused");
        for(unsigned a=0;a<12;++a)check(planar.readWord(a)==expected[a],"short planar span ROP or edge differs");
        uint16_t word[4];check(planar.readPlanes4(0,word),"planar word access refused");
        for(unsigned p=0;p<4;++p)check(word[p]==planes[p*planar.planeWords],"planar word access changes plane order");
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
