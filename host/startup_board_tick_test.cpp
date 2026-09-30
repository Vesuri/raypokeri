// Compare batched quiet time with the original 1 ms Board::tick sequence.
#include "../src/board/Board.h"
#include "../src/native/StartupBudget.h"
#include <cassert>
#include <cstdio>
#include <memory>
using namespace pokeri;
int main(){
    Config c;c.cpuHz=8000000;c.systemHz=100;c.inputHz=50;c.watchdogMs=10;c.watchdogResetUs=100;
    std::unique_ptr<Board> a(new Board(c)),b(new Board(c));
    a->peer.enabled=b->peer.enabled=true;a->ay.clockHz=b->ay.clockHz=1000000;
    Board::TimingSnapshot time;
    assert(!a->timingSnapshot(time)); // threshold cache is not valid before first tick
    uint32_t rng=3;unsigned grouped=0;
    for(unsigned trial=0;trial<300;++trial){
        rng=rng*1664525u+1013904223u;
        a->watchdogKick();b->watchdogKick();
        unsigned initial=rng%80000;
        a->tick(initial);b->tick(initial);
        for(auto *v:{a.get(),b.get()}){
            v->pia[0].flags[0]=v->pia[0].flags[1]=v->pia[2].flags[1]=0;
            v->peer.state=trial%4;
        }
        assert(a->timingSnapshot(time));
        unsigned ticks=startupQuietTicks(trial%10,uint32_t(time.systemPhase),uint32_t(time.inputPhase),
            time.watchdogAge,time.warning,time.reset,true,true);
        grouped+=ticks>1;
        auto system=a->systemEdges,input=a->inputEdges;
        for(unsigned i=1;i<=ticks;++i){
            a->tick(8000);
            if(i<ticks){
                assert(a->systemEdges==system && a->inputEdges==input);
                assert(!a->pia[2].flags[1] && !a->resetRequested);
            }
        }
        b->tick(ticks*8000);
        State expected,actual;a->state(expected);b->state(actual);
        assert(expected.bytes==actual.bytes);
    }
    assert(grouped>50);
    ++a->config.watchdogResetUs;assert(!a->timingSnapshot(time));
    a->tick(0);assert(a->timingSnapshot(time));
    std::printf("PASS: 300 complete Board states after quiet batching (%u grouped), no skipped intermediate edge; stale watchdog caches refused\n",grouped);
}
