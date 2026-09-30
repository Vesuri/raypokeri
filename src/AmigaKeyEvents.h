#ifndef POKERI_AMIGA_KEY_EVENTS_H
#define POKERI_AMIGA_KEY_EVENTS_H
#include <stdint.h>
#include "ReadLatchedButtons.h"
namespace pokeri {
// Keyboard IRQ producer, service consumer. The caller masks keyboard IRQs
// around take(); no OS calls, allocation or input acknowledgment live here.
class AmigaKeyEvents {
public:
    enum { ButtonCount=12, Door=12, Coin=13, Lamps=14, Count=15 };
    struct Snapshot {
        uint8_t count[Count];
        bool append(ReadLatchedButtons (&buttons)[2]) const {
            bool okay=true;
            for(unsigned i=0;i<8;++i)okay=buttons[0].append(i,count[i]) && okay;
            for(unsigned i=0;i<3;++i)okay=buttons[1].append(i,count[8+i]) && okay;
            return buttons[1].append(5,count[11]) && okay;
        }
    };
    void key(unsigned code,bool down){
        if(code==0x45){quitDown=down;return;}
        // PA0 Deal, PA1 Collect, PA2 Bet, PA3 High, PA4 Low, PA5 Double,
        // PA6/7 Hold5/4; PB0/1/2/5 Hold3/2/Operator/Hold1.
        // Cabinet rows: F1-F5 holds; F6-F10 Double/Low/High/Bet/Collect.
        // Space Deal, Enter Coin, Delete Door, O Operator, L Lamps.
        static const uint8_t codes[Count]={0x40,0x59,0x58,0x57,0x56,0x55,0x54,0x53,0x52,0x51,0x18,0x50,0x46,0x44,0x28};
        unsigned i=0;while(i<Count && codes[i]!=code)++i;
        if(i==Count || bool(level[i])==down)return;
        level[i]=down;
        if(transitions[i]==255)overflow=true;else ++transitions[i];
        if(i>=ButtonCount && down){
            unsigned event=i-ButtonCount;
            if(presses[event]==255)overflow=true;else ++presses[event];
        }
    }
    bool quit() const {return quitDown;}
    bool take(Snapshot &out){
        for(unsigned i=0;i<ButtonCount;++i){out.count[i]=transitions[i];transitions[i]=0;}
        for(unsigned i=ButtonCount;i<Count;++i){out.count[i]=presses[i-ButtonCount];presses[i-ButtonCount]=0;transitions[i]=0;}
        return !overflow;
    }
private:
    volatile uint8_t level[Count]={},transitions[Count]={},presses[Count-ButtonCount]={};
    volatile bool quitDown=false,overflow=false;
};
}
#endif
