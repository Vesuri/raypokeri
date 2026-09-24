#include "Board.h"
namespace pokeri {
uint8_t Ay38912::read8(unsigned) {
    if(selected == 14 && (registers[7] & 0x40)) return port;
    return registers[selected];
}
void Ay38912::write8(unsigned offset, uint8_t value) {
    if(!offset) { selected = value & 15; return; }
    static const uint8_t masks[16] = {255,15,255,15,255,15,31,255,31,31,31,255,255,15,255,255};
    registers[selected] = value & masks[selected];
    ++writes[selected];
    if(selected==13){envelopeCount=0;envelopeStep=15;envelopeAttack=(value&4)?15:0;envelopeHold=false;}
}
uint8_t Pia6821::read8(unsigned offset) {
    unsigned side = (offset >> 1) & 1;
    if(offset & 1) return control[side] | flags[side];
    if(!(control[side] & 4)) return direction[side];
    flags[side] = 0;
    return (output[side] & direction[side]) | (input[side] & ~direction[side]);
}
void Pia6821::write8(unsigned offset, uint8_t value) {
    unsigned side = (offset >> 1) & 1;
    if(offset & 1) control[side] = value & 0x3f;
    else if(control[side] & 4) output[side] = value;
    else direction[side] = value;
}
void Pia6821::edge(unsigned side, unsigned pin, bool rising) {
    uint8_t c = control[side];
    if(pin == 1 && bool(c & 2) == rising) flags[side] |= 0x80;
    if(pin == 2 && !(c & 0x20) && bool(c & 0x10) == rising) flags[side] |= 0x40;
}
bool Pia6821::irq() const {
    for(unsigned s=0;s<2;++s)
        if(((flags[s]&0x80) && (control[s]&1)) ||
           ((flags[s]&0x40) && (control[s]&8) && !(control[s]&0x20))) return true;
    return false;
}
const char *Board::name(uint32_t a) const {
    a &= 0xfffff;
    if(a < 0x40000) return "rom";
    if(a < 0x80000) return "ram";
    if(a >= 0xd0000 && a < 0xd8000) return "nvram";
    if(a >= 0xf6000 && a < 0xf6004) return "hd63484";
    if(a >= 0xfb014 && a < 0xfb020) return "pia";
    if((a >= 0xfb002 && a <= 0xfb003) || (a >= 0xfb006 && a <= 0xfb007) ||
       (a >= 0xfb00a && a <= 0xfb00b)) return "acia";
    return "unmapped";
}
uint8_t Board::read8(uint32_t a) {
    a &= 0xfffff;
    if(a < memory.size()) return memory[a];
    if(a >= 0xd0000 && a < 0xd8000) return nvram.read8(a-0xd0000);
    if(a >= 0xf6000 && a < 0xf6004) return video.read8(a-0xf6000);
    if(a >= 0xfb014 && a < 0xfb020) {
        if(a == 0xfb014 && (pia[0].output[1]&0x82)==0x82)
            pia[0].input[0] = ay.read8(1);
        return pia[(a-0xfb014)/4].read8((a-0xfb014)%4);
    }
    for(unsigned i=0;i<3;++i)
        if(a >= 0xfb002+4*i && a <= 0xfb003+4*i) return serial[i].read8(a-(0xfb002+4*i));
    fault=true; return 0;
}
void Board::peripheralWrite(unsigned offset, uint8_t value) {
    if(offset != 2 || !(pia[0].control[1]&4)) return;
    uint8_t old = pia[0].output[1], data = pia[0].output[0];
    if((old&2) && !(value&2)) {
        latchData = data;
        if((value&0x80) && ay.selected == 15) {fault=true;faultReason="AY-3-8912 has no register 15";return;}
        ay.write8(value&0x80 ? 1 : 0, data);
        if((value&0x80) && log) log("AY",ay.selected,data);
    }
    if((old&4) && !(value&4)) outputLatches[data&7] = latchData;
}
void Board::write8(uint32_t a, uint8_t value) {
    a &= 0xfffff;
    if(a < 0x40000) return;
    if(a < memory.size()) { memory[a]=value; return; }
    if(a >= 0xd0000 && a < 0xd8000) { nvram.write8(a-0xd0000,value); return; }
    if(a >= 0xf6000 && a < 0xf6004) {
        video.write8(a-0xf6000,value);
        if(video.error) {fault=true;faultReason=video.error;}
        return;
    }
    if(a >= 0xfb014 && a < 0xfb020) {
        if(a < 0xfb018) peripheralWrite(a-0xfb014,value);
        if(a == 0xfb01e && (pia[2].control[1]&4) && (value&0x80)) watchdogKick();
        pia[(a-0xfb014)/4].write8((a-0xfb014)%4,value); return;
    }
    for(unsigned i=0;i<3;++i)
        if(a >= 0xfb002+4*i && a <= 0xfb003+4*i) {serial[i].write8(a-(0xfb002+4*i),value);return;}
    fault=true;
}
void Board::reset() {
    for(unsigned i=0;i<3;++i) { auto a=pia[i].input[0],b=pia[i].input[1];pia[i] = Pia6821();pia[i].input[0]=a;pia[i].input[1]=b;serial[i] = Acia6850();}
    watchdogKick();
}
void Board::watchdogKick() { watchdogAge=0; resetRequested=false; }
void Board::tick(uint32_t cycles) {
    ay.cpuHz=config.cpuHz;ay.tick(cycles);
    if(peer.enabled) {
        while(!serial[0].transmit.empty()){peer.transmit(serial[0].transmit.front());serial[0].transmit.pop_front();}
        peer.tick(cycles,config.cpuHz,serial[0].receive);
        if(peer.error){fault=true;faultReason=peer.error;}
    } else for(auto &s:serial)s.transmit.clear();
    systemPhase += uint64_t(cycles)*config.systemHz;
    while(systemPhase >= config.cpuHz) {
        systemPhase -= config.cpuHz; ++systemEdges;
        pia[0].edge(1,2,false); pia[0].edge(1,2,true);
    }
    inputPhase += uint64_t(cycles)*config.inputHz;
    while(inputPhase >= config.cpuHz) {
        inputPhase -= config.cpuHz; ++inputEdges;
        pia[0].edge(0,1,false); pia[0].edge(0,1,true);
    }
    if(config.watchdogMs) {
        uint64_t threshold = uint64_t(config.cpuHz)*config.watchdogMs/1000;
        if(watchdogAge < threshold && watchdogAge+cycles >= threshold) pia[2].edge(1,2,true);
        watchdogAge += cycles;
        if(config.watchdogResetUs && watchdogAge >= threshold + uint64_t(config.cpuHz)*config.watchdogResetUs/1000000)
            resetRequested=true;
    }
}
unsigned Board::irq() const { return pia[0].irq() || video.irq() || serial[0].irq() ? 5 : 0; }
unsigned Board::vector() const {
    if((pia[0].flags[1]&0x40) && (pia[0].control[1]&8)) return 0x43;
    if((pia[0].flags[0]&0x80) && (pia[0].control[0]&1)) return 0x46;
    // INFERRED: vector $40 (-> $2E26) services the HD63484 FIFO; priority below the PIA sources
    // is a guess until the board's interrupt encoder is known.
    if(video.irq()) return 0x40;
    if(serial[0].irq()) return 0x47;
    return 24;
}
}
