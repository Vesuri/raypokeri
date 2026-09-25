#include "PlanarSurface.h"
namespace pokeri {
namespace {
// Two adjacent packed pixels become two bits in each of four planar nibbles.
// This is a format conversion table, independent of the game/ROM contents.
struct PairTable {uint16_t values[256];};
constexpr uint16_t pairBits(unsigned v){
    return ((v&1)<<13)|((v&16)<<8)|((v&2)<<8)|((v&32)<<3)|
           ((v&4)<<3)|((v&64)>>2)|((v&8)>>2)|((v&128)>>7);
}
// C++11-compatible compile-time indices; no native startup initialization.
template<unsigned... I> struct Indices {};
template<unsigned N,unsigned... I> struct Sequence:Sequence<N-1,N-1,I...> {};
template<unsigned... I> struct Sequence<0,I...> {using type=Indices<I...>;};
template<unsigned... I> constexpr PairTable makePairs(Indices<I...>){return {{pairBits(I)...}};}
constexpr PairTable pairs=makePairs(Sequence<256>::type{});
constexpr uint16_t spread[]={0,0x1000,0x100,0x1100,0x10,0x1010,0x110,0x1110,
    1,0x1001,0x101,0x1101,0x11,0x1011,0x111,0x1111};
}
uint16_t PlanarSurface::readWord(uint32_t a)const{
    uint16_t value=0;unsigned offset=12-((a&3)<<2);uint32_t word=a>>2;
    for(unsigned p=0;p<4;++p){
        value|=spread[(data[word]>>offset)&15]<<p;word+=planeWords;
    }
    return value;
}
void PlanarSurface::writeWord(uint32_t a,uint16_t value){
    unsigned offset=12-((a&3)<<2);uint16_t mask=uint16_t(15<<offset);uint32_t word=a>>2;
    uint16_t bits=uint16_t((pairs.values[value&255]<<2)|pairs.values[value>>8]);
    for(unsigned p=0;p<4;++p){
        data[word]=(data[word]&~mask)|((bits>>12)<<offset);
        bits<<=4;word+=planeWords;
    }
    changed=true;
}
uint16_t PlanarSurface::pixel4(uint32_t a,unsigned shift)const{
    unsigned bit=15-((a&3)<<2)-(shift>>2);uint32_t word=a>>2;uint16_t color=0;
    for(unsigned p=0;p<4;++p){color|=((data[word]>>bit)&1)<<p;word+=planeWords;}
    return color;
}
void PlanarSurface::plot4(uint32_t a,unsigned shift,unsigned color,unsigned op){
    uint16_t mask=uint16_t(1u<<(15-((a&3)<<2)-(shift>>2)));uint32_t word=a>>2;
    for(unsigned p=0;p<4;++p){uint16_t bits=(color&(1<<p))?mask:0;
        switch(op){case 0:data[word]=(data[word]&~mask)|bits;break;case 1:data[word]|=bits;break;
            case 2:data[word]&=uint16_t(~mask|bits);break;case 3:data[word]^=bits;break;}
        word+=planeWords;}
    changed=true;
}
}
