#pragma once
#include "DoubleScenario.h"
namespace pokeri {
// Keyboard-only payout regression. Observes accounting; never writes game state.
struct PayoutScenario {
    DoubleScenario player;
    unsigned phase=0;
    uint32_t deadline=0,credits=0,reserve=0,paid=0;
    bool done=false,failed=false;
    template<class Read,class Holds,class Key>
    void step(uint32_t now,bool ready,Read read,Holds choose,Key key){
        if(done || failed)return;
        if(phase==0){
            player.step(now,ready,choose,[&](unsigned code,bool down){
                key(code==0x55?0x59:code,down); // Collect instead of Double
            });
            if(player.failed){failed=true;return;}
            if(player.doubled){phase=1;deadline=now+DoubleScenario::ms(200);}
            return;
        }
        if(!DoubleScenario::due(now,deadline))return;
        switch(phase){
        case 1:key(0x59,false);phase=2;deadline=now+DoubleScenario::ms(4000);break;
        case 2:
            credits=read(0x44074);reserve=read(0x44000);paid=read(0x44078);
            if(!paid){failed=true;break;}
            key(0x59,true);phase=3;deadline=now+DoubleScenario::ms(200);break;
        case 3:key(0x59,false);phase=4;deadline=now+DoubleScenario::ms(20000);break;
        case 4:
            if(read(0x44078)!=0 || read(0x44074)!=credits || read(0x44000)+paid!=reserve){failed=true;break;}
            key(0x44,true);phase=5;deadline=now+DoubleScenario::ms(200);break;
        case 5:key(0x44,false);phase=6;deadline=now+DoubleScenario::ms(2000);break;
        case 6:
            if(read(0x44074)<=credits){failed=true;break;}
            credits=read(0x44074);key(0x40,true);phase=7;deadline=now+DoubleScenario::ms(200);break;
        case 7:key(0x40,false);phase=8;deadline=now+DoubleScenario::ms(8000);break;
        case 8:done=read(0x44074)<credits;failed=!done;break;
        }
    }
};
}
