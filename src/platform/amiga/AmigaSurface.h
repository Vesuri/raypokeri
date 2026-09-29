#ifndef POKERI_AMIGA_SURFACE_H
#define POKERI_AMIGA_SURFACE_H
#include "board/PlanarSurface.h"
#ifdef POKERI_CARD_DAMAGE
#include "../CardDamage.h"
#endif
class AmigaSurface : public pokeri::PlanarSurface {
public:
#ifdef POKERI_CARD_DAMAGE
    bool boundedCards=true;
    pokeri::CardDamage dirtyCard;
    void damageCard(uint32_t first)override{if(boundedCards)dirtyCard.include(first,changed);else changed=true;}
#endif
#ifdef POKERI_CARD_OBSERVER
    void (*pixelObserver)()=nullptr;
#endif
    bool cardBlitFits(uint32_t first)const override{return interleaved && PlanarSurface::cardBlitFits(first);}
    bool cardBlit(uint32_t first,const uint16_t *image,const uint16_t *mask)override;
    bool cardBlitTest();
    bool cardTested=false;
    uint32_t cardTestFailure=0;
    uint32_t cardBlits=0;
    uint32_t allocatedWords=0;
    bool prepare();
    void synchronize()const;
    void queued(){pending=true;
#ifdef POKERI_READ_ONLY_DMA
        pendingWrites=true;
#endif
    }
    bool cpuAccess4(pokeri::CpuPlanes &out)override;
    bool readPlanes4(uint32_t a,uint16_t *planes)const override;
    bool copy180(uint32_t from,uint32_t to,unsigned stride,unsigned width,unsigned height,unsigned op)override;
    bool line4(uint32_t first,uint32_t wordMask,int rowStep,int dx,int dy,int sx,unsigned color,unsigned op)override;
    bool curve4(uint32_t base,uint32_t wordMask,unsigned rowWords,const pokeri::CurveWord *runs,unsigned count,uint16_t color,unsigned op)override;
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
#ifdef POKERI_PATTERN_INTERLEAVED
    static constexpr unsigned patternWords=256;
#else
    static constexpr unsigned patternWords=160;
#endif
    pokeri::PatternTile patternKeys[cacheSize];
    uint16_t *patternData=nullptr,*copyMasks=nullptr;
    bool blitPlanes(uint32_t source,unsigned stride,uint16_t *dest,uint16_t *begin,uint16_t *end,unsigned destStride,unsigned destPlane,unsigned offset,unsigned width,unsigned height,unsigned op,bool visible);
    unsigned patternCount=0,patternNext=0;
    mutable bool pending=false;
#ifdef POKERI_READ_ONLY_DMA
    mutable bool pendingWrites=false;
    void synchronizeRead()const;
#else
    void synchronizeRead()const{synchronize();}
#endif
    bool rowFits(uint32_t first,unsigned width)const;
    bool fits(uint32_t first,unsigned stride,unsigned width,unsigned height)const;
};
#endif
