#define ECS_SPECIFIC
#include "AmigaScreen.h"
#include "../../board/DisplayGeometry.h"
#include "OutputPanel.h"
#include "NativeTiming.h"
#include "../../board/WordMath.h"
#include "CopperList.h"
#include "AmigaHardware.h"
#include <proto/exec.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>
bool AmigaScreen::prepare(AmigaSurface &video,const uint8_t *rom){
    surface=&video;
    GfxBase *gfx=(GfxBase*)OpenLibrary("graphics.library",0);
    const unsigned agaBits=GFXF_AA_ALICE|GFXF_AA_LISA;
    bool aga=gfx && (gfx->ChipRevBits0&agaBits)==agaBits;
    // Some Kickstarts expose only the internal MLISA flag ($13 measured on
    // A1200). Require both physical Alice and Lisa IDs for that fallback;
    // CPU type alone must never enable wide bitplane fetches.
    unsigned lisa=*(volatile uint16_t*)0xdff07c;
    unsigned agnus=*(volatile uint16_t*)0xdff004;
    aga=aga || ((lisa&255)==0xf8 && (agnus&0x0f00)==0x0300);
    if(gfx)CloseLibrary((Library*)gfx);
    AmigaHardware::hasAGAChipSet=aga;
    unsigned brightest=0,darkest=1000;
    for(unsigned i=0;i<16;++i){unsigned light=0;for(unsigned c=0;c<3;++c)light+=rom[0x5d76+i*3+c];
        if(light>=brightest){brightest=light;bright=i;}if(light<darkest){darkest=light;dark=i;}}
    for(unsigned b=0;b<2;++b){
        // Eight bytes preserve AGA pointer alignment while providing a safe,
        // masked prefetch column before the first display word.
        uint16_t *allocation=(uint16_t*)AllocMem(Bytes+8,MEMF_CHIP|MEMF_CLEAR);
        buffers[b]=allocation?allocation+4:nullptr;
        lists[b]=CopperList::allocate(48);
        if(!buffers[b] || !lists[b])return false;
        uint32_t *p=lists[b]->data();unsigned n=0;
        auto move=[&](unsigned reg,unsigned value){p[n++]=(reg<<16)|value;};
        move(0x1fc,aga?3:0); // 64-bit AGA fetch, explicit ECS fallback
        if(aga)move(0x10c,0); // clear inherited AGA palette XOR
        move(0x100,0xc201); // hires, four planes, COLOR, ECS BPLCON3 enabled
        move(0x102,0);move(0x104,0x24);move(0x106,0x0c00);
        // Hardware Reference Manual 3-4-2 / 3-2-7: PAL lines 29..311;
        // HSTART=$91, HSTOP=$1B1 (576 hires pixels). 36 fetched words:
        // DDFSTRT=HSTART/2-4.5=$44; DDFSTOP=$44+4*(36-2)=$CC.
        // AGA: nine 64-pixel fetch groups, 8-byte-aligned plane pointers.
        // Start at an unscrolled fetch boundary: DIW $81, DDF $38..$B8.
        move(0x08e,aga?0x1d81:0x1d91);move(0x090,aga?0x38a1:0x38b1);move(0x1e4,0x2100);
        move(0x092,aga?0x38:0x44);move(0x094,aga?0xb8:0xcc);
        move(0x108,216);move(0x10a,216);
        for(unsigned plane=0;plane<4;++plane){uint32_t address=uint32_t(buffers[b]+plane*36);
            move(0xe0+plane*4,address>>16);move(0xe2+plane*4,address&65535);}
        for(unsigned color=0;color<16;++color){unsigned rgb=0;
            for(unsigned c=0;c<3;++c)rgb=(rgb<<4)|(rom[0x5d76+color*3+c]>>2);
            move(0x180+color*2,rgb);}
        p[n]=0xfffffffe;
    }
    return true;
}
bool AmigaScreen::region(unsigned dx,unsigned dy,uint32_t source,unsigned stride,unsigned width,unsigned height,bool visible,uint16_t *out){
    if(!height || !width)return true;
    if((stride&15) || source+uint32_t(uint16_t(height-1))*uint16_t(stride)+width>0x100000){error="unsupported planar display alignment/wrap";return false;}
#ifdef POKERI_CARD_OBSERVER
    if(visible && surface->pixelObserver)surface->pixelObserver();
#endif
    composedPixels+=uint32_t(uint16_t(width))*uint16_t(height);
    if(surface->displayBlit(out,out-4,out+Bytes/2,144,36,dx,dy,source,stride,width,height,visible))return true;
    // Bounded edge fallback; never read beyond VRAM for a shifted prefetch.
    surface->synchronize();
    surface->displayRegion(out,144,36,dx,dy,source,stride,width,height,visible);
    return true;
}
bool AmigaScreen::present(pokeri::Hd63484 &video,bool force){
    if(!buffers[0])return true;
    // Only explicit capture/benchmark callers force a redraw. A pending list
    // may already be latched by the Copper; wait for retirement before reusing
    // either buffer. Normal play never waits here. Tests own both buffers.
    if(force && !testing){
        if((AmigaHardware::enabledInterrupts()&(INTF_INTEN|INTF_VERTB))!=(INTF_INTEN|INTF_VERTB)){
            error="forced display capture requires VBI interrupts";return false;
        }
        while(pending>=0)presentReady();
    }
    if(!force && pending>=0){presentReady();return true;}
    bool changed=surface->changed || overlayDirty || registersDirty;
    if(!force && !changed)return true;
    auto reg=[&](unsigned a){return unsigned(video.control[a])*256+video.control[a+1];};
    unsigned dcr=reg(6),omr=reg(4);
    unsigned heights[3]={dcr&0x2000?reg(0x8c)&4095:0,reg(0x8a)&4095,dcr&0x800?reg(0x8e)&4095:0};
    if(!geometrySeen){if(!(omr&0x4000) || video.control[0x85]!=71 || heights[0]+heights[1]+heights[2]!=292)return true;geometrySeen=true;}
    if(heights[0]+heights[1]+heights[2]!=292 || (video.control[2]&7)!=2 || (omr&0xff)!=0x28 || video.control[0x85]!=71 || video.control[0xea]){error="unsupported native display geometry";return false;}
    unsigned back=pending>=0?unsigned(pending):front^1;uint16_t *out=buffers[back];
    if(surface->changed || backgroundDirty || overlayDirty || showOutputs)
        backgroundValid[0]=backgroundValid[1]=false;
    bool full=force || !incremental || !backgroundValid[back];
    Bounds repair=full?Bounds{0,0,576,283}:previousWindow[back];
    if(full)++fullFrames;else ++partialFrames;
    unsigned top=0,enables[3]={0x1000,0x4000,0x400};
    for(unsigned n=0;n<3;++n){
        unsigned begin=top<5?5:top,end=top+heights[n];if(end>288)end=288;
        unsigned a=0xc0+n*8,mw=reg(a+2),sar=reg(a+6)|((reg(a+4)&15)<<16);
        if(mw&0x8000){error="unsupported native character mode";return false;}
        if(begin<repair.y+5)begin=repair.y+5;
        if(end>repair.y+5+repair.height)end=repair.y+5+repair.height;
        if(end>begin && repair.width){uint32_t source=((sar+uint32_t(uint16_t(begin-top))*uint16_t(mw&4095))<<2)+((reg(a+4)>>8)&15)/4+repair.x;
            if(!region(repair.x,begin-5,source,(mw&4095)<<2,repair.width,end-begin,(omr&0x4000)&&(dcr&enables[n]),out))return false;}
        top+=heights[n];
    }
    previousWindow[back]=Bounds{};
    if((omr&0x4000) && (dcr&0x200)){
        pokeri::InterleavedWindow window(reg(0x92),reg(0x84),8);
        int wx=window.x,wy=int(reg(0x94)&4095)-int(reg(0x88)>>8);
        int ww=window.width,wh=reg(0x96)&4095;
        int x0=wx<0?0:wx,y0=wy<5?5:wy,x1=wx+ww>576?576:wx+ww,y1=wy+wh>288?288:wy+wh;
        if(x1>x0 && y1>y0){unsigned mw=reg(0xda),sar=reg(0xde)|((reg(0xdc)&15)<<16);
            if(mw&0x8000){error="unsupported native window character mode";return false;}
            uint32_t source=((sar+uint32_t(uint16_t(y0-wy))*uint16_t(mw&4095))<<2)+((reg(0xdc)>>8)&15)/4+unsigned(x0-wx);
            if(!region(x0,y0-5,source,(mw&4095)<<2,x1-x0,y1-y0,dcr&0x100,out))return false;
            previousWindow[back]=Bounds{unsigned(x0),unsigned(y0-5),unsigned(x1-x0),unsigned(y1-y0)};}
    }
    registersDirty=backgroundDirty=false;backgroundValid[back]=true;
    if(force || showOutputs)surface->synchronize();
    if(showOutputs)drawOutputs(out);
    surface->changed=false;overlayDirty=false;pending=back;++frames;presentReady();return true;
}
void AmigaScreen::armReady(){
    if(!AmigaHardware::blitterIdle())return;
    // This runs outside the ISR, after asynchronous composition has finished.
    // Mask only the short pointer publication. The beam guard leaves hundreds
    // of microseconds before the next reload even on a 68000 with hires DMA.
    const unsigned enabled=AmigaHardware::enabledInterrupts()&INTF_INTEN;
    AmigaHardware::setInterrupts(INTF_INTEN,false);
    unsigned line=(*(volatile uint32_t*)0xdff004>>8)&511;
    bool verticalPending=*(volatile uint16_t*)0xdff01e&INTF_VERTB;
    if(pokeri::FrameSwap::armWindow(line,verticalPending) && arm()){
        AmigaHardware::setCopperList(*lists[armed],false);
        ++arms;
    }
    if(enabled)AmigaHardware::setInterrupts(INTF_INTEN,true);
}
void AmigaScreen::vbi(){
    if(testing)return;
    if(pokeri::FrameSwap::vblank()){
        ++swaps;
        if(!displaying){AmigaHardware::setDMAChannels(DMAF_RASTER,true);displaying=true;}
    }
}
void AmigaScreen::release(){
    AmigaHardware::blitterDrain();
    for(unsigned i=0;i<2;++i){delete lists[i];lists[i]=nullptr;if(buffers[i]){FreeMem(buffers[i]-4,Bytes+8);buffers[i]=nullptr;}}
}

void AmigaScreen::outputs(bool enabled,const uint8_t *values){
    if(enabled!=showOutputs)overlayDirty=true;
    for(unsigned i=0;i<8;++i){if(enabled && latches[i]!=values[i])overlayDirty=true;latches[i]=values[i];}
    showOutputs=enabled;
}
void AmigaScreen::drawOutputs(uint16_t *out){
    pokeri::outputPanel(out,latches,bright,dark);
}

// Called only by the explicit native-benchmark path before guest execution.
// No buffers or checks are allocated in normal play.
bool AmigaScreen::compositionTest(pokeri::Hd63484 &video,uint32_t ticks[2]){
    uint16_t *reference=(uint16_t*)AllocMem(Bytes,MEMF_FAST);
    if(!reference)return false;
    testing=true;surface->synchronize();pending=-1;
    bool savedIncremental=incremental,ok=true;
    auto reg=[&](unsigned address,unsigned value){
        video.ar=address;controlWrite(video,value>>8);video.control[address]=value>>8;
        video.ar=address+1;controlWrite(video,value);video.control[address+1]=value;
    };
    reg(2,0x0200);reg(4,0xcd28);reg(6,0x7f00);reg(0x84,0x0947);reg(0x88,0x1d00);
    reg(0x8c,24);reg(0x8a,244);reg(0x8e,24);reg(0xea,0);
    for(unsigned n=0;n<4;++n){reg(0xc2+8*n,152);reg(0xc4+8*n,0);reg(0xc6+8*n,0x1000+n*0x6000);}
    for(unsigned p=0;p<4;++p)for(unsigned w=0;w<0x10000;++w)
        surface->data[p*surface->planeStride+surface->storageWord(w)]=uint16_t(w^(w>>5)^(0x1357u<<p));
    surface->changed=true;
    auto rem=[](uint16_t n,uint16_t d){return unsigned(n)-pokeri::wordProduct(pokeri::wordQuotient(n,d),d);};
    auto retire=[&](){surface->synchronize();if(pending>=0){front=pending;pending=-1;}};
    // Compare every incremental output against a full redraw in the same
    // buffer, including two buffer ages, clipping, blanking and invalidation.
    incremental=true;
    for(unsigned n=0;n<96 && ok;++n){
        reg(0x92,((rem(n,80))<<8)|(n&1?10:11));reg(0x94,rem(n*23,340));reg(0x96,101);
        reg(6,rem(n,13)==0?0x7c00:rem(n,13)==1?0x7e00:0x7f00);
        if(rem(n,17)==0)reg(0xc6,0x1000+(n&3)*16);
        if(rem(n,19)==0)surface->writeWord(0x1000+n,uint16_t(n));
        if(rem(n,11)==0)reg(0xde,0x3000+(n&3));
        if(rem(n,7)==0)reg(0xdc,(n&3)<<10|1);
        uint8_t lamps[8]={uint8_t(n),0,0,0,0,0,0,0};outputs(rem(n,23)<2,lamps);
        ok=present(video);surface->synchronize();
        unsigned b=pending>=0?unsigned(pending):front;
        for(unsigned w=0;w<Bytes/2;++w)reference[w]=buffers[b][w];
        if(ok)ok=present(video,true);
        surface->synchronize();
        for(unsigned w=0;w<Bytes/2 && ok;++w)if(reference[w]!=buffers[b][w])ok=false;
        retire();
    }
    outputs(false,latches);reg(6,0x7f00);reg(0x96,100);reg(0xdc,1);reg(0xde,0x3000);
    // Measure with the actual detected-chipset hires DMA competing for RAM.
    AmigaHardware::setCopperList(*lists[front],true);
    AmigaHardware::setDMAChannels(DMAF_RASTER,true);
    displaying=true;
    for(unsigned mode=0;mode<2 && ok;++mode){
        incremental=mode!=0;surface->changed=true;
        uint32_t start=NativeTiming::benchmarkClock();
        for(unsigned n=0;n<128 && ok;++n){
            reg(0x92,((9+rem(n,52))<<8)|10);reg(0x94,29+rem(n*7,200));
            ok=present(video);retire();
        }
        ticks[mode]=NativeTiming::benchmarkClock()-start;
    }
    incremental=savedIncremental;testing=false;FreeMem(reference,Bytes);return ok;
}
