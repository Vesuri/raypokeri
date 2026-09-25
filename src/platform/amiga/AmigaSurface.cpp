#include "AmigaSurface.h"
#include <initializer_list>
#include "AmigaHardware.h"
#include <proto/exec.h>
#include <exec/memory.h>
bool AmigaSurface::prepare(){attach((uint16_t*)AllocMem(0x80000,MEMF_CHIP|MEMF_CLEAR),0x40000);return data!=nullptr;}
void AmigaSurface::synchronize()const{if(pending){AmigaHardware::blitterDrain();pending=false;}}
uint16_t AmigaSurface::readWord(uint32_t a)const{synchronize();return PlanarSurface::readWord(a);}
void AmigaSurface::writeWord(uint32_t a,uint16_t value){synchronize();PlanarSurface::writeWord(a,value);}
uint16_t AmigaSurface::pixel4(uint32_t a,unsigned shift)const{synchronize();return PlanarSurface::pixel4(a,shift);}
void AmigaSurface::plot4(uint32_t a,unsigned shift,unsigned color,unsigned op){synchronize();PlanarSurface::plot4(a,shift,color,op);}
void AmigaSurface::release(){synchronize();if(data)FreeMem(data,0x80000);data=nullptr;}
bool AmigaSurface::fits(uint32_t first,unsigned stride,unsigned width,unsigned height)const{
    return width && height && height<=1023 && stride && !(stride&15) && width<=stride &&
        ((first&15)+width+15)/16<=64 && first<0x100000 &&
        first+uint32_t(uint16_t(height-1))*uint16_t(stride)+width<=0x100000;
}
static unsigned minterm(unsigned op){static const uint8_t table[]={0xca,0xea,0x8a,0x6a};return table[op&3];}
bool AmigaSurface::fill(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t pattern,unsigned op){
    if(!fits(first,stride,width,height))return false;
    unsigned count=((first&15)+width+15)>>4,tail=(first+width)&15;
    uint16_t firstMask=uint16_t(0xffffu>>(first&15)),lastMask=tail?uint16_t(0xffffu<<(16-tail)):0xffff;
    uint32_t address=first>>4;
    for(unsigned p=0;p<4;++p){
        unsigned nibble=0;for(unsigned x=0;x<4;++x)nibble=(nibble<<1)|((pattern>>(x*4+p))&1);
        uint32_t dest=uint32_t(data+address);unsigned modulo=(stride>>3)-(count<<1);
        const uint16_t pairs[]={bltcon0,uint16_t(0x300|minterm(op)),bltcon1,0,
            bltafwm,firstMask,bltalwm,lastMask,bltadat,0xffff,
            bltbdat,uint16_t(uint16_t(nibble)*uint16_t(0x1111)),
            bltcmod,uint16_t(modulo),bltdmod,uint16_t(modulo),
            bltcpth,uint16_t(dest>>16),bltcptl,uint16_t(dest),
            bltdpth,uint16_t(dest>>16),bltdptl,uint16_t(dest),
            bltsize,uint16_t((height<<6)|(count&63))};
        AmigaHardware::blitterSubmit(pairs,13);address+=planeWords;
    }
    queued();changed=true;++fills;return true;
}
bool AmigaSurface::copy(uint32_t from,uint32_t to,unsigned stride,unsigned width,unsigned height,unsigned op){
    if(!fits(from,stride,width,height) || !fits(to,stride,width,height) || ((from^to)&15))return false;
    uint32_t span=uint32_t(uint16_t(height-1))*uint16_t(stride)+width;
    // Preserve the ACRTC's sequential overlap semantics through the shared
    // planar pixel path when a block transfer could change the read order.
    if(from<to+span && to<from+span)return false;
    unsigned count=((to&15)+width+15)>>4,tail=(to+width)&15;
    uint16_t firstMask=uint16_t(0xffffu>>(to&15)),lastMask=tail?uint16_t(0xffffu<<(16-tail)):0xffff;
    from>>=4;to>>=4;
    for(unsigned p=0;p<4;++p){
        uint32_t source=uint32_t(data+from),dest=uint32_t(data+to);unsigned modulo=(stride>>3)-(count<<1);
        const uint16_t pairs[]={bltcon0,uint16_t(0x700|minterm(op)),bltcon1,0,
            bltafwm,firstMask,bltalwm,lastMask,bltadat,0xffff,
            bltbmod,uint16_t(modulo),bltcmod,uint16_t(modulo),bltdmod,uint16_t(modulo),
            bltbpth,uint16_t(source>>16),bltbptl,uint16_t(source),
            bltcpth,uint16_t(dest>>16),bltcptl,uint16_t(dest),
            bltdpth,uint16_t(dest>>16),bltdptl,uint16_t(dest),
            bltsize,uint16_t((height<<6)|(count&63))};
        AmigaHardware::blitterSubmit(pairs,15);from+=planeWords;to+=planeWords;
    }
    queued();changed=true;++copies;return true;
}

bool AmigaSurface::selfTest(){
    // Exercise the real Agnus masks/minterms against an independent packed
    // reference before any game VRAM access. Includes partial edge words.
    uint16_t *expected=new uint16_t[1024];if(!expected)return false;
    bool ok=true;
    for(unsigned op=0;op<4 && ok;++op)for(unsigned offset: {0u,1u,4u,15u}){
        for(unsigned a=0;a<1024;++a){expected[a]=uint16_t(a^0xa569);writeWord(a,expected[a]);}
        unsigned first=offset,width=37,height=3,stride=608;
        if(!fill(first,stride,width,height,0xac39,op)){ok=false;break;}
        auto plot=[&](unsigned pixel,unsigned color){unsigned a=pixel>>2,shift=(pixel&3)*4;uint16_t mask=15<<shift,bits=color<<shift;
            switch(op){case 0:expected[a]=(expected[a]&~mask)|bits;break;case 1:expected[a]|=bits;break;case 2:expected[a]&=uint16_t(~mask|bits);break;case 3:expected[a]^=bits;break;}};
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){unsigned pixel=first+y*608+x;plot(pixel,(0xac39>>((pixel&3)*4))&15);}
        if(!copy(first,2048+offset,stride,width,2,op)){ok=false;break;}
        for(unsigned y=0;y<2;++y)for(unsigned x=0;x<width;++x){unsigned pixel=first+y*608+x;unsigned color=(expected[pixel>>2]>>((pixel&3)*4))&15;plot(2048+offset+y*608+x,color);}
        for(unsigned a=0;a<1024;++a)if(readWord(a)!=expected[a]){ok=false;break;}
    }
    // More submissions than the ring can hold while completion interrupts
    // are masked: exercise wrap/backpressure without overwriting live records.
    // Shift each tall fill down one row. Every submission leaves its own
    // retained row, so a dropped or reordered queue entry cannot be hidden
    // by the final fill overwriting all earlier results.
    for(unsigned n=0;n<512 && ok;++n)ok=fill(n*16,16,16,1023,uint16_t(n),0);
    synchronize();
    if(ok)for(unsigned a=0;a<6136;++a){
        unsigned row=a>>2;uint16_t value=row<512?row:511;
        if(readWord(a)!=value){ok=false;break;}
    }
    // Then let the actual BLIT handler drain a batch while the CPU is free.
    if(ok){
        fill(0,16,16,1023,0xa35c,0);
        uint16_t savedSr;asm volatile("move.w %%sr,%0\n\tmove.w #0x2000,%%sr":"=d"(savedSr)::"cc","memory");
        while(!AmigaHardware::blitterIdle())asm volatile("nop":::"memory");
        asm volatile("move.w %0,%%sr"::"d"(savedSr):"cc","memory");
        synchronize();
        if(readWord(4091)!=0xa35c)ok=false;
    }
    delete[] expected;
    for(uint32_t i=0;i<0x40000;++i)data[i]=0;
    changed=true;fills=copies=0;tested=ok;return ok;
}
