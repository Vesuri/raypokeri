// Synthetic peripheral tests: no ROM bytes or game logic.
#include "../src/board/Board.h"
#include <cstdio>
#include <stdexcept>
using namespace pokeri;
static void check(bool value,const char *message) {
    if(!value) throw std::runtime_error(message);
}
int main() try {
    Pia6821 p;
    p.write8(0,0xf0); p.write8(1,4); p.input[0]=0x5a; p.write8(0,0xa5);
    check(p.read8(0)==0xaa,"PIA DDR must combine inputs with output latch");
    p.write8(1,0); check(p.read8(0)==0xf0,"PIA DDR selection");
    p.write8(3,0x0e); p.edge(1,2,true);
    check(!(p.read8(3)&0x40),"PIA wrong edge must not assert CB2");
    p.edge(1,2,false); check(p.irq() && (p.read8(3)&0x40),"CB2 edge/IRQ latch");
    p.read8(2); check(!p.irq(),"data read acknowledges PIA IRQ");
    p.write8(1,6); p.edge(0,1,true);
    check((p.read8(1)&0x80) && !p.irq(),"disabled interrupt must still latch CA1 edge");
    p.write8(1,7); check(p.irq(),"enabling an already latched interrupt");
    p.read8(0); check(!p.irq(),"CA1 acknowledgment");
    Config c; c.cpuHz=8000000;c.systemHz=100;c.inputHz=50;c.watchdogMs=400;
    Board b(c); b.write8(0xfb017,0x0e);b.write8(0xfb015,7);
    b.tick(79999);check(!b.irq(),"periodic source fired early");
    b.tick(1);check(b.irq()==5 && b.vector()==0x43,"system source vector");
    b.read8(0xfb016);check(!b.irq(),"system IRQ cleared");
    b.tick(80000);b.read8(0xfb016);check(b.vector()==0x46,"input source vector");
    b.reset();check(!b.irq() && b.pia[0].direction[0]==0,"peripheral reset");
    b.write8(0xfb01f,0x14);b.tick(3199999);
    check(!(b.read8(0xfb01f)&0x40),"watchdog fired early");
    b.tick(1);check(b.read8(0xfb01f)&0x40,"watchdog elapsed edge");
    b.nvram.write8(0x7fff,0x5a);b.reset();check(b.nvram.read8(0x7fff)==0x5a,"reset must preserve NVRAM");
    Ay38912 ay;ay.write8(0,1);ay.write8(1,0xff);check(ay.read8(1)==15,"AY coarse period mask");
    b.read8(0xfb000);check(b.fault,"unidentified FB000 access must stop");
    puts("PASS: PIA DDR, edges, IRQ acknowledgment, peripheral reset, timed signal experiments, NVRAM retention, AY masks, unknown register guard");
    return 0;
} catch(const std::exception &e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
