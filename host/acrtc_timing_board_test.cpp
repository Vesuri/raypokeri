#include "../src/board/Board.h"
#include "acrtc_timing_device.h"
#include <cstdio>
#include <stdexcept>
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
static bool ten(const pokeri::Hd63484&,const std::vector<uint16_t>&,uint64_t&t){t=10;return true;}
static void word(pokeri::Board &b,uint16_t w){b.write8(0xf6002,w>>8);b.write8(0xf6002,w);}
int main(){try{
    pokeri::Board b;pokeri_research::AcrtcTimingDevice d(b.video,ten);b.timedVideo=&d;
    word(b,0x4800);word(b,0xa55a);
    check(!(b.read8(0xf6000)&pokeri::Hd63484::CED) && !b.video.frame[0],"bus uses delayed status and rendering");
    b.write8(0xf6000,3);b.write8(0xf6002,1);
    check(b.irq()==5 && b.vector()==0x40,"timed FIFO interrupt reaches encoder");
    b.pia[0].flags[1]=0x40;b.pia[0].control[1]=8;
    check(b.vector()==0x43,"system interrupt retains priority");
    b.pia[0].flags[1]=0;b.tick(9);check(!b.video.frame[0],"board tick preserves deadline");
    b.reset();b.tick(1);check(b.video.frame[0]==0xa55a && !b.fault,"CPU reset preserves in-flight device work");
    bool refused=false;try{pokeri::State s;b.state(s);}catch(const std::runtime_error&){refused=true;}
    check(refused,"timed snapshots cannot masquerade as synchronous state");
    b.video.presentationBusy=true;b.read8(0xf6000);
    check(b.fault && b.faultReason==d.fault(),"read faults reach board");
    pokeri::Board c;pokeri_research::AcrtcTimingDevice e(c.video,ten);c.timedVideo=&e;
    word(c,0x0820);check(c.fault && c.faultReason==e.fault(),"write faults reach board");
    pokeri::Board f;pokeri_research::AcrtcTimingDevice g(f.video,ten);f.timedVideo=&g;
    f.video.presentationBusy=true;f.tick(1);check(f.fault && f.faultReason==g.fault(),"tick faults reach board");
    puts("PASS: timing bus routing, IRQ priority, tick deadlines, reset preservation, snapshot refusal and read/write/tick faults");return 0;
}catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
