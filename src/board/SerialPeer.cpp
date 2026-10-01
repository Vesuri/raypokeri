#include "WordMath.h"
#include "SerialPeer.h"
namespace pokeri {
void SerialPeer::send(std::vector<uint8_t> p) {
    unsigned sum=0;for(auto b:p){sum+=b;wire.push_back(b);}wire.push_back(uint8_t(~sum)|0x80);
}
void SerialPeer::enqueue(std::vector<uint8_t> p) {
    if(p.empty() || p.size()>3 || p[0]>=0x40){error="invalid serial peer application packet";return;}
    for(auto b:p)if(b&0x80){error="serial payload has terminator bit";return;}
    pending.push_back(p);
}
void SerialPeer::transmit(uint8_t byte) {
    if(assembling.size()==4){error="serial packet exceeds four bytes";return;}
    assembling.push_back(byte);if(!(byte&0x80))return;
    unsigned sum=0;for(unsigned i=0;i+1<assembling.size();++i)sum+=assembling[i];
    if(assembling.size()<2 || uint8_t(uint8_t(~sum)|0x80)!=byte){error="serial transmit checksum";return;}
    auto p=assembling;assembling.clear();p.pop_back();
    if(packetLog)packetLog(p);
    unsigned h=p[0];
    if(p.size()==1 && h==0x30) {
        // The ROM can request transmission while we are requesting it too.
        // Yield the link, retaining our pending event for the next session.
        if(link()==Idle || link()==Request){link(Receive);receivedSequence|=0x80;}
        send({uint8_t(receivedSequence&0x40)});return;
    }
    if(p.size()==1 && h==0x50) {
        if(link()==End)link(Idle);
        else if(link()==CoinEnd)state=CoinIntervalMs<<8;
        else {link(Idle);send({0x50});}
        return;
    }
    if(link()==Request && p.size()==1 && (h==0 || h==0x40)) {
        sentSequence=h^0x40;auto data=pending.front();data[0]|=sentSequence;send(data);link(Data);return;
    }
    if(link()==Data && p.size()==1 && h==sentSequence) {
        const unsigned event=pending.front()[0];
        const bool sensor=coinEvent(event);
        pending.pop_front();send({0x50});link(sensor?CoinEnd:End);return;
    }
    // A crossed request may leave the ROM's acknowledgement arriving after
    // we yielded. Close that abandoned transfer without consuming an event.
    if(link()==Receive && p.size()==1 && (h==0 || h==0x40)) {
        link(Idle);send({0x50});return;
    }
    // Transport retransmissions must not dispense the same coins twice.
    if(p.size()==1 && (h==0 || h==0x40 || h==0x28 || h==0x68)) {
        error="unexpected serial peer acknowledgement";return;
    }
    if(link()==Data || link()==End || link()==CoinEnd){
        error="serial application packet during peer transfer";return;
    }
    const bool fresh=link()!=Receive || receivedSequence!=(h&0x40);
    receivedSequence=h&0x40;link(Receive);send({receivedSequence});
    const unsigned command=h&0x3f;
    if(command==0x0e || command==0x16 || command==0x26){
        if(p.size()!=1){error="invalid accounting meter command length";return;}
        // Meter pulses complete independently of the mechanical hopper.
        // Service these before the remaining queued coins.
        if(fresh)pending.push_front({uint8_t(command==0x0e?0x1e:command==0x16?0x2e:0x3e)});
        return;
    }
    if(command==9 && p.size()==2 && p[1]==1 && fresh){
        // $BB36 clears all ROM payout counters after sending this stop.
        for(unsigned left=pending.size();left;--left){
            auto event=pending.front();pending.pop_front();
            if(!coinEvent(event[0]))pending.push_back(event);
        }
        return;
    }
    unsigned sensor=0;
    switch(command){
    case 0x24:sensor=0x05;break; // hopper 1 -> player
    case 0x25:sensor=0x06;break; // hopper 1 -> cashbox
    case 0x2c:sensor=0x0d;break; // hopper 2 -> player
    case 0x2d:sensor=0x0e;break; // hopper 2 -> cashbox
    case 0x34:sensor=0x15;break; // hopper 3 -> player
    }
    if(sensor){
        if(p.size()!=2){error="invalid hopper command length";return;}
        if(fresh)for(unsigned coin=0;coin<p[1];++coin)enqueue({uint8_t(sensor)});
    }
    // Unknown application commands retain transport-only handling; no guessed
    // status/result is supplied. The ROM owns inventory and payout accounting.

}
void SerialPeer::tick(uint32_t cycles,uint32_t cpuHz,std::deque<uint8_t>& rx) {
    phase+=wideProduct32(cycles,1000);
    while(phase>=cpuHz){
        phase-=cpuHz;
        // INFERRED successful mechanism: at most ten coin pulses/second.
        // Encoded in the existing serial state so snapshots retain the delay.
        if(state>>8)state-=0x100;
        if(!wire.empty()){rx.push_back(wire.front());wire.pop_front();}
        else if(link()==Idle && !pending.empty() &&
                (!(state>>8) || !coinEvent(pending.front()[0]))){send({0x30});link(Request);}
    }
}
}
