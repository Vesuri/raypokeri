#ifndef POKERI_PLANAR_SURFACE_H
#define POKERI_PLANAR_SURFACE_H
#include "Surface.h"
namespace pokeri {
// Four native bitplanes; no packed framebuffer or per-frame conversion.
// Addresses on Surface's drawing API are linear pixels, stride is in pixels.
class PlanarSurface : public Surface, public PlanarLayout {
public:
    uint16_t *data=nullptr;
    uint32_t words=0,planeWords=0;
    bool changed=true;
    void attach(uint16_t *storage,uint32_t packedWords,bool rows=false){
        data=storage;words=packedWords;planeWords=packedWords>>2;layout(planeWords,rows);
    }
    static bool rectanglesOverlap(uint32_t first,uint32_t second,unsigned stride,
                                  unsigned width,unsigned height);
    // Compose a validated pixel rectangle into four interleaved display planes.
    // Source and destination must be disjoint; caller drains pending DMA first.
    void displayRegion(uint16_t *out,unsigned rowWords,unsigned planeStride,
                       unsigned dx,unsigned dy,uint32_t source,unsigned stride,
                       unsigned width,unsigned height,bool visible)const;
    bool cpuAccess4(CpuPlanes &out)override{out.data=data;out.planeWords=planeWords;out.changed=&changed;out.layout(planeWords,interleaved);return true;}
    bool readPlanes4(uint32_t address,uint16_t *planes)const override;
    bool copy180(uint32_t from,uint32_t to,unsigned stride,unsigned width,unsigned height,unsigned op)override;
    bool line4(uint32_t first,uint32_t wordMask,int rowStep,int dx,int dy,int sx,unsigned color,unsigned op)override;
    bool curve4(uint32_t base,uint32_t wordMask,unsigned rowWords,const pokeri::CurveWord *runs,unsigned count,uint16_t color,unsigned op)override;
    bool smallFill4(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t color,unsigned op);
    bool span4(uint32_t first,unsigned width,const uint16_t *colors,unsigned op)override;
    uint16_t readWord(uint32_t address)const override;
    void writeWord(uint32_t address,uint16_t value)override;
    uint16_t pixel4(uint32_t address,unsigned shift)const override;
    void plot4(uint32_t address,unsigned shift,unsigned color,unsigned op)override;
};
}
#endif
