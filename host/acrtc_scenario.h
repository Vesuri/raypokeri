#ifndef POKERI_HOST_ACRTC_SCENARIO_H
#define POKERI_HOST_ACRTC_SCENARIO_H
#include "../src/native/DoubleScenario.h"
#include "../src/AmigaKeyEvents.h"
#include "../src/CabinetInput.h"
namespace pokeri_research {
// Same external player and read-acknowledged buttons as native diagnostics.
// No CPU results, card ranks, credits or game RAM are ever supplied.
struct AcrtcScenario {
    pokeri::DoubleScenario player;
    pokeri::AmigaKeyEvents keys;
    pokeri::ReadLatchedButtons buttons[2];
    pokeri::CabinetInput cabinet;
    uint64_t next=0;
    bool failed=false;
    static uint32_t word(const pokeri::Board &b,unsigned a){
        const auto &m=b.memory;return (uint32_t(m[a])<<24)|(uint32_t(m[a+1])<<16)|(uint32_t(m[a+2])<<8)|m[a+3];
    }
    template<class Log> void step(pokeri::Board &b,uint64_t elapsed,Log log){
        if(elapsed<next || failed)return;
        next=elapsed+160000; // 50 Hz external input observation at 8 MHz.
        player.step(uint32_t(elapsed),b.memory[0x4112f]!=0,
            [&]{return pokeri::DoubleScenario::holds([&](unsigned i){return word(b,0x41150+4*i);},
                                                    [&](unsigned i){return word(b,0x41168+4*i);});},
            [&](unsigned code,bool down){keys.key(code,down);log(code,down);});
        pokeri::AmigaKeyEvents::Snapshot events;
        bool okay=keys.take(events);okay=events.append(buttons) && okay;
        if(!okay){failed=true;return;}
        b.pia[1].input[0]=uint8_t(~buttons[0].advance());
        b.pia[1].input[1]=(b.pia[1].input[1]&~0x27)|uint8_t((~buttons[1].advance())&0x27);
        for(unsigned i=0;i<events.count[pokeri::AmigaKeyEvents::Coin];++i)cabinet.coin();
        cabinet.step(b);
    }
    void read(unsigned side,uint8_t value,uint8_t mask){buttons[side].read(value,mask);}
};
}
#endif
