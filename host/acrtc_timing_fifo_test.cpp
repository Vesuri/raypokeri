// Synthetic command protocol: no ROM data or physical timing calibration.
#include "acrtc_timing_fifo.h"
#include <cstdio>
#include <stdexcept>
#include <utility>
struct Policy {
    bool rejectDuration=false,rejectCompletion=false;
    std::vector<std::pair<uint16_t,uint64_t>> done;
    int format(uint16_t w)const{return w==1?2:w==2?1:w==3?-1:w==4?-2:0;}
    bool duration(const std::vector<uint16_t>&w,uint64_t &ticks){ticks=w[0]==2?0:10;return !rejectDuration;}
    bool complete(const std::vector<uint16_t>&w,uint64_t now){done.emplace_back(w[0],now);return !rejectCompletion;}
};
using Fifo=pokeri_research::AcrtcTimingFifo<Policy>;
static void check(bool b,const char *s){if(!b)throw std::runtime_error(s);}
int main(){try{
    Policy p;Fifo f(p);check(f.status()==0x23,"reset status");
    f.write8(0);check(f.status()==0x23 && f.partialByte(),"high-byte staging");
    f.addressSelected();f.write16(1);check(f.status()==3 && f.collectedWords()==1,"empty while awaiting parameters");
    f.write16(42);check(f.status()==3 && f.busyTicks()==10 && f.irq(1) && !f.irq(0x20),"WFE independent of busy/CED");
    f.tick(9);check(p.done.empty(),"no early completion");
    for(unsigned i=0;i<8;++i)f.write16(2);
    check(f.status()==0 && f.queuedWords()==8,"exact eight-word queue capacity");
    f.tick(1);check(p.done.size()==9 && f.status()==0x23,"completion drains zero-duration queued commands");
    for(const auto &v:p.done)check(v.second==10,"same-boundary completion order");
    check(p.done.front().first==1 && p.done.back().first==2,"command order");
    f.write16(1);f.write16(0);for(unsigned i=0;i<8;++i)f.write16(2);f.write16(2);
    check(f.error && (f.status()&0x80),"overflow is a loud stop");
    f.abort();check(f.status()==0x23 && !f.partialByte(),"abort resets FIFO/status");
    unsigned before=p.done.size();f.write16(1);f.write16(9);f.tick(4);f.abort();f.tick(100);
    check(p.done.size()==before,"abort cannot complete abandoned command");
    f.write16(3);f.write16(20);for(unsigned i=0;i<20;++i)f.write16(uint16_t(i));
    check(!f.error && f.busyTicks()==10,"long parameter collection is not FIFO occupancy");
    f.tick(10);f.write16(4);f.write16(5);for(unsigned i=0;i<10;++i)f.write16(0);
    check(f.busyTicks()==10,"variable polyline count");f.tick(10);
    f.write16(0xffff);check(f.error,"unknown opcode rejected");f.abort();
    p.rejectDuration=true;f.write16(2);check(f.error,"unknown timing rejected");f.abort();p.rejectDuration=false;
    p.rejectCompletion=true;f.write16(2);check(f.error,"device completion failure propagated");
    // A single large clock advance and every split of that advance must
    // produce the same exact completion timestamps, queue and residual work.
    for(unsigned split=0;split<=100;++split){
        Policy a,b;Fifo x(a),y(b);
        for(auto *q:{&x,&y}){
            q->write16(1);q->write16(8);
            for(unsigned n=0;n<4;++n){q->write16(1);q->write16(uint16_t(n));}
        }
        x.tick(100);y.tick(split);y.tick(100-split);
        check(a.done==b.done && a.done.size()==5 && x.status()==y.status() && x.now==y.now && !x.error && !y.error,"partition-invariant scheduling");
        for(unsigned i=0;i<5;++i)check(a.done[i].second==10*(i+1),"serial commands finish on their own deadlines");
    }
    // A word-sized CPU access is still two sequential bus bytes when a
    // previous high byte is already staged.
    Policy partial;Fifo bytes(partial);bytes.write8(0);bytes.write16(0x0100);
    check(bytes.collectedWords()==1 && bytes.partialByte(),"partial byte survives word access");
    bytes.write8(7);check(bytes.busyTicks()==10 && !bytes.partialByte(),"partial parameter completed");
    bytes.addressSelected();bytes.tick(10);check(partial.done.size()==1,"address selection does not abort running work");
    // Repeated full queues exercise every ring-head position, not just zero.
    Policy r;Fifo q(r);
    for(unsigned n=0;n<1000;++n){q.write16(1);q.write16(0);for(unsigned i=0;i<8;++i)q.write16(2);q.tick(10);check(!q.error && q.status()==0x23,"ring wrap");}
    check(r.done.size()==9000,"no lost/duplicate wrapped commands");
    Policy z;Fifo overflow(z);overflow.now=UINT64_MAX-2;overflow.tick(3);check(overflow.error,"clock overflow refused");
    puts("PASS: independent WFE/CED, byte staging, eight-word capacity, variable parameters, abort/faults, 101 deadline partitions and 1000 ring wraps");return 0;
}catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
