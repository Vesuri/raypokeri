#ifndef POKERI_PLANAR_LAYOUT_H
#define POKERI_PLANAR_LAYOUT_H
#include "WordMath.h"
namespace pokeri {
// Map a logical 16-pixel word to plane zero. Native rows contain 38 words
// of each plane. Guest masking precedes this map; padding is never guest RAM.
struct PlanarLayout {
    bool interleaved=false;
    uint32_t planeStride=0;
    mutable uint32_t cachedRow=0,cachedNative=0;
    void layout(uint32_t planeWords,bool rows){
        interleaved=rows;planeStride=rows?38:planeWords;cachedRow=cachedNative=0;
    }
    uint32_t storageWord(uint32_t q)const {
        if(!interleaved)return q;
        if(q>=cachedRow && q-cachedRow<38)return cachedNative+q-cachedRow;
        if(q>=cachedRow+38 && q-cachedRow<76){cachedRow+=38;cachedNative+=152;}
        else if(cachedRow>=38 && q<cachedRow && cachedRow-q<=38){cachedRow-=38;cachedNative-=152;}
        else {
            // The ACRTC has at most 2 MB: q/38 fits a native word.
            unsigned row=rowOf(q);
            cachedRow=wordProduct(uint16_t(row),38);cachedNative=cachedRow<<2;
        }
        return cachedNative+q-cachedRow;
    }
    static unsigned rowOf(uint32_t q){
#ifdef __m68k__
        uint16_t divisor=38;
        __asm volatile("divu.w %1,%0":"+d"(q):"d"(divisor):"cc");
        return uint16_t(q);
#else
        return q/38;
#endif
    }
    static uint32_t storageWords(uint32_t packedWords,bool rows){
        return rows?wordProduct(uint16_t(rowOf((packedWords>>2)-1)+1),152):packedWords;
    }
};
}
#endif
