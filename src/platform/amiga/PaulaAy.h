#ifndef POKERI_PAULA_AY_H
#define POKERI_PAULA_AY_H
#include "board/AyBackend.h"
#include "board/AyEnvelope.h"
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
    uint32_t streamHash=5381,writeCount=0;
private:
    MsgPort *port=nullptr;
    IOAudio *request=nullptr;
    bool deviceOpen=false;
    uint8_t channelMask=15;
    uint8_t regs[16]={};
    uint8_t *waves=nullptr;
    uint16_t periods[4096]={};
    uint32_t rng=0x13579bdf;
    pokeri::AyEnvelope envelope;
    uint16_t noiseOffset=0;
    bool active=false;
    void fillNoise(unsigned offset,unsigned count);
};
#endif
