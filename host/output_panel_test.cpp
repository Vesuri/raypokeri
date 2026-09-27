#include "platform/amiga/OutputPanel.h"
#include "board/WordMath.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <climits>
int main(){
    static const uint8_t digits[8][5]={{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2}};
    std::array<uint16_t,283*160> expected,actual;
    for(unsigned n=0;n<512;++n){
        for(unsigned i=0;i<expected.size();++i)actual[i]=expected[i]=uint16_t(i*127+n);
        uint8_t latch[8];for(unsigned i=0;i<8;++i)latch[i]=n+i*31;
        unsigned bright=n<256?15:n&15,dark=n<256?0:(n>>4)&15;
        auto pixel=[&](unsigned x,unsigned y,unsigned color){
            uint16_t bit=0x8000u>>(x&15);
            for(unsigned p=0;p<4;++p){auto &word=expected[y*160+p*40+(x>>4)];word=(word&~bit)|((color&(1<<p))?bit:0);}
        };
        for(unsigned y=4;y<70;++y)for(unsigned x=534;x<608;++x)pixel(x,y,dark);
        for(unsigned row=0;row<8;++row){
            for(unsigned y=0;y<5;++y)for(unsigned x=0;x<3;++x)if(digits[row][y]&(4>>x))pixel(536+x,6+row*8+y,bright);
            for(unsigned b=0;b<8;++b)for(unsigned y=0;y<5;++y)for(unsigned x=0;x<5;++x)
                if((latch[row]&(1<<b)) || !x || !y || x==4 || y==4)pixel(544+b*8+x,6+row*8+y,bright);
        }
        pokeri::outputPanel(actual.data(),latch,bright,dark);assert(actual==expected);
    }
    const int values[]={INT_MIN,-65536,-32769,-32768,-1,0,1,32767,32768,65535,INT_MAX};
    for(int a:values)for(int b:values)assert(pokeri::coordinateProduct(a,b)==int64_t(a)*b);
    puts("PASS: 512 planar-panel cases against pixel oracle, complete buffer/edge preservation; signed coordinate products and wide fallback");
}
