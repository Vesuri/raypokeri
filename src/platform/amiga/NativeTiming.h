#ifndef POKERI_NATIVE_TIMING_H
#define POKERI_NATIVE_TIMING_H
#include <stdint.h>
// Opt-in counters and VBI PC samples. Never call ReadEClock in a hot scope.
namespace NativeTiming {
enum Kind {Service,BoardTick,Present,Guard,AyTick,AyVbi,BlitWait,VideoBus,Count};
struct Sample {uint32_t pc,cycles,context;};
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
extern uint32_t frequency,started,elapsed;
bool prepare();
void begin();
void end();
void release();
uint32_t benchmarkClock(); // whole-batch boundaries only
void mark(Point,uint32_t cycles,uint32_t pc);
inline void routine(Routine routine){
#ifdef POKERI_DISPATCH_COUNTS
    if(active)++routines[routine];
#endif
}
inline void dispatch(unsigned kind){if(active && kind<48)++kinds[kind];}
inline void hook(unsigned index){if(active && index<4096)++hooks[index];}
class Scope {
    unsigned previous=Count;bool enabled=false;
public:
    Scope(Kind kind,unsigned=0,bool requested=true):enabled(active && requested){if(enabled){previous=context;context=kind;++calls[kind];}}
    ~Scope(){if(enabled)context=previous;}
};
}
#endif
