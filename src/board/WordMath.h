#ifndef POKERI_WORD_MATH_H
#define POKERI_WORD_MATH_H
#include <cstdint>
namespace pokeri {
// HD63484 pattern divisors are 1..256; the dividend may span a signed
// coordinate difference. Use the 68000's native 32-by-16 DIVU, not libgcc.
inline int patternRemainder(int n,int divisor){
    // Most ROM patterns use power-of-two extents. Unsigned masking gives
    // the same nonnegative remainder for negative coordinates as well.
    if(divisor>0 && divisor<=256 && !(unsigned(divisor)&unsigned(divisor-1)))
        return uint32_t(n)&unsigned(divisor-1);
#ifdef __m68k__
    extern void boardMathFault();
    if(divisor<=0 || divisor>256) {boardMathFault();return 0;}
    if(divisor==1)return 0;
    uint32_t magnitude=n<0?0u-uint32_t(n):uint32_t(n);
    uint16_t d=divisor;
    // Reduce the upper word first, so DIVU cannot overflow even for long
    // patterned polylines. The second dividend has a high word below d.
    if((magnitude>>16)>=unsigned(divisor)){
        uint32_t high=magnitude>>16;
        __asm volatile("divu.w %1,%0" : "+d"(high) : "d"(d) : "cc");
        magnitude=(high&0xffff0000u)|(magnitude&65535);
    }
    __asm volatile("divu.w %1,%0" : "+d"(magnitude) : "d"(d) : "cc");
    int result=magnitude>>16;
    return n<0 && result?divisor-result:result;
#else
    int result=n%divisor;return result<0?result+divisor:result;
#endif
}
inline unsigned wordQuotient(uint16_t n,uint16_t d){
    if(d==1)return n;
#ifdef __m68k__
    uint32_t result=n;__asm volatile("divu.w %1,%0" : "+d"(result) : "d"(d) : "cc");return uint16_t(result);
#else
    return n/d;
#endif
}
}
#endif
