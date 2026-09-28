#ifndef POKERI_SHUFFLE_WAIT_H
#define POKERI_SHUFFLE_WAIT_H
#include <stdint.h>
namespace pokeri {
// User-approved presentation boundary, not an estimate of ACRTC execution time.
// The partial-copy helper's RTS preserves all registers/CCR. Only these callers
// belong to the two-pass sideways shuffle; other animations are not paced here.
struct ShuffleWait {
    static constexpr uint32_t pc=0x1e0c2;
    static bool caller(uint32_t address){
        switch(address){
        case 0x1dfdc:
        case 0x1dfe4:
        case 0x1dfec:
        case 0x1dff4:
        case 0x1dffc:
        case 0x1e004:
        case 0x1e014:
        case 0x1e022:
        case 0x1e028:
        case 0x1e036:
        case 0x1e03c:
        case 0x1e042:
        case 0x1e048:
        case 0x1e05e:
        case 0x1e06c:
            return true;
        default:return false;
        }
    }
    template<class Memory> static bool drained(const Memory &memory){
        // Original producer/consumer pointers used by the interrupt feeder at
        // $2E3A/$2E44. Compare their encoded bytes: native pointers relocate.
        for(unsigned i=0;i<4;++i)if(memory[0x41326+i]!=memory[0x4132a+i])return false;
        return true;
    }
};
}
#endif
