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
    c.watchdogResetUs=50000;Board timed(c);timed.pia[1].input[0]=0xa5;
    timed.tick(3200000);check(!timed.resetRequested,"watchdog warning precedes reset");
    timed.tick(399999);check(!timed.resetRequested,"watchdog reset delay");
    timed.tick(1);check(timed.resetRequested,"watchdog reset requested after delay");
    timed.watchdogKick();timed.config.cpuHz=4000000;timed.config.watchdogMs=200;timed.config.watchdogResetUs=100000;
    timed.tick(1199999);check(!timed.resetRequested,"changed watchdog timing must use the new clock and settings");
    timed.tick(1);check(timed.resetRequested,"changed watchdog reset boundary");
    timed.reset();check(!timed.resetRequested && timed.pia[1].input[0]==0xa5,"reset preserves external input pins and rearms timer");
    timed.tick(799999);
    auto edges=timed.systemEdges;
    timed.tick(4000000,0);
    check(!timed.resetRequested && timed.systemEdges==edges+100,"presentation wait advances peripherals without aging watchdog");
    timed.tick(400000);check(!timed.resetRequested,"watchdog retains age across presentation wait");
    timed.tick(1);check(timed.resetRequested,"normal watchdog reset resumes after presentation wait");
    timed.reset();
    timed.serial[0].write8(0,0xb5);check(timed.irq()==5 && timed.vector()==0x47 && timed.serial[0].read8(0)==0x82,"ACIA0 TX-ready interrupt routing");
    timed.serial[0].write8(0,0x95);check(!timed.serial[0].irq(),"ACIA TX IRQ disable");
    timed.serial[0].receive.push_back(0x5a);check(timed.serial[0].read8(0)==0x83 && timed.serial[0].irq(),"ACIA receive IRQ status");
    check(timed.serial[0].read8(1)==0x5a && !timed.serial[0].irq(),"ACIA data read acknowledges receive");
    timed.serial[0].receive.push_back(1);timed.serial[0].write8(0,3);check(timed.serial[0].receive.empty(),"ACIA reset clears receive");
    Ay38912 ay;ay.write8(0,1);ay.write8(1,0xff);check(ay.read8(1)==15,"AY coarse period mask");
    b.read8(0xfb000);check(b.fault,"unidentified FB000 access must stop");
    // HD63484, 8-bit bus: RS = offset bit 1, words high byte first (synthetic sequences).
    Hd63484 v;
    auto word=[&v](uint16_t w){v.write8(2,w>>8);v.write8(2,w&0xff);};
    check(v.read8(0)==0x23,"idle status is WFE|WFR|CED");
    v.write8(0,0);word(0x080c);word(0x0000);word(0x080d);word(0x0ff0);
    check(v.rwp==0x00ff,"WPR 0C/0D set the 20-bit read/write pointer");
    word(0x4800);word(0x55aa);word(0x4800);word(0xaa55);
    check(v.frame[0xff]==0x55aa && v.frame[0x100]==0xaa55 && v.rwp==0x101,"WT writes and advances RWP");
    word(0x080d);word(0x0ff0);word(0x4400);
    check(v.read8(0)&Hd63484::RFR,"RD fills the read FIFO");
    check(v.read8(2)==0x55 && v.read8(2)==0xaa && !(v.read8(0)&Hd63484::RFR),"read FIFO high byte first");
    v.read8(2);v.read8(2);check(v.readUnderflows==1,"empty read FIFO counts an underflow");
    v.write8(2,0x58);check(v.read8(0)==0x23,"half a word does not start a command");
    v.write8(2,0x00);check(!(v.read8(0)&Hd63484::CED),"partial CLR clears CED");
    word(1);word(2);word(3);check((v.read8(0)&Hd63484::CED) && v.unexecuted==0 && v.frame[0x100]==1,"CLR executes only after all four words arrive");
    v.write8(0,0x82);v.write8(2,0x5f);v.write8(2,0x06);check(v.control[0x82]==0x5f && v.control[0x83]==6 && v.ar==0x84,"display registers auto-increment per byte");
    v.write8(0,3);v.write8(2,0x20);check(v.ar==3 && v.irq(),"CCR low byte enables the matching status interrupts");
    v.write8(2,0);check(!v.irq(),"interrupts off");
    v.write8(0,0);word(0x1800);word(2);word(0x1111);check(!(v.read8(0)&Hd63484::CED),"WPTN n=2 needs two data words (ROM: n counts words)");
    word(0x2222);check((v.read8(0)&Hd63484::CED),"WPTN n=2 ends after two data words");
    word(0x0804);word(0x00ff);word(0x080c);word(0);word(0x080d);word(0x0100);word(0x4800);word(0xaaaa);
    word(0x080d);word(0x0100);word(0x4c00);word(0x5555);
    check(v.frame[0x10]==0xaa55,"MOD replace changes only MASK bits");
    v.write8(0,0);word(0x0000);check(v.error && (v.read8(0)&Hd63484::CER),"invalid command word is a loud error");
    puts("PASS: PIA DDR, edges, IRQ acknowledgment, peripheral reset, timed signal experiments, NVRAM retention, AY masks, unknown register guard, HD63484 bus/FIFO/RWP/commands");
    return 0;
} catch(const std::exception &e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
