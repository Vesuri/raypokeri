// Local-ROM recipe proof. No original graphics data is stored in this source.
#include "../src/board/Hd63484.h"
#include "../amiga/generated/CardBackRecipe.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
using namespace pokeri;
struct Canvas : Surface {
    enum {Width=88,Height=100,Size=Width*Height};
    uint8_t pixels[Size]={},written[Size]={};
    mutable uint8_t dependent[Size]={};
    mutable unsigned dependencies=0,outside=0;
    uint32_t origin;
    unsigned index(uint32_t a,unsigned shift)const {
        int delta=int((a-origin+0x20000)&0x3ffff)-0x20000;
        int y=(-delta+21)/152;
        if(-delta+21<0 && (-delta+21)%152)--y;
        int x=(delta+y*152)*4+int(shift/4);
        if(x<0 || x>=Width || y<0 || y>=Height){++outside;return Size;}
        return unsigned(y*Width+x);
    }
    uint16_t readWord(uint32_t)const override{++outside;return 0;}
    void writeWord(uint32_t,uint16_t)override{++outside;}
    uint16_t pixel4(uint32_t a,unsigned shift)const override {
        unsigned i=index(a,shift);if(i==Size)return 0;
        if(!written[i]){++dependencies;dependent[i]=1;}
        return pixels[i];
    }
    void plot4(uint32_t a,unsigned shift,unsigned c,unsigned op)override {
        unsigned i=index(a,shift);if(i==Size)return;
        if(op && !written[i])++dependencies;
        if(op==0)pixels[i]=c;else if(op==1)pixels[i]|=c;
        else if(op==2)pixels[i]&=c;else if(op==3)pixels[i]^=c;
        else {++outside;return;}
        written[i]=1;
    }
};
static void render(Canvas &s,std::array<uint16_t,32> &parameters){
    using namespace card_recipe;
    Hd63484 v;
    v.origin=context[0];v.frameMask=context[1];v.rwp=context[2];v.status=context[3];
    for(unsigned i=0;i<32;++i)v.parameter[i]=context[4+i];
    for(unsigned i=0;i<16;++i)v.pattern[i]=context[36+i];
    for(unsigned i=0;i<256;++i)v.control[i]=context[52+i];
    v.surface=&s;s.origin=(v.origin>>4)&v.frameMask;
    for(unsigned c=0;c<79;++c){
        for(unsigned i=offsets[c];i<offsets[c+1];++i)v.writeFifoWord(words[i]);
        if(v.error){std::fprintf(stderr,"command %u: %s\n",c,v.error);std::exit(1);}
    }
    if(s.outside){std::fprintf(stderr,"scratch canvas bounds exceeded\n");std::exit(1);}
    parameters=v.parameter;
}
#include "../amiga/generated/CardBackSamples.h"
int main(int argc,char **argv){
    Canvas reference;std::array<uint16_t,32> expected;
    render(reference,expected);
    unsigned coverage=0,dependent=0,checked=0,rejected=0,realHits=0;
    for(unsigned i=0;i<Canvas::Size;++i){coverage+=reference.written[i];dependent+=reference.dependent[i];}
    auto check=[&](Canvas &s,bool real){
        bool eligible=true;uint8_t original[Canvas::Size];
        std::copy(s.pixels,s.pixels+Canvas::Size,original);
        for(unsigned i=0;i<Canvas::Size;++i)if(reference.dependent[i] && (s.pixels[i]==1 || s.pixels[i]==15))eligible=false;
        std::array<uint16_t,32> result;render(s,result);
        if(!eligible){++rejected;return;}
        if(real)++realHits;
        for(unsigned i=0;i<Canvas::Size;++i){
            uint8_t cached=reference.written[i]?reference.pixels[i]:original[i];
            if(s.pixels[i]!=cached || s.written[i]!=reference.written[i]){
                std::fprintf(stderr,"guarded cache differs at (%u,%u)\n",i%88,i/88);std::exit(1);
            }
        }
        if(result!=expected){std::fprintf(stderr,"guarded semantic state differs\n");std::exit(1);}
        ++checked;
    };
    for(unsigned bg=0;bg<16;++bg){Canvas s;std::fill(s.pixels,s.pixels+Canvas::Size,bg);check(s,false);}
    // Exhaust every colour at every pixel on which the reference path depends.
    for(unsigned i=0;i<Canvas::Size;++i)if(reference.dependent[i])for(unsigned c=0;c<16;++c){Canvas s;s.pixels[i]=c;check(s,false);}
    uint32_t rng=13;
    for(unsigned n=0;n<64;++n){Canvas s;
        for(unsigned i=0;i<Canvas::Size;++i){rng=rng*1664525u+1013904223u;unsigned c=rng>>28;
            if(reference.dependent[i] && (c==1 || c==15))c=0;s.pixels[i]=c;}
        check(s,false);
    }
    for(const char *background:cardBackgrounds){Canvas s;
        for(unsigned i=0;i<Canvas::Size;++i){char c=background[i];s.pixels[i]=c<='9'?c-'0':c-'a'+10;}
        check(s,true);
    }
    std::printf("card-back: coverage=%u, undefined reads=%u at %u pixels; guarded equality %u cases, fallback %u; actual backgrounds %u/%zu eligible\n",
        coverage,reference.dependencies,dependent,checked,rejected,realHits,sizeof(cardBackgrounds)/sizeof(*cardBackgrounds));
    if(argc>1 && std::string(argv[1])=="--require-independent" && dependent)return 2;
}
