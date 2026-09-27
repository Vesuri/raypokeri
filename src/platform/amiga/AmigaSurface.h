#ifndef POKERI_AMIGA_SURFACE_H
#define POKERI_AMIGA_SURFACE_H
#include "board/PlanarSurface.h"
class AmigaSurface : public pokeri::PlanarSurface {
public:
    bool prepare();
    void synchronize()const;
    void queued(){pending=true;}
    bool readPlanes4(uint32_t a,uint16_t *planes)const override;
    bool line4(uint32_t first,uint32_t wordMask,int rowStep,int dx,int dy,int sx,unsigned color,unsigned op)override;
    bool span4(uint32_t first,unsigned width,const uint16_t *colors,unsigned op)override;
    uint16_t readWord(uint32_t a)const override;
    void writeWord(uint32_t a,uint16_t value)override;
    uint16_t pixel4(uint32_t a,unsigned shift)const override;
    void plot4(uint32_t a,unsigned shift,unsigned color,unsigned op)override;
    bool selfTest();
    bool tested=false;
    void release();
    bool fill(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t pattern,unsigned op)override;
    bool copy(uint32_t from,uint32_t to,unsigned stride,unsigned width,unsigned height,unsigned op)override;
    // Copies a visible region into four external planes. Allocation bounds
    // include a preserved leading word for the left-shift prefetch column.
    bool displayBlit(uint16_t *out,uint16_t *begin,uint16_t *end,unsigned rowWords,unsigned planeStride,
                     unsigned dx,unsigned dy,uint32_t source,unsigned stride,unsigned width,unsigned height,bool visible);
    bool patternTile(uint32_t first,unsigned stride,const pokeri::PatternTile&,unsigned op)override;
    uint32_t fills=0,copies=0,patternHits=0,patternMisses=0;
    uint32_t copyRejectedBounds=0,copyRejectedOverlap=0,shiftedCopies=0,displayBlits=0;
private:
    static constexpr unsigned cacheSize=64;
    pokeri::PatternTile patternKeys[cacheSize];
    uint16_t *patternData=nullptr,*copyMasks=nullptr;
    bool blitPlanes(uint32_t source,unsigned stride,uint16_t *dest,uint16_t *begin,uint16_t *end,unsigned destStride,unsigned destPlane,unsigned offset,unsigned width,unsigned height,unsigned op,bool visible);
    unsigned patternCount=0,patternNext=0;
    mutable bool pending=false;
    bool fits(uint32_t first,unsigned stride,unsigned width,unsigned height)const;
};
#endif
