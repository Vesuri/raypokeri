#ifndef POKERI_PAULA_AY_H
#define POKERI_PAULA_AY_H
#include "board/AyBackend.h"
#include "board/AyEnvelope.h"
#include "PaulaNoise.h"
#include <exec/interrupts.h>
struct PaulaStream {
    const uint8_t *begin,*end,*next;
    volatile void *hardware;
    uint16_t irq,bytes;
    volatile uint32_t interrupts;
    uint16_t period,currentPeriod,periodWait,streaming;
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
    void refreshNoise(); // bounded work after display publication
#ifdef POKERI_TIME_LEDGER
    void recordApplied(); // called after the screen swap, never before it
    uint32_t appliedWrites=~0u,appliedLevel=~0u;
#endif
    // Enabled at live Ready only; replay and preparation retain board time.
    bool wallEnvelope=false;
    uint32_t streamHash=5381,writeCount=0;
    const char *error=nullptr;
    uint32_t noiseRefreshes=0;
    bool muted=false; // preparation only; register/envelope state stays live
private:
    MsgPort *port=nullptr;
    IOAudio *request=nullptr;
    bool deviceOpen=false;
    uint8_t channelMask=15;
    uint8_t regs[16]={};
    uint8_t *waves=nullptr;
    uint16_t periods[4096]={};
    uint8_t *noiseWaves=nullptr;
    uint16_t noisePeriods[32]={};
    pokeri::PaulaNoise noiseSource;
    PaulaStream streams[3]={};
    Interrupt servers[3]={};
    Interrupt *oldServers[3]={};
    int selected[3]={-2,-2,-2};
    uint16_t selectedPeriod[3]={};
    bool serversInstalled=false;
    pokeri::AyEnvelope envelope;
    bool active=false,audibleNoise=false;
};
#endif
