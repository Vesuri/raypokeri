#ifndef POKERI_PLANAR_SURFACE_H
#define POKERI_PLANAR_SURFACE_H
#include "Surface.h"
namespace pokeri {
// Four native bitplanes; no packed framebuffer or per-frame conversion.
// Addresses on Surface's drawing API are linear pixels, stride is in pixels.
class PlanarSurface : public Surface {
public:
    uint16_t *data=nullptr;
    uint32_t words=0,planeWords=0;
    bool changed=true;
    void attach(uint16_t *storage,uint32_t packedWords){data=storage;words=packedWords;planeWords=packedWords>>2;}
    uint16_t readWord(uint32_t address)const override;
    void writeWord(uint32_t address,uint16_t value)override;
    uint16_t pixel4(uint32_t address,unsigned shift)const override;
    void plot4(uint32_t address,unsigned shift,unsigned color,unsigned op)override;
};
}
#endif
