#include "PaulaAy.h"
#include "PaulaPeriods.h"
#include "NativeTiming.h"
#include "board/WordMath.h"
#include <proto/exec.h>
#include <exec/memory.h>
#include <hardware/custom.h>
#include <devices/audio.h>
#include <hardware/intbits.h>
#include <stddef.h>
extern "C" void pokeriPaulaStream();
static_assert(offsetof(PaulaStream,bytes)==18 && offsetof(PaulaStream,interrupts)==20 && offsetof(PaulaStream,period)==24 && offsetof(PaulaStream,streaming)==30,"Paula stream assembly layout");
static volatile Custom *const custom=(volatile Custom*)0xdff000;
// Paula pitches short tone loops and shared, evolving noise/gated-noise loops.
// VBI refresh is bounded; no per-note rendering or per-sample interrupt mixer.
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
    noiseWaves=(uint8_t*)AllocMem(pokeri::PaulaNoise::Bytes,MEMF_CHIP);
    if(!noiseWaves)return false;
    noiseSource.prepare(noiseWaves);
    pokeri::paulaNoisePeriods(noisePeriods);
    pokeri::paulaPeriods(periods);return true;
}
void PaulaAy::start(){
    if(!waves)return;
    custom->dmacon=15;custom->adkcon=0xff;custom->intena=0x780;
    custom->intreq=0x780;custom->intreq=0x780;
    for(unsigned c=0;c<3;++c){
        streams[c].hardware=&custom->aud[c];streams[c].irq=INTF_AUD0<<c;streams[c].currentPeriod=170;
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
    if(noiseWaves){FreeMem(noiseWaves,pokeri::PaulaNoise::Bytes);noiseWaves=nullptr;}
    if(waves){FreeMem(waves,32);waves=nullptr;}
}
void PaulaAy::write(unsigned reg,uint8_t value){
#ifdef POKERI_TIME_LEDGER
    // Timestamp before publication: VBI may otherwise apply this write before
    // its event is recorded, creating a false multi-frame latency in the log.
    NativeTiming::event(1,(reg<<8)|value,writeCount+1,NativeTiming::slowCycles());
#endif
    // A live shape restart and its register must publish together with respect
    // to VBI. INTENA is also safe when this backend runs in physical user mode.
    const uint16_t restore=wallEnvelope?(custom->intenar&INTF_VERTB):0;
    if(wallEnvelope)custom->intena=INTF_VERTB;
    asm volatile("" ::: "memory");
    regs[reg]=value;++writeCount;
    streamHash=((streamHash<<5)+streamHash)^reg;
    streamHash=((streamHash<<5)+streamHash)^value;
    if(reg==13)envelope.restart(value);
    asm volatile("" ::: "memory");
    if(restore)custom->intena=INTF_SETCLR|restore;
}
void PaulaAy::tick(uint32_t cycles){
    if(wallEnvelope)return;
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
    if(muted)return;
    NativeTiming::Scope timing(NativeTiming::AyVbi);
    if(wallEnvelope)envelope.tick(160000,unsigned(regs[11])|(unsigned(regs[12])<<8),regs[13]);
    static const uint8_t volume[16]={0,1,1,1,1,2,3,4,6,8,11,16,23,32,45,64};
    audibleNoise=false;
    for(unsigned c=0;c<3;++c){
        unsigned p=regs[c*2]|(unsigned(regs[c*2+1])<<8);
        unsigned v=volume[(regs[c+8]&16)?envelope.level():regs[c+8]&15];
        bool tone=!(regs[7]&(1<<c)),noise=!(regs[7]&(8<<c));
        // Envelope timing stays live, but silent voices need no DMA slice
        // interrupts. Oscillator phase on a new attack is approximate.
        int index=noise && v?(tone?int(pokeri::paulaMixedWave(p)):0):-1;
        unsigned period=tone?periods[p]:noisePeriods[regs[6]&31];
        if(index>0)period=pokeri::paulaMixedPeriod(p,period);
        if(index>=0)audibleNoise=true;
        // VBI is level 3, the stream server level 4. Publish the descriptor
        // and next DMA pointer together; no generation or buffer copy here.
        uint16_t sr;asm volatile("move.w %%sr,%0\n\tmove.w #0x2400,%%sr":"=d"(sr)::"cc","memory");
        if(selected[c]!=index || selectedPeriod[c]!=period){
            unsigned length=index>=0?pokeri::PaulaNoise::Length:(p>2300?8:2);
            const uint8_t *begin=index>=0?noiseWaves+unsigned(index)*pokeri::PaulaNoise::Length:waves+(p>2300?2:0);
            unsigned bytes=pokeri::paulaSlice(period);if(bytes>length)bytes=length;
            PaulaStream &stream=streams[c];
            custom->intena=stream.irq;
            stream.begin=begin;stream.end=begin+length;stream.next=begin+bytes;
            stream.bytes=bytes;stream.period=period;stream.streaming=index>=0;
            // A slower period must not stretch an already-latched long block.
            // Queue the shorter slices first; two DMA boundaries flush both
            // the current and prefetched blocks before changing the period.
            stream.periodWait=period>stream.currentPeriod?2:0;
            if(!stream.periodWait){custom->aud[c].ac_per=period;stream.currentPeriod=period;}
            custom->aud[c].ac_ptr=(uint16_t*)begin;custom->aud[c].ac_len=bytes/2;
            custom->intreq=stream.irq;custom->intreq=stream.irq;
            // Pure tones need only the handoff, then loop without interrupts.
            custom->intena=0x8000|stream.irq;
        }
        custom->aud[c].ac_vol=index>=0 || (tone && !noise)?v:0;
        selectedPeriod[c]=period;
        selected[c]=index;
        asm volatile("move.w %0,%%sr"::"d"(sr):"cc","memory");
    }
}
void PaulaAy::refreshNoise(){
    // Fixed phase gates never change; replacing noise under DMA only evolves
    // the noise source. All notes share it and volume-only changes do no work.
    if(active && audibleNoise){noiseSource.refresh(noiseWaves);++noiseRefreshes;}
}
