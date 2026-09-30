#include "acrtc_timing_device.h"
#include <cstdio>
#include <stdexcept>
using pokeri::Hd63484;
using pokeri_research::AcrtcTimingDevice;
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
static bool ten(const Hd63484&,const std::vector<uint16_t>&,uint64_t&t){t=10;return true;}
static bool reject(const Hd63484&,const std::vector<uint16_t>&,uint64_t&){return false;}
static void word(AcrtcTimingDevice &d,uint16_t w){d.write8(2,w>>8);d.write8(2,w);}
int main(){try{
    Hd63484 v;AcrtcTimingDevice d(v,ten);
    word(d,0x4800);word(d,0xa55a); // WT, RWP initially zero.
    check(!d.fault() && !v.frame[0] && !(d.status()&Hd63484::CED),"drawing not committed on submission");
    d.write8(0,3);d.write8(2,1);check(d.irq(),"WFE interrupt remains available while busy");
    d.tick(9);check(!v.frame[0],"no pixels before deadline");d.tick(1);
    check(v.frame[0]==0xa55a && v.rwp==1 && v.ar==3 && d.completed()==1,"deadline commits original renderer and preserves selector");
    d.write8(0,0);word(d,0x0800);word(d,0x1234);word(d,0x0c00);
    d.tick(10);check(v.parameter[0]==0x1234 && !(d.status()&Hd63484::RFR),"queued RPR result not early");
    d.tick(10);check(d.status()&Hd63484::RFR,"RPR result appears at its deadline");
    check(d.read8(2)==0x12,"high result byte");
    word(d,0x4800);word(d,0x4321);d.tick(10);
    check(d.read8(2)==0x34 && v.frame[1]==0x4321,"completion does not disturb half-consumed read result");
    word(d,0x4800);word(d,0xffff);d.tick(5);d.write8(0,2);d.write8(2,0x82);d.tick(100);
    check(!d.fault() && !v.frame[2] && (d.status()&0x23)==0x23,"ABT cancels drawing and restores FIFO status");
    // Byte order of control-register auto-increment is shared with the model.
    d.write8(0,0xc2);d.write8(2,0);d.write8(2,152);check(v.control[0xc3]==152 && v.ar==0xc4,"control address increment");
    d.write8(0,0);word(d,0x4800);word(d,1);d.write8(0,0xc2);d.write8(2,1);
    check(d.fault()!=nullptr,"unknown in-flight memory-width latching stops loudly");
    Hd63484 u;AcrtcTimingDevice unknown(u,reject);word(unknown,0x0800);word(unknown,1);
    check(unknown.fault() && u.parameter[0]==0,"unsupported duration cannot execute");
    Hd63484 bad;AcrtcTimingDevice invalid(bad,ten);word(invalid,0x0800|0x20);
    check(invalid.fault()!=nullptr,"shared reserved-bit decoder");
    Hd63484 paced;paced.presentationBusy=true;AcrtcTimingDevice unsupported(paced,ten);unsupported.tick(1);
    check(unsupported.fault()!=nullptr,"unintegrated pacing policy refused");
    // Equivalent complete streams agree with the authoritative synchronous
    // model, including read results, RWP and drawing registers.
    Hd63484 reference,actual;AcrtcTimingDevice timed(actual,ten);
    const uint16_t words[]={0x0800,0xabcd,0x4800,0x4567,0x080c,0,0x080d,0,0x4400,0x0c00};
    for(uint16_t w:words){reference.writeFifoWord(w);word(timed,w);timed.tick(10);}
    check(!timed.fault() && reference.frame==actual.frame && reference.parameter==actual.parameter && reference.rwp==actual.rwp,"complete stream renderer equality");
    for(unsigned i=0;i<4;++i)check(reference.read8(2)==timed.read8(2),"read FIFO order equality");
    puts("PASS: delayed pixels/read results, WFE IRQ during drawing, queued state dependencies, ABT, read-byte preservation, control semantics and synchronous renderer equality");return 0;
}catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
