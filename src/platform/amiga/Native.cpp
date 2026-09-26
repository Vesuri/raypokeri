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
#include "native/BootPolicy.h"
#include "Startup.h"
#include "native/Replay.h"
#include <stddef.h>
inline void *operator new(size_t,void *address) noexcept {return address;}
#include "../../../amiga/generated/NativeTables.h"
using namespace pokeri;
struct DosLibrary *DOSBase=nullptr;
extern "C" {
void *pokeriAllocateUninitialized(unsigned long);
Registers nativeRegisters;
uint8_t nativeServiceStack[32768];
uint32_t nativeReturnStack,nativeOsUsp,nativePrepareStack,nativeOldLevel3,nativeOldLevel6,nativeOldLevel2;
void nativeLevel3();void nativeLevel6();void nativeLevel2();
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
static uint8_t *boardAllocation,*rom,*guard,*replayData;
static PreparedHook preparedHooks[sizeof(hooks)/sizeof(*hooks)];
static bool genericHooks=false;
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
uint32_t nativeShortGuest=0,nativeShortNominal=0,nativeShortCalls=0,nativeShortCharge[256]={};
}
struct PreparedAccess {uint32_t physical;};
static PreparedAccess preparedAccesses[sizeof(accesses)/sizeof(*accesses)];
static uint8_t originalVectors[12];
static uint32_t romBase,ramBase,guardBase,replaySize,virtualUsp,virtualSsp,lastGuardCycle,liveStopCycles,guardCursor;
static uint32_t liveTicks=0;
// A reserved CIA timer counts only the intervals outside native services.
bool nativeGuestTimerPrepare();void nativeGuestTimerRelease();
extern "C" volatile uint8_t *nativeGuestTimerControl,*nativeGuestTimerLow,*nativeGuestTimerHigh;
extern "C" volatile uint16_t nativeClockEnabled;
extern "C" volatile uint16_t nativeClockRunning=0;
extern "C" uint32_t nativeClockResumePc=0;
static uint32_t guestClockPhase=0;
extern "C" volatile uint32_t pendingFrames=0;
// 0 retains the old scale/contract; 1 corrects units only; 2 enables option C.
extern "C" uint16_t nativeClockMode=2;
static LiveClock liveClock;
static bool clockDisplayCalibrated=false;
extern "C" volatile uint32_t nativeClockRaw=0,nativePollMin=0xffffffffu,nativePollMax=0,nativePollCount=0,nativePollTotal=0;
extern "C" __attribute__((noinline)) void nativeClockSampleReady(){asm volatile("" ::: "memory");}
extern "C" uint16_t nativePollSamples[256],nativeCalibrationSamples[64];
uint16_t nativePollSamples[256],nativeCalibrationSamples[64];
static uint32_t previousPollD1=0;static bool uninterruptedPoll=false;
extern "C" uint64_t nativeClockCharged[3]={},nativeClockObserved=0;
static void accountGuestCycles(uint32_t cycles,unsigned source=0){
    if(NativeTiming::active)nativeClockCharged[source]+=cycles;
    if(nativeClockMode==2)cycles=liveClock.grant(cycles,source!=0,pendingFrames,liveTicks>=2?160000:guestClockPhase+(liveTicks?80000:0));
    guestClockPhase+=cycles;
    while(guestClockPhase>=80000){guestClockPhase-=80000;++liveTicks;}
}
static bool liveIrqActive=false;
static uint64_t liveCycles=0;
static PaulaAy paula;
static AmigaScreen screen;
static AmigaSurface videoSurface;
static bool liveRequested=false,displayRequested=false;
extern "C" uint16_t nativeBenchmarkRequested=0;
extern "C" uint32_t nativeBenchTicks[6]={},nativeBenchShortTicks[2]={};
extern "C" void nativeShortBenchmarkLoop(),nativeShortBenchmarkControl(),nativeShortBenchmarkOpcode();
extern "C" volatile uint32_t nativeBenchSink=0;
static uint32_t lastPresentCycle=0;
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
    if(!diagnostic && nativeShortCalls){
        uint32_t guest=nativeShortGuest,nominal=nativeShortNominal;nativeShortGuest=nativeShortNominal=0;
        if(guest)accountGuestCycles(guest);
        if(nominal)accountGuestCycles(nominal,1);
    }
    if(!diagnostic && nativeClockRunning){
        if(NativeTiming::active)nativeClockObserved+=nativeClockRaw;
        accountGuestCycles(nativeClockRaw>nativeClockOverhead?nativeClockRaw-nativeClockOverhead:0);
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
        // requested ratio. These calibrate CPU throughput, not device services.
        const uint32_t reference[]={8192*14-2,8192*22-2,8192*28-2};
        for(unsigned i=0;i<3;++i){
            uint32_t limit=(reference[i]<<3)+(reference[i]<<2)+(reference[i]<<1),cost=nativeSpeedCycles[i];
            unsigned ratio=1;while(ratio<liveClock.ratioSixteenths && cost+nativeSpeedCycles[i]<=limit){cost+=nativeSpeedCycles[i];++ratio;}
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
static bool advanceClock(uint32_t target){NativeTiming::Scope timing(NativeTiming::BoardTick);if(diagnostic && target<nativeCycles)return fail("replay clock reversed");uint32_t delta=target-nativeCycles;board->tick(delta);nativeCachedVideoStatus=board->video.statusNow();if(!diagnostic)liveCycles+=delta;nativeCycles=target;return !board->fault || fail(board->faultReason);}
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
static void setSr(uint16_t value){value&=0xa71f;if(value&0x8000)fail("uncovered guest trace mode");Registers&r=nativeRegisters;if((r.sr^value)&0x2000){if(r.sr&0x2000){virtualSsp=r.a[7];r.a[7]=virtualUsp;}else{virtualUsp=r.a[7];r.a[7]=virtualSsp;}}r.sr=value;}
static bool pushException(unsigned vector,unsigned level){
    Registers&r=nativeRegisters;uint16_t sr=r.sr;setSr(uint16_t((sr|0x2000)&~0x8000));
    if(level)r.sr=uint16_t((r.sr&~0x700)|(level<<8));
    uint32_t sp=canonical(r.a[7]-6);if(sp<0x40000 || sp>=0x7fffa)return fail("virtual exception stack outside RAM");
    r.a[7]-=6;put16(board->memory.data()+sp,sr);put32(board->memory.data()+sp+2,r.pc);r.pc=get32(rom+vector*4);return true;
}
static void resetCpu(){liveIrqActive=false;setSr(0x2700);nativeRegisters.a[7]=get32(rom);nativeRegisters.pc=get32(rom+4);virtualSsp=nativeRegisters.a[7];}
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
                if(((local+i)&~1u)==0xf6002 && board->video.ar>=2)screen.invalidate();
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
                unsigned offset=e.address-0xf6000;
                if(!writing)value=0;
                for(unsigned byte=0;byte<size;++byte){
                    if(writing){
                        if((offset+byte)>=2 && board->video.ar>=2)screen.invalidate();
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
    startup.step(*board,[](unsigned pia,unsigned side,unsigned value){ReplayEvent e{};e.a=(pia<<16)|side;e.b=value;applyInput(e);});
    if(startup.error){fail(startup.error);return;}
    if(startup.stage==pokeri::Startup::Ready){nativeSetupReady=1;liveStart=uint32_t(liveCycles);NativeTiming::mark(NativeTiming::PlayReady,nativeCycles,nativeLastPc);nativePlayReady();}
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
        const Key &key=keys[testInputIndex++];amigaInputKey(key.code,key.down);
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
            nativeBootVerified=1;nativeBootReady();
            if(!liveRequested){nativeStatus=2;return false;}
            NativeTiming::begin();
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
    board->writePia(2,2,uint8_t(value));
    if(kind==2){
        if(coldSetup && !nativeSetupReady)startup.observe(0x2472);
        else amigaInputObserve(0x2472,*board);
    }
    nativeShortPending=(nativeShortPending&1)|((pendingFrames!=seenFrames || quitRequested)?2:0);
    return uint8_t(value);
}
extern "C" unsigned nativeShortPiaReadValue(){
    unsigned value=board->readPia(2,0);
    nativeShortPending=(nativeShortPending&1)|((pendingFrames!=seenFrames || quitRequested)?2:0);return value;
}
static void shortIoCompleted(){
    unsigned irq=board->irq();
    nativeShortPending=(liveTicks || irq?1:0)|((pendingFrames!=seenFrames || quitRequested || irq>((nativeRegisters.sr>>8)&7))?2:0);
    if(board->fault){fail(board->faultReason);nativeShortPending|=2;}
}
extern "C" unsigned nativeShortIoReadValue(uint32_t address){
    unsigned value=board->read8(address-guardBase+0x80000);shortIoCompleted();return value;
}
extern "C" unsigned nativeShortIoWriteValue(uint32_t address,unsigned value){
    board->write8(address-guardBase+0x80000,uint8_t(value));shortIoCompleted();return uint8_t(value);
}
// Exactly the same byte-ordered endpoint operations as PreparedBus. Keep the
// model authoritative, including command completion, FIFO and IRQ side effects.
extern "C" unsigned nativeShortVideoWriteValue(uint32_t address,unsigned value,unsigned kind){
    unsigned offset=address-guardBase+0x80000-0xf6000;
    if(offset>=2 && board->video.ar>=2)screen.invalidate();
    if(kind&2){
        board->video.write8(offset,value>>8);
        if(offset+1>=2 && board->video.ar>=2)screen.invalidate();
        board->video.write8(offset+1,value);
    }else board->video.write8(offset,value);
    if(board->video.error){board->fault=true;board->faultReason=board->video.error;}
    nativeCachedVideoStatus=board->video.statusNow();
    shortIoCompleted();return value;
}
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
extern "C" unsigned nativeDispatch(unsigned kind){
    NativeTiming::dispatch(kind);
    uint32_t timingPc=canonical(nativeRegisters.pc);
    if(NativeTiming::active){
        if(timingPc==0x20be)NativeTiming::mark(NativeTiming::RamTestEnd,nativeCycles,timingPc);
        if(timingPc==0x10fc0)NativeTiming::mark(NativeTiming::ChecksumStart,nativeCycles,timingPc);
    }
    unsigned loopCycles=timingPc==0x20be?34:timingPc==0x2118?10:timingPc==0x214a?26:0;
    uint32_t counter=timingPc==0x20be?nativeRegisters.d[1]:nativeRegisters.d[2];
    if(!diagnostic && nativeClockRunning && kind==10 && loopCycles &&
       previousTimingPc==timingPc && previousTimingCounter==counter+1){
        NativeTiming::routine(NativeTiming::RGuestCharge);accountGuestCycles(loopCycles,2);nativeClockRunning=0;
    }
    previousTimingPc=kind==10?timingPc:0xffffffffu;previousTimingCounter=counter;
    // Resuming directly at another patched instruction executes no original
    // instruction before its exception. Timer quantization is not guest work.
    if(kind==10 && nativeClockResumePc==nativeRegisters.pc)nativeClockRunning=0;
    NativeTiming::routine(NativeTiming::RClockPause);nativeClockPause();
    if(nativeShortDrained){
        if(NativeTiming::active && NativeTiming::milestones[NativeTiming::ChecksumEnd].seen)
            NativeTiming::mark(NativeTiming::DrainEnd,nativeCycles,timingPc);
        nativeShortDrained=0;
    }
    ServiceInterrupts serviceInterrupts;
    NativeTiming::Scope timing(NativeTiming::Service,63);
    if(quitRequested){nativeStatus=3;return false;}
    Registers&r=nativeRegisters;r.sr=uint16_t((r.sr&~31)|(nativePhysicalSr&31));uint32_t pc=timingPc;nativeLastPc=pc;
    if(coldSetup && !nativeSetupReady)startup.observe(pc);
    else if(pc==0x2472 || pc==0x246a)amigaInputObserve(pc,*board);
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
    if(kind!=11)++nativeInstructions;
    if(!diagnostic && kind>=32 && kind<48){NativeTiming::routine(NativeTiming::RGuestCharge);accountGuestCycles(34,1);}
    if(kind==10){
        unsigned index=get16(rom+pc)&0xfff;
        NativeTiming::hook(index);
        if(!diagnostic){NativeTiming::routine(NativeTiming::RGuestCharge);accountGuestCycles(index<sizeof(hooks)/sizeof(*hooks)?hookMetadata[index].cycles:index<nativeShortCount?controlCycles[index-sizeof(hooks)/sizeof(*hooks)]:hookCycles(pc),1);}
        if(index<sizeof(hooks)/sizeof(*hooks)){
            const pokeri::Hook &h=hooks[index];if(h.pc!=pc)return fail("Line-A index/site mismatch");
            bool device=hardwareHooks[index];
            if(diagnostic && device){if(!haveEvent || nextEvent.kind!=ReplayBus || nextEvent.instruction!=nativeInstructions || nextEvent.pc!=pc)return fail("replay I/O boundary mismatch");if(!advanceClock(nextEvent.cycle) || !advanceEvent())return false;}
            bool okay;
            if(genericHooks){Bus bus;bus.pc=pc;bus.firstAccess=hookMetadata[index].first;bus.lastAccess=hookMetadata[index].last;NativeTiming::routine(NativeTiming::RGenericHook);okay=executeHook(h,r,bus);}
            else {PreparedBus bus{hookMetadata[index],pc};NativeTiming::routine(NativeTiming::RPreparedHook);okay=executePreparedHook(preparedHooks[index],r,bus);}
            if(!okay)return fail("unsupported native hook");
        }else if(index==0xffe){if(diagnostic && !videoSurface.tested && !videoSurface.selfTest())return fail("planar blitter self-test failed");r.d[7]=ramBase-0x40000;r.a[6]=0x40b00;r.pc+=6;}
        else if(index==0xffd){
            if(!(r.sr&0x2000))return fail("virtual privilege violation at RESET");
            bool found=false;for(auto offset:resets)if(pc==offset)found=true;if(!found)return fail("unknown RESET hook");
            if(diagnostic){if(!haveEvent || nextEvent.kind!=ReplayPeripheralReset || nextEvent.instruction!=nativeInstructions || nextEvent.pc!=pc)return fail("replay RESET mismatch");if(!advanceClock(nextEvent.cycle)||!advanceEvent())return false;}NativeTiming::routine(NativeTiming::RBoardReset);board->reset();r.pc+=2;
        }else if(index<nativeShortCount){
            unsigned i=index-sizeof(hooks)/sizeof(*hooks);if(controls[i]!=pc)return fail("CPU-control index/site mismatch");uint16_t op=originalControl[i];
            if((op&0xfff8)!=0x40c0 && !(r.sr&0x2000))return fail("virtual privilege violation at CPU-control hook");
            if(op==0x4e73){NativeTiming::routine(NativeTiming::RCpuRte);uint32_t sp=canonical(r.a[7]);if(sp<0x40000 || sp>=0x7fffa)return fail("RTE stack outside RAM");uint16_t sr=get16(board->memory.data()+sp);r.pc=get32(board->memory.data()+sp+2);r.a[7]+=6;setSr(sr);}
            else if((op&0xfff0)==0x4e60){NativeTiming::routine(NativeTiming::RCpuUsp);unsigned reg=op&7;if(op&8)r.a[reg]=virtualUsp;else virtualUsp=r.a[reg];r.pc+=2;}
            else if((op&0xfff8)==0x40c0){NativeTiming::routine(NativeTiming::RCpuReadSr);r.d[op&7]=(r.d[op&7]&0xffff0000)|r.sr;r.pc+=2;}
            else if(op==0x007c || op==0x027c || op==0x0a7c){NativeTiming::routine(NativeTiming::RCpuLogicSr);unsigned operand=get16(rom+pc+2);setSr(op==0x007c?r.sr|operand:op==0x027c?r.sr&operand:r.sr^operand);r.pc+=4;}
            else return fail("unimplemented CPU-control form");
        }else return fail("unknown Line-A opcode");
    }else if(kind>=32 && kind<48){NativeTiming::routine(NativeTiming::RPushException);if(!pushException(kind,0))return false;}
    else if(kind!=9 && kind!=11)return fail("unknown native exception vector");
    unsigned pendingIrq=0;
    if(diagnostic){if(!replayBoundary())return false;}
    else {
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
            if(!advanceClock(nativeCycles+80000))return false;
            NativeTiming::routine(NativeTiming::RLiveInputs);
            if(!liveInputs())return false;
            if((!coldSetup || nativeSetupReady) && uint32_t(board->inputEdges)!=lastInputEdge){
                lastInputEdge=uint32_t(board->inputEdges);diagnosticKeys();amigaInputApply(*board);
            }
            NativeTiming::routine(NativeTiming::RColdSetup);coldSetupStep();
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
    if(displayRequested && nativeCycles-lastPresentCycle>=160000){
        NativeTiming::Scope timing(NativeTiming::Present);
        lastPresentCycle=nativeCycles;
        screen.outputs(amigaInputLamps(),board->outputs());
        NativeTiming::routine(NativeTiming::RPresentation);if(!screen.present(board->video))return fail(screen.error);
    }
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
extern "C" void nativeProfileBenchmark(){
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
    nativeRomBegin=oldBegin;nativeRomEnd=oldEnd;nativeShortStatus[0]=oldDescriptor;

}
CopperList *nativeCopper(){return displayRequested?screen.copper():nullptr;}
void nativeAudioStart(){if(liveRequested){if(!amigaInputStart()){fail("keyboard resource unavailable");return;}paula.start();}}
void nativeAudioStop(){if(liveRequested){paula.stop();amigaInputStop();}}
void nativeVbi(bool quit){paula.vbi();screen.vbi();if(screen.swaps)NativeTiming::mark(NativeTiming::FirstSwap,nativeCycles,nativeLastPc);++pendingFrames;
    // The isolated exception benchmark runs synthetic supervisor code, not
    // guest instructions; do not schedule a game boundary into that context.
    if(nativeBenchmarkRequested)seenFrames=pendingFrames;
    if(quit || amigaInputQuit()){quitRequested=true;nativeFastBoundary=0;}}
extern "C" bool nativePrepareInner(){
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
    BPTR tests=Open("native-hardware-tests",MODE_OLDFILE);
    nativeSkipHardwareTests=!diagnostic && !tests;if(tests)Close(tests);
    BPTR live=Open("native-live",MODE_OLDFILE);liveRequested=!diagnostic || live!=0;
    BPTR display=Open("native-display",MODE_OLDFILE);displayRequested=liveRequested || display!=0;if(display)Close(display);
    if(live){uint8_t limit[5];LONG n=Read(live,limit,5);Close(live);if(n!=0 && n!=4)return fail("native-live must be empty or a four-byte cycle budget");if(n==4)liveStopCycles=get32(limit);}
    boardAllocation=(uint8_t*)pokeriAllocateUninitialized(sizeof(Board)+255);guard=(uint8_t*)pokeriAllocateUninitialized(0x80000);
    if(!boardAllocation || !guard)return fail("native allocations failed");
    board=new((void*)((uint32_t(boardAllocation)+255)&~255u)) Board();
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
                nativeShortStatus[i]=shortDescriptor(romBase+h.pc,preparedAccesses[meta.first].physical,uint16_t(0x0800|kind),meta.cycles);continue;
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
    put16(rom+0x10ae,0x6000);put16(rom+0x10b0,0x30);put16(rom+0x110c,0x6000);put16(rom+0x110e,0x2c);
    for(unsigned i=0;i<sizeof(hooks)/sizeof(*hooks);++i)put16(rom+hooks[i].pc,0xa000|i);
    for(auto pc:resets)put16(rom+pc,0xaffd);
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
    for(unsigned i=0;i<10;++i)if(i==9?(settings[i]!=1 && settings[i]!=3):settings[i]!=supported[i])return fail("unsupported native replay configuration");
    nativeSkipHardwareTests=(settings[9]&2)!=0;
    }
    if(nativeSkipHardwareTests)applyBootPolicy(rom);
    CacheClearU(); // Publish relocated/patched instructions to 68020+ caches.
    board->config.cpuHz=settings[0];board->config.systemHz=settings[1];board->config.inputHz=settings[2];board->config.watchdogMs=settings[3];board->config.watchdogResetUs=settings[4];board->ay.clockHz=settings[5];board->peer.enabled=settings[8];
if(liveRequested){if(!paula.prepare())return fail("Paula allocation failed");board->ay.backend=&paula;}
    if(!videoSurface.prepare())return fail("video bitplane allocation failed");
    board->video.surface=&videoSurface;
    if(displayRequested && !screen.prepare(videoSurface,board->memory.data()))return fail("screen allocation failed");
    if(diagnostic && !advanceEvent())return false;
    if(!diagnostic){coldSetup=true;board->pia[1].input[0]=0xff;board->pia[1].input[1]=0x7f;board->pia[2].input[0]=8;}
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
    nativeOldLevel2=vectors[26];vectors[26]=uint32_t(nativeLevel2);
    for(unsigned i=2;i<12;++i){savedVectors[i]=vectors[i];vectors[i]=uint32_t(i==9?nativeTrace:i==10?nativeLineA:nativeFault);}
    for(unsigned i=32;i<48;++i){savedVectors[i]=vectors[i];vectors[i]=uint32_t(traps[i-32]);}installed=true;
}
extern "C" void nativeRestoreVectors(){
    volatile uint32_t *vectors=nativeVectors;
    for(unsigned i=2;i<12;++i)vectors[i]=savedVectors[i];
    for(unsigned i=32;i<48;++i)vectors[i]=savedVectors[i];
    installed=false;
    vectors[27]=nativeOldLevel3;vectors[30]=nativeOldLevel6;vectors[26]=nativeOldLevel2;
    nativeVectorsRestored=vectors[27]==nativeOldLevel3 && vectors[30]==nativeOldLevel6 && vectors[26]==nativeOldLevel2;
    if(privateVectors){nativeWriteVbr(originalVbr);if(nativeReadVbr()!=originalVbr)nativeVectorsRestored=0;}
    for(unsigned i=2;i<12;++i)if(vectors[i]!=savedVectors[i])nativeVectorsRestored=0;
    for(unsigned i=32;i<48;++i)if(vectors[i]!=savedVectors[i])nativeVectorsRestored=0;
}
extern "C" __attribute__((noinline)) void nativeReturned(){asm volatile("" ::: "memory");}
void nativeRun(){
    if(nativeStatus!=1)return;
    seenFrames=pendingFrames;quitRequested=false;liveClock.reset(pendingFrames);
    if(!nativeBenchmarkRequested)NativeTiming::begin();
    NativeTiming::mark(NativeTiming::GuestStart,nativeCycles,nativeLastPc);
    Forbid();
    Supervisor((ULONG(*)())nativeEntry);
    NativeTiming::mark(NativeTiming::Finished,nativeCycles,nativeLastPc);
    NativeTiming::end();
    AmigaHardware::blitterDrain();
    Permit();
    checkGuard();
    if(!nativeVectorsRestored)fail("native vector restoration failed");
    nativeReturned();
}
void nativeRelease(){if(privateVectors){FreeMem(privateVectors,1024);privateVectors=nullptr;}nativeGuestTimerRelease();NativeTiming::release();if(liveRequested && board && (nativeStatus==3 || nativeStatus==4)){const char *error=saveNvram(board->nvram);if(error)fail(error);}
    screen.release();videoSurface.release();paula.release();if(DOSBase && nativeError){PutStr(nativeError);PutStr("\n");}delete reader;delete[] replayData;delete[] guard;if(board)board->~Board();delete[] boardAllocation;reader=nullptr;replayData=guard=boardAllocation=nullptr;board=nullptr;if(DOSBase)CloseLibrary((Library*)DOSBase);DOSBase=nullptr;}
