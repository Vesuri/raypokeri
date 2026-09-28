#ifndef POKERI_CARD_DAMAGE_H
#define POKERI_CARD_DAMAGE_H
#include "../board/WordMath.h"
namespace pokeri {
struct DamageBounds {
    unsigned x,y,width,height;
    DamageBounds(unsigned px=0,unsigned py=0,unsigned w=0,unsigned h=0):x(px),y(py),width(w),height(h){}
    void include(const DamageBounds &b){
        if(!b.width || !b.height)return;
        if(!width || !height){*this=b;return;}
        unsigned right=x+width,bottom=y+height;
        if(b.x+b.width>right)right=b.x+b.width;
        if(b.y+b.height>bottom)bottom=b.y+b.height;
        if(b.x<x)x=b.x;
        if(b.y<y)y=b.y;
        width=right-x;height=bottom-y;
    }
};
// Known cached-card writes are separate from the existing unknown-damage flag.
// Multiple different cards before collection conservatively require a full copy.
struct CardDamage {
    bool marked=false;
    uint32_t first=0;
    void include(uint32_t address,bool &unknown){
        if(marked && first!=address)unknown=true;
        first=address;marked=true;
    }
    void clear(){marked=false;}
    static bool project(uint32_t card,uint32_t source,unsigned stride,
                        unsigned screenY,unsigned height,DamageBounds &out){
        if(stride!=608 || screenY>283 || height>283-screenY ||
           card>0x100000u-99u*608u-88u || source>0x100000u ||
           wordProduct(uint16_t(height),608)>0x100000u-source)return false;
        if(!height || card+99u*608u+88u<=source || card>=source+wordProduct(uint16_t(height),608))return true;
        bool negative=card<source;uint32_t magnitude=negative?source-card:card-source;
        // floor(delta/608), using only the existing bounded 16-bit DIVU helper.
        unsigned rows=wordQuotient(uint16_t((magnitude+(negative?607:0))>>5),19);
        int y=negative?-int(rows):int(rows);
        unsigned x=negative?wordProduct(uint16_t(rows),608)-magnitude:magnitude-wordProduct(uint16_t(rows),608);
        if(x+88>608)return false; // row-straddling source needs general composition
        int bottom=y+100;if(y<0)y=0;if(bottom>int(height))bottom=height;
        if(bottom>y)out.include(DamageBounds(x,screenY+unsigned(y),88,unsigned(bottom-y)));
        return true;
    }
};
}
#endif
