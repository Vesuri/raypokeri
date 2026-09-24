// Synthetic device/link/audio/state checks, never application or ROM bytes.
#include "../src/board/Board.h"
#include <cstdio>
#include <stdexcept>
using namespace pokeri;
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
static void feed(SerialPeer &p,std::initializer_list<uint8_t> bytes){unsigned sum=0;for(auto b:bytes){p.transmit(b);sum+=b;}p.transmit(uint8_t(~sum)|0x80);check(!p.error,"link rejected valid packet");}
static void wire(SerialPeer &p,std::initializer_list<uint8_t> expected){std::deque<uint8_t> rx;p.tick(100000,1000000,rx);check(rx==std::deque<uint8_t>(expected),"serial transport reply");}
struct Samples:Tone {std::vector<int16_t> data;void sample(int16_t v)override{data.push_back(v);}};
static void write(Ay38912 &a,unsigned r,unsigned v){a.write8(0,r);a.write8(1,v);}
int main()try{
    SerialPeer p;feed(p,{0x30});wire(p,{0,255});feed(p,{0x49,2});wire(p,{0x40,0xbf});feed(p,{0x50});wire(p,{0x50,0xaf});
    p.enqueue({3});std::deque<uint8_t> rx;p.tick(1000,1000000,rx);wire(p,{0x30,0xcf});
    feed(p,{0x40});wire(p,{3,0xfc});feed(p,{0});wire(p,{0x50,0xaf});feed(p,{0x50});check(p.state==0 && p.pending.empty() && p.wire.empty(),"outgoing session completes without echo loop");
    SerialPeer bad;bad.transmit(0x30);bad.transmit(0xff);check(bad.error,"bad serial checksum stops");
    Ay38912 a;write(a,0,2);write(a,7,0x3e);write(a,8,15);
    a.clockStep();check(!a.toneHigh[0],"tone period counts up");a.clockStep();check(a.toneHigh[0],"tone half period");
    write(a,0,0);a.clockStep();check(!a.toneHigh[0],"zero tone period equals one");
    Ay38912 n;n.clockStep();check(n.lfsr==1,"noise prescaler");n.clockStep();check(n.lfsr==0x10000,"noise feedback");
    for(unsigned shape=0;shape<16;++shape){
        Ay38912 e;write(e,11,1);write(e,13,shape);
        unsigned first=(shape&4)?0:15;check((e.envelopeStep^e.envelopeAttack)==first,"envelope restart");
        for(unsigned i=0;i<30;++i)e.clockStep();check((e.envelopeStep^e.envelopeAttack)==(15-first),"envelope 16 levels");
        e.clockStep();e.clockStep();
        unsigned expected=!(shape&8)?0:(shape&1)?((shape&2)?first:15-first):((shape&2)?15-first:first);
        check((e.envelopeStep^e.envelopeAttack)==expected,"envelope shape endpoint/loop");
        write(e,13,shape);check(!e.envelopeHold && e.envelopeStep==15,"repeated shape write restarts");
    }
    Board b;b.config.cpuHz=1000000;b.ay.clockHz=1000000;b.ay.cpuHz=1000000;b.config.systemHz=100;
    write(b.ay,0,100);write(b.ay,7,0x3e);write(b.ay,8,15);
    b.memory[0x45678]=0xa5;b.pia[2].input[1]=0x37;b.serial[0].receive.push_back(4);
    b.video.write8(0,0);b.video.write8(2,0x08); // snapshot a half command word
    b.peer.enabled=true;b.peer.enqueue({3});b.tick(13);
    State saved;b.state(saved);Board clone;State loaded(saved.bytes);clone.state(loaded);check(loaded.cursor==saved.bytes.size(),"device state consumes exact bytes");
    Samples one,two;b.ay.sink=&one;clone.ay.sink=&two;
    for(unsigned i=0;i<10000;++i){b.tick(100);clone.tick(100);}
    State s1,s2;b.state(s1);clone.state(s2);check(s1.bytes==s2.bytes && one.data==two.data,"restored devices and audio evolve identically");
    check(one.data.size()==44100,"sample count for one CPU second");
    auto truncated=saved.bytes;truncated.pop_back();bool rejected=false;
    try{Board other;State s(truncated);other.state(s);}catch(const std::exception&){rejected=true;}check(rejected,"truncated device state rejected");
    puts("PASS: serial transport, AY tone/noise/all envelope shapes, sample timing, full device state and deterministic continuation");
}catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
