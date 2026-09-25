#include "Display.h"
#include <stdexcept>
namespace pokeri {
VideoFrame compose(const pokeri::Hd63484 &v) {
    auto reg=[&](unsigned a){return unsigned(v.control[a])*256+v.control[a+1];};
    unsigned omr=reg(4),dcr=reg(6),gbm=v.control[2]&7;
    VideoFrame f;
    if(!(omr&0x4000)) return f;
    if(gbm!=2 || (omr&15)!=8 || v.control[0xea])
        throw std::runtime_error("unsupported HD63484 display mode/zoom");
    unsigned ppmc=(16/(1<<gbm))*(1<<((omr>>4)&7))/2;
    f.width=((reg(0x84)&255)+1)*ppmc;
    unsigned heights[3]={reg(0x8c)&0xfff,reg(0x8a)&0xfff,reg(0x8e)&0xfff};
    unsigned enables[3]={0x2000,0x8000,0x800};
    for(unsigned i=0;i<3;++i) if(i==1 || (dcr&enables[i])) f.height+=heights[i];else heights[i]=0;
    if(f.width>4096 || f.height>4096) throw std::runtime_error("excessive display geometry");
    f.indices.resize(f.width*f.height);
    auto dot=[&](unsigned dn,unsigned x,unsigned y) {
        unsigned a=0xc0+dn*8,mw=reg(a+2);
        if(mw&0x8000) throw std::runtime_error("character display mode is not implemented");
        unsigned sar=reg(a+6)|((reg(a+4)&15)<<16);
        unsigned pixel=x+((reg(a+4)>>8)&15)/4;
        return uint8_t((v.readWord(sar+y*(mw&0xfff)+pixel/4)>>((pixel%4)*4))&15);
    };
    unsigned top=0;
    for(unsigned dn=0;dn<3;++dn) {
        bool displayed=dcr&(enables[dn]>>1);
        for(unsigned y=0;y<heights[dn];++y)for(unsigned x=0;x<f.width;++x)
            f.indices[(top+y)*f.width+x]=displayed?dot(dn,x,y):0;
        top+=heights[dn];
    }
    if(dcr&0x200) {
        // Odd window widths in interleaved mode delay the window by two
        // memory cycles (see docs/rom-set.md, window alignment finding).
        int delay=((reg(0x92)+1)&1)?2:0;
        int wx=(int(reg(0x92)>>8)-int(reg(0x84)>>8)+delay)*int(ppmc);
        int wy=int(reg(0x94)&0xfff)-int(reg(0x88)>>8);
        int ww=((reg(0x92)&255)+1)*ppmc,wh=reg(0x96)&0xfff;
        for(int y=0;y<wh;++y)for(int x=0;x<ww;++x)
            if(x+wx>=0 && y+wy>=0 && x+wx<int(f.width) && y+wy<int(f.height))
                f.indices[(y+wy)*f.width+x+wx]=(dcr&0x100)?dot(3,x,y):0;
    }
    return f;
}
}
