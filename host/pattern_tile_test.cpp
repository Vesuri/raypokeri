// Compare planar word construction against independent scalar pixel expansion.
#include "../src/board/Surface.h"
#include <array>
#include <cassert>
#include <cstdio>
using pokeri::PatternTile;
static void scalar(const PatternTile &t,uint16_t *out){
    for(unsigned i=0;i<160;++i)out[i]=0;
    unsigned py=t.point>>12;
    for(unsigned y=0;y<t.height;++y){
        unsigned px=(t.point>>4)&15;
        for(unsigned x=0;x<t.width;++x){
            bool bit=(t.rows[py]>>px)&1;
            if(!((t.mode==1 && !bit)||(t.mode==2 && bit))){
                unsigned dot=t.offset+x,index=(t.height-1-y)*2+(dot>>4);
                uint16_t mask=uint16_t(0x8000u>>(dot&15));
                unsigned color=(t.colors[bit]>>((dot&3)*4))&15;
                out[index]|=mask;
                for(unsigned plane=0;plane<4;++plane)if(color&(1<<plane))out[(plane+1)*32+index]|=mask;
            }
            if(++px>((t.end>>4)&15))px=(t.start>>4)&15;
        }
        if(++py>(t.end>>12))py=t.start>>12;
    }
}
int main(){
    uint32_t random=1;auto next=[&](){random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    unsigned cases=0;
    auto check=[&](PatternTile &t){
        assert(t.valid());std::array<uint16_t,162>a,b;a.fill(0xa55a);b.fill(0xa55a);
        t.expand(a.data()+1);scalar(t,b.data()+1);assert(a==b);
        std::array<uint16_t,258> inter;inter.fill(0x5a5a);
        t.expand<true>(inter.data()+1);
        assert(inter.front()==0x5a5a && inter.back()==0x5a5a);
        for(unsigned y=0;y<16;++y)for(unsigned p=0;p<4;++p)for(unsigned w=0;w<2;++w){
            assert(inter[1+y*8+p*2+w]==b[1+y*2+w]);
            assert(inter[129+y*8+p*2+w]==b[1+(p+1)*32+y*2+w]);
        }
        ++cases;
    };
    for(unsigned offset=0;offset<16;++offset)for(unsigned width=1;width<=16;++width)
    for(unsigned height=1;height<=16;++height)for(unsigned mode=0;mode<3;++mode){
        PatternTile t={};t.width=width;t.height=height;t.offset=offset;t.mode=mode;t.end=0xf0f0;
        for(auto &row:t.rows)row=next();t.colors[0]=next();t.colors[1]=next();check(t);
    }
    for(unsigned left=0;left<16;++left)for(unsigned right=left;right<16;++right)
    for(unsigned point=left;point<=right;++point)for(unsigned mode=0;mode<3;++mode)
    for(unsigned offset=0;offset<16;++offset){
        PatternTile t={};t.width=16;t.height=16;t.offset=offset;t.mode=mode;
        unsigned top=next()&15,bottom=top+(next()%(16-top)),py=top+(next()%(bottom-top+1));
        t.start=(top<<12)|(left<<4);t.end=(bottom<<12)|(right<<4);t.point=(py<<12)|(point<<4);
        for(auto &row:t.rows)row=next();t.colors[0]=next();t.colors[1]=next();check(t);
    }
    printf("PASS: %u pattern tiles, all alignments/dimensions/modes/window phases and patterned colours\n",cases);
}
