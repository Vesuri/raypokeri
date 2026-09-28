#ifndef POKERI_CACHED_RASTER_REFERENCE_H
#define POKERI_CACHED_RASTER_REFERENCE_H
#include "../src/board/CardBackCache.h"
// This is an executable specification, not the native implementation. The
// independent oracle remains ordinary FIFO execution and primitive rendering.
inline bool applyRaster(pokeri::CardBackCache::RasterGrant &g,uint16_t value){
    unsigned stage=*g.matched;
    if(stage<=6 || stage>=pokeri::CardBackCache::Commands-1)return false;
    unsigned begin=g.offsets[stage],end=g.offsets[stage+1],n=*g.pendingCount+1;
    if(n!=end-begin || n>64 || value!=g.words[end-1])return false;
    if(n==1 ? *g.pendingLength!=0 : *g.pendingLength!=int(n))return false;
    unsigned group=g.words[begin]>>10;
    if(group==2 || group==32 || group==33)return false;
    for(unsigned i=0;i+1<n;++i)if(g.pending[i]!=g.words[begin+i])return false;
    g.pending[n-1]=value;
    *g.writeHigh=uint8_t(value>>8);
    for(unsigned i=0;i<n;++i)g.buffered[*g.used+i]=g.pending[i];
    *g.used+=n;*g.matched=stage+1;
    auto &p=g.progress[stage];
    int x=int16_t(g.anchorX+p.x),y=int16_t(g.anchorY+p.y);
    g.parameter[18]=uint16_t(x);g.parameter[19]=uint16_t(y);
    int dot=x+int((g.origin&15)>>2);
    int word=dot>=0?dot/4:-int((unsigned(-dot)+3)/4);
    uint32_t address=((g.origin>>4)+uint32_t(word)-uint32_t(y*152))&0xfffff;
    g.parameter[16]=uint16_t((g.origin>>16&0xc000)|(address>>12));
    g.parameter[17]=uint16_t((address<<4)|((unsigned(dot)&3)<<2));
    *g.work=g.rectangleWork?p.rectangleWork:p.scalarWork;
    *g.stopped=false;*g.cpuTried=false;*g.cpuData=nullptr;
    ++g.commands[group];*g.pendingCount=0;*g.pendingLength=0;
    *g.status|=pokeri::Hd63484::CED;
    return true;
}
#endif
