#include "NativeTiming.h"
#include <proto/exec.h>
#include <proto/timer.h>
#include <devices/timer.h>
#include <proto/cia.h>
#include <resources/cia.h>
#include <hardware/cia.h>
#include <exec/interrupts.h>
struct Device *TimerBase=nullptr;
namespace NativeTiming {
Record records[Count];
bool active=false;
uint32_t frequency=0,started=0,elapsed=0,readOverhead=0;
static MsgPort *port=nullptr;
static timerequest *request=nullptr;
uint32_t now(){EClockVal value;ReadEClock(&value);return value.ev_lo;}
bool prepare(){
    port=CreateMsgPort();if(!port)return false;
    request=(timerequest*)CreateIORequest(port,sizeof(timerequest));if(!request)return false;
    if(OpenDevice((UBYTE*)TIMERNAME,UNIT_ECLOCK,(IORequest*)request,0))return false;
    TimerBase=request->tr_node.io_Device;
    EClockVal value;frequency=ReadEClock(&value);
    readOverhead=0xffffffffu;
    for(unsigned i=0;i<16;++i){uint32_t a=now(),n=now()-a;if(n<readOverhead)readOverhead=n;}
    return true;
}
void begin(){if(TimerBase){started=now();active=true;}}
void end(){if(active){elapsed=now()-started;active=false;}}
void release(){
    active=false;if(TimerBase){CloseDevice((IORequest*)request);TimerBase=nullptr;}
    if(request){DeleteIORequest((IORequest*)request);request=nullptr;}
    if(port){DeleteMsgPort(port);port=nullptr;}
}
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
