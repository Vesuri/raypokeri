#ifndef POKERI_HOST_ACRTC_TIMING_FIFO_H
#define POKERI_HOST_ACRTC_TIMING_FIFO_H
// Host research only. The policy supplies command formats, explicit durations
// and completion effects; this scheduler makes no physical-clock assumption.
#include <array>
#include <cstdint>
#include <vector>
#include <limits>
namespace pokeri_research {
template<class Policy> class AcrtcTimingFifo {
    Policy &policy;
    std::array<uint16_t,8> fifo{};
    unsigned head=0,count=0;
    std::vector<uint16_t> command;
    int expected=0;
    uint64_t remaining=0;
    bool half=false;
    uint8_t high=0;
    void fail(const char *s){if(!error)error=s;}
    void finish(){
        if(!policy.complete(command,now)){fail("ACRTC timing: command completion refused");return;}
        ++completed;command.clear();expected=0;
    }
    void drain(){
        while(!error && !remaining && count){
            uint16_t word=fifo[head];head=(head+1)&7;--count;
            if(command.empty()){
                expected=policy.format(word);
                if(!expected || expected < -2 || expected>131072){fail("ACRTC timing: unsupported command format");return;}
            }
            command.push_back(word);
            if(command.size()==2 && expected<0)
                expected=expected==-1?2+unsigned(word):2+2*unsigned(word);
            if(expected>0 && command.size()==unsigned(expected)){
                if(!policy.duration(command,remaining)){fail("ACRTC timing: unsupported command duration");return;}
                if(!remaining)finish();
            }
        }
    }
public:
    uint64_t now=0,completed=0;
    const char *error=nullptr;
    explicit AcrtcTimingFifo(Policy &p):policy(p){}
    // Manual pp.61–62: WFE/WFR concern the eight-word FIFO, independently
    // of CED. A partially collected command can have an empty write FIFO.
    uint8_t status()const{
        return uint8_t((count?0:1)|(count<8?2:0)|
                      (!remaining && command.empty() && !count?0x20:0)|(error?0x80:0));
    }
    bool irq(uint8_t enables)const{return (status()&enables)!=0;}
    unsigned queuedWords()const{return count;}
    unsigned collectedWords()const{return unsigned(command.size());}
    uint64_t busyTicks()const{return remaining;}
    bool partialByte()const{return half;}
    // The existing 8-bit board protocol stages high then low; selecting a
    // new address cancels that staging without aborting a collected command.
    void addressSelected(){half=false;}
    void write8(uint8_t value){
        if(error)return;
        if(!half){high=value;half=true;return;}
        half=false;
        if(count==8){fail("ACRTC timing: write FIFO overflow");return;}
        fifo[(head+count)&7]=uint16_t((uint16_t(high)<<8)|value);++count;
        drain();
    }
    void write16(uint16_t value){write8(uint8_t(value>>8));write8(uint8_t(value));}
    void tick(uint64_t ticks){
        if(error)return;
        if(ticks>std::numeric_limits<uint64_t>::max()-now){fail("ACRTC timing: clock overflow");return;}
        while(!error && remaining && ticks>=remaining){
            ticks-=remaining;now+=remaining;remaining=0;finish();drain();
        }
        if(error)return;
        if(remaining)remaining-=ticks;
        now+=ticks;
    }
    // ABT cancels both the running command and the FIFO. No cancelled
    // command may commit pixels or a read-FIFO result afterwards.
    void abort(){head=count=0;command.clear();expected=0;remaining=0;half=false;error=nullptr;}
};
}
#endif
