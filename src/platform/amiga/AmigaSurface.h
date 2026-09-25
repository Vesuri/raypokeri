#ifndef POKERI_AMIGA_SURFACE_H
#define POKERI_AMIGA_SURFACE_H
#include "board/PlanarSurface.h"
class AmigaSurface : public pokeri::PlanarSurface {
public:
    bool prepare();
    void synchronize()const;
    void queued(){pending=true;}
    uint16_t readWord(uint32_t a)const override;
    void writeWord(uint32_t a,uint16_t value)override;
    uint16_t pixel4(uint32_t a,unsigned shift)const override;
    void plot4(uint32_t a,unsigned shift,unsigned color,unsigned op)override;
    bool selfTest();
    bool tested=false;
    void release();
    bool fill(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t pattern,unsigned op)override;
    bool copy(uint32_t from,uint32_t to,unsigned stride,unsigned width,unsigned height,unsigned op)override;
    bool patternTile(uint32_t first,unsigned stride,const pokeri::PatternTile&,unsigned op)override;
    uint32_t fills=0,copies=0,patternHits=0,patternMisses=0;
private:
    static constexpr unsigned cacheSize=64;
    pokeri::PatternTile patternKeys[cacheSize];
    uint16_t *patternData=nullptr;
    unsigned patternCount=0,patternNext=0;
    mutable bool pending=false;
    bool fits(uint32_t first,unsigned stride,unsigned width,unsigned height)const;
};
#endif
