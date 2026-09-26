#ifndef POKERI_AMIGA_SCREEN_H
#define POKERI_AMIGA_SCREEN_H
#include "board/Hd63484.h"
#include "AmigaSurface.h"
class CopperList;
class AmigaScreen {
public:
    bool prepare(AmigaSurface &video,const uint8_t *rom);
    bool present(pokeri::Hd63484 &video,bool force=false);
    void vbi();
    bool active()const{return displaying;}
    void controlWrite(const pokeri::Hd63484 &video,uint8_t value){
        unsigned ar=video.ar;
        // CCR high: only GBM changes pixel format; low is IRQ enables.
        // Other control writes remain conservatively display-affecting.
        unsigned mask=ar==2?7:ar==3?0:255;
        if(ar>=2 && ((video.control[ar]^value)&mask))registersDirty=true;
    }
    void outputs(bool enabled,const uint8_t *values);
    void release();
    CopperList *copper()const{return lists[front];}
    uint16_t *pixels()const{return buffers[pending>=0?pending:front];}
    uint32_t frames=0;
    volatile uint32_t swaps=0,lateSwaps=0;
    const char *error=nullptr;
private:
    enum {Bytes=81504};
    AmigaSurface *surface=nullptr;
    uint8_t latches[8]={};
    bool registersDirty=true;
    bool showOutputs=false,overlayDirty=false;
    unsigned bright=15,dark=0;
    void drawOutputs(uint16_t *out);
    uint16_t *buffers[2]={};
    CopperList *lists[2]={};
    volatile int pending=-1;
    unsigned front=0;
    bool geometrySeen=false;
    volatile bool displaying=false;
    bool region(unsigned destX,unsigned destY,uint32_t source,unsigned stride,unsigned width,unsigned height,bool visible,uint16_t *out);
};
#endif
