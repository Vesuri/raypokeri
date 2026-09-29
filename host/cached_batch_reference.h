#ifndef POKERI_CACHED_BATCH_REFERENCE_H
#define POKERI_CACHED_BATCH_REFERENCE_H
#include "../src/board/CardBackCache.h"
// Host-only executable specification. Native integration must keep every
// existing boundary and call materialize before any observation or callout.
// This deliberately does not replace the independent ordinary FIFO oracle.
class CachedBatchReference {
    using Grant=pokeri::CardBackCache::RasterGrant;
    Grant g;
    unsigned first=0,stage=0,count=0,accepted=0;
    uint16_t last=0;
    bool active=false;
    unsigned size()const{return g.offsets[stage+1]-g.offsets[stage];}
    uint16_t word(unsigned s,unsigned i)const{
        unsigned start=g.offsets[s];uint16_t value=g.words[start+i];
        if(i && g.words[start]>>10==32)value+=i==1?g.anchorX:g.anchorY;
        return value;
    }
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
        if(active || *view.pendingCount || *view.pendingLength || view.buffered)return false;
        g=view;first=stage=*g.matched;count=accepted=0;
        active=eligible();return active;
    }
    bool accept(uint16_t value){
        if(!active || !eligible() || value!=word(stage,count))return false;
        if(count && g.words[g.offsets[stage]]>>10==32){
            int anchor=count==1?g.anchorX:g.anchorY;
            if(int16_t(value)-int16_t(g.words[g.offsets[stage]+count])!=anchor)return false;
        }
        last=value;++accepted;
        if(++count==size()){++stage;count=0;}
        return true;
    }
    void materialize(){
        if(!active)return;
        active=false;if(!accepted)return;
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
            uint32_t address=((g.origin>>4)+uint32_t(word)-uint32_t(y*152))&0xfffff;
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
    bool borrowed()const{return active;}
};
#endif
