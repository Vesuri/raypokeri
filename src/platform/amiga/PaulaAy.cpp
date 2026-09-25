#include "PaulaAy.h"
#include "PaulaPeriods.h"
#include "NativeTiming.h"
#include "board/WordMath.h"
#include <proto/exec.h>
#include <exec/memory.h>
#include <hardware/custom.h>
#include <devices/audio.h>
static volatile Custom *const custom=(volatile Custom*)0xdff000;
// Three independent square-loop voices plus shared noise on Paula channel 3.
// Noise+tone uses an additive approximation, not AY's bitwise mixer. The noise
// loop and envelope update cadence deliberately follow the inexpensive ROF
// POKEY->Paula approach. No live PCM synthesis or game-state approximation.
void PaulaAy::fillNoise(unsigned offset,unsigned count){
    uint32_t *p=(uint32_t*)(waves+32+offset),x=rng;
    for(unsigned i=0;i<count;++i){x^=x<<1;x^=x>>5;x^=x<<16;p[i]=x;}
    rng=x;
}
bool PaulaAy::prepare(){
    port=CreateMsgPort();if(!port)return false;
    request=(IOAudio*)CreateIORequest(port,sizeof(IOAudio));if(!request)return false;
    request->ioa_Request.io_Message.mn_Node.ln_Pri=127;
    request->ioa_Request.io_Flags=ADIOF_NOWAIT;
    request->ioa_Data=&channelMask;request->ioa_Length=1;
    if(OpenDevice((UBYTE*)AUDIONAME,0,(IORequest*)request,0))return false;
    deviceOpen=true;
    waves=(uint8_t*)AllocMem(32+8192,MEMF_CHIP|MEMF_CLEAR);
    if(!waves)return false;
    // Two-sample tone, followed by an eight-sample loop for the lowest notes.
    waves[0]=127;waves[1]=129;
    // Use an eight-sample square for low notes (offset 2, four high/four low).
    for(unsigned i=2;i<10;++i)waves[i]=i<6?127:129;
    pokeri::paulaPeriods(periods);fillNoise(0,2048);return true;
}
void PaulaAy::start(){
    if(!waves)return;
    custom->dmacon=15;custom->adkcon=0xff;
    for(unsigned c=0;c<4;++c){custom->aud[c].ac_vol=0;custom->aud[c].ac_per=124;
        custom->aud[c].ac_ptr=(uint16_t*)(c==3?waves+32:waves);
        custom->aud[c].ac_len=c==3?4096:1;}
    custom->dmacon=0x820f;active=true;
}
void PaulaAy::stop(){if(!active)return;active=false;custom->dmacon=15;for(unsigned c=0;c<4;++c)custom->aud[c].ac_vol=0;}
void PaulaAy::release(){
    if(deviceOpen){CloseDevice((IORequest*)request);deviceOpen=false;}
    if(request){DeleteIORequest((IORequest*)request);request=nullptr;}
    if(port){DeleteMsgPort(port);port=nullptr;}
    if(waves){FreeMem(waves,8224);waves=nullptr;}
}
void PaulaAy::write(unsigned reg,uint8_t value){
    regs[reg]=value;++writeCount;
    streamHash=((streamHash<<5)+streamHash)^reg;
    streamHash=((streamHash<<5)+streamHash)^value;
    if(reg==13)envelope.restart(value);
}
void PaulaAy::tick(uint32_t cycles){
    NativeTiming::Scope timing(NativeTiming::AyTick);
    while(cycles){uint32_t n=cycles>160000?160000:cycles;envelope.tick(n,unsigned(regs[11])|(unsigned(regs[12])<<8),regs[13]);cycles-=n;}
}
void PaulaAy::vbi(){
    if(!active)return;
    NativeTiming::Scope timing(NativeTiming::AyVbi);
    static const uint8_t volume[16]={0,1,1,1,1,2,3,4,6,8,11,16,23,32,45,64};
    unsigned noiseVolume=0;
    for(unsigned c=0;c<3;++c){
        unsigned p=regs[c*2]|(unsigned(regs[c*2+1])<<8);
        unsigned v=volume[(regs[c+8]&16)?envelope.level():regs[c+8]&15];
        bool tone=!(regs[7]&(1<<c)),noise=!(regs[7]&(8<<c));
        custom->aud[c].ac_per=periods[p];
        // Both outgoing tone loops are short. A pointer change takes effect at
        // their next natural wrap; noise always stays on its own DMA channel.
        custom->aud[c].ac_ptr=(uint16_t*)(waves+(p>2300?2:0));
        custom->aud[c].ac_len=p>2300?4:1;
        custom->aud[c].ac_vol=tone?(noise?v>>1:v):0;
        if(noise)noiseVolume+=tone?v>>1:v;
    }
    unsigned n=regs[6]?regs[6]:1;
    // AY noise divider is clock/(16*N), one Paula sample per noise step.
    custom->aud[3].ac_per=unsigned(uint16_t(n)*uint16_t(57));
    if(custom->aud[3].ac_per<124)custom->aud[3].ac_per=124;
    custom->aud[3].ac_vol=noiseVolume>64?64:noiseVolume;
    if(noiseVolume){fillNoise(noiseOffset,16);noiseOffset=(noiseOffset+64)&8191;}
}
