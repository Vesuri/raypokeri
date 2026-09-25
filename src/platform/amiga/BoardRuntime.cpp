#include <cstdint>
extern "C" void pokeriRuntimeFault(const char*);
extern "C" {
// 64-bit counters need wide operations; no 32-bit mul/div runtime is supplied.
uint64_t __muldi3(uint64_t a,uint64_t b){
    // Timer steps normally multiply a short cycle count by 50/100/1000.
    // Keep those exact products on MULU instead of paying for ten products.
    if(!((a|b)&0xffffffffffff0000ULL))return uint32_t(uint16_t(a))*uint16_t(b);
    if(!(a>>32) && !(b>>16)){
        return uint64_t(uint32_t(uint16_t(a))*uint16_t(b))+
            (uint64_t(uint32_t(uint16_t(a>>16))*uint16_t(b))<<16);
    }
    if(!(b>>32) && !(a>>16)){
        return uint64_t(uint32_t(uint16_t(b))*uint16_t(a))+
            (uint64_t(uint32_t(uint16_t(b>>16))*uint16_t(a))<<16);
    }
    // Ten native 16x16 products cover the low 64 bits. No 32-bit helper.
    uint16_t a0=a,a1=a>>16,a2=a>>32,a3=a>>48;
    uint16_t b0=b,b1=b>>16,b2=b>>32,b3=b>>48;
    uint64_t result=uint32_t(a0)*b0;
    result+=(uint64_t(uint32_t(a0)*b1)+uint32_t(a1)*b0)<<16;
    result+=(uint64_t(uint32_t(a0)*b2)+uint32_t(a1)*b1+uint32_t(a2)*b0)<<32;
    result+=(uint64_t(uint32_t(a0)*b3)+uint32_t(a1)*b2+uint32_t(a2)*b1+uint32_t(a3)*b0)<<48;
    return result;
}
uint64_t __udivdi3(uint64_t a,uint64_t b){
    if(!b)pokeriRuntimeFault("wide division by zero");
    uint64_t result=0,bit=1;
    while(!(b&(uint64_t(1)<<63)) && b<a){b<<=1;bit<<=1;}
    while(bit){if(a>=b){a-=b;result|=bit;}bit>>=1;b>>=1;}return result;
}
}
namespace pokeri {void boardMathFault(){pokeriRuntimeFault("HD63484 pattern divisor/range unsupported");}}
