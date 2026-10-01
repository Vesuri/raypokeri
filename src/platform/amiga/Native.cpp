#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#ifdef POKERI_PAYOUT_SCENARIO
#include "native/PayoutScenario.h"
#endif
#ifdef POKERI_DOUBLE_SCENARIO
#include "native/DoubleScenario.h"
#endif
#include "Native.h"
#include "NativeTiming.h"
#if defined(POKERI_RELEASE) && (!defined(POKERI_NO_PROFILE_SUPPORT) || defined(POKERI_DOUBLE_SCENARIO) || defined(POKERI_PAYOUT_SCENARIO) || defined(POKERI_WHD_DEBUG_MAP) || defined(POKERI_TRACE_CODE) || defined(POKERI_STARTUP_PROFILE) || defined(POKERI_VBI_LATENCY))
#error Release builds cannot include diagnostic instrumentation
#endif
#include "PaulaAy.h"
#include "AmigaScreen.h"
#include "AmigaInput.h"
#include "AmigaHardware.h"
#include "NvramFile.h"
#include "board/Board.h"
#include "native/Hook.h"
#include "native/PreparedHook.h"
#include "native/LiveClock.h"
#include "native/IrqCache.h"
#include "native/DelayBudget.h"
#include "native/StartupBudget.h"
#include "native/BootPolicy.h"
#include "native/ShuffleWait.h"
#include "native/ShuffleQueue.h"
#include "Startup.h"
#include "native/Replay.h"
#include <stddef.h>
inline void *operator new(size_t,void *address) noexcept {return address;}
#include "../../../amiga/generated/NativeTables.h"
using namespace pokeri;
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
#include "../../../amiga/generated/CardBackPrepared.h"
alignas(4) static pokeri::CardBackCache *nativeCardCache=nullptr;
alignas(4) static uint16_t *nativeCardStorage=nullptr;
alignas(4) static volatile uint32_t nativeCardPrepareTicks=0;
#ifdef POKERI_TIME_LEDGER
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
alignas(4) struct DosLibrary *DOSBase=nullptr;
extern "C" {
void *pokeriAllocateUninitialized(unsigned long);
Registers nativeRegisters;
uint8_t nativeServiceStack[32768];
alignas(4) uint32_t nativeReturnStack,nativeOsUsp,nativePrepareStack,nativeOldLevel3,nativeOldLevel6,nativeOldLevel2,nativeOldLevel4;
void nativeLevel4();void nativeLevel3();void nativeLevel6();void nativeLevel2();
[[noreturn]] void nativeAbort();
[[noreturn]] void nativePrepareAbort();
uint16_t nativePhysicalSr,nativePhysicalResume;
uint16_t nativeSkipHardwareTests=0;
uint16_t nativeExtendedFrame=0,nativeFrameBytes=6;
uint32_t nativeReadVbr();uint32_t nativeProbeVbr();
void nativeWriteVbr(uint32_t);
alignas(4) uint32_t nativeFastBoundary=0,nativeRomBegin=0,nativeRomEnd=0,nativeRamBegin=0,nativeRamEnd=0;
alignas(4) volatile uint32_t nativeStatus=0,nativeInstructions=0,nativeInterrupts=0,nativeLastPc=0,nativeCycles=0,nativeVectorsRestored=0;
alignas(4) const char *nativeError=nullptr;
alignas(4) uint32_t nativeExitCode=20; // 21: replay/VBR, 22: save slots
void nativeEntry();void nativeLineA();void nativeTrace();void nativeFault();
#define TRAP(n) void nativeTrap##n();
TRAP(0) TRAP(1) TRAP(2) TRAP(3) TRAP(4) TRAP(5) TRAP(6) TRAP(7) TRAP(8) TRAP(9) TRAP(10) TRAP(11) TRAP(12) TRAP(13) TRAP(14) TRAP(15)
}
alignas(4) static Board *board;
alignas(4) static Hd63484 *videoDevice; // borrowed from Board; avoids repeated large member offsets
alignas(4) static uint8_t *boardAllocation,*rom,*guard,*replayData;
#ifdef POKERI_TRACE_CODE
// Diagnostic layout only: zero-filled writable storage in the code hunk, so
// traced original instructions keep their PCs. The executable holds no ROM data.
extern "C" uint8_t nativeTraceBoardStorage[];
asm(".pushsection .text\n.balign 4\n.globl nativeTraceBoardStorage\nnativeTraceBoardStorage:\n.space 561152\n.popsection");
static_assert(sizeof(Board)+255<=561152,"trace Board storage");
#endif
alignas(4) static PreparedHook preparedHooks[sizeof(hooks)/sizeof(*hooks)];
static bool shuffleActive=false,shuffleQueued=false,shuffleSwap=false;
alignas(4) static uint32_t shuffleFrame=0,shuffleTicket=0;
static ShuffleQueue shuffleQueue;
extern "C" alignas(4) uint32_t nativeShuffleNextPointer=0;
extern "C" alignas(4) volatile uint32_t nativeShuffleSteps=0,nativeShuffleWaitFrames=0,nativeShuffleAyWrites=0,nativeShufflePeak=0;
alignas(4) static uint32_t shuffleAyStart=0;
extern "C" __attribute__((noinline)) void nativeShufflePresented(){asm volatile("" ::: "memory");}
// mask bit 15: guarded longword compare/test; bit 1 selects A0/D4 (else A2/D0),
// bit 0 selects TST/2 bytes (else CMP/4 bytes). address then holds the value.
struct ShortStatus {uint32_t pc,address;uint16_t mask,cycles;uint32_t calls,guard,body;uint16_t length,promote;uint32_t reserved;};
static_assert(sizeof(ShortStatus)==32 && offsetof(ShortStatus,guard)==16 && offsetof(ShortStatus,length)==24,"assembly short descriptor layout");
extern "C" void nativeShortTickRteRead();
extern "C" void nativeShortStatusGuard(),nativeShortStatusRead(),nativeShortSentinelGuard(),nativeShortSentinelRead(),nativeShortControlGuard(),nativeShortControlRead(),nativeShortPiaGuard(),nativeShortPiaRead(),nativeShortIoGuard(),nativeShortIoRead(),nativeShortTrapRead(),nativeShortVideoGuard(),nativeShortVideoWrite(),nativeShortAbsoluteGuard(),nativeShortAbsoluteRead(),nativeShortSerialGuard(),nativeShortSerialPost(),nativeShortSerialBit();
static ShortStatus shortDescriptor(uint32_t pc,uint32_t address,uint16_t mask,uint16_t cycles){
    void (*guard)()=nativeShortStatusGuard,(*body)()=nativeShortStatusRead;
    unsigned length=4,promote=0;
    if(mask&0x8000){guard=nativeShortSentinelGuard;body=nativeShortSentinelRead;length=mask&1?2:4;}
    else if(mask&0x4000){guard=nativeShortControlGuard;body=nativeShortControlRead;length=0;promote=3;}
    else if(mask&0x2000){guard=nativeShortPiaGuard;body=nativeShortPiaRead;length=0;promote=2;}
    else if(mask&0x1000){guard=mask&0x80?nativeShortSerialGuard:nativeShortIoGuard;body=mask&0x200?nativeShortSerialBit:mask&0x100?nativeShortSerialPost:nativeShortIoRead;length=mask&0x300?4:mask&0x20?6:mask&0x40?2:4;promote=2;}
    else if(mask&0x0800){guard=nativeShortVideoGuard;body=nativeShortVideoWrite;length=(mask&1) && !(mask&4)?6:4;promote=2;}
    else if(mask&0x0400){guard=nativeShortAbsoluteGuard;body=nativeShortAbsoluteRead;length=mask&0x20?8:6;promote=2;}
    return {pc,address,mask,cycles,0,uint32_t(guard),uint32_t(body),uint16_t(length),uint16_t(promote),0};
}
extern "C" {
ShortStatus nativeShortTraps[16]={};
uint16_t nativeTrapNumber=0;
void nativeServiceDescriptor();
struct ServiceRedirect {uint32_t stub,pc;uint16_t armed;};
static_assert(offsetof(ServiceRedirect,armed)==8,"redirect slot layout");
alignas(4) ServiceRedirect nativeServiceRedirectState={};
uint16_t nativeServiceRedirectEnabled=0,nativeServiceOpcode=0;
uint16_t nativeServiceRequestPending=0;
static constexpr unsigned serviceDescriptors=1;
ShortStatus nativeShortStatus[sizeof(hooks)/sizeof(*hooks)+sizeof(controls)/sizeof(*controls)+serviceDescriptors]={};
uint16_t nativeShortCount=sizeof(nativeShortStatus)/sizeof(*nativeShortStatus),nativeDiagnostic=1;
uint16_t nativeShortPending=1; // bit 0: clock/IRQ work; bit 1: frame/quit during a short service
uint8_t nativeCachedVideoStatus=0;
alignas(4) uint32_t nativeShortDrainPc=0,nativeShortDrained=0;
void nativeShortAddressWrite();
alignas(4) uint32_t nativeHandlerTailPc=0,nativeHandlerTailExit=0;
// Verified at preparation: vector target $2E26, MOVEA immediate and the
// $2E30 entry descriptor. Zero keeps ordinary guest delivery.
void nativeShortHandlerJoinedSetup();
alignas(4) uint32_t nativeJoinedVector=0,nativeJoinedA0=0,nativeJoinedEntry=0;
alignas(4) uint32_t nativeHandlerFeed=0,nativeHandlerEmpty=0;
void nativeShortHandlerEntry(),nativeShortHandlerExit(),nativeShortFifoControl(),nativeShortSoundWrite();
Hd63484::AddressSelector nativeVideoSelector={};
static_assert(sizeof(Hd63484::AddressSelector)==12 && sizeof(bool)==1,"assembly address selector layout");
void nativeShortFeedLoopWrite(),nativeShortFeedRead();
alignas(4) uint32_t nativeFeedLoopWords=0,nativeFeedLoopTurns=0,nativeFeedLoopSaved=0;
alignas(4) CachedRasterGrant nativeRasterGrant{};
alignas(4) uint32_t nativeRasterGrantActive=0,nativeRasterHits=0;
static void revokeRasterGrant(){
    nativeRasterGrantActive=0;
}
alignas(4) uint32_t nativeFeedInlineCount=0,nativeFeedInlineWords=0;
alignas(4) uint32_t nativeFeedHeaderGrant=0,nativeFeedHeaderWords=0;
alignas(4) int *nativeFeedInlineLength=nullptr;
alignas(4) const Hd63484::CommandFormat *nativeFeedFormats=Hd63484::formats;
alignas(4) uint16_t *nativeFeedInlineWord=nullptr;
alignas(4) unsigned *nativeFeedInlinePending=nullptr;
alignas(4) uint8_t *nativeFeedInlineHigh=nullptr;
alignas(4) uint32_t nativeFeedTarget=0,nativeFeedTests=0,nativeFeedBranches=0,nativeFeedWrites=0;
alignas(4) uint32_t nativeShortGuest=0,nativeShortNominal=0,nativeShortCalls=0,nativeShortCharge[256]={};
}
struct PreparedAccess {uint32_t physical;};
alignas(4) static PreparedAccess preparedAccesses[sizeof(accesses)/sizeof(*accesses)];
static uint8_t originalVectors[12];
alignas(4) static uint32_t romBase,ramBase,guardBase,replaySize,lastGuardCycle,liveStopCycles,guardCursor;
extern "C" alignas(4) uint32_t nativeVirtualUsp=0,nativeVirtualSsp=0;
alignas(4) static uint32_t liveTicks=0;
// A reserved CIA timer counts only the intervals outside native services.
bool nativeGuestTimerPrepare();void nativeGuestTimerRelease();
extern "C" volatile uint8_t *nativeGuestTimerControl,*nativeGuestTimerLow,*nativeGuestTimerHigh;
extern "C" volatile uint16_t nativeClockEnabled;
extern "C" volatile uint16_t nativeClockRunning=0;
extern "C" alignas(4) uint32_t nativeClockResumePc=0;
#ifdef POKERI_STARTUP_PROFILE
extern "C" volatile uint32_t pendingFrames;
extern "C" uint32_t nativeStartupTicks[3]={};
// Authored diagnostic marker: three TOD samples, paired PAL-frame samples,
// then a completion mask. Recoverable by read-only WHDLoad RAM capture.
extern "C" volatile uint32_t nativeStartupRecord[11]={0x504f4b21,0x424f4f54,0x54494d45,0x30303031};
static void startupTimestamp(unsigned slot){
    // Read the CIA-A TOD high/mid/low latch once at each startup boundary.
    // Calibration against the existing PAL VBI count is part of the capture.
    // These three diagnostic calls are outside recurring guest services.
    Disable();
    unsigned high=*(volatile uint8_t*)0xbfea01;
    unsigned mid=*(volatile uint8_t*)0xbfe901;
    unsigned low=*(volatile uint8_t*)0xbfe801;
    const uint32_t tick=(high<<16)|(mid<<8)|low;
    nativeStartupRecord[4+slot]=tick;
    nativeStartupRecord[7+slot]=pendingFrames;
    nativeStartupRecord[10]|=1u<<slot;
    Enable();
    nativeStartupTicks[slot]=tick;
}
#endif
alignas(4) static uint32_t guestClockPhase=0;
static bool startupFast=false;
static uint16_t startupDelayOpcode=0,startupCabinetTicks=0;
extern "C" alignas(4) volatile uint32_t pendingFrames=0;
static LiveClock liveClock;
// Request 4 from the separately measured acceptance-workload lower bounds.
// Boot keeps its independently calibrated 1.5 cap. CPU probes may lower both.
static uint16_t playClockRatio=64,cpuClockLimit=80,playClockWindow=3;
static bool clockDisplayCalibrated=false;
extern "C" alignas(4) volatile uint32_t nativeClockRaw=0,nativePollMin=0xffffffffu,nativePollMax=0,nativePollCount=0,nativePollTotal=0;
extern "C" __attribute__((noinline)) void nativeClockSampleReady(){asm volatile("" ::: "memory");}
extern "C" uint16_t nativePollSamples[256],nativeCalibrationSamples[64];
alignas(4) uint16_t nativePollSamples[256],nativeCalibrationSamples[64];
alignas(4) static uint32_t previousPollD1=0;static bool uninterruptedPoll=false;
extern "C" alignas(4) uint64_t nativeClockCharged[3]={},nativeClockObserved=0;
__attribute__((always_inline)) inline
static void accountGuestCycles(uint32_t cycles,unsigned source=0){
    if(NativeTiming::isActive())nativeClockCharged[source]+=cycles;
    if(!startupFast)cycles=liveClock.grant(cycles,source!=0,pendingFrames,liveTicks>=2?160000:guestClockPhase+(liveTicks?80000:0));
    if(startupFast)cycles=startupWorkCycles(cycles,source!=0,liveClock.ratioSixteenths);
    guestClockPhase+=cycles;
    const unsigned quantum=startupFast?8000:80000;
    while(guestClockPhase>=quantum){guestClockPhase-=quantum;++liveTicks;}
}
static bool liveIrqActive=false;
alignas(4) static uint64_t liveCycles=0;
alignas(4) static PaulaAy paula;
static AmigaScreen screen;
static AmigaSurface videoSurface;
static bool liveRequested=false,displayRequested=false;
extern "C" bool compositionPending=false;
// Outermost original system-tick exception frame, including user-mode callbacks.
// Nested ticks must not release presentation before the outer callback returns.
extern "C" alignas(4) uint32_t presentationTickFrame=0;
extern "C" alignas(4) volatile uint32_t nativeBootVerified=0;
extern "C" __attribute__((noinline)) void nativeBootReady(){asm volatile("" ::: "memory");}
alignas(4) static ReplayReader *reader;static ReplayEvent nextEvent;static bool haveEvent;
#ifdef POKERI_RELEASE
static constexpr bool diagnostic=false;
#else
static bool diagnostic=true;
#endif
// Marker files are development controls, never user-facing release options.
static inline BPTR researchMarker(const char *name){
#ifdef POKERI_RELEASE
    (void)name;return 0;
#else
    return Open(name,MODE_OLDFILE);
#endif
}
static IrqCache nativeIrqCache;
static void invalidatePeripheralIrq(){
    nativeIrqCache.invalidate();
}
static void peripheralAccess(uint32_t address,unsigned size){
    nativeIrqCache.beforeAccess(address,size);
}
static bool peripheralIrq(){
    if(!diagnostic)return nativeIrqCache.peripherals(*board);
    return board->pia[0].Pia6821::irq() || board->serial[0].Acia6850::irq();
}
static unsigned currentIrq(){
    if(!diagnostic)return nativeIrqCache.level(*board,board->video.statusNow());
    return board->irq();
}

// The full live dispatcher has just refreshed video status, or advanced the
// board clock (which refreshes it). Neither source query changes device state.
static unsigned dispatchIrqAtStatus(){
    return nativeIrqCache.level(*board,nativeCachedVideoStatus);
}

extern "C" alignas(4) volatile uint32_t nativeClockOverhead=0,nativeClockMinimum=0,nativeClockMaximum=0;
extern "C" volatile uint16_t nativeClockCalibrating=0;
extern "C" void nativeClockCalibrationCode();
alignas(4) static Registers clockSavedRegisters;
static uint16_t clockSavedResume;
static uint8_t clockCalibrationStack[64];
alignas(4) static uint32_t clockCalibrationTotal;
extern "C" void nativeSpeedLoop();
extern "C" void nativeSpeedMemory();
extern "C" void nativeSpeedArithmetic();
extern "C" uint32_t nativeSpeedCycles[3]={};
alignas(4) static unsigned speedCalibration=0;
// Same 8,192 synthetic iterations, with an IRQ window every 256 iterations.
// Every piece uses the measured exception overhead and has its own final
// not-taken branch, accounted for in the reference below.
alignas(4) static unsigned speedPiece=0;
static constexpr unsigned speedIterations=256,speedPieces=32;
static_assert(speedIterations*speedPieces==8192,"calibration instruction budget");
alignas(4) static uint32_t speedMemory[16]={};
static void speedNext(){
    void (*const code[])()={nativeSpeedLoop,nativeSpeedMemory,nativeSpeedArithmetic};
    nativeRegisters.pc=uint32_t(code[speedCalibration-1]);nativeRegisters.d[0]=speedIterations;
    nativeRegisters.a[0]=uint32_t(speedMemory);nativeClockCalibrating=1;
}
static void prepareShortClock(){
    for(unsigned i=0;i<256;++i){uint32_t raw=boardClockCycles(i);
        nativeShortCharge[i]=raw>nativeClockOverhead?raw-nativeClockOverhead:0;}
}
extern "C" void nativeClockEnter(){
    if(nativeClockEnabled){
        uint16_t ticks=uint16_t(0xffff-(uint16_t(*nativeGuestTimerHigh)<<8|*nativeGuestTimerLow));
        nativeClockRaw=boardClockCycles(ticks);
    }
}
extern "C" void nativeClockLeave(){
    nativeClockRunning=1;
}
extern "C" void nativeClockPause(){
    if(!nativeShortGuest && !nativeShortNominal && !nativeClockRunning)return;
    if(!diagnostic){
        // The call counter is cumulative. Only write deferred totals when
        // actual work is pending; preserve the separate credit grants/order.
        uint32_t guest=nativeShortGuest,nominal=nativeShortNominal;
        // Single contributions retain their cheaper original path. Batch only
        // with VBI masked; low-IPL callers retain per-grant wall-frame reads.
        if((guest && nominal) || (nativeClockRunning && (guest || nominal))){
            uint16_t sr;asm volatile("move.w %%sr,%0":"=d"(sr));
            if((sr&0x0700)>=0x0300 && !NativeTiming::isActive()
               && !startupFast
            ){
                if(guest)nativeShortGuest=0;
                if(nominal)nativeShortNominal=0;
                uint32_t raw=nativeClockRaw>nativeClockOverhead?nativeClockRaw-nativeClockOverhead:0;
                guestClockPhase+=liveClock.grantBatch(guest,nominal,raw,nativeClockRunning,
                    pendingFrames,liveTicks>=2?160000:guestClockPhase+(liveTicks?80000:0));
                while(guestClockPhase>=80000){guestClockPhase-=80000;++liveTicks;}
                nativeClockRunning=0;return;
            }
        }
        if(guest){nativeShortGuest=0;accountGuestCycles(guest);}
        if(nominal){nativeShortNominal=0;accountGuestCycles(nominal,1);}
        if(nativeClockRunning){
            if(NativeTiming::isActive())nativeClockObserved+=nativeClockRaw;
            accountGuestCycles(nativeClockRaw>nativeClockOverhead?nativeClockRaw-nativeClockOverhead:0);
        }
    }
    nativeClockRunning=0;
}
extern "C" void nativeClockPauseInterrupt(){
    // Autovector entry (44) versus Line-A (34), plus BTST/BNE.W (24)
    // versus MOVE-to-SR (16) before the identical timer-stop sequence.
    const unsigned overhead=20;
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
        nativeSpeedCycles[speedCalibration-1]+=nativeClockRaw>nativeClockOverhead?nativeClockRaw-nativeClockOverhead:1;
        if(++speedPiece<speedPieces){speedNext();return;}
        speedPiece=0;
        if(++speedCalibration<=3){speedNext();return;}
        // Three synthetic instruction mixes, 12.5% headroom, never above the
        // requested ratio when applied. Keep the ceiling for the separately
        // measured gameplay cap. These probes alone do not validate a workload.
        const uint32_t reference[]={8192*14-2*speedPieces,8192*22-2*speedPieces,8192*28-2*speedPieces};
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
    const unsigned nop=5;
    nativeClockOverhead=nativeClockMaximum>nop?nativeClockMaximum-nop:0;
    prepareShortClock();
    speedPiece=0;nativeSpeedCycles[0]=nativeSpeedCycles[1]=nativeSpeedCycles[2]=0;
    speedCalibration=1;speedNext();
}
static unsigned hookCycles(uint32_t pc){
    unsigned low=0,high=sizeof(originalCycles)/sizeof(*originalCycles);
    while(low<high){unsigned mid=(low+high)/2;if(originalCycles[mid].pc<pc)low=mid+1;else high=mid;}
    return low<sizeof(originalCycles)/sizeof(*originalCycles) && originalCycles[low].pc==pc?originalCycles[low].cycles:0;
}
alignas(4) static uint32_t savedVectors[48];
alignas(4) static volatile uint32_t *nativeVectors;
alignas(4) static uint32_t *privateVectors=nullptr,originalVbr=0;
extern "C" uint16_t pokeriWhdLoad;
static_assert(offsetof(Registers,a)==32 && offsetof(Registers,pc)==64 && offsetof(Registers,sr)==68,"assembly register layout");
extern "C" alignas(4) uint32_t seenFrames=0;static volatile bool installed=false,quitRequested=false;
alignas(4) static uint16_t originalControl[sizeof(controls)/sizeof(*controls)],controlCycles[sizeof(controls)/sizeof(*controls)];
static uint32_t get32(const uint8_t*p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
static uint16_t get16(const uint8_t*p){return (uint16_t(p[0])<<8)|p[1];}
static void put32(uint8_t*p,uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static void put16(uint8_t*p,unsigned n){p[0]=n>>8;p[1]=n;}
static bool fail(const char *s){if(!nativeError)nativeError=s;nativeStatus=0xdead;return false;}
extern "C" void pokeriRuntimeFault(const char *s){fail(s);if(installed)nativeAbort();nativePrepareAbort();}
// Guest layout: ROM $00000-$3FFFF, RAM window $40000-$4FFFF, device window
// $F0000-$FFFFF (every relocated device address and audited access lies in
// $F6000-$FBFFF). Other guest addresses have no allocation.
static constexpr uint32_t ramEnd=Board::mappedMemory,deviceBegin=0xf0000,guardSize=0x10000;
static uint32_t canonical(uint32_t a){if(a>=romBase && a-romBase<0x40000)return a-romBase;if(a>=ramBase && a-ramBase<ramEnd-0x40000)return a-ramBase+0x40000;if(a>=guardBase && a-guardBase<guardSize)return a-guardBase+deviceBegin;return 0xffffffffu;}
static uint32_t relocated(uint32_t a){return a<0x40000?romBase+a:a<ramEnd?ramBase+a-0x40000:a>=deviceBegin && a<0x100000?guardBase+a-deviceBegin:0;}
static bool advanceEvent(){haveEvent=reader->next(nextEvent);nativeFastBoundary=diagnostic && haveEvent && !quitRequested?nextEvent.instruction:0;return haveEvent || reader->complete()?true:fail("invalid/truncated replay");}
static bool advanceClock(uint32_t target){NativeTiming::Scope timing(NativeTiming::BoardTick);if(diagnostic && target<nativeCycles)return fail("replay clock reversed");uint32_t delta=target-nativeCycles;
    invalidatePeripheralIrq();board->tick(delta);nativeCachedVideoStatus=board->video.statusNow();if(!diagnostic)liveCycles+=delta;nativeCycles=target;return !board->fault || fail(board->faultReason);}
// Live service is bounded to 1 KB; diagnostic replay and exit inspect all 64 KB
// and the RAM-window canary. One complete live sweep takes 64 serviced frames
// (1.28 s at 50 Hz); the 4 KB RAM canary is checked when the sweep wraps.
static bool canaryRange(const uint8_t *base,unsigned begin,unsigned end){
    const uint32_t *at=(const uint32_t*)(base+begin),*finish=(const uint32_t*)(base+end);
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
static bool guardRange(unsigned begin,unsigned end){return canaryRange(guard,begin,end);}
static bool ramCanaryIntact(){return canaryRange(board->memory.data(),ramEnd,ramEnd+Board::ramCanary);}
extern "C" alignas(4) volatile uint32_t nativeGuardSelfTest=0;
static bool testGuard(){
    if(!guardRange(0,guardSize) || !ramCanaryIntact())return false;
    for(unsigned offset: {0u,1020u,0x8000u,unsigned(guardSize-4)}){
        uint32_t *word=(uint32_t*)(guard+offset);*word^=1;
        bool detected=!guardRange(0,guardSize);
        bool bounded=guardRange(0,1024)==(offset>=1024);
        *word^=1;if(!detected || !bounded)return false;
    }
    for(unsigned offset: {0u,unsigned(Board::ramCanary-4)}){
        uint32_t *word=(uint32_t*)(board->memory.data()+ramEnd+offset);*word^=1;
        bool detected=!ramCanaryIntact();
        *word^=1;if(!detected)return false;
    }
    nativeGuardSelfTest=1;return guardRange(0,guardSize) && ramCanaryIntact();
}
static bool checkGuard(bool incremental=false){
    NativeTiming::Scope timing(NativeTiming::Guard);
    unsigned begin=incremental?guardCursor:0,end=incremental?begin+1024:guardSize;
    if(!guardRange(begin,end))return fail("unhooked device write reached guard");
    if((!incremental || end==guardSize) && !ramCanaryIntact())return fail("write beyond guest RAM window");
    if(incremental)guardCursor=end&(guardSize-1);
    lastGuardCycle=nativeCycles;return true;
}
static void setSr(uint16_t value){value&=0xa71f;if(value&0x8000)fail("uncovered guest trace mode");Registers&r=nativeRegisters;if((r.sr^value)&0x2000){if(r.sr&0x2000){nativeVirtualSsp=r.a[7];r.a[7]=nativeVirtualUsp;}else{nativeVirtualUsp=r.a[7];r.a[7]=nativeVirtualSsp;}}r.sr=value;}
extern "C" uint32_t nativeExceptionFrame(uint8_t*,unsigned,uint32_t,const uint8_t*);
static bool pushException(unsigned vector,unsigned level){
    Registers&r=nativeRegisters;uint16_t sr=r.sr;setSr(uint16_t((sr|0x2000)&~0x8000));
    if(level)r.sr=uint16_t((r.sr&~0x700)|(level<<8));
    uint32_t sp=canonical(r.a[7]-6);if(sp<0x40000 || sp>=ramEnd-6)return fail("virtual exception stack outside RAM");
    r.a[7]-=6;
    r.pc=nativeExceptionFrame(board->memory.data()+sp,sr,r.pc,rom+vector*4);
    if(!diagnostic && vector==0x43 && !presentationTickFrame)presentationTickFrame=r.a[7];
    return true;
}
static void resetShuffle(){shuffleQueue.reset();shuffleActive=shuffleQueued=false;nativeShuffleNextPointer=0;board->video.presentationBusy=false;}
static void resetCpu(){invalidatePeripheralIrq();compositionPending=false;presentationTickFrame=0;resetShuffle();liveIrqActive=false;setSr(0x2700);nativeRegisters.a[7]=get32(rom);nativeRegisters.pc=get32(rom+4);nativeVirtualSsp=nativeRegisters.a[7];}
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
        if(local>=ramEnd){
            unsigned first=firstAccess;
            bool allowed=false;
            while(first<lastAccess){
                const auto &e=accesses[first++];
                if(e.address==local && e.size==size && e.write==writing){allowed=true;break;}
            }
            if(!allowed)return fail("device access outside hook table");
        }
        if(writing && local<0x40000){if(pc==0x2184 || pc==0x2358 || pc==0x25aa)return true;return fail("unexpected write to program image");}
        peripheralAccess(local,size);
        NativeTiming::Scope videoTiming(NativeTiming::VideoBus,0,local>=0xf6000 && local<0xf6004);
        if(!writing)v=0;
        for(unsigned i=0;i<size;++i){if(writing){uint8_t b=v>>(8*(size-i-1));if(local<ramEnd)board->memory[local+i]=b;else {
                // Only control-register writes can change display geometry.
                // FIFO drawing marks Surface dirty separately. Observe each
                // byte so an AR auto-increment is handled in bus order.
                if(((local+i)&~1u)==0xf6002)screen.controlWrite(board->video,b);
                board->write8(local+i,b);
            }}else v=(v<<8)|(local<0x40000?rom[local+i]:local<ramEnd?board->memory[local+i]:board->read8(local+i));}
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
            peripheralAccess(e.address,size);
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
    else if(pia==3){if(!side)invalidatePeripheralIrq();board->serial[side].receive.push_back(e.b);}
    else board->pia[pia].input[side]=e.b;
    return true;
}
// Opt-in platform diagnostics exercise the normal key path after boot.
extern "C" __attribute__((noinline)) void nativeUnexpectedReset(){asm volatile("" ::: "memory");}
extern "C" alignas(4) volatile uint32_t nativeLiveWatchdogResets=0,nativeFirstResetPc=0,nativeFirstResetCycle=0;
static bool testInputs=false,testWrap=false,stopOnLiveReset=false;
alignas(4) static uint32_t testInputIndex=0,liveStart=0,lastInputEdge=0;
static bool coldSetup=false;
static pokeri::Startup startup;
extern "C" alignas(4) volatile uint32_t nativeSetupReady=0;
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
        if(startupFast){
            startupFast=false;guestClockPhase=liveTicks=0;
            nativeShortGuest=nativeShortNominal=0;
            liveClock.reset(pendingFrames);
            // A one-time startup transition, not a recurring service operation.
            // Return the original delay instruction before normal play resumes.
            put16(rom+0x2442,startupDelayOpcode);CacheClearU();
            paula.muted=false;
        }
        paula.wallEnvelope=!diagnostic;
        nativeSetupReady=1;NativeTiming::playMark(0,nativeCycles,pendingFrames);liveStart=uint32_t(liveCycles);
        if(playClockRatio || playClockWindow!=1){
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
#ifdef POKERI_PAYOUT_SCENARIO
pokeri::PayoutScenario nativePayoutScenario;
#endif
#ifdef POKERI_DOUBLE_SCENARIO
pokeri::DoubleScenario nativeDoubleScenario;
#endif
#ifdef POKERI_RELEASE
static void diagnosticKeys(){}
#else
static void diagnosticKeys(){
#ifdef POKERI_PAYOUT_SCENARIO
    if(!testInputs)return;
    const uint8_t *m=board->memory.data();
    nativePayoutScenario.step(uint32_t(liveCycles-liveStart),m[0x4112f]!=0,
        [m](unsigned address){return get32(m+address);},
        [m]{return pokeri::DoubleScenario::holds([m](unsigned i){return get32(m+0x41150+4*i);},
                                                 [m](unsigned i){return get32(m+0x41168+4*i);});},
        [](unsigned code,bool down){++testInputIndex;amigaInputKey(code,down);});
    if(nativePayoutScenario.failed)fail("payout regression: accounting or subsequent play failed");
    if(nativePayoutScenario.done)liveStopCycles=uint32_t(liveCycles);
    return;
#endif
#ifdef POKERI_DOUBLE_SCENARIO
    if(!testInputs)return;
    const uint8_t *m=board->memory.data();
    nativeDoubleScenario.step(uint32_t(liveCycles-liveStart),m[0x4112f]!=0,
        [m]{return pokeri::DoubleScenario::holds([m](unsigned i){return get32(m+0x41150+4*i);},
                                                 [m](unsigned i){return get32(m+0x41168+4*i);});},
        [](unsigned code,bool down){++testInputIndex;amigaInputKey(code,down);});
    if(nativeDoubleScenario.failed)fail("diagnostic found no Double-ready hand in 12 rounds");
    if(nativeDoubleScenario.done)liveStopCycles=uint32_t(liveCycles);
    return;
#endif
    struct Key {uint16_t ms;uint8_t code,down;};
    static const Key keys[]={
        {100,0x44,1},{300,0x44,0}, // coin for a clean zero-credit start
        {500,0x40,1},{700,0x40,0}, // deal
        {8500,0x51,1},{8500,0x53,1},{8500,0x54,1},
        {8700,0x51,0},{8700,0x53,0},{8700,0x54,0},
        {10500,0x40,1},{10700,0x40,0}, // draw
        {19520,0x55,1},{19720,0x55,0}, // double
        {23500,0x57,1},{23700,0x57,0}, // big
        {28000,0x28,1},{28200,0x28,0}, // lamp panel
        {29000,0x44,1},{29200,0x44,0}, // coin
        {31000,0x46,1},{31200,0x46,0}, // service door
        {35000,0x46,1},{35200,0x46,0}
    };
    if(!testInputs)return;
    while(testInputIndex<sizeof(keys)/sizeof(*keys) &&
          liveCycles-liveStart>=uint32_t(keys[testInputIndex].ms)*uint16_t(8000)){
        const Key &key=keys[testInputIndex++];NativeTiming::playMark(testInputIndex,nativeCycles,pendingFrames);amigaInputKey(key.code,key.down);
    }
}
#endif
#ifdef POKERI_RELEASE
static bool liveInputs(){return true;}
static bool replayBoundary(){return true;}
#else
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
        if(e.kind==ReplayIrq){if(currentIrq()!=e.a || board->vector()!=e.b || ((nativeRegisters.sr>>8)&7)>=e.a)return fail("replay interrupt state mismatch");++nativeInterrupts;if(!pushException(e.b,e.a))return false;}
        else if(e.kind==ReplayReset){if(!board->resetRequested)return fail("replay watchdog not due");invalidatePeripheralIrq();board->reset();resetCpu();}
        else if(e.kind==ReplayInput){if(!applyInput(e))return false;}
        else if(e.kind==ReplayEnd){nativeLastPc=canonical(nativeRegisters.pc);if(nativeInterrupts!=e.a)return fail("replay IRQ count mismatch");if(!advanceEvent())return false;
            if(displayRequested && !screen.present(board->video,true))return fail(screen.error);
            board->video.flushCard();videoSurface.synchronize();
            nativeBootVerified=1;nativeBootReady();
            if(!liveRequested){nativeStatus=2;return false;}
            NativeTiming::begin();
            put16(rom+ShuffleWait::pc,0xaffb);
            diagnostic=false;nativeDiagnostic=0;nativeClockEnabled=1;nativeFastBoundary=0;seenFrames=pendingFrames;liveTicks=0;liveCycles=nativeCycles;liveStart=nativeCycles;liveClock.reset(pendingFrames);
            if(testWrap){nativeCycles=0xffff0000u;lastGuardCycle=nativeCycles;}
            return true;}
        else return fail("unexpected replay event");
        if(!advanceEvent())return false;
    }
    if(haveEvent && nextEvent.instruction<nativeInstructions)return fail("missed replay boundary");
    return true;
}
#endif
// These three audited boot loops contain no other hook or callable path.
// CIA quantization on a fast CPU cannot time a single iteration. Charge its
// actual 68000 branch/decrement cost when consecutive polls prove that path.
alignas(4) static uint32_t previousTimingPc=0xffffffffu,previousTimingCounter=0;
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
        else {if(NativeTiming::isActive())++NativeTiming::mainLoops;amigaInputObserve(0x2472,*board);}
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
    unsigned irq=currentIrq();
    nativeShortPending=(liveTicks || irq?1:0)|((pendingFrames!=seenFrames || quitRequested || irq>((nativeRegisters.sr>>8)&7))?2:0);
    if(board->fault){fail(board->faultReason);nativeShortPending|=2;}
}
extern "C" unsigned nativeShortIoReadValue(uint32_t address){
    LEDGER_SCOPE(call,ShortCall);
    nativeIrqCache.beforeByte<false>(address-guardBase+deviceBegin);
    unsigned value=board->read8(address-guardBase+deviceBegin);shortIoCompleted();return value;
}
extern "C" unsigned nativeShortIoWriteValue(uint32_t address,unsigned value){
    LEDGER_SCOPE(call,ShortCall);
    nativeIrqCache.beforeByte<true>(address-guardBase+deviceBegin);
    board->write8(address-guardBase+deviceBegin,uint8_t(value));shortIoCompleted();return uint8_t(value);
}
// Exactly the same byte-ordered endpoint operations as PreparedBus. Keep the
// model authoritative, including command completion, FIFO and IRQ side effects.
extern "C" unsigned nativeShortVideoWriteValue(uint32_t address,unsigned value,unsigned kind){
    LEDGER_SCOPE(call,ShortCall);
    Hd63484 &video=*videoDevice;
    unsigned offset=address-guardBase+deviceBegin-0xf6000;
    LEDGER_SCOPE(command,Command);
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
    if(!diagnostic && kind==7 && offset==2)
    {
        nativeRasterGrantActive=video.cardCache && video.cardCache->rasterGrant(video,nativeRasterGrant,true,true);
        nativeFeedInlineCount=video.inlineParameters(nativeFeedInlineWord,nativeFeedInlinePending,nativeFeedInlineHigh);
        if(!nativeFeedInlineCount)
            nativeFeedHeaderGrant=video.inlineHeader(nativeFeedInlineWord,nativeFeedInlinePending,nativeFeedInlineHigh,nativeFeedInlineLength);
    }
    return value;
}
#ifdef POKERI_TIME_LEDGER
extern "C" void nativeFeedHeaderStarted(unsigned word){
    if(videoDevice->cardCache)videoDevice->cardCache->wordStart(uint16_t(word));
}
#endif
// Called only by the existing guarded byte-write member of the fused triplet.
// CCR low has no display or FIFO side effects. All other cases keep the
// authoritative general endpoint; interrupt selection/order is unchanged.
extern "C" unsigned nativeFifoControlValue(uint32_t address,unsigned value,unsigned kind){
    Hd63484 &video=*videoDevice;
    if(diagnostic || video.ar!=3 || video.error || board->fault
      )return nativeShortVideoWriteValue(address,value,kind);
    LEDGER_SCOPE(call,ShortCall);
    video.control[3]=uint8_t(value);
    nativeCachedVideoStatus=video.statusNow();
    unsigned irq=peripheralIrq() || (nativeCachedVideoStatus&uint8_t(value))?5:0;
    nativeShortPending=(liveTicks || irq?1:0)|((pendingFrames!=seenFrames || quitRequested || irq>((nativeRegisters.sr>>8)&7))?2:0);
    nativeFeedInlineCount=0;nativeFeedHeaderGrant=0;revokeRasterGrant();
    return value;
}
#ifndef POKERI_RELEASE
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
#endif
extern "C" uint32_t nativeDelayApply(Registers*,uint32_t);
extern "C" alignas(4) uint32_t nativeIdleCalls=0,nativeIdleInstructions=0,nativeIdleCycles=0,nativeIdleWaits=0;
static unsigned startupQuietBudget(){
    const auto &c=board->config;
    // Preserve unsupported/research profiles and explicit input-file timing.
    if(!startupFast || haveEvent || board->fault || liveIrqActive ||
       (nativeRegisters.sr&0x700)>=0x500 || c.cpuHz!=8000000 ||
       c.systemHz!=100 || c.inputHz!=50)return 1;
    pokeri::Board::TimingSnapshot time;
    if(!board->timingSnapshot(time) || time.systemPhase>=8000000 || time.inputPhase>=8000000)return 1;
    if(board->peer.enabled && (!board->serial[0].transmit.empty() ||
       !board->peer.wire.empty() || (!board->peer.pending.empty() && (board->peer.link()==pokeri::SerialPeer::Idle)) ||
       board->peer.error))return 1;
    unsigned ticks=startupQuietTicks(startupCabinetTicks,uint32_t(time.systemPhase),
        uint32_t(time.inputPhase),time.watchdogAge,time.warning,time.reset,
        c.watchdogMs!=0,c.watchdogMs && c.watchdogResetUs);
    uint64_t end=liveCycles;
    if(liveStopCycles)for(unsigned n=1;n<ticks;++n){end+=8000;if(end>=liveStopCycles)return n;}
    return ticks;
}
extern "C" alignas(4) uint32_t nativeStartupDelayShortHits=0;
// Clock endpoints are shared with full dispatch. Declining after this call must
// use the already-paused entry: reading the CIA again would charge service time.
extern "C" unsigned nativeTryStartupDelay(uint32_t counter,uint32_t pc){
    nativeClockEnter();
    if(nativeClockResumePc==pc)nativeClockRunning=0;
    nativeClockPause();
    ServiceInterrupts serviceInterrupts;
    if(!startupFast || diagnostic || haveEvent || NativeTiming::isActive() ||
       board->fault || quitRequested || liveIrqActive || liveTicks ||
       (nativeRegisters.sr&0x700)>=0x500 || nativeShortPending ||
       pendingFrames!=seenFrames || nativeClockCalibrating ||
       nativeFeedInlineCount || nativeFeedHeaderGrant)return 0;
    if(nativeRasterGrantActive)return 0;
    // Only a complete loop strictly before the next existing 1 ms quantum.
    // Zero wraps 65536 times and cannot fit. No hardware or frame edge is skipped.
    if(!uint16_t(counter) || guestClockPhase>=8000)return 0;
    uint32_t cycles=wordProduct(uint16_t(counter),14)-2;
    if(cycles>=8000-guestClockPhase)return 0;
    ++nativeStartupDelayShortHits;++nativeIdleCalls;
    nativeIdleInstructions+=uint32_t(uint16_t(counter))<<1;nativeIdleCycles+=cycles;
    accountGuestCycles(cycles,2);
    return 1;
}
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
    if(startupFast){
        if(quitRequested || pendingFrames!=seenFrames || liveTicks ||
           currentIrq()>((nativeRegisters.sr>>8)&7))return 0;
        // 1 ms is the next possible serial-peer edge; timer/input/watchdog
        // edges in the supported profile are integer multiples of this.
        return delaySteps(uint16_t(nativeRegisters.d[6]),startupDelayAvailable(guestClockPhase,startupQuietBudget()));
    }
    for(;;){
        if(quitRequested || pendingFrames!=seenFrames || liveTicks ||
           currentIrq()>((nativeRegisters.sr>>8)&7))return 0;
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
    if(sp<0x40000 || sp>ramEnd-4)return fail("shuffle return stack outside RAM");
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
    if(diagnostic)return true;
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
            if(NativeTiming::isActive())nativeShufflePresented();
        }
    }
    video.presentationBusy=shuffleQueue.held;
    nativeShuffleNextPointer=shuffleQueue.nextPointer();
    return true;
}
#include "NativeVideoIrqLayout.h"
// Keep the ordinary instruction executor out of the common scheduler. This
// reduces measured dispatch cost without changing instruction effects/order.
static __attribute__((noinline)) bool executeLineA(uint32_t pc,bool countInstruction){
    Registers &r=nativeRegisters;
    unsigned index=get16(rom+pc)&0xfff;
    NativeTiming::hook(index);
    if(!diagnostic && index!=0xffc && index!=0xffb){NativeTiming::routine(NativeTiming::RGuestCharge);accountGuestCycles(index<sizeof(hooks)/sizeof(*hooks)?hookMetadata[index].cycles:index<nativeShortCount?controlCycles[index-sizeof(hooks)/sizeof(*hooks)]:hookCycles(pc),1);}
    if(index<sizeof(hooks)/sizeof(*hooks)){
        const pokeri::Hook &h=hooks[index];if(h.pc!=pc)return fail("Line-A index/site mismatch");
        bool device=hardwareHooks[index];
        if(diagnostic && device){if(!haveEvent || nextEvent.kind!=ReplayBus || nextEvent.instruction!=nativeInstructions || nextEvent.pc!=pc)return fail("replay I/O boundary mismatch");if(!advanceClock(nextEvent.cycle) || !advanceEvent())return false;}
        PreparedBus bus{hookMetadata[index],pc};NativeTiming::routine(NativeTiming::RPreparedHook);LEDGER_SCOPE(hookTiming,HookExec);
        if(!executePreparedHook(preparedHooks[index],r,bus))return fail("unsupported native hook");
    }else if(index==0xffb){
        if(diagnostic || pc!=ShuffleWait::pc)return fail("unknown shuffle hook");
        if(!shuffleBoundary())return false;
    }else if(index==0xffc){
        if(!startupFast || pc!=0x2442)return fail("unknown idle hook");
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
        if(diagnostic && !videoSurface.cardTested && !videoSurface.cardBlitTest())return fail("card masked-blit self-test failed");
        r.d[7]=ramBase-0x40000;r.a[6]=0x40b00;r.pc+=6;}
    else if(index==0xffd){
        if(!(r.sr&0x2000))return fail("virtual privilege violation at RESET");
        bool found=false;for(auto offset:resets)if(pc==offset)found=true;if(!found)return fail("unknown RESET hook");
        if(diagnostic){if(!haveEvent || nextEvent.kind!=ReplayPeripheralReset || nextEvent.instruction!=nativeInstructions || nextEvent.pc!=pc)return fail("replay RESET mismatch");if(!advanceClock(nextEvent.cycle)||!advanceEvent())return false;}NativeTiming::routine(NativeTiming::RBoardReset);invalidatePeripheralIrq();board->reset();resetShuffle();compositionPending=false;presentationTickFrame=0;r.pc+=2;
    }else if(index<nativeShortCount){
        unsigned i=index-sizeof(hooks)/sizeof(*hooks);if(controls[i]!=pc)return fail("CPU-control index/site mismatch");uint16_t op=originalControl[i];
        if((op&0xfff8)!=0x40c0 && !(r.sr&0x2000))return fail("virtual privilege violation at CPU-control hook");
        if(op==0x4e73){NativeTiming::routine(NativeTiming::RCpuRte);const bool tickReturn=r.a[7]==presentationTickFrame;uint32_t sp=canonical(r.a[7]);if(sp<0x40000 || sp>=ramEnd-6)return fail("RTE stack outside RAM");uint16_t sr=get16(board->memory.data()+sp);r.pc=get32(board->memory.data()+sp+2);r.a[7]+=6;setSr(sr);
            if(!diagnostic && tickReturn){presentationTickFrame=0;compositionPending=true;}
        }
        else if((op&0xfff0)==0x4e60){NativeTiming::routine(NativeTiming::RCpuUsp);unsigned reg=op&7;if(op&8)r.a[reg]=nativeVirtualUsp;else nativeVirtualUsp=r.a[reg];r.pc+=2;}
        else if((op&0xfff8)==0x40c0){NativeTiming::routine(NativeTiming::RCpuReadSr);r.d[op&7]=(r.d[op&7]&0xffff0000)|r.sr;r.pc+=2;}
        else if(op==0x007c || op==0x027c || op==0x0a7c){NativeTiming::routine(NativeTiming::RCpuLogicSr);unsigned operand=get16(rom+pc+2);setSr(op==0x007c?r.sr|operand:op==0x027c?r.sr&operand:r.sr^operand);r.pc+=4;}
        else return fail("unimplemented CPU-control form");
    }else return fail("unknown Line-A opcode");
    return true;
}
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
    if(NativeTiming::isActive()){
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
        if(NativeTiming::isActive() && NativeTiming::milestones[NativeTiming::ChecksumEnd].seen)
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
        if(NativeTiming::isActive() && kind==10)++NativeTiming::mainLoops;
        amigaInputObserve(pc,*board);
    }
    if(NativeTiming::isActive() && !diagnostic && pc==0x20be && kind==10){
        if(uninterruptedPoll && previousPollD1==r.d[1]+1){
            if(nativeClockRaw<nativePollMin)nativePollMin=nativeClockRaw;
            if(nativeClockRaw>nativePollMax)nativePollMax=nativeClockRaw;
            if(nativePollCount<256)nativePollSamples[nativePollCount]=nativeClockRaw;
            nativePollTotal+=nativeClockRaw;if(++nativePollCount==256)nativeClockSampleReady();
        }
        previousPollD1=r.d[1];uninterruptedPoll=true;
    }else uninterruptedPoll=false;
    if(kind==0)return fail("native CPU exception");
    if(pc>=ramEnd)return fail("native PC outside ROM/RAM");
#ifdef POKERI_LIVE_INSTRUCTION_COUNTS
    const bool countInstruction=true;
#else
    const bool countInstruction=diagnostic;
#endif
    if(countInstruction && kind!=11)++nativeInstructions;
    if(!diagnostic && kind>=32 && kind<48){NativeTiming::routine(NativeTiming::RGuestCharge);accountGuestCycles(34,1);}
    if(kind==10){
        if(!executeLineA(pc,countInstruction))return false;
    }else if(kind>=32 && kind<48){NativeTiming::routine(NativeTiming::RPushException);if(!pushException(kind,0))return false;}
    else if(kind!=9 && kind!=11)return fail("unknown native exception vector");
    // Snapshot after the executed instruction and before injecting another IRQ.
    // A newly injected tick has not run yet, so a completed prior update may
    // still be composed here; an in-progress callback must never be exposed.
    const bool tickReturned=!presentationTickFrame;
    enum {ShuffleWork=1,ComposeWork=2,PublishWork=4,StatusWork=8};
    unsigned work=(diagnostic?StatusWork:0)|(displayRequested?PublishWork:0);
    if(!diagnostic && shuffleQueue.active())work|=ShuffleWork;
    if((work&ShuffleWork) && !shuffleService())return false;
    unsigned pendingIrq=0;
    if(diagnostic){if(!replayBoundary())return false;}
    else {
        // A VBI pauses the guest clock before its callback increments frames.
        // Its return trace has no new guest interval to charge. Publish the
        // new wall deadline and spend already-earned credit anyway; otherwise
        // it waits for another accounting call (often the next VBI).
        if(!startupFast
           && (liveClock.frame!=pendingFrames || (liveClock.credit && liveClock.debt)))accountGuestCycles(0);
        unsigned nowFrames=pendingFrames,frames=nowFrames-seenFrames;seenFrames=nowFrames;
        if(frames){NativeTiming::routine(NativeTiming::RGuardCheck);if(!checkGuard(true))return false;}
        if(((r.sr>>8)&7)<5)liveIrqActive=false;
        // Deliver a pending source before advancing time again. An injected
        // handler must return before the next 100 Hz edge can replace its flag.
        NativeTiming::routine(NativeTiming::RBoardIrq);
        nativeCachedVideoStatus=board->video.statusNow();
        unsigned irq=dispatchIrqAtStatus();
        if(!(irq>((r.sr>>8)&7)) && liveTicks && !liveIrqActive){
            unsigned quanta=startupQuietBudget();if(quanta>liveTicks)quanta=liveTicks;
            liveTicks-=quanta;
            // Keep pressed edges latched until the game's next 50 Hz input
            // scan, even when native rendering makes one virtual frame slow.
            NativeTiming::routine(NativeTiming::RBoardTick);
            const bool accelerating=startupFast;
            const unsigned quantum=accelerating?wordProduct(uint16_t(quanta),8000):80000;
            if(!advanceClock(nativeCycles+quantum))return false;
            NativeTiming::routine(NativeTiming::RLiveInputs);
            if(!liveInputs())return false;
            if((!coldSetup || nativeSetupReady) && uint32_t(board->inputEdges)!=lastInputEdge){
                lastInputEdge=uint32_t(board->inputEdges);diagnosticKeys();amigaInputApply(*board);
            }
            // Cabinet protocol setup keeps its existing 10 ms observations.
            if(accelerating)startupCabinetTicks+=quanta;
            if(!accelerating || startupCabinetTicks==10){
                startupCabinetTicks=0;
            NativeTiming::routine(NativeTiming::RColdSetup);coldSetupStep();
            }
            NativeTiming::routine(NativeTiming::RBoardIrq);
            irq=dispatchIrqAtStatus();
        }
        if(board->resetRequested){
            if(++nativeLiveWatchdogResets==1){nativeFirstResetPc=canonical(r.pc);nativeFirstResetCycle=uint32_t(liveCycles-liveStart);}
            if(nativeLiveWatchdogResets>1)nativeUnexpectedReset();
            if(stopOnLiveReset)return fail("live watchdog expired");
            NativeTiming::routine(NativeTiming::RBoardReset);invalidatePeripheralIrq();board->reset();resetCpu();
            work|=StatusWork;
        }
        else if(irq>((r.sr>>8)&7)){
            ++nativeInterrupts;liveIrqActive=true;
            NativeTiming::routine(NativeTiming::RPushException);if(!pushException(board->vector(),irq))return false;
        }
        pendingIrq=irq;
    }
    // The original tick handler is the only ordinary refresh request source.
    // Wait for its callbacks, the original command ring, and a complete ACRTC
    // command/recognized card. VBI only publishes the prepared buffer.
    if(!diagnostic && displayRequested && compositionPending && tickReturned && !shuffleQueue.active())work|=ComposeWork;
    bool compose=(work&ComposeWork) &&
        !screen.presentationPending() && !board->video.receivingCommand() &&
        get32(board->memory.data()+0x41326)==get32(board->memory.data()+0x4132a);
    if(compose && nativeCardCache && nativeCardCache->sequenceIncoming())compose=false;
    // Fast-forward coalesces requests until Ready; no separate progress timer.
    if(startupFast)compose=false;
    if(displayRequested && !shuffleQueue.active() && compose){
        compositionPending=false;
        NativeTiming::Scope timing(NativeTiming::Present);
        screen.outputs(amigaInputLamps(),board->outputs());
        work|=StatusWork; // composition may materialize a deferred card prefix
        NativeTiming::routine(NativeTiming::RPresentation);if(!screen.present(board->video))return fail(screen.error);
    }
    if(work&PublishWork)screen.presentReady();
    if(NativeTiming::isActive()){
        uint32_t nextPc=canonical(r.pc);
        if(pc==0x10fcc && r.d[2]==1)NativeTiming::mark(NativeTiming::ChecksumEnd,nativeCycles,nextPc);
        if(NativeTiming::milestones[NativeTiming::ChecksumEnd].seen && pc==0x11040 && (r.sr&4))NativeTiming::mark(NativeTiming::DrainEnd,nativeCycles,pc);
    }
    if(work&StatusWork){NativeTiming::routine(NativeTiming::RVideoStatus);nativeCachedVideoStatus=board->video.statusNow();}
    if(nativeStatus==0xdead)return false;
    if(!diagnostic && liveStopCycles && liveCycles>=liveStopCycles){nativeLastPc=canonical(r.pc);nativeStatus=4;return false;}
    if(diagnostic && nativeCycles-lastGuardCycle>=160000){NativeTiming::routine(NativeTiming::RGuardCheck);if(!checkGuard())return false;}
    nativeShortPending=(liveTicks || pendingIrq)?1:0;
    bool traceService=liveTicks && !liveIrqActive;
    nativeServiceRequestPending=nativeServiceRedirectEnabled && traceService;
    if(nativeServiceRedirectEnabled)traceService=false;
    nativePhysicalResume=uint16_t(((diagnostic || traceService)?0x8000:0)|(r.sr&31));
    if(!diagnostic && (!nativeClockOverhead || (screen.active() && !clockDisplayCalibrated))){
        clockDisplayCalibrated=screen.active();NativeTiming::routine(NativeTiming::RClockCalibration);nativeClockCalibrateBegin();
    }
    return true;
}
CopperList *nativeCopper(){return displayRequested?screen.copper():nullptr;}
void nativeAudioStart(){if(liveRequested){if(!amigaInputStart()){fail("keyboard resource unavailable");return;}paula.start();}}
void nativeAudioStop(){if(liveRequested){paula.stop();amigaInputStop();}}
#ifdef POKERI_VBI_LATENCY
// Rows: startup/play, each without/with BLITHOG. Count, max line, line>=29.
uint32_t nativeVbiLatency[4][3]={};
#endif
void nativeVbi(bool quit){paula.vbi();screen.vbi();paula.refreshNoise();
    if(screen.swaps)NativeTiming::mark(NativeTiming::FirstSwap,nativeCycles,nativeLastPc);
#ifdef POKERI_VBI_LATENCY
    // Observe after audio and screen work; never postpone their service.
    unsigned line=(*(volatile uint32_t*)0xdff004>>8)&511;
    unsigned priority=(AmigaHardware::enabledDMAChannels()&0x400)?1:0;
    uint32_t *sample=nativeVbiLatency[(nativeSetupReady?2:0)+priority];
    ++sample[0];if(line>sample[1])sample[1]=line;if(line>=29)++sample[2];
#endif
    ++pendingFrames;
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
    if(nativeExtendedFrame && !pokeriWhdLoad)privateVectors=(uint32_t*)AllocMem(1024,MEMF_FAST); // optional optimization
    nativeStatus=0;DOSBase=(DosLibrary*)OpenLibrary("dos.library",0);if(!DOSBase)return fail("DOS unavailable");
    BPTR ratio=researchMarker("native-clock-ratio");
    if(ratio){uint8_t value[2];LONG n=Read(ratio,value,2);Close(ratio);
        if(n!=1 || value[0]<1 || value[0]>37)return fail("clock ratio must be one byte, 1..37 sixteenths");
        liveClock.ratioSixteenths=value[0];}
    BPTR window=researchMarker("native-clock-window");
    if(window){uint8_t value[2];LONG n=Read(window,value,2);Close(window);
        if(n!=1 || value[0]<1 || value[0]>3)return fail("clock window must be one byte, 1..3 PAL frames");
        playClockWindow=value[0];}
    BPTR measure=researchMarker("native-measure");if(measure)Close(measure);
#ifdef POKERI_NO_PROFILE_SUPPORT
    if(measure)return fail("native-measure requires PROFILE_SUPPORT=1 build");
#endif
    if(measure && !NativeTiming::prepare())return fail("measurement timer unavailable");
    BPTR resetTest=researchMarker("native-stop-on-watchdog");stopOnLiveReset=resetTest!=0;if(resetTest)Close(resetTest);
    BPTR test=researchMarker("native-test-inputs");testInputs=test!=0;if(test)Close(test);
    test=researchMarker("native-test-wrap");testWrap=test!=0;if(test)Close(test);
    BPTR replay=researchMarker("native-replay");
#ifndef POKERI_RELEASE
    diagnostic=replay!=0;
#endif
    nativeDiagnostic=diagnostic;if(replay)Close(replay);
    // WHDLoad cannot forward trace exceptions from a moved VBR. Diagnostic
    // replay traces every instruction; check before board allocation or
    // display takeover.
    nativeServiceRedirectEnabled=!diagnostic;
    if(diagnostic && pokeriWhdLoad && Supervisor((ULONG(*)())nativeProbeVbr)){
        nativeExitCode=21;
        return fail("native-replay requires NOVBRMOVE under WHDLoad");
    }
    BPTR playRatio=researchMarker("native-clock-play-ratio");
    if(playRatio){uint8_t value[2];LONG n=Read(playRatio,value,2);Close(playRatio);
        if(n!=1 || value[0]>64)return fail("play clock ratio must be one byte, 0..64 sixteenths (0 retains boot ratio)");
        playClockRatio=value[0];}
    BPTR tests=researchMarker("native-hardware-tests");
    nativeSkipHardwareTests=!diagnostic && !tests;if(tests)Close(tests);
    BPTR live=researchMarker("native-live");liveRequested=!diagnostic || live!=0;
    BPTR display=researchMarker("native-display");displayRequested=liveRequested || display!=0;if(display)Close(display);
    if(live){uint8_t limit[5];LONG n=Read(live,limit,5);Close(live);if(n!=0 && n!=4)return fail("native-live must be empty or a four-byte cycle budget");if(n==4)liveStopCycles=get32(limit);}
#ifdef POKERI_TRACE_CODE
    // FS-UAE's instruction trace records PCs only inside the first code hunk.
    boardAllocation=nativeTraceBoardStorage;guard=(uint8_t*)pokeriAllocateUninitialized(guardSize);
#else
    boardAllocation=(uint8_t*)pokeriAllocateUninitialized(sizeof(Board)+255);guard=(uint8_t*)pokeriAllocateUninitialized(guardSize);
#endif
    if(!boardAllocation || !guard)return fail("native allocations failed");
    board=new((void*)((uint32_t(boardAllocation)+255)&~255u)) Board();
    videoDevice=&board->video;nativeVideoSelector=videoDevice->addressSelector();
    rom=board->memory.data();romBase=uint32_t(rom);ramBase=uint32_t(rom+0x40000);guardBase=uint32_t(guard);
    nativeRomBegin=romBase;nativeRomEnd=romBase+0x40000;nativeRamBegin=ramBase;nativeRamEnd=ramBase+(ramEnd-0x40000);
    static const char *names[]={"77POK30","77POK38","77POK34","PARA200J"};
    // WHDLoad's current drawer is data/; trying other names costs OS switches.
    static const char *standalonePrefixes[]={"data/","","rom/"},*whdLoadPrefixes[]={""};
    const char *const *prefixes=pokeriWhdLoad?whdLoadPrefixes:standalonePrefixes;
    const unsigned prefixCount=pokeriWhdLoad?1:3;
    for(unsigned chip=0;chip<4;++chip){
        bool found=false;
        for(unsigned p=0;p<prefixCount;++p){const char *prefix=prefixes[p];
            char path[32];unsigned n=0;
            while(*prefix)path[n++]=*prefix++;
            for(const char *name=names[chip];*name;)path[n++]=*name++;
            path[n]=0;
            BPTR f=Open(path,MODE_OLDFILE);
            if(!f){if(IoErr()!=ERROR_OBJECT_NOT_FOUND && IoErr()!=ERROR_DIR_NOT_FOUND)return fail("cannot open ROM file");continue;}
            // One Read: a 65,537th byte proves an oversized file. It lands in the
            // next chip, loaded next, or the first RAM byte, cleared below.
            LONG got=Read(f,rom+(chip<<16),65537);Close(f);
            if(got!=65536)return fail("ROM must be exactly 65536 bytes");
            found=true;break;
        }
        if(!found)return fail("ROM missing: install four chips in data/ or current drawer");
    }
    board->memory[0x40000]=0;
    for(const auto &patch:handlerTailWords)if(get16(rom+patch.offset)!=patch.value)return fail("handler tail ROM shape mismatch");
    for(const auto &patch:patchWords)if(get16(rom+patch.offset)!=patch.value)return fail("ROM patch-site mismatch");
    // Audited low-vector sentinel reads need the unrelocated vectors only.
    for(unsigned i=0;i<sizeof(originalVectors);++i)originalVectors[i]=rom[i];
    for(unsigned i=0;i<guardSize;++i)guard[i]=0xa5;
    for(unsigned i=0;i<Board::ramCanary;++i)board->memory[ramEnd+i]=0xa5;
    BPTR guardTest=researchMarker("native-test-guard");
    if(guardTest){Close(guardTest);if(!testGuard())return fail("guard self-test failed");}
    for(const auto &f:fixups){
        uint32_t v=get32(rom+f.offset);
        // Kind 2 is a RAM-relative addend, not an address. Every other target
        // must lie inside its allocated window.
        if(f.kind==0?v>=0x40000:f.kind==1?(v<0x40000 || v>=ramEnd):f.kind==3?(v<deviceBegin || v>=0x100000):false)return fail("relocated address outside native window");
        v+=f.kind==0?romBase:f.kind==3?guardBase-deviceBegin:ramBase-0x40000;put32(rom+f.offset,v);
    }
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
        if(h.operation==Operation::move && meta.last==meta.first+1){
            const auto &e=accesses[meta.first];
            const Operand &port=e.write?h.dest:h.source,&value=e.write?h.source:h.dest;
            bool immediate=e.write && value.kind==Ea::immediate;
            if(port.kind==Ea::absolute_long && e.address>=0xf6000 && e.address+e.size<=0xf6004 &&
               e.size==h.size && (h.size==1 || (e.write && h.size==2)) &&
               (immediate || (value.kind==Ea::data && value.reg>=0 && value.reg<=7))){
                if(h.length!=(immediate?8:6))return fail("short absolute video length mismatch");
                unsigned kind=(e.write?8:0)|(h.size==2?16:0)|(immediate?32:unsigned(value.reg));
                nativeShortStatus[i]=shortDescriptor(romBase+h.pc,preparedAccesses[meta.first].physical,uint16_t(0x0400|kind),meta.cycles);
                continue;
            }
        }
        if(h.operation==Operation::move && h.size==1 && meta.last==meta.first+1){
            const auto &e=accesses[meta.first];
            bool peripheral=(e.address>=0xfb014 && e.address<0xfb020) ||
                e.address==0xfb002 || e.address==0xfb003 || e.address==0xfb006 || e.address==0xfb007 || e.address==0xfb00a || e.address==0xfb00b;
            const Operand &port=e.write?h.dest:h.source,&value=e.write?h.source:h.dest;
            bool indirect=port.kind==Ea::indirect,immediate=e.write && value.kind==Ea::immediate;
            bool serial=port.reg==1,post=serial && e.write && value.kind==Ea::postincrement && value.reg==2 && !indirect;
            if(peripheral && e.size==1 && (port.reg==3 || serial) && (indirect || port.kind==Ea::displacement) &&
                ((value.kind==Ea::data && value.reg>=0 && value.reg<=(e.write?7:2)) || (immediate && !indirect) || post)){
                unsigned kind=(e.write?8:0)|(immediate?0x20:post?0x100:unsigned(value.reg))|(indirect?0x40:0)|(serial?0x80:0);
                if(h.length!=(immediate?6:indirect?2:4))return fail("short peripheral length mismatch");
                nativeShortStatus[i]=shortDescriptor(romBase+h.pc,preparedAccesses[meta.first].physical,uint16_t(0x1000|kind),meta.cycles);continue;
            }
        }
        if(h.operation==Operation::bit_test && h.size==1 && h.length==4 &&
           h.source.kind==Ea::immediate && h.dest.kind==Ea::indirect && h.dest.reg==1 && meta.last==meta.first+1){
            const auto &e=accesses[meta.first];
            if(!e.write && e.size==1 && (e.address==0xfb002 || e.address==0xfb006 || e.address==0xfb00a)){
                unsigned kind=0x2c0|(preparedHooks[i].sourceExtension&7);
                nativeShortStatus[i]=shortDescriptor(romBase+h.pc,preparedAccesses[meta.first].physical,uint16_t(0x1000|kind),meta.cycles);
                continue;
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
                if(kind==0 && e.address==0xf6000)
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
                   d->address!=relocated(n==1?0xf6002:0xf6000))
                    return fail("FIFO control fusion shape mismatch");
            }
            sequence[0]->reserved=uint32_t(sequence[1]);
            sequence[1]->reserved=uint32_t(sequence[2]);
            sequence[0]->body=uint32_t(nativeShortFifoControl);
        }
    }
    if(!diagnostic){
        const unsigned pcs[]={0xd5a,0xd64,0xd68,0xd6c,0xd78,0xd7c};
        const unsigned regs[]={0,1,3,2,1,3},ports[]={0x14,0x16,0x16,0x14,0x16,0x16};
        ShortStatus *sequence[6]={};
        for(unsigned n=0;n<6;++n){
            for(auto &d:nativeShortStatus)if(d.pc==romBase+pcs[n])sequence[n]=&d;
            auto *d=sequence[n];
            // Authored MOVE.B Dn,d16(A3) encoding and shared decoded endpoint.
            if(!d || get16(rom+pcs[n])!=(0x1740|regs[n]) || get16(rom+pcs[n]+2)!=ports[n] ||
               d->mask!=(0x1008|regs[n]) || d->length!=4 || d->cycles!=12 ||
               d->address!=relocated(0xfb000)+ports[n])return fail("sound fusion write shape mismatch");
        }
        // MOVE.L D1,D3; ANDI.B #$FD,D1 twice; ORI.B #$80,D1.
        if(get16(rom+0xd5e)!=0x2601 || get16(rom+0xd60)!=0x0201 || get16(rom+0xd62)!=0x00fd ||
           get16(rom+0xd70)!=0x0201 || get16(rom+0xd72)!=0x00fd ||
           get16(rom+0xd74)!=0x0001 || get16(rom+0xd76)!=0x0080)
            return fail("sound fusion arithmetic shape mismatch");
        for(unsigned n=0;n<5;++n)sequence[n]->reserved=uint32_t(sequence[n+1]);
        sequence[0]->body=uint32_t(nativeShortSoundWrite);
    }
    {
        ShortStatus *status=nullptr,*write=nullptr;
        for(auto &d:nativeShortStatus){if(d.pc==romBase+0x2e58)status=&d;if(d.pc==romBase+0x2e5e)write=&d;}
        unsigned branch=get16(rom+0x2e5c);
        if(!status || !write || status->mask!=2 || write->mask!=0x0807 ||
           status->length!=4 || write->length!=4 || write->address!=status->address+2 ||
           (branch&0xff00)!=0x6700 || romBase+0x2e5e + int8_t(branch)!=romBase+0x2e7e)
            return fail("feed fusion shape mismatch");
        status->reserved=uint32_t(write);status->body=uint32_t(nativeShortFeedRead);
        nativeFeedTarget=romBase+0x2e7e;
        write->body=uint32_t(nativeShortFeedLoopWrite);write->reserved=uint32_t(status);
    }
    put16(rom+0x10ae,0x6000);put16(rom+0x10b0,0x30);put16(rom+0x110c,0x6000);put16(rom+0x110e,0x2c);
    for(unsigned i=0;i<sizeof(hooks)/sizeof(*hooks);++i)put16(rom+hooks[i].pc,0xa000|i);
    for(auto pc:resets)put16(rom+pc,0xaffd);
    // Explicit clock experiments retain their historical startup contract.
    startupFast=!diagnostic && nativeSkipHardwareTests && !ratio && !playRatio && !window;
    if(startupFast && (get16(rom+0x2442)!=0x5346 || get16(rom+0x2444)!=0x66fc))
        return fail("startup delay SUBQ/BNE shape mismatch");
    startupDelayOpcode=get16(rom+0x2442);
    paula.muted=startupFast;
    if(startupFast)put16(rom+0x2442,0xaffc);
    if(!diagnostic)put16(rom+ShuffleWait::pc,0xaffb);
    for(unsigned i=0;i<sizeof(controls)/sizeof(*controls);++i){
        unsigned pc=controls[i],index=sizeof(hooks)/sizeof(*hooks)+i;
        uint16_t op=originalControl[i]=get16(rom+pc);controlCycles[i]=hookCycles(pc);
        unsigned kind=op==0x007c?0:op==0x027c?1:op==0x4e73?2:3;
        if(kind<3)nativeShortStatus[index]=shortDescriptor(romBase+pc,kind<2?get16(rom+pc+2):0,uint16_t(0x4000|kind),controlCycles[i]);
        if(!diagnostic && pc==0x0c3e){
            if(op!=0x4e73 || controlCycles[i]!=20)return fail("tick RTE shape mismatch");
            nativeShortStatus[index].body=uint32_t(nativeShortTickRteRead);
        }
        put16(rom+pc,0xa000|index);
    }
    if(!diagnostic){
        ShortStatus *status=nullptr,*address=nullptr;
        for(auto &d:nativeShortStatus){
            if(d.pc==romBase+0x2e30)status=&d;
            if(d.pc==romBase+0x2e36)address=&d;
        }
        if(!status || !address || status->mask!=0x0080 || status->cycles!=12 ||
           status->guard!=uint32_t(nativeShortStatusGuard) || status->length!=4 ||
           status->address!=relocated(0xf6000) || get16(rom+0x2e32)!=7 || get16(rom+0x2e34)!=0x6656 ||
           address->mask!=0x0800 || address->cycles!=12 || address->length!=4 ||
           address->address!=status->address || get16(rom+0x2e38)!=0 ||
           address->body!=uint32_t(nativeShortAddressWrite))
            return fail("handler entry fusion shape mismatch");
        status->reserved=uint32_t(address);status->body=uint32_t(nativeShortHandlerEntry);
    }
    // Live execution uses the verified joined video-handler path.
    if(!diagnostic){
        ShortStatus *entry=nullptr,*select=nullptr,*feed=nullptr,*empty=nullptr;
        for(auto &d:nativeShortStatus){
            if(d.pc==romBase+0x2e30)entry=&d;
            if(d.pc==romBase+0x2e36)select=&d;
            if(d.pc==romBase+0x2e58)feed=&d;
            if(d.pc==romBase+0x2e70)empty=&d;
        }
        // MOVEM.L D0-D1/A0-A1,-(SP), MOVEA.L #device,A0 and the nine queue
        // instructions at $2E3A-$2E56: encodings are checked, never copied.
        static const uint16_t queue[]={0x222e,0x8826,0x226e,0x88c2,0x2009,0x226e,0x882a,
            0xb3c1,0x6724,0xb3c0,0x6604,0x226e,0x88be,0xb3c1,0x6726};
        bool shape=entry && select && feed && empty &&
            entry->body==uint32_t(nativeShortHandlerEntry) && entry->guard==uint32_t(nativeShortStatusGuard) &&
            select->body==uint32_t(nativeShortAddressWrite) && select->length==4 && select->cycles==12 &&
            feed->body==uint32_t(nativeShortFeedRead) && feed->guard==uint32_t(nativeShortStatusGuard) &&
            empty->body==uint32_t(nativeShortFifoControl) && empty->guard==uint32_t(nativeShortVideoGuard) &&
            entry->address==relocated(0xf6000) && select->address==entry->address &&
            feed->address==entry->address && empty->address==entry->address &&
            get16(rom+0x2e26)==0x48e7 && get16(rom+0x2e28)==0xc0c0 && get16(rom+0x2e2a)==0x207c &&
            get32(rom+0x2e2c)==relocated(0xf6000) && get32(rom+0x100)==romBase+0x2e26 &&
            nativeFeedTarget==romBase+0x2e7e;
        for(unsigned i=0;shape && i<sizeof(queue)/sizeof(*queue);++i)shape=get16(rom+0x2e3a+2*i)==queue[i];
        if(!shape)return fail("joined handler shape mismatch");
        nativeHandlerFeed=uint32_t(feed);nativeHandlerEmpty=uint32_t(empty);
        select->body=uint32_t(nativeShortHandlerJoinedSetup);
        nativeJoinedEntry=uint32_t(entry);nativeJoinedA0=relocated(0xf6000);
        nativeJoinedVector=romBase+0x2e26;
    }
    if(!diagnostic){
        ShortStatus *address=nullptr,*rte=nullptr;
        for(auto &d:nativeShortStatus){
            if(d.pc==romBase+0x2e82)address=&d;
            if(d.pc==romBase+0x2e8a)rte=&d;
        }
        // The address MOVE and RTE opcodes were already verified before patching.
        // Verify their operands/descriptors and the intervening MOVEM exactly.
        if(!address || !rte || address->mask!=0x0800 || address->length!=4 ||
           address->cycles!=12 || address->address!=relocated(0xf6000) ||
           get16(rom+0x2e84)!=3 || get16(rom+0x2e86)!=0x4cdf || get16(rom+0x2e88)!=0x0303 ||
           rte->mask!=0x4002 || rte->cycles!=20 ||
           rte->guard!=uint32_t(nativeShortControlGuard) || rte->body!=uint32_t(nativeShortControlRead))
            return fail("handler exit fusion shape mismatch");
        address->reserved=uint32_t(rte);address->body=uint32_t(nativeShortHandlerExit);
        nativeHandlerTailPc=romBase+0x2e7e;nativeHandlerTailExit=uint32_t(address);
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
    // The opcode indexes the same exact-PC descriptor table as original hooks.
    // No common Line-A branch is added. Diagnostic and generic paths retain T.
    const unsigned serviceIndex=nativeShortCount-1;
    if(serviceIndex>=0xffb)return fail("service descriptor collides with reserved opcode");
    nativeServiceOpcode=0xa000|serviceIndex;
    nativeServiceRedirectState={uint32_t(&nativeServiceOpcode),0,0};
    nativeShortStatus[serviceIndex]={uint32_t(&nativeServiceOpcode),0,0,0,0,
        uint32_t(nativeServiceDescriptor),0,0,0,0};
    CacheClearU(); // Publish relocated/patched instructions to 68020+ caches.
    board->config.cpuHz=settings[0];board->config.systemHz=settings[1];board->config.inputHz=settings[2];board->config.watchdogMs=settings[3];board->config.watchdogResetUs=settings[4];board->ay.clockHz=settings[5];board->peer.enabled=settings[8];
if(liveRequested){if(!paula.prepare())return fail("Paula allocation failed");board->ay.backend=&paula;}
    if(!videoSurface.prepare())return fail("video bitplane allocation failed");
    board->video.surface=&videoSurface;
    {
        uint32_t started=measure?NativeTiming::benchmarkClock():0;
        nativeCardStorage=(uint16_t*)AllocMem(CardBackCache::BitmapWords*4,MEMF_CHIP);
        nativeCardCache=new CardBackCache;
        if(nativeCardCache && nativeCardStorage){
            const CardBackCache::Recipe recipe={card_recipe::words,card_recipe::offsets,card_recipe::context};
            bool prepared=nativeCardCache->installPrepared(recipe,card_prepared::data,nativeCardStorage,
                nativeCardStorage+CardBackCache::BitmapWords);
            if(prepared){nativeCardCache->attach(board->video,true);
#ifdef POKERI_TIME_LEDGER
                nativeCardCache->timing=[](unsigned kind,unsigned detail){
                    if(kind!=0 || !nativeCardObserver.matched)NativeTiming::event(3+kind,detail,videoSurface.cardBlits,nativeCycles);
                };
#endif
            }else if(nativeCardCache->error)return fail(nativeCardCache->error);
        }
        if(measure)nativeCardPrepareTicks=NativeTiming::benchmarkClock()-started;
    }
#ifdef POKERI_TIME_LEDGER
    // Completion attributes the enclosing Command scope to the opcode group.
    board->video.commandLog=[](const uint16_t *words,unsigned count,bool executed){
#ifdef POKERI_TIME_LEDGER
        NativeTiming::commandGroup=words[0]>>10;
        for(unsigned i=0;i<8;++i)NativeTiming::commandWords[i]=i<count?words[i]:0;
#endif
#ifdef POKERI_TIME_LEDGER
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
    if(displayRequested && !screen.prepare(videoSurface,board->memory.data()))return fail("screen allocation failed");
    if(diagnostic && !advanceEvent())return false;
    if(!diagnostic){
        // Missing or invalid save files stop before takeover; none is created.
        const char *error=loadAccounting(board->memory.data(),startup.retained);if(error){if(pokeriWhdLoad)nativeExitCode=22;return fail(error);}
        coldSetup=true;board->pia[1].input[0]=0xff;board->pia[1].input[1]=0x7f;board->pia[2].input[0]=8;}
    if(liveRequested){const char *error=loadNvram(board->nvram);if(error){if(pokeriWhdLoad)nativeExitCode=22;return fail(error);}}
    if(liveRequested && !nativeGuestTimerPrepare())return fail("CIA-A timer A unavailable for guest clock");
    if(diagnostic)nativeClockEnabled=0;
    nativeCachedVideoStatus=board->video.statusNow();
    resetCpu();if(diagnostic?!replayBoundary():!liveInputs())return false;nativePhysicalResume=diagnostic?0x8000:0;nativeStatus=1;return true;
}
extern "C" void nativeInstallVectors(){
    void(*traps[])()={nativeTrap0,nativeTrap1,nativeTrap2,nativeTrap3,nativeTrap4,nativeTrap5,nativeTrap6,nativeTrap7,nativeTrap8,nativeTrap9,nativeTrap10,nativeTrap11,nativeTrap12,nativeTrap13,nativeTrap14,nativeTrap15};
    // WHDLoad owns VBR; the installed NoVBRMove option admits our trace handler.
    originalVbr=pokeriWhdLoad?0:nativeReadVbr();
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
    NativeTiming::begin();
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
    if(nativeCardCache){nativeCardCache->detach();videoSurface.synchronize();delete nativeCardCache;nativeCardCache=nullptr;}
    if(nativeCardStorage){FreeMem(nativeCardStorage,CardBackCache::BitmapWords*4);nativeCardStorage=nullptr;}
    screen.release();videoSurface.release();paula.release();if(DOSBase && nativeError){PutStr(nativeError);PutStr("\n");}delete reader;delete[] replayData;delete[] guard;if(board)board->~Board();
#ifdef POKERI_TRACE_CODE
    if(boardAllocation!=nativeTraceBoardStorage)
#endif
    delete[] boardAllocation;reader=nullptr;replayData=guard=boardAllocation=nullptr;board=nullptr;if(DOSBase)CloseLibrary((Library*)DOSBase);DOSBase=nullptr;}
