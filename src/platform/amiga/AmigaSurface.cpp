#include "AmigaSurface.h"
#include "board/WordMath.h"
#include <initializer_list>
#include "AmigaHardware.h"
#include <proto/exec.h>
#include <exec/memory.h>
#include <hardware/intbits.h>
bool AmigaSurface::prepare(){
    attach((uint16_t*)AllocMem(0x80000,MEMF_CHIP|MEMF_CLEAR),0x40000);
    patternData=(uint16_t*)AllocMem(cacheSize*320,MEMF_CHIP);
    copyMasks=(uint16_t*)AllocMem(16*66*2,MEMF_CHIP);
    if(copyMasks)for(unsigned offset=0;offset<16;++offset)for(unsigned i=0;i<66;++i)
        copyMasks[offset*66+i]=i==1?uint16_t(0xffffu>>offset):0xffff;
    if(!data || !patternData || !copyMasks){release();return false;}return true;
}
void AmigaSurface::synchronize()const{if(pending){AmigaHardware::blitterDrain();pending=false;}}
bool AmigaSurface::readPlanes4(uint32_t a,uint16_t *planes)const{synchronize();return PlanarSurface::readPlanes4(a,planes);}
bool AmigaSurface::copy180(uint32_t from,uint32_t to,unsigned stride,unsigned width,unsigned height,unsigned op){
    synchronize();
    if(!PlanarSurface::copy180(from,to,stride,width,height,op))return false;
    ++copies;return true;
}
bool AmigaSurface::line4(uint32_t first,uint32_t mask,int rowStep,int dx,int dy,int sx,unsigned color,unsigned op){
    synchronize();return PlanarSurface::line4(first,mask,rowStep,dx,dy,sx,color,op);
}
bool AmigaSurface::curve4(uint32_t base,uint32_t mask,unsigned rowWords,const pokeri::CurveWord *runs,unsigned count,uint16_t color,unsigned op){
    synchronize();return PlanarSurface::curve4(base,mask,rowWords,runs,count,color,op);
}
bool AmigaSurface::span4(uint32_t first,unsigned width,const uint16_t *colors,unsigned op){synchronize();return PlanarSurface::span4(first,width,colors,op);}
uint16_t AmigaSurface::readWord(uint32_t a)const{synchronize();return PlanarSurface::readWord(a);}
void AmigaSurface::writeWord(uint32_t a,uint16_t value){synchronize();PlanarSurface::writeWord(a,value);}
uint16_t AmigaSurface::pixel4(uint32_t a,unsigned shift)const{synchronize();return PlanarSurface::pixel4(a,shift);}
void AmigaSurface::plot4(uint32_t a,unsigned shift,unsigned color,unsigned op){synchronize();PlanarSurface::plot4(a,shift,color,op);}
void AmigaSurface::release(){synchronize();if(data)FreeMem(data,0x80000);data=nullptr;if(patternData)FreeMem(patternData,cacheSize*320);patternData=nullptr;if(copyMasks)FreeMem(copyMasks,16*66*2);copyMasks=nullptr;patternCount=patternNext=0;}
bool AmigaSurface::fits(uint32_t first,unsigned stride,unsigned width,unsigned height)const{
    return width && height && height<=65536 && stride && stride<=65535 && !(stride&15) && width<=stride &&
        ((first&15)+width+15)/16<=64 && first<0x100000 &&
        pokeri::wordProduct(uint16_t(height-1),uint16_t(stride))+width<=0x100000-first;
}
static unsigned minterm(unsigned op){static const uint8_t table[]={0xca,0xea,0x8a,0x6a};return table[op&3];}
bool AmigaSurface::fill(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t pattern,unsigned op){
    // A replace fill wider than its pitch covers one continuous interval.
    // The ACRTC boot CLR deliberately overlaps adjacent rows by one word.
    if(op==0 && width>stride && height && height<=65536 && stride && stride<=65535 && !(stride&15)){
        uint32_t rowsSpan=pokeri::wordProduct(uint16_t(height-1),uint16_t(stride));
        if(first>=0x100000 || rowsSpan>0x100000-first || width>0x100000-first-rowsSpan)return false;
        uint32_t pixels=rowsSpan+width;
        while(pixels){
            unsigned n,rows=1;
            if(first&15)n=16-(first&15);
            else if(pixels>=1024){n=1024;rows=pixels>>10;if(rows>1023)rows=1023;}
            else n=pixels;
            if(n>pixels)n=pixels;
            if(!fill(first,1024,n,rows,pattern,op))return false;
            uint32_t done=uint32_t(uint16_t(n))*uint16_t(rows);first+=done;pixels-=done;
        }
        return true;
    }
    if(!fits(first,stride,width,height))return false;
    // Short edges/spans cost less as masked CPU words than four blit setups.
    // Do not drain older work to take this shortcut: keep queued rectangles
    // asynchronous. CPU access is profitable only with no pending DMA.
    if(!pending && height<=16 && pokeri::wordProduct(uint16_t(((first&15)+width+15)>>4),uint16_t(height))<=16){
        synchronize();
        if(PlanarSurface::smallFill4(first,stride,width,height,pattern,op)){++fills;return true;}
    }
    if(height>1023){
        while(height){unsigned rows=height>1023?1023:height;
            if(!fill(first,stride,width,rows,pattern,op))return false;
            first+=uint32_t(uint16_t(rows))*uint16_t(stride);height-=rows;}
        return true;
    }
    unsigned count=((first&15)+width+15)>>4,tail=(first+width)&15;
    uint16_t firstMask=uint16_t(0xffffu>>(first&15)),lastMask=tail?uint16_t(0xffffu<<(16-tail)):0xffff;
    uint32_t address=first>>4;
    for(unsigned p=0;p<4;++p){
        unsigned nibble=0;for(unsigned x=0;x<4;++x)nibble=(nibble<<1)|((pattern>>(x*4+p))&1);
        uint32_t dest=uint32_t(data+address);unsigned modulo=(stride>>3)-(count<<1);
        if(op==0 && firstMask==0xffff && lastMask==0xffff){
            // Every bit is replaced. D-only with constant A avoids reading C
            // or a mask; preserve the same four-pixel repeating fill pattern.
            const uint16_t pairs[]={bltcon0,0x1f0,bltcon1,0,
                bltafwm,0xffff,bltalwm,0xffff,
                bltadat,uint16_t(uint16_t(nibble)*uint16_t(0x1111)),
                bltdmod,uint16_t(modulo),bltdpth,uint16_t(dest>>16),bltdptl,uint16_t(dest),
                bltsize,uint16_t((height<<6)|(count&63))};
            AmigaHardware::blitterSubmit(pairs,9);address+=planeWords;continue;
        }
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
bool AmigaSurface::patternTile(uint32_t first,unsigned stride,const pokeri::PatternTile &tile,unsigned op){
    if(!tile.valid() || op>3 || tile.offset!=(first&15) || !fits(first,stride,tile.width,tile.height))return false;
    unsigned slot=0;
    while(slot<patternCount && !(patternKeys[slot]==tile))++slot;
    if(slot<patternCount)++patternHits;
    else {
        ++patternMisses;
        if(patternCount<cacheSize)slot=patternCount++;
        else {slot=patternNext;patternNext=(patternNext+1)&(cacheSize-1);synchronize();}
        patternKeys[slot]=tile;tile.expand(patternData+slot*160);
    }
    unsigned count=(tile.offset+tile.width+15)>>4;
    uint32_t mask=uint32_t(patternData+slot*160),address=first>>4;
    for(unsigned p=0;p<4;++p){
        uint32_t source=mask+(p+1)*64,dest=uint32_t(data+address);
        const uint16_t pairs[]={bltcon0,uint16_t(0xf00|minterm(op)),bltcon1,0,
            bltafwm,0xffff,bltalwm,0xffff,
            bltamod,uint16_t(4-count*2),bltbmod,uint16_t(4-count*2),
            bltcmod,uint16_t((stride>>3)-count*2),bltdmod,uint16_t((stride>>3)-count*2),
            bltapth,uint16_t(mask>>16),bltaptl,uint16_t(mask),
            bltbpth,uint16_t(source>>16),bltbptl,uint16_t(source),
            bltcpth,uint16_t(dest>>16),bltcptl,uint16_t(dest),
            bltdpth,uint16_t(dest>>16),bltdptl,uint16_t(dest),
            bltsize,uint16_t((tile.height<<6)|count)};
        AmigaHardware::blitterSubmit(pairs,17);address+=planeWords;
    }
    queued();changed=true;return true;
}
bool AmigaSurface::blitPlanes(uint32_t source,unsigned stride,uint16_t *dest,uint16_t *begin,uint16_t *end,unsigned destStride,unsigned destPlane,unsigned offset,unsigned width,unsigned height,unsigned op,bool visible){
    if(!width || !height || height>1023 || op>3 || offset>15 || (stride&15))return false;
    unsigned count=(offset+width+15)>>4,tail=(offset+width)&15;
    if(count>63)return false;
    unsigned sourceOffset=source&15;
    bool prefetch=visible && sourceOffset>offset;
    unsigned words=count+unsigned(prefetch),shift=(offset-sourceOffset)&15;
    uint32_t first=source>>4;
    // A zero-mask leading column supplies the B shifter's previous word.
    // It writes C back unchanged. No prefetch may escape either allocation.
    if(prefetch){if(dest<=begin)return false;--dest;}
    uint32_t sourceLast=first+uint32_t(uint16_t(height-1))*uint16_t(stride>>4)+words;
    if(visible && sourceLast>planeWords)return false;
    if(dest<begin || dest+3*destPlane+uint32_t(uint16_t(height-1))*uint16_t(destStride)+words>end)return false;
    uint16_t lastMask=tail?uint16_t(0xffffu<<(16-tail)):0xffff;
    uint16_t *mask=copyMasks+offset*66+(prefetch?0:1);
    uint16_t *sourcePlane=data+first;
    for(unsigned p=0;p<4;++p,sourcePlane+=planeWords,dest+=destPlane){
        uint32_t src=uint32_t(sourcePlane),dst=uint32_t(dest),a=uint32_t(mask);
        if(op==0 && offset==0 && !(width&15) && (!visible || sourceOffset==0)){
            // Full replacement words need neither an A-mask stream nor C
            // reads. Use A->D for an aligned source, D-only for blanking.
            const uint16_t pairs[]={bltcon0,uint16_t(visible?0x9f0:0x100),bltcon1,0,
                bltafwm,0xffff,bltalwm,0xffff,
                bltamod,uint16_t((stride>>3)-words*2),bltdmod,uint16_t(destStride*2-words*2),
                bltapth,uint16_t(src>>16),bltaptl,uint16_t(src),
                bltdpth,uint16_t(dst>>16),bltdptl,uint16_t(dst),
                bltsize,uint16_t((height<<6)|(words&63))};
            AmigaHardware::blitterSubmit(pairs,11);continue;
        }
        const uint16_t pairs[]={bltcon0,uint16_t((visible?0xf00:0xb00)|(visible?minterm(op):0x0a)),bltcon1,uint16_t(visible?shift<<12:0),
            bltafwm,uint16_t(prefetch?0:0xffff),bltalwm,lastMask,
            bltamod,uint16_t(-int(words*2)),bltbmod,uint16_t((stride>>3)-words*2),
            bltcmod,uint16_t(destStride*2-words*2),bltdmod,uint16_t(destStride*2-words*2),
            bltapth,uint16_t(a>>16),bltaptl,uint16_t(a),
            bltbpth,uint16_t(src>>16),bltbptl,uint16_t(src),
            bltcpth,uint16_t(dst>>16),bltcptl,uint16_t(dst),
            bltdpth,uint16_t(dst>>16),bltdptl,uint16_t(dst),
            bltsize,uint16_t((height<<6)|(words&63))};
        AmigaHardware::blitterSubmit(pairs,17);
    }
    queued();return true;
}
bool AmigaSurface::copy(uint32_t from,uint32_t to,unsigned stride,unsigned width,unsigned height,unsigned op){
    if(!fits(from,stride,width,height) || !fits(to,stride,width,height)) {++copyRejectedBounds;return false;}
    // ACRTC overlap is sequential, not memmove. Keep its ordered fallback.
    if(rectanglesOverlap(from,to,stride,width,height)){++copyRejectedOverlap;return false;}
    if(!blitPlanes(from,stride,data+(to>>4),data,data+planeWords*4,stride>>4,planeWords,to&15,width,height,op,true)){
        ++copyRejectedBounds;return false;
    }
    if((from^to)&15)++shiftedCopies;
    changed=true;++copies;return true;
}
bool AmigaSurface::displayBlit(uint16_t *out,uint16_t *begin,uint16_t *end,unsigned rowWords,unsigned planeStride,
                             unsigned dx,unsigned dy,uint32_t source,unsigned stride,unsigned width,unsigned height,bool visible){
    uint16_t *dest=out+uint32_t(uint16_t(dy))*uint16_t(rowWords)+(dx>>4);
    if(!blitPlanes(source,stride,dest,begin,end,rowWords,planeStride,dx&15,width,height,0,visible))return false;
    ++displayBlits;return true;
}

bool AmigaSurface::selfTest(){
    // Exercise the real Agnus masks/minterms against an independent packed
    // reference before any game VRAM access. Includes partial edge words.
    uint16_t *expected=new uint16_t[1024];if(!expected)return false;
    bool ok=true;
    for(unsigned op=0;op<4 && ok;++op)for(unsigned offset: {0u,1u,4u,15u})for(unsigned width: {32u,37u}){
        for(unsigned a=0;a<1024;++a){expected[a]=uint16_t(a^0xa569);writeWord(a,expected[a]);}
        unsigned first=offset,height=3,stride=608;
        if(!fill(first,stride,width,height,0xac39,op)){ok=false;break;}
        auto plot=[&](unsigned pixel,unsigned color){unsigned a=pixel>>2,shift=(pixel&3)*4;uint16_t mask=15<<shift,bits=color<<shift;
            switch(op){case 0:expected[a]=(expected[a]&~mask)|bits;break;case 1:expected[a]|=bits;break;case 2:expected[a]&=uint16_t(~mask|bits);break;case 3:expected[a]^=bits;break;}};
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){unsigned pixel=first+y*608+x;plot(pixel,(0xac39>>((pixel&3)*4))&15);}
        // Include side-by-side rectangles whose enclosing address spans overlap.
        for(unsigned target: {48u,2048u}){
            if(!copy(first,target+offset,stride,width,2,op)){ok=false;break;}
            for(unsigned y=0;y<2;++y)for(unsigned x=0;x<width;++x){unsigned pixel=first+y*608+x;unsigned color=(expected[pixel>>2]>>((pixel&3)*4))&15;plot(target+offset+y*608+x,color);}
        }
        for(unsigned a=0;a<1024;++a)if(readWord(a)!=expected[a]){ok=false;break;}
    }
    // All alignment pairs, narrow/boundary widths and logical operations.
    // Source and destination are disjoint; the packed oracle is independent
    // of shifter masks, word fetches and queued execution order.
    for(unsigned op=0;op<4 && ok;++op)for(unsigned so=0;so<16 && ok;++so)for(unsigned destOffset=0;destOffset<16 && ok;++destOffset){
        for(unsigned a=0;a<256;++a){expected[a]=uint16_t(a*0x321u+0x59ac);writeWord(a,expected[a]);}
        for(unsigned width: {1u,15u,16u,17u,31u,32u,33u,97u}){
            unsigned source=128+so,target=640+destOffset;
            if(!copy(source,target,128,width,2,op)){ok=false;break;}
            for(unsigned y=0;y<2;++y)for(unsigned x=0;x<width;++x){
                unsigned from=source+y*128+x,to=target+y*128+x;
                unsigned color=(expected[from>>2]>>((from&3)*4))&15,shift=(to&3)*4;
                uint16_t mask=15<<shift,bits=color<<shift;
                if(op==0)expected[to>>2]=(expected[to>>2]&~mask)|bits;
                else if(op==1)expected[to>>2]|=bits;
                else if(op==2)expected[to>>2]&=~mask|bits;
                else expected[to>>2]^=bits;
            }
            for(unsigned a=0;a<256 && ok;++a)if(readWord(a)!=expected[a])ok=false;
        }
    }
    if(ok && (copy(17,0,128,1,1,0) || copy(0xfffff,640,128,1,1,0) || copy(128,129,128,33,2,0)))ok=false;
    // Separate interleaved display planes, a leading prefetch guard, blanked
    // regions and untouched edges. These have a different destination pitch.
    uint16_t *display=(uint16_t*)AllocMem(260,MEMF_CHIP);
    if(!display)ok=false;
    for(unsigned so=0;so<16 && ok;++so)for(unsigned offset=0;offset<16 && ok;++offset)for(unsigned visible=0;visible<2 && ok;++visible)for(unsigned width: {32u,33u}){
        uint16_t reference[130];
        for(unsigned a=0;a<130;++a)display[a]=reference[a]=uint16_t(a*0x213+0xab59);
        for(unsigned y=0;y<2;++y)for(unsigned x=0;x<width;++x){
            unsigned pixel=128+so+y*128+x,color=visible?((expected[pixel>>2]>>((pixel&3)*4))&15):0;
            for(unsigned plane=0;plane<4;++plane){unsigned a=2+y*32+plane*8+((offset+x)>>4);uint16_t bit=0x8000u>>((offset+x)&15);
                reference[a]=(reference[a]&~bit)|((color&(1<<plane))?bit:0);}
        }
        if(!displayBlit(display+2,display,display+130,32,8,offset,0,128+so,128,width,2,visible))ok=false;
        synchronize();for(unsigned a=0;a<130 && ok;++a)if(display[a]!=reference[a])ok=false;
    }
    if(display)FreeMem(display,260);
    // Cached mask/colour planes: every alignment, colour mode and logical
    // operation, plus eviction while earlier DMA is still queued.
    for(unsigned op=0;op<4 && ok;++op)for(unsigned mode=0;mode<3 && ok;++mode)for(unsigned offset=0;offset<16 && ok;++offset){
        for(unsigned a=0;a<1024;++a){expected[a]=0x5555;writeWord(a,expected[a]);}
        pokeri::PatternTile tile={};
        for(unsigned y=0;y<16;++y)tile.rows[y]=uint16_t(0xa55a^(y*0x123));
        tile.colors[0]=0x1234;tile.colors[1]=0x89ab;tile.point=0x3040;tile.start=0x2020;tile.end=0x8070;
        tile.mode=mode;tile.width=15;tile.height=14;tile.offset=offset;
        for(unsigned n=0;n<2;++n){
            if(!patternTile(offset,64,tile,op)){ok=false;break;}
            for(unsigned y=0;y<14;++y)for(unsigned x=0;x<15;++x){
                bool bit=(tile.rows[2+pokeri::patternRemainder(14-y,7)]>>(2+pokeri::patternRemainder(2+x,6)))&1;
                if((mode==1 && !bit)||(mode==2 && bit))continue;
                unsigned pixel=offset+y*64+x,a=pixel>>2,shift=(pixel&3)*4;
                unsigned mask=15<<shift,bits=tile.colors[bit]&mask;
                if(op==0)expected[a]=(expected[a]&~mask)|bits;
                else if(op==1)expected[a]|=bits;else if(op==2)expected[a]&=~mask|bits;else expected[a]^=bits;
            }
        }
        for(unsigned a=0;a<1024 && ok;++a)if(readWord(a)!=expected[a])ok=false;
    }
    if(ok){
        pokeri::PatternTile tile={};tile.width=16;tile.height=1;
        for(unsigned n=0;n<128 && ok;++n){tile.colors[0]=n;ok=patternTile(n*16,16,tile,0);}
        for(unsigned n=0;n<128 && ok;++n)if(readWord(n*4)!=n)ok=false;
    }
    // The following test needs untouched storage.
    synchronize();for(uint32_t i=0;i<0x40000;++i)data[i]=0;
    // Tall overlapping-row CLR: must use bounded blits, including both
    // partial endpoint words. Test an untouched range beyond earlier cases.
    if(ok){
        const uint32_t first=65549,end=first+1024*32+36;
        ok=fill(first,32,36,1025,0xac39,0);
        for(uint32_t a=(first>>2)-1;a<=(end>>2)+1 && ok;++a){
            uint16_t expectedWord=0;
            for(unsigned x=0;x<4;++x){uint32_t pixel=(a<<2)+x;
                if(pixel>=first && pixel<end)expectedWord|=0xac39&(15<<(x*4));}
            if(readWord(a)!=expectedWord)ok=false;
        }
    }
    // A single long blit leaves the queue empty while Agnus is busy. This
    // catches noncanonical assembly bool returns hidden by inlined branches.
    if(ok){
        synchronize();uint32_t dest=uint32_t(data);
        const uint16_t pairs[]={bltcon0,0x0100,bltcon1,0,bltdmod,0,
            bltdpth,uint16_t(dest>>16),bltdptl,uint16_t(dest),bltsize,0xffc0};
        AmigaHardware::blitterSubmit(pairs,6);
        // Preserve raw results across calls: another optimized bool inversion
        // could otherwise cancel the very ABI error this test must catch.
        volatile uint8_t whileBusy=AmigaHardware::blitterIdle();
        AmigaHardware::blitterDrain();
        volatile uint8_t afterDrain=AmigaHardware::blitterIdle();
        if(whileBusy!=0 || afterDrain!=1)ok=false;
    }
    // More submissions than the ring can hold while completion interrupts
    // are masked: exercise wrap/backpressure without overwriting live records.
    // Shift each tall fill down one row. Every submission leaves its own
    // retained row, so a dropped or reordered queue entry cannot be hidden
    // by the final fill overwriting all earlier results.
    bool blitEnabled=AmigaHardware::enabledInterrupts()&INTF_BLIT;
    AmigaHardware::setInterrupts(INTF_BLIT,false);
    for(unsigned n=0;n<512 && ok;++n){
        ok=fill(n*16,16,16,1023,uint16_t(n),0);
        // The assembly consumer also publishes a Boolean to C++ memory.
        if(*reinterpret_cast<const volatile uint8_t*>(&AmigaHardware::hasQueuedBlits)>1)ok=false;
    }
    synchronize();
    if(blitEnabled)AmigaHardware::setInterrupts(INTF_BLIT,true);
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
