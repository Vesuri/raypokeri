#ifndef POKERI_SERIAL_PEER_H
#define POKERI_SERIAL_PEER_H
#include <cstdint>
#include <deque>
#include <vector>
namespace pokeri {
// ROM-derived link transport, successful payout sensors and accounting meters.
// Other application input (coins, cabinet status) is supplied externally.
struct SerialPeer {
    bool enabled=false;
    // Low byte: transport; upper bits: remaining mechanical cooldown in ms.
    // Both are in the existing serialized state word (old idle snapshots work).
    enum Link {Idle,Request,Data,End,Receive,CoinEnd};
    static constexpr unsigned CoinIntervalMs=100; // INFERRED, not calibrated
    static constexpr unsigned CabinetStatus=0x0302; // input enable + transfer inhibits
    unsigned state=Idle;
    unsigned link()const{return state&255;}
    void link(unsigned value){state=(state&~255u)|value;}
    static bool coinEvent(unsigned event){return event==5 || event==6 || event==13 || event==14 || event==21;}
    uint8_t sentSequence=0x40, receivedSequence=0x80; // sentinel: no application received yet
    uint64_t phase=0;
    std::vector<uint8_t> assembling;
    std::deque<uint8_t> wire;
    std::deque<std::vector<uint8_t>> pending;
    const char *error=nullptr;
    void (*packetLog)(const std::vector<uint8_t>&)=nullptr;
    void transmit(uint8_t byte);
    void enqueue(std::vector<uint8_t> packet);
    void tick(uint32_t cycles,uint32_t cpuHz,std::deque<uint8_t>& rx);
    void send(std::vector<uint8_t> packet);
};
}
#endif
