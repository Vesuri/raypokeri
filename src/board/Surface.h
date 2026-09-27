#ifndef POKERI_SURFACE_H
#define POKERI_SURFACE_H
#include <cstdint>
namespace pokeri {
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
    void expand(uint16_t *out)const {
        for(unsigned i=0;i<160;++i)out[i]=0;
        unsigned top=start>>12,bottom=end>>12,left=(start>>4)&15,right=(end>>4)&15;
        unsigned py=point>>12;
        for(unsigned y=0;y<height;++y){
            unsigned px=(point>>4)&15,row=(height-1-y)*2;
            for(unsigned x=0;x<width;++x){
                bool bit=(rows[py]>>px)&1;
                if(!((mode==1 && !bit)||(mode==2 && bit))){
                    unsigned dot=offset+x,index=row+(dot>>4);
                    uint16_t mask=uint16_t(0x8000u>>(dot&15));
                    unsigned color=(colors[bit]>>((dot&3)*4))&15;
                    out[index]|=mask;
                    for(unsigned p=0;p<4;++p)if(color&(1<<p))out[(p+1)*32+index]|=mask;
                }
                if(++px>right)px=left;
            }
            if(++py>bottom)py=top;
        }
    }
};
// Relative planar words, grouped by logical row (never by aliased VRAM address).
struct CurveWord {int16_t x,y;uint16_t mask,padding;};
// A synchronized CPU lease over the four planes. The caller must discard it
// before any operation which may queue a write. No virtual calls per pixel.
struct CpuPlanes {
    uint16_t *data=nullptr;
    uint32_t planeWords=0;
    bool *changed=nullptr;
    uint16_t pixel4(uint32_t a,unsigned shift)const{
        uint16_t mask=uint16_t(0x8000u>>(((a&3)<<2)+(shift>>2)));
        const uint16_t *p=data+(a>>2);
        unsigned color=(*p&mask)?1:0;p+=planeWords;
        if(*p&mask)color|=2;p+=planeWords;
        if(*p&mask)color|=4;p+=planeWords;
        if(*p&mask)color|=8;return color;
    }
    void readPlanes4(uint32_t a,uint16_t *out)const{
        const uint16_t *p=data+(a>>2);
        out[0]=*p;p+=planeWords;out[1]=*p;p+=planeWords;
        out[2]=*p;p+=planeWords;out[3]=*p;
    }
    void plot4(uint32_t a,unsigned shift,unsigned color,unsigned op){
        uint16_t mask=uint16_t(0x8000u>>(((a&3)<<2)+(shift>>2)));
        uint16_t *p0=data+(a>>2),*p1=p0+planeWords,*p2=p1+planeWords,*p3=p2+planeWords;
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
        for(unsigned p=0;p<4;++p){
            unsigned bits=((color>>p)&1)*8+((color>>(p+4))&1)*4+
                ((color>>(p+8))&1)*2+((color>>(p+12))&1);
            planes[p]=uint16_t(bits|(bits<<4)|(bits<<8)|(bits<<12));
        }
    }
    virtual bool fill(uint32_t,unsigned,unsigned,unsigned,uint16_t,unsigned){return false;}
    virtual bool patternTile(uint32_t,unsigned,const PatternTile&,unsigned){return false;}
    virtual bool copy180(uint32_t,uint32_t,unsigned,unsigned,unsigned,unsigned){return false;}
    virtual bool copy(uint32_t,uint32_t,unsigned,unsigned,unsigned,unsigned){return false;}
};
}
#endif
