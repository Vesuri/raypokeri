#ifndef POKERI_OUTPUT_PANEL_H
#define POKERI_OUTPUT_PANEL_H
#include <stdint.h>
namespace pokeri {
// 608-pixel (640-pixel padded rows), four-plane interleaved display. Touch only panel pixels;
// the six pixels preceding the first partial word remain unchanged.
inline void outputPanel(uint16_t *out,const uint8_t *latches,unsigned bright,unsigned dark){
    static const uint8_t digits[8][5]={{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2}};
    for(unsigned y=4;y<70;++y)for(unsigned p=0;p<4;++p){
        uint16_t *at=out+y*160+p*40+33;
        uint16_t bits=dark&(1<<p)?0xffff:0;
        *at=(*at&0xfc00)|(bits&0x03ff);++at;
        for(unsigned x=0;x<4;++x)*at++=bits;
    }
    auto mask=[&](unsigned word,unsigned y,uint16_t bits){
        for(unsigned p=0;p<4;++p){uint16_t &at=out[y*160+p*40+word];
            at=(at&~bits)|((bright&(1<<p))?bits:0);}
    };
    for(unsigned row=0;row<8;++row){
        for(unsigned y=0;y<5;++y)mask(33,6+row*8+y,uint16_t(digits[row][y])<<5);
        for(unsigned bit=0;bit<8;++bit)for(unsigned y=0;y<5;++y){
            uint16_t bits=(latches[row]&(1<<bit)) || y==0 || y==4?0xf800:0x8800;
            mask(34+(bit>>1),6+row*8+y,bits>>((bit&1)*8));
        }
    }
}
}
#endif
