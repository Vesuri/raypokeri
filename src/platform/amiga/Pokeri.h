#ifndef _POKERI_H
#define _POKERI_H

#include "Util.h"

class CopperList;
struct View;

// The Amiga application: takes the display over from the OS, runs until the left
// mouse button is pressed, and hands the display back.  The emulated 68008 board
// (HD63484 video, AY-3-8912 audio, inputs) will hang off this class; for now it
// only owns the takeover and the vertical-blank server.  See amiga/ARCH.md.
class Pokeri {
public:
    Pokeri();
    ~Pokeri();

    bool isRunnable() const { return runnable; }
    void run();

    // Called from the exec VERTB server (interrupt context, 50 Hz PAL).
    void verticalBlank();

    static volatile uint16_t vbiCount;

private:
    CopperList* copperList;
    struct View* oldActiView;
    uint16_t oldEnabledDMAChannels;
    uint16_t oldEnabledInterrupts;
    bool serverInstalled;
    bool runnable;
    volatile bool quit;
};

#endif
