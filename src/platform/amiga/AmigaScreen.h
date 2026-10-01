#ifndef POKERI_AMIGA_SCREEN_H
#define POKERI_AMIGA_SCREEN_H
#include "board/Hd63484.h"
#include "AmigaSurface.h"
#include "../FrameSwap.h"
#include "../CardDamage.h"
class CopperList;
class AmigaScreen : private pokeri::FrameSwap {
public:
    bool prepare(AmigaSurface &video,const uint8_t *rom);
    bool present(pokeri::Hd63484 &video,bool force=false);
    void vbi();
    void presentReady(){if(pending>=0 && armed<0 && !testing)armReady();}
    bool active()const{return displaying;}
    bool presentationPending()const{return pending>=0;}
    bool awaitingPublication()const{return pending>=0 && armed<0;}
    void controlWrite(const pokeri::Hd63484 &video,uint8_t value){
        unsigned ar=video.ar;
        // CCR high: only GBM changes pixel format; low is IRQ enables.
        // Other control writes remain conservatively display-affecting.
        unsigned mask=ar==2?7:ar==3?0:255;
        if(ar>=2 && ((video.control[ar]^value)&mask)){
            registersDirty=true;
            // Window position/size/source affect only its old and new bounds.
            // DCR high bits 0/1 are window display/enable, all others are base.
            bool window=(ar>=0x92 && ar<=0x97) || (ar>=0xd8 && ar<=0xdf) || ar==0x88;
            if(!window && !(ar==6 && !((video.control[ar]^value)&0xfc)))backgroundDirty=true;
        }
    }
    void outputs(bool enabled,const uint8_t *values);
    void release();
    CopperList *copper()const{return lists[front];}
    uint16_t *pixels()const{return buffers[pending>=0?pending:front];}
    bool incremental=true; // full-composition diagnostic comparison
    bool compositionTest(pokeri::Hd63484 &video,uint32_t ticks[2]);
    uint32_t fullFrames=0,partialFrames=0,composedPixels=0;
    uint32_t frames=0;
    uint32_t cardRepairCases=0,cardRepairTicks[2]={},cardRepairMismatch[3]={};
    volatile uint32_t swaps=0,lateSwaps=0;
    uint32_t arms=0;
    const char *error=nullptr;
private:
    friend struct NativeVideoIrqLayout; // compile-time addresses for guarded native service
    enum {Width=608,Height=283,PlaneWords=40,RowWords=160,Bytes=Height*RowWords*2};
    AmigaSurface *surface=nullptr;
    uint8_t latches[8]={};
    bool registersDirty=true;
    bool showOutputs=false,overlayDirty=false;
    unsigned bright=15,dark=0;
    void drawOutputs(uint16_t *out);
    uint16_t *buffers[2]={};
    CopperList *lists[2]={};
    void armReady();
    using Bounds=pokeri::DamageBounds;
    Bounds cardRepair[2];
    Bounds previousWindow[2];
    bool backgroundValid[2]={},backgroundDirty=true,testing=false;
    bool geometrySeen=false;
    volatile bool displaying=false;
    bool region(pokeri::Hd63484 &video,unsigned destX,unsigned destY,uint32_t source,unsigned stride,unsigned width,unsigned height,bool visible,uint16_t *out);
};
#endif
