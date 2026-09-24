#include "PlanarSurface.h"
namespace pokeri {
uint16_t PlanarSurface::readWord(uint32_t a)const{
    uint16_t value=0;unsigned offset=15-((a&3)<<2);uint32_t word=a>>2;
    for(unsigned p=0;p<4;++p){uint16_t bits=data[word];word+=planeWords;
        for(unsigned x=0;x<4;++x)value|=((bits>>(offset-x))&1)<<(x*4+p);}
    return value;
}
void PlanarSurface::writeWord(uint32_t a,uint16_t value){
    unsigned offset=12-((a&3)<<2);uint16_t mask=uint16_t(15<<offset);uint32_t word=a>>2;
    for(unsigned p=0;p<4;++p){unsigned bits=0;
        for(unsigned x=0;x<4;++x)bits=(bits<<1)|((value>>(x*4+p))&1);
        data[word]=(data[word]&~mask)|(bits<<offset);word+=planeWords;}
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
