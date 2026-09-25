#define ECS_SPECIFIC
#include "AmigaScreen.h"
#include "CopperList.h"
#include "AmigaHardware.h"
#include <proto/exec.h>
#include <exec/memory.h>
bool AmigaScreen::prepare(AmigaSurface &video,const uint8_t *rom){
    surface=&video;
    unsigned brightest=0,darkest=1000;
    for(unsigned i=0;i<16;++i){unsigned light=0;for(unsigned c=0;c<3;++c)light+=rom[0x5d76+i*3+c];
        if(light>=brightest){brightest=light;bright=i;}if(light<darkest){darkest=light;dark=i;}}
    for(unsigned b=0;b<2;++b){
        buffers[b]=(uint16_t*)AllocMem(Bytes,MEMF_CHIP|MEMF_CLEAR);
        lists[b]=CopperList::allocate(48);
        if(!buffers[b] || !lists[b])return false;
        uint32_t *p=lists[b]->data();unsigned n=0;
        auto move=[&](unsigned reg,unsigned value){p[n++]=(reg<<16)|value;};
        move(0x100,0xc201); // hires, four planes, COLOR, ECS BPLCON3 enabled
        move(0x102,0);move(0x104,0x24);move(0x106,0x0c00);
        // Hardware Reference Manual 3-4-2 / 3-2-7: PAL lines 29..311;
        // HSTART=$91, HSTOP=$1B1 (576 hires pixels). 36 fetched words:
        // DDFSTRT=HSTART/2-4.5=$44; DDFSTOP=$44+4*(36-2)=$CC.
        move(0x08e,0x1d91);move(0x090,0x38b1);move(0x1e4,0x2100);
        move(0x092,0x44);move(0x094,0xcc);
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
    if((stride&15) || ((source^dx)&15) || source+uint32_t(uint16_t(height-1))*uint16_t(stride)+width>0x100000){error="unsupported planar display alignment/wrap";return false;}
    unsigned count=((dx&15)+width+15)>>4,tail=(dx+width)&15;
    uint16_t firstMask=uint16_t(0xffffu>>(dx&15)),lastMask=tail?uint16_t(0xffffu<<(16-tail)):0xffff;
    uint32_t from=source>>4;uint16_t *dest=out+uint32_t(uint16_t(dy))*144+(dx>>4);
    for(unsigned p=0;p<4;++p){
        uint32_t src=uint32_t(surface->data+from),dst=uint32_t(dest);
        const uint16_t pairs[]={bltcon0,uint16_t(visible?0x07ca:0x030a),bltcon1,0,
            bltafwm,firstMask,bltalwm,lastMask,bltadat,0xffff,
            bltbmod,uint16_t((stride>>3)-(count<<1)),bltcmod,uint16_t(288-(count<<1)),bltdmod,uint16_t(288-(count<<1)),
            bltbpth,uint16_t(src>>16),bltbptl,uint16_t(src),
            bltcpth,uint16_t(dst>>16),bltcptl,uint16_t(dst),
            bltdpth,uint16_t(dst>>16),bltdptl,uint16_t(dst),bltsize,uint16_t((height<<6)|count)};
        AmigaHardware::blitterSubmit(pairs,15);from+=surface->planeWords;dest+=36;
    }
    surface->queued();return true;
}
bool AmigaScreen::present(pokeri::Hd63484 &video,bool force){
    if(!buffers[0] || (!force && pending>=0))return true;
    bool changed=surface->changed || overlayDirty || registersDirty;
    if(!force && !changed)return true;
    auto reg=[&](unsigned a){return unsigned(video.control[a])*256+video.control[a+1];};
    unsigned dcr=reg(6),omr=reg(4);
    unsigned heights[3]={dcr&0x2000?reg(0x8c)&4095:0,reg(0x8a)&4095,dcr&0x800?reg(0x8e)&4095:0};
    if(!geometrySeen){if(!(omr&0x4000) || video.control[0x85]!=71 || heights[0]+heights[1]+heights[2]!=292)return true;geometrySeen=true;}
    if(heights[0]+heights[1]+heights[2]!=292 || (video.control[2]&7)!=2 || (omr&0xff)!=0x28 || video.control[0x85]!=71 || video.control[0xea]){error="unsupported native display geometry";return false;}
    unsigned back=pending>=0?unsigned(pending):front^1;uint16_t *out=buffers[back];
    unsigned top=0,enables[3]={0x1000,0x4000,0x400};
    for(unsigned n=0;n<3;++n){
        unsigned begin=top<5?5:top,end=top+heights[n];if(end>288)end=288;
        unsigned a=0xc0+n*8,mw=reg(a+2),sar=reg(a+6)|((reg(a+4)&15)<<16);
        if(mw&0x8000){error="unsupported native character mode";return false;}
        if(end>begin){uint32_t source=((sar+uint32_t(uint16_t(begin-top))*uint16_t(mw&4095))<<2)+((reg(a+4)>>8)&15)/4;
            if(!region(0,begin-5,source,(mw&4095)<<2,576,end-begin,(omr&0x4000)&&(dcr&enables[n]),out))return false;}
        top+=heights[n];
    }
    if((omr&0x4000) && (dcr&0x200)){
        int wx=(int(reg(0x92)>>8)-int(reg(0x84)>>8))*8,wy=int(reg(0x94)&4095)-int(reg(0x88)>>8);
        int ww=((reg(0x92)&255)+1)*8,wh=reg(0x96)&4095;
        int x0=wx<0?0:wx,y0=wy<5?5:wy,x1=wx+ww>576?576:wx+ww,y1=wy+wh>288?288:wy+wh;
        if(x1>x0 && y1>y0){unsigned mw=reg(0xda),sar=reg(0xde)|((reg(0xdc)&15)<<16);
            if(mw&0x8000){error="unsupported native window character mode";return false;}
            uint32_t source=((sar+uint32_t(uint16_t(y0-wy))*uint16_t(mw&4095))<<2)+((reg(0xdc)>>8)&15)/4+unsigned(x0-wx);
            if(!region(x0,y0-5,source,(mw&4095)<<2,x1-x0,y1-y0,dcr&0x100,out))return false;}
    }
    registersDirty=false;
    if(force || showOutputs)surface->synchronize();
    if(showOutputs)drawOutputs(out);
    surface->changed=false;overlayDirty=false;pending=back;++frames;return true;
}
void AmigaScreen::vbi(){
    if(pending>=0 && AmigaHardware::blitterIdle()){front=pending;pending=-1;AmigaHardware::setCopperList(*lists[front],true);}
}
void AmigaScreen::release(){
    AmigaHardware::blitterDrain();
    for(unsigned i=0;i<2;++i){delete lists[i];lists[i]=nullptr;if(buffers[i]){FreeMem(buffers[i],Bytes);buffers[i]=nullptr;}}
}

void AmigaScreen::outputs(bool enabled,const uint8_t *values){
    if(enabled!=showOutputs)overlayDirty=true;
    for(unsigned i=0;i<8;++i){if(enabled && latches[i]!=values[i])overlayDirty=true;latches[i]=values[i];}
    showOutputs=enabled;
}
void AmigaScreen::drawOutputs(uint16_t *out){
    // Optional F3 panel: rows 0..7 are output latches, columns are bits 0..7.
    // Physical lamp names remain unidentified; never label guessed wiring.
    static const uint8_t digits[8][5]={{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2}};
    auto dot=[&](unsigned x,unsigned y,unsigned color){uint16_t mask=uint16_t(0x8000u>>(x&15));
        for(unsigned p=0;p<4;++p){uint16_t &word=out[y*144+p*36+(x>>4)];word=(word&~mask)|((color&(1<<p))?mask:0);}};
    for(unsigned y=4;y<70;++y)for(unsigned x=502;x<576;++x)dot(x,y,dark);
    for(unsigned row=0;row<8;++row){
        for(unsigned y=0;y<5;++y)for(unsigned x=0;x<3;++x)if(digits[row][y]&(4>>x))dot(504+x,6+row*8+y,bright);
        for(unsigned bit=0;bit<8;++bit)for(unsigned y=0;y<5;++y)for(unsigned x=0;x<5;++x)
            if((latches[row]&(1<<bit)) || !x || !y || x==4 || y==4)dot(512+bit*8+x,6+row*8+y,bright);
    }
}
