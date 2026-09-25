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
bool PlanarSurface::rectanglesOverlap(uint32_t first,uint32_t second,unsigned stride,
                                      unsigned width,unsigned height){
    // Compare sorted row intervals, not the enclosing linear address spans:
    // two side-by-side cards share a span but never share a pixel. No division.
    if(!width || !height)return false;
    unsigned a=0,b=0;
    while(a<height && b<height){
        if(first<second+width && second<first+width)return true;
        if(first<second){first+=stride;++a;}else{second+=stride;++b;}
    }
    return false;
}
void PlanarSurface::displayRegion(uint16_t *out,unsigned rowWords,unsigned planeStride,
                                 unsigned dx,unsigned dy,uint32_t source,unsigned stride,
                                 unsigned width,unsigned height,bool visible)const{
    if(!width || !height)return;
    unsigned count=((dx&15)+width+15)>>4,tail=(dx+width)&15;
    uint16_t first=uint16_t(0xffffu>>(dx&15));
    uint16_t last=tail?uint16_t(0xffffu<<(16-tail)):0xffff;
    int32_t bit=int32_t(source)-int32_t(dx&15);
    for(unsigned y=0;y<height;++y,bit+=stride){
        // Arithmetic right shift also handles the masked prefix before bit 0.
        int32_t start=bit>>4;unsigned shift=unsigned(bit)&15;
        for(unsigned p=0;p<4;++p){
            const uint16_t *src=data+p*planeWords;
            uint16_t *dst=out+uint32_t(uint16_t(dy+y))*uint16_t(rowWords)+p*planeStride+(dx>>4);
            for(unsigned w=0;w<count;++w){
                uint16_t mask=0xffff;if(!w)mask&=first;if(w+1==count)mask&=last;
                uint16_t value=0;int32_t a=start+int32_t(w);
                if(visible){
                    if(a>=0 && uint32_t(a)<planeWords)value=uint16_t(src[a]<<shift);
                    if(shift && a+1>=0 && uint32_t(a+1)<planeWords)value|=src[a+1]>>(16-shift);
                }
                dst[w]=(dst[w]&~mask)|(value&mask);
            }
        }
    }
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
