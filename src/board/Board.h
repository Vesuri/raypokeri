#ifndef POKERI_BOARD_H
#define POKERI_BOARD_H
#include <array>
#include <cstdint>
#include "Device.h"
#include "Hd63484.h"
#include "SerialPeer.h"
#include "State.h"
namespace pokeri {
struct Nvram : Device {
    std::array<uint8_t, 0x8000> bytes{};
    uint8_t read8(unsigned offset) override { return bytes[offset & 0x7fff]; }
    void write8(unsigned offset, uint8_t value) override { bytes[offset & 0x7fff] = value; }
    void tick(uint32_t) override {}
    bool irq() const override { return false; }
};
struct Tone { virtual ~Tone() {} virtual void sample(int16_t value)=0; };
struct Ay38912 : Device {
    std::array<uint8_t, 16> registers{};
    std::array<uint64_t, 16> writes{};
    uint8_t selected = 0;
    uint8_t port = 0xff;
    uint32_t clockHz=0,cpuHz=8000000,sampleRate=44100;
    uint64_t clockPhase=0,samplePhase=0;
    uint32_t toneCount[3]={},noiseCount=0,envelopeCount=0,lfsr=1;
    bool toneHigh[3]={},noiseHalf=false,envelopeHold=false;
    uint8_t envelopeStep=15,envelopeAttack=0;
    int64_t dc=0;
    Tone *sink=nullptr;
    void clockStep();
    void state(State &s);
    uint8_t read8(unsigned) override;
    void write8(unsigned offset, uint8_t value) override;
    void tick(uint32_t cycles) override;
    bool irq() const override { return false; }
};
struct Pia6821 : Device {
    uint8_t control[2] = {}, direction[2] = {}, output[2] = {}, input[2] = {};
    uint8_t flags[2] = {};
    uint8_t read8(unsigned offset) override;
    void write8(unsigned offset, uint8_t value) override;
    void edge(unsigned side, unsigned pin, bool rising);
    void tick(uint32_t) override {}
    bool irq() const override;
};
struct Acia6850 : Device {
    uint8_t control = 3;
    std::deque<uint8_t> receive, transmit;
    uint8_t read8(unsigned offset) override {
        if(!(offset&1)) return uint8_t(2 | (!receive.empty()?1:0) | (irq()?0x80:0));
        if(receive.empty())return 0;
        uint8_t v=receive.front();receive.pop_front();return v;
    }
    void write8(unsigned offset, uint8_t value) override { if (offset & 1) transmit.push_back(value); else { control = value; if((value&3)==3){receive.clear();transmit.clear();} } }
    void tick(uint32_t) override {}
    bool irq() const override { return (control & 3)!=3 && ((control & 0x60) == 0x20 || ((control&0x80) && !receive.empty())); }
};
struct Config {
    uint32_t cpuHz = 8000000;
    // Zero means no hypothesised external source. Research settings are explicit.
    uint32_t systemHz = 0, inputHz = 0, watchdogMs = 0;
    uint32_t watchdogResetUs = 0; // explicit hypothesis: reset delay after warning edge
};
class Board {
public:
    std::array<uint8_t, 0x80000> memory{};
    Nvram nvram;
    Pia6821 pia[3];
    Acia6850 serial[3];
    SerialPeer peer;
    Ay38912 ay;
    Hd63484 video;
    Config config;
    void (*log)(const char *device, unsigned reg, uint8_t value) = nullptr;
    bool fault = false, resetRequested = false;
    const char *faultReason = "unknown device access";
    uint64_t systemEdges = 0, inputEdges = 0;
    explicit Board(Config c = Config()) : config(c) {}
    uint8_t read8(uint32_t address);
    void write8(uint32_t address, uint8_t value);
    void tick(uint32_t cycles);
    void state(State &s);
    void watchdogKick();
    void reset();
    unsigned irq() const;
    unsigned vector() const;
    const char *name(uint32_t address) const;
private:
    uint64_t systemPhase = 0, inputPhase = 0, watchdogAge = 0;
    // Derived timing cache, deliberately excluded from serialized device state.
    uint32_t watchdogClockCache=0,watchdogMsCache=0,watchdogResetCache=0;
    uint64_t watchdogThreshold=0,watchdogResetThreshold=0;
    uint8_t outputLatches[8] = {}, latchData = 0;
    void peripheralWrite(unsigned offset, uint8_t value);
};
}
#endif
