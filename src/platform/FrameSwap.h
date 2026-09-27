#ifndef POKERI_FRAME_SWAP_H
#define POKERI_FRAME_SWAP_H
#include <cstdint>
namespace pokeri {
// Two buffers: queued rendering, then a COP1LC update, then a frame boundary.
// The old front is never writable until the scheduled list has been reloaded.
struct FrameSwap {
    volatile unsigned front=0;
    volatile int pending=-1,armed=-1;
    volatile uint32_t frame=0,armedFrame=0;
    static bool armWindow(unsigned line,bool verticalPending){
        // Keep the short register update away from the physical reload edge.
        // Main-thread callers also reject an unserviced VERTB request.
        return line>=8 && line<300 && !verticalPending;
    }
    bool arm(){
        if(pending<0 || pending>1 || armed>=0 || unsigned(pending)==front)return false;
        armedFrame=frame;armed=pending;return true;
    }
    bool vblank(){
        ++frame;
        if(armed<0 || frame==armedFrame)return false;
        front=unsigned(armed);armed=-1;pending=-1;return true;
    }
};
}
#endif
