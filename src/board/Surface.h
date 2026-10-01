#ifndef POKERI_SURFACE_H
#define POKERI_SURFACE_H
#include <cstdint>
#include "PlanarLayout.h"
namespace pokeri {
// A parameter word supplies four repeating pixel nibbles in host order.
inline void expandColorPlanes4(uint16_t color,uint16_t *planes){
    const uint16_t nibble=color&15;
    if(color==uint16_t(nibble*uint16_t(0x1111))){
        planes[0]=uint16_t(-int(nibble&1));
        planes[1]=uint16_t(-int((nibble>>1)&1));
        planes[2]=uint16_t(-int((nibble>>2)&1));
        planes[3]=uint16_t(-int((nibble>>3)&1));
        return;
    }
    for(unsigned p=0;p<4;++p){
        unsigned bits=((color>>p)&1)*8+((color>>(p+4))&1)*4+
            ((color>>(p+8))&1)*2+((color>>(p+12))&1);
        planes[p]=uint16_t(bits|(bits<<4)|(bits<<8)|(bits<<12));
    }
}
// Storage/drawing boundary. Word accesses retain the ACRTC's host-bus layout;
// a platform is free to store the same bits in a different representation.
// A small, unzoomed HD63484 pattern, keyed by content rather than ROM address.
struct PatternTile {
    uint16_t rows[16],colors[2],point,start,end;
    unsigned mode,width,height,offset;
    bool operator==(const PatternTile &b)const {
        if(point!=b.point || start!=b.start || end!=b.end || mode!=b.mode ||
           width!=b.width || height!=b.height || offset!=b.offset ||
           colors[0]!=b.colors[0] || colors[1]!=b.colors[1])return false;
        for(unsigned i=0;i<16;++i)if(rows[i]!=b.rows[i])return false;
        return true;
    }
    bool valid()const {
        return width && width<=16 && height && height<=16 && offset<16 && mode<3 &&
            !(point&0x0f0f) && !(end&0x0f0f) &&
            ((start>>4)&15)<=((point>>4)&15) && ((point>>4)&15)<=((end>>4)&15) &&
            (start>>12)<=(point>>12) && (point>>12)<=(end>>12);
    }
    // Two words per row, sixteen rows per plane: mask, then four colours.
    // Top display row corresponds to the final logical pattern row.
    static uint16_t reverse(uint16_t v){
        v=uint16_t(((v>>1)&0x5555)|((v&0x5555)<<1));
        v=uint16_t(((v>>2)&0x3333)|((v&0x3333)<<2));
        v=uint16_t(((v>>4)&0x0f0f)|((v&0x0f0f)<<4));
        return uint16_t((v>>8)|(v<<8));
    }
    // Interleaved output repeats the mask for each plane, allowing one DMA
    // operation over all four planes. Ordinary output remains 160 words.
    template<bool Interleaved=false> void expand(uint16_t *out)const {
        if(Interleaved){
            // Every active mask/colour word is assigned below. Only unused
            // rows need clearing; never clear and then rewrite the whole tile.
            for(unsigned i=height*8;i<128;++i){out[i]=0;out[128+i]=0;}
        }else for(unsigned i=0;i<160;++i)out[i]=0;
        uint16_t planes[2][4];
        for(unsigned color=0;color<2;++color)expandColorPlanes4(colors[color],planes[color]);
        unsigned top=start>>12,bottom=end>>12,left=(start>>4)&15,right=(end>>4)&15;
        unsigned py=point>>12,px=(point>>4)&15;
        uint32_t range=(uint32_t(0xffff0000u)<<(16-width))>>offset;
        for(unsigned y=0;y<height;++y){
            uint16_t bits;
            if(width<=right-px+1)bits=reverse(uint16_t(rows[py]>>px));
            else {
                // Arbitrary small repeating windows retain exact wrap order.
                bits=0;unsigned column=px;
                for(unsigned x=0;x<width;++x){
                    if(rows[py]&(1u<<column))bits|=uint16_t(0x8000u>>x);
                    if(++column>right)column=left;
                }
            }
            uint32_t ones=((uint32_t(bits)<<16)>>offset)&range;
            uint32_t mask=mode==1?ones:mode==2?range&~ones:range;
            unsigned row=(height-1-y)*2;
            uint16_t high=uint16_t(ones>>16),low=uint16_t(ones);
            if(!Interleaved){out[row]=uint16_t(mask>>16);out[row+1]=uint16_t(mask);}
            for(unsigned p=0;p<4;++p){
                unsigned m=Interleaved?row*4+p*2:row;
                unsigned c=Interleaved?128+m:(p+1)*32+row;
                if(Interleaved){out[m]=uint16_t(mask>>16);out[m+1]=uint16_t(mask);}
                out[c]=uint16_t(((high&planes[1][p])|(~high&planes[0][p]))&out[m]);
                out[c+1]=uint16_t(((low&planes[1][p])|(~low&planes[0][p]))&out[m+1]);
            }
            if(++py>bottom)py=top;
        }
    }
};
// Relative planar words, grouped by logical row (never by aliased VRAM address).
struct CurveWord {int16_t x,y;uint16_t mask,padding;};
// A synchronized CPU lease over the four planes. The caller must discard it
// before any operation which may queue a write. No virtual calls per pixel.
struct CpuPlanes : PlanarLayout {
    uint16_t *data=nullptr;
    uint32_t planeWords=0;
    bool *changed=nullptr;
    uint16_t pixel4(uint32_t a,unsigned shift)const{
        uint16_t mask=uint16_t(0x8000u>>(((a&3)<<2)+(shift>>2)));
        const uint16_t *p=data+storageWord(a>>2);
        unsigned color=(*p&mask)?1:0;p+=planeStride;
        if(*p&mask)color|=2;p+=planeStride;
        if(*p&mask)color|=4;p+=planeStride;
        if(*p&mask)color|=8;return color;
    }
    void readPlanes4(uint32_t a,uint16_t *out)const{
        const uint16_t *p=data+storageWord(a>>2);
        out[0]=*p;p+=planeStride;out[1]=*p;p+=planeStride;
        out[2]=*p;p+=planeStride;out[3]=*p;
    }
    void plot4(uint32_t a,unsigned shift,unsigned color,unsigned op){
        uint16_t mask=uint16_t(0x8000u>>(((a&3)<<2)+(shift>>2)));
        uint16_t *p0=data+storageWord(a>>2),*p1=p0+planeStride,*p2=p1+planeStride,*p3=p2+planeStride;
        uint16_t b0=color&1?mask:0,b1=color&2?mask:0,b2=color&4?mask:0,b3=color&8?mask:0;
        switch(op){
        case 0:*p0=(*p0&~mask)|b0;*p1=(*p1&~mask)|b1;*p2=(*p2&~mask)|b2;*p3=(*p3&~mask)|b3;break;
        case 1:*p0|=b0;*p1|=b1;*p2|=b2;*p3|=b3;break;
        case 2:*p0&=uint16_t(~mask|b0);*p1&=uint16_t(~mask|b1);*p2&=uint16_t(~mask|b2);*p3&=uint16_t(~mask|b3);break;
        case 3:*p0^=b0;*p1^=b1;*p2^=b2;*p3^=b3;break;
        }
        *changed=true;
    }
};
struct Surface {
    virtual ~Surface() {}
    virtual void damage(){}
    virtual void damageCard(uint32_t){damage();}
    virtual bool cardBlitFits(uint32_t)const{return false;}
    // Card assets: 100 rows of four 7-word planes; bits after pixel 87 are zero
    // in both image and mask. The prepared immutable cache proves these bounds.
    virtual bool cardBlit(uint32_t,const uint16_t *,const uint16_t *){return false;}
    virtual uint16_t readWord(uint32_t address) const=0;
    virtual void writeWord(uint32_t address,uint16_t value)=0;
    virtual uint16_t pixel4(uint32_t address,unsigned shift) const=0;
    virtual void plot4(uint32_t address,unsigned shift,unsigned color,unsigned op)=0;
    // Optional planar word operations. Address is a packed-word address aligned
    // to four words (16 pixels); bit 15 is the leftmost pixel. Callers retain a
    // scalar fallback. Implementations must finish queued writes before CPU use.
    virtual bool cpuAccess4(CpuPlanes &){return false;}
    virtual bool readPlanes4(uint32_t,uint16_t *)const{return false;}
    // Uniform-colour Bresenham, excluded endpoint. first is a pixel address;
    // wordMask and rowStep are native 16-pixel-word units. Signed X step is ±1.
    virtual bool line4(uint32_t,uint32_t,int,int,int,int,unsigned,unsigned){return false;}
    virtual bool curve4(uint32_t,uint32_t,unsigned,const CurveWord *,unsigned,uint16_t,unsigned){return false;}
    virtual bool span4(uint32_t,unsigned,const uint16_t *,unsigned){return false;}
    static void colorPlanes4(uint16_t color,uint16_t *planes){
        expandColorPlanes4(color,planes);
    }
    virtual bool fill(uint32_t,unsigned,unsigned,unsigned,uint16_t,unsigned){return false;}
    virtual bool patternTile(uint32_t,unsigned,const PatternTile&,unsigned){return false;}
    virtual bool copy180(uint32_t,uint32_t,unsigned,unsigned,unsigned,unsigned){return false;}
    virtual bool copy(uint32_t,uint32_t,unsigned,unsigned,unsigned,unsigned){return false;}
};
}
#endif
