// Included only by Native.cpp for the guarded assembly service. These assembler
// aliases name the actual owning objects, not cached copies. C++ resolves field
// addresses/layout; no hard-coded private offsets or runtime initialization.
struct NativeVideoIrqLayout {
    static_assert(offsetof(LiveClock,credit)==0 && offsetof(LiveClock,debt)==4 &&
                  offsetof(LiveClock,frame)==8 && offsetof(LiveClock,limited)==16 &&
                  offsetof(LiveClock,ratioSixteenths)==20 && offsetof(LiveClock,windowFrames)==22,
                  "video clock assembly layout");
    __attribute__((used)) static void emit(){
#define IRQ_ADDRESS(name,object) asm volatile(".globl " #name "\n.set " #name ",%c0"::"i"(&(object)))
        IRQ_ADDRESS(irqDiagnostic,diagnostic);
        IRQ_ADDRESS(irqRomBase,romBase);
        IRQ_ADDRESS(irqRom,rom);
        IRQ_ADDRESS(irqRamBase,ramBase);
        IRQ_ADDRESS(irqScreenActive,screen.displaying);
        IRQ_ADDRESS(irqScreenPending,screen.pending);
        IRQ_ADDRESS(irqDisplayCalibrated,clockDisplayCalibrated);
        IRQ_ADDRESS(irqStopCycles,liveStopCycles);
        IRQ_ADDRESS(irqLiveCycles,liveCycles);
#ifdef POKERI_STARTUP_FAST_FORWARD
        IRQ_ADDRESS(irqStartupFast,startupFast);
#endif
        IRQ_ADDRESS(irqLiveTicks,liveTicks);
        IRQ_ADDRESS(irqClockFrame,liveClock.frame);
        IRQ_ADDRESS(irqClockCredit,liveClock.credit);
        IRQ_ADDRESS(irqClockDebt,liveClock.debt);
        IRQ_ADDRESS(irqClock,liveClock);
        IRQ_ADDRESS(irqGuestPhase,guestClockPhase);
        IRQ_ADDRESS(irqQuit,quitRequested);
        IRQ_ADDRESS(irqShuffleCount,shuffleQueue.count);
        IRQ_ADDRESS(irqShuffleActive,shuffleActive);
        IRQ_ADDRESS(irqShuffleQueued,shuffleQueued);
        IRQ_ADDRESS(irqCompositionPending,compositionPending);
        IRQ_ADDRESS(irqLiveActive,liveIrqActive);
        IRQ_ADDRESS(irqUninterruptedPoll,uninterruptedPoll);
#undef IRQ_ADDRESS
    }
};
// Keep source predicates in the authoritative portable models. This leaf query
// has no scheduling, guest stores, virtual dispatch or persistent authorization.
// A nonzero result is the current status of the sole eligible IRQ source.
extern "C" unsigned nativeVideoIrqSource(){
    if(board->fault || board->resetRequested || board->video.error ||
#ifdef POKERI_IRQ_CACHE
       peripheralIrq() ||
#else
       board->pia[0].Pia6821::irq() || board->serial[0].Acia6850::irq() ||
#endif
       ((board->pia[0].flags[1]&0x40) && (board->pia[0].control[1]&8)))return 0;
    unsigned status=board->video.statusNow();
    return status&board->video.control[3]?status:0;
}
// Presentation hold is separate from FIFO status: its CED/error bits may still
// request an IRQ. Keep that guard even when source selection succeeds.
extern "C" unsigned nativeVideoIrqHeld(){return board->video.presentationBusy;}

#ifndef POKERI_NO_PROFILE_SUPPORT
extern "C" unsigned nativeVideoIrqProfile(){return NativeTiming::isActive();}
#endif
