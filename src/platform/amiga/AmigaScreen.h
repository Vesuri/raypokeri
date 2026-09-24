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
    void outputs(bool enabled,const uint8_t *values);
    void release();
    CopperList *copper()const{return lists[front];}
    uint16_t *pixels()const{return buffers[pending>=0?pending:front];}
    uint32_t frames=0;
    const char *error=nullptr;
private:
    enum {Bytes=81504};
    AmigaSurface *surface=nullptr;
    uint8_t previous[256]={},latches[8]={};
    bool showOutputs=false,overlayDirty=false;
    unsigned bright=15,dark=0;
    void drawOutputs(uint16_t *out);
    uint16_t *buffers[2]={};
    CopperList *lists[2]={};
    volatile int pending=-1;
    unsigned front=0;
    bool geometrySeen=false;
    bool region(unsigned destX,unsigned destY,uint32_t source,unsigned stride,unsigned width,unsigned height,bool visible,uint16_t *out);
};
#endif
