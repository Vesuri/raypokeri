#include "CardBackCache.h"
#include "WordMath.h"
namespace pokeri {
namespace {
// Startup-only canvas. Preserve actual written coverage, and derive predicates
// for previously undefined pixels from the PAINT which reads them. Bounds and
// unsupported dependencies are failures, never silently transparent pixels.
struct Canvas : Surface {
    uint8_t pixels[8800]={},defined[1100]={},whiteDefined[1100]={};
    Hd63484 &video;
    CardBackCache &cache;
    uint16_t command=0;
    bool rectangles=false;
    mutable bool invalid=false;
    Canvas(Hd63484 &v,CardBackCache &c):video(v),cache(c){}
    static unsigned divide(uint32_t n){
#ifdef __m68k__
        uint16_t d=152;__asm volatile("divu.w %1,%0":"+d"(n):"d"(d):"cc");return uint16_t(n);
#else
        return n/152;
#endif
    }
    unsigned index(uint32_t a,unsigned shift)const {
        int32_t delta=int32_t((a-((video.origin>>4)&video.frameMask)+0x20000)&0x3ffff)-0x20000;
        int32_t n=21-delta;unsigned q=divide(n<0?uint32_t(-n):uint32_t(n));
        int y=n<0?-int(q)-(uint32_t(-n)!=wordProduct(uint16_t(q),152)):int(q);
        int x=delta+int32_t(int16_t(y))*int16_t(152);x=x*4+int(shift>>2);
        if(x<0 || x>=88 || y<0 || y>=100){invalid=true;return 8800;}
        return wordProduct(uint16_t(y),88)+unsigned(x);
    }
    uint16_t readWord(uint32_t)const override{invalid=true;return 0;}
    void writeWord(uint32_t,uint16_t)override{invalid=true;}
    uint16_t pixel4(uint32_t a,unsigned shift)const override{
        unsigned i=index(a,shift);if(i==8800)return 0;
        if(!(defined[i>>3]&(1<<(i&7)))){
            if((command>>10)!=50){invalid=true;return 0;}
            unsigned c0=(video.parameter[0]>>shift)&15,c1=(video.parameter[1]>>shift)&15,e=(video.parameter[3]>>shift)&15;
            uint16_t allowed=uint16_t(~((1u<<c0)|(1u<<c1)));
            allowed&=command&0x100?uint16_t(1u<<e):uint16_t(~(1u<<e));
            // The canonical preparation background is zero. Any other
            // predicate needs a separate proven preparation path.
            if(!(allowed&1)){invalid=true;return 0;}
            unsigned g=0;while(g<cache.guardCount && cache.guards[g].pixel!=i)++g;
            if(g==cache.guardCount){
                if(g==CardBackCache::MaxGuards){invalid=true;return 0;}
                cache.guards[g]={uint16_t(i),allowed};++cache.guardCount;
            }else cache.guards[g].allowed&=allowed;
        }
        return pixels[i];
    }
    void plot4(uint32_t a,unsigned shift,unsigned color,unsigned op)override{
        unsigned i=index(a,shift);if(i==8800)return;
        if(op){invalid=true;return;}
        pixels[i]=color;defined[i>>3]|=1<<(i&7);
    }
    bool fill(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t color,unsigned op)override{
        if(!rectangles)return false;
        if(stride!=608 || op)return false;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
            uint32_t p=first+wordProduct(uint16_t(y),608)+x;
            plot4(p>>2,(p&3)<<2,(color>>((p&3)<<2))&15,op);
        }
        return !invalid;
    }
};
}
void CardBackCache::restoreShadow(Surface *surface){
    shadow.control=entry.control;shadow.parameter=entry.parameter;shadow.pattern=entry.pattern;
    shadow.origin=entry.origin;shadow.frameMask=entry.mask;shadow.rwp=entry.rwp;shadow.status=entry.status;
    shadow.error=nullptr;shadow.ar=0;shadow.writeLow=shadow.readLow=false;
    shadow.clearPending();shadow.readFifo.clear();shadow.surface=surface;shadow.invalidateCpu();
}
void CardBackCache::save(const Hd63484 &v){
    entry.control=v.control;entry.parameter=v.parameter;entry.pattern=v.pattern;
    entry.origin=v.origin;entry.mask=v.frameMask;entry.rwp=v.rwp;entry.status=v.status;
}
bool CardBackCache::prepare(Recipe descriptor,uint16_t *imageStorage,uint16_t *maskStorage){
    if(owner)detach();
    ready=whiteReady=false;error=nullptr;guardCount=coverage=0;recipe=descriptor;image=imageStorage;mask=maskStorage;
    if(!image || !mask)return false;
    if(recipe.offsets[0]!=0 || recipe.offsets[Commands]!=Words){error="card cache: descriptor bounds";return false;}
    const uint32_t *c=recipe.context;
    shadow.origin=c[0];shadow.frameMask=c[1];shadow.rwp=c[2];shadow.status=c[3];
    for(unsigned i=0;i<32;++i)shadow.parameter[i]=c[4+i];
    for(unsigned i=0;i<16;++i)shadow.pattern[i]=c[36+i];
    for(unsigned i=0;i<256;++i)shadow.control[i]=c[52+i];
    save(shadow);
    for(unsigned i=0;i<BitmapWords;++i)image[i]=mask[i]=0;
    bool whiteProven=true;
    for(unsigned pass=0;pass<2;++pass){
        Canvas *canvas=new Canvas(shadow,*this);if(!canvas)return false;
        canvas->rectangles=pass!=0;restoreShadow(canvas);
        for(unsigned n=0;n<Commands;++n){
            unsigned begin=recipe.offsets[n],end=recipe.offsets[n+1];
            if(end<=begin || end>Words){error="card cache: descriptor command bounds";break;}
            canvas->command=recipe.words[begin];
            for(unsigned i=begin;i<end;++i)shadow.writeFifoWord(recipe.words[i]);
            if(shadow.error || canvas->invalid){error=shadow.error?shadow.error:"card cache: unproved drawing dependency/bounds";break;}
            if(n+1==WhiteCommands){
                for(unsigned i=0;i<1100;++i)canvas->whiteDefined[i]=canvas->defined[i];
                for(unsigned i=0;i<8800;++i)if((canvas->defined[i>>3]&(1<<(i&7))) && canvas->pixels[i]!=15)whiteProven=false;
            }
            if(!pass){progress[n].x=shadow.parameter[18];progress[n].y=shadow.parameter[19];progress[n].scalarWork=shadow.drawingWork;}
            else {progress[n].rectangleWork=shadow.drawingWork;
                if(progress[n].x!=int16_t(shadow.parameter[18]) || progress[n].y!=int16_t(shadow.parameter[19])){error="card cache: accelerated position differs";break;}}
        }
        if(!error)for(unsigned y=0;y<100;++y)for(unsigned x=0;x<88;++x){
            unsigned pixel=wordProduct(uint16_t(y),88)+x;
            bool written=canvas->defined[pixel>>3]&(1<<(pixel&7));
            if(written!=bool(canvas->whiteDefined[pixel>>3]&(1<<(pixel&7))))whiteProven=false;
            unsigned base=wordProduct(uint16_t(99-y),28)+(x>>4);uint16_t bit=uint16_t(0x8000u>>(x&15));
            if(!pass && written)++coverage;
            for(unsigned p=0;p<4;++p,base+=7){
                uint16_t m=written?bit:0,b=written && (canvas->pixels[pixel]&(1<<p))?bit:0;
                if(!pass){mask[base]|=m;image[base]|=b;}
                else if((mask[base]&bit)!=m || (image[base]&bit)!=b)error="card cache: accelerated coverage/pixels differ";
            }
        }
        shadow.surface=nullptr;delete canvas;
        if(error)return false;
    }
    whiteReady=whiteProven;ready=true;clear();return true;
}
void CardBackCache::attach(Hd63484 &v,bool rectangleSemantics){
    if(owner)detach();if(!ready)return;
    owner=&v;rectangles=rectangleSemantics;v.cardCache=this;
}
void CardBackCache::detach(){if(owner){flush(*owner);owner->cardCache=nullptr;owner=nullptr;}}
bool CardBackCache::context(const Hd63484 &v)const{
    const uint32_t *c=recipe.context;
    if(v.error || (v.status&(Hd63484::CER|Hd63484::ARD)) || !v.readFifo.empty() ||
       v.frameMask!=c[1] || v.origin!=c[0] || v.wptnCountsBytes || (v.control[2]&7)!=2 || !v.surface)return false;
    unsigned mw=0xc2+8*(v.origin>>30);
    if(v.control[mw] || v.control[mw+1]!=152)return false;
    // The exact recipe sets PS/PE to row 0, bits 0..7, no zoom. All drawing
    // uses those pattern bits; the remaining pattern RAM is untouched state.
    if((v.pattern[0]&255)!=(c[36]&255))return false;
    for(unsigned p:{2u,4u,8u,9u,10u,11u})if(v.parameter[p]!=c[4+p])return false;
    return true;
}
bool CardBackCache::admit(Hd63484 &v,int x,int y){
    if(x< -32768 || x>32767-87 || y< -32768 || y>32767-99){++boundsMisses;return false;}
    unsigned shift;uint32_t a=v.pixelAddress(x,y+99,shift)&v.frameMask;
    destination=(a<<2)+(shift>>2);
    if(!v.surface->cardBlitFits(destination)){++boundsMisses;return false;}
    for(unsigned g=0;g<guardCount;++g){
        unsigned row=wordQuotient(guards[g].pixel,88),col=guards[g].pixel-wordProduct(uint16_t(row),88);
        uint32_t address=v.pixelAddress(x+int(col),y+int(row),shift)&v.frameMask;
        unsigned color=v.surface->pixel4(address,shift);
        if(!(guards[g].allowed&(1u<<color))){++guardMisses;return false;}
    }
    anchorX=x;anchorY=y;return true;
}
bool CardBackCache::rasterGrant(Hd63484 &v,RasterGrant &out,bool controls,bool absolute){
    // An enabled CED IRQ or observer must see the general completion path.
    // No mutable drawing state can change during this borrowed FIFO span.
    if(!ready || !enabled || owner!=&v || matched<=6 || matched>=Commands-1 ||
       !v.cachedPixels || v.ar>=2 || v.writeLow || v.presentationBusy || v.error ||
       v.commandLog || (v.control[3]&Hd63484::CED) || !v.pendingSpill.empty() ||
       v.pendingCount>=64 || v.pendingLength>64 || (v.control[2]&7)!=2 ||
       v.memoryWidth(v.origin>>30)!=152)return false;
    const unsigned group=recipe.words[recipe.offsets[matched]]>>10;
    if((group==32 && !absolute) || ((group==2 || group==33) && !controls))return false;
    if(group==2 && ((recipe.words[recipe.offsets[matched]]&31)==12 ||
                    (recipe.words[recipe.offsets[matched]]&31)==13))return false;
    // The view holds addresses of fixed members. Rebind when either object
    // or the recipe changes; ordinary command completions need no recopy.
    if(out.pending!=v.pendingWords || out.matched!=&matched ||
       out.words!=recipe.words || out.offsets!=recipe.offsets){
        out.words=recipe.words;out.offsets=recipe.offsets;out.progress=progress;
        out.buffered=buffered;out.pending=v.pendingWords;out.parameter=v.parameter.data();
        out.matched=&matched;out.used=&used;out.pendingCount=&v.pendingCount;
        out.pendingLength=&v.pendingLength;out.writeHigh=&v.writeHigh;out.status=&v.status;
        out.work=&v.drawingWork;out.stopped=&v.drawingStopped;out.cpuTried=&v.cpuAccessTried;
        out.cpuData=&v.cpuPlanes.data;out.commands=v.commands.data();
    }
    out.anchorX=anchorX;out.anchorY=anchorY;out.origin=v.origin;
    out.rectangleWork=unsigned(rectangles);out.controls=controls;out.absolute=absolute;
    return true;
}
bool CardBackCache::command(Hd63484 &v,const uint16_t *w,unsigned n){
    if(!ready || !enabled){if(matched)flush(v);return false;}
    unsigned begin=recipe.offsets[matched],end=recipe.offsets[matched+1];
    bool match=n==end-begin && w[0]==recipe.words[begin];
    int ax=anchorX,ay=anchorY;
    if(match && w[0]==0x8000){
        int x=int16_t(w[1])-int16_t(recipe.words[begin+1]),y=int16_t(w[2])-int16_t(recipe.words[begin+2]);
        if(matched==5){ax=x;ay=y;}else match=x==ax && y==ay;
    }else if(match)for(unsigned i=1;i<n;++i)if(w[i]!=recipe.words[begin+i]){match=false;break;}
    if(!match){
        if(matched){++misses;++mismatchStage[matched];flush(v,1);return command(v,w,n);}
        return false;
    }
    if(!matched){if(!context(v)){++contextMisses;return false;}save(v);++starts;
#ifdef POKERI_LEDGER_FAST_CACHE
        // Inline headers bypass push(); time successful recognition instead.
        if(timing)timing(0,w[0]);
#endif
    }
    if(matched==5 && !admit(v,ax,ay)){clear();return false;}
    for(unsigned i=0;i<n;++i)buffered[used++]=w[i];
    unsigned stage=matched++;
    // WPR and MOVE stay on their already-cheap authoritative fast path. All
    // raster work from the first rectangle onward is replaced on a hit.
    if((w[0]>>10)==2 || (w[0]>>10)==32 || (w[0]>>10)==33)return false;
    v.position(anchorX+progress[stage].x,anchorY+progress[stage].y);
    v.drawingStopped=false;v.drawingWork=rectangles?progress[stage].rectangleWork:progress[stage].scalarWork;
    v.invalidateCpu();
    if(!v.cachedPixels){v.cachedPixels=true;v.surface->damageCard(destination);}
    if(matched==Commands){
        if(v.surface->cardBlit(destination,image,mask)){++hits;v.cachedPixels=false;
#ifdef POKERI_TIME_LEDGER
            if(timing)timing(1,hits);
#endif
            clear();}
        else {++boundsMisses;flush(v);}
    }
    return true;
}
void CardBackCache::flush(Hd63484 &v,unsigned reason){
    if(!matched)return;
    if(v.cachedPixels){
        ++barriers;++barrierStage[matched];++barrierReason[reason];v.cachedPixels=false;
#ifdef POKERI_TIME_LEDGER
        if(timing)timing(2,(reason<<16)|matched);
#endif
        restoreShadow(v.surface);
        unsigned from=0;
        // Every card shares the proven opaque-white prefix. Delay this stamp
        // until an observation/mismatch so a complete back still uses one blit.
        // The prepared coverage mask is also its four-plane white image: no
        // extra resident bitmap. Later inset/rank/suit commands retain order.
        if(whiteReady && whiteEnabled && matched>=WhiteCommands &&
           v.surface->cardBlit(destination,mask,mask)){
            ++whiteHits;
            for(unsigned stage=0;stage<WhiteCommands;++stage){
                unsigned begin=recipe.offsets[stage],end=recipe.offsets[stage+1];
                unsigned group=buffered[begin]>>10;
                if(group==2 || group==32 || group==33){
                    for(unsigned i=begin;i<end;++i)shadow.writeFifoWord(buffered[i]);
                }else{
                    shadow.position(anchorX+progress[stage].x,anchorY+progress[stage].y);
                    shadow.drawingStopped=false;
                    shadow.drawingWork=rectangles?progress[stage].rectangleWork:progress[stage].scalarWork;
                }
            }
            from=recipe.offsets[WhiteCommands];
        }
        if(from<used)++prefixReplays;
        for(unsigned i=from;i<used;++i)shadow.writeFifoWord(buffered[i]);
        if(shadow.error)v.fail(shadow.error);
    }
    clear();
}
}
