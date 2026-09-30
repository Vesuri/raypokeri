// Synthetic device/link/audio/state checks, never application or ROM bytes.
#include "../src/Startup.h"
#include <cstdio>
#include <stdexcept>
#include <string>
using namespace pokeri;
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
static void feed(SerialPeer &p,std::initializer_list<uint8_t> bytes){unsigned sum=0;for(auto b:bytes){p.transmit(b);sum+=b;}p.transmit(uint8_t(~sum)|0x80);check(!p.error,"link rejected valid packet");}
static void wire(SerialPeer &p,std::initializer_list<uint8_t> expected){std::deque<uint8_t> rx;p.tick(100000,1000000,rx);check(rx==std::deque<uint8_t>(expected),"serial transport reply");}
struct Samples:Tone {std::vector<int16_t> data;void sample(int16_t v)override{data.push_back(v);}};
static void write(Ay38912 &a,unsigned r,unsigned v){a.write8(0,r);a.write8(1,v);}
static bool acceptHeader(Hd63484 &v,uint16_t value){
    uint16_t *words;unsigned *count;uint8_t *high;int *length;
    const auto &f=Hd63484::formats[value>>10];
    if(!v.inlineHeader(words,count,high,length) || !f.words || f.words==1 || (value&f.reserved))return false;
    *words=value;*count=1;*high=uint8_t(value>>8);*length=f.words;return true;
}
static void sameVideo(Hd63484 &a,Hd63484 &b){
    check(a.statusNow()==b.statusNow() && a.irq()==b.irq(),"header status/IRQ differs");
    check(bool(a.error)==bool(b.error),"header error differs");
    if(a.error)check(std::string(a.error)==b.error,"header fault reason differs");
    const char *ea=a.error,*eb=b.error;a.error=b.error=nullptr;
    State x,y;a.state(x);b.state(y);check(x.bytes==y.bytes,"header complete state differs");a.error=ea;b.error=eb;
}
static void videoIrqControlBytes(){
    unsigned cases=0;Hd63484 a,b;
    for(Hd63484 *v:{&a,&b}){
        v->writeFifoWord(0x0c00); // unread result must survive CCR changes
        v->writeFifoWord(0x0400); // unfinished ORG must also survive
        v->write8(0,3);
    }
    for(unsigned value=0;value<256;++value)for(unsigned status=0;status<256;++status)
    for(unsigned phase=0;phase<4;++phase){
        for(Hd63484 *v:{&a,&b}){
            v->control[3]=uint8_t(status);v->status=uint8_t(status);v->presentationBusy=status&1;
            auto fields=v->addressSelector();*fields.writePhase=phase&1;*fields.readPhase=phase&2;
            v->error=status&2?"synthetic existing fault":nullptr;
        }
        a.write8(2,uint8_t(value));b.control[3]=uint8_t(value);
        auto x=a.addressSelector(),y=b.addressSelector();
        check(a.control==b.control && a.parameter==b.parameter && a.statusNow()==b.statusNow() && a.irq()==b.irq(),"CCR low controls/status/IRQ");
        check(a.ar==b.ar && *x.writePhase==*y.writePhase && *x.readPhase==*y.readPhase && a.error==b.error,"CCR low phases and fault");
        // The retained pending command/result evolves across the entire matrix.
        // Sample the large full-VRAM serialization instead of copying terabytes.
        if(!(cases&4095))sameVideo(a,b);
        ++cases;
    }
    sameVideo(a,b);
    std::printf("PASS: %u CCR-low states with sampled full serialization; all old/new enables, status, phases, pending command/result and faults\n",cases);
}
static void videoAddressSelectors(){
    unsigned cases=0;
    for(unsigned value=0;value<256;++value)for(unsigned phase=0;phase<4;++phase){
        unsigned context=(value^phase)&15;Hd63484 a,b;
        for(Hd63484 *v:{&a,&b}){
            v->writeFifoWord(0x0400); // unfinished command must survive selection
            v->status=uint8_t(context*17);v->control[3]=uint8_t(value^0xa5);
            v->presentationBusy=context&1;
            auto fields=v->addressSelector();*fields.writePhase=phase&1;*fields.readPhase=phase&2;
            if(context&2)v->error="synthetic existing fault";
        }
        a.write8(0,uint8_t(value));
        auto fields=b.addressSelector();*fields.address=uint8_t(value);*fields.writePhase=*fields.readPhase=false;
        sameVideo(a,b);
        // Subsequent byte transfers must begin in the same phase, including
        // register auto-increment, FIFO selection and an unfinished command.
        for(Hd63484 *v:{&a,&b}){v->write8(2,0x12);v->write8(2,0x34);v->read8(2);}
        sameVideo(a,b);++cases;
    }
    std::printf("PASS: %u address-selector states and byte continuations, unchanged status/IRQ/pending command\n",cases);
}
static void inlineVideoHeaders(){
    Hd63484 a,b;
    for(unsigned opcode=0;opcode<65536;++opcode){
        // Independent pre-bridge decoder contract, not another production table.
        unsigned g=opcode>>10,allowed=0;
        if(g==2 || g==3)allowed=0x1f;
        else if(g==6 || g==7)allowed=0xf;
        else if(g==11 || g==19 || g==23)allowed=3;
        else if(g>=24 && g<=31)allowed=0x303;
        else if((g>=34 && g<=41) || g==48 || g==49 || g==51)allowed=0xff;
        else if(g>=42 && g<=50)allowed=0x1ff;
        else if(g>=52)allowed=0x3ff;
        check(Hd63484::formats[g].reserved==(0x3ff^allowed),"shared reserved-bit decoder contract");
        a.error=b.error=nullptr;
        uint16_t *wa=nullptr,*wb=nullptr;unsigned *ca=nullptr,*cb=nullptr;uint8_t *ha=nullptr,*hb=nullptr;int *la=nullptr,*lb=nullptr;
        check(a.inlineHeader(wa,ca,ha,la) && b.inlineHeader(wb,cb,hb,lb),"fresh header grant");
        a.writeFifoWord(uint16_t(opcode));
        if(!acceptHeader(b,uint16_t(opcode)))b.writeFifoWord(uint16_t(opcode));
        check(*ca==*cb && *la==*lb && *ha==*hb && (!*ca || *wa==*wb),"every header authoritative field");
        check(a.statusNow()==b.statusNow() && a.irq()==b.irq() && a.commands==b.commands &&
              a.parameter==b.parameter && a.rwp==b.rwp && bool(a.error)==bool(b.error),"every opcode result");
        if(!(opcode&1023))sameVideo(a,b);
        // Abort must clear a borrowed header exactly as an ordinary header.
        for(Hd63484 *v:{&a,&b}){v->write8(0,2);v->write8(2,0x80);v->write8(0,0);}
        check(*ca==*cb && *la==*lb,"ABT clears both header states");
        if(!(opcode&1023))sameVideo(a,b);
    }
    for(unsigned enable=0;enable<256;++enable){
        Hd63484 v(false);v.control[3]=uint8_t(enable);
        check(acceptHeader(v,0x0800)==!(enable&Hd63484::CED),"CED IRQ disables header borrowing");
    }
    for(unsigned barrier=0;barrier<6;++barrier){
        Hd63484 v(false);
        if(barrier==0)v.write8(2,8);
        if(barrier==1)v.ar=2;
        if(barrier==2)v.presentationBusy=true;
        if(barrier==3)v.error="test";
        if(barrier==4)v.writeFifoWord(0x0800);
        if(barrier==5)v.writeFifoWord(0x1800);
        check(!acceptHeader(v,0x0800),"protocol barrier prevents header grant");
    }
    puts("PASS inline headers: all 65536 opcode words/fields, sampled full state, ABT, all IRQ enables and protocol barriers");
}
static void inlineVideoParameters(){
    for(bool byteCounts:{false,true})for(unsigned count=0;count<=70;++count){
        Hd63484 reference,fast;reference.wptnCountsBytes=fast.wptnCountsBytes=byteCounts;
        std::vector<uint16_t> words={0x1800,uint16_t(count)};
        unsigned n=byteCounts?count/2:count;for(unsigned i=0;i<n;++i)words.push_back(uint16_t(i*71+0x8123));
        for(uint16_t value:words){
            reference.writeFifoWord(value);
            uint16_t *dest=nullptr;unsigned *pending=nullptr;uint8_t *high=nullptr;
            if(fast.inlineParameters(dest,pending,high)){*dest=value;++*pending;*high=uint8_t(value>>8);}
            else if(!acceptHeader(fast,value))fast.writeFifoWord(value);
            check(reference.statusNow()==fast.statusNow() && reference.irq()==fast.irq(),"inline parameter status/IRQ differs");
            check(bool(reference.error)==bool(fast.error),"inline parameter command error differs");
            if(reference.error){check(std::string(reference.error)==fast.error,"inline parameter error reason differs");break;}
            State a,b;reference.state(a);fast.state(b);check(a.bytes==b.bytes,"inline parameter preserves entire FIFO state at each word");
        }
    }
    Hd63484 v;uint16_t *dest=nullptr;unsigned *count=nullptr;uint8_t *high=nullptr;
    check(!v.inlineParameters(dest,count,high),"empty command cannot borrow");
    v.writeFifoWord(0x0400);check(v.inlineParameters(dest,count,high)==1,"ORG only permits intermediate parameter");
    v.write8(2,0x12);check(!v.inlineParameters(dest,count,high),"partial byte cannot borrow");
    v.write8(0,0);v.presentationBusy=true;check(!v.inlineParameters(dest,count,high),"held consumer cannot borrow");
    v.presentationBusy=false;v.write8(0,3);check(!v.inlineParameters(dest,count,high),"control write cannot borrow");
    puts("PASS inline FIFO parameters: per-word full state, variable counts/spills and protocol barriers");
}
static void pendingVideoState(){
    auto word=[](Hd63484 &v,unsigned n){v.write8(2,n>>8);v.write8(2,n);};
    for(unsigned split:{1u,2u,63u,64u,65u,141u})for(bool half:{false,true}){
        Hd63484 first;
        first.write8(0,2);first.write8(2,2);first.write8(0,0);
        word(first,0x0800);word(first,0x3333);
        std::vector<unsigned> words={0x9c03,70};
        for(unsigned i=0;i<70;++i){words.push_back(i&1?0xfff9:9);words.push_back(i&1?3:0xfffe);}
        for(unsigned i=0;i<split;++i)word(first,words[i]);
        if(half)first.write8(2,words[split]>>8);
        State saved;first.state(saved);Hd63484 second;word(second,0x0800);State loaded(saved.bytes);second.state(loaded);
        if(half){first.write8(2,words[split]);second.write8(2,words[split]);}
        for(unsigned i=split+unsigned(half);i<words.size();++i){word(first,words[i]);word(second,words[i]);}
        check(!first.error && !second.error,"restored inline/spilled polygon completes");
        State a,b;first.state(a);second.state(b);check(a.bytes==b.bytes,"pending command snapshot preserves complete continuation across inline/spill and half-word boundaries");
    }
    Hd63484 abort;
    word(abort,0x9c00);word(abort,70);for(unsigned i=0;i<65;++i)word(abort,0);
    abort.write8(0,2);abort.write8(2,0x82);abort.write8(0,0);
    word(abort,0x0800);word(abort,0x1234);
    check(!abort.error && abort.parameter[0]==0x1234 && (abort.statusNow()&Hd63484::CED),"ABT clears spilled command before next WPR");
}
static void fifoWordEquivalence(){
    auto bytes=[](Hd63484 &v,uint16_t word){v.write8(2,word>>8);v.write8(3,word);};
    unsigned checks=0;
    for(unsigned address=0;address<256;++address)for(bool half:{false,true}){
        Hd63484 byte,word;
        for(Hd63484 *v:{&byte,&word}){
            v->write8(0,0);if(half)v->write8(2,0x08);
            v->read8(2); // Keep the independent read-byte phase in the state.
            v->ar=address;
        }
        uint32_t random=0x7295+address;
        for(unsigned n=0;n<32;++n){
            random=random*1664525+1013904223;
            uint16_t value=n==0?0x0800:n==1?0x3333:uint16_t(random>>16);
            bytes(byte,value);if(!word.writeFifoWord(value))bytes(word,value);
            check(byte.ar==word.ar && byte.control==word.control && byte.parameter==word.parameter &&
                  byte.statusNow()==word.statusNow() && byte.commands==word.commands &&
                  bool(byte.error)==bool(word.error),"whole word matches each byte-protocol transition");
            if(byte.error)check(std::string(byte.error)==word.error,"same word protocol fault");
            ++checks;
        }
        // Snapshot rejects faulted devices. Compare the recorded fault above,
        // then encode the remaining state, including private byte latches.
        byte.error=word.error=nullptr;
        State a,b; a.bytes.reserve(2200000);b.bytes.reserve(2200000);
        byte.state(a);word.state(b);check(a.bytes==b.bytes,"whole-word complete state and partial-byte phases match");
    }
    for(bool half:{false,true}){
        Hd63484 a,b;
        for(Hd63484 *v:{&a,&b}){v->write8(0,2);v->write8(2,2);v->write8(0,0);}
        const uint16_t words[]={0x0800,0x3333,0x0400,0,0,0x1800,1,0,0x8000,3,4,0x8400,2,0xffff,0xcc00,0x0c00,0x4800,0x5aa5,0x4400};
        std::vector<uint8_t> stream;
        for(auto value:words){stream.push_back(value>>8);stream.push_back(value);}
        unsigned i=0;if(half){a.write8(2,stream[i]);b.write8(2,stream[i++]);}
        for(;i+1<stream.size();i+=2){bytes(a,uint16_t(stream[i]<<8|stream[i+1]));check(b.writeFifoWord(uint16_t(stream[i]<<8|stream[i+1])),"FIFO admission");}
        if(i<stream.size()){a.write8(2,stream[i]);b.write8(2,stream[i]);}
        check(!a.error && !b.error,"mixed complete/half word command stream");
        State x,y;a.state(x);b.state(y);check(x.bytes==y.bytes,"WPR/ORG/pattern/move/draw/read/write word semantics");
    }
    printf("PASS: %u whole-word byte transitions, all AR values and both byte phases, exact state and fault equivalence\n",checks);
}
// Setup must not queue the second status while the ROM is still processing
// the first. Exercise all three entry paths and each independent idle guard.
static void startupStatusPacing(){
    for(unsigned path=0;path<3;++path){
        Board b;b.peer.enabled=true;b.serial[0].control=0x95;b.memory[0x4142e]=0x61;
        Startup setup;setup.retained=path==0;
        setup.stage=path==0?Startup::Boot:path==1?Startup::Door:Startup::Close;
        b.memory[0x413f4]=path==1;
        unsigned emitted=0;
        auto emit=[&](unsigned kind,unsigned value,unsigned payload){
            check(kind==4,"status uses application input");
            check(value==(emitted?0x31:1) && payload==(emitted?(0x20000|SerialPeer::CabinetStatus):0x20000),"status order and payload");
            ++emitted;b.peer.enqueue({uint8_t(value),uint8_t((payload>>8)&0x7f),uint8_t(payload)});
        };
        setup.observe(0x2472);setup.step(b,emit);
        check(emitted==1 && b.peer.pending.size()==1,"setup must queue only first status");
        setup.observe(0x246a);setup.step(b,emit);
        check(emitted==1,"in-flight status blocks second status");
        b.peer.pending.clear();
        for(unsigned busy=0;busy<10;++busy){
            if(busy==0)b.peer.wire.push_back(0x50);
            if(busy==1)b.peer.state=3;
            if(busy==2)b.peer.assembling.push_back(0x71);
            if(busy==3)b.serial[0].receive.push_back(0x30);
            if(busy==4)b.serial[0].transmit.push_back(0x71);
            if(busy==5)b.serial[0].control=0xb5;
            if(busy==6)b.memory[0x4142e]=0x60;
            if(busy==7)b.memory[0x415db]=1;
            if(busy==8)b.memory[0x415df]=1;
            if(busy==9)b.peer.enabled=false;
            setup.step(b,emit);check(emitted==1,"ROM/peer busy must retain second status");
            b.peer=SerialPeer();b.peer.enabled=true;b.serial[0]=Acia6850();b.serial[0].control=0x95;
            b.memory[0x4142e]=0x61;b.memory[0x415db]=b.memory[0x415df]=0;
        }
        setup.step(b,emit);check(emitted==2 && b.peer.pending.size()==1,"idle admits second status exactly once");
        setup.step(b,emit);check(emitted==2,"second status is not duplicated");
    }
}

// Act as the ROM side of the actual framed link, including crossed requests.
static void drain(SerialPeer &p){
    std::deque<uint8_t> rx;
    while(!p.wire.empty())p.tick(1000,1000000,rx);
}
static void command(SerialPeer &p,unsigned header,unsigned count=256){
    feed(p,{0x30});drain(p);
    if(count==256)feed(p,{uint8_t(header)});
    else feed(p,{uint8_t(header),uint8_t(count)});
    drain(p);feed(p,{0x50});drain(p);
}
static unsigned event(SerialPeer &p){
    std::deque<uint8_t> rx;
    for(unsigned n=0;p.link()==SerialPeer::Idle && n<200;++n)p.tick(1000,1000000,rx);
    check(p.link()==SerialPeer::Request,"pending mechanism event starts a transfer");drain(p);
    unsigned result=p.pending.front()[0];feed(p,{0});drain(p);
    feed(p,{0x40});drain(p);feed(p,{0x50});
    return result;
}
static void coinHardware(){
    const unsigned commands[]={0x24,0x25,0x2c,0x2d,0x34};
    const unsigned events[]={5,6,13,14,21};
    for(unsigned i=0;i<5;++i){
        SerialPeer p;command(p,commands[i],3);
        for(unsigned j=0;j<3;++j)check(event(p)==events[i],"one correct sensor event per requested coin");
        check(p.pending.empty(),"exact requested count, no extra coins");
        command(p,commands[i],0);check(p.pending.empty(),"zero count is not a coin");
        command(p,commands[i],1);check(event(p)==events[i],"repeated session still pays a new request");
    }
    SerialPeer p;feed(p,{0x30});drain(p);feed(p,{0x24,2});drain(p);
    feed(p,{0x24,2});drain(p);check(p.pending.size()==2,"data retransmission does not duplicate payout");
    feed(p,{0x50});drain(p);
    check(event(p)==5,"first coin");
    auto delay=p.state>>8;check(delay==SerialPeer::CoinIntervalMs,"mechanical spacing begins after coin");
    command(p,0x16);check(event(p)==0x2e,"meter completion bypasses waiting coins");
    check((p.state>>8)>0 && (p.state>>8)<delay,"meter traffic does not erase mechanical spacing");
    check(event(p)==5 && p.pending.empty(),"remaining coin after meter");
    const unsigned meters[]={0x0e,0x16,0x26},acks[]={0x1e,0x2e,0x3e};
    for(unsigned i=0;i<3;++i){command(p,meters[i]);
        check(event(p)==acks[i],"accounting meter acknowledges each pulse");}
    command(p,0x24,2);command(p,0x09,1);check(p.pending.empty(),"stop cancels undelivered payout coins");
    // A request and the ROM request can cross on the full-duplex serial wire.
    SerialPeer crossed;crossed.enqueue({3});std::deque<uint8_t> rx;crossed.tick(1000,1000000,rx);drain(crossed);
    feed(crossed,{0x30});drain(crossed);feed(crossed,{0});drain(crossed);
    check(crossed.pending.size()==1,"crossed acknowledgement retains pending event");
    check(event(crossed)==3,"retained event can be delivered");
    SerialPeer malformed;malformed.transmit(0x24);malformed.transmit(0xdb);
    check(malformed.error,"missing payout count is a loud failure");
    // A save in mid-payout retains both transport and mechanical pacing.
    Board a,b;a.peer.enabled=true;command(a.peer,0x24,3);event(a.peer);
    State saved;a.state(saved);State restored(saved.bytes);b.state(restored);
    for(unsigned i=0;i<2;++i)check(event(a.peer)==event(b.peer),"restored payout emits identical remaining events");
    State x,y;a.state(x);b.state(y);check(x.bytes==y.bytes,"mid-payout snapshot round trip");
    puts("PASS: hopper counts/routes, retransmission, meters, pacing, stop, crossed link and payout snapshot");
}
int main()try{
    coinHardware();
    startupStatusPacing();
    videoIrqControlBytes();videoAddressSelectors();inlineVideoHeaders();inlineVideoParameters();pendingVideoState();fifoWordEquivalence();
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
        const std::vector<uint8_t> expected[]={{3},{1,0,0},{0x31,SerialPeer::CabinetStatus>>8,SerialPeer::CabinetStatus&255},{3}};
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
