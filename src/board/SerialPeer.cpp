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
    if(p.size()==1 && h==0x30) { send({receivedSequence});return; }
    if(p.size()==1 && h==0x50) { if(state==3)state=0;else send({0x50});return; }
    if(state==1 && p.size()==1 && (h==0 || h==0x40)) {
        sentSequence=h^0x40;auto data=pending.front();data[0]|=sentSequence;send(data);state=2;return;
    }
    if(state==2 && p.size()==1 && h==sentSequence) {
        pending.pop_front();send({0x50});state=3;return;
    }
    // Acknowledge application data at the transport layer only. No inferred
    // hardware result or status is generated in response to an unknown command.
    if(p.size()==1 && (h==0 || h==0x40 || h==0x28 || h==0x68)) {
        error="unexpected serial peer acknowledgement";return;
    }
    receivedSequence=h&0x40;send({receivedSequence});
}
void SerialPeer::tick(uint32_t cycles,uint32_t cpuHz,std::deque<uint8_t>& rx) {
    phase+=uint64_t(cycles)*1000;
    while(phase>=cpuHz){
        phase-=cpuHz;
        if(!wire.empty()){rx.push_back(wire.front());wire.pop_front();}
        else if(!state && !pending.empty()){send({0x30});state=1;}
    }
}
}
