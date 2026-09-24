#ifndef POKERI_NATIVE_REPLAY_H
#define POKERI_NATIVE_REPLAY_H
#include <stdint.h>
#include <stddef.h>
namespace pokeri {
enum ReplayKind { ReplayBus=1,ReplayIrq=2,ReplayInput=3,ReplayReset=4,ReplayEnd=5,ReplayPeripheralReset=6,ReplayConfig=7 };
struct ReplayEvent {uint32_t kind,instruction,cycle,pc,a,b;};
// Bounded, allocation-free parser suitable for a file loaded before takeover.
class ReplayReader {
    const uint8_t *cursor,*end;uint32_t instruction=0,cycle=0,pc=0;bool valid=false,finished=false;
    bool integer(uint32_t &n){n=0;for(unsigned i=0;i<5;++i){if(cursor==end)return false;unsigned b=*cursor++;if(i==4 && (b&0xf0))return false;n|=uint32_t(b&127)<<(7*i);if(!(b&128))return i==0 || (b&127)!=0;}return false;}
public:
    ReplayReader(const uint8_t *data,size_t size):cursor(data),end(data+size){
        static const uint8_t magic[]={'P','K','R','E','P','L','A','Y',2};
        if(size<sizeof magic)return;
        for(unsigned i=0;i<sizeof magic;++i)if(*cursor++!=magic[i])return;
        valid=true;
    }
    bool next(ReplayEvent &e){
        if(!valid || finished)return false;
        uint32_t di,dc,dp;e.a=e.b=0;
        if(!integer(e.kind) || e.kind<ReplayBus || e.kind>ReplayConfig || !integer(di) || !integer(dc) ||
           !integer(dp) || ((e.kind==ReplayIrq || e.kind==ReplayInput || e.kind==ReplayEnd || e.kind==ReplayConfig) && !integer(e.a)) ||
           ((e.kind==ReplayIrq || e.kind==ReplayInput || e.kind==ReplayEnd) && !integer(e.b)) || di>0xffffffffu-instruction || dc>0xffffffffu-cycle){valid=false;return false;}
        int32_t delta=int32_t((dp>>1)^uint32_t(-int32_t(dp&1)));
        if((delta<0 && (0u-uint32_t(delta))>pc) || (delta>=0 && uint32_t(delta)>0x7ffff-pc)){valid=false;return false;}
        pc+=delta;e.pc=pc;instruction+=di;cycle+=dc;e.instruction=instruction;e.cycle=cycle;
        if(e.kind==ReplayConfig && (instruction || cycle || e.pc>9)){valid=false;return false;}
        if(e.kind!=ReplayConfig && (e.pc>=0x80000 || (e.pc&1))){valid=false;return false;}
        if(e.kind==ReplayEnd){finished=true;if(cursor!=end || e.b)valid=false;}
        return valid;
    }
    bool complete()const{return valid && finished && cursor==end;}
};
}
#endif
