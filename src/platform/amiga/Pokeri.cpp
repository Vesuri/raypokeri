#include <proto/exec.h>
#include <proto/graphics.h>
#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <graphics/gfxbase.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>
#include "Pokeri.h"
#include "Native.h"
#include "AmigaHardware.h"
#include "CopperList.h"

extern struct ExecBase* SysBase;
extern struct GfxBase* GfxBase;

volatile uint16_t Pokeri::vbiCount = 0;

// The VERTB server hangs off exec's chain (AddIntServer), not a raw level-3 autovector:
// exec keeps its own level-3 handler (CIA timers, the BLIT vector), there is no VBR
// plumbing, and it is WHDLoad-safe.  Rescue on Fractalus later replaced the whole VERTB
// IntVector to win back ~4% of the frame from the OS servers ahead of it (its
// PlatformAmiga.cpp, "Real INTB_VERTB VBI handler") — revisit only if measurement says so.
static struct Interrupt vbiInterrupt;
static Pokeri* vbiOwner = 0;

// exec calls a server with is_Data in a1 and walks on while it returns Z set (d0 = 0).
static uint32_t vbiServer()
{
    if (vbiOwner) {
        vbiOwner->verticalBlank();
    }
    return 0;
}

Pokeri::Pokeri() :
    copperList(0),
    oldActiView(0),
    oldEnabledDMAChannels(0),
    oldEnabledInterrupts(0),
    serverInstalled(false),
    runnable(false),
    quit(false)
{
    if(!nativePrepare())return;
    GfxBase = (struct GfxBase*)OpenLibrary((uint8_t*)"graphics.library", 0);
    if (!GfxBase) {
        return;
    }

    // ⚠ Built at runtime on purpose: .MEMF_CHIP is a BSS hunk, so a __chip static
    // initialiser would be silently discarded (Rescue on Fractalus, asset-extraction.md §6.1).
    copperList = CopperList::allocate(3);
    if (!copperList) {
        return;
    }
    copperList->data()[1] = copperMove(color00, 0x0000);

    oldActiView = GfxBase->ActiView;
    LoadView(0);
    WaitTOF();
    WaitTOF();

    oldEnabledDMAChannels = AmigaHardware::enabledDMAChannels();
    oldEnabledInterrupts = AmigaHardware::enabledInterrupts();
    AmigaHardware::setDMAChannels(DMAF_ALL, false);
    AmigaHardware::setCopperList(nativeCopper()?*nativeCopper():*copperList, true);
    AmigaHardware::setDMAChannels(DMAF_MASTER | DMAF_COPPER | DMAF_BLITTER | (nativeCopper()?DMAF_RASTER:0), true);

    vbiOwner = this;
    vbiInterrupt.is_Node.ln_Type = NT_INTERRUPT;
    vbiInterrupt.is_Node.ln_Pri = 0;
    vbiInterrupt.is_Node.ln_Name = (char*)"Pokeri VBI";
    vbiInterrupt.is_Data = this;
    vbiInterrupt.is_Code = (void (*)())vbiServer;
    AddIntServer(INTB_VERTB, &vbiInterrupt);
    serverInstalled = true;

    nativeAudioStart();
    runnable = true;
}

Pokeri::~Pokeri()
{
    nativeAudioStop();
    if (serverInstalled) {
        RemIntServer(INTB_VERTB, &vbiInterrupt);
        vbiOwner = 0;
    }
    if (oldEnabledDMAChannels || oldEnabledInterrupts) {
        while (AmigaHardware::hasQueuedBlits || AmigaHardware::isBlitterBusy());
        AmigaHardware::setDMAChannels(DMAF_ALL, false);
        AmigaHardware::setCopperList(CopperList((uint32_t*)GfxBase->copinit), true);
        AmigaHardware::setDMAChannels(oldEnabledDMAChannels, true);
        LoadView(oldActiView);
        WaitTOF();
        WaitTOF();
    }
    delete copperList;
    if (GfxBase) {
        CloseLibrary((struct Library*)GfxBase);
    }
    nativeRelease();
}

void Pokeri::run()
{
    nativeRun();
}

void Pokeri::verticalBlank()
{
    vbiCount++;
    if (AmigaHardware::isLeftMouseButtonPressed()) {
        quit = true;
    }
    nativeVbi(quit);
}
