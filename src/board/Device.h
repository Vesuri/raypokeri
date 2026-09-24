#ifndef POKERI_DEVICE_H
#define POKERI_DEVICE_H
#include <cstdint>
namespace pokeri {
// Devices have no filesystem, clock, CPU-core or platform dependency.
struct Device {
    virtual ~Device() {}
    virtual uint8_t read8(unsigned offset) = 0;
    virtual void write8(unsigned offset, uint8_t value) = 0;
    virtual void tick(uint32_t cycles) = 0;
    virtual bool irq() const = 0;
};
}
#endif
