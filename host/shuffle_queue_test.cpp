#include "../src/native/ShuffleQueue.h"
#include "../src/board/Hd63484.h"
#include <cassert>
#include <cstdio>
using namespace pokeri;
int main(){
    std::array<uint8_t,256> control{};for(unsigned i=0;i<256;++i)control[i]=uint8_t(i);
    auto original=control;
    ShuffleDisplay display;display.capture(control);control.fill(0);
    unsigned changed=0;display.exchange(control,[&](unsigned,uint8_t){++changed;});
    assert(changed==ShuffleDisplay::size && control[3]==0 && control[6]==6 && control[0xd9]==0xd9 && control[0xea]==0xea);
    display.exchange(control,[](unsigned,uint8_t){});for(auto x:control)assert(!x);
    ShuffleQueue q;const unsigned begin=0x1000,end=begin+4008;
    assert(q.mark(begin+4,begin,begin,end,original));assert(!q.held && q.nextPointer()==begin+4);
    assert(!q.consume(begin+2));assert(q.consume(begin+4));assert(q.held && !q.nextPointer());
    assert(q.mark(end,begin+4,begin,end,original));assert(q.mark(begin+2,begin+4,begin,end,original));
    // Snapshot inside a hold retains FIFO boundaries and captured display state.
    State save;q.state(save);ShuffleQueue restored;State load(save.bytes);restored.state(load);
    assert(restored.held && restored.count==3 && restored.markers[restored.head].display.bytes==q.markers[q.head].display.bytes);
    assert(restored.release());assert(restored.nextPointer()==end);assert(restored.consume(end));assert(restored.release());
    assert(restored.nextPointer()==begin+2);assert(!restored.consume(begin));assert(restored.consume(begin+2));assert(restored.release());assert(!restored.active());
    // A pointer at the exact ring end is equivalent to the first byte.
    q.reset();assert(q.mark(begin,end,begin,end,original));assert(q.held);assert(q.release());
    q.reset();assert(q.mark(begin,begin+2,begin,end,original));assert(q.nextPointer()==end);assert(q.consume(begin));assert(q.release());
    // Repeated wraps of the marker queue do not grow storage or lose order.
    q.reset();for(unsigned round=0;round<100;++round){
        for(unsigned i=0;i<32;++i)assert(q.mark(begin+2*(i+1),begin,begin,end,original));
        for(unsigned i=0;i<32;++i){assert(q.consume(begin+2*(i+1)));assert(q.release());}
    }
    for(unsigned i=0;i<32;++i)assert(q.mark(begin+2*(i+1),begin,begin,end,original));
    assert(!q.mark(begin+66,begin,begin,end,original));assert(q.error && q.count==32);
    q.reset();assert(!q.error && !q.active() && !q.held);
    assert(!q.mark(begin+1,begin,begin,end,original));q.reset();
    assert(!q.mark(end+2,begin,begin,end,original));q.reset();
    assert(!q.mark(begin,begin,begin,end+2,original));q.reset();
    assert(!q.release());q.reset();
    assert(q.mark(begin+2,begin,begin,end,original));assert(!q.mark(begin+4,begin,begin+2,end+2,original));
    // Corrupt snapshots fail before exposing an out-of-bounds marker.
    auto bad=save.bytes;bad[0]=32;State corrupt(bad);bool failed=false;
    try{restored.state(corrupt);}catch(const std::runtime_error&){failed=true;}assert(failed);
    Hd63484 video;video.control[3]=Hd63484::WFE;
    assert(video.irq());video.presentationBusy=true;
    assert(!video.irq() && !(video.statusNow()&(Hd63484::WFE|Hd63484::WFR)) && (video.statusNow()&Hd63484::CED));
    video.presentationBusy=false;assert(video.irq());
    video.presentationBusy=true;video.writeFifoWord(0x8000);assert(video.error);
    puts("PASS: bounded shuffle queue, display-state isolation, wrap, overflow/reset, snapshot validation and coherent FIFO backpressure");
}
