#include "NativeTiming.h"
#include <proto/exec.h>
#include <proto/timer.h>
#include <devices/timer.h>
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
