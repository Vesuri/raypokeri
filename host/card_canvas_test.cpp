// Synthetic preparation-canvas check. Including the implementation keeps its
// startup-only canvas private in production and exposes no new device API.
#include "../src/board/CardBackCache.cpp"
#include <cstdio>
#include <cstring>
using namespace pokeri;
static bool scalar(Canvas &c,uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t color,unsigned op){
    if(!c.rectangles || stride!=608 || op)return false;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        uint32_t p=first+y*608+x;
        int delta=int(((p/4-((c.video.origin>>4)&c.video.frameMask)+0x20000)&0x3ffff))-0x20000;
        int n=21-delta,row=n>=0?n/152:-int((unsigned(-n)+151)/152);
        int col=(delta+row*152)*4+int(p&3);
        if(col<0 || col>=88 || row<0 || row>=100){c.invalid=true;continue;}
        unsigned i=unsigned(row*88+col);
        c.pixels[i]=(color>>((p&3)*4))&15;c.defined[i/8]|=1u<<(i&7);
    }
    return !c.invalid;
}
int main(){
    Hd63484 v;v.frameMask=0x3ffff;CardBackCache cache;
    Canvas actual(v,cache),expected(v,cache);unsigned cases=0;
    actual.rectangles=expected.rectangles=true;
    for(uint32_t origin:{0u,0x80000u,0xc0080000u})
    for(int x:{-1,0,1,2,3,7,8,15,16,79,80,87,88})
    for(int y:{-1,0,1,7,8,98,99,100})
    for(unsigned width:{0u,1u,7u,8u,16u,87u,88u,89u})
    for(unsigned height:{0u,1u,2u,7u,99u,100u,101u})
    for(uint16_t color:{uint16_t(0),uint16_t(0xffff),uint16_t(0x1234),uint16_t(0xa5a5)}){
        v.origin=origin;uint32_t first=((origin>>4)*4+x-y*608)&0xfffff;
        std::memset(actual.pixels,6,sizeof actual.pixels);std::memset(expected.pixels,6,sizeof expected.pixels);
        std::memset(actual.defined,0xa5,sizeof actual.defined);std::memset(expected.defined,0xa5,sizeof expected.defined);
        actual.invalid=expected.invalid=false;
        bool a=actual.fill(first,608,width,height,color,0),b=scalar(expected,first,608,width,height,color,0);
        // An invalid preparation canvas is discarded; only successful fills
        // expose pixel/coverage data. Rejected rectangles must agree on error.
        if(a!=b || actual.invalid!=expected.invalid || (a &&
           (std::memcmp(actual.pixels,expected.pixels,sizeof actual.pixels) ||
            std::memcmp(actual.defined,expected.defined,sizeof actual.defined)))){
            std::printf("FAIL origin=%x x=%d y=%d width=%u height=%u color=%x\n",origin,x,y,width,height,color);return 1;
        }
        ++cases;
    }
    std::printf("PASS: %u synthetic preparation rectangles, pixels/coverage/nibble phase/bounds/zero extents\n",cases);
}
