#ifndef POKERI_AY_BACKEND_H
#define POKERI_AY_BACKEND_H
#include <cstdint>
namespace pokeri {
// Platform output consumes the same masked register writes as the reference.
// It owns oscillator/envelope output time; CPU-visible registers remain in AY.
struct AyBackend {
    virtual ~AyBackend() {}
    virtual void write(unsigned reg,uint8_t value)=0;
    virtual void tick(uint32_t cycles)=0;
};
}
#endif
