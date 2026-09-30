#include "acrtc_scenario.h"
#include <cstdio>
#include <stdexcept>
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
    pokeri::Board b;pokeri_research::AcrtcScenario s;
    b.pia[1].input[0]=0xff;b.pia[1].input[1]=0x7f;
    auto original=b.memory;unsigned events=0;
    auto log=[&](unsigned,bool){++events;};
    s.step(b,800000,log);check(s.cabinet.pending.size()==1 && events==1,"coin queues external packet");
    s.step(b,2400000,log);s.step(b,12000000,log);
    check(!(b.pia[1].input[0]&1),"deal key pressed");
    s.step(b,13600000,log);check(!(b.pia[1].input[0]&1),"release waits for input observation");
    s.read(0,b.pia[1].input[0],0xff);s.step(b,13760000,log);
    check(b.pia[1].input[0]&1,"observed press permits release");
    check(b.memory==original && !s.failed,"driver never supplies CPU or game RAM state");
    s.player.phase=8;b.memory[0x4112f]=1;
    s.step(b,14000000,log);check(s.player.doubled && !(b.pia[1].input[0]&32),"ROM-ready flag permits external Double press");
    check(b.memory[0x4112f]==1,"driver does not clear ROM ready flag");
    puts("PASS: external coin/button driver, observed releases, read-only game state and Double-ready gating");return 0;
}catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
