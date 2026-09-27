#include "PaulaAy.h"
#include "PaulaPeriods.h"
#include "../../../amiga/generated/PaulaWaves.h"
#include "NativeTiming.h"
#include "board/WordMath.h"
#include <proto/exec.h>
#include <exec/memory.h>
#include <hardware/custom.h>
#include <devices/audio.h>
#include <hardware/intbits.h>
#include <stddef.h>
extern "C" void pokeriPaulaStream();
static_assert(offsetof(PaulaStream,interrupts)==20,"Paula stream assembly layout");
static volatile Custom *const custom=(volatile Custom*)0xdff000;
// Three AY voices. Pure tones use short hardware loops; noise/mixed voices
// select precomputed AND-gated loops. An assembly interrupt queues 256-byte
// slices so a sound change takes effect within 12.3 ms, not a whole noise loop.
bool PaulaAy::prepare(){
    port=CreateMsgPort();if(!port)return false;
    request=(IOAudio*)CreateIORequest(port,sizeof(IOAudio));if(!request)return false;
    request->ioa_Request.io_Message.mn_Node.ln_Pri=127;
    request->ioa_Request.io_Flags=ADIOF_NOWAIT;
    request->ioa_Data=&channelMask;request->ioa_Length=1;
    if(OpenDevice((UBYTE*)AUDIONAME,0,(IORequest*)request,0))return false;
    deviceOpen=true;
    waves=(uint8_t*)AllocMem(32,MEMF_CHIP|MEMF_CLEAR);
    if(!waves)return false;
    // Two-sample tone, followed by an eight-sample loop for the lowest notes.
    waves[0]=127;waves[1]=129;
    // Use an eight-sample square for low notes (offset 2, four high/four low).
    for(unsigned i=2;i<10;++i)waves[i]=i<6?127:129;
    waveBank=(uint8_t*)AllocMem(sizeof(paulaWaveData),MEMF_CHIP);
    if(!waveBank)return false;
    CopyMem((APTR)paulaWaveData,waveBank,sizeof(paulaWaveData));
    pokeri::paulaPeriods(periods);return true;
}
void PaulaAy::start(){
    if(!waves)return;
    custom->dmacon=15;custom->adkcon=0xff;custom->intena=0x780;
    custom->intreq=0x780;custom->intreq=0x780;
    for(unsigned c=0;c<3;++c){
        streams[c].hardware=&custom->aud[c];streams[c].irq=INTF_AUD0<<c;
        servers[c].is_Node.ln_Type=NT_INTERRUPT;
        servers[c].is_Node.ln_Name=(char*)"Pokeri Paula waveform";
        servers[c].is_Data=&streams[c];servers[c].is_Code=pokeriPaulaStream;
        oldServers[c]=SetIntVector(INTB_AUD0+c,&servers[c]);
    }
    serversInstalled=true;
    for(unsigned c=0;c<4;++c){custom->aud[c].ac_vol=0;custom->aud[c].ac_per=170;
        custom->aud[c].ac_ptr=(uint16_t*)waves;custom->aud[c].ac_len=1;}
    custom->dmacon=0x8207;active=true;
}
void PaulaAy::stop(){
    if(!active)return;
    active=false;custom->intena=0x780;custom->dmacon=15;
    custom->intreq=0x780;custom->intreq=0x780;
    for(unsigned c=0;c<4;++c)custom->aud[c].ac_vol=0;
    if(serversInstalled){for(unsigned c=0;c<3;++c)SetIntVector(INTB_AUD0+c,oldServers[c]);serversInstalled=false;}
}
void PaulaAy::release(){
    stop();
    if(deviceOpen){CloseDevice((IORequest*)request);deviceOpen=false;}
    if(request){DeleteIORequest((IORequest*)request);request=nullptr;}
    if(port){DeleteMsgPort(port);port=nullptr;}
    if(waveBank){FreeMem(waveBank,sizeof(paulaWaveData));waveBank=nullptr;}
    if(waves){FreeMem(waves,32);waves=nullptr;}
}
void PaulaAy::write(unsigned reg,uint8_t value){
    regs[reg]=value;++writeCount;
#ifdef POKERI_TIME_LEDGER
    NativeTiming::event(1,(reg<<8)|value,writeCount,NativeTiming::slowCycles());
#endif
    streamHash=((streamHash<<5)+streamHash)^reg;
    streamHash=((streamHash<<5)+streamHash)^value;
    if(reg==13)envelope.restart(value);
}
void PaulaAy::tick(uint32_t cycles){
    NativeTiming::Scope timing(NativeTiming::AyTick);
    while(cycles){uint32_t n=cycles>160000?160000:cycles;envelope.tick(n,unsigned(regs[11])|(unsigned(regs[12])<<8),regs[13]);cycles-=n;}
}
#ifdef POKERI_TIME_LEDGER
void PaulaAy::recordApplied(){
    unsigned level=envelope.level();
    if(appliedWrites!=writeCount || appliedLevel!=level){
        NativeTiming::event(2,writeCount,level,NativeTiming::slowCycles());
        appliedWrites=writeCount;appliedLevel=level;
    }
}
#endif
void PaulaAy::vbi(){
    if(!active)return;
    NativeTiming::Scope timing(NativeTiming::AyVbi);
    static const uint8_t volume[16]={0,1,1,1,1,2,3,4,6,8,11,16,23,32,45,64};
    for(unsigned c=0;c<3;++c){
        unsigned p=regs[c*2]|(unsigned(regs[c*2+1])<<8);
        unsigned v=volume[(regs[c+8]&16)?envelope.level():regs[c+8]&15];
        bool tone=!(regs[7]&(1<<c)),noise=!(regs[7]&(8<<c));
        // Envelope timing stays live, but silent voices need no DMA slice
        // interrupts. Oscillator phase on a new attack is approximate.
        int index=-1;
        if(noise && v){
            unsigned tp=tone?(p?p:1):0,np=regs[6]?regs[6]:1;
            if(selected[c]>=0 && paulaWaveEntries[selected[c]].tone==tp && paulaWaveEntries[selected[c]].noise==np)
                index=selected[c];
            else for(unsigned i=0;i<sizeof(paulaWaveEntries)/sizeof(*paulaWaveEntries);++i)
                if(paulaWaveEntries[i].tone==tp && paulaWaveEntries[i].noise==np){index=i;break;}
            if(index<0){missingTone=tp;missingNoise=np;error="unbanked AY noise waveform";v=0;}
        }
        // VBI is level 3, the stream server level 4. Publish the descriptor
        // and next DMA pointer together; no generation or buffer copy here.
        uint16_t sr;asm volatile("move.w %%sr,%0\n\tmove.w #0x2400,%%sr":"=d"(sr)::"cc","memory");
        if(index>=0){
            if(selected[c]!=index){
                const PaulaWaveEntry &wave=paulaWaveEntries[index];
                const uint8_t *begin=waveBank+wave.offset;
                custom->intena=streams[c].irq;
                streams[c].begin=begin;streams[c].end=begin+wave.length;streams[c].next=begin+256;
                custom->aud[c].ac_ptr=(uint16_t*)begin;custom->aud[c].ac_len=128;
                custom->intreq=streams[c].irq;custom->intreq=streams[c].irq;
                custom->intena=0x8000|streams[c].irq;
            }
            custom->aud[c].ac_per=170;custom->aud[c].ac_vol=v;
        }else{
            custom->intena=streams[c].irq;
            custom->intreq=streams[c].irq;custom->intreq=streams[c].irq;
            custom->aud[c].ac_per=periods[p];
            custom->aud[c].ac_ptr=(uint16_t*)(waves+(p>2300?2:0));
            custom->aud[c].ac_len=p>2300?4:1;
            custom->aud[c].ac_vol=tone && !noise?v:0;
        }
        selected[c]=index;
        asm volatile("move.w %0,%%sr"::"d"(sr):"cc","memory");
    }
}
