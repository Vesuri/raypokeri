#ifndef POKERI_AMIGA_SURFACE_H
#define POKERI_AMIGA_SURFACE_H
#include "board/PlanarSurface.h"
class AmigaSurface : public pokeri::PlanarSurface {
public:
    bool prepare();
    bool selfTest();
    bool tested=false;
    void release();
    bool fill(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t pattern,unsigned op)override;
    bool copy(uint32_t from,uint32_t to,unsigned stride,unsigned width,unsigned height,unsigned op)override;
    uint32_t fills=0,copies=0;
private:
    bool fits(uint32_t first,unsigned stride,unsigned width,unsigned height)const;
};
#endif
