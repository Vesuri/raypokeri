#include "NativeTiming.h"
#include <proto/exec.h>
#include <proto/timer.h>
#include <devices/timer.h>
#include <proto/cia.h>
#include <resources/cia.h>
#include <hardware/cia.h>
#include <exec/interrupts.h>
#include <exec/memory.h>
struct Device *TimerBase=nullptr;
extern "C" volatile uint16_t nativeProfileEnabled=0;
extern "C" volatile uint32_t nativeCycles;
extern "C" uint64_t nativeClockCharged[3];
extern "C" uint32_t nativeShortGuest,nativeShortNominal,nativeShortCalls;
extern "C" volatile uint32_t nativeInstructions;
namespace NativeTiming {
static constexpr unsigned Capacity=65536;
uint32_t calls[Count],kinds[48],*hooks=nullptr,routines[RoutineCount];
Sample *samples=nullptr;
PlaySample *playSamples=nullptr;
uint32_t mainLoops=0;
volatile uint32_t sampleCount=0,dropped=0;
Milestone milestones[PointCount];
unsigned context=Count;
bool active=false;
uint32_t frequency=0,started=0,elapsed=0;
#ifdef POKERI_TIME_LEDGER
static constexpr unsigned FrameCapacity=16384;
Ledger ledger,*ledgerMarks=nullptr;
FrameRecord *frameRecords=nullptr;
SlowCommand *slowCommands=nullptr;
volatile uint32_t frameCount=0,slowCount=0;
unsigned commandGroup=64;
uint16_t commandWords[8];
uint32_t kindTicks[Count],ledgerTicks=0,ledgerReadCost=0;
volatile uint8_t *ledgerLow=nullptr,*ledgerHigh=nullptr;
uint16_t ledgerLast=0;
static Library *ledgerCia=nullptr;
static Interrupt ledgerInterrupt;
static unsigned ledgerBit=0;
static volatile uint8_t *ledgerControl=nullptr;
static uint32_t ledgerUnusedInterrupt(){return 0;}
// Try CIA-A timer B, then either CIA-B timer; never steal an OS owner.
// Its interrupt stays disabled: VBI records and scopes extend the count.
static bool ledgerClockPrepare(){
    struct Candidate {const char *name;unsigned bit;uint32_t control,low,high;};
    static const Candidate candidates[]={
        {CIAANAME,CIAICRB_TB,0xbfef01,0xbfe601,0xbfe701},
        {CIABNAME,CIAICRB_TA,0xbfde00,0xbfd400,0xbfd500},
        {CIABNAME,CIAICRB_TB,0xbfdf00,0xbfd600,0xbfd700}};
    ledgerInterrupt.is_Node.ln_Type=NT_INTERRUPT;
    ledgerInterrupt.is_Node.ln_Name=(char*)"Pokeri time ledger";
    ledgerInterrupt.is_Code=(void(*)())ledgerUnusedInterrupt;
    for(const auto &c:candidates){
        Library *resource=(Library*)OpenResource((CONST_STRPTR)c.name);if(!resource)continue;
        Disable();
        if(AddICRVector(resource,c.bit,&ledgerInterrupt)){Enable();continue;}
        AbleICR(resource,1u<<c.bit);
        ledgerCia=resource;ledgerBit=c.bit;ledgerControl=(volatile uint8_t*)c.control;
        ledgerLow=(volatile uint8_t*)c.low;ledgerHigh=(volatile uint8_t*)c.high;
        *ledgerControl=0;*ledgerLow=0xff;*ledgerHigh=0xff;
        *ledgerControl=0x11; // continuous E-clock count, force load and start
        Enable();
        ledgerLast=uint16_t((*ledgerHigh<<8)|*ledgerLow);ledgerTicks=0;
        uint32_t start=ledgerNow();for(unsigned i=0;i<256;++i)ledgerNow();
        ledgerReadCost=ledgerNow()-start;
        return true;
    }
    return false;
}
static void ledgerClockRelease(){
    if(!ledgerCia)return;
    Disable();*ledgerControl=0;SetICR(ledgerCia,1u<<ledgerBit);
    RemICRVector(ledgerCia,ledgerBit,&ledgerInterrupt);Enable();ledgerCia=nullptr;
}
uint32_t slowCycles(){return nativeCycles;}
void frameRecord(){
    uint32_t f=frameCount;
    if(!active || !frameRecords || f>=FrameCapacity)return;
    frameRecords[f]={ledgerNow(),nativeCycles,uint32_t(nativeClockCharged[0])+nativeShortGuest,
        kindTicks[Service],kindTicks[Command],kindTicks[Present],kindTicks[BlitWait],kindTicks[ShortCall]};
    frameCount=f+1;
}
void ledgerSnapshot(Ledger &out){
    out=ledger;out.clock=ledgerNow();out.cycles=nativeCycles;
    out.guest=uint32_t(nativeClockCharged[0])+nativeShortGuest;
    out.hooked=uint32_t(nativeClockCharged[1])+nativeShortNominal;
    out.shortCalls=nativeShortCalls;out.dispatches=nativeInstructions;
}
#endif
static MsgPort *port=nullptr;
static timerequest *request=nullptr;
// Only begin/end call the OS clock, never a scope, bus access or command.
static uint32_t now(){EClockVal value;ReadEClock(&value);return value.ev_lo;}
uint32_t benchmarkClock(){return now();}
bool prepare(){
    samples=(Sample*)AllocMem(Capacity*sizeof(Sample),MEMF_FAST);
    hooks=(uint32_t*)AllocMem(4096*sizeof(uint32_t),MEMF_FAST|MEMF_CLEAR);
    playSamples=(PlaySample*)AllocMem(26*sizeof(PlaySample),MEMF_FAST|MEMF_CLEAR);
    if(!samples || !hooks || !playSamples)return false;
#ifdef POKERI_TIME_LEDGER
    ledgerMarks=(Ledger*)AllocMem(26*sizeof(Ledger),MEMF_FAST|MEMF_CLEAR);
    frameRecords=(FrameRecord*)AllocMem(FrameCapacity*sizeof(FrameRecord),MEMF_FAST|MEMF_CLEAR);
    slowCommands=(SlowCommand*)AllocMem(SlowCapacity*sizeof(SlowCommand),MEMF_FAST|MEMF_CLEAR);
    if(!ledgerMarks || !frameRecords || !slowCommands || !ledgerClockPrepare())return false;
#endif
    port=CreateMsgPort();if(!port)return false;
    request=(timerequest*)CreateIORequest(port,sizeof(timerequest));if(!request)return false;
    if(OpenDevice((UBYTE*)TIMERNAME,UNIT_ECLOCK,(IORequest*)request,0))return false;
    TimerBase=request->tr_node.io_Device;
    EClockVal value;frequency=ReadEClock(&value);return true;
}
void playMark(unsigned index,uint32_t cycles,uint32_t frames){
    if(!active || !playSamples || index>=26)return;
    playSamples[index]={cycles,frames,uint32_t(nativeClockCharged[0])+nativeShortGuest,
        uint32_t(nativeClockCharged[1])+nativeShortNominal,mainLoops};
#ifdef POKERI_TIME_LEDGER
    ledgerSnapshot(ledgerMarks[index]);
#endif
}
void mark(Point point,uint32_t cycles,uint32_t pc){
    if(!active || milestones[point].seen)return;
    milestones[point]={1,sampleCount,cycles,pc,uint32_t(nativeClockCharged[0]),uint32_t(nativeClockCharged[1]),uint32_t(nativeClockCharged[2])};
}
void begin(){if(TimerBase){started=now();active=true;nativeProfileEnabled=1;}}
void end(){nativeProfileEnabled=0;if(active){elapsed=now()-started;active=false;}}
void release(){
    nativeProfileEnabled=0;active=false;
    if(TimerBase){CloseDevice((IORequest*)request);TimerBase=nullptr;}
    if(request){DeleteIORequest((IORequest*)request);request=nullptr;}
    if(port){DeleteMsgPort(port);port=nullptr;}
    if(samples){FreeMem(samples,Capacity*sizeof(Sample));samples=nullptr;}
    if(playSamples){FreeMem(playSamples,26*sizeof(PlaySample));playSamples=nullptr;}
    if(hooks){FreeMem(hooks,4096*sizeof(uint32_t));hooks=nullptr;}
#ifdef POKERI_TIME_LEDGER
    ledgerClockRelease();
    if(ledgerMarks){FreeMem(ledgerMarks,26*sizeof(Ledger));ledgerMarks=nullptr;}
    if(frameRecords){FreeMem(frameRecords,FrameCapacity*sizeof(FrameRecord));frameRecords=nullptr;}
    if(slowCommands){FreeMem(slowCommands,SlowCapacity*sizeof(SlowCommand));slowCommands=nullptr;}
#endif
}
}
// The level-3 wrapper calls this only for a pending, enabled VERTB. BLIT-only
// interrupts do not sample. Stacked PC is captured before any C handler runs.
extern "C" void nativeProfileSample(uint32_t pc){
    using namespace NativeTiming;
    uint32_t n=sampleCount;
    if(n==Capacity){++dropped;return;}
    samples[n]={pc,nativeCycles,context};sampleCount=n+1;
}

// Reserve CIA-A timer A without stealing an OS owner. The short assembly
// boundaries address its control register directly. IRQs are not needed:
// native VBI/CIA wrappers sample guest intervals before the 16-bit wrap.
extern "C" {
volatile uint16_t nativeClockEnabled=0;
volatile uint8_t *nativeGuestTimerControl=nullptr;
volatile uint8_t *nativeGuestTimerLow=nullptr;
volatile uint8_t *nativeGuestTimerHigh=nullptr;
}
static Library *guestCia=nullptr;
static Interrupt guestTimerInterrupt;
static uint8_t guestTimerSavedControl;
static uint32_t guestTimerUnusedInterrupt(){return 0;}
bool nativeGuestTimerPrepare(){
    Library *resource=(Library*)OpenResource(CIAANAME);if(!resource)return false;
    guestTimerInterrupt.is_Node.ln_Type=NT_INTERRUPT;
    guestTimerInterrupt.is_Node.ln_Name=(char*)"Pokeri guest clock";
    guestTimerInterrupt.is_Code=(void(*)())guestTimerUnusedInterrupt;
    Disable();
    if(AddICRVector(resource,CIAICRB_TA,&guestTimerInterrupt)){Enable();return false;}
    AbleICR(resource,CIAICRF_TA);guestCia=resource;
    nativeGuestTimerControl=(volatile uint8_t*)0xbfee01;
    nativeGuestTimerLow=(volatile uint8_t*)0xbfe401;
    nativeGuestTimerHigh=(volatile uint8_t*)0xbfe501;
    guestTimerSavedControl=*nativeGuestTimerControl;
    *nativeGuestTimerControl=0;
    *nativeGuestTimerLow=0xff;*nativeGuestTimerHigh=0xff;
    SetICR(resource,CIAICRF_TA);nativeClockEnabled=1;Enable();return true;
}
void nativeGuestTimerRelease(){
    if(!guestCia)return;
    Disable();nativeClockEnabled=0;*nativeGuestTimerControl=0;
    SetICR(guestCia,CIAICRF_TA);
    *nativeGuestTimerControl=guestTimerSavedControl&~0x11;
    RemICRVector(guestCia,CIAICRB_TA,&guestTimerInterrupt);Enable();
    guestCia=nullptr;nativeGuestTimerControl=nullptr;
}
