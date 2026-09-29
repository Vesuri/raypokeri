#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include "Native.h"
#include "NativeTiming.h"
#include "PaulaAy.h"
#include "AmigaScreen.h"
#include "AmigaInput.h"
#include "AmigaHardware.h"
#include "NvramFile.h"
#include "board/Board.h"
#include "native/Hook.h"
#include "native/PreparedHook.h"
#include "native/LiveClock.h"
#include "native/DelayBudget.h"
#ifdef POKERI_CACHE_BATCH
#include "native/CachedBatch.h"
extern "C" pokeri::CachedBatch nativeBatch{};
extern "C" void nativeBatchFinish();
static_assert(offsetof(pokeri::CachedBatch,translated)==116 && offsetof(pokeri::CachedBatch,first)==716 && offsetof(pokeri::CachedBatch,stage)==720 && offsetof(pokeri::CachedBatch,count)==724,"batch CPU-test layout");
static_assert(offsetof(pokeri::CachedBatch,cursor)==0 && offsetof(pokeri::CachedBatch,limit)==4,"batch cursor ABI");
#endif
#ifdef POKERI_STARTUP_FAST_FORWARD
#include "native/StartupBudget.h"
#endif
#include "native/BootPolicy.h"
#include "native/ShuffleWait.h"
#include "native/ShuffleQueue.h"
#include "Startup.h"
#include "native/Replay.h"
#include <stddef.h>
inline void *operator new(size_t,void *address) noexcept {return address;}
#include "../../../amiga/generated/NativeTables.h"
using namespace pokeri;
#ifdef POKERI_CARD_CACHE
#include "board/CardBackCache.h"
// ABI consumed by CachedRaster.s; fail the build if the borrowed view moves.
using CachedRasterGrant=pokeri::CardBackCache::RasterGrant;
static_assert(sizeof(CachedRasterGrant)==92 && sizeof(pokeri::CardBackCache::Progress)==12 && sizeof(pokeri::Hd63484::CommandCount)==4,"cached raster native widths");
static_assert(offsetof(CachedRasterGrant,absolute)==88,"cached absolute offset");
static_assert(offsetof(CachedRasterGrant,controls)==84,"cached raster controls offset");
static_assert(offsetof(CachedRasterGrant,words)==0,"cached raster words offset");
static_assert(offsetof(CachedRasterGrant,offsets)==4,"cached raster offsets offset");
static_assert(offsetof(CachedRasterGrant,progress)==8,"cached raster progress offset");
static_assert(offsetof(CachedRasterGrant,buffered)==12,"cached raster buffered offset");
static_assert(offsetof(CachedRasterGrant,pending)==16,"cached raster pending offset");
static_assert(offsetof(CachedRasterGrant,parameter)==20,"cached raster parameter offset");
static_assert(offsetof(CachedRasterGrant,matched)==24,"cached raster matched offset");
static_assert(offsetof(CachedRasterGrant,used)==28,"cached raster used offset");
static_assert(offsetof(CachedRasterGrant,pendingCount)==32,"cached raster pendingCount offset");
static_assert(offsetof(CachedRasterGrant,pendingLength)==36,"cached raster pendingLength offset");
static_assert(offsetof(CachedRasterGrant,writeHigh)==40,"cached raster writeHigh offset");
static_assert(offsetof(CachedRasterGrant,status)==44,"cached raster status offset");
static_assert(offsetof(CachedRasterGrant,work)==48,"cached raster work offset");
static_assert(offsetof(CachedRasterGrant,stopped)==52,"cached raster stopped offset");
static_assert(offsetof(CachedRasterGrant,cpuTried)==56,"cached raster cpuTried offset");
static_assert(offsetof(CachedRasterGrant,cpuData)==60,"cached raster cpuData offset");
static_assert(offsetof(CachedRasterGrant,commands)==64,"cached raster commands offset");
static_assert(offsetof(CachedRasterGrant,anchorX)==68,"cached raster anchorX offset");
static_assert(offsetof(CachedRasterGrant,anchorY)==72,"cached raster anchorY offset");
static_assert(offsetof(CachedRasterGrant,origin)==76,"cached raster origin offset");
static_assert(offsetof(CachedRasterGrant,rectangleWork)==80,"cached raster rectangleWork offset");

#include "../../../amiga/generated/CardBackRecipe.h"
static pokeri::CardBackCache *nativeCardCache=nullptr;
static uint16_t *nativeCardStorage=nullptr;
static volatile uint32_t nativeCardPrepareTicks=0;
static volatile uint32_t nativeCardDmaTicks[16]={};
static volatile uint32_t nativeWhiteBenchTicks[2]={};
#endif
#if defined(POKERI_CARD_OBSERVER) || (defined(POKERI_TIME_LEDGER) && defined(POKERI_CARD_CACHE))
#include "board/CommandSequenceObserver.h"
#include "../../../amiga/generated/CardBackRecipe.h"
pokeri::CommandSequenceObserver nativeCardObserver(card_recipe::words,card_recipe::offsets,79,7);
#endif
// Wall-time ledger scopes exist only in the TIME_LEDGER diagnostic build.
#ifdef POKERI_TIME_LEDGER
#define LEDGER_SCOPE(name,kind) NativeTiming::Scope name(NativeTiming::kind)
alignas(4) static uint8_t prologueStorage[sizeof(NativeTiming::Scope)];
#else
#define LEDGER_SCOPE(name,kind)
#endif
struct DosLibrary *DOSBase=nullptr;
extern "C" {
void *pokeriAllocateUninitialized(unsigned long);
Registers nativeRegisters;
uint8_t nativeServiceStack[32768];
uint32_t nativeReturnStack,nativeOsUsp,nativePrepareStack,nativeOldLevel3,nativeOldLevel6,nativeOldLevel2,nativeOldLevel4;
void nativeLevel4();void nativeLevel3();void nativeLevel6();void nativeLevel2();
[[noreturn]] void nativeAbort();
[[noreturn]] void nativePrepareAbort();
uint16_t nativePhysicalSr,nativePhysicalResume;
uint16_t nativeSkipHardwareTests=0;
uint16_t nativeExtendedFrame=0,nativeFrameBytes=6;
uint32_t nativeReadVbr();
void nativeWriteVbr(uint32_t);
uint32_t nativeFastBoundary=0,nativeRomBegin=0,nativeRomEnd=0,nativeRamBegin=0,nativeRamEnd=0;
volatile uint32_t nativeStatus=0,nativeInstructions=0,nativeInterrupts=0,nativeLastPc=0,nativeCycles=0,nativeVectorsRestored=0;
const char *nativeError=nullptr;
void nativeEntry();void nativeLineA();void nativeTrace();void nativeFault();
#define TRAP(n) void nativeTrap##n();
TRAP(0) TRAP(1) TRAP(2) TRAP(3) TRAP(4) TRAP(5) TRAP(6) TRAP(7) TRAP(8) TRAP(9) TRAP(10) TRAP(11) TRAP(12) TRAP(13) TRAP(14) TRAP(15)
}
static Board *board;
static Hd63484 *videoDevice; // borrowed from Board; avoids repeated large member offsets
static uint8_t *boardAllocation,*rom,*guard,*replayData;
static PreparedHook preparedHooks[sizeof(hooks)/sizeof(*hooks)];
static bool addressSelectorEnabled=true;
static bool genericHooks=false,feedFusion=true,feedLoop=true,idleHook=false;
static bool shuffleEnabled=false,shuffleActive=false,shuffleQueued=false,shuffleSwap=false;
static uint32_t shuffleFrame=0,shuffleTicket=0;
static ShuffleQueue shuffleQueue;
extern "C" uint32_t nativeShuffleNextPointer=0;
extern "C" volatile uint32_t nativeShuffleSteps=0,nativeShuffleWaitFrames=0,nativeShuffleAyWrites=0,nativeShufflePeak=0;
static uint32_t shuffleAyStart=0;
extern "C" __attribute__((noinline)) void nativeShufflePresented(){asm volatile("" ::: "memory");}
// mask bit 15: guarded longword compare/test; bit 1 selects A0/D4 (else A2/D0),
// bit 0 selects TST/2 bytes (else CMP/4 bytes). address then holds the value.
struct ShortStatus {uint32_t pc,address;uint16_t mask,cycles;uint32_t calls,guard,body;uint16_t length,promote;uint32_t reserved;};
static_assert(sizeof(ShortStatus)==32 && offsetof(ShortStatus,guard)==16 && offsetof(ShortStatus,length)==24,"assembly short descriptor layout");
extern "C" void nativeShortStatusGuard(),nativeShortStatusRead(),nativeShortSentinelGuard(),nativeShortSentinelRead(),nativeShortControlGuard(),nativeShortControlRead(),nativeShortPiaGuard(),nativeShortPiaRead(),nativeShortIoGuard(),nativeShortIoRead(),nativeShortTrapRead(),nativeShortVideoGuard(),nativeShortVideoWrite();
static ShortStatus shortDescriptor(uint32_t pc,uint32_t address,uint16_t mask,uint16_t cycles){
    void (*guard)()=nativeShortStatusGuard,(*body)()=nativeShortStatusRead;
    unsigned length=4,promote=0;
    if(mask&0x8000){guard=nativeShortSentinelGuard;body=nativeShortSentinelRead;length=mask&1?2:4;}
    else if(mask&0x4000){guard=nativeShortControlGuard;body=nativeShortControlRead;length=0;promote=3;}
    else if(mask&0x2000){guard=nativeShortPiaGuard;body=nativeShortPiaRead;length=0;promote=2;}
    else if(mask&0x1000){guard=nativeShortIoGuard;body=nativeShortIoRead;length=mask&0x20?6:mask&0x40?2:4;promote=2;}
    else if(mask&0x0800){guard=nativeShortVideoGuard;body=nativeShortVideoWrite;length=(mask&1) && !(mask&4)?6:4;promote=2;}
    return {pc,address,mask,cycles,0,uint32_t(guard),uint32_t(body),uint16_t(length),uint16_t(promote),0};
}
extern "C" {
ShortStatus nativeShortTraps[16]={};
uint16_t nativeTrapNumber=0;
ShortStatus nativeShortStatus[sizeof(hooks)/sizeof(*hooks)+sizeof(controls)/sizeof(*controls)]={};
uint16_t nativeShortCount=sizeof(nativeShortStatus)/sizeof(*nativeShortStatus),nativeShortEnabled=1,nativeDiagnostic=1;
uint16_t nativeShortPending=1; // bit 0: clock/IRQ work; bit 1: frame/quit during a short service
uint8_t nativeCachedVideoStatus=0;
uint32_t nativeShortDrainPc=0,nativeShortDrained=0;
void nativeRingBenchmark(),nativeRingHead(),nativeRingStatus(),nativeRingWrite(),nativeRingExit();
uint32_t nativeRingBenchTicks[2]={},nativeRegisterBenchTicks[2][2]={};
void nativeShortAddressWrite();
#ifdef POKERI_FIFO_CONTROL_FUSION
void nativeShortFifoControl(),nativeFifoControlBenchmark(),nativeFifoControlFirst(),nativeFifoControlMiddle(),nativeFifoControlLast(),nativeFifoControlEnd();
uint32_t nativeFifoControlBenchTicks[2]={};
#endif
Hd63484::AddressSelector nativeVideoSelector={};
static_assert(sizeof(Hd63484::AddressSelector)==12 && sizeof(bool)==1,"assembly address selector layout");
uint32_t nativeAddressBenchTicks[2]={},nativeStackBenchTicks[2]={};
void nativeStackBenchmarkLoop(),nativeStackBenchmarkOpcode();
void nativeUserTrapBenchmarkLoop(),nativeUserTrapBenchmarkOpcode(),nativeUserTrapBenchmarkTarget();
uint32_t nativeUserTrapBenchTicks[2]={};
void nativeShortFeedLoopWrite(),nativeShortFeedRead(),nativeFeedBenchmarkLoop(),nativeFeedBenchmarkOpcode(),nativeFeedBenchmarkWrite(),nativeFeedBenchmarkTarget();
uint32_t nativeScreenBenchTicks[2]={};
uint32_t nativeFeedLoopWords=0,nativeFeedLoopTurns=0,nativeFeedLoopSaved=0;
uint16_t nativeFeedLoopFast=1,nativeInlineFeedEnabled=1,nativeRegisterFeedEnabled=1;
#ifdef POKERI_CACHED_RASTER
CachedRasterGrant nativeRasterGrant{};
uint32_t nativeRasterGrantActive=0,nativeRasterHits=0,nativeRasterBenchBytes=1024,nativeRasterBenchTicks[4]={},nativeWhiteRasterTicks[4]={};
bool nativeRasterEnabled=true;
#ifdef POKERI_CACHED_ABSOLUTE
bool nativeRasterAbsoluteEnabled=true;
#else
bool nativeRasterAbsoluteEnabled=false;
#endif
#ifdef POKERI_CACHED_CONTROLS
bool nativeRasterControlsEnabled=true;
#else
bool nativeRasterControlsEnabled=false;
#endif
#ifdef POKERI_RASTER_CHUNKS
uint32_t nativeChunkRasterTicks[2][3]={};
#endif
uint32_t nativeBenchCacheBits=0;
static void revokeRasterGrant(){
#ifdef POKERI_CACHE_BATCH
    nativeBatchFinish();
#endif
    nativeRasterGrantActive=0;
}
#else
static void revokeRasterGrant(){}
#endif
uint32_t nativeFeedInlineCount=0,nativeFeedInlineWords=0,nativeInlineBenchTicks[2]={},nativePatternBenchTicks[2]={},nativeScrollBenchTicks[2]={};
uint16_t nativeHeaderFeedEnabled=1; // validated header-only acceptance
uint32_t nativeFeedHeaderGrant=0,nativeFeedHeaderWords=0,nativeHeaderBenchTicks[2]={};
int *nativeFeedInlineLength=nullptr;
const Hd63484::CommandFormat *nativeFeedFormats=Hd63484::formats;
uint16_t *nativeFeedInlineWord=nullptr;
unsigned *nativeFeedInlinePending=nullptr;
uint8_t *nativeFeedInlineHigh=nullptr;
uint32_t nativeFeedTarget=0,nativeFeedTests=0,nativeFeedBranches=0,nativeFeedWrites=0,nativeFeedBenchTicks[2]={},nativeDrawingBenchTicks[3]={},nativeCardBenchTicks[2]={};
uint32_t nativeShortGuest=0,nativeShortNominal=0,nativeShortCalls=0,nativeShortCharge[256]={};
}
struct PreparedAccess {uint32_t physical;};
static PreparedAccess preparedAccesses[sizeof(accesses)/sizeof(*accesses)];
static uint8_t originalVectors[12];
static uint32_t romBase,ramBase,guardBase,replaySize,lastGuardCycle,liveStopCycles,guardCursor;
extern "C" uint32_t nativeVirtualUsp=0,nativeVirtualSsp=0;
extern "C" uint16_t nativeStackSwitchEnabled=1;
extern "C" uint16_t nativeUserTrapEnabled=1;
static uint32_t liveTicks=0;
// A reserved CIA timer counts only the intervals outside native services.
bool nativeGuestTimerPrepare();void nativeGuestTimerRelease();
extern "C" volatile uint8_t *nativeGuestTimerControl,*nativeGuestTimerLow,*nativeGuestTimerHigh;
extern "C" volatile uint16_t nativeClockEnabled;
extern "C" volatile uint16_t nativeClockRunning=0;
extern "C" uint32_t nativeClockResumePc=0;
#ifdef POKERI_STARTUP_PROFILE
extern "C" uint32_t nativeStartupTicks[3]={};
static void startupTimestamp(unsigned slot){
    // Read the CIA-A TOD high/mid/low latch once at each startup boundary.
    // Calibration against the existing PAL VBI count is part of the capture.
    // These three diagnostic calls are outside recurring guest services.
    Disable();
    unsigned high=*(volatile uint8_t*)0xbfea01;
    unsigned mid=*(volatile uint8_t*)0xbfe901;
    unsigned low=*(volatile uint8_t*)0xbfe801;
    Enable();
    nativeStartupTicks[slot]=(high<<16)|(mid<<8)|low;
}
#endif
static uint32_t guestClockPhase=0;
#ifdef POKERI_STARTUP_FAST_FORWARD
static bool startupFast=false;
static uint16_t startupDelayOpcode=0,startupCabinetTicks=0;
static uint32_t startupPresentFrame=~0u;
#endif
extern "C" volatile uint32_t pendingFrames=0;
// 0 retains the old scale/contract; 1 corrects units only; 2 enables option C.
extern "C" uint16_t nativeClockMode=2;
static LiveClock liveClock;
// Request 4 from the separately measured acceptance-workload lower bounds.
// Boot keeps its independently calibrated 1.5 cap. CPU probes may lower both.
static uint16_t playClockRatio=64,cpuClockLimit=80,playClockWindow=3;
static bool clockDisplayCalibrated=false;
extern "C" volatile uint32_t nativeClockRaw=0,nativePollMin=0xffffffffu,nativePollMax=0,nativePollCount=0,nativePollTotal=0;
extern "C" __attribute__((noinline)) void nativeClockSampleReady(){asm volatile("" ::: "memory");}
extern "C" uint16_t nativePollSamples[256],nativeCalibrationSamples[64];
uint16_t nativePollSamples[256],nativeCalibrationSamples[64];
static uint32_t previousPollD1=0;static bool uninterruptedPoll=false;
extern "C" uint64_t nativeClockCharged[3]={},nativeClockObserved=0;
#ifdef POKERI_CLOCK_INLINE_ACCOUNT
__attribute__((always_inline)) inline
#endif
static void accountGuestCycles(uint32_t cycles,unsigned source=0){
    if(NativeTiming::active)nativeClockCharged[source]+=cycles;
    if(nativeClockMode==2
#ifdef POKERI_STARTUP_FAST_FORWARD
       && !startupFast
#endif
    )cycles=liveClock.grant(cycles,source!=0,pendingFrames,liveTicks>=2?160000:guestClockPhase+(liveTicks?80000:0));
#ifdef POKERI_STARTUP_FAST_FORWARD
    if(startupFast)cycles=startupWorkCycles(cycles,source!=0,liveClock.ratioSixteenths);
#endif
    guestClockPhase+=cycles;
#ifdef POKERI_STARTUP_FAST_FORWARD
    const unsigned quantum=startupFast?8000:80000;
#else
    const unsigned quantum=80000;
#endif
    while(guestClockPhase>=quantum){guestClockPhase-=quantum;++liveTicks;}
}
static bool liveIrqActive=false;
static uint64_t liveCycles=0;
static PaulaAy paula;
static AmigaScreen screen;
static AmigaSurface videoSurface;
static bool liveRequested=false,displayRequested=false;
extern "C" uint16_t nativeBenchmarkRequested=0;
extern "C" uint32_t nativeBenchTicks[6]={},nativeBenchShortTicks[2]={};
#ifdef POKERI_FEED_FLOOR_BENCHMARK
extern "C" uint16_t nativeFeedFloorBypass=0;
extern "C" uint32_t nativeFeedFloorTicks[4][2]={};
#endif
#ifdef POKERI_IRQ_BENCHMARK
extern "C" uint32_t nativeIrqBenchTicks[4]={};
#endif
#ifdef POKERI_CLOCK_BENCHMARK
extern "C" uint32_t nativeClockBenchTicks[24][2]={};
#endif
extern "C" void nativeShortBenchmarkLoop(),nativeShortBenchmarkControl(),nativeShortBenchmarkOpcode();
extern "C" volatile uint32_t nativeBenchSink=0;
static uint32_t lastPresentCycle=0;
#ifdef POKERI_CARD_PRESENT
static uint32_t presentedCardHits=0,lastCardPresentFrame=0;
static bool cardFrameSeen=false;
extern "C" uint32_t nativeEarlyCardFrames=0;
#endif
extern "C" volatile uint32_t nativeBootVerified=0;
extern "C" __attribute__((noinline)) void nativeBootReady(){asm volatile("" ::: "memory");}
static ReplayReader *reader;static ReplayEvent nextEvent;static bool haveEvent,diagnostic=true;
extern "C" volatile uint32_t nativeClockOverhead=0,nativeClockMinimum=0,nativeClockMaximum=0;
extern "C" volatile uint16_t nativeClockCalibrating=0;
extern "C" void nativeClockCalibrationCode();
static Registers clockSavedRegisters;
static uint16_t clockSavedResume;
static uint8_t clockCalibrationStack[64];
static uint32_t clockCalibrationTotal;
extern "C" void nativeSpeedLoop();
extern "C" void nativeSpeedMemory();
extern "C" void nativeSpeedArithmetic();
extern "C" uint32_t nativeSpeedCycles[3]={};
static unsigned speedCalibration=0;
static uint32_t speedMemory[16]={};
static void speedNext(){
    void (*const code[])()={nativeSpeedLoop,nativeSpeedMemory,nativeSpeedArithmetic};
    nativeRegisters.pc=uint32_t(code[speedCalibration-1]);nativeRegisters.d[0]=8192;
    nativeRegisters.a[0]=uint32_t(speedMemory);nativeClockCalibrating=1;
}
static void prepareShortClock(){
    for(unsigned i=0;i<256;++i){uint32_t raw=nativeClockMode?boardClockCycles(i):wordProduct(i,10);
        nativeShortCharge[i]=raw>nativeClockOverhead?raw-nativeClockOverhead:0;}
}
extern "C" void nativeClockEnter(){
    if(nativeClockEnabled){
        uint16_t ticks=uint16_t(0xffff-(uint16_t(*nativeGuestTimerHigh)<<8|*nativeGuestTimerLow));
        nativeClockRaw=nativeClockMode?boardClockCycles(ticks):wordProduct(ticks,10);
    }
}
extern "C" void nativeClockLeave(){
    nativeClockRunning=1;
}
extern "C" void nativeClockPause(){
    if(!diagnostic){
        // The call counter is cumulative. Only write deferred totals when
        // actual work is pending; preserve the separate credit grants/order.
        uint32_t guest=nativeShortGuest,nominal=nativeShortNominal;
        if(guest){nativeShortGuest=0;accountGuestCycles(guest);}
        if(nominal){nativeShortNominal=0;accountGuestCycles(nominal,1);}
        if(nativeClockRunning){
            if(NativeTiming::active)nativeClockObserved+=nativeClockRaw;
            accountGuestCycles(nativeClockRaw>nativeClockOverhead?nativeClockRaw-nativeClockOverhead:0);
        }
    }
    nativeClockRunning=0;
}
extern "C" void nativeClockPauseInterrupt(){
    // Autovector entry (44) versus Line-A (34), plus BTST/BNE.W (24)
    // versus MOVE-to-SR (16) before the identical timer-stop sequence.
    unsigned overhead=nativeClockMode?20:18;
    nativeClockRaw=nativeClockRaw>overhead?nativeClockRaw-overhead:0;
    nativeClockPause();
}
extern "C" void nativeClockCalibrateBegin(){
    if(!nativeClockEnabled)return;
    clockSavedRegisters=nativeRegisters;clockSavedResume=nativePhysicalResume;
    nativeRegisters.pc=uint32_t(nativeClockCalibrationCode);
    nativeRegisters.a[7]=uint32_t(clockCalibrationStack+sizeof(clockCalibrationStack));
    nativePhysicalResume=0x0700;nativeClockCalibrating=1032;clockCalibrationTotal=0;nativeClockMinimum=0xffffffffu;nativeClockMaximum=0;
}
extern "C" void nativeClockCalibrateNext(){
    if(speedCalibration){
        nativeSpeedCycles[speedCalibration-1]=nativeClockRaw>nativeClockOverhead?nativeClockRaw-nativeClockOverhead:1;
        if(++speedCalibration<=3){speedNext();return;}
        // Three synthetic instruction mixes, 12.5% headroom, never above the
        // requested ratio when applied. Keep the ceiling for the separately
        // measured gameplay cap. These probes alone do not validate a workload.
        const uint32_t reference[]={8192*14-2,8192*22-2,8192*28-2};
        for(unsigned i=0;i<3;++i){
            uint32_t limit=(reference[i]<<3)+(reference[i]<<2)+(reference[i]<<1),cost=nativeSpeedCycles[i];
            unsigned ratio=1;while(ratio<80 && cost+nativeSpeedCycles[i]<=limit){cost+=nativeSpeedCycles[i];++ratio;}
            if(ratio<cpuClockLimit)cpuClockLimit=ratio;
            if(ratio<liveClock.ratioSixteenths)liveClock.ratioSixteenths=ratio;
        }
        speedCalibration=0;nativeClockCalibrating=0;
        nativeRegisters=clockSavedRegisters;nativePhysicalResume=clockSavedResume;nativeClockRunning=0;return;
    }
    if(nativeClockCalibrating<=1024){
        clockCalibrationTotal+=nativeClockRaw;
        if(nativeClockRaw<nativeClockMinimum)nativeClockMinimum=nativeClockRaw;
        if(nativeClockRaw>nativeClockMaximum)nativeClockMaximum=nativeClockRaw;
    }
    if(nativeClockCalibrating<=64)nativeCalibrationSamples[64-nativeClockCalibrating]=nativeClockRaw;
    if(--nativeClockCalibrating){nativeRegisters.pc=uint32_t(nativeClockCalibrationCode);return;}
    // Exactly one original NOP executes between the same resume/Line-A paths.
    // Chip-bus contention makes vector-fetch latency phase-dependent. Remove
    // its measured upper bound: native service stalls must not advance the
    // original watchdog. This conservatively undercounts shorter intervals.
    unsigned nop=nativeClockMode?5:4;
    nativeClockOverhead=nativeClockMaximum>nop?nativeClockMaximum-nop:0;
    prepareShortClock();
    if(nativeClockMode==2){speedCalibration=1;speedNext();return;}
    nativeRegisters=clockSavedRegisters;nativePhysicalResume=clockSavedResume;
    nativeClockRunning=0;
}
static unsigned hookCycles(uint32_t pc){
    unsigned low=0,high=sizeof(originalCycles)/sizeof(*originalCycles);
    while(low<high){unsigned mid=(low+high)/2;if(originalCycles[mid].pc<pc)low=mid+1;else high=mid;}
    return low<sizeof(originalCycles)/sizeof(*originalCycles) && originalCycles[low].pc==pc?originalCycles[low].cycles:0;
}
static uint32_t savedVectors[48];
static volatile uint32_t *nativeVectors;
static uint32_t *privateVectors=nullptr,originalVbr=0;
static_assert(offsetof(Registers,a)==32 && offsetof(Registers,pc)==64 && offsetof(Registers,sr)==68,"assembly register layout");extern "C" uint32_t seenFrames=0;static volatile bool installed=false,quitRequested=false;
static uint16_t originalControl[sizeof(controls)/sizeof(*controls)],controlCycles[sizeof(controls)/sizeof(*controls)];
static uint32_t get32(const uint8_t*p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
static uint16_t get16(const uint8_t*p){return (uint16_t(p[0])<<8)|p[1];}
static void put32(uint8_t*p,uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static void put16(uint8_t*p,unsigned n){p[0]=n>>8;p[1]=n;}
static bool fail(const char *s){if(!nativeError)nativeError=s;nativeStatus=0xdead;return false;}
extern "C" void pokeriRuntimeFault(const char *s){fail(s);if(installed)nativeAbort();nativePrepareAbort();}
static bool fileRead(const char *path,void *data,uint32_t size){BPTR f=Open(path,MODE_OLDFILE);if(!f)return fail("cannot open native input");LONG n=Read(f,data,size);uint8_t extra;LONG tail=Read(f,&extra,1);Close(f);return n==LONG(size) && tail==0?true:fail("native input size mismatch");}
static uint32_t canonical(uint32_t a){if(a>=romBase && a-romBase<0x40000)return a-romBase;if(a>=ramBase && a-ramBase<0x40000)return a-ramBase+0x40000;if(a>=guardBase && a-guardBase<0x80000)return a-guardBase+0x80000;return 0xffffffffu;}
static uint32_t relocated(uint32_t a){return a<0x40000?romBase+a:a<0x80000?ramBase+a-0x40000:guardBase+a-0x80000;}
static bool advanceEvent(){haveEvent=reader->next(nextEvent);nativeFastBoundary=diagnostic && haveEvent && !quitRequested?nextEvent.instruction:0;return haveEvent || reader->complete()?true:fail("invalid/truncated replay");}
static bool advanceClock(uint32_t target){NativeTiming::Scope timing(NativeTiming::BoardTick);if(diagnostic && target<nativeCycles)return fail("replay clock reversed");uint32_t delta=target-nativeCycles;
    board->tick(delta);nativeCachedVideoStatus=board->video.statusNow();if(!diagnostic)liveCycles+=delta;nativeCycles=target;return !board->fault || fail(board->faultReason);}
// Live service is bounded to 1 KB; diagnostic replay and exit inspect all 512 KB.
// One complete live sweep takes 512 serviced frames (10.24 s at 50 Hz).
static bool guardRange(unsigned begin,unsigned end){
    const uint32_t *at=(const uint32_t*)(guard+begin),*finish=(const uint32_t*)(guard+end);
    while(at<finish){
        unsigned words=finish-at;if(words>65536)words=65536;
        uint16_t remaining=words-1;uint8_t mismatch;
        const uint32_t expected=0xa5a5a5a5;
        // DBNE ends on the first mismatch, or after every word was checked.
        // Test Z explicitly: with 65,536 words, Dn=$FFFF also occurs on a
        // first-word mismatch, so the counter alone cannot signal success.
        asm volatile("1: cmp.l (%1)+,%3\n\tdbne %0,1b\n\tsne %2"
            : "+d"(remaining),"+a"(at),"=d"(mismatch):"d"(expected):"cc","memory");
        if(mismatch)return false;
    }
    return true;
}
extern "C" volatile uint32_t nativeGuardSelfTest=0;
static bool testGuard(){
    if(!guardRange(0,0x80000))return false;
    for(unsigned offset: {0u,1020u,0x3fffcu,0x40000u,0x7fffcu}){
        uint32_t *word=(uint32_t*)(guard+offset);*word^=1;
        bool detected=!guardRange(0,0x80000);
        bool bounded=guardRange(0,1024)==(offset>=1024);
        *word^=1;if(!detected || !bounded)return false;
    }
    nativeGuardSelfTest=1;return guardRange(0,0x80000);
}
static bool checkGuard(bool incremental=false){
    NativeTiming::Scope timing(NativeTiming::Guard);
    unsigned begin=incremental?guardCursor:0,end=incremental?begin+1024:0x80000;
    if(!guardRange(begin,end))return fail("unhooked device write reached guard");
    if(incremental)guardCursor=end&0x7ffff;
    lastGuardCycle=nativeCycles;return true;
}
static void setSr(uint16_t value){value&=0xa71f;if(value&0x8000)fail("uncovered guest trace mode");Registers&r=nativeRegisters;if((r.sr^value)&0x2000){if(r.sr&0x2000){nativeVirtualSsp=r.a[7];r.a[7]=nativeVirtualUsp;}else{nativeVirtualUsp=r.a[7];r.a[7]=nativeVirtualSsp;}}r.sr=value;}
#ifdef POKERI_EXCEPTION_FRAME_WORDS
extern "C" uint32_t nativeExceptionFrame(uint8_t*,unsigned,uint32_t,const uint8_t*);
#endif
static bool pushException(unsigned vector,unsigned level){
    Registers&r=nativeRegisters;uint16_t sr=r.sr;setSr(uint16_t((sr|0x2000)&~0x8000));
    if(level)r.sr=uint16_t((r.sr&~0x700)|(level<<8));
    uint32_t sp=canonical(r.a[7]-6);if(sp<0x40000 || sp>=0x7fffa)return fail("virtual exception stack outside RAM");
    r.a[7]-=6;
#ifdef POKERI_EXCEPTION_FRAME_WORDS
    r.pc=nativeExceptionFrame(board->memory.data()+sp,sr,r.pc,rom+vector*4);
#else
    put16(board->memory.data()+sp,sr);put32(board->memory.data()+sp+2,r.pc);r.pc=get32(rom+vector*4);
#endif
    return true;
}
static void resetShuffle(){shuffleQueue.reset();shuffleActive=shuffleQueued=false;nativeShuffleNextPointer=0;board->video.presentationBusy=false;}
static void resetCpu(){resetShuffle();liveIrqActive=false;setSr(0x2700);nativeRegisters.a[7]=get32(rom);nativeRegisters.pc=get32(rom+4);nativeVirtualSsp=nativeRegisters.a[7];}
struct Bus:HookBus {
    uint32_t pc;unsigned firstAccess,lastAccess;
    bool access(uint32_t a,unsigned size,bool writing,uint32_t &v){
        uint32_t local=canonical(a);
        // Five audited sentinel accesses observe immutable original vector data.
        if(a<32 && !writing && ((pc==0x616a && a==4)||(pc==0x6170 && a==0)||(pc==0x6186 && a==4)||(pc==0x61ca && a==0)||(pc==0x61e2 && a==8)) && size==4){v=get32(originalVectors+a);return true;}
        if(pc==0x2358 && a==0x91 && size==1 && writing && v==0xc3)return true; // immutable-ROM marker after fast boot
        if(local==0xffffffffu || canonical(a+size-1)!=local+size-1)return fail("hook address outside allocation");
        if(!writing && local<0x40000){
            v=size==1?rom[local]:size==2?get16(rom+local):get32(rom+local);return true;
        }
        if(local>=0x80000){
            unsigned first=firstAccess;
            bool allowed=false;
            while(first<lastAccess){
                const auto &e=accesses[first++];
                if(e.address==local && e.size==size && e.write==writing){allowed=true;break;}
            }
            if(!allowed)return fail("device access outside hook table");
        }
        if(writing && local<0x40000){if(pc==0x2184 || pc==0x2358 || pc==0x25aa)return true;return fail("unexpected write to program image");}
        NativeTiming::Scope videoTiming(NativeTiming::VideoBus,0,local>=0xf6000 && local<0xf6004);
        if(!writing)v=0;
        for(unsigned i=0;i<size;++i){if(writing){uint8_t b=v>>(8*(size-i-1));if(local<0x80000)board->memory[local+i]=b;else {
                // Only control-register writes can change display geometry.
                // FIFO drawing marks Surface dirty separately. Observe each
                // byte so an AR auto-increment is handled in bus order.
                if(((local+i)&~1u)==0xf6002)screen.controlWrite(board->video,b);
                board->write8(local+i,b);
            }}else v=(v<<8)|(local<0x40000?rom[local+i]:local<0x80000?board->memory[local+i]:board->read8(local+i));}
        return !board->fault || fail(board->faultReason);
    }
    bool read(uint32_t a,unsigned n,uint32_t&v)override{return access(a,n,false,v);}bool write(uint32_t a,unsigned n,uint32_t v)override{return access(a,n,true,v);}
};
// Immutable relocation and access-table checks are prepared once. Dynamic EAs
// must still equal an admitted physical address, width and direction on every use.
struct PreparedBus {
    const HookMetadata &metadata;
    uint32_t pc;
    bool access(uint32_t address,unsigned size,bool writing,uint32_t &value){
        for(unsigned i=metadata.first;i<metadata.last;++i){
            const auto &e=accesses[i];
            if(address!=preparedAccesses[i].physical || size!=e.size || writing!=e.write)continue;
            if(e.address>=0xf6000 && e.address+size<=0xf6004){
                NativeTiming::Scope scope(NativeTiming::VideoBus);
                LEDGER_SCOPE(command,Command);
                unsigned offset=e.address-0xf6000;
                if(!writing)value=0;
                for(unsigned byte=0;byte<size;++byte){
                    if(writing){
                        if((offset+byte)>=2)screen.controlWrite(board->video,uint8_t(value>>(8*(size-byte-1))));
                        board->video.write8(offset+byte,value>>(8*(size-byte-1)));
                        if(board->video.error){board->fault=true;board->faultReason=board->video.error;}
                    }else value=(value<<8)|board->video.read8(offset+byte);
                }
                return !board->fault || fail(board->faultReason);
            }
            if(!writing)value=0;
            for(unsigned byte=0;byte<size;++byte){
                if(writing)board->write8(e.address+byte,value>>(8*(size-byte-1)));
                else value=(value<<8)|board->read8(e.address+byte);
            }
            return !board->fault || fail(board->faultReason);
        }
        // ROM/RAM operands, sentinel vectors and faults retain the checked bus.
        Bus fallback;fallback.pc=pc;fallback.firstAccess=metadata.first;fallback.lastAccess=metadata.last;
        return fallback.access(address,size,writing,value);
    }
    bool read(uint32_t a,unsigned n,uint32_t &v){return access(a,n,false,v);}
    bool write(uint32_t a,unsigned n,uint32_t v){return access(a,n,true,v);}
};
static bool applyInput(const ReplayEvent &e){
    unsigned pia=e.a>>16,side=e.a&65535;
    if(pia>4 || (pia<3 && side>1) || (pia==3 && side!=0) || (pia==4 && (side>63 || (e.b>>16)>2)))return fail("invalid replay input");
    if(pia==4){std::vector<uint8_t> packet{uint8_t(side)};if((e.b>>16)==2)packet.push_back(e.b>>8);if(e.b>>16)packet.push_back(e.b);board->peer.enqueue(packet);}
    else if(pia==3)board->serial[side].receive.push_back(e.b);
    else board->pia[pia].input[side]=e.b;
    return true;
}
// Opt-in platform diagnostics exercise the normal key path after boot.
extern "C" __attribute__((noinline)) void nativeUnexpectedReset(){asm volatile("" ::: "memory");}
extern "C" volatile uint32_t nativeLiveWatchdogResets=0,nativeFirstResetPc=0,nativeFirstResetCycle=0;
static bool testInputs=false,testWrap=false,stopOnLiveReset=false;
static uint32_t testInputIndex=0,liveStart=0,lastInputEdge=0;
static bool coldSetup=false;
static pokeri::Startup startup;
extern "C" volatile uint32_t nativeSetupReady=0;
extern "C" __attribute__((noinline)) void nativePlayReady(){asm volatile("" ::: "memory");}
// The same external operator actions used by SDL's clean startup. No CPU,
// accounting RAM, or card state is supplied: the original ROM handles them.
static void coldSetupStep(){
    if(!coldSetup || nativeSetupReady)return;
#ifdef POKERI_TIME_LEDGER
    auto previous=startup.stage;unsigned coins=startup.coins;
#endif
    startup.step(*board,[](unsigned pia,unsigned side,unsigned value){ReplayEvent e{};e.a=(pia<<16)|side;e.b=value;applyInput(e);});
#ifdef POKERI_TIME_LEDGER
    if(startup.stage!=previous)NativeTiming::startupMark(startup.stage,nativeCycles);
    if(startup.coins!=coins)NativeTiming::event(8,startup.coins,0,nativeCycles);
#endif
    if(startup.error){fail(startup.error);return;}
    if(startup.stage==pokeri::Startup::Ready){
#ifdef POKERI_STARTUP_FAST_FORWARD
        if(startupFast){
            startupFast=false;guestClockPhase=liveTicks=0;
            nativeShortGuest=nativeShortNominal=0;
            liveClock.reset(pendingFrames);lastPresentCycle=nativeCycles-160000;
            // A one-time startup transition, not a recurring service operation.
            // Return the original delay instruction before normal play resumes.
            if(!idleHook){put16(rom+0x2442,startupDelayOpcode);CacheClearU();}
            paula.muted=false;
        }
#endif
        nativeSetupReady=1;NativeTiming::playMark(0,nativeCycles,pendingFrames);liveStart=uint32_t(liveCycles);
        if(nativeClockMode==2 && (playClockRatio || playClockWindow!=1)){
            if(playClockRatio)liveClock.ratioSixteenths=playClockRatio<cpuClockLimit?playClockRatio:cpuClockLimit;
            liveClock.windowFrames=playClockWindow;
            liveClock.reset(pendingFrames);
        }
        NativeTiming::mark(NativeTiming::PlayReady,nativeCycles,nativeLastPc);
#ifdef POKERI_STARTUP_PROFILE
        startupTimestamp(2);
#endif
        nativePlayReady();
    }
}
static void diagnosticKeys(){
    struct Key {uint16_t ms;uint8_t code,down;};
    static const Key keys[]={
        {100,0x33,1},{300,0x33,0}, // coin for a clean zero-credit start
        {500,0x40,1},{700,0x40,0}, // deal
        {8500,2,1},{8500,4,1},{8500,5,1},
        {8700,2,0},{8700,4,0},{8700,5,0},
        {10500,0x40,1},{10700,0x40,0}, // draw
        {19520,0x22,1},{19720,0x22,0}, // double
        {23500,0x4f,1},{23700,0x4f,0}, // big
        {28000,0x52,1},{28200,0x52,0}, // lamp panel
        {29000,0x33,1},{29200,0x33,0}, // coin
        {31000,0x50,1},{31200,0x50,0}, // service door
        {35000,0x50,1},{35200,0x50,0}
    };
    if(!testInputs)return;
    while(testInputIndex<sizeof(keys)/sizeof(*keys) &&
          liveCycles-liveStart>=uint32_t(keys[testInputIndex].ms)*uint16_t(8000)){
        const Key &key=keys[testInputIndex++];NativeTiming::playMark(testInputIndex,nativeCycles,pendingFrames);amigaInputKey(key.code,key.down);
    }
}
static bool liveInputs(){
    while(haveEvent){
        if(nextEvent.kind==ReplayInput){if(nextEvent.cycle>liveCycles)break;if(!applyInput(nextEvent))return false;}
        if(!advanceEvent())return false;
    }
    return true;
}
static bool replayBoundary(){
    while(haveEvent && nextEvent.kind!=ReplayBus && nextEvent.kind!=ReplayPeripheralReset && nextEvent.instruction==nativeInstructions){
        ReplayEvent e=nextEvent;if(canonical(nativeRegisters.pc)!=e.pc)return fail("replay boundary PC mismatch");
        if(!advanceClock(e.cycle))return false;
        if(e.kind==ReplayIrq){if(board->irq()!=e.a || board->vector()!=e.b || ((nativeRegisters.sr>>8)&7)>=e.a)return fail("replay interrupt state mismatch");++nativeInterrupts;if(!pushException(e.b,e.a))return false;}
        else if(e.kind==ReplayReset){if(!board->resetRequested)return fail("replay watchdog not due");board->reset();resetCpu();}
        else if(e.kind==ReplayInput){if(!applyInput(e))return false;}
        else if(e.kind==ReplayEnd){nativeLastPc=canonical(nativeRegisters.pc);if(nativeInterrupts!=e.a)return fail("replay IRQ count mismatch");if(!advanceEvent())return false;
            if(displayRequested && !screen.present(board->video,true))return fail(screen.error);
            board->video.flushCard();videoSurface.synchronize();
            nativeBootVerified=1;nativeBootReady();
            if(!liveRequested){nativeStatus=2;return false;}
            NativeTiming::begin();
            if(shuffleEnabled)put16(rom+ShuffleWait::pc,0xaffb);
            diagnostic=false;nativeDiagnostic=0;nativeClockEnabled=1;nativeFastBoundary=0;seenFrames=pendingFrames;liveTicks=0;liveCycles=nativeCycles;liveStart=nativeCycles;liveClock.reset(pendingFrames);
            if(testWrap){nativeCycles=0xffff0000u;lastPresentCycle=nativeCycles;lastGuardCycle=nativeCycles;}
            return true;}
        else return fail("unexpected replay event");
        if(!advanceEvent())return false;
    }
    if(haveEvent && nextEvent.instruction<nativeInstructions)return fail("missed replay boundary");
    return true;
}
// These three audited boot loops contain no other hook or callable path.
// CIA quantization on a fast CPU cannot time a single iteration. Charge its
// actual 68000 branch/decrement cost when consecutive polls prove that path.
static uint32_t previousTimingPc=0xffffffffu,previousTimingCounter=0;
// Hooks pause the guest timer, but must not stop Amiga VBI/audio or the
// blitter queue while a device service runs. Supervisor-mode IRQs chain to
// Exec without touching saved guest registers or injecting a game handler.
class ServiceInterrupts {
    uint16_t saved;
public:
    ServiceInterrupts(){asm volatile("move.w %%sr,%0\n\tmove.w #0x2000,%%sr":"=d"(saved)::"cc","memory");}
    ~ServiceInterrupts(){asm volatile("move.w %0,%%sr"::"d"(saved):"cc","memory");}
};
// Bank 2 does not feed the game's IRQ encoder. These use the exact shared
// PIA/watchdog semantics; no parallel native device state is maintained.
extern "C" unsigned nativeShortPiaWrite(unsigned value,unsigned kind){
    LEDGER_SCOPE(call,ShortCall);
    board->writePia(2,2,uint8_t(value));
    if(kind==2){
        if(coldSetup && !nativeSetupReady)startup.observe(0x2472);
        else {if(NativeTiming::active)++NativeTiming::mainLoops;amigaInputObserve(0x2472,*board);}
    }
    nativeShortPending=(nativeShortPending&1)|((pendingFrames!=seenFrames || quitRequested)?2:0);
    return uint8_t(value);
}
extern "C" unsigned nativeShortPiaReadValue(){
    LEDGER_SCOPE(call,ShortCall);
    unsigned value=board->readPia(2,0);
    nativeShortPending=(nativeShortPending&1)|((pendingFrames!=seenFrames || quitRequested)?2:0);return value;
}
static void shortIoCompleted(){
    unsigned irq=board->irq();
    nativeShortPending=(liveTicks || irq?1:0)|((pendingFrames!=seenFrames || quitRequested || irq>((nativeRegisters.sr>>8)&7))?2:0);
    if(board->fault){fail(board->faultReason);nativeShortPending|=2;}
}
extern "C" unsigned nativeShortIoReadValue(uint32_t address){
    LEDGER_SCOPE(call,ShortCall);
    unsigned value=board->read8(address-guardBase+0x80000);shortIoCompleted();return value;
}
extern "C" unsigned nativeShortIoWriteValue(uint32_t address,unsigned value){
    LEDGER_SCOPE(call,ShortCall);
    board->write8(address-guardBase+0x80000,uint8_t(value));shortIoCompleted();return uint8_t(value);
}
// Exactly the same byte-ordered endpoint operations as PreparedBus. Keep the
// model authoritative, including command completion, FIFO and IRQ side effects.
#ifdef POKERI_CACHE_BATCH
extern "C" void nativeBatchFinish(){
    if(!nativeBatch.borrowed())return;
    nativeBatch.materialize();
    nativeCachedVideoStatus=videoDevice->statusNow();
    nativeFeedInlineCount=nativeFeedHeaderGrant=nativeRasterGrantActive=0;
}
#endif
extern "C" unsigned nativeShortVideoWriteValue(uint32_t address,unsigned value,unsigned kind){
    LEDGER_SCOPE(call,ShortCall);
    Hd63484 &video=*videoDevice;
    unsigned offset=address-guardBase+0x80000-0xf6000;
    LEDGER_SCOPE(command,Command);
#ifdef POKERI_CACHE_BATCH
    nativeBatchFinish();
#endif
    if((kind&2) && offset==2 && video.writeFifoWord(uint16_t(value))){
        // FIFO writes cannot modify display control registers.
    }else if(kind&2){
        if(offset>=2)screen.controlWrite(video,uint8_t(value>>8));
        video.Hd63484::write8(offset,value>>8);
        if(offset+1>=2)screen.controlWrite(video,uint8_t(value));
        video.Hd63484::write8(offset+1,value);
    }else {if(offset>=2)screen.controlWrite(video,uint8_t(value));video.Hd63484::write8(offset,value);}
    if(video.error){board->fault=true;board->faultReason=video.error;}
    nativeCachedVideoStatus=video.statusNow();
    shortIoCompleted();
    nativeFeedInlineCount=0;nativeFeedHeaderGrant=0;revokeRasterGrant();
    if(nativeInlineFeedEnabled && !diagnostic && kind==7 && offset==2)
    {
#ifdef POKERI_CACHED_RASTER
        nativeRasterGrantActive=nativeRasterEnabled && nativeHeaderFeedEnabled && video.cardCache && video.cardCache->rasterGrant(video,nativeRasterGrant,nativeRasterControlsEnabled,nativeRasterAbsoluteEnabled);
#ifdef POKERI_CACHE_BATCH
        if(nativeRasterGrantActive && nativeRegisterFeedEnabled && nativeRasterControlsEnabled && nativeRasterAbsoluteEnabled)
            nativeBatch.begin(nativeRasterGrant);
#endif
#endif
        nativeFeedInlineCount=video.inlineParameters(nativeFeedInlineWord,nativeFeedInlinePending,nativeFeedInlineHigh);
        if(!nativeFeedInlineCount && nativeHeaderFeedEnabled)
            nativeFeedHeaderGrant=video.inlineHeader(nativeFeedInlineWord,nativeFeedInlinePending,nativeFeedInlineHigh,nativeFeedInlineLength);
    }
    return value;
}
#ifdef POKERI_TIME_LEDGER
extern "C" void nativeFeedHeaderStarted(unsigned word){
    if(videoDevice->cardCache)videoDevice->cardCache->wordStart(uint16_t(word));
}
#endif
#ifdef POKERI_FAST_FIFO_VALUE
// Called only by the existing guarded byte-write member of the fused triplet.
// CCR low has no display or FIFO side effects. All other cases keep the
// authoritative general endpoint; interrupt selection/order is unchanged.
extern "C" unsigned nativeFifoControlValue(uint32_t address,unsigned value,unsigned kind){
    Hd63484 &video=*videoDevice;
    if(diagnostic || video.ar!=3 || video.error || board->fault
#ifdef POKERI_CACHE_BATCH
       || nativeBatch.borrowed()
#endif
      )return nativeShortVideoWriteValue(address,value,kind);
    LEDGER_SCOPE(call,ShortCall);
    video.control[3]=uint8_t(value);
    nativeCachedVideoStatus=video.statusNow();
    unsigned irq=board->pia[0].Pia6821::irq() || (nativeCachedVideoStatus&uint8_t(value)) || board->serial[0].Acia6850::irq()?5:0;
    nativeShortPending=(liveTicks || irq?1:0)|((pendingFrames!=seenFrames || quitRequested || irq>((nativeRegisters.sr>>8)&7))?2:0);
    nativeFeedInlineCount=0;nativeFeedHeaderGrant=0;revokeRasterGrant();
    return value;
}
#endif
extern "C" unsigned nativeShortReplayStart(uint32_t physicalPc){
    ++nativeInstructions;uint32_t pc=physicalPc-romBase;
    unsigned index=get16(rom+pc)&0xfff;
    if(index>=nativeShortCount || nativeShortStatus[index].pc!=physicalPc)return fail("short replay site mismatch");
    // These compare/test reads are CPU memory operations, not ReplayBus events.
    if(nativeShortStatus[index].mask&0xc000)return true;
    if(!haveEvent || nextEvent.kind!=ReplayBus || nextEvent.instruction!=nativeInstructions || nextEvent.pc!=pc)
        return fail("short replay I/O boundary mismatch");
    return advanceClock(nextEvent.cycle) && advanceEvent();
}
// No guest state is changed here. An event at the just-completed instruction
// belongs to the ordinary scheduler, using the actual saved intermediate PC/SR.
extern "C" unsigned nativeFeedReplayContinue(){
    return haveEvent && nextEvent.instruction>nativeInstructions && !quitRequested &&
        nativeCycles-lastGuardCycle<160000;
}
extern "C" uint32_t nativeDelayApply(Registers*,uint32_t);
extern "C" uint32_t nativeIdleCalls=0,nativeIdleInstructions=0,nativeIdleCycles=0,nativeIdleWaits=0;
static uint32_t idleBudget(){
    uint32_t iterations=uint16_t(nativeRegisters.d[6]);if(!iterations)iterations=65536;
    uint32_t maximum=iterations<<1;
    if(diagnostic){
        // The dispatch already counted this SUBQ. Stop at the next recorded
        // instruction boundary, including between SUBQ and its BNE.
        if(!haveEvent || nextEvent.instruction<nativeInstructions){fail("idle replay boundary missing");return 0;}
        uint32_t remaining=uint32_t(nextEvent.instruction-nativeInstructions)+1;
        return remaining<maximum?remaining:maximum;
    }
    // The reference delay is an idle point, not extra guest throughput credit.
    // Existing handlers may finish even if they have masked a pending tick.
    if(liveIrqActive || (nativeRegisters.sr&0x700)>=0x500)return 1;
#ifdef POKERI_STARTUP_FAST_FORWARD
    if(startupFast){
        if(quitRequested || pendingFrames!=seenFrames || liveTicks ||
           board->irq()>((nativeRegisters.sr>>8)&7))return 0;
        // 1 ms is the next possible serial-peer edge; timer/input/watchdog
        // edges in the supported profile are integer multiples of this.
        return delaySteps(uint16_t(nativeRegisters.d[6]),startupDelayAvailable(guestClockPhase));
    }
#endif
    for(;;){
        if(quitRequested || pendingFrames!=seenFrames || liveTicks ||
           board->irq()>((nativeRegisters.sr>>8)&7))return 0;
        accountGuestCycles(0);
        if(liveTicks)return 0;
        uint32_t available=liveClock.debt>liveClock.credit?liveClock.debt-liveClock.credit:0;
        uint32_t untilTick=80000-guestClockPhase;if(untilTick<4)untilTick=4;
        if(available>untilTick)available=untilTick;
        uint32_t steps=delaySteps(uint16_t(nativeRegisters.d[6]),available);
        if(steps)return steps;
        if(displayRequested)screen.presentReady();
        // Sleep only in our supervisor service context. Amiga IRQs continue;
        // the next VBI wakes us without executing original code in its ISR.
        LEDGER_SCOPE(wait,IdleWait);
        ++nativeIdleWaits;
        asm volatile("stop #0x2000" ::: "cc","memory");
    }
}
// The producer returns immediately. Device consumption stops at each marker;
// ordinary guest scheduling (including sound) continues while a frame retires.
static bool shuffleBoundary(){
    Registers &r=nativeRegisters;
    uint32_t sp=canonical(r.a[7]);
    if(sp<0x40000 || sp>0x7fffc)return fail("shuffle return stack outside RAM");
    uint32_t target=get32(board->memory.data()+sp);
    if(!ShuffleWait::caller(canonical(target)))return fail("shuffle caller outside verified loop");
    if((r.sr&0x700)>=0x500)return fail("shuffle marker with board IRQs masked");
    const uint8_t *m=board->memory.data();
    if(!shuffleQueue.active())shuffleAyStart=paula.writeCount;
    if(!shuffleQueue.mark(get32(m+0x41326),get32(m+0x4132a),get32(m+0x413be),get32(m+0x413c2),board->video.control))return fail(shuffleQueue.error);
    if(shuffleQueue.count>nativeShufflePeak)nativeShufflePeak=shuffleQueue.count;
    r.pc=target;r.a[7]+=4;accountGuestCycles(16,1); // original RTS, CCR unchanged
    return true;
}
static bool shuffleService(){
    if(!shuffleEnabled || diagnostic)return true;
    if(nativeRegisters.pc==romBase+0x2e62)shuffleQueue.consume(nativeRegisters.a[1]);
    Hd63484 &video=board->video;
    if(shuffleQueue.held){
        unsigned now=pendingFrames;
        if(!shuffleActive){shuffleActive=true;shuffleQueued=false;shuffleFrame=now;}
        nativeShuffleWaitFrames+=now-shuffleFrame;shuffleFrame=now;
        if(!shuffleQueued && (!displayRequested || !screen.presentationPending())){
            if(displayRequested){
                uint8_t ar=video.ar;
                auto changed=[&](unsigned a,uint8_t value){video.ar=uint8_t(a);screen.controlWrite(video,value);};
                shuffleQueue.display().exchange(video.control,changed);video.ar=ar;
                unsigned before=screen.frames;shuffleTicket=screen.swaps+1;
                bool okay=screen.present(video);
                shuffleQueue.display().exchange(video.control,changed);video.ar=ar;
                if(!okay)return fail(screen.error);
                shuffleSwap=screen.frames!=before;
                if(!shuffleSwap)shuffleTicket=now+1;
            }else {shuffleSwap=false;shuffleTicket=now+1;}
            shuffleQueued=true;
        }
        if(shuffleQueued && (shuffleSwap?int32_t(screen.swaps-shuffleTicket)>=0:int32_t(now-shuffleTicket)>=0)){
            if(!shuffleQueue.release())return fail(shuffleQueue.error);
            shuffleActive=shuffleQueued=false;++nativeShuffleSteps;
            if(!shuffleQueue.active())nativeShuffleAyWrites+=paula.writeCount-shuffleAyStart;
            if(NativeTiming::active)nativeShufflePresented();
        }
    }
    video.presentationBusy=shuffleQueue.held;
    nativeShuffleNextPointer=shuffleQueue.nextPointer();
    nativeCachedVideoStatus=video.statusNow();
    return true;
}
#ifdef POKERI_VIDEO_IRQ_FAST
#ifdef POKERI_VIDEO_IRQ_COUNTS
extern "C" uint32_t nativeVideoIrqHits=0;
#endif
// Service-only shortcut. The original control write has completed, grants are
// revoked, and the guest timer is stopped. No guest instruction is replaced.
#ifdef POKERI_VIDEO_IRQ_FRAME_ASM
extern "C" uint32_t nativeCheckVideoIrq(uint32_t pc,uint32_t sp,unsigned physicalSr){
#else
extern "C" uint32_t nativeTryVideoIrq(uint32_t pc,uint32_t sp,unsigned physicalSr){
#endif
    if(diagnostic || NativeTiming::active || !nativeSetupReady || nativeStatus!=1 ||
       pc!=romBase+0x2ebc || nativeClockMode!=2 || !nativeClockEnabled ||
       nativeClockCalibrating || !nativeClockOverhead ||
       (screen.active() && !clockDisplayCalibrated) || (physicalSr&0x2000) ||
       (liveStopCycles && liveCycles>=liveStopCycles))return 0;
#ifdef POKERI_STARTUP_FAST_FORWARD
    if(startupFast)return 0;
#endif
    const uint16_t sr=uint16_t((nativeRegisters.sr&~31)|(physicalSr&31));
    if((sr&0x8000) || ((sr>>8)&7)>=5)return 0;
    const uint32_t ssp=sr&0x2000?sp:nativeVirtualSsp;
    if((ssp&1) || ssp<=6 || ssp<ramBase+6 || ssp>=ramBase+0x40000)return 0;
    const uint32_t target=get32(rom+0x100);
    if((target&1) || !((target>=nativeRomBegin && target<nativeRomEnd) ||
                       (target>=nativeRamBegin && target<nativeRamEnd)))return 0;
    // This IRQ belongs to the existing video vector, with no competing source.
    // Disabled latched PIA flags do not by themselves constitute an IRQ.
    if(board->fault || board->resetRequested || board->video.error ||
       board->pia[0].Pia6821::irq() || board->serial[0].Acia6850::irq() ||
       !board->video.Hd63484::irq() || board->vector()!=0x40)return 0;
    {
        ServiceInterrupts interrupts;
        // May create a due timer tick. A rejection leaves drained totals for
        // the ordinary dispatcher, whose next pause must charge nothing twice.
        nativeClockPause();
        if(liveTicks || pendingFrames!=seenFrames || liveClock.frame!=pendingFrames ||
           (liveClock.credit && liveClock.debt) || quitRequested || nativeShortDrained ||
           shuffleQueue.active() || shuffleActive || shuffleQueued || nativeShuffleNextPointer ||
           board->video.presentationBusy || screen.presentationPending() ||
           nativeCycles-lastPresentCycle>=160000
#ifdef POKERI_CARD_PRESENT
           || (nativeCardCache && nativeCardCache->hits!=presentedCardHits)
#endif
          )return 0;
    }
    // The scope restored IPL7. Close the VBI/quit race before the first guest
    // store; physical callbacks never run original handlers or mutate devices.
    if(pendingFrames!=seenFrames || quitRequested)return 0;
#ifdef POKERI_VIDEO_IRQ_FRAME_ASM
    // Admission is now irrevocable, with physical IPL7 held. Assembly builds
    // the already-validated six-byte frame and performs the virtual stack switch.
    liveIrqActive=true;uninterruptedPoll=false;
    nativeCachedVideoStatus=board->video.statusNow();
#ifdef POKERI_VIDEO_IRQ_COUNTS
    ++nativeVideoIrqHits;
#endif
    return ssp-6;
#else
    nativeRegisters.pc=pc;nativeRegisters.a[7]=sp;nativeRegisters.sr=sr;
    // The preceding exact stack/trace checks prove pushException cannot fail.
    // Reuse its existing user/supervisor switch and frame implementation.
    if(!pushException(0x40,5))return 0;
    ++nativeInterrupts;liveIrqActive=true;uninterruptedPoll=false;
    nativeLastPc=0x2ebc;nativePhysicalSr=uint16_t(physicalSr);
    nativePhysicalResume=sr&31;nativeShortPending=1;
    nativeCachedVideoStatus=board->video.statusNow();
#ifdef POKERI_VIDEO_IRQ_COUNTS
    ++nativeVideoIrqHits;
#endif
    return nativeRegisters.a[7];
#endif
}
#endif
extern "C" unsigned nativeDispatch(unsigned kind){
    nativeFeedInlineCount=0;nativeFeedHeaderGrant=0;revokeRasterGrant(); // no borrow crosses a scheduler boundary
#ifdef POKERI_TIME_LEDGER
    // Masked C prologue until interrupts are re-enabled (asm entry excluded).
    NativeTiming::Scope *prologue=new(prologueStorage) NativeTiming::Scope(NativeTiming::Prologue);
#endif
    NativeTiming::dispatch(kind);
    // ROM and RAM are contiguous. The range check below still rejects PCs
    // outside both; device-address canonicalization is only for data accesses.
    uint32_t timingPc=nativeRegisters.pc-romBase;
    if(NativeTiming::active){
        if(timingPc==0x20be)NativeTiming::mark(NativeTiming::RamTestEnd,nativeCycles,timingPc);
        if(timingPc==0x10fc0)NativeTiming::mark(NativeTiming::ChecksumStart,nativeCycles,timingPc);
    }
    if(!diagnostic && !nativeSkipHardwareTests){
        unsigned loopCycles=timingPc==0x20be?34:timingPc==0x2118?10:timingPc==0x214a?26:0;
        uint32_t counter=timingPc==0x20be?nativeRegisters.d[1]:nativeRegisters.d[2];
        if(nativeClockRunning && kind==10 && loopCycles &&
           previousTimingPc==timingPc && previousTimingCounter==counter+1){
            NativeTiming::routine(NativeTiming::RGuestCharge);accountGuestCycles(loopCycles,2);nativeClockRunning=0;
        }
        previousTimingPc=kind==10?timingPc:0xffffffffu;previousTimingCounter=counter;
    }
    // Resuming directly at another patched instruction executes no original
    // instruction before its exception. Timer quantization is not guest work.
    if(kind==10 && nativeClockResumePc==nativeRegisters.pc)nativeClockRunning=0;
    NativeTiming::routine(NativeTiming::RClockPause);nativeClockPause();
    if(nativeShortDrained){
        if(NativeTiming::active && NativeTiming::milestones[NativeTiming::ChecksumEnd].seen)
            NativeTiming::mark(NativeTiming::DrainEnd,nativeCycles,timingPc);
        nativeShortDrained=0;
    }
#ifdef POKERI_TIME_LEDGER
    prologue->~Scope();
#endif
    ServiceInterrupts serviceInterrupts;
    NativeTiming::Scope timing(NativeTiming::Service,63);
    if(quitRequested){nativeStatus=3;return false;}
    Registers&r=nativeRegisters;r.sr=uint16_t((r.sr&~31)|(nativePhysicalSr&31));uint32_t pc=timingPc;nativeLastPc=pc;
    if(coldSetup && !nativeSetupReady)startup.observe(pc);
    else if(pc==0x2472 || pc==0x246a){
        // A trace may stop BEFORE the hooked instruction at this PC. Count
        // only its actual Line-A execution; the short body counts itself.
        if(NativeTiming::active && kind==10)++NativeTiming::mainLoops;
        amigaInputObserve(pc,*board);
    }
    if(NativeTiming::active && !diagnostic && pc==0x20be && kind==10){
        if(uninterruptedPoll && previousPollD1==r.d[1]+1){
            if(nativeClockRaw<nativePollMin)nativePollMin=nativeClockRaw;
            if(nativeClockRaw>nativePollMax)nativePollMax=nativeClockRaw;
            if(nativePollCount<256)nativePollSamples[nativePollCount]=nativeClockRaw;
            nativePollTotal+=nativeClockRaw;if(++nativePollCount==256)nativeClockSampleReady();
        }
        previousPollD1=r.d[1];uninterruptedPoll=true;
    }else uninterruptedPoll=false;
    if(kind==0)return fail("native CPU exception");
    if(pc>=0x80000)return fail("native PC outside ROM/RAM");
#ifdef POKERI_LIVE_INSTRUCTION_COUNTS
    const bool countInstruction=true;
#else
    const bool countInstruction=diagnostic;
#endif
    if(countInstruction && kind!=11)++nativeInstructions;
    if(!diagnostic && kind>=32 && kind<48){NativeTiming::routine(NativeTiming::RGuestCharge);accountGuestCycles(34,1);}
    if(kind==10){
        unsigned index=get16(rom+pc)&0xfff;
        NativeTiming::hook(index);
        if(!diagnostic && index!=0xffc && index!=0xffb){NativeTiming::routine(NativeTiming::RGuestCharge);accountGuestCycles(index<sizeof(hooks)/sizeof(*hooks)?hookMetadata[index].cycles:index<nativeShortCount?controlCycles[index-sizeof(hooks)/sizeof(*hooks)]:hookCycles(pc),1);}
        if(index<sizeof(hooks)/sizeof(*hooks)){
            const pokeri::Hook &h=hooks[index];if(h.pc!=pc)return fail("Line-A index/site mismatch");
            bool device=hardwareHooks[index];
            if(diagnostic && device){if(!haveEvent || nextEvent.kind!=ReplayBus || nextEvent.instruction!=nativeInstructions || nextEvent.pc!=pc)return fail("replay I/O boundary mismatch");if(!advanceClock(nextEvent.cycle) || !advanceEvent())return false;}
            bool okay;
            if(genericHooks){Bus bus;bus.pc=pc;bus.firstAccess=hookMetadata[index].first;bus.lastAccess=hookMetadata[index].last;NativeTiming::routine(NativeTiming::RGenericHook);okay=executeHook(h,r,bus);}
            else {PreparedBus bus{hookMetadata[index],pc};NativeTiming::routine(NativeTiming::RPreparedHook);LEDGER_SCOPE(hookTiming,HookExec);okay=executePreparedHook(preparedHooks[index],r,bus);}
            if(!okay)return fail("unsupported native hook");
        }else if(index==0xffb){
            if(!shuffleEnabled || diagnostic || pc!=ShuffleWait::pc)return fail("unknown shuffle hook");
            if(!shuffleBoundary())return false;
        }else if(index==0xffc){
            if((!idleHook
#ifdef POKERI_STARTUP_FAST_FORWARD
                && !startupFast
#endif
               ) || pc!=0x2442)return fail("unknown idle hook");
            ++nativeIdleCalls;
            uint32_t steps=idleBudget();
            if(nativeStatus==0xdead)return false;
            if(quitRequested){nativeStatus=3;return false;}
            if(steps){
                uint32_t cycles=nativeDelayApply(&r,steps);
                if(diagnostic)nativeInstructions+=steps-1;
                else accountGuestCycles(cycles,2);
                nativeIdleInstructions+=steps;nativeIdleCycles+=cycles;
            }else if(countInstruction)--nativeInstructions; // no original instruction executed while waiting
        }else if(index==0xffe){if(diagnostic && !videoSurface.tested && !videoSurface.selfTest())return fail("planar blitter self-test failed");
#ifdef POKERI_CARD_CACHE
            if(diagnostic && !videoSurface.cardTested && !videoSurface.cardBlitTest())return fail("card masked-blit self-test failed");
#endif
            r.d[7]=ramBase-0x40000;r.a[6]=0x40b00;r.pc+=6;}
        else if(index==0xffd){
            if(!(r.sr&0x2000))return fail("virtual privilege violation at RESET");
            bool found=false;for(auto offset:resets)if(pc==offset)found=true;if(!found)return fail("unknown RESET hook");
            if(diagnostic){if(!haveEvent || nextEvent.kind!=ReplayPeripheralReset || nextEvent.instruction!=nativeInstructions || nextEvent.pc!=pc)return fail("replay RESET mismatch");if(!advanceClock(nextEvent.cycle)||!advanceEvent())return false;}NativeTiming::routine(NativeTiming::RBoardReset);board->reset();resetShuffle();r.pc+=2;
        }else if(index<nativeShortCount){
            unsigned i=index-sizeof(hooks)/sizeof(*hooks);if(controls[i]!=pc)return fail("CPU-control index/site mismatch");uint16_t op=originalControl[i];
            if((op&0xfff8)!=0x40c0 && !(r.sr&0x2000))return fail("virtual privilege violation at CPU-control hook");
            if(op==0x4e73){NativeTiming::routine(NativeTiming::RCpuRte);uint32_t sp=canonical(r.a[7]);if(sp<0x40000 || sp>=0x7fffa)return fail("RTE stack outside RAM");uint16_t sr=get16(board->memory.data()+sp);r.pc=get32(board->memory.data()+sp+2);r.a[7]+=6;setSr(sr);}
            else if((op&0xfff0)==0x4e60){NativeTiming::routine(NativeTiming::RCpuUsp);unsigned reg=op&7;if(op&8)r.a[reg]=nativeVirtualUsp;else nativeVirtualUsp=r.a[reg];r.pc+=2;}
            else if((op&0xfff8)==0x40c0){NativeTiming::routine(NativeTiming::RCpuReadSr);r.d[op&7]=(r.d[op&7]&0xffff0000)|r.sr;r.pc+=2;}
            else if(op==0x007c || op==0x027c || op==0x0a7c){NativeTiming::routine(NativeTiming::RCpuLogicSr);unsigned operand=get16(rom+pc+2);setSr(op==0x007c?r.sr|operand:op==0x027c?r.sr&operand:r.sr^operand);r.pc+=4;}
            else return fail("unimplemented CPU-control form");
        }else return fail("unknown Line-A opcode");
    }else if(kind>=32 && kind<48){NativeTiming::routine(NativeTiming::RPushException);if(!pushException(kind,0))return false;}
    else if(kind!=9 && kind!=11)return fail("unknown native exception vector");
    if(!shuffleService())return false;
    unsigned pendingIrq=0;
    if(diagnostic){if(!replayBoundary())return false;}
    else {
        // A VBI pauses the guest clock before its callback increments frames.
        // Its return trace has no new guest interval to charge. Publish the
        // new wall deadline and spend already-earned credit anyway; otherwise
        // it waits for another accounting call (often the next VBI).
        if(nativeClockMode==2
#ifdef POKERI_STARTUP_FAST_FORWARD
           && !startupFast
#endif
           && (liveClock.frame!=pendingFrames || (liveClock.credit && liveClock.debt)))accountGuestCycles(0);
        unsigned nowFrames=pendingFrames,frames=nowFrames-seenFrames;seenFrames=nowFrames;
        if(frames){NativeTiming::routine(NativeTiming::RGuardCheck);if(!checkGuard(true))return false;}
        if(((r.sr>>8)&7)<5)liveIrqActive=false;
        // Deliver a pending source before advancing time again. An injected
        // handler must return before the next 100 Hz edge can replace its flag.
        NativeTiming::routine(NativeTiming::RBoardIrq);unsigned irq=board->irq();
        if(!(irq>((r.sr>>8)&7)) && liveTicks && !liveIrqActive){
            --liveTicks;
            // Keep pressed edges latched until the game's next 50 Hz input
            // scan, even when native rendering makes one virtual frame slow.
            NativeTiming::routine(NativeTiming::RBoardTick);
#ifdef POKERI_STARTUP_FAST_FORWARD
            const bool accelerating=startupFast;
            const unsigned quantum=accelerating?8000:80000;
#else
            const unsigned quantum=80000;
#endif
            if(!advanceClock(nativeCycles+quantum))return false;
            NativeTiming::routine(NativeTiming::RLiveInputs);
            if(!liveInputs())return false;
            if((!coldSetup || nativeSetupReady) && uint32_t(board->inputEdges)!=lastInputEdge){
                lastInputEdge=uint32_t(board->inputEdges);diagnosticKeys();amigaInputApply(*board);
            }
#ifdef POKERI_STARTUP_FAST_FORWARD
            // Cabinet protocol setup keeps its existing 10 ms observations.
            if(!accelerating || ++startupCabinetTicks==10){
                startupCabinetTicks=0;
#endif
            NativeTiming::routine(NativeTiming::RColdSetup);coldSetupStep();
#ifdef POKERI_STARTUP_FAST_FORWARD
            }
#endif
            NativeTiming::routine(NativeTiming::RBoardIrq);irq=board->irq();
        }
        if(board->resetRequested){
            if(++nativeLiveWatchdogResets==1){nativeFirstResetPc=canonical(r.pc);nativeFirstResetCycle=uint32_t(liveCycles-liveStart);}
            if(nativeLiveWatchdogResets>1)nativeUnexpectedReset();
            if(stopOnLiveReset)return fail("live watchdog expired");
            NativeTiming::routine(NativeTiming::RBoardReset);board->reset();resetCpu();
        }
        else if(irq>((r.sr>>8)&7)){
            ++nativeInterrupts;liveIrqActive=true;
            NativeTiming::routine(NativeTiming::RPushException);if(!pushException(board->vector(),irq))return false;
        }
        pendingIrq=irq;
    }
#ifdef POKERI_CARD_PRESENT
    bool earlyCard=false;
    if(!diagnostic && nativeSetupReady && displayRequested && !shuffleQueue.active() &&
       nativeCardCache && nativeCardCache->hits!=presentedCardHits &&
       !board->video.cachedPixels && videoSurface.dirtyCard.marked && !videoSurface.changed &&
       (!cardFrameSeen || pendingFrames!=lastCardPresentFrame) && screen.cardPresentationReady())earlyCard=true;
    bool presentationDue=nativeCycles-lastPresentCycle>=160000 || earlyCard;
#else
    bool presentationDue=nativeCycles-lastPresentCycle>=160000;
#endif
#ifdef POKERI_STARTUP_FAST_FORWARD
    // Preparation shows progress at most once a second; Ready forces a refresh.
    if(startupFast)presentationDue=startupPresentFrame==~0u || pendingFrames-startupPresentFrame>=50;
#endif
    if(displayRequested && !shuffleQueue.active() && presentationDue){
        NativeTiming::Scope timing(NativeTiming::Present);
        lastPresentCycle=nativeCycles;
#ifdef POKERI_STARTUP_FAST_FORWARD
        if(startupFast)startupPresentFrame=pendingFrames;
#endif
        screen.outputs(amigaInputLamps(),board->outputs());
#ifdef POKERI_CARD_PRESENT
        uint32_t before=screen.frames;
#endif
        NativeTiming::routine(NativeTiming::RPresentation);if(!screen.present(board->video))return fail(screen.error);
#ifdef POKERI_CARD_PRESENT
        if(screen.frames!=before){
            presentedCardHits=nativeCardCache?nativeCardCache->hits:0;
            lastCardPresentFrame=pendingFrames;cardFrameSeen=true;
            if(earlyCard)++nativeEarlyCardFrames;
        }
#endif
    }
    if(displayRequested)screen.presentReady();
    if(NativeTiming::active){
        uint32_t nextPc=canonical(r.pc);
        if(pc==0x10fcc && r.d[2]==1)NativeTiming::mark(NativeTiming::ChecksumEnd,nativeCycles,nextPc);
        if(NativeTiming::milestones[NativeTiming::ChecksumEnd].seen && pc==0x11040 && (r.sr&4))NativeTiming::mark(NativeTiming::DrainEnd,nativeCycles,pc);
    }
    NativeTiming::routine(NativeTiming::RVideoStatus);nativeCachedVideoStatus=board->video.statusNow();
    if(nativeStatus==0xdead)return false;
    if(!diagnostic && liveStopCycles && liveCycles>=liveStopCycles){nativeLastPc=canonical(r.pc);nativeStatus=4;return false;}
    if(diagnostic && nativeCycles-lastGuardCycle>=160000){NativeTiming::routine(NativeTiming::RGuardCheck);if(!checkGuard())return false;}
    nativeShortPending=(liveTicks || pendingIrq)?1:0;
    nativePhysicalResume=uint16_t(((diagnostic || (liveTicks && !liveIrqActive))?0x8000:0)|(r.sr&31));
    if(!diagnostic && (!nativeClockOverhead || (screen.active() && !clockDisplayCalibrated))){
        clockDisplayCalibrated=screen.active();NativeTiming::routine(NativeTiming::RClockCalibration);nativeClockCalibrateBegin();
    }
    return true;
}
// An explicit isolated diagnostic, before original execution. The audited
// checksum status BTST reads a side-effect-free port. No game loop is replaced.
#ifdef POKERI_READ_ONLY_DMA
uint32_t nativeReadDmaTicks[2]={},nativeReadDmaTotal[2]={};
#endif
extern "C" void nativeProfileBenchmark(){
#ifdef POKERI_CACHED_RASTER
    // Query flags without changing them. Exec also clears caches, before any
    // timed batch here; never call this from a live service or interrupt.
    nativeBenchCacheBits=CacheControl(0,0);
#endif
    ServiceInterrupts benchmarkInterrupts; // timer.device overflow accounting must run
    constexpr unsigned N=512;
    unsigned index=0;
    while(index<sizeof(hooks)/sizeof(*hooks) && hooks[index].pc!=0x10fc6)++index;
    if(index==sizeof(hooks)/sizeof(*hooks)){fail("benchmark hook missing");return;}
    Registers initial=nativeRegisters;initial.pc=romBase+0x10fc6;
    initial.a[0]=relocated(0xf6000);initial.sr=0x2700;
    Bus bus;bus.pc=0x10fc6;bus.firstAccess=hookMetadata[index].first;bus.lastAccess=hookMetadata[index].last;
    // Benchmark ends without resuming the guest. Prevent the first-use clock
    // calibration from redirecting our synthetic registers into the NOP loop.
    nativeClockOverhead=1;nativeClockRunning=0;
    for(unsigned stage=0;stage<6;++stage){
        uint32_t start=NativeTiming::benchmarkClock();
        for(unsigned n=0;n<N;++n){
            uint32_t value=0;
            if(stage<=1 || stage==5)nativeRegisters=initial;
            if(stage==0){if(!nativeDispatch(10))return;value=nativeRegisters.sr;}
            else if(stage==1){if(!executeHook(hooks[index],nativeRegisters,bus)){fail("benchmark hook failed");return;}value=nativeRegisters.sr;}
            else if(stage==2){if(!bus.read(initial.a[0],1,value))return;}
            else if(stage==3)value=board->read8(0xf6000);
            else if(stage==4)value=board->video.read8(0);
            else value=nativeRegisters.sr;
            nativeBenchSink=value;
        }
        nativeBenchTicks[stage]=NativeTiming::benchmarkClock()-start;
    }
    if(nativeCycles || liveTicks || board->fault){fail("benchmark advanced board state");return;}
    nativeStatus=4;
#ifdef POKERI_IRQ_BENCHMARK
    // Attribute existing virtual IRQ service with one timer pair per batch.
    // The saved context and shared FIFO IRQ source are identical in each mode.
    // This explicit pre-game diagnostic executes no original handler or game.
    {
        const uint8_t control=board->video.control[3],status=board->video.status;
        const uint32_t interrupts=nativeInterrupts,lastPc=nativeLastPc;
        const bool activeIrq=liveIrqActive;
        const uint16_t pending=nativeShortPending,resume=nativePhysicalResume;
        Registers irqInitial=initial;irqInitial.pc=romBase+0x2ec0;
        irqInitial.a[7]=ramBase+0x20000;irqInitial.sr=0x2000;
        uint8_t stack[6];for(unsigned i=0;i<6;++i)stack[i]=board->memory[0x5fffa+i];
        board->video.control[3]=1;board->video.status=Hd63484::WFE;
        if(board->irq()!=5 || board->vector()!=0x40){fail("IRQ benchmark source mismatch");return;}
        for(unsigned mode=0;mode<4;++mode){
            const uint32_t begin=NativeTiming::benchmarkClock();
            for(unsigned n=0;n<N;++n){
                nativeRegisters=irqInitial;
                if(mode==0){if(!nativeDispatch(11))return;}
                else if(mode==1){if(!pushException(0x40,5))return;}
                else if(mode==2)nativeBenchSink=(board->irq()<<8)|board->vector();
                else nativeBenchSink=nativeRegisters.sr;
            }
            nativeIrqBenchTicks[mode]=NativeTiming::benchmarkClock()-begin;
        }
        if(nativeInterrupts-interrupts!=N || nativeCycles || liveTicks || pendingFrames || board->fault){fail("IRQ benchmark schedule changed");return;}
        for(unsigned i=0;i<6;++i)board->memory[0x5fffa+i]=stack[i];
        board->video.control[3]=control;board->video.status=status;
        nativeInterrupts=interrupts;nativeLastPc=lastPc;liveIrqActive=activeIrq;
        nativeShortPending=pending;nativePhysicalResume=resume;nativeRegisters=initial;
        nativeCachedVideoStatus=board->video.statusNow();
    }
#endif
#ifdef POKERI_CLOCK_BENCHMARK
    // Isolate deferred accounting that the zero-credit IRQ batch omits. Each
    // pair has identical context setup; timer reads surround whole batches.
    // No board tick, guest instruction, physical timer or wall frame advances.
    {
        if(diagnostic || NativeTiming::active || pendingFrames){fail("clock benchmark context");return;}
        const LiveClock saved=liveClock;
        const uint32_t savedGuest=nativeShortGuest,savedNominal=nativeShortNominal;
        const uint32_t savedPhase=guestClockPhase,savedTicks=liveTicks;
        const uint16_t savedMode=nativeClockMode,savedRunning=nativeClockRunning;
        nativeClockMode=2;
        unsigned row=0;
        for(unsigned debt: {0u,160000u})for(unsigned guest: {0u,208u,4096u})
        for(unsigned nominal: {0u,4200u})for(unsigned phase: {0u,72000u}){
            LiveClock fixture;fixture.ratioSixteenths=64;fixture.windowFrames=3;
            fixture.debt=debt;
            for(unsigned mode=0;mode<2;++mode){
                uint32_t begin=NativeTiming::benchmarkClock();
                for(unsigned n=0;n<N;++n){
                    liveClock=fixture;guestClockPhase=phase;liveTicks=0;
                    nativeShortGuest=guest;nativeShortNominal=nominal;nativeClockRunning=0;
                    if(mode)nativeClockPause();
                    else nativeBenchSink=guestClockPhase;
                }
                nativeClockBenchTicks[row][mode]=NativeTiming::benchmarkClock()-begin;
            }
            const uint32_t earned=(guest<<2)+nominal;
            uint32_t used=debt?earned:0,available=160000-phase;
            if(used>available)used=available;
            uint32_t expectedPhase=phase+used,expectedTicks=0;
            while(expectedPhase>=80000){expectedPhase-=80000;++expectedTicks;}
            if(nativeShortGuest || nativeShortNominal || nativeClockRunning ||
               guestClockPhase!=expectedPhase || liveTicks!=expectedTicks ||
               liveClock.credit!=earned-used || liveClock.debt!=debt-used ||
               nativeCycles || pendingFrames || board->fault){fail("clock benchmark accounting mismatch");return;}
            ++row;
        }
        liveClock=saved;nativeShortGuest=savedGuest;nativeShortNominal=savedNominal;
        guestClockPhase=savedPhase;liveTicks=savedTicks;
        nativeClockMode=savedMode;nativeClockRunning=savedRunning;
    }
#endif
    // Conservative control comparison: the old path includes saved-register
    // preparation and nativeDispatch, but excludes exception entry/exit. The
    // assembly measurement below includes real Line-A/RTE, plus per-iteration
    // virtual-SR setup. No per-operation timer reads or original game mutation.
    uint32_t savedUser=nativeVirtualUsp,savedSupervisor=nativeVirtualSsp;
    uint16_t savedStackMode=nativeStackSwitchEnabled;
    Registers controlInitial=initial;controlInitial.pc=romBase+0xd98;
    nativeVirtualUsp=controlInitial.a[7];
    uint32_t controlStart=NativeTiming::benchmarkClock();
    for(unsigned n=0;n<N;++n){
        nativeRegisters=controlInitial;
        if(!nativeDispatch(10))return;
    }
    nativeStackBenchTicks[0]=NativeTiming::benchmarkClock()-controlStart;
    Registers trapInitial=initial;trapInitial.sr=0;trapInitial.a[7]=nativeRamBegin+0x10000;
    controlStart=NativeTiming::benchmarkClock();
    for(unsigned n=0;n<N;++n){
        nativeRegisters=trapInitial;nativeVirtualSsp=nativeRamBegin+0x20000;
        if(!nativeDispatch(37))return;
    }
    nativeUserTrapBenchTicks[0]=NativeTiming::benchmarkClock()-controlStart;

    // Time actual Line-A entry/RTE in whole batches. No per-access OS calls.
    // Synthetic code is admitted only for this explicit pre-game diagnostic.
    uint32_t oldBegin=nativeRomBegin,oldEnd=nativeRomEnd;
    ShortStatus oldDescriptor=nativeShortStatus[0];
    nativeRomBegin=uint32_t(nativeShortBenchmarkOpcode);nativeRomEnd=nativeRomBegin+4;
    nativeShortStatus[0]=shortDescriptor(nativeRomBegin,relocated(0xf6000),1,16);
    nativeShortEnabled=1;nativeDiagnostic=0;prepareShortClock();nativeCachedVideoStatus=board->video.statusNow();
    uint32_t start=NativeTiming::benchmarkClock();nativeShortBenchmarkLoop();
    nativeBenchShortTicks[0]=NativeTiming::benchmarkClock()-start;
    start=NativeTiming::benchmarkClock();nativeShortBenchmarkControl();
    nativeBenchShortTicks[1]=NativeTiming::benchmarkClock()-start;
    nativeRomBegin=uint32_t(nativeStackBenchmarkOpcode);nativeRomEnd=nativeRomBegin+4;
    nativeShortStatus[0]=shortDescriptor(nativeRomBegin,0xd0ff,0x4001,20);
    nativeStackSwitchEnabled=1;nativeShortPending=0;seenFrames=pendingFrames;
    start=NativeTiming::benchmarkClock();nativeStackBenchmarkLoop();
    nativeStackBenchTicks[1]=NativeTiming::benchmarkClock()-start;

    ShortStatus savedTrap=nativeShortTraps[5];
    uint16_t savedUserTrap=nativeUserTrapEnabled;nativeUserTrapEnabled=1;
    nativeRomBegin=uint32_t(nativeUserTrapBenchmarkOpcode);
    nativeRomEnd=uint32_t(nativeUserTrapBenchmarkTarget)+2;
    nativeShortTraps[5].address=uint32_t(nativeUserTrapBenchmarkTarget);
    nativeVirtualSsp=nativeRamBegin+0x20000;
    nativeShortPending=0;seenFrames=pendingFrames;
    start=NativeTiming::benchmarkClock();nativeUserTrapBenchmarkLoop();
    nativeUserTrapBenchTicks[1]=NativeTiming::benchmarkClock()-start;
    nativeShortTraps[5]=savedTrap;nativeUserTrapEnabled=savedUserTrap;
    nativeVirtualUsp=savedUser;nativeVirtualSsp=savedSupervisor;
    nativeStackSwitchEnabled=savedStackMode;nativeRegisters=initial;
    nativeRomBegin=uint32_t(nativeShortBenchmarkOpcode);nativeRomEnd=nativeRomBegin+4;
    // Same admitted immediate byte MOVE, with a pure address-port endpoint.
    // The synthetic extension is NOP's word; only its low byte selects AR.
    nativeShortStatus[0]=shortDescriptor(nativeRomBegin,relocated(0xf6000),0x0800,16);
    for(unsigned mode=0;mode<2;++mode){
        nativeShortStatus[0].body=uint32_t(mode?nativeShortAddressWrite:nativeShortVideoWrite);
        nativeShortPending=0;seenFrames=pendingFrames;
        start=NativeTiming::benchmarkClock();nativeShortBenchmarkLoop();
        nativeAddressBenchTicks[mode]=NativeTiming::benchmarkClock()-start;
    }
#ifdef POKERI_FIFO_CONTROL_FUSION
    {
        ShortStatus saved[3]={nativeShortStatus[0],nativeShortStatus[1],nativeShortStatus[2]};
        uint32_t begin=nativeRomBegin,end=nativeRomEnd;
        uint8_t control=board->video.control[3],status=board->video.status;
        nativeRomBegin=uint32_t(nativeFifoControlFirst);nativeRomEnd=uint32_t(nativeFifoControlEnd)+2;
        const uint32_t pcs[]={uint32_t(nativeFifoControlFirst),uint32_t(nativeFifoControlMiddle),uint32_t(nativeFifoControlLast)};
        for(unsigned n=0;n<3;++n){
            nativeShortStatus[n]=shortDescriptor(pcs[n],relocated(n==1?0xf6002:0xf6000),n==1?0x0801:0x0800,n==1?16:12);
            nativeShortStatus[n].body=uint32_t(n==1?nativeShortVideoWrite:nativeShortAddressWrite);
            nativeShortStatus[n].reserved=n<2?uint32_t(&nativeShortStatus[n+1]):0;
        }
        board->video.control[3]=0x80;board->video.status&=~Hd63484::CER;
        for(unsigned mode=0;mode<2;++mode){
            nativeShortStatus[0].body=uint32_t(mode?nativeShortFifoControl:nativeShortAddressWrite);
            nativeShortPending=0;seenFrames=pendingFrames;
            start=NativeTiming::benchmarkClock();nativeFifoControlBenchmark();
            nativeFifoControlBenchTicks[mode]=NativeTiming::benchmarkClock()-start;
        }
        for(unsigned n=0;n<3;++n)nativeShortStatus[n]=saved[n];
        board->video.control[3]=control;board->video.status=status;
        nativeRomBegin=begin;nativeRomEnd=end;
    }
#endif
    // Paired synthetic ready/branch/word writes, using the same shared device
    // endpoint and dynamic source guard in each mode. No ROM bytes are copied.
    ShortStatus oldWrite=nativeShortStatus[1];uint32_t oldTarget=nativeFeedTarget;
    nativeRomBegin=uint32_t(nativeFeedBenchmarkOpcode);nativeRomEnd=uint32_t(nativeFeedBenchmarkTarget)+2;
    nativeShortStatus[0]=shortDescriptor(nativeRomBegin,relocated(0xf6000),2,12);
    nativeShortStatus[1]=shortDescriptor(uint32_t(nativeFeedBenchmarkWrite),relocated(0xf6002),0x0807,16);
    nativeShortStatus[0].reserved=uint32_t(&nativeShortStatus[1]);nativeFeedTarget=uint32_t(nativeFeedBenchmarkTarget);
    board->video.ar=2;nativeShortPending=0;
    for(unsigned n=0;n<512;++n)put16(rom+0x40000+n*2,0x0202);
    for(unsigned mode=0;mode<2;++mode){
        nativeShortStatus[0].body=uint32_t(mode?nativeShortFeedRead:nativeShortStatusRead);
        start=NativeTiming::benchmarkClock();nativeFeedBenchmarkLoop();
        nativeFeedBenchTicks[mode]=NativeTiming::benchmarkClock()-start;
    }
    nativeRomBegin=uint32_t(nativeRingHead);nativeRomEnd=uint32_t(nativeRingExit)+2;
    nativeShortStatus[0]=shortDescriptor(uint32_t(nativeRingStatus),relocated(0xf6000),2,12);
    nativeShortStatus[1]=shortDescriptor(uint32_t(nativeRingWrite),relocated(0xf6002),0x0807,16);
    nativeShortStatus[0].reserved=uint32_t(&nativeShortStatus[1]);
    nativeShortStatus[0].body=uint32_t(nativeShortFeedRead);nativeFeedTarget=uint32_t(nativeRingExit);
    for(unsigned n=0;n<256;++n){put16((uint8_t*)nativeRamBegin+n*4,0x0800);put16((uint8_t*)nativeRamBegin+n*4+2,0x3333);}
    board->video.Hd63484::write8(0,0);
    for(unsigned mode=0;mode<2;++mode){
        nativeShortStatus[1].body=uint32_t(mode?nativeShortFeedLoopWrite:nativeShortVideoWrite);
        nativeShortStatus[1].reserved=uint32_t(&nativeShortStatus[0]);
        nativeShortPending=0;seenFrames=pendingFrames;
        start=NativeTiming::benchmarkClock();nativeRingBenchmark();
        nativeRingBenchTicks[mode]=NativeTiming::benchmarkClock()-start;
    }
    // Variable command, 13 intermediate words per 16-word packet. This
    // isolates parameter acceptance from raster work and command completion.
    bool byteCounts=board->video.wptnCountsBytes;board->video.wptnCountsBytes=false;
    unsigned inlineMode=nativeInlineFeedEnabled,headerMode=nativeHeaderFeedEnabled;
    nativeHeaderFeedEnabled=0; // isolate parameter acceptance from header acceptance
    for(unsigned n=0;n<512;++n)put16((uint8_t*)nativeRamBegin+n*2,
        (n&15)==0?0x1800:(n&15)==1?14:uint16_t(n));
    for(unsigned mode=0;mode<2;++mode){
        nativeInlineFeedEnabled=mode;nativeFeedInlineCount=0;revokeRasterGrant();
        nativeShortPending=0;seenFrames=pendingFrames;
        start=NativeTiming::benchmarkClock();nativeRingBenchmark();
        nativeInlineBenchTicks[mode]=NativeTiming::benchmarkClock()-start;
    }
    nativeInlineFeedEnabled=inlineMode;board->video.wptnCountsBytes=byteCounts;
    nativeInlineFeedEnabled=1;
    for(unsigned n=0;n<512;++n)put16((uint8_t*)nativeRamBegin+n*2,(n&1)?0x3333:0x0800);
    for(unsigned mode=0;mode<2;++mode){
        nativeHeaderFeedEnabled=mode;nativeFeedInlineCount=0;nativeFeedHeaderGrant=0;revokeRasterGrant();
        nativeShortPending=0;seenFrames=pendingFrames;
        start=NativeTiming::benchmarkClock();nativeRingBenchmark();
        nativeHeaderBenchTicks[mode]=NativeTiming::benchmarkClock()-start;
    }
#ifdef POKERI_FEED_FLOOR_BENCHMARK
    {
        // No ROM data or original game runs in these batches. The diagnostic
        // bypass deliberately omits the device endpoint to bound feeder cost.
        // Alternate batch order to expose beam-phase/order sensitivity.
        const unsigned savedRegister=nativeRegisterFeedEnabled;
        nativeRegisterFeedEnabled=1;
        uint32_t expectedInstructions=0,expectedNominal=0;
        for(unsigned trial=0;trial<4;++trial)for(unsigned order=0;order<2;++order){
            unsigned mode=order^(trial&1);
            nativeFeedInlineCount=nativeFeedHeaderGrant=0;revokeRasterGrant();
            nativeShortPending=0;seenFrames=pendingFrames;
            const uint32_t instructions=nativeInstructions,nominal=nativeShortNominal;
            nativeFeedFloorBypass=mode;
            uint32_t began=NativeTiming::benchmarkClock();nativeRingBenchmark();
            nativeFeedFloorTicks[trial][mode]=NativeTiming::benchmarkClock()-began;
            nativeFeedFloorBypass=0;
            uint32_t charged=nativeShortNominal-nominal,executed=nativeInstructions-instructions;
            if(!trial && !order){expectedInstructions=executed;expectedNominal=charged;}
            if(!executed || charged!=expectedNominal || executed!=expectedInstructions ||
               nativeCycles || liveTicks || pendingFrames || board->fault || board->video.error){
                fail("feed floor benchmark boundary mismatch");return;
            }
        }
        nativeRegisterFeedEnabled=savedRegister;
    }
#endif
    unsigned registerMode=nativeRegisterFeedEnabled;
    board->video.wptnCountsBytes=false;
    for(unsigned workload=0;workload<2;++workload){
        for(unsigned n=0;n<512;++n)put16((uint8_t*)nativeRamBegin+n*2,
            workload?((n&15)==0?0x1800:(n&15)==1?14:uint16_t(n)):((n&1)?0x3333:0x0800));
        for(unsigned mode=0;mode<2;++mode){
            nativeRegisterFeedEnabled=mode;nativeFeedInlineCount=0;nativeFeedHeaderGrant=0;revokeRasterGrant();
            nativeShortPending=0;seenFrames=pendingFrames;
            start=NativeTiming::benchmarkClock();nativeRingBenchmark();
            nativeRegisterBenchTicks[workload][mode]=NativeTiming::benchmarkClock()-start;
        }
    }
#ifdef POKERI_CACHED_RASTER
    // Same real assembly feeder in both modes, exactly one cached card.
    // Context/clearing and recipe translation are outside the timed interval.
    if(nativeCardCache && nativeCardCache->ready){
        auto &v=board->video;
        bool controlsMode=nativeRasterControlsEnabled,absoluteMode=nativeRasterAbsoluteEnabled;
        nativeRasterBenchBytes=CardBackCache::Words*2;
        nativeRegisterFeedEnabled=nativeHeaderFeedEnabled=nativeInlineFeedEnabled=1;
#if defined(POKERI_TIME_LEDGER) && defined(POKERI_LEDGER_FAST_CACHE)
        // Isolated complete-card scopes, including final DMA completion.
        // Suppress inner endpoints only; keep assembly grants and no opcode logger.
        auto cardTiming=nativeCardCache->timing;nativeCardCache->timing=nullptr;
        NativeTiming::begin();
#endif
#if defined(POKERI_RASTER_CHUNKS)
        // Same command stream and final DMA drain, with deterministic feeder
        // returns. This omits IRQ handling and is not a live latency result.
        for(unsigned white=0;white<2;++white)for(unsigned mode=3;mode<4;++mode)
        for(unsigned chunk=0;chunk<3;++chunk)for(unsigned trial=0;trial<4;++trial){
#elif defined(POKERI_RASTER_SAMPLES)
        // Sample only completed backs, excluding context setup and clearing.
        for(unsigned white=0;white<1;++white)for(unsigned mode=3;mode<4;++mode)for(unsigned trial=0;trial<512;++trial){
#else
        for(unsigned white=0;white<2;++white)for(unsigned mode=0;mode<4;++mode)for(unsigned trial=0;trial<4;++trial){
#endif
            nativeRasterBenchBytes=2*(white?card_recipe::offsets[CardBackCache::WhiteCommands]:CardBackCache::Words);
            v.flushCard();v.Hd63484::write8(0,2);v.Hd63484::write8(2,0x82);
            const uint32_t *c=card_recipe::context;
            v.origin=c[0];v.frameMask=c[1];v.rwp=c[2];v.status=c[3];
            for(unsigned i=0;i<32;++i)v.parameter[i]=c[4+i];
            for(unsigned i=0;i<16;++i)v.pattern[i]=c[36+i];
            for(unsigned i=0;i<256;++i)v.control[i]=c[52+i];
            v.error=nullptr;v.Hd63484::write8(0,0);
            uint32_t first=(((v.origin>>4)+4-225*152)&v.frameMask)<<2;
            if(!videoSurface.fill(first,608,88,100,0,0)){fail("raster ring clear");return;}
            videoSurface.synchronize();
            for(unsigned n=0;n<CardBackCache::Commands;++n){
                unsigned begin=card_recipe::offsets[n],end=card_recipe::offsets[n+1];
                for(unsigned i=begin;i<end;++i){uint16_t value=card_recipe::words[i];
                    if(card_recipe::words[begin]==0x8000 && i>begin)value+=i==begin+1?16:126;
                    put16((uint8_t*)nativeRamBegin+i*2,value);}
            }
            nativeRasterEnabled=mode;nativeRasterControlsEnabled=mode>=2;nativeRasterAbsoluteEnabled=mode==3;nativeFeedInlineCount=0;nativeFeedHeaderGrant=0;revokeRasterGrant();
            nativeShortPending=0;seenFrames=pendingFrames;nativeCachedVideoStatus=v.statusNow();
            unsigned hits=white?nativeCardCache->whiteHits:nativeCardCache->hits;
#if defined(POKERI_TIME_LEDGER) && defined(POKERI_LEDGER_FAST_CACHE)
            NativeTiming::event(3,mode+4*white,trial,nativeCycles);
#endif
#ifdef POKERI_RASTER_SAMPLES
            nativeProfileEnabled=1;
#endif
            uint32_t began=NativeTiming::benchmarkClock();
#ifdef POKERI_RASTER_CHUNKS
            const uint32_t startRam=nativeRamBegin,total=nativeRasterBenchBytes;
            const unsigned step=chunk==0?total:chunk==1?20:2;
            for(unsigned offset=0;offset<total;offset+=step){
                nativeRamBegin=startRam+offset;
                nativeRasterBenchBytes=total-offset<step?total-offset:step;
                nativeRingBenchmark();
            }
            nativeRamBegin=startRam;nativeRasterBenchBytes=total;
#else
            nativeRingBenchmark();
#endif
            if(white)v.flushCard();
            videoSurface.synchronize();
#ifdef POKERI_RASTER_CHUNKS
            nativeChunkRasterTicks[white][chunk]+=NativeTiming::benchmarkClock()-began;
#else
            (white?nativeWhiteRasterTicks:nativeRasterBenchTicks)[mode]+=NativeTiming::benchmarkClock()-began;
#endif
#ifdef POKERI_RASTER_SAMPLES
            nativeProfileEnabled=0;
#endif
#if defined(POKERI_TIME_LEDGER) && defined(POKERI_LEDGER_FAST_CACHE)
            NativeTiming::event(6,mode+4*white,trial,nativeCycles);
#endif
            if(v.error || (white?nativeCardCache->whiteHits:nativeCardCache->hits)!=hits+1){fail("raster ring admission");return;}
        }
#if defined(POKERI_TIME_LEDGER) && defined(POKERI_LEDGER_FAST_CACHE)
        NativeTiming::end();nativeCardCache->timing=cardTiming;
#endif
        nativeRasterEnabled=true;nativeRasterControlsEnabled=controlsMode;nativeRasterAbsoluteEnabled=absoluteMode;nativeRasterBenchBytes=1024;
    }
#endif
    nativeRegisterFeedEnabled=registerMode;board->video.wptnCountsBytes=byteCounts;
    nativeHeaderFeedEnabled=headerMode;nativeInlineFeedEnabled=inlineMode;
    nativeFeedHeaderGrant=0;revokeRasterGrant();
    nativeFeedTarget=oldTarget;nativeShortStatus[1]=oldWrite;
    nativeRomBegin=oldBegin;nativeRomEnd=oldEnd;nativeShortStatus[0]=oldDescriptor;
    // Controlled synthetic drawing batches, separate from exception overhead.
    // Include queued completion and use no ROM artwork or game state.
    Hd63484 &video=*videoDevice;
    video.control[2]=2;video.control[3]=0;video.control[0xc2]=0;video.control[0xc3]=64;
    video.parameter[0]=0x3333;video.parameter[1]=0xcccc;video.parameter[3]=0xeeee;
    video.parameter[5]=video.parameter[6]=video.parameter[7]=0;video.pattern[0]=0;
    auto command=[&](std::initializer_list<uint16_t> words){
        video.Hd63484::write8(0,0);
        for(uint16_t value:words){video.Hd63484::write8(2,value>>8);video.Hd63484::write8(2,value);}
    };
    command({0x0400,2,0});
    for(unsigned stage=0;stage<3;++stage){
        start=NativeTiming::benchmarkClock();
        for(unsigned n=0;n<(stage==0?512:stage==1?16:8);++n){
            command({0x8000,0,0});
            if(stage==0)command({0xcc00});
            else if(stage==1)command({0xa900,24});
            else {
                videoSurface.fill(0x8000-12*256-25,256,50,26,0xeeee,0);
                videoSurface.fill(0x8000-11*256-24,256,48,24,0x5555,0);
                command({0xc800});
            }
        }
        videoSurface.synchronize();nativeDrawingBenchTicks[stage]=NativeTiming::benchmarkClock()-start;
    }
    // Synthetic card workload: the measured command SHAPES/counts, never ROM
    // parameters or artwork. Two independent clears expose cold/warm outlines.
    // 79 commands / 260 FIFO words (ORG and clearing are outside the timer).
    for(unsigned pass=0;pass<2;++pass){
        videoSurface.fill(0x8000-96*256-32,256,192,128,0x5555,0);
        videoSurface.synchronize();
        start=NativeTiming::benchmarkClock();
        for(unsigned r:{0u,1u,3u,4u,5u,6u,7u,0u,1u,3u})
            command({uint16_t(0x0800+r),uint16_t(r==5 || r==6 || r==7?0:0x3333)});
        for(unsigned n=0;n<8;++n){
            command({0x8000,uint16_t((n&3)*24),uint16_t(n<4?0:24)});
            if(n<4)command({0xa900,7});else command({0xad00,4,9,6});
            command({0xc800});
        }
        command({0x8000,0,48});
        command({0x9c00,13, 4,0,4,0,4,4,4,4,0,4,0,4,0xfffc,4,
                 0xfffc,4,0xfffc,0,0xfffc,0xfffc,0,0xfffc,0,0xfff8,0,0xfffc});
        command({0x8000,32,48});
        command({0x9c00,8, 4,0,4,4,0,4,0xfffc,4,0xfffc,0,0xfffc,0xfffc,0,0xfffc,4,0xfffc});
        command({0x8000,64,48});
        command({0x9c00,6, 8,0,4,4,0xfffc,4,0xfff8,0,0xfffc,0xfffc,4,0xfffc});
        command({0x8400,4,4});command({0xc800});
        command({0x8400,0xffc0,24});
        for(unsigned n=0;n<17;++n){command({0x8400,5,0});command({0xc400,3,1});}
        command({0x8400,0,1});command({0x8400,0,0xffff});
        videoSurface.synchronize();nativeCardBenchTicks[pass]=NativeTiming::benchmarkClock()-start;
    }
    if(video.error)fail(video.error);
    // The assembly entry also gates this entire function on native-benchmark.
    if(nativeBenchmarkRequested && displayRequested && !screen.compositionTest(video,nativeScreenBenchTicks))fail("incremental composition differs from full redraw");
#ifdef POKERI_CARD_CACHE
    // Isolated complete jobs with hires raster DMA enabled by compositionTest.
    // OS clock reads bracket whole blits, never individual register accesses.
    if(nativeBenchmarkRequested && nativeCardStorage && nativeCardCache && nativeCardCache->ready){
        for(unsigned align=0;align<16;++align){
            videoSurface.synchronize();uint32_t start=NativeTiming::benchmarkClock();
            if(!videoSurface.cardBlit(608*2+16+align,nativeCardStorage,nativeCardStorage+CardBackCache::BitmapWords))fail("card DMA benchmark bounds");
            videoSurface.synchronize();nativeCardDmaTicks[align]=NativeTiming::benchmarkClock()-start;
        }
        // Three paired common-white prefixes, with the same prepared cache,
        // feed and DMA completion. Only prefix raster reuse differs. This
        // explicit benchmark uses the locally generated recipe, never a ROM
        // routine replacement; normal play performs none of this setup.
        bool savedWhite=nativeCardCache->whiteEnabled;
        for(unsigned mode=0;mode<2;++mode)for(unsigned trial=0;trial<3;++trial){
            video.flushCard();video.Hd63484::write8(0,2);video.Hd63484::write8(2,0x82);
            const uint32_t *c=card_recipe::context;
            video.origin=c[0];video.frameMask=c[1];video.rwp=c[2];video.status=c[3];
            for(unsigned i=0;i<32;++i)video.parameter[i]=c[4+i];
            for(unsigned i=0;i<16;++i)video.pattern[i]=c[36+i];
            for(unsigned i=0;i<256;++i)video.control[i]=c[52+i];
            video.error=nullptr;video.Hd63484::write8(0,0);
            uint32_t first=(((video.origin>>4)+4-225*152)&video.frameMask)<<2;
            if(!videoSurface.fill(first,608,88,100,0,0)){fail("white benchmark clear");return;}
            videoSurface.synchronize();nativeCardCache->whiteEnabled=mode;
            unsigned before=nativeCardCache->whiteHits;
            uint32_t start=NativeTiming::benchmarkClock();
            for(unsigned n=0;n<CardBackCache::WhiteCommands;++n){
                unsigned begin=card_recipe::offsets[n],end=card_recipe::offsets[n+1];
                for(unsigned i=begin;i<end;++i){uint16_t value=card_recipe::words[i];
                    if(card_recipe::words[begin]==0x8000 && i>begin)value+=i==begin+1?16:126;
                    video.writeFifoWord(value);
                }
            }
            video.flushCard();videoSurface.synchronize();
            nativeWhiteBenchTicks[mode]+=NativeTiming::benchmarkClock()-start;
            if(video.error || nativeCardCache->whiteHits-before!=mode){fail("white benchmark admission");return;}
        }
        nativeCardCache->whiteEnabled=savedWhite;
    }
#endif
#ifdef POKERI_READ_ONLY_DMA
    // Synthetic full display copy followed by 68 read-only guard probes.
    // Compare forced serialization against the new dependency rule on the
    // same hardware/build. Timers bracket batches, never individual probes.
    const unsigned displayWords=152*255;
    uint16_t *readDisplay=(uint16_t*)AllocMem(displayWords*2,MEMF_CHIP);
    if(!readDisplay){fail("read DMA benchmark allocation");return;}
    for(unsigned mode=0;mode<2;++mode)for(unsigned trial=0;trial<16;++trial){
        videoSurface.synchronize();
        uint32_t totalStart=NativeTiming::benchmarkClock();
        if(!videoSurface.displayBlit(readDisplay,readDisplay,readDisplay+displayWords,152,38,0,0,0,608,608,255,true)){
            videoSurface.synchronize();FreeMem(readDisplay,displayWords*2);fail("read DMA benchmark bounds");return;
        }
        uint32_t readStart=NativeTiming::benchmarkClock();
        if(!mode)videoSurface.synchronize();
        for(unsigned probe=0;probe<68;++probe)nativeBenchSink=videoSurface.pixel4(probe*152,0);
        nativeReadDmaTicks[mode]+=NativeTiming::benchmarkClock()-readStart;
        videoSurface.synchronize();
        nativeReadDmaTotal[mode]+=NativeTiming::benchmarkClock()-totalStart;
    }
    FreeMem(readDisplay,displayWords*2);
#endif
    PatternTile tile={};tile.width=15;tile.height=14;tile.offset=7;
    tile.colors[0]=0x1111;tile.colors[1]=0xffff;
    tile.point=tile.start=0x2000;tile.end=0xf0f0;tile.mode=1;
    for(unsigned i=0;i<16;++i)tile.rows[i]=uint16_t(0x1234u+i*71);
    uint16_t expanded[160];
    start=NativeTiming::benchmarkClock();
    for(unsigned n=0;n<512;++n){tile.rows[2]^=uint16_t(n+0x2345);tile.expand(expanded);nativeBenchSink=expanded[26]+expanded[58];}
    nativePatternBenchTicks[0]=NativeTiming::benchmarkClock()-start;
    start=NativeTiming::benchmarkClock();
    for(unsigned n=0;n<512;++n){tile.rows[2]=uint16_t(n+0x3456);if(!videoSurface.patternTile(0x8007,608,tile,0)){fail("pattern benchmark bounds");return;}}
    videoSurface.synchronize();nativePatternBenchTicks[1]=NativeTiming::benchmarkClock()-start;
    // Synthetic pixels, matching the two observed scrolling rectangle sizes.
    // Cover all source alignments and physical row seams; drain each step so
    // this reports ready-to-display cost rather than queue submission alone.
    for(unsigned kind=0;kind<2;++kind){
        unsigned width=kind?150:211,dest=608*80+(kind?342:172);
        start=NativeTiming::benchmarkClock();
        for(unsigned n=0;n<256;++n){
            if(!videoSurface.copy(608*700+280+n,dest,608,width,20,0)){fail("scroll benchmark bounds");return;}
            videoSurface.synchronize();
        }
        nativeScrollBenchTicks[kind]=NativeTiming::benchmarkClock()-start;
    }

}
CopperList *nativeCopper(){return displayRequested?screen.copper():nullptr;}
void nativeAudioStart(){if(liveRequested){if(!amigaInputStart()){fail("keyboard resource unavailable");return;}paula.start();}}
void nativeAudioStop(){if(liveRequested){paula.stop();amigaInputStop();}}
#ifdef POKERI_VBI_LATENCY
// Rows: startup/play, each without/with BLITHOG. Count, max line, line>=29.
uint32_t nativeVbiLatency[4][3]={};
#endif
void nativeVbi(bool quit){paula.vbi();screen.vbi();if(screen.swaps)NativeTiming::mark(NativeTiming::FirstSwap,nativeCycles,nativeLastPc);
#ifdef POKERI_VBI_LATENCY
    // Observe after audio and screen work; never postpone their service.
    unsigned line=(*(volatile uint32_t*)0xdff004>>8)&511;
    unsigned priority=(AmigaHardware::enabledDMAChannels()&0x400)?1:0;
    uint32_t *sample=nativeVbiLatency[(nativeSetupReady?2:0)+priority];
    ++sample[0];if(line>sample[1])sample[1]=line;if(line>=29)++sample[2];
#endif
    // Benchmarks run synthetic supervisor code. Freeze both guest frame
    // counters: updating both here can race their separate unmasked reads
    // in shortIoCompleted and falsely promote a synthetic PC into the game.
    // Display/audio IRQ work above still runs; normal gameplay counts VBI.
    if(!nativeBenchmarkRequested)++pendingFrames;
#ifdef POKERI_TIME_LEDGER
    NativeTiming::frameRecord();paula.recordApplied();
#endif
    if(paula.error){quitRequested=true;nativeFastBoundary=0;}
    if(quit || amigaInputQuit()){quitRequested=true;nativeFastBoundary=0;}}
extern "C" bool nativePrepareInner(){
#ifdef POKERI_STARTUP_PROFILE
    startupTimestamp(0);
#endif
    // Retain zero-valued symbols for existing read-only debugger scripts even
    // when the linker can discard their per-access updates in a normal build.
    nativeShortCalls=nativeFeedTests=nativeFeedBranches=nativeFeedWrites=0;
    nativeFeedLoopWords=nativeFeedLoopTurns=nativeFeedLoopSaved=0;
    nativeFeedInlineWords=nativeFeedHeaderWords=0;
    nativeExtendedFrame=(SysBase->AttnFlags & AFF_68010)?1:0;
    nativeFrameBytes=nativeExtendedFrame?8:6;
    if(nativeExtendedFrame)privateVectors=(uint32_t*)AllocMem(1024,MEMF_FAST); // optional optimization
    nativeStatus=0;DOSBase=(DosLibrary*)OpenLibrary("dos.library",0);if(!DOSBase)return fail("DOS unavailable");
    BPTR legacy=Open("native-clock-legacy",MODE_OLDFILE);if(legacy){Close(legacy);nativeClockMode=0;}
    BPTR corrected=Open("native-clock-corrected",MODE_OLDFILE);if(corrected){Close(corrected);nativeClockMode=1;}
    BPTR ratio=Open("native-clock-ratio",MODE_OLDFILE);
    if(ratio){uint8_t value[2];LONG n=Read(ratio,value,2);Close(ratio);
        if(n!=1 || value[0]<1 || value[0]>37)return fail("clock ratio must be one byte, 1..37 sixteenths");
        liveClock.ratioSixteenths=value[0];}
    BPTR window=Open("native-clock-window",MODE_OLDFILE);
    if(window){uint8_t value[2];LONG n=Read(window,value,2);Close(window);
        if(n!=1 || value[0]<1 || value[0]>3 || nativeClockMode!=2)return fail("clock window requires mode C and one byte, 1..3 PAL frames");
        playClockWindow=value[0];}
    BPTR shuffle=Open("native-no-shuffle-vblank",MODE_OLDFILE);shuffleEnabled=!shuffle && nativeClockMode==2;if(shuffle)Close(shuffle);
    BPTR idle=Open("native-idle-hook",MODE_OLDFILE);idleHook=idle && nativeClockMode==2;if(idle)Close(idle);
    BPTR selector=Open("native-no-address-selector",MODE_OLDFILE);addressSelectorEnabled=selector==0;if(selector)Close(selector);
    BPTR userTrap=Open("native-no-user-trap",MODE_OLDFILE);nativeUserTrapEnabled=userTrap==0;if(userTrap)Close(userTrap);
    BPTR stackSwitch=Open("native-no-stack-switch",MODE_OLDFILE);nativeStackSwitchEnabled=stackSwitch==0;if(stackSwitch)Close(stackSwitch);
    BPTR registerFeed=Open("native-no-register-feed",MODE_OLDFILE);nativeRegisterFeedEnabled=registerFeed==0;if(registerFeed)Close(registerFeed);
    BPTR headerFeed=Open("native-no-header-feed",MODE_OLDFILE);nativeHeaderFeedEnabled=headerFeed==0;if(headerFeed)Close(headerFeed);
    BPTR inlineFeed=Open("native-no-inline-feed",MODE_OLDFILE);nativeInlineFeedEnabled=inlineFeed==0;if(inlineFeed)Close(inlineFeed);
    BPTR loop=Open("native-no-feed-loop",MODE_OLDFILE);feedLoop=loop==0;if(loop)Close(loop);
    BPTR feed=Open("native-no-feed-fusion",MODE_OLDFILE);feedFusion=feed==0;if(feed)Close(feed);
    BPTR generic=Open("native-generic-hooks",MODE_OLDFILE);genericHooks=generic!=0;if(generic)Close(generic);
    BPTR benchmark=Open("native-benchmark",MODE_OLDFILE);nativeBenchmarkRequested=benchmark!=0;if(benchmark)Close(benchmark);
    BPTR measure=Open("native-measure",MODE_OLDFILE);if(measure)Close(measure);
    if((measure || nativeBenchmarkRequested) && !NativeTiming::prepare())return fail("measurement timer unavailable");
    BPTR resetTest=Open("native-stop-on-watchdog",MODE_OLDFILE);stopOnLiveReset=resetTest!=0;if(resetTest)Close(resetTest);
    BPTR test=Open("native-test-inputs",MODE_OLDFILE);testInputs=test!=0;if(test)Close(test);
    test=Open("native-test-wrap",MODE_OLDFILE);testWrap=test!=0;if(test)Close(test);
    BPTR slow=Open("native-no-short-hooks",MODE_OLDFILE);if(slow){Close(slow);nativeShortEnabled=0;}
    if(genericHooks)nativeShortEnabled=0;
    BPTR replay=Open("native-replay",MODE_OLDFILE);diagnostic=replay!=0;nativeDiagnostic=diagnostic;if(replay)Close(replay);
    BPTR playRatio=Open("native-clock-play-ratio",MODE_OLDFILE);
    if(playRatio){uint8_t value[2];LONG n=Read(playRatio,value,2);Close(playRatio);
        if(n!=1 || value[0]>64)return fail("play clock ratio must be one byte, 0..64 sixteenths (0 retains boot ratio)");
        playClockRatio=value[0];}
    BPTR tests=Open("native-hardware-tests",MODE_OLDFILE);
    nativeSkipHardwareTests=!diagnostic && !tests;if(tests)Close(tests);
    BPTR live=Open("native-live",MODE_OLDFILE);liveRequested=!diagnostic || live!=0;
    BPTR display=Open("native-display",MODE_OLDFILE);displayRequested=liveRequested || display!=0;if(display)Close(display);
    if(live){uint8_t limit[5];LONG n=Read(live,limit,5);Close(live);if(n!=0 && n!=4)return fail("native-live must be empty or a four-byte cycle budget");if(n==4)liveStopCycles=get32(limit);}
    boardAllocation=(uint8_t*)pokeriAllocateUninitialized(sizeof(Board)+255);guard=(uint8_t*)pokeriAllocateUninitialized(0x80000);
    if(!boardAllocation || !guard)return fail("native allocations failed");
    board=new((void*)((uint32_t(boardAllocation)+255)&~255u)) Board();
    videoDevice=&board->video;nativeVideoSelector=videoDevice->addressSelector();
    rom=board->memory.data();romBase=uint32_t(rom);ramBase=uint32_t(rom+0x40000);guardBase=uint32_t(guard);
    nativeRomBegin=romBase;nativeRomEnd=romBase+0x40000;nativeRamBegin=ramBase;nativeRamEnd=ramBase+0x40000;
    static const char *names[]={"rom/77POK30","rom/77POK38","rom/77POK34","rom/PARA200J"};
    for(unsigned chip=0;chip<4;++chip)if(!fileRead(names[chip],rom+(chip<<16),65536))return false;
    for(const auto &patch:patchWords)if(get16(rom+patch.offset)!=patch.value)return fail("ROM patch-site mismatch");
    // Audited low-vector sentinel reads need the unrelocated vectors only.
    for(unsigned i=0;i<sizeof(originalVectors);++i)originalVectors[i]=rom[i];
    for(unsigned i=0;i<0x80000;++i)guard[i]=0xa5;
    BPTR guardTest=Open("native-test-guard",MODE_OLDFILE);
    if(guardTest){Close(guardTest);if(!testGuard())return fail("guard self-test failed");}
    for(const auto &f:fixups){uint32_t v=get32(rom+f.offset);v+=f.kind==0?romBase:f.kind==3?guardBase-0x80000:ramBase-0x40000;put32(rom+f.offset,v);}
    for(unsigned i=0;i<sizeof(hooks)/sizeof(*hooks);++i)
        if(!prepareHook(hooks[i],rom+hooks[i].pc,preparedHooks[i]))return fail("invalid prepared hook");
    for(unsigned i=0;i<sizeof(accesses)/sizeof(*accesses);++i)preparedAccesses[i].physical=relocated(accesses[i].address);
    nativeShortDrainPc=romBase+0x11040;
    for(unsigned i=0;i<sizeof(hooks)/sizeof(*hooks);++i){
        const auto &h=hooks[i];const auto &meta=hookMetadata[i];
        unsigned sentinel=0,vectorOffset=0;
        switch(h.pc){
        case 0x616a:case 0x6186:sentinel=0x8000;vectorOffset=4;break;
        case 0x6170:sentinel=0x8001;break;
        case 0x61ca:sentinel=0x8003;break;
        case 0x61e2:sentinel=0x8002;vectorOffset=8;break;
        }
        if(sentinel){
            nativeShortStatus[i]=shortDescriptor(romBase+h.pc,get32(originalVectors+vectorOffset),uint16_t(sentinel),meta.cycles);
            continue;
        }
        unsigned piaKind=h.pc==0x13e2?0:h.pc==0x13e6?1:h.pc==0x2472?2:h.pc==0xb3e2?3:4;
        if(piaKind<4){
            if(h.operation!=Operation::move || h.size!=1 || meta.last!=meta.first+1)return fail("short PIA shape mismatch");
            const auto &e=accesses[meta.first];
            if(e.address!=(piaKind==3?0xfb01c:0xfb01e) || e.write!=(piaKind!=3) || e.size!=1)return fail("short PIA endpoint mismatch");
            if(piaKind==2 && (preparedHooks[i].sourceExtension&0xff00)!=0x5000)return fail("short PIA index must be D5.W");
            nativeShortStatus[i]=shortDescriptor(romBase+h.pc,preparedAccesses[meta.first].physical,uint16_t(0x2000|piaKind),meta.cycles);
            continue;
        }
        if(h.operation==Operation::move && h.size==1 && meta.last==meta.first+1){
            const auto &e=accesses[meta.first];
            bool peripheral=(e.address>=0xfb014 && e.address<0xfb020) ||
                e.address==0xfb002 || e.address==0xfb003 || e.address==0xfb006 || e.address==0xfb007 || e.address==0xfb00a || e.address==0xfb00b;
            const Operand &port=e.write?h.dest:h.source,&value=e.write?h.source:h.dest;
            bool indirect=port.kind==Ea::indirect,immediate=e.write && value.kind==Ea::immediate;
            if(peripheral && e.size==1 && port.reg==3 && (indirect || port.kind==Ea::displacement) &&
                ((value.kind==Ea::data && value.reg>=0 && value.reg<=2) || (immediate && !indirect))){
                unsigned kind=(e.write?8:0)|(immediate?0x20:unsigned(value.reg))|(indirect?0x40:0);
                if(h.length!=(immediate?6:indirect?2:4))return fail("short peripheral length mismatch");
                nativeShortStatus[i]=shortDescriptor(romBase+h.pc,preparedAccesses[meta.first].physical,uint16_t(0x1000|kind),meta.cycles);continue;
            }
        }
        if(h.operation==Operation::move && (h.size==1 || h.size==2) && meta.last==meta.first+1){
            const auto &e=accesses[meta.first];
            bool displacement=h.dest.kind==Ea::displacement;
            bool post=h.source.kind==Ea::postincrement && h.source.reg==1 && h.size==2 && displacement;
            bool immediate=h.source.kind==Ea::immediate;
            if(e.write && e.size==h.size && e.address>=0xf6000 && e.address+h.size<=0xf6004 &&
               h.dest.reg==0 && (displacement || h.dest.kind==Ea::indirect) && (post || immediate)){
                unsigned kind=(displacement?1:0)|(h.size==2?2:0)|(post?4:0);
                if(h.length!=(displacement && !post?6:4))return fail("short video length mismatch");
                nativeShortStatus[i]=shortDescriptor(romBase+h.pc,preparedAccesses[meta.first].physical,uint16_t(0x0800|kind),meta.cycles);
                if(addressSelectorEnabled && kind==0 && e.address==0xf6000)
                    nativeShortStatus[i].body=uint32_t(nativeShortAddressWrite);
                continue;
            }
        }
        if(h.operation!=Operation::bit_test || h.size!=1 || h.length!=4 ||
           h.source.kind!=Ea::immediate || h.dest.kind!=Ea::indirect || h.dest.reg!=0 ||
           meta.last!=meta.first+1)continue;
        const auto &e=accesses[meta.first];
        if(e.address!=0xf6000 || e.size!=1 || e.write)continue;
        nativeShortStatus[i]=shortDescriptor(romBase+h.pc,preparedAccesses[meta.first].physical,
            uint16_t(1u<<(preparedHooks[i].sourceExtension&7)),meta.cycles);
    }
#ifdef POKERI_FIFO_CONTROL_FUSION
    if(!diagnostic){
        for(unsigned start:{0x2e70u,0x2eb2u}){
            const unsigned offsets[]={0,4,10},lengths[]={4,6,4},cycles[]={12,16,12};
            const unsigned values[]={3,start==0x2e70?0x80u:0x81u,0};
            ShortStatus *sequence[3]={};
            for(unsigned n=0;n<3;++n){
                unsigned pc=start+offsets[n];
                for(auto &d:nativeShortStatus)if(d.pc==romBase+pc)sequence[n]=&d;
                auto *d=sequence[n];
                // Exact generic MOVE.B encodings, immediate words and displacement.
                // ROM bytes stay local; the three original accesses remain the reference.
                if(!d || get16(rom+pc)!=(n==1?0x117c:0x10bc) ||
                   get16(rom+pc+2)!=values[n] || (n==1 && get16(rom+pc+4)!=2) ||
                   d->mask!=(n==1?0x0801:0x0800) || d->length!=lengths[n] || d->cycles!=cycles[n] ||
                   d->address!=guardBase+(n==1?0x76002:0x76000))
                    return fail("FIFO control fusion shape mismatch");
            }
            sequence[0]->reserved=uint32_t(sequence[1]);
            sequence[1]->reserved=uint32_t(sequence[2]);
            sequence[0]->body=uint32_t(nativeShortFifoControl);
        }
    }
#endif
    if(feedFusion){
        ShortStatus *status=nullptr,*write=nullptr;
        for(auto &d:nativeShortStatus){if(d.pc==romBase+0x2e58)status=&d;if(d.pc==romBase+0x2e5e)write=&d;}
        unsigned branch=get16(rom+0x2e5c);
        if(!status || !write || status->mask!=2 || write->mask!=0x0807 ||
           status->length!=4 || write->length!=4 || write->address!=status->address+2 ||
           (branch&0xff00)!=0x6700 || romBase+0x2e5e + int8_t(branch)!=romBase+0x2e7e)
            return fail("feed fusion shape mismatch");
        status->reserved=uint32_t(write);status->body=uint32_t(nativeShortFeedRead);
        nativeFeedTarget=romBase+0x2e7e;
        if(feedLoop){write->body=uint32_t(nativeShortFeedLoopWrite);write->reserved=uint32_t(status);}
    }
    put16(rom+0x10ae,0x6000);put16(rom+0x10b0,0x30);put16(rom+0x110c,0x6000);put16(rom+0x110e,0x2c);
    for(unsigned i=0;i<sizeof(hooks)/sizeof(*hooks);++i)put16(rom+hooks[i].pc,0xa000|i);
    for(auto pc:resets)put16(rom+pc,0xaffd);
#ifdef POKERI_STARTUP_FAST_FORWARD
    // Explicit clock experiments retain their historical startup contract.
    startupFast=!diagnostic && nativeSkipHardwareTests && nativeClockMode==2 &&
        !nativeBenchmarkRequested && !ratio && !playRatio && !window;
    BPTR startupWall=Open("native-startup-wall",MODE_OLDFILE);
    if(startupWall){Close(startupWall);startupFast=false;}
    startupDelayOpcode=get16(rom+0x2442);
    paula.muted=startupFast;
    if(idleHook || startupFast)put16(rom+0x2442,0xaffc);
#else
    if(idleHook)put16(rom+0x2442,0xaffc);
#endif
    if(shuffleEnabled && !diagnostic)put16(rom+ShuffleWait::pc,0xaffb);
    for(unsigned i=0;i<sizeof(controls)/sizeof(*controls);++i){
        unsigned pc=controls[i],index=sizeof(hooks)/sizeof(*hooks)+i;
        uint16_t op=originalControl[i]=get16(rom+pc);controlCycles[i]=hookCycles(pc);
        unsigned kind=op==0x007c?0:op==0x027c?1:op==0x4e73?2:3;
        if(kind<3)nativeShortStatus[index]=shortDescriptor(romBase+pc,kind<2?get16(rom+pc+2):0,uint16_t(0x4000|kind),controlCycles[i]);
        put16(rom+pc,0xa000|index);
    }
    for(unsigned i=0;i<16;++i){
        uint32_t target=get32(rom+(32+i)*4);
        nativeShortTraps[i]={0,target,0,34,0,0,uint32_t(nativeShortTrapRead),0,3,0};
    }
    put16(rom+0x2194,0xaffe);
    const uint32_t supported[]={8000000,100,50,400,50000,1000000,0x3ffff,0,1,1};
    uint32_t settings[10];for(unsigned i=0;i<10;++i)settings[i]=supported[i];
    if(diagnostic){
    BPTR f=Open("replay.bin",MODE_OLDFILE);if(!f)return fail("replay.bin missing");Seek(f,0,OFFSET_END);LONG size=Seek(f,0,OFFSET_BEGINNING);if(size<9 || size>6000000){Close(f);return fail("replay size outside budget");}replaySize=size;replayData=(uint8_t*)pokeriAllocateUninitialized(replaySize);if(!replayData){Close(f);return fail("replay allocation failed");}LONG got=Read(f,replayData,replaySize);Close(f);if(got!=size)return fail("replay read failed");reader=new ReplayReader(replayData,replaySize);if(!reader)return fail("replay reader allocation failed");
    for(unsigned i=0;i<10;++i){if(!reader->next(nextEvent) || nextEvent.kind!=ReplayConfig || nextEvent.pc!=i)return fail("replay config invalid");settings[i]=nextEvent.a;}
    for(unsigned i=0;i<10;++i)if(i==9?(settings[i]!=1 && settings[i]!=3 && settings[i]!=7):settings[i]!=supported[i])return fail("unsupported native replay configuration");
    nativeSkipHardwareTests=(settings[9]&2)!=0;
    }
    if(nativeSkipHardwareTests)applyBootPolicy(rom,!diagnostic || (settings[9]&4));
    CacheClearU(); // Publish relocated/patched instructions to 68020+ caches.
    board->config.cpuHz=settings[0];board->config.systemHz=settings[1];board->config.inputHz=settings[2];board->config.watchdogMs=settings[3];board->config.watchdogResetUs=settings[4];board->ay.clockHz=settings[5];board->peer.enabled=settings[8];
if(liveRequested){if(!paula.prepare())return fail("Paula allocation failed");board->ay.backend=&paula;}
    if(!videoSurface.prepare())return fail("video bitplane allocation failed");
    board->video.surface=&videoSurface;
#ifdef POKERI_CARD_CACHE
    BPTR noCard=Open("native-no-card-cache",MODE_OLDFILE);
    bool disableCard=noCard!=0;if(noCard)Close(noCard);
    bool prepareCard=!disableCard;
#ifdef POKERI_TIME_LEDGER
    prepareCard=true; // paired diagnostic records the same recipe in both modes
#endif
    if(prepareCard){
        uint32_t started=measure?NativeTiming::benchmarkClock():0;
        nativeCardStorage=(uint16_t*)AllocMem(CardBackCache::BitmapWords*4,MEMF_CHIP);
        nativeCardCache=new CardBackCache;
        if(nativeCardCache && nativeCardStorage){
            if(nativeCardCache->prepare({card_recipe::words,card_recipe::offsets,card_recipe::context},
                nativeCardStorage,nativeCardStorage+CardBackCache::BitmapWords)){nativeCardCache->attach(board->video,true);
#ifdef POKERI_TIME_LEDGER
                nativeCardCache->timing=[](unsigned kind,unsigned detail){
#ifdef POKERI_LEDGER_FAST_CACHE
                    NativeTiming::event(3+kind,detail,videoSurface.cardBlits,nativeCycles);
                    if(kind==1)NativeTiming::event(6,detail,videoSurface.cardBlits,nativeCycles);
#else
                    if(kind!=0 || !nativeCardObserver.matched)NativeTiming::event(3+kind,detail,videoSurface.cardBlits,nativeCycles);
#endif
                };
#endif
                nativeCardCache->enabled=!disableCard;
            }else if(nativeCardCache->error)return fail(nativeCardCache->error);
        }
        if(measure)nativeCardPrepareTicks=NativeTiming::benchmarkClock()-started;
    }
#endif
#if (defined(POKERI_TIME_LEDGER) || defined(POKERI_CARD_OBSERVER)) && !defined(POKERI_LEDGER_FAST_CACHE)
    // Completion attributes the enclosing Command scope to the opcode group.
    board->video.commandLog=[](const uint16_t *words,unsigned count,bool executed){
#ifdef POKERI_TIME_LEDGER
        NativeTiming::commandGroup=words[0]>>10;
        for(unsigned i=0;i<8;++i)NativeTiming::commandWords[i]=i<count?words[i]:0;
#endif
#if defined(POKERI_CARD_OBSERVER) || (defined(POKERI_TIME_LEDGER) && defined(POKERI_CARD_CACHE))
#ifdef POKERI_TIME_LEDGER
        unsigned complete=nativeCardObserver.complete;
#endif
        nativeCardObserver.command(words,count,executed);
#ifdef POKERI_TIME_LEDGER
        if(nativeCardObserver.complete!=complete)NativeTiming::event(6,nativeCardObserver.complete,videoSurface.cardBlits,nativeCycles);
#endif
#endif
    };
#endif
    #ifdef POKERI_CARD_OBSERVER
    videoSurface.pixelObserver=[](){nativeCardObserver.observe();};
#endif
    if(displayRequested && !screen.prepare(videoSurface,board->memory.data()))return fail("screen allocation failed");
    if(diagnostic && !advanceEvent())return false;
    if(!diagnostic){
        const char *error=loadAccounting(board->memory.data(),startup.retained);if(error)return fail(error);
        coldSetup=true;board->pia[1].input[0]=0xff;board->pia[1].input[1]=0x7f;board->pia[2].input[0]=8;}
    if(liveRequested){const char *error=loadNvram(board->nvram);if(error)return fail(error);}
    if(liveRequested && !nativeGuestTimerPrepare())return fail("CIA-A timer A unavailable for guest clock");
    if(diagnostic)nativeClockEnabled=0;
    nativeCachedVideoStatus=board->video.statusNow();
    resetCpu();if(diagnostic?!replayBoundary():!liveInputs())return false;nativePhysicalResume=diagnostic?0x8000:0;nativeStatus=1;return true;
}
extern "C" void nativeInstallVectors(){
    void(*traps[])()={nativeTrap0,nativeTrap1,nativeTrap2,nativeTrap3,nativeTrap4,nativeTrap5,nativeTrap6,nativeTrap7,nativeTrap8,nativeTrap9,nativeTrap10,nativeTrap11,nativeTrap12,nativeTrap13,nativeTrap14,nativeTrap15};
    originalVbr=nativeReadVbr();
    volatile uint32_t *vectors=(volatile uint32_t*)originalVbr;
    if(privateVectors){
        for(unsigned i=0;i<256;++i)privateVectors[i]=vectors[i];
        vectors=privateVectors;nativeWriteVbr(uint32_t(vectors));
    }
    nativeVectors=vectors;
    nativeOldLevel3=vectors[27];vectors[27]=uint32_t(nativeLevel3);
    nativeOldLevel6=vectors[30];vectors[30]=uint32_t(nativeLevel6);
    nativeOldLevel4=vectors[28];vectors[28]=uint32_t(nativeLevel4);
    nativeOldLevel2=vectors[26];vectors[26]=uint32_t(nativeLevel2);
    for(unsigned i=2;i<12;++i){savedVectors[i]=vectors[i];vectors[i]=uint32_t(i==9?nativeTrace:i==10?nativeLineA:nativeFault);}
    for(unsigned i=32;i<48;++i){savedVectors[i]=vectors[i];vectors[i]=uint32_t(traps[i-32]);}installed=true;
}
extern "C" void nativeRestoreVectors(){
    if(board){board->video.flushCard();videoSurface.synchronize();}
    volatile uint32_t *vectors=nativeVectors;
    for(unsigned i=2;i<12;++i)vectors[i]=savedVectors[i];
    for(unsigned i=32;i<48;++i)vectors[i]=savedVectors[i];
    installed=false;
    vectors[28]=nativeOldLevel4;
    vectors[27]=nativeOldLevel3;vectors[30]=nativeOldLevel6;vectors[26]=nativeOldLevel2;
    nativeVectorsRestored=vectors[28]==nativeOldLevel4 && vectors[27]==nativeOldLevel3 && vectors[30]==nativeOldLevel6 && vectors[26]==nativeOldLevel2;
    if(privateVectors){nativeWriteVbr(originalVbr);if(nativeReadVbr()!=originalVbr)nativeVectorsRestored=0;}
    for(unsigned i=2;i<12;++i)if(vectors[i]!=savedVectors[i])nativeVectorsRestored=0;
    for(unsigned i=32;i<48;++i)if(vectors[i]!=savedVectors[i])nativeVectorsRestored=0;
}
extern "C" __attribute__((noinline)) void nativeReturned(){asm volatile("" ::: "memory");}
void nativeRun(){
    if(nativeStatus!=1)return;
#ifdef POKERI_STARTUP_PROFILE
    startupTimestamp(1);
#endif
    seenFrames=pendingFrames;quitRequested=false;liveClock.reset(pendingFrames);
    if(!nativeBenchmarkRequested)NativeTiming::begin();
    NativeTiming::mark(NativeTiming::GuestStart,nativeCycles,nativeLastPc);
    Forbid();
    Supervisor((ULONG(*)())nativeEntry);
    NativeTiming::playMark(25,nativeCycles,pendingFrames);
    NativeTiming::mark(NativeTiming::Finished,nativeCycles,nativeLastPc);
    NativeTiming::end();
    AmigaHardware::blitterDrain();
    Permit();
    checkGuard();
    if(paula.error)fail(paula.error);
    if(!nativeVectorsRestored)fail("native vector restoration failed");
    nativeReturned();
}
void nativeRelease(){if(privateVectors){FreeMem(privateVectors,1024);privateVectors=nullptr;}nativeGuestTimerRelease();NativeTiming::release();if(liveRequested && board && (nativeStatus==3 || nativeStatus==4)){const char *error=saveNvram(board->nvram);if(error)fail(error);
    if(!error && !diagnostic && nativeSetupReady){error=saveAccounting(board->memory.data());if(error)fail(error);}
}
#ifdef POKERI_CARD_CACHE
    if(nativeCardCache){nativeCardCache->detach();videoSurface.synchronize();delete nativeCardCache;nativeCardCache=nullptr;}
    if(nativeCardStorage){FreeMem(nativeCardStorage,CardBackCache::BitmapWords*4);nativeCardStorage=nullptr;}
#endif
    screen.release();videoSurface.release();paula.release();if(DOSBase && nativeError){PutStr(nativeError);PutStr("\n");}delete reader;delete[] replayData;delete[] guard;if(board)board->~Board();delete[] boardAllocation;reader=nullptr;replayData=guard=boardAllocation=nullptr;board=nullptr;if(DOSBase)CloseLibrary((Library*)DOSBase);DOSBase=nullptr;}
