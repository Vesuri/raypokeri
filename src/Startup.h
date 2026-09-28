#ifndef POKERI_STARTUP_H
#define POKERI_STARTUP_H
#include "CabinetInput.h"
namespace pokeri {
// External cabinet setup for a fresh game. Observe ROM state, never supply it.
// Called every 10 ms of board time; link pacing remains SerialPeer's protocol.
struct Startup {
    enum Stage {Boot,Door,Status,Collect,Refill,Close,Confirm,Settle,Ready,WarmConfirm};
    Stage stage=Boot;
    unsigned age=0,coins=0;
    bool mainPass=false,retained=false;
    const char *error=nullptr;
    void observe(uint32_t pc){if(pc==0x2472 || pc==0x246a)mainPass=true;}
    void next(Stage value){stage=value;age=0;mainPass=false;}
    template<class Emit> void step(const Board &b,Emit emit){
        if(stage==Ready || error)return;
        if(++age>3000){error="cabinet setup made no progress for 30 board seconds";return;}
        const auto &m=b.memory;
        auto byte=[&](unsigned displacement){return m[0x48b00-displacement];};
        auto word=[&](unsigned address){return (uint32_t(m[address])<<24)|(uint32_t(m[address+1])<<16)|(uint32_t(m[address+2])<<8)|m[address+3];};
        bool idle=cabinetLinkIdle(b);
        switch(stage){
        case Boot:
            if(mainPass && (!retained || idle)){
                if(retained){emit(4,1,0x20000);emit(4,0x31,0x20100);next(WarmConfirm);}
                else {emit(1,1,0x3f);next(Door);}
            }break;
        case Door:
            if(byte(0x770c) && mainPass && idle){emit(4,1,0x20000);emit(4,0x31,0x20100);next(Status);}break;
        case Status:
            if(!byte(0x78ce) && byte(0x78de) && idle){emit(1,0,0xfd);next(Collect);}break;
        case Collect:
            if(byte(0x78d2)){emit(1,0,0xff);next(Refill);}break;
        case Refill:
            // One coin at a time: wait for both protocol completion and the
            // ROM accounting update, rather than guessing an inter-coin delay.
            if(mainPass && idle && word(0x4400c)==coins){
                if(coins==100){emit(1,1,0x7f);next(Close);}
                else {emit(4,3,0);++coins;age=0;mainPass=false;}
            }break;
        case Close:
            if(mainPass && idle && !byte(0x770c) && !byte(0x78d2)){
                emit(4,1,0x20000);emit(4,0x31,0x20100);next(Confirm);
            }break;
        case Confirm:
            if(mainPass && idle && !byte(0x770c) && !byte(0x78d2) && !byte(0x78ce) && byte(0x78de)){
                if(word(0x44074)!=0 || word(0x4400c)!=100){error="cabinet setup accounting differs from zero-credit start";return;}
                next(Settle);
            }break;
        case Settle:
            // Let the close-door callback finish and the main loop resume;
            // flags can clear before its display/accounting work returns.
            if(mainPass && idle)next(Ready);
            break;
        case WarmConfirm:
            // Original boot recovers accounting/game state. A warm cabinet only
            // exchanges peripheral status; never collect or refill its credits.
            if(mainPass && idle && !byte(0x770c) && !byte(0x78d2) && !byte(0x78ce) && byte(0x78de))next(Settle);
            break;
        case Ready:break;
        }
    }
};
}
#endif
