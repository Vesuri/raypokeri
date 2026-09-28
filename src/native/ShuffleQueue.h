#ifndef POKERI_SHUFFLE_QUEUE_H
#define POKERI_SHUFFLE_QUEUE_H
#include <array>
#include <stdint.h>
#ifndef POKERI_FREESTANDING
#include "../board/State.h"
#endif
namespace pokeri {
// Presentation-only state. These are precisely the control bytes read by the
// host/native scanout implementations; drawing parameters and IRQ enables are
// never rolled back. Assets and FIFO words stay in the original guest ring.
struct ShuffleDisplay {
    static constexpr unsigned size=62;
    std::array<uint8_t,size> bytes{};
    static unsigned address(unsigned i){return i==0?2:i<5?3+i:i<29?0x80+i-5:i<61?0xc0+i-29:0xea;}
    template<class Controls> void capture(const Controls &control){for(unsigned i=0;i<size;++i)bytes[i]=control[address(i)];}
    template<class Controls,class Changed> void exchange(Controls &control,Changed changed){
        for(unsigned i=0;i<size;++i){unsigned a=address(i);uint8_t old=control[a];
            if(old!=bytes[i]){changed(a,bytes[i]);control[a]=bytes[i];}bytes[i]=old;}
    }
};
// One two-pass shuffle has 30 markers. Fixed storage bounds all buffering; the
// original 4,008-byte ring still supplies its own producer backpressure.
struct ShuffleQueue {
    static constexpr unsigned capacity=32;
    struct Marker {uint32_t pointer=0;ShuffleDisplay display;};
    std::array<Marker,capacity> markers{};
    unsigned head=0,count=0;
    uint32_t begin=0,end=0;
    bool held=false;
    const char *error=nullptr;
    void reset(){head=count=0;begin=end=0;held=false;error=nullptr;}
    bool active()const{return count!=0;}
    bool pointerValid(uint32_t p)const{return !(p&1) && p>=begin && p<=end;}
    uint32_t normalized(uint32_t p)const{return p==end?begin:p;}
    // The assembly feeder observes its cursor before the ring-wrap tail.
    uint32_t nextPointer()const{return !count || held?0:markers[head].pointer==begin?end:markers[head].pointer;}
    bool fail(const char *why){if(!error)error=why;return false;}
    bool consume(uint32_t cursor){
        if(error || held || !count)return false;
        if(normalized(cursor)!=normalized(markers[head].pointer))return false;
        held=true;return true;
    }
    template<class Controls> bool mark(uint32_t producer,uint32_t consumer,uint32_t first,uint32_t last,const Controls &control){
        if(error)return false;
        if((first|last)&1 || last<first || last-first!=4008)return fail("shuffle command ring layout changed");
        if(count && (begin!=first || end!=last))return fail("shuffle command ring moved while queued");
        begin=first;end=last;
        if(!pointerValid(producer) || !pointerValid(consumer))return fail("shuffle cursor outside command ring");
        if(count==capacity)return fail("shuffle marker queue overflow");
        Marker &m=markers[(head+count)&(capacity-1)];m.pointer=producer;m.display.capture(control);++count;
        if(count==1)consume(consumer);
        return true;
    }
    bool release(){if(!held || !count)return fail("shuffle release without a presented frame");held=false;head=(head+1)&(capacity-1);--count;return true;}
    ShuffleDisplay &display(){return markers[head].display;}
#ifndef POKERI_FREESTANDING
    void state(State &s){
        if(error)throw std::runtime_error(error);
        s.fields(head,count,begin,end,held);
        for(auto &m:markers)s.fields(m.pointer,m.display.bytes);
        if(head>=capacity || count>capacity || (held && !count) || (count && ((begin|end)&1 || end<begin || end-begin!=4008)))
            throw std::runtime_error("invalid shuffle queue state");
        for(unsigned i=0;i<count;++i)if(!pointerValid(markers[(head+i)&(capacity-1)].pointer))throw std::runtime_error("invalid shuffle marker state");
    }
#endif
};
}
#endif
