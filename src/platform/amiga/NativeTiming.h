#ifndef POKERI_NATIVE_TIMING_H
#define POKERI_NATIVE_TIMING_H
#include <stdint.h>
// Optional performance observations, separate from the reserved CIA guest
// clock. Tick totals are inclusive (nested categories must not be added
// together). Frequent services
// sample one in 64 calls to bound the observer's cost.
namespace NativeTiming {
enum Kind {Service,BoardTick,Present,Guard,AyTick,AyVbi,BlitWait,VideoBus,Count};
struct Record {uint32_t calls=0,samples=0,maximum=0;uint64_t ticks=0;};
extern Record records[Count];
extern bool active;
extern uint32_t frequency,started,elapsed,readOverhead;
bool prepare();
void begin();
void end();
void release();
uint32_t now();
class Scope {
    Record *record=nullptr;uint32_t start=0;
public:
    Scope(Kind kind,unsigned mask=0,bool enabled=true){if(active && enabled){Record &r=records[kind];if(!(r.calls++&mask)){record=&r;start=now();}}}
    ~Scope(){if(record){uint32_t n=now()-start;++record->samples;record->ticks+=n;if(n>record->maximum)record->maximum=n;}}
};
}
#endif
