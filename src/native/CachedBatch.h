#ifndef POKERI_NATIVE_CACHED_BATCH_H
#define POKERI_NATIVE_CACHED_BATCH_H
#include "board/CardBackCache.h"
#include "board/WordMath.h"
namespace pokeri {
// Private state borrowed only inside the verified native feeder. These first
// two fields are the assembly acceptance cursor and exclusive expected bound.
// Materialize before any observer, callout, mismatch or return to guest.
struct CachedBatch {
    const uint16_t *cursor=nullptr,*limit=nullptr;
// Remaining members are implementation state; public for a standard-layout ABI.
    using Grant=CardBackCache::RasterGrant;
    Grant g;
    const uint16_t *cachedWords=nullptr,*cachedOffsets=nullptr;
    int cachedX=0,cachedY=0;
    uint16_t translated[CardBackCache::Words];
    bool translatedSafe[CardBackCache::Commands];
    unsigned first=0,stage=0,count=0;
    uint32_t cachedControls=~uint32_t(0),cachedAbsolute=~uint32_t(0);
    uint16_t endOffsets[CardBackCache::Commands];
    unsigned size()const{return g.offsets[stage+1]-g.offsets[stage];}
    uint16_t word(unsigned s,unsigned i)const{return translated[g.offsets[s]+i];}
    bool eligible()const{
        if(stage<=6 || stage>=pokeri::CardBackCache::Commands-1)return false;
        unsigned op=g.words[g.offsets[stage]],group=op>>10,n=size();
        if(!n || n>64)return false;
        const auto &f=pokeri::Hd63484::formats[group];
        if(!f.words || (op&f.reserved))return false;
        if(f.words>0 && unsigned(f.words)!=n)return false;
        // Only polygons among admitted commands have a variable count.
        if(f.words<0 && (group!=39 || n!=2+2*g.words[g.offsets[stage]+1]))return false;
        if(group==2)return g.controls && n==2 && (op&31)<12;
        if(group==32)return g.absolute && n==3;
        if(group==33)return g.controls && n==3;
        return group==39 || group==42 || group==43 || group==49 || group==50;
    }
public:
    bool begin(const Grant &view){
        if(cursor || *view.pendingCount || *view.pendingLength || view.buffered)return false;
        g=view;first=stage=*g.matched;count=0;
        if(!eligible())return false;
        if(cachedWords!=g.words || cachedOffsets!=g.offsets || cachedX!=g.anchorX || cachedY!=g.anchorY || cachedControls!=g.controls || cachedAbsolute!=g.absolute){
            cachedWords=g.words;cachedOffsets=g.offsets;cachedX=g.anchorX;cachedY=g.anchorY;cachedControls=g.controls;cachedAbsolute=g.absolute;
            for(unsigned s=0;s<CardBackCache::Commands;++s){
                translatedSafe[s]=true;
                for(unsigned i=g.offsets[s];i<g.offsets[s+1];++i){
                    uint16_t value=g.words[i];
                    if(i!=g.offsets[s] && g.words[g.offsets[s]]>>10==32){
                        int anchor=i==g.offsets[s]+1?g.anchorX:g.anchorY;
                        value=uint16_t(value+anchor);
                        if(int16_t(value)-int16_t(g.words[i])!=anchor)translatedSafe[s]=false;
                    }
                    translated[i]=value;
                }
            }
            // Eligibility depends only on the immutable recipe, translation
            // and two grant options. Reuse its bound across IRQ-separated
            // borrows; never re-scan the remaining recipe in a live handler.
            unsigned stop=CardBackCache::Commands-1;
            for(unsigned s=CardBackCache::Commands-1;s-->7;){
                stage=s;if(!eligible() || !translatedSafe[s])stop=s;
                endOffsets[s]=g.offsets[stop];
            }
            stage=first;
        }
        unsigned end=endOffsets[first];
        if(end==g.offsets[first])return false;
        cursor=translated+g.offsets[first];limit=translated+end;return true;
    }
    bool accept(uint16_t value){
        if(!cursor || cursor==limit || value!=*cursor)return false;
        ++cursor;return true;
    }
    void materialize(){
        if(!cursor)return;
        unsigned offset=unsigned(cursor-translated);
        cursor=limit=nullptr;
        if(offset==g.offsets[first])return;
        while(stage<CardBackCache::Commands-1 && offset>=g.offsets[stage+1])++stage;
        count=offset-g.offsets[stage];
        uint16_t last=translated[offset-1];
        bool moved=false,raster=false;
        int x=int16_t(g.parameter[18]),y=int16_t(g.parameter[19]);
        unsigned work=0;
        // This proof form aggregates address computation and flag stores.
        // Native prefix tables may replace the scan only after equivalence.
        for(unsigned s=first;s<stage;++s){
            unsigned begin=g.offsets[s],op=g.words[begin],group=op>>10;
            ++g.commands[group];
            if(group==2){g.parameter[op&31]=g.words[begin+1];continue;}
            moved=true;
            if(group==32){x=int16_t(word(s,1));y=int16_t(word(s,2));work=0;}
            else if(group==33){x=int16_t(x+int16_t(word(s,1)));y=int16_t(y+int16_t(word(s,2)));work=0;}
            else {x=int16_t(g.anchorX+g.progress[s].x);y=int16_t(g.anchorY+g.progress[s].y);
                work=g.rectangleWork?g.progress[s].rectangleWork:g.progress[s].scalarWork;raster=true;}
        }
        if(moved){
            g.parameter[18]=uint16_t(x);g.parameter[19]=uint16_t(y);
            int dot=x+int((g.origin&15)>>2);
            int word=dot>=0?dot/4:-int((unsigned(-dot)+3)/4);
            uint32_t address=((g.origin>>4)+uint32_t(word)-(y<0?0u-wordProduct(uint16_t(-y),152):wordProduct(uint16_t(y),152)))&0xfffff;
            g.parameter[16]=uint16_t((g.origin>>16&0xc000)|(address>>12));
            g.parameter[17]=uint16_t((address<<4)|((unsigned(dot)&3)<<2));
            *g.work=work;*g.stopped=false;
        }
        if(raster){*g.cpuTried=false;*g.cpuData=nullptr;}
        if(stage!=first){*g.status|=pokeri::Hd63484::CED;*g.used+=g.offsets[stage]-g.offsets[first];}
        *g.matched=stage;*g.pendingCount=count;
        *g.pendingLength=count?pokeri::Hd63484::formats[g.words[g.offsets[stage]]>>10].words:0;
        if(count>=2 && *g.pendingLength<0)*g.pendingLength=size();
        for(unsigned i=0;i<count;++i)g.pending[i]=word(stage,i);
        *g.writeHigh=uint8_t(last>>8);
    }
    bool borrowed()const{return cursor!=nullptr;}
};
}
#endif
