// Synthetic device/link/audio/state checks, never application or ROM bytes.
#include "../src/CabinetInput.h"
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
    {
        Board b;b.peer.enabled=true;b.serial[0].control=0x95;b.memory[0x4142e]=0x61;
        check(cabinetLinkIdle(b),"clean cabinet link is ready");
        CabinetInput input;input.coin();input.door();input.coin();
        for(unsigned busy=0;busy<11;++busy){
            if(busy==0)b.peer.pending.push_back({3});
            if(busy==1)b.peer.wire.push_back(0x50);
            if(busy==2)b.peer.state=1;
            if(busy==3)b.peer.assembling.push_back(0x71);
            if(busy==4)b.serial[0].receive.push_back(0x30);
            if(busy==5)b.serial[0].transmit.push_back(0x71);
            if(busy==6)b.serial[0].control=0xb5;
            if(busy==7)b.memory[0x4142e]=0x60;
            if(busy==8)b.memory[0x415db]=1;
            if(busy==9)b.memory[0x415df]=1;
            if(busy==10)b.peer.enabled=false;
            input.step(b);check(input.pending.size()==5,"busy link retains every external edge");
            b.peer=SerialPeer();b.peer.enabled=true;b.serial[0]=Acia6850();b.serial[0].control=0x95;
            b.memory[0x4142e]=0x61;b.memory[0x415db]=b.memory[0x415df]=0;
        }
        const std::vector<uint8_t> expected[]={{3},{1,0,0},{0x31,1,0},{3}};
        b.pia[1].input[1]=0x7f;
        unsigned packetIndex=0;
        for(auto packet:expected){
            if(packetIndex++==1){
                input.step(b);check(!(b.pia[1].input[1]&0x40) && b.peer.pending.empty(),"door edge precedes its reply");
                input.observe(0x2472,b);input.step(b);check(b.peer.pending.empty(),"old door mode cannot acknowledge new edge");
                b.memory[0x413f4]=1;input.step(b);check(b.peer.pending.empty(),"door mode alone cannot acknowledge unfinished callback");
                input.observe(0x1234,b);input.step(b);check(b.peer.pending.empty(),"unrelated PC is not a completed main-loop pass");
                input.observe(0x2472,b);
            }
            input.step(b);check(b.peer.pending.size()==1 && b.peer.pending.front()==packet,"cabinet packets retain order and payload");
            unsigned remaining=input.pending.size();input.step(b);check(input.pending.size()==remaining,"one in-flight packet at a time");
            b.peer.pending.clear();
        }
        check(input.pending.empty(),"all retained cabinet edges delivered");
        input.door();input.door();input.step(b);
        check((b.pia[1].input[1]&0x40) && input.waitingDoor,"rapid second door edge starts closing first");
        input.observe(0x2472,b);input.step(b);check(b.peer.pending.empty(),"close waits for matching ROM state too");
        b.memory[0x413f4]=0;input.observe(0x246a,b);input.step(b);b.peer.pending.clear();input.step(b);b.peer.pending.clear();input.step(b);
        check(!(b.pia[1].input[1]&0x40) && input.waitingDoor && !input.doorPass,"queued reopen gets its own acknowledgement");
    }
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
