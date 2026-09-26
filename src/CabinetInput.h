#ifndef POKERI_CABINET_INPUT_H
#define POKERI_CABINET_INPUT_H
#include "board/Board.h"
namespace pokeri {
// Observed ROM transport state, shared with cold operator setup. Empty peer
// queues alone do not mean the ROM has finished its outgoing application data.
inline bool cabinetLinkIdle(const Board &b){
    const auto &m=b.memory;
    auto zero=[&](unsigned a){return !(m[a]|m[a+1]|m[a+2]|m[a+3]);};
    return b.peer.enabled && b.peer.pending.empty() && b.peer.wire.empty() &&
        b.peer.state==0 && b.peer.assembling.empty() && b.serial[0].receive.empty() &&
        b.serial[0].transmit.empty() && (b.serial[0].control&0x60)!=0x20 &&
        m[0x4142e]==0x61 && zero(0x415d8) && zero(0x415dc);
}
// Keep external key edges until the link can accept a new application packet.
// This supplies no game state or acknowledgement; the original ROM does both.
struct CabinetInput {
    std::deque<uint8_t> pending;
    void coin(){pending.push_back(3);}
    void status(){pending.push_back(1);pending.push_back(0x31);}
    void step(Board &b){
        if(pending.empty() || !cabinetLinkIdle(b))return;
        unsigned kind=pending.front();pending.pop_front();
        if(kind==3)b.peer.enqueue({3});
        else if(kind==1)b.peer.enqueue({1,0,0});
        else b.peer.enqueue({0x31,1,0});
    }
};
}
#endif
