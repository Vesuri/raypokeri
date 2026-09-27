#include "Board.h"
namespace pokeri {
void Hd63484::state(State &s) {
    if(error)throw std::runtime_error("cannot snapshot a video fault");
    // Keep the snapshot wire format identical to the former vector storage.
    std::vector<uint16_t> pending;
    if(!s.reading)pending.assign(pendingData(),pendingData()+pendingCount);
    s.fields(ar,control,parameter,pattern,frame,frameMask,wptnCountsBytes,rwp,origin,status,commands,
             unexecuted,readUnderflows,writeLow,readLow,writeHigh,readLatch,pending,readFifo,drawingStopped,drawingWork);
    if(s.reading){
        clearPending();pendingCount=pending.size();
        for(unsigned i=0;i<pendingCount && i<64;++i)pendingWords[i]=pending[i];
        if(pendingCount>64)pendingSpill=std::move(pending);
        if(pendingCount){
            const uint16_t *words=pendingData();pendingLength=length(words[0]);
            if(pendingCount>=2 && pendingLength<0)
                pendingLength=pendingLength==-1?2+(wptnCountsBytes?words[1]/2:words[1]):2+2*words[1];
        }
    }
    if(frame.size()!=1u<<20 || frameMask>=frame.size() || (frameMask&(frameMask+1)))throw std::runtime_error("invalid video state");
}
void Board::state(State &s) {
    if(fault || peer.error)throw std::runtime_error("cannot snapshot a board fault");
    s.fields(config.cpuHz,config.systemHz,config.inputHz,config.watchdogMs,config.watchdogResetUs);
    if(!config.cpuHz)throw std::runtime_error("invalid state clock");
    for(size_t i=0x40000;i<memory.size();++i)s.value(memory[i]);
    s.value(nvram.bytes);
    for(auto &p:pia)s.fields(p.control,p.direction,p.output,p.input,p.flags);
    for(auto &a:serial)s.fields(a.control,a.receive,a.transmit);
    ay.state(s);
    if(ay.selected>15)throw std::runtime_error("invalid AY state");
    video.state(s);
    s.fields(peer.enabled,peer.state,peer.sentSequence,peer.receivedSequence,peer.phase,peer.assembling,peer.wire,peer.pending);
    s.fields(resetRequested,systemEdges,inputEdges,systemPhase,inputPhase,watchdogAge,outputLatches,latchData);
}
}
