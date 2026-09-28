#ifndef POKERI_PAULA_AY_H
#define POKERI_PAULA_AY_H
#include "board/AyBackend.h"
#include "board/AyEnvelope.h"
#include <exec/interrupts.h>
struct PaulaStream {
    const uint8_t *begin,*end,*next;
    volatile void *hardware;
    uint16_t irq,pad;
    volatile uint32_t interrupts;
};
struct MsgPort;
struct IOAudio;
class PaulaAy : public pokeri::AyBackend {
public:
    bool prepare();
    void start();
    void stop();
    void release();
    void write(unsigned reg,uint8_t value) override;
    void tick(uint32_t cycles) override;
    void vbi();
#ifdef POKERI_TIME_LEDGER
    void recordApplied(); // called after the screen swap, never before it
    uint32_t appliedWrites=~0u,appliedLevel=~0u;
#endif
    uint32_t streamHash=5381,writeCount=0;
    const char *error=nullptr;
    unsigned missingTone=0,missingNoise=0;
#ifdef POKERI_STARTUP_FAST_FORWARD
    bool muted=false; // preparation only; register/envelope state stays live
#endif
private:
    MsgPort *port=nullptr;
    IOAudio *request=nullptr;
    bool deviceOpen=false;
    uint8_t channelMask=15;
    uint8_t regs[16]={};
    uint8_t *waves=nullptr;
    uint16_t periods[4096]={};
    uint8_t *waveBank=nullptr;
    PaulaStream streams[3]={};
    Interrupt servers[3]={};
    Interrupt *oldServers[3]={};
    int selected[3]={-2,-2,-2};
    bool serversInstalled=false;
    pokeri::AyEnvelope envelope;
    bool active=false;
};
#endif
