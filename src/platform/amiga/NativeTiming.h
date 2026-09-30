#ifndef POKERI_NATIVE_TIMING_H
#define POKERI_NATIVE_TIMING_H
#include <stdint.h>
extern "C" volatile uint16_t nativeProfileEnabled;
// Opt-in counters and VBI PC samples. Never call ReadEClock in a hot scope.
namespace NativeTiming {
enum Kind {Service,BoardTick,Present,Guard,AyTick,AyVbi,BlitWait,VideoBus,
#ifdef POKERI_TIME_LEDGER
    ShortCall,Command,Backpressure,HookExec,Prologue,IdleWait,
#endif
    Count};
#ifdef POKERI_TIME_LEDGER
// Separate diagnostic build only. A reserved, free-running CIA timer counts
// E-clock ticks (1.41 us); VBI and every scope extend it past its 92 ms wrap.
// INTENA, not SR, masks the extension race: callers may be in user mode.
extern volatile uint8_t *ledgerLow,*ledgerHigh;
extern uint16_t ledgerLast;
extern uint32_t ledgerTicks,ledgerReads;
#ifdef POKERI_TIMING_TEST
uint32_t ledgerNow();
#else
inline uint32_t ledgerNow(){
    volatile uint16_t *intena=(volatile uint16_t*)0xdff09a,*intenar=(volatile uint16_t*)0xdff01c;
    uint16_t enabled=*intenar&0x4000;*intena=0x4000;
    unsigned high=*ledgerHigh,low=*ledgerLow,again=*ledgerHigh;
    if(again!=high)low=*ledgerLow;
    uint16_t now=uint16_t((again<<8)|low);
    ++ledgerReads;ledgerTicks+=uint16_t(ledgerLast-now);ledgerLast=now;uint32_t result=ledgerTicks;
    if(enabled)*intena=0xc000;
    return result;
}
#endif
// Inclusive E-clock ticks by [enclosing kind][kind]; Count as parent is top level.
struct Ledger {
    uint32_t clock,cycles,guest,hooked,shortCalls,dispatches;
    uint32_t ticks[Count+1][Count],calls[Count+1][Count];
    uint32_t opTicks[64],opCalls[64],opMax[64];
};
// Cumulative per-kind totals sampled once per VBI, after the swap test.
struct FrameRecord {uint32_t clock,cycles,guest,service,command,present,blitWait,shortCall;};
// Commands of at least 2 ms (1419 E-ticks) keep their first eight words.
struct SlowCommand {uint32_t clock,ticks,cycles;uint16_t words[8];};
constexpr unsigned SlowCapacity=4096;
extern Ledger ledger,*ledgerMarks,*startupMarks;
extern volatile uint32_t fastCache;
void startupMark(unsigned stage,uint32_t cycles);
extern uint32_t kindTicks[Count],ledgerReadCost; // read cost: ticks per 256 reads
extern FrameRecord *frameRecords;
extern SlowCommand *slowCommands;
extern volatile uint32_t frameCount,slowCount;
extern unsigned commandGroup;
extern uint16_t commandWords[8];
struct Event {uint32_t clock,cycles,type,a,b,reads;};
constexpr unsigned EventCapacity=16384;
// Per-card endpoints include currently open scopes; totals remain inclusive.
struct CardCost {uint32_t type,clock,cycles,reads,guest,hooked,dispatches,shortCalls,observerTicks;uint32_t ticks[Count];};
constexpr unsigned CardCostCapacity=1024;
extern CardCost *cardCosts;
extern uint32_t cardCostCount,cardCostDropped;
void cardCost(unsigned type);
extern Event *events;
extern volatile uint32_t eventCount,eventDropped;
void event(unsigned type,uint32_t a,uint32_t b,uint32_t cycles);
void frameRecord();
uint32_t slowCycles();
void ledgerSnapshot(Ledger &out);
#endif
struct Sample {uint32_t pc,cycles,context;};
struct PlaySample {uint32_t cycles,frames,guest,hooked,loops;};
extern PlaySample *playSamples;
extern uint32_t mainLoops;
#ifdef POKERI_RELEASE
inline void playMark(unsigned,uint32_t,uint32_t){}
#else
void playMark(unsigned index,uint32_t cycles,uint32_t frames);
#endif
struct Milestone {uint32_t seen,samples,cycles,pc,guest,hooked,polls;};
enum Point {GuestStart,FirstSwap,ChecksumEnd,DrainEnd,PlayReady,Finished,RamTestEnd,ChecksumStart,PointCount};
// Direct nativeDispatch call sites / CPU-control branches, not inclusive time.
enum Routine {RClockPause,RGuestCharge,RPreparedHook,RGenericHook,RCpuRte,
    RCpuUsp,RCpuReadSr,RCpuLogicSr,RBoardReset,RPushException,RGuardCheck,
    RBoardIrq,RBoardTick,RLiveInputs,RColdSetup,RPresentation,RVideoStatus,
    RClockCalibration,RoutineCount};
extern uint32_t calls[Count],kinds[48],*hooks,routines[RoutineCount];
extern Sample *samples;
extern volatile uint32_t sampleCount,dropped;
extern Milestone milestones[PointCount];
extern unsigned context;
extern bool active;
inline bool isActive(){
#ifdef POKERI_NO_PROFILE_SUPPORT
    return false;
#else
    return active;
#endif
}
extern uint32_t frequency,started,elapsed;
#ifdef POKERI_RELEASE
inline bool prepare(){return false;}
inline void begin(){}
inline void end(){}
inline void release(){}
inline uint32_t benchmarkClock(){return 0;}
inline void mark(Point,uint32_t,uint32_t){}
#else
bool prepare();
void begin();
void end();
void release();
uint32_t benchmarkClock(); // whole-batch boundaries only
void mark(Point,uint32_t cycles,uint32_t pc);
#endif
inline void routine(Routine routine){
#ifdef POKERI_DISPATCH_COUNTS
    if(isActive())++routines[routine];
#endif
}
inline void dispatch(unsigned kind){if(isActive() && kind<48)++kinds[kind];}
inline void hook(unsigned index){if(isActive() && index<4096)++hooks[index];}
class Scope {
    unsigned previous=Count;bool enabled=false;
#ifdef POKERI_TIME_LEDGER
    unsigned kind=Count;uint32_t start=0;Scope *caller=nullptr;
    static Scope *top;
public:
    static void snapshot(uint32_t *out,uint32_t now){
        for(unsigned k=0;k<Count;++k)out[k]=kindTicks[k];
        for(Scope *s=top;s;s=s->caller)
            if(s->kind!=AyTick && s->kind!=AyVbi && s->previous!=s->kind)out[s->kind]+=now-s->start;
    }
    // Audio scopes run in the VBI before its scanline-limited swap test;
    // keep them untimed so the observer cannot defer presentation.
    Scope(Kind k,unsigned=0,bool requested=true):enabled(isActive() && requested){
        if(enabled){previous=context;context=kind=k;++calls[k];if(k==Command)commandGroup=64;if(k!=AyTick && k!=AyVbi)start=ledgerNow();caller=top;top=this;}
    }
    ~Scope(){
        if(!enabled)return;
        top=caller;
        if(kind==AyTick || kind==AyVbi){context=previous;return;}
        uint32_t delta=ledgerNow()-start;
        ledger.ticks[previous][kind]+=delta;++ledger.calls[previous][kind];
        if(previous!=kind)kindTicks[kind]+=delta;
        // A Command scope wraps one FIFO write; only a completed command logs its group.
        if(kind==Command && commandGroup<64){
            ledger.opTicks[commandGroup]+=delta;++ledger.opCalls[commandGroup];
            if(delta>ledger.opMax[commandGroup])ledger.opMax[commandGroup]=delta;
            if(delta>=1419 && slowCommands && slowCount<SlowCapacity){
                SlowCommand &c=slowCommands[slowCount++];c.clock=start;c.ticks=delta;c.cycles=slowCycles();
                for(unsigned i=0;i<8;++i)c.words[i]=commandWords[i];
            }
        }
        context=previous;
    }
#else
public:
    Scope(Kind kind,unsigned=0,bool requested=true):enabled(isActive() && requested){if(enabled){previous=context;context=kind;++calls[kind];}}
    ~Scope(){if(enabled)context=previous;}
#endif
};
}
#endif
