#include "NativeTiming.h"
#include <proto/exec.h>
#include <proto/timer.h>
#include <devices/timer.h>
#include <proto/cia.h>
#include <resources/cia.h>
#include <hardware/cia.h>
#include <exec/interrupts.h>
#include <exec/memory.h>
#ifndef POKERI_RELEASE
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
#ifdef POKERI_LEDGER_FAST_CACHE
volatile uint32_t fastCache=1;
#else
volatile uint32_t fastCache=0;
#endif
Scope *Scope::top=nullptr;
CardCost *cardCosts=nullptr;
uint32_t cardCostCount=0,cardCostDropped=0;
void cardCost(unsigned type){
    if(!cardCosts)return;
    if(cardCostCount==CardCostCapacity){++cardCostDropped;return;}
    CardCost &r=cardCosts[cardCostCount++];
    r.type=type;r.clock=ledgerNow();r.cycles=nativeCycles;r.reads=ledgerReads;
    r.guest=uint32_t(nativeClockCharged[0])+nativeShortGuest;
    r.hooked=uint32_t(nativeClockCharged[1])+nativeShortNominal;
    r.dispatches=nativeInstructions;r.shortCalls=nativeShortCalls;
    Scope::snapshot(r.ticks,r.clock);
    r.observerTicks=ledgerNow()-r.clock;
}
static constexpr unsigned FrameCapacity=16384;
Ledger ledger,*ledgerMarks=nullptr,*startupMarks=nullptr;
FrameRecord *frameRecords=nullptr;
SlowCommand *slowCommands=nullptr;
Event *events=nullptr;
volatile uint32_t eventCount=0,eventDropped=0;
void event(unsigned type,uint32_t a,uint32_t b,uint32_t cycles){
    if(!isActive() || !events)return;
    // Main code and VBI publish complete records under the same short mask.
    volatile uint16_t *ena=(volatile uint16_t*)0xdff09a,*read=(volatile uint16_t*)0xdff01c;
    uint16_t enabled=*read&0x4000;*ena=0x4000;
    unsigned n=eventCount;
    if(n<EventCapacity){events[n]={ledgerNow(),cycles,type,a,b,ledgerReads};eventCount=n+1;}else ++eventDropped;
    if(type==3 || type==6)cardCost(type);
    if(enabled)*ena=0xc000;
}
volatile uint32_t frameCount=0,slowCount=0;
unsigned commandGroup=64;
uint16_t commandWords[8];
uint32_t kindTicks[Count],ledgerTicks=0,ledgerReadCost=0,ledgerReads=0;
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
    if(!isActive() || !frameRecords || f>=FrameCapacity)return;
    frameRecords[f]={ledgerNow(),nativeCycles,uint32_t(nativeClockCharged[0])+nativeShortGuest,
        kindTicks[Service],kindTicks[Command],kindTicks[Present],kindTicks[BlitWait],kindTicks[ShortCall]};
    frameCount=f+1;
}
void startupMark(unsigned stage,uint32_t cycles){
    if(!isActive() || !startupMarks || stage>=9)return;
    ledgerSnapshot(startupMarks[stage]);event(7,stage,0,cycles);
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
    // Keep this diagnostic metadata in the linked image, not just DWARF.
    (void)fastCache;
    ledgerMarks=(Ledger*)AllocMem(26*sizeof(Ledger),MEMF_FAST|MEMF_CLEAR);
    startupMarks=(Ledger*)AllocMem(9*sizeof(Ledger),MEMF_FAST|MEMF_CLEAR);
    frameRecords=(FrameRecord*)AllocMem(FrameCapacity*sizeof(FrameRecord),MEMF_FAST|MEMF_CLEAR);
    slowCommands=(SlowCommand*)AllocMem(SlowCapacity*sizeof(SlowCommand),MEMF_FAST|MEMF_CLEAR);
    events=(Event*)AllocMem(EventCapacity*sizeof(Event),MEMF_FAST);
    cardCosts=(CardCost*)AllocMem(CardCostCapacity*sizeof(CardCost),MEMF_FAST);
    if(!cardCosts || !events || !startupMarks || !ledgerMarks || !frameRecords || !slowCommands || !ledgerClockPrepare())return false;
#endif
    port=CreateMsgPort();if(!port)return false;
    request=(timerequest*)CreateIORequest(port,sizeof(timerequest));if(!request)return false;
    if(OpenDevice((UBYTE*)TIMERNAME,UNIT_ECLOCK,(IORequest*)request,0))return false;
    TimerBase=request->tr_node.io_Device;
    EClockVal value;frequency=ReadEClock(&value);return true;
}
void playMark(unsigned index,uint32_t cycles,uint32_t frames){
    if(!isActive() || !playSamples || index>=26)return;
    playSamples[index]={cycles,frames,uint32_t(nativeClockCharged[0])+nativeShortGuest,
        uint32_t(nativeClockCharged[1])+nativeShortNominal,mainLoops};
#ifdef POKERI_TIME_LEDGER
    ledgerSnapshot(ledgerMarks[index]);
#endif
}
void mark(Point point,uint32_t cycles,uint32_t pc){
    if(!isActive() || milestones[point].seen)return;
    milestones[point]={1,sampleCount,cycles,pc,uint32_t(nativeClockCharged[0]),uint32_t(nativeClockCharged[1]),uint32_t(nativeClockCharged[2])};
}
void begin(){
#ifndef POKERI_NO_PROFILE_SUPPORT
    if(TimerBase){started=now();active=true;nativeProfileEnabled=1;
#ifdef POKERI_TIME_LEDGER
        startupMark(0,nativeCycles);
#endif
    }
#endif
}
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
    if(cardCosts){FreeMem(cardCosts,CardCostCapacity*sizeof(CardCost));cardCosts=nullptr;}
    if(events){FreeMem(events,EventCapacity*sizeof(Event));events=nullptr;}
    if(ledgerMarks){FreeMem(ledgerMarks,26*sizeof(Ledger));ledgerMarks=nullptr;}
    if(startupMarks){FreeMem(startupMarks,9*sizeof(Ledger));startupMarks=nullptr;}
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

#endif
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
