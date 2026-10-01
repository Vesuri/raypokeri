#ifndef POKERI_BOARD_H
#define POKERI_BOARD_H
#include <array>
#include <cstdint>
#include "Device.h"
#include "AyBackend.h"
#include "Hd63484.h"
#include "SerialPeer.h"
#include "State.h"
#ifdef POKERI_HOST_ACRTC_TIMING
#ifdef POKERI_FREESTANDING
#error Host timing research must never enter the native build
#endif
namespace pokeri_research { class AcrtcTimingDevice; }
#endif
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
    AyBackend *backend=nullptr; // runtime attachment, never serialized
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
#ifdef POKERI_FREESTANDING
    // Native: ROM $00000-$3FFFF and a 64 KiB RAM window $40000-$4FFFF. The board
    // fits 16 KB; the program reaches $47000 (docs/memory-audit.md). A stray-write
    // canary follows the window and is not guest-addressable.
    static constexpr unsigned mappedMemory=0x50000,ramCanary=0x1000;
    // Native ROM is loaded in place; do not clear the half overwritten by file I/O.
    std::array<uint8_t, mappedMemory+ramCanary> memory;
#else
    static constexpr unsigned mappedMemory=0x80000;
    std::array<uint8_t, mappedMemory> memory{};
#endif
    Nvram nvram;
    Pia6821 pia[3];
    Acia6850 serial[3];
    SerialPeer peer;
    Ay38912 ay;
    Hd63484 video;
#ifdef POKERI_HOST_ACRTC_TIMING
    pokeri_research::AcrtcTimingDevice *timedVideo=nullptr;
    void checkTimedVideo();
    bool videoIrq() const;
#endif
    Config config;
    void (*log)(const char *device, unsigned reg, uint8_t value) = nullptr;
    // Optional frontend acknowledgment; not hardware or serialized state.
    void (*inputRead)(unsigned side,uint8_t value,uint8_t inputMask)=nullptr;
    bool fault = false, resetRequested = false;
    const char *faultReason = "unknown device access";
    uint64_t systemEdges = 0, inputEdges = 0;
    explicit Board(Config c = Config()) : config(c) {
#ifdef POKERI_FREESTANDING
        for(unsigned i=0x40000;i<mappedMemory;++i)memory[i]=0;
#endif
    }
    uint8_t read8(uint32_t address);
    void write8(uint32_t address, uint8_t value);
    void tick(uint32_t cycles) { tick(cycles,cycles); }
    // Presentation waits advance peripherals but do not age the cabinet watchdog.
    void tick(uint32_t cycles,uint32_t watchdogCycles);
    // Decoded endpoints: callers must validate chip (0..2) and register (0..3).
    uint8_t readPia(unsigned chip,unsigned reg);
    void writePia(unsigned chip,unsigned reg,uint8_t value);
    void state(State &s);
    const uint8_t *outputs()const{return outputLatches;}
    // Read-only scheduler view. Refuse stale derived watchdog thresholds;
    // ordinary tick() remains responsible for refreshing them.
    struct TimingSnapshot {uint64_t systemPhase,inputPhase,watchdogAge,warning,reset;};
    bool timingSnapshot(TimingSnapshot &out)const{
        if(config.watchdogMs && (watchdogClockCache!=config.cpuHz ||
           watchdogMsCache!=config.watchdogMs || watchdogResetCache!=config.watchdogResetUs))return false;
        out={systemPhase,inputPhase,watchdogAge,watchdogThreshold,watchdogResetThreshold};return true;
    }
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
inline uint8_t Pia6821::read8(unsigned offset) {
    unsigned side = (offset >> 1) & 1;
    if(offset & 1) return control[side] | flags[side];
    if(!(control[side] & 4)) return direction[side];
    flags[side] = 0;
    return (output[side] & direction[side]) | (input[side] & ~direction[side]);
}
inline void Pia6821::write8(unsigned offset, uint8_t value) {
    unsigned side = (offset >> 1) & 1;
    if(offset & 1) control[side] = value & 0x3f;
    else if(control[side] & 4) output[side] = value;
    else direction[side] = value;
}
// These members are owned PIA values, never derived devices. Qualified calls
// let constant native endpoints specialize without speculative vtable checks.
inline uint8_t Board::readPia(unsigned chip,unsigned reg) {
    if(chip==0 && reg==0 && (pia[0].output[1]&0x82)==0x82)
        pia[0].input[0]=ay.read8(1);
    uint8_t value=pia[chip].Pia6821::read8(reg);
    if(chip==1 && !(reg&1) && (pia[1].control[reg>>1]&4) && inputRead)
        inputRead(reg>>1,value,uint8_t(~pia[1].direction[reg>>1]));
    return value;
}
inline void Board::writePia(unsigned chip,unsigned reg,uint8_t value) {
    if(chip==0)peripheralWrite(reg,value);
    if(chip==2 && reg==2 && (pia[2].control[1]&4) && (value&0x80))watchdogKick();
    pia[chip].Pia6821::write8(reg,value);
}
inline void Board::watchdogKick() { watchdogAge=0; resetRequested=false; }
}
#endif
