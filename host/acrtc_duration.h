#ifndef POKERI_HOST_ACRTC_DURATION_H
#define POKERI_HOST_ACRTC_DURATION_H
#ifndef POKERI_HOST_ACRTC_TIMING
#error Duration geometry observation requires the isolated research renderer
#endif
#include "../src/board/Hd63484.h"
#include <algorithm>
#include <limits>
namespace pokeri_research {
// Approximate table counts from HD63484 UM printed p.173. Geometry comes from
// the existing renderer, not a measurement of silicon. Irregular PAINT uses an
// explicit scan-run extension of the rectangular formula (INFERRED).
struct AcrtcDuration {
    struct Geometry {
        uint64_t dots=0,runs=0;int lastX=0,lastY=0;
        static void dot(void *context,int x,int y){
            auto &g=*static_cast<Geometry*>(context);
            if(!g.dots || y!=g.lastY || x!=g.lastX+1)++g.runs;
            ++g.dots;g.lastX=x;g.lastY=y;
        }
    };
    static uint64_t extent(uint16_t x){int v=int16_t(x);return uint64_t(v<0?-v:v)+1;}
    static uint64_t line(int x,int y,int ex,int ey){return std::max(std::abs(ex-x),std::abs(ey-y));}
    static bool counts(const pokeri::Hd63484 &v,const std::vector<uint16_t>&w,
                       uint64_t &cycles,bool &inferred){
        cycles=0;inferred=false;if(w.empty())return false;
        unsigned op=w[0],g=op>>10;auto format=pokeri::Hd63484::formats[g];
        if(!format.words || (op&format.reserved))return false;
        int n=format.words;
        if(n<0){if(w.size()<2)return false;n=n==-1?2+unsigned(w[1]):2+2*unsigned(w[1]);}
        if(w.size()!=unsigned(n))return false;
        const uint16_t *p=w.data()+1;uint64_t P=(op&7)<4?4:6;
        int x=int16_t(v.parameter[0x12]),y=int16_t(v.parameter[0x13]);
        switch(g){
        case 1:cycles=8;break;
        case 2:case 3:cycles=6;break;
        case 6:cycles=4*uint64_t(p[0])+8;break;
        case 17:cycles=12;break;
        case 18:case 19:cycles=8;break;
        case 22:cycles=(2*extent(p[1])+8)*extent(p[2])+12;break;
        case 32:case 33:cycles=56;break;
        case 35:cycles=P*line(x,y,int16_t(x+int16_t(p[0])),int16_t(y+int16_t(p[1])))+18;break;
        case 38:case 39:
            cycles=8;
            for(unsigned i=0;i<p[0];++i){
                int ex=int16_t(p[1+2*i]+(g==39?x:0));
                int ey=int16_t(p[2+2*i]+(g==39?y:0));
                cycles+=P*line(x,y,ex,ey)+16;x=ex;y=ey;
            }break;
        case 42:case 43:case 45:case 47:case 50:{
            // Dry run on a complete private copy; reads, writes, CP, caches and
            // callbacks of the authoritative renderer remain untouched.
            if(v.surface || v.cardCache || v.receivingCommand() || v.error)return false;
            pokeri::Hd63484 copy=v;Geometry geometry;
            copy.commandLog=nullptr;copy.ar=0;
            copy.researchDot=Geometry::dot;copy.researchContext=&geometry;
            for(uint16_t word:w)if(!copy.writeFifoWord(word))return false;
            if(copy.error)return false;
            inferred=true;
            if(g==50)cycles=geometry.dots?18*geometry.dots+102*geometry.runs-58:0;
            else cycles=(g==43 || g==47?10:8)*geometry.dots+(g==42?66:g==43?90:g==45?18:96);
            break;
        }
        case 49:cycles=(P*extent(p[0])+8)*extent(p[1])+18;break;
        case 51:cycles=18;break;
        default:
            if(g>=52 && g<=55)cycles=(P*((p[0]&255)+1)+10)*((p[0]>>8)+1)+20;
            else if(g>=56 && g<=59)cycles=((P+2)*extent(p[2])+10)*extent(p[3])+70;
            else return false; // No pretend success for unimplemented drawing.
        }
        return true;
    }
    static bool convert(uint64_t count,uint64_t boardHz,uint64_t tableHz,uint64_t &ticks){
        if(!boardHz || !tableHz || count>(std::numeric_limits<uint64_t>::max()-(tableHz-1))/boardHz)return false;
        ticks=(count*boardHz+tableHz-1)/tableHz;return true;
    }
};
}
#endif
