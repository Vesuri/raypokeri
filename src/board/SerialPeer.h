#ifndef POKERI_SERIAL_PEER_H
#define POKERI_SERIAL_PEER_H
#include <cstdint>
#include <deque>
#include <vector>
namespace pokeri {
// Explicit diagnostic peer: implements the observed link transport, not a coin
// mechanism's application logic. Application packets must be supplied externally.
struct SerialPeer {
    bool enabled=false;
    unsigned state=0;
    uint8_t sentSequence=0x40, receivedSequence=0;
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
