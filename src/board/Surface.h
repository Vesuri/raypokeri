#ifndef POKERI_SURFACE_H
#define POKERI_SURFACE_H
#include <cstdint>
namespace pokeri {
// Storage/drawing boundary. Word accesses retain the ACRTC's host-bus layout;
// a platform is free to store the same bits in a different representation.
struct Surface {
    virtual ~Surface() {}
    virtual uint16_t readWord(uint32_t address) const=0;
    virtual void writeWord(uint32_t address,uint16_t value)=0;
    virtual uint16_t pixel4(uint32_t address,unsigned shift) const=0;
    virtual void plot4(uint32_t address,unsigned shift,unsigned color,unsigned op)=0;
    virtual bool fill(uint32_t,unsigned,unsigned,unsigned,uint16_t,unsigned){return false;}
    virtual bool copy(uint32_t,uint32_t,unsigned,unsigned,unsigned,unsigned){return false;}
};
}
#endif
