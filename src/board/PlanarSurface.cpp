#include "PlanarSurface.h"
#include "WordMath.h"
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
struct ReverseTable {uint8_t values[256];};
constexpr uint8_t reverseByte(unsigned v){
    return ((v&1)<<7)|((v&2)<<5)|((v&4)<<3)|((v&8)<<1)|
           ((v&16)>>1)|((v&32)>>3)|((v&64)>>5)|((v&128)>>7);
}
template<unsigned... I> constexpr ReverseTable makeReverse(Indices<I...>){return {{reverseByte(I)...}};}
constexpr ReverseTable reversed=makeReverse(Sequence<256>::type{});
uint16_t reverseWord(uint16_t value){return uint16_t(reversed.values[value&255]<<8)|reversed.values[value>>8];}
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
            const uint16_t *src=data+p*this->planeStride;
            uint16_t *dst=out+uint32_t(uint16_t(dy+y))*uint16_t(rowWords)+p*planeStride+(dx>>4);
            for(unsigned w=0;w<count;++w){
                uint16_t mask=0xffff;if(!w)mask&=first;if(w+1==count)mask&=last;
                uint16_t value=0;int32_t a=start+int32_t(w);
                if(visible){
                    if(a>=0 && uint32_t(a)<planeWords)value=uint16_t(src[storageWord(a)]<<shift);
                    if(shift && a+1>=0 && uint32_t(a+1)<planeWords)value|=src[storageWord(a+1)]>>(16-shift);
                }
                dst[w]=(dst[w]&~mask)|(value&mask);
            }
        }
    }
}
bool PlanarSurface::readPlanes4(uint32_t a,uint16_t *planes)const{
    const uint16_t *p=data+storageWord(a>>2);
    planes[0]=*p;p+=planeStride;planes[1]=*p;p+=planeStride;
    planes[2]=*p;p+=planeStride;planes[3]=*p;
    return true;
}
namespace {
struct LineWord {uint32_t address;uint16_t mask;};
// Select each plane's operation once per bounded batch. This keeps the 68000
// compiler from calling a captured lambda and saving registers at every point.
void lineWords(uint16_t *plane,uint32_t planeStride,const LineWord *runs,unsigned count,unsigned color,unsigned op){
    for(unsigned p=0;p<4;++p,plane+=planeStride,color>>=1){
        unsigned mode=op==0?(color&1?1:2):op==2?(color&1?0:2):(color&1?op:0);
        switch(mode){
        case 1:for(unsigned i=0;i<count;++i)plane[runs[i].address]|=runs[i].mask;break;
        case 2:for(unsigned i=0;i<count;++i)plane[runs[i].address]&=uint16_t(~runs[i].mask);break;
        case 3:for(unsigned i=0;i<count;++i)plane[runs[i].address]^=runs[i].mask;break;
        }
    }
}
}
bool PlanarSurface::line4(uint32_t first,uint32_t wordMask,int rowStep,int dx,int dy,int sx,unsigned color,unsigned op){
    if(wordMask>=planeWords || (first>>4)>wordMask || op>3 || color>15)return false;
    const int major=dx>dy?dx:dy,minor=dx>dy?dy:dx;
    int err=2*minor-major;uint32_t word=first>>4;
    uint16_t bit=uint16_t(0x8000u>>(first&15)),mask=0;
    LineWord runs[128];unsigned count=0;
    for(int i=0;i<major;++i){
        // XOR must retain repeated physical pixels when VRAM rows alias.
        if(op==3)mask^=bit;else mask|=bit;
        uint32_t previous=word;bool diagonal=err>=0;
        if(diagonal)err-=2*major;
        if(dx>dy || diagonal){
            if(sx>0){bit>>=1;if(!bit){bit=0x8000;++word;}}
            else {bit<<=1;if(!bit){bit=1;--word;}}
        }
        if(dx<=dy || diagonal)word+=rowStep;
        word&=wordMask;err+=2*minor;
        if(word!=previous){
            runs[count++]={storageWord(previous),mask};mask=0;
            if(count==128){lineWords(data,planeStride,runs,count,color,op);count=0;}
        }
    }
    if(mask)runs[count++]={storageWord(word),mask};
    if(count)lineWords(data,planeStride,runs,count,color,op);
    if(major)changed=true;
    return true;
}
bool PlanarSurface::curve4(uint32_t base,uint32_t wordMask,unsigned rowWords,const CurveWord *runs,unsigned count,uint16_t color,unsigned op){
    if(wordMask>=planeWords || base>wordMask || rowWords>16383 || op>3)return false;
    uint16_t colors[4];colorPlanes4(color,colors);
    LineWord batch[128];
    while(count){
        unsigned n=count<128?count:128;
        for(unsigned i=0;i<n;++i){
            int32_t row=int32_t(runs[i].y)*int16_t(rowWords);
            batch[i]={storageWord((base+uint32_t(int32_t(runs[i].x))-uint32_t(row))&wordMask),runs[i].mask};
        }
        uint16_t *plane=data;
        for(unsigned p=0;p<4;++p,plane+=planeStride){
            const uint16_t bits=colors[p];
            switch(op){
            case 0:for(unsigned i=0;i<n;++i){auto &d=plane[batch[i].address];d=(d&~batch[i].mask)|(bits&batch[i].mask);}break;
            case 1:for(unsigned i=0;i<n;++i)plane[batch[i].address]|=bits&batch[i].mask;break;
            case 2:for(unsigned i=0;i<n;++i)plane[batch[i].address]&=uint16_t(~batch[i].mask|bits);break;
            case 3:for(unsigned i=0;i<n;++i)plane[batch[i].address]^=bits&batch[i].mask;break;
            }
        }
        runs+=n;count-=n;
    }
    changed=true;return true;
}
bool PlanarSurface::copy180(uint32_t from,uint32_t to,unsigned stride,unsigned width,unsigned height,unsigned op){
    if(!width || !height || width>stride || stride>65535 || height>65535 || op>3)return false;
    uint32_t rows=wordProduct(uint16_t(height-1),uint16_t(stride));
    if(from>=words*4 || to>=words*4 || rows+width>words*4-from || rows+width>words*4-to ||
       rectanglesOverlap(from,to,stride,width,height))return false;
    // The caller has excluded coordinate/VRAM wrap. Disjoint rectangles allow
    // plane/row reordering; overlap retains the device's sequential pixel path.
    const uint16_t *source=data;uint16_t *dest=data;
    PlanarLayout sourceMap=*this,destMap=*this;
    for(unsigned p=0;p<4;++p,source+=planeStride,dest+=planeStride){
        uint32_t srcRow=from+rows,dstRow=to;
        for(unsigned y=0;y<height;++y,srcRow-=stride,dstRow+=stride){
            uint32_t right=srcRow+width-1,outWord=dstRow>>4;
            unsigned remaining=width,offset=dstRow&15;
            while(remaining){
                unsigned count=remaining<16-offset?remaining:16-offset,end=right&15;
                uint16_t value=uint16_t(unsigned(reverseWord(source[sourceMap.storageWord(right>>4)]))<<(15-end));
                // Read a preceding word only when requested pixels cross it;
                // masked source padding must never read before the allocation.
                if(count>end+1)value|=reverseWord(source[sourceMap.storageWord((right>>4)-1)])>>(end+1);
                uint16_t mask=uint16_t((0xffffu>>offset)&(0xffffu<<(16-offset-count)));
                value=(value>>offset)&mask;
                uint16_t *out=dest+destMap.storageWord(outWord);
                switch(op){
                case 0:*out=(*out&~mask)|value;break;
                case 1:*out|=value;break;
                case 2:*out&=uint16_t(~mask|value);break;
                case 3:*out^=value;break;
                }
                ++outWord;right-=count;remaining-=count;offset=0;
            }
        }
    }
    changed=true;return true;
}
bool PlanarSurface::smallFill4(uint32_t first,unsigned stride,unsigned width,unsigned height,uint16_t color,unsigned op){
    if(!width || !height || stride>65535 || !stride || (stride&15) || width>stride || height>64 || op>3)return false;
    unsigned count=((first&15)+width+15)>>4;
    if(count>64 || wordProduct(uint16_t(count),uint16_t(height))>64)return false;
    uint32_t rows=wordProduct(uint16_t(height-1),uint16_t(stride));
    if(first>=words*4 || rows+width>words*4-first)return false;
    uint16_t colors[4];colorPlanes4(color,colors);
    uint16_t head=uint16_t(0xffffu>>(first&15)),tail=uint16_t(0xffffu<<(15-((first+width-1)&15)));
    uint16_t *plane=data;unsigned pitch=stride>>4;
    for(unsigned p=0;p<4;++p,plane+=planeStride){
        uint32_t row=(first>>4);const uint16_t bits=colors[p];
        for(unsigned y=0;y<height;++y,row+=pitch){
            for(unsigned w=0;w<count;++w){
                uint16_t mask=(w?0xffff:head)&(w+1==count?tail:0xffff);
                switch(op){
                case 0:plane[storageWord(row+w)]=(plane[storageWord(row+w)]&~mask)|(bits&mask);break;
                case 1:plane[storageWord(row+w)]|=bits&mask;break;
                case 2:plane[storageWord(row+w)]&=uint16_t(~mask|bits);break;
                case 3:plane[storageWord(row+w)]^=bits&mask;break;
                }
            }
        }
    }
    changed=true;return true;
}
bool PlanarSurface::span4(uint32_t first,unsigned width,const uint16_t *colors,unsigned op){
    if(!width || width>16 || first+width>words*4 || op>3)return false;
    unsigned offset=first&15,count=(offset+width+15)>>4;
    uint16_t head=uint16_t(0xffffu>>offset),tail=uint16_t(0xffffu<<(15-((first+width-1)&15)));
    uint16_t *dest=data;
    for(unsigned p=0;p<4;++p,dest+=planeStride){
        for(unsigned w=0;w<count;++w){
            uint16_t mask=(w?0xffff:head)&(w+1==count?tail:0xffff),bits=colors[p]&mask;
            switch(op){
            case 0:dest[storageWord((first>>4)+w)]=(dest[storageWord((first>>4)+w)]&~mask)|bits;break;
            case 1:dest[storageWord((first>>4)+w)]|=bits;break;
            case 2:dest[storageWord((first>>4)+w)]&=uint16_t(~mask|bits);break;
            case 3:dest[storageWord((first>>4)+w)]^=bits;break;
            }
        }
    }
    changed=true;return true;
}
uint16_t PlanarSurface::readWord(uint32_t a)const{
    uint16_t value=0;unsigned offset=12-((a&3)<<2);uint32_t word=storageWord(a>>2);
    for(unsigned p=0;p<4;++p){
        value|=spread[(data[word]>>offset)&15]<<p;word+=planeStride;
    }
    return value;
}
void PlanarSurface::writeWord(uint32_t a,uint16_t value){
    unsigned offset=12-((a&3)<<2);uint16_t mask=uint16_t(15<<offset);uint32_t word=storageWord(a>>2);
    uint16_t bits=uint16_t((pairs.values[value&255]<<2)|pairs.values[value>>8]);
    for(unsigned p=0;p<4;++p){
        data[word]=(data[word]&~mask)|((bits>>12)<<offset);
        bits<<=4;word+=planeStride;
    }
    changed=true;
}
uint16_t PlanarSurface::pixel4(uint32_t a,unsigned shift)const{
    unsigned bit=15-((a&3)<<2)-(shift>>2);uint32_t word=storageWord(a>>2);uint16_t color=0;
    for(unsigned p=0;p<4;++p){color|=((data[word]>>bit)&1)<<p;word+=planeStride;}
    return color;
}
void PlanarSurface::plot4(uint32_t a,unsigned shift,unsigned color,unsigned op){
    uint16_t mask=uint16_t(1u<<(15-((a&3)<<2)-(shift>>2)));uint32_t word=storageWord(a>>2);
    uint16_t *p0=data+word,*p1=p0+planeStride,*p2=p1+planeStride,*p3=p2+planeStride;
    uint16_t b0=color&1?mask:0,b1=color&2?mask:0,b2=color&4?mask:0,b3=color&8?mask:0;
    // Select the ROP once, then touch each plane once. Keep exact masked bits
    // for all four operations without a per-plane loop/branch/address update.
    switch(op){
    case 0:*p0=(*p0&~mask)|b0;*p1=(*p1&~mask)|b1;*p2=(*p2&~mask)|b2;*p3=(*p3&~mask)|b3;break;
    case 1:*p0|=b0;*p1|=b1;*p2|=b2;*p3|=b3;break;
    case 2:*p0&=uint16_t(~mask|b0);*p1&=uint16_t(~mask|b1);*p2&=uint16_t(~mask|b2);*p3&=uint16_t(~mask|b3);break;
    case 3:*p0^=b0;*p1^=b1;*p2^=b2;*p3^=b3;break;
    }
    changed=true;
}
bool PlanarSurface::cardBlitFits(uint32_t first)const{
    if(first>=words*4 || 99u*608+88>words*4-first)return false;
    unsigned col=unsigned((first>>4)-wordProduct(uint16_t(rowOf(first>>4)),38));
    return col+(((first&15)+88+15)>>4)<=38;
}
bool PlanarSurface::cardBlit(uint32_t first,const uint16_t *image,const uint16_t *mask){
    if(!cardBlitFits(first))return false;
    for(unsigned y=0;y<100;++y)for(unsigned x=0;x<88;++x){
        unsigned index=wordProduct(uint16_t(y),28)+(x>>4);uint16_t bit=uint16_t(0x8000u>>(x&15));
        if(!(mask[index]&bit))continue;
        unsigned color=0;for(unsigned p=0;p<4;++p,index+=7)if(image[index]&bit)color|=1<<p;
        uint32_t a=first+wordProduct(uint16_t(y),608)+x;
        plot4(a>>2,(a&3)<<2,color,0);
    }
    return true;
}

}
