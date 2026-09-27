#ifndef POKERI_COMMAND_SEQUENCE_OBSERVER_H
#define POKERI_COMMAND_SEQUENCE_OBSERVER_H
#include <cstdint>
namespace pokeri {
// Diagnostic only: observes completed commands without changing their execution.
// Exact words except translated AMOVE coordinates. A shape match is an upper
// bound on cache eligibility; it does not prove the entry context is admissible.
struct CommandSequenceObserver {
    const uint16_t *words,*offsets;
    unsigned count,firstRaster;
    uint32_t starts=0,complete=0,unobserved=0,mismatches=0;
    uint32_t barrierStages[80]={},mismatchStages[80]={};
    uint32_t interruptedComplete=0,maxObservations=0;
    unsigned matched=0,observations=0;
    int anchorX=0,anchorY=0;
    bool anchor=false;
    CommandSequenceObserver(const uint16_t *w,const uint16_t *o,unsigned n,unsigned raster):
        words(w),offsets(o),count(n),firstRaster(raster){}
    bool matches(const uint16_t *w,unsigned n) {
        unsigned begin=offsets[matched],length=offsets[matched+1]-begin;
        if(length!=n || w[0]!=words[begin])return false;
        if(w[0]==0x8000){
            int dx=int16_t(w[1])-int16_t(words[begin+1]);
            int dy=int16_t(w[2])-int16_t(words[begin+2]);
            if(!anchor){anchorX=dx;anchorY=dy;anchor=true;}
            return dx==anchorX && dy==anchorY;
        }
        for(unsigned i=1;i<n;++i)if(w[i]!=words[begin+i])return false;
        return true;
    }
    void command(const uint16_t *w,unsigned n,bool executed) {
        if(!n)return;
        if(!executed || !matches(w,n)){
            if(matched){++mismatches;++mismatchStages[matched];}
            matched=0;anchor=false;observations=0;
            if(!executed || !matches(w,n))return;
        }
        if(!matched)++starts;
        if(++matched==count){
            ++complete;
            if(observations)++interruptedComplete;else ++unobserved;
            if(observations>maxObservations)maxObservations=observations;
            matched=0;anchor=false;observations=0;
        }
    }
    // Actual external pixel observation, not internal renderer reads or status.
    void observe(){if(matched>firstRaster){++barrierStages[matched];++observations;}}
};
}
#endif
