	| Absolute diagnostic tag: no memory is read at address zero/one.
	.globl nativeFeedCounterMode
.ifdef POKERI_FEED_COUNTS
	.equ nativeFeedCounterMode,1
.else
	.equ nativeFeedCounterMode,0
.endif
	.macro stopclock
	tst.w nativeClockEnabled
	beq 1f
	move.b #0,0xbfee01
1:
	.endm
	.text
	.globl nativePrepare,nativePrepareAbort,nativeAbort
nativePrepare:
	movem.l %d2-%d7/%a2-%a6,-(%sp)
	move.l %sp,nativePrepareStack
	jsr nativePrepareInner
	bra nativePrepared
nativePrepareAbort:
	moveq #0,%d0
nativePrepared:
	move.l nativePrepareStack,%sp
	movem.l (%sp)+,%d2-%d7/%a2-%a6
	rts
nativeAbort:
	move.w #0x2700,%sr
	lea nativeServiceStack+32768,%sp
	bra nativeExit
	.globl nativeEntry,nativeLineA,nativeTrace,nativeFault,nativeLevel3
	| Keep Exec handling level 3. Arm one trace on return to physical user
	| mode so VBI time/IRQs can be serviced even in a hook-free game loop.
nativeLevel3:
	tst.w nativeProfileEnabled
	beq nativeLevel3ProfileDone
	| VERTB can share level 3 with BLIT. Test both request and enable bits.
	btst #5,0xdff01f
	beq nativeLevel3ProfileDone
	btst #5,0xdff01d
	beq nativeLevel3ProfileDone
	movem.l %d0-%d1/%a0-%a1,-(%sp)
	move.l 18(%sp),-(%sp)
	jsr nativeProfileSample
	addq.l #4,%sp
	movem.l (%sp)+,%d0-%d1/%a0-%a1
nativeLevel3ProfileDone:
	btst #5,(%sp)
	bne nativeChainLevel3
	stopclock
	movem.l %d0-%d1/%a0-%a1,-(%sp)
	jsr nativeClockEnter
	jsr nativeClockPauseInterrupt
	movem.l (%sp)+,%d0-%d1/%a0-%a1
	ori.w #0x8000,(%sp)
nativeChainLevel3:
	move.l nativeOldLevel3,-(%sp)
	rts
	| timer.device uses level 6. Preserve the real exception frame and all
	| registers, then tail-call its saved handler with the same RTS idiom
	| as nativeChainLevel3 above: RTS pops the handler address into PC.
	.globl nativeLevel6
nativeLevel6:
	btst #5,(%sp)
	bne nativeChainLevel6
	stopclock
	movem.l %d0-%d1/%a0-%a1,-(%sp)
	jsr nativeClockEnter
	jsr nativeClockPauseInterrupt
	movem.l (%sp)+,%d0-%d1/%a0-%a1
	ori.w #0x8000,(%sp)
nativeChainLevel6:
	move.l nativeOldLevel6,-(%sp)
	rts
	.globl nativeLevel2
nativeLevel2:
	btst #5,(%sp)
	bne nativeChainLevel2
	stopclock
	movem.l %d0-%d1/%a0-%a1,-(%sp)
	jsr nativeClockEnter
	jsr nativeClockPauseInterrupt
	movem.l (%sp)+,%d0-%d1/%a0-%a1
	ori.w #0x8000,(%sp)
nativeChainLevel2:
	move.l nativeOldLevel2,-(%sp)
	rts
	.globl nativeLevel4
nativeLevel4:
	btst #5,(%sp)
	bne nativeChainLevel4
	stopclock
	movem.l %d0-%d1/%a0-%a1,-(%sp)
	jsr nativeClockEnter
	jsr nativeClockPauseInterrupt
	movem.l (%sp)+,%d0-%d1/%a0-%a1
	ori.w #0x8000,(%sp)
nativeChainLevel4:
	move.l nativeOldLevel4,-(%sp)
	rts
nativeEntry:
	move.w #0x2700,%sr
	movem.l %d2-%d7/%a2-%a6,-(%sp)
	move.l %sp,nativeReturnStack
	move.l %usp,%a0
	move.l %a0,nativeOsUsp
	lea nativeServiceStack+32768,%sp
	jsr nativeInstallVectors
	tst.w nativeBenchmarkRequested
	beq nativeEntryGuest
	jsr nativeProfileBenchmark
	bra nativeExit
nativeEntryGuest:
	jsr nativeClockCalibrateBegin
	bra nativeResume
nativeLineA:
	move.w #0x2700,%sr
	stopclock
	tst.w nativeShortEnabled
	beq nativeLineASlow
	movem.l %d0-%d1/%a0-%a1,-(%sp)
	tst.w nativeDiagnostic
	bne nativeShortLookup
	btst #7,16(%sp)
	bne nativeShortDecline
nativeShortLookup:
	move.l 18(%sp),%a0
	| PC has just been fetched by the CPU; the guarded descriptor below
	| still requires the exact admitted site before any device access.
	moveq #0,%d0
	move.w (%a0),%d0
	andi.w #0x0fff,%d0
	cmp.w nativeShortCount,%d0
	bcc nativeShortDecline
	lsl.l #5,%d0
	lea nativeShortStatus,%a1
	adda.l %d0,%a1
	cmpa.l (%a1),%a0
	bne nativeShortDecline
	| Guard and body addresses are prepared once; the exact PC check above
	| prevents unadmitted indices from reaching either pointer.
	move.l 16(%a1),-(%sp)
	rts
	.globl nativeShortStatusGuard,nativeShortStatusRead
	.globl nativeShortSentinelGuard,nativeShortSentinelRead
	.globl nativeShortControlGuard,nativeShortControlRead
	.globl nativeShortPiaGuard,nativeShortPiaRead,nativeShortIoGuard,nativeShortIoRead
nativeShortStatusGuard:
	move.l 8(%sp),%d0
	cmp.l 4(%a1),%d0
	bne nativeShortDecline
	bra nativeShortAdmitted

	.globl nativeShortVideoGuard,nativeShortVideoWrite
nativeShortVideoGuard:
	move.l 8(%sp),%d0
	btst #0,9(%a1)
	beq nativeShortVideoPort
	btst #2,9(%a1)
	bne nativeShortVideoPostPort
	move.w 4(%a0),%d0
	bra nativeShortVideoDisplacement
nativeShortVideoPostPort:
	move.w 2(%a0),%d0
nativeShortVideoDisplacement:
	ext.l %d0
	add.l 8(%sp),%d0
nativeShortVideoPort:
	cmp.l 4(%a1),%d0
	bne nativeShortDecline
	btst #2,9(%a1)
	bne nativeShortVideoSource
	moveq #0,%d1
	move.w 2(%a0),%d1
	bra nativeShortAdmitted
nativeShortVideoSource:
	move.l 12(%sp),%d0
	btst #0,%d0
	bne nativeShortDecline
	addq.l #2,%d0
	bcs nativeShortDecline
	move.l 12(%sp),%a0
	cmpa.l nativeRomBegin,%a0
	bcs nativeShortVideoRam
	cmp.l nativeRomEnd,%d0
	bls nativeShortVideoRead
nativeShortVideoRam:
	cmpa.l nativeRamBegin,%a0
	bcs nativeShortDecline
	cmp.l nativeRamEnd,%d0
	bhi nativeShortDecline
nativeShortVideoRead:
	moveq #0,%d1
	move.w (%a0),%d1
	move.l 18(%sp),%a0
	bra nativeShortAdmitted

nativeShortIoGuard:
	move.l %a3,%d0
	btst #6,9(%a1)
	bne nativeShortIoPort
	btst #5,9(%a1)
	bne nativeShortIoImmediatePort
	move.w 2(%a0),%d0
	bra nativeShortIoDisplacement
nativeShortIoImmediatePort:
	move.w 4(%a0),%d0
nativeShortIoDisplacement:
	ext.l %d0
	add.l %a3,%d0
nativeShortIoPort:
	cmp.l 4(%a1),%d0
	bne nativeShortDecline
	btst #3,9(%a1)
	beq nativeShortAdmitted
	btst #5,9(%a1)
	bne nativeShortIoImmediate
	move.w 8(%a1),%d0
	andi.w #7,%d0
	beq nativeShortIoD0
	subq.w #1,%d0
	beq nativeShortIoD1
	move.l %d2,%d1
	bra nativeShortAdmitted
nativeShortIoD0:
	move.l (%sp),%d1
	bra nativeShortAdmitted
nativeShortIoD1:
	move.l 4(%sp),%d1
	bra nativeShortAdmitted
nativeShortIoImmediate:
	move.b 3(%a0),%d1
	bra nativeShortAdmitted

nativeShortPiaGuard:
	| Dynamic port EA must still equal the admitted endpoint.
	cmpi.b #0,9(%a1)
	beq nativeShortPiaShortDisplacement
	cmpi.b #3,9(%a1)
	beq nativeShortPiaShortDisplacement
	move.w 4(%a0),%d0
	bra nativeShortPiaPort
nativeShortPiaShortDisplacement:
	move.w 2(%a0),%d0
nativeShortPiaPort:
	ext.l %d0
	add.l %a3,%d0
	cmp.l 4(%a1),%d0
	bne nativeShortDecline
	cmpi.b #3,9(%a1)
	beq nativeShortAdmitted
	tst.b 9(%a1)
	bne nativeShortPiaSource
	move.l %d4,%d1
	bra nativeShortAdmitted
nativeShortPiaSource:
	cmpi.b #2,9(%a1)
	beq nativeShortPiaIndexed
	move.w 2(%a0),%d0
	ext.l %d0
	add.l %a6,%d0
	bra nativeShortPiaSourceRange
nativeShortPiaIndexed:
	move.b 3(%a0),%d0
	ext.w %d0
	ext.l %d0
	move.w %d5,%d1
	ext.l %d1
	add.l %d1,%d0
	add.l %a4,%d0
nativeShortPiaSourceRange:
	cmp.l nativeRomBegin,%d0
	bcs nativeShortPiaRam
	cmp.l nativeRomEnd,%d0
	bcs nativeShortPiaSourceRead
nativeShortPiaRam:
	cmp.l nativeRamBegin,%d0
	bcs nativeShortDecline
	cmp.l nativeRamEnd,%d0
	bcc nativeShortDecline
nativeShortPiaSourceRead:
	move.l %d0,%a0
	moveq #0,%d1
	move.b (%a0),%d1
	move.l 18(%sp),%a0
	bra nativeShortAdmitted

nativeShortControlGuard:
	| Virtual supervisor entry is required. Trace transitions retain the
	| checked full handler; virtual user return uses the measured short path.
	btst #5,nativeRegisters+68
	beq nativeShortDecline
	cmpi.b #2,9(%a1)
	bne nativeShortControlLogicGuard
	move.l %usp,%a0
	move.l %a0,%d0
	btst #0,%d0
	bne nativeShortDecline
	cmpa.l nativeRamBegin,%a0
	bcs nativeShortDecline
	addq.l #6,%d0
	bcs nativeShortDecline
	cmp.l nativeRamEnd,%d0
	bcc nativeShortDecline
	move.w (%a0),%d0
	btst #15,%d0
	bne nativeShortDecline
	btst #13,%d0
	bne nativeShortControlRteReady
	tst.w nativeStackSwitchEnabled
	beq nativeShortDecline
nativeShortControlRteReady:
	move.l %a0,%d1
	bra nativeShortControlReady
nativeShortControlLogicGuard:
	move.w nativeRegisters+68,%d0
	andi.w #0xffe0,%d0
	move.w 16(%sp),%d1
	andi.w #31,%d1
	or.w %d1,%d0
	btst #0,9(%a1)
	bne nativeShortControlAnd
	or.w 6(%a1),%d0
	bra nativeShortControlLogicValue
nativeShortControlAnd:
	and.w 6(%a1),%d0
nativeShortControlLogicValue:
	move.w %d0,%d1
	btst #15,%d0
	bne nativeShortDecline
	btst #13,%d0
	bne nativeShortControlReady
	tst.w nativeStackSwitchEnabled
	beq nativeShortDecline
nativeShortControlReady:
	move.l 18(%sp),%a0
	bra nativeShortAdmitted

nativeShortSentinelGuard:
	btst #1,9(%a1)
	bne nativeShortMemoryA0
	move.l %a2,%d0
	bra nativeShortMemoryBase
nativeShortMemoryA0:
	move.l 8(%sp),%d0
nativeShortMemoryBase:
	tst.l %d0
	beq nativeShortVectorValue
	btst #0,9(%a1)
	bne nativeShortMemoryRange
	addq.l #4,%d0
	btst #1,9(%a1)
	beq nativeShortMemoryRange
	addq.l #4,%d0
nativeShortMemoryRange:
	| A real read must be aligned and wholly inside our ROM or RAM.
	| Device/guard space, boundary crossings and odd EAs use the checked path.
	btst #0,%d0
	bne nativeShortDecline
	move.l %d0,%a0
	addq.l #4,%d0
	bcs nativeShortDecline
	cmpa.l nativeRomBegin,%a0
	bcs nativeShortMemoryRam
	cmp.l nativeRomEnd,%d0
	bls nativeShortMemoryRead
nativeShortMemoryRam:
	cmpa.l nativeRamBegin,%a0
	bcs nativeShortDecline
	cmp.l nativeRamEnd,%d0
	bhi nativeShortDecline
nativeShortMemoryRead:
	move.l (%a0),%d1
	bra nativeShortMemoryValueReady
nativeShortVectorValue:
	move.l 4(%a1),%d1
nativeShortMemoryValueReady:
	move.l 18(%sp),%a0
nativeShortAdmitted:
	tst.w nativeDiagnostic
	beq nativeShortLive
	move.l %d1,-(%sp)
	move.l %a1,-(%sp)
	move.l %a0,-(%sp)
	jsr nativeShortReplayStart
	addq.l #4,%sp
	move.l (%sp)+,%a1
	move.l (%sp)+,%d1
	tst.l %d0
	beq nativeShortFailed
	bra nativeShortRead
nativeShortLive:
	| Amiga IRQs see supervisor mode and chain without touching guest state.
	| The clock is stopped; deadline work promotes after this one access.
	move.w #0x2000,%sr
	addq.l #1,nativeInstructions
	cmpa.l nativeClockResumePc,%a0
	beq nativeShortNominalOnly
	| The timer was stopped at exactly the ordinary Line-A boundary.
	| Defer accounting to the next full boundary, never charge service time.
	moveq #0,%d0
	move.b 0xbfe501,%d0
	lsl.w #8,%d0
	move.b 0xbfe401,%d0
	not.w %d0
	cmpi.w #256,%d0
	bcc nativeShortLongClock
	lsl.w #2,%d0
	lea nativeShortCharge,%a0
	move.l (%a0,%d0.w),%d0
	bra nativeShortCharged
nativeShortLongClock:
	tst.w nativeClockMode
	beq nativeShortOldUnits
	mulu.w #361,%d0
	lsr.l #5,%d0
	bra nativeShortSubtract
nativeShortOldUnits:
	mulu.w #10,%d0
nativeShortSubtract:
	sub.l nativeClockOverhead,%d0
	bcc nativeShortCharged
	moveq #0,%d0
nativeShortCharged:
	add.l %d0,nativeShortGuest
nativeShortNominalOnly:
	moveq #0,%d0
	move.w 10(%a1),%d0
	add.l %d0,nativeShortNominal
nativeShortRead:
	move.l 20(%a1),%a0
	jmp (%a0)
nativeShortStatusRead:
	| Published from the shared device after every full boundary/tick.
	| Status reads have no side effects, so this snapshot stays exact.
	moveq #0,%d0
	move.b nativeCachedVideoStatus,%d0
	and.w 8(%a1),%d0
	beq nativeShortZero
	andi.w #0xfffb,16(%sp)
	bra nativeShortDone
nativeShortZero:
	ori.w #4,16(%sp)
	tst.w nativeProfileEnabled
	beq nativeShortDone
	move.l (%a1),%d0
	cmp.l nativeShortDrainPc,%d0
	bne nativeShortDone
	move.l #1,nativeShortDrained
	bra nativeShortDone
nativeShortSentinelRead:
	btst #0,9(%a1)
	bne nativeShortSentinelTest
	btst #1,9(%a1)
	bne nativeShortSentinelCompareD4
	move.l (%sp),%d0
	cmp.l %d1,%d0
	bra nativeShortSentinelFlags
nativeShortSentinelCompareD4:
	cmp.l %d1,%d4
	bra nativeShortSentinelFlags
nativeShortSentinelTest:
	tst.l %d1
nativeShortSentinelFlags:
	| Use the CPU's own CMP/TST flags; X and every other stacked SR bit stay.
	move.w %sr,%d0
	andi.w #15,%d0
	andi.w #0xfff0,16(%sp)
	or.w %d0,16(%sp)
	bra nativeShortDone
nativeShortVideoWrite:
	| Original postincrement completes before the device write. The guard
	| admitted both EAs before changing any saved register or device state.
	btst #2,9(%a1)
	beq nativeShortVideoCall
	addq.l #2,12(%sp)
nativeShortVideoCall:
	moveq #0,%d0
	move.b 9(%a1),%d0
	move.l %a1,-(%sp)
	move.l %d0,-(%sp)
	move.l %d1,-(%sp)
	move.l 4(%a1),-(%sp)
	jsr nativeShortVideoWriteValue
	lea 12(%sp),%sp
	move.l (%sp)+,%a1
	btst #1,9(%a1)
	beq nativeShortVideoByteFlags
	tst.w %d0
	bra nativeShortVideoFlags
nativeShortVideoByteFlags:
	tst.b %d0
nativeShortVideoFlags:
	move.w %sr,%d0
	andi.w #15,%d0
	andi.w #0xfff0,16(%sp)
	or.w %d0,16(%sp)
	| Saved CCR is complete. A marker may promote only after this write.
	move.l nativeShuffleNextPointer,%d0
	beq 1f
	cmp.l 12(%sp),%d0
	bne 1f
	ori.w #2,nativeShortPending
1:
	bra nativeShortDone
    | Only an admitted byte MOVE to the address port selects this body.
    | AR and both byte phases are the actual shared model fields. IRQ/status
    | and the latched pending-event bits cannot change through this operation;
    | the common exit still tests VBI/quit work before returning to the guest.
    .globl nativeShortAddressWrite
nativeShortAddressWrite:
    move.l nativeVideoSelector,%a0
    move.b %d1,(%a0)
    move.l nativeVideoSelector+4,%a0
    clr.b (%a0)
    move.l nativeVideoSelector+8,%a0
    clr.b (%a0)
    clr.l nativeFeedInlineCount
    clr.l nativeFeedHeaderGrant
.ifdef POKERI_CACHED_RASTER
	clr.l nativeRasterGrantActive
.endif
    move.l %d1,%d0
    bra nativeShortVideoByteFlags
 .ifdef POKERI_FIFO_CONTROL_FUSION
	| Exactly the approved address / CCR-low / address triplets. The first
	| instruction is admitted and charged by the ordinary short guard.
	.globl nativeShortFifoControl,nativeFifoControlBoundary
nativeShortFifoControl:
	btst #0,9(%a1)
	bne nativeFifoControlData
	move.l nativeVideoSelector,%a0
	move.b %d1,(%a0)
	move.l nativeVideoSelector+4,%a0
	clr.b (%a0)
	move.l nativeVideoSelector+8,%a0
	clr.b (%a0)
	clr.l nativeFeedInlineCount
	clr.l nativeFeedHeaderGrant
 .ifdef POKERI_CACHED_RASTER
	clr.l nativeRasterGrantActive
 .endif
	move.l %d1,%d0
	bra nativeFifoControlFlags
nativeFifoControlData:
	move.l %a1,-(%sp)
	pea 1
	move.l %d1,-(%sp)
	move.l 4(%a1),-(%sp)
	jsr nativeShortVideoWriteValue
	lea 12(%sp),%sp
	move.l (%sp)+,%a1
nativeFifoControlFlags:
	tst.b %d0
	move.w %sr,%d0
	andi.w #15,%d0
	andi.w #0xfff0,16(%sp)
	or.w %d0,16(%sp)
	moveq #0,%d0
	move.w 24(%a1),%d0
	add.l %d0,18(%sp)
	move.l 18(%sp),nativeClockResumePc
 .ifdef POKERI_DISPATCH_COUNTS
	tst.w nativeProfileEnabled
	beq 1f
	addq.l #1,12(%a1)
1:
 .endif
 .ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeShortCalls
 .endif
nativeFifoControlBoundary:
	bsr nativeFeedBoundary
	tst.l %d0
	beq nativeShortControlPromote
	move.l 28(%a1),%a0
	cmpa.w #0,%a0
	beq nativeShortNoControlDue
	move.l %a0,%a1
	| Validate the next effective address before admitting or charging it.
	move.l (%a1),%a0
	move.l 8(%sp),%d0
	btst #0,9(%a1)
	beq 2f
	move.w 4(%a0),%d0
	ext.l %d0
	add.l 8(%sp),%d0
2:
	cmp.l 4(%a1),%d0
	bne nativeShortControlPromote
	moveq #0,%d1
	move.w 2(%a0),%d1
	moveq #0,%d0
	move.w 10(%a1),%d0
	add.l %d0,nativeShortNominal
	addq.l #1,nativeInstructions
	move.w #0x2000,%sr
	bra nativeShortFifoControl
 .endif
nativeShortIoRead:
	btst #3,9(%a1)
	bne nativeShortIoWrite
	move.l %a1,-(%sp)
	move.l 4(%a1),-(%sp)
	jsr nativeShortIoReadValue
	addq.l #4,%sp
	move.l (%sp)+,%a1
	move.w 8(%a1),%d1
	andi.w #7,%d1
	beq nativeShortIoStoreD0
	subq.w #1,%d1
	beq nativeShortIoStoreD1
	move.b %d0,%d2
	bra nativeShortIoFlags
nativeShortIoStoreD0:
	move.b %d0,3(%sp)
	bra nativeShortIoFlags
nativeShortIoStoreD1:
	move.b %d0,7(%sp)
	bra nativeShortIoFlags
nativeShortIoWrite:
	move.l %a1,-(%sp)
	move.l %d1,-(%sp)
	move.l 4(%a1),-(%sp)
	jsr nativeShortIoWriteValue
	addq.l #8,%sp
	move.l (%sp)+,%a1
nativeShortIoFlags:
	tst.b %d0
	move.w %sr,%d0
	andi.w #15,%d0
	andi.w #0xfff0,16(%sp)
	or.w %d0,16(%sp)
	bra nativeShortDone
nativeShortPiaRead:
	cmpi.b #3,9(%a1)
	beq nativeShortPiaInput
	move.l %a1,-(%sp)
	moveq #0,%d0
	move.b 9(%a1),%d0
	move.l %d0,-(%sp)
	move.l %d1,-(%sp)
	jsr nativeShortPiaWrite
	addq.l #8,%sp
	move.l (%sp)+,%a1
	bra nativeShortPiaFlags
nativeShortPiaInput:
	move.l %a1,-(%sp)
	jsr nativeShortPiaReadValue
	move.l (%sp)+,%a1
	move.b %d0,%d2
nativeShortPiaFlags:
	tst.b %d0
	move.w %sr,%d0
	andi.w #15,%d0
	andi.w #0xfff0,16(%sp)
	or.w %d0,16(%sp)
	addq.l #4,18(%sp)
	tst.b 9(%a1)
	beq nativeShortLengthDone
	cmpi.b #3,9(%a1)
	beq nativeShortLengthDone
	addq.l #2,18(%sp)
	bra nativeShortLengthDone
nativeShortControlRead:
	cmpi.b #2,9(%a1)
	bne nativeShortControlLogicStore
	move.l %d1,%a0
	move.w (%a0)+,%d1
	move.l (%a0)+,18(%sp)
	move.l %a0,%usp
	bra nativeShortControlStore
nativeShortControlLogicStore:
	addq.l #4,18(%sp)
nativeShortControlStore:
	| The guard proved virtual supervisor entry. On a user return save the
	| popped/current supervisor stack and install the authoritative user SP.
	| Physical guest execution stays in user mode; service stack is separate.
	btst #13,%d1
	bne nativeShortControlKeepStack
	move.l %usp,%a0
	move.l %a0,nativeVirtualSsp
	move.l nativeVirtualUsp,%a0
	move.l %a0,%usp
nativeShortControlKeepStack:
	andi.w #0xa71f,%d1
	move.w %d1,nativeRegisters+68
	andi.w #31,%d1
	andi.w #0xffe0,16(%sp)
	or.w %d1,16(%sp)
	bra nativeShortLengthDone
nativeShortDone:
	moveq #0,%d0
	move.w 24(%a1),%d0
	add.l %d0,18(%sp)
nativeShortLengthDone:
	move.w #0x2700,%sr
.ifdef POKERI_DISPATCH_COUNTS
	tst.w nativeProfileEnabled
	beq nativeShortUncounted
	addq.l #1,12(%a1)
nativeShortUncounted:
.endif
	move.l 18(%sp),nativeClockResumePc
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeShortCalls
.endif
	move.l pendingFrames,%d0
	cmp.l seenFrames,%d0
	bne nativeShortControlPromote
	move.w nativeShortPending,%d0
	and.w 26(%a1),%d0
	bne nativeShortControlPromote
nativeShortNoControlDue:
	clr.l nativeFeedInlineCount
	clr.l nativeFeedHeaderGrant
.ifdef POKERI_CACHED_RASTER
	clr.l nativeRasterGrantActive
.endif
	movem.l (%sp)+,%d0-%d1/%a0-%a1
	tst.w nativeDiagnostic
	bne nativeShortPromote
	| Live mode owns/enables this timer; diagnostic mode promoted above.
	move.b #0x11,0xbfee01
nativeShortReturn:
	rte
nativeShortControlPromote:
	clr.l nativeFeedInlineCount
	clr.l nativeFeedHeaderGrant
.ifdef POKERI_CACHED_RASTER
	clr.l nativeRasterGrantActive
.endif
	clr.w nativeClockRunning
	movem.l (%sp)+,%d0-%d1/%a0-%a1
nativeShortPromote:
	| Replay has already advanced the board and executed this instruction.
	| Capture the real resulting context, then handle scheduled events once.
	movem.l %d0-%d7/%a0-%a6,nativeRegisters
	moveq #11,%d0
	bra nativeSave
nativeShortFailed:
	movem.l (%sp)+,%d0-%d1/%a0-%a1
	movem.l %d0-%d7/%a0-%a6,nativeRegisters
	moveq #0,%d0
	bra nativeSave
nativeShortDecline:
	movem.l (%sp)+,%d0-%d1/%a0-%a1
nativeLineASlow:
	movem.l %d0-%d7/%a0-%a6,nativeRegisters
	jsr nativeClockEnter
	tst.w nativeClockCalibrating
	bne nativeClockCalibrationTrap
	moveq #10,%d0
	bra nativeSave
nativeTrace:
	move.w #0x2700,%sr
	stopclock
	| A traced TRAP on a 68000 also traces the exception entry itself.
	| Leave its pending game TRAP frame for our TRAP entry to consume.
	btst #5,(%sp)
	beq nativeGameTrace
	cmpi.l #nativeTrap0,2(%sp)
	bcs nativeGameTrace
	cmpi.l #nativeSave,2(%sp)
	bcc nativeGameTrace
	rte
nativeGameTrace:
	| Still trace every original instruction, but avoid a C dispatch between
	| scheduled boundaries. Hooks/TRAPs always use their full handlers. The
	| actual PC remains range-checked and RTE restores every CCR bit.
	tst.l nativeFastBoundary
	beq nativeSlowTrace
	move.l %d0,-(%sp)
	move.l 6(%sp),%d0
	cmp.l nativeRomBegin,%d0
	bcs nativeFastRam
	cmp.l nativeRomEnd,%d0
	bcs nativeFastCount
nativeFastRam:
	cmp.l nativeRamBegin,%d0
	bcs nativeFastDispatch
	cmp.l nativeRamEnd,%d0
	bcc nativeFastDispatch
nativeFastCount:
	move.l nativeInstructions,%d0
	addq.l #1,%d0
	cmp.l nativeFastBoundary,%d0
	bcc nativeFastDispatch
	move.l %d0,nativeInstructions
	move.l (%sp)+,%d0
	rte
nativeFastDispatch:
	move.l (%sp)+,%d0
nativeSlowTrace:
	movem.l %d0-%d7/%a0-%a6,nativeRegisters
	jsr nativeClockEnter
	moveq #9,%d0
	bra nativeSave
nativeFault:
	move.w #0x2700,%sr
	stopclock
	movem.l %d0-%d7/%a0-%a6,nativeRegisters
	jsr nativeClockEnter
	moveq #0,%d0
	bra nativeSave
	.macro trapentry number
	.globl nativeTrap\number
nativeTrap\number:
	move.w #0x2700,%sr
	stopclock
	movem.l %d0-%d1/%a0-%a1,-(%sp)
	moveq #\number,%d1
	bra nativeTrapShort
	.endm
	.irp number,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
	trapentry \number
	.endr
	.globl nativeTrapShort,nativeTrapGuard,nativeTrapAdmitted,nativeTrapDecline,nativeShortTrapRead
nativeTrapShort:
	tst.w nativeShortEnabled
	beq nativeTrapDecline
	tst.w nativeDiagnostic
	bne nativeTrapGuard
	btst #7,16(%sp)
	bne nativeTrapDecline
nativeTrapGuard:
	| User entry uses the authoritative saved supervisor stack. All guards
	| run before either stack bank or exception-frame memory is modified.
	move.w nativeRegisters+68,%d0
	btst #15,%d0
	bne nativeTrapDecline
	btst #13,%d0
	bne nativeTrapModeReady
	tst.w nativeUserTrapEnabled
	beq nativeTrapDecline
nativeTrapModeReady:
	move.l 18(%sp),%a0
	subq.l #2,%a0
	cmpa.l nativeRomBegin,%a0
	bcs nativeTrapRamPc
	cmpa.l nativeRomEnd,%a0
	bcs nativeTrapOpcode
nativeTrapRamPc:
	cmpa.l nativeRamBegin,%a0
	bcs nativeTrapDecline
	cmpa.l nativeRamEnd,%a0
	bcc nativeTrapDecline
nativeTrapOpcode:
	move.w %d1,%d0
	ori.w #0x4e40,%d0
	cmp.w (%a0),%d0
	bne nativeTrapDecline
	move.l %usp,%a1
	btst #5,nativeRegisters+68
	bne nativeTrapStackReady
	move.l nativeVirtualSsp,%a1
nativeTrapStackReady:
	move.l %a1,%d0
	btst #0,%d0
	bne nativeTrapDecline
	cmpa.l nativeRamEnd,%a1
	bcc nativeTrapDecline
	subq.l #6,%d0
	bcs nativeTrapDecline
	cmp.l nativeRamBegin,%d0
	bcs nativeTrapDecline
	move.l %d1,%d0
	lsl.w #5,%d0
	lea nativeShortTraps,%a1
	adda.w %d0,%a1
	| Vectors are relocated immutable ROM; range-check the target as well.
	move.l 4(%a1),%d0
	btst #0,%d0
	bne nativeTrapDecline
	cmp.l nativeRomBegin,%d0
	bcs nativeTrapRamTarget
	cmp.l nativeRomEnd,%d0
	bcs nativeTrapAdmitted
nativeTrapRamTarget:
	cmp.l nativeRamBegin,%d0
	bcs nativeTrapDecline
	cmp.l nativeRamEnd,%d0
	bcc nativeTrapDecline
nativeTrapAdmitted:
	tst.w nativeDiagnostic
	beq nativeShortLive
	addq.l #1,nativeInstructions
	bra nativeShortRead
nativeShortTrapRead:
	move.l %usp,%a0
	btst #5,nativeRegisters+68
	bne nativeTrapStackSelected
	move.l %a0,nativeVirtualUsp
	move.l nativeVirtualSsp,%a0
nativeTrapStackSelected:
	move.l 18(%sp),-(%a0)
	move.w nativeRegisters+68,%d0
	andi.w #0xa700,%d0
	move.w 16(%sp),%d1
	andi.w #31,%d1
	or.w %d1,%d0
	move.w %d0,-(%a0)
	move.l %a0,%usp
	ori.w #0x2000,%d0
	move.w %d0,nativeRegisters+68
	move.l 4(%a1),18(%sp)
	bra nativeShortLengthDone
nativeTrapDecline:
	move.w %d1,nativeTrapNumber
	movem.l (%sp)+,%d0-%d1/%a0-%a1
	movem.l %d0-%d7/%a0-%a6,nativeRegisters
	jsr nativeClockEnter
	moveq #32,%d0
	add.w nativeTrapNumber,%d0
	bra nativeSave
nativeSave:
	move.w #0x2700,%sr
	move.l %usp,%a0
	move.l %a0,nativeRegisters+60
	move.l 2(%sp),nativeRegisters+64
	move.w (%sp),nativePhysicalSr
	| 68020 trace exceptions carry a format-2 instruction-address longword.
	| Line-A/TRAP use format 0. Removing only eight bytes leaks the extra
	| four on each VBI-return trace and eventually corrupts the service stack.
	tst.w nativeExtendedFrame
	beq 3f
	move.w 6(%sp),%d1
	andi.w #0xf000,%d1
	beq 3f
	cmpi.w #0x2000,%d1
	bne 4f
	addq.l #4,%sp
	bra 3f
4:
	moveq #0,%d0
3:
	adda.w nativeFrameBytes,%sp
	move.l %d0,-(%sp)
	jsr nativeDispatch
	addq.l #4,%sp
	tst.l %d0
	beq nativeExit
nativeResume:
	move.l nativeRegisters+64,nativeClockResumePc
	move.l nativeRegisters+60,%a0
	move.l %a0,%usp
	tst.w nativeExtendedFrame
	beq 2f
	clr.w -(%sp)
2:
	move.l nativeRegisters+64,-(%sp)
	move.w nativePhysicalResume,-(%sp)
	jsr nativeClockLeave
	tst.w nativeClockEnabled
	beq nativeResumeUnclocked
	movem.l nativeRegisters,%d0-%d7/%a0-%a6
	move.b #0x11,0xbfee01
	rte
nativeResumeUnclocked:
	movem.l nativeRegisters,%d0-%d7/%a0-%a6
	rte
nativeExit:
	move.w #0x2700,%sr
	jsr nativeRestoreVectors
	move.l nativeOsUsp,%a0
	move.l %a0,%usp
	move.l nativeReturnStack,%sp
	movem.l (%sp)+,%d2-%d7/%a2-%a6
	rte

	.globl nativeClockCalibrationCode
nativeClockCalibrationCode:
	nop
	.word 0xa000
nativeClockCalibrationTrap:
	adda.w nativeFrameBytes,%sp
	jsr nativeClockCalibrateNext
	bra nativeResume

	| Called only in supervisor mode; MOVEC is conditional on Exec CPU flags.
	.globl nativeReadVbr
nativeReadVbr:
	moveq #0,%d0
	tst.w nativeExtendedFrame
	beq 1f
	.word 0x4e7a,0x0801	| MOVEC VBR,D0 (68010+)
1:
	rts

	.globl nativeWriteVbr
nativeWriteVbr:
	move.l 4(%sp),%d0
	.word 0x4e7b,0x0801	| MOVEC D0,VBR (only called on 68010+)
	rts

	| Synthetic timing probes, not replacements for original game code.
	| 8192 iterations, D0 and A0 supplied on the private calibration context.
	.globl nativeSpeedLoop,nativeSpeedMemory,nativeSpeedArithmetic
nativeSpeedLoop:
	subq.w #1,%d0
	bne.s nativeSpeedLoop
	.word 0xa000
nativeSpeedMemory:
	move.b (%a0),%d1
	subq.w #1,%d0
	bne.s nativeSpeedMemory
	.word 0xa000
nativeSpeedArithmetic:
	add.l %d1,%d2
	eor.l %d2,%d3
	subq.w #1,%d0
	bne.s nativeSpeedArithmetic
	.word 0xa000

	| Isolated whole-batch timing of real exception entry and RTE. Only an
	| explicit pre-game benchmark temporarily admits this synthetic site.
	.globl nativeShortBenchmarkLoop,nativeShortBenchmarkControl,nativeShortBenchmarkOpcode
nativeShortBenchmarkLoop:
	move.l %d7,-(%sp)
	move.l nativeShortStatus+4,%a0
	move.w #511,%d7
nativeShortBenchmarkOpcode:
	.word 0xa000
	nop
	dbra %d7,nativeShortBenchmarkOpcode
	move.l (%sp)+,%d7
	rts
 .ifdef POKERI_FIFO_CONTROL_FUSION
	| Synthetic address/CCR/address writes for whole-batch comparison.
	.globl nativeFifoControlBenchmark,nativeFifoControlFirst,nativeFifoControlMiddle,nativeFifoControlLast,nativeFifoControlEnd
nativeFifoControlBenchmark:
	move.l %d7,-(%sp)
	move.l nativeShortStatus+4,%a0
	move.w #511,%d7
nativeFifoControlFirst:
	.word 0xa000,3
nativeFifoControlMiddle:
	.word 0xa001,0x80,2
nativeFifoControlLast:
	.word 0xa002,0
nativeFifoControlEnd:
	dbra %d7,nativeFifoControlFirst
	move.l (%sp)+,%d7
	rts
 .endif
	.globl nativeStackBenchmarkLoop,nativeStackBenchmarkOpcode
nativeStackBenchmarkLoop:
	move.l %d7,-(%sp)
	move.l %usp,%a0
	move.l %a0,-(%sp)
	move.l %a0,nativeVirtualUsp
	move.w #511,%d7
nativeStackBenchmarkIteration:
	move.w #0x2700,nativeRegisters+68
nativeStackBenchmarkOpcode:
	.word 0xa000
	nop
	dbra %d7,nativeStackBenchmarkIteration
	move.l (%sp)+,%a0
	move.l %a0,%usp
	move.l (%sp)+,%d7
	rts
	.globl nativeUserTrapBenchmarkLoop,nativeUserTrapBenchmarkOpcode,nativeUserTrapBenchmarkTarget
nativeUserTrapBenchmarkLoop:
	move.l %d7,-(%sp)
	move.l %usp,%a0
	move.l %a0,-(%sp)
	move.w #511,%d7
nativeUserTrapBenchmarkIteration:
	move.w #0,nativeRegisters+68
	move.l nativeRamBegin,%a0
	adda.l #0x10000,%a0
	move.l %a0,%usp
nativeUserTrapBenchmarkOpcode:
	trap #5
nativeUserTrapBenchmarkTarget:
	dbra %d7,nativeUserTrapBenchmarkIteration
	move.l (%sp)+,%a0
	move.l %a0,%usp
	move.l (%sp)+,%d7
	rts
nativeShortBenchmarkControl:
	move.l %d7,-(%sp)
	move.l nativeShortStatus+4,%a0
	move.w #511,%d7
nativeShortControlLoop:
	dbra %d7,nativeShortControlLoop
	move.l (%sp)+,%d7
	rts


	| Approved opt-in experiment: exactly BTST / BEQ / MOVE.W. All saved
	| guest registers stay on the usual private short frame. Each boundary
	| may promote without repeating its already completed instruction.
	.globl nativeShortFeedRead,nativeFeedBoundary0,nativeFeedBoundary1,nativeFeedSource,nativeFeedExit
nativeShortFeedRead:
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedTests
.endif
	move.b nativeCachedVideoStatus,%d0
	btst #1,%d0
	beq nativeFeedNotReady
	andi.w #0xfffb,16(%sp)
	bra nativeFeedStatusDone
nativeFeedNotReady:
	ori.w #4,16(%sp)
nativeFeedStatusDone:
	addq.l #4,18(%sp)
	| Publish the deferred status charge even if an intermediate event exits.
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeShortCalls
.endif
nativeFeedBoundary0:
	bsr nativeFeedBoundary
	tst.l %d0
	beq nativeFeedExit
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedBranches
.endif
	btst #2,17(%sp)
	beq nativeFeedBranchFalse
	move.l nativeFeedTarget,18(%sp)
	moveq #10,%d0
	bra nativeFeedBranchDone
nativeFeedBranchFalse:
	addq.l #2,18(%sp)
	moveq #8,%d0
nativeFeedBranchDone:
	tst.w nativeDiagnostic
	beq nativeFeedBranchLive
	addq.l #1,nativeInstructions
	bra nativeFeedBoundary1
nativeFeedBranchLive:
	add.l %d0,nativeShortNominal
nativeFeedBoundary1:
	bsr nativeFeedBoundary
	tst.l %d0
	beq nativeFeedExit
	btst #2,17(%sp)
	bne nativeFeedFinished
nativeFeedSource:
	| The unchanged A0 was admitted at the status endpoint. Preparation
	| proves the second descriptor is that endpoint +2. Validate the word
	| source before any postincrement or write, including odd/wrapped EAs.
	move.l 12(%sp),%d0
	btst #0,%d0
	bne nativeFeedExit
	addq.l #2,%d0
	bcs nativeFeedExit
	move.l 12(%sp),%a0
	cmpa.l nativeRomBegin,%a0
	bcs nativeFeedRam
	cmp.l nativeRomEnd,%d0
	bls nativeFeedSourceReady
nativeFeedRam:
	cmpa.l nativeRamBegin,%a0
	bcs nativeFeedExit
	cmp.l nativeRamEnd,%d0
	bhi nativeFeedExit
nativeFeedSourceReady:
.ifdef POKERI_DISPATCH_COUNTS
	tst.w nativeProfileEnabled
	beq nativeFeedStatusCounted
	addq.l #1,12(%a1)
nativeFeedStatusCounted:
.endif
	moveq #0,%d1
	move.w (%a0),%d1
	move.l 28(%a1),%a1
	move.l 18(%sp),%a0
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedWrites
.endif
	tst.w nativeDiagnostic
	bne nativeShortAdmitted
	| The second hook shares the stopped clock; charge no service interval.
	addq.l #1,nativeInstructions
	move.w #0x2000,%sr
	bra nativeShortNominalOnly
nativeFeedFinished:
	bra nativeShortLengthDone
nativeFeedExit:
.ifdef POKERI_DISPATCH_COUNTS
	tst.w nativeProfileEnabled
	beq nativeFeedExitCounted
	addq.l #1,12(%a1)
nativeFeedExitCounted:
.endif
	bra nativeShortControlPromote

nativeFeedBoundary:
	| BSR adds four bytes; no helper may alter the descriptor or stacked CCR.
	move.w #0x2700,%sr
	tst.w nativeDiagnostic
	bne nativeFeedReplayBoundary
	moveq #0,%d0
	move.l pendingFrames,%d1
	cmp.l seenFrames,%d1
	bne nativeFeedBoundaryReturn
	btst #1,nativeShortPending+1
	bne nativeFeedBoundaryReturn
	moveq #1,%d0
nativeFeedBoundaryReturn:
	rts
nativeFeedReplayBoundary:
	move.l %a1,-(%sp)
	jsr nativeFeedReplayContinue
	move.l (%sp)+,%a1
	rts

	| Authorized whole-feed experiment. The first CMPA/BEQ still executes in
	| guest code. After each admitted write, run the verified loop tail here.
	| Every original instruction has an exact resumable PC/CCR boundary.
	| No guest register beyond the standard short frame is scratch storage.
	.macro feedflags
	move.w %sr,%d0
	andi.w #15,%d0
	andi.w #0xfff0,16(%sp)
	or.w %d0,16(%sp)
	.endm
	.macro feedstep length,cycles
	addq.l #\length,18(%sp)
	tst.w nativeDiagnostic
	beq 1f
	addq.l #1,nativeInstructions
	bra 2f
1:
	addi.l #\cycles,nativeShortNominal
2:
	bsr nativeFeedBoundary
	tst.l %d0
	beq nativeFeedLoopExit
	.endm
	.globl nativeShortFeedLoopWrite,nativeFeedLoopAfterWrite,nativeFeedLoopExit
nativeShortFeedLoopWrite:
	addq.l #2,12(%sp)
	bsr nativeFeedAcceptWord
nativeFeedLoopValueReady:
	tst.w %d0
	feedflags
	addq.l #4,18(%sp)
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeShortCalls
.endif
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedLoopWords
.endif
.ifdef POKERI_DISPATCH_COUNTS
	tst.w nativeProfileEnabled
	beq 1f
	addq.l #1,12(%a1)
1:
.endif
nativeFeedLoopAfterWrite:
	| Exit before the next FIFO word when this completed a visible step.
	| Only scratch D0 changes; the original MOVE flags are already stacked.
	move.l nativeShuffleNextPointer,%d0
	beq 1f
	cmp.l 12(%sp),%d0
	bne 1f
	ori.w #2,nativeShortPending
1:
	bsr nativeFeedBoundary
	tst.l %d0
	beq nativeFeedLoopExit
	tst.w nativeDiagnostic
	bne nativeFeedLoopSlowTail
	tst.w nativeFeedLoopFast
	beq nativeFeedLoopSlowTail
	tst.w nativeRegisterFeedEnabled
	bne nativeRegisterFeedBegin
	bra nativeFeedLoopLiveTail
nativeFeedLoopSlowTail:
	| CMPA.L D0,A1; BCS loop-head.
	move.l 12(%sp),%a0
	cmpa.l (%sp),%a0
	feedflags
	feedstep 2,6
	btst #0,17(%sp)
	beq nativeFeedLoopAtEnd
	| Taken BCS: target is four bytes before the status descriptor PC.
	move.l 28(%a1),%a0
	move.l (%a0),%d0
	subq.l #6,%d0
	move.l %d0,18(%sp)
	feedstep 2,10
	bra nativeFeedLoopHead
nativeFeedLoopAtEnd:
	feedstep 2,8
	| CMP.L D0,D1; BEQ exit.
	move.l 4(%sp),%d1
	cmp.l (%sp),%d1
	feedflags
	feedstep 2,6
	btst #2,17(%sp)
	bne nativeFeedLoopEqual
	feedstep 2,8
	| MOVEA.L ring-start(A6),A1. Validate the real load before reading it.
	lea -30530(%a6),%a0
	move.l %a0,%d0
	btst #0,%d0
	bne nativeFeedLoopExit
	cmpa.l nativeRamBegin,%a0
	bcs nativeFeedLoopExit
	addq.l #4,%d0
	bcs nativeFeedLoopExit
	cmp.l nativeRamEnd,%d0
	bhi nativeFeedLoopExit
	move.l (%a0),12(%sp)
	feedstep 4,16
	| BRA loop-head. MOVEA and branches do not alter the saved CCR.
	move.l 28(%a1),%a0
	move.l (%a0),%d0
	subq.l #6,%d0
	move.l %d0,18(%sp)
	feedstep 2,10
nativeFeedLoopHead:
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedLoopTurns
.endif
	| CMPA.L D1,A1; BEQ exit.
	move.l 12(%sp),%a0
	cmpa.l 4(%sp),%a0
	feedflags
	feedstep 2,6
	btst #2,17(%sp)
	bne nativeFeedLoopEqual
	feedstep 2,8
nativeFeedLoopContinue:
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedLoopSaved
.endif
	| The next status access uses its ordinary descriptor and replay event.
	move.l 28(%a1),%a1
	move.l 18(%sp),%a0
	tst.w nativeDiagnostic
	bne nativeShortAdmitted
	addq.l #1,nativeInstructions
	move.w #0x2000,%sr
	bra nativeShortNominalOnly
	| Only the live non-I/O tail is collapsed. The word boundary above still
	| handles frame/IRQ/fault work; diagnostic replay keeps every instruction
	| boundary. This bounded masked block never calls a device or service.
	| D1 is scratch: use it for the exact nominal cycle total.
nativeFeedLoopLiveTail:
	move.l 12(%sp),%a0
	cmpa.l (%sp),%a0
	bcs nativeFeedLoopLiveWithin
	move.l 4(%sp),%d1
	cmp.l (%sp),%d1
	beq nativeFeedLoopLiveEnd
	| Guard the ring-start load before modifying any saved guest state. On
	| failure the general tail publishes its precise pre-load boundary.
	lea -30530(%a6),%a0
	move.l %a0,%d0
	btst #0,%d0
	bne nativeFeedLoopSlowTail
	cmpa.l nativeRamBegin,%a0
	bcs nativeFeedLoopSlowTail
	addq.l #4,%d0
	bcs nativeFeedLoopSlowTail
	cmp.l nativeRamEnd,%d0
	bhi nativeFeedLoopSlowTail
	move.l (%a0),12(%sp)
	moveq #54,%d1
	bra nativeFeedLoopLiveHead
nativeFeedLoopLiveWithin:
	moveq #16,%d1
nativeFeedLoopLiveHead:
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedLoopTurns
.endif
	move.l 12(%sp),%a0
	cmpa.l 4(%sp),%a0
	feedflags
	btst #2,17(%sp)
	bne nativeFeedLoopLiveEmpty
	addi.w #14,%d1
	add.l %d1,nativeShortNominal
	move.l 28(%a1),%a0
	move.l (%a0),18(%sp)
	bra nativeFeedLoopContinue
nativeFeedLoopLiveEnd:
	feedflags
	moveq #30,%d1
	bra nativeFeedLoopLiveExit
nativeFeedLoopLiveEmpty:
	addi.w #16,%d1
nativeFeedLoopLiveExit:
	add.l %d1,nativeShortNominal
	move.l nativeFeedTarget,18(%sp)
	move.l 18(%sp),nativeClockResumePc
	bra nativeShortNoControlDue

nativeFeedLoopEqual:
	move.l nativeFeedTarget,%d0
	subq.l #2,%d0
	move.l %d0,18(%sp)
	feedstep 2,10
nativeFeedLoopExit:
	move.l 18(%sp),nativeClockResumePc
	bra nativeShortControlPromote

	| Verified register-resident live loop. The old frame is materialized on
	| every exit, at exactly the same status/branch/write boundaries.
	| A2=frame, A3=cursor, A4=write descriptor, A5=status descriptor;
	| D2=end, D3=producer, D4=CCR, D5=PC. D6 holds a tail cycle charge.
	.macro registerfeedflags
	move.w %sr,%d0
	andi.w #15,%d0
	andi.w #0xfff0,%d4
	or.w %d0,%d4
	.endm
nativeRegisterFeedBegin:
 .ifdef POKERI_FEED_SOURCE_SPAN
	movem.l %d2-%d7/%a2-%a5,-(%sp)
	lea 40(%sp),%a2
	moveq #0,%d7
 .else
	movem.l %d2-%d6/%a2-%a5,-(%sp)
	lea 36(%sp),%a2
 .endif
	move.l (%a2),%d2
	move.l 4(%a2),%d3
	move.l 12(%a2),%a3
	move.w 16(%a2),%d4
	move.l 18(%a2),%d5
	move.l %a1,%a4
	move.l 28(%a1),%a5
nativeRegisterFeedTail:
	cmpa.l %d2,%a3
	bcs nativeRegisterFeedWithin
	cmp.l %d2,%d3
	beq nativeRegisterFeedEnd
	| A bad ring-start load falls back before changing the saved word state.
	lea -30530(%a6),%a0
	move.l %a0,%d0
	btst #0,%d0
	bne nativeRegisterFeedFallback
	cmpa.l nativeRamBegin,%a0
	bcs nativeRegisterFeedFallback
	addq.l #4,%d0
	bcs nativeRegisterFeedFallback
	cmp.l nativeRamEnd,%d0
	bhi nativeRegisterFeedFallback
	move.l (%a0),%a3
 .ifdef POKERI_FEED_SOURCE_SPAN
	moveq #0,%d7
 .endif
	moveq #54,%d6
	bra nativeRegisterFeedHead
nativeRegisterFeedWithin:
	moveq #16,%d6
	.globl nativeInlineBoundaryMode,nativeRegisterBoundary0,nativeRegisterBoundary1,nativeRegisterBoundary2
.ifdef POKERI_INLINE_BOUNDARY
	.set nativeInlineBoundaryMode,1
.else
	.set nativeInlineBoundaryMode,0
.endif
	.ifndef POKERI_JOIN_BRANCH_BOUNDARY
	.set POKERI_JOIN_BRANCH_BOUNDARY,0
	.endif
	.globl nativeJoinedBoundaryMode
	.set nativeJoinedBoundaryMode,POKERI_JOIN_BRANCH_BOUNDARY
	.macro registerboundary number
nativeRegisterBoundary\number:
	| Boundary 0 leaves IPL7 set. Only local branch bookkeeping follows;
	| no IRQ or device call can change either scheduler field before 1.
	.if (\number != 1) || (POKERI_JOIN_BRANCH_BOUNDARY == 0)
.ifdef POKERI_INLINE_BOUNDARY
	| This loop is live-only. Keep the same physical mask, frame and pending
	| checks at all three original device-instruction boundaries.
	move.w #0x2700,%sr
	move.l pendingFrames,%d0
	cmp.l seenFrames,%d0
	bne nativeRegisterFeedPromote
	btst #1,nativeShortPending+1
	bne nativeRegisterFeedPromote
.else
	bsr nativeFeedBoundary
	tst.l %d0
	beq nativeRegisterFeedPromote
.endif
	.endif
	.endm
nativeRegisterFeedHead:
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedLoopTurns
.endif
	cmpa.l %d3,%a3
	registerfeedflags
	btst #2,%d4
	bne nativeRegisterFeedEmpty
	addi.w #14,%d6
	add.l %d6,nativeShortNominal
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedLoopSaved
.endif
	move.l %a5,%a1
	move.l (%a5),%d5
	addq.l #1,nativeInstructions
	move.w #0x2000,%sr
	addi.l #12,nativeShortNominal
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedTests
.endif
	btst #1,nativeCachedVideoStatus
	beq nativeRegisterFeedNotReady
	andi.w #0xfffb,%d4
	bra nativeRegisterFeedStatusDone
nativeRegisterFeedNotReady:
	ori.w #4,%d4
nativeRegisterFeedStatusDone:
	addq.l #4,%d5
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeShortCalls
.endif
	registerboundary 0
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedBranches
.endif
	btst #2,%d4
	beq nativeRegisterFeedBranchFalse
	move.l nativeFeedTarget,%d5
	addi.l #10,nativeShortNominal
	bra nativeRegisterFeedBranchDone
nativeRegisterFeedBranchFalse:
	addq.l #2,%d5
	addi.l #8,nativeShortNominal
nativeRegisterFeedBranchDone:
	registerboundary 1
	btst #2,%d4
	bne nativeRegisterFeedFinished
 .ifdef POKERI_FEED_SOURCE_SPAN
	| D7 is the exclusive last legal start bound (region end minus one).
	| Zero cannot admit an unsigned address. Cursor only advances by two;
	| every ring wrap and promotion discards the previous span.
	cmpa.l %d7,%a3
	bcs nativeRegisterFeedSourceReady
 .endif
	| Keep the complete source guard, before loading or incrementing it.
	move.l %a3,%d0
	btst #0,%d0
	bne nativeRegisterFeedPromote
	addq.l #2,%d0
	bcs nativeRegisterFeedPromote
	cmpa.l nativeRomBegin,%a3
	bcs nativeRegisterFeedRam
	cmp.l nativeRomEnd,%d0
 .ifdef POKERI_FEED_SOURCE_SPAN
	bhi nativeRegisterFeedRam
	move.l nativeRomEnd,%d7
	subq.l #1,%d7
	bra nativeRegisterFeedSourceReady
 .else
	bls nativeRegisterFeedSourceReady
 .endif
nativeRegisterFeedRam:
	cmpa.l nativeRamBegin,%a3
	bcs nativeRegisterFeedPromote
	cmp.l nativeRamEnd,%d0
	bhi nativeRegisterFeedPromote
 .ifdef POKERI_FEED_SOURCE_SPAN
	move.l nativeRamEnd,%d7
	subq.l #1,%d7
 .endif
nativeRegisterFeedSourceReady:
.ifdef POKERI_DISPATCH_COUNTS
	tst.w nativeProfileEnabled
	beq 1f
	addq.l #1,12(%a1)
1:
.endif
	moveq #0,%d1
	move.w (%a3)+,%d1
	move.l %a4,%a1
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedWrites
.endif
	addq.l #1,nativeInstructions
	move.w #0x2000,%sr
	addi.l #16,nativeShortNominal
	bsr nativeFeedAcceptWord
	tst.w %d0
	registerfeedflags
	addq.l #4,%d5
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeShortCalls
	addq.l #1,nativeFeedLoopWords
.endif
.ifdef POKERI_DISPATCH_COUNTS
	tst.w nativeProfileEnabled
	beq 1f
	addq.l #1,12(%a1)
1:
.endif
	move.l nativeShuffleNextPointer,%d0
	beq 1f
	cmp.l %a3,%d0
	bne 1f
	ori.w #2,nativeShortPending
1:
	registerboundary 2
	bra nativeRegisterFeedTail
nativeRegisterFeedEnd:
	registerfeedflags
	moveq #30,%d6
	bra nativeRegisterFeedExit
nativeRegisterFeedEmpty:
	addi.w #16,%d6
nativeRegisterFeedExit:
	add.l %d6,nativeShortNominal
	move.l nativeFeedTarget,%d5
	bsr nativeRegisterFeedStore
 .ifdef POKERI_FEED_SOURCE_SPAN
	movem.l (%sp)+,%d2-%d7/%a2-%a5
 .else
	movem.l (%sp)+,%d2-%d6/%a2-%a5
 .endif
	bra nativeShortNoControlDue
nativeRegisterFeedFallback:
	bsr nativeRegisterFeedStore
 .ifdef POKERI_FEED_SOURCE_SPAN
	movem.l (%sp)+,%d2-%d7/%a2-%a5
 .else
	movem.l (%sp)+,%d2-%d6/%a2-%a5
 .endif
	bra nativeFeedLoopSlowTail
nativeRegisterFeedFinished:
	bsr nativeRegisterFeedStore
 .ifdef POKERI_FEED_SOURCE_SPAN
	movem.l (%sp)+,%d2-%d7/%a2-%a5
 .else
	movem.l (%sp)+,%d2-%d6/%a2-%a5
 .endif
	bra nativeShortLengthDone
nativeRegisterFeedPromote:
.ifdef POKERI_DISPATCH_COUNTS
	cmpa.l %a5,%a1
	bne 1f
	tst.w nativeProfileEnabled
	beq 1f
	addq.l #1,12(%a1)
1:
.endif
	bsr nativeRegisterFeedStore
 .ifdef POKERI_FEED_SOURCE_SPAN
	movem.l (%sp)+,%d2-%d7/%a2-%a5
 .else
	movem.l (%sp)+,%d2-%d6/%a2-%a5
 .endif
	bra nativeShortControlPromote
nativeRegisterFeedStore:
	move.l %a3,12(%a2)
	move.w %d4,16(%a2)
	move.l %d5,18(%a2)
	move.l %d5,nativeClockResumePc
	rts

	| Shared word acceptance; only D0/D1/A0 may be clobbered, A1 is retained.
	| Both loop implementations use the same model grant and opcode decoder.
nativeFeedAcceptWord:
	tst.l nativeFeedInlineCount
	beq nativeFeedLoopTryHeader
	| A model-granted span contains no opcode, variable count or final word.
	| It is invalidated before every scheduler boundary and return to guest.
	move.l nativeFeedInlineWord,%a0
	move.w %d1,(%a0)+
	move.l %a0,nativeFeedInlineWord
	move.l nativeFeedInlinePending,%a0
	addq.l #1,(%a0)
	move.l %d1,%d0
	lsr.w #8,%d0
	move.l nativeFeedInlineHigh,%a0
	move.b %d0,(%a0)
	subq.l #1,nativeFeedInlineCount
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedInlineWords
.endif
	move.l %d1,%d0
	bra nativeFeedAccepted
nativeFeedLoopTryHeader:
	tst.l nativeFeedHeaderGrant
	beq nativeFeedLoopCallModel
	move.l %a1,-(%sp)
	move.l %d1,%d0
	andi.l #0xfc00,%d0
	lsr.l #8,%d0
	move.l nativeFeedFormats,%a0
	adda.l %d0,%a0
	move.w 2(%a0),%d0
	and.w %d1,%d0
	bne nativeFeedHeaderRejected
	move.w (%a0),%d0
	beq nativeFeedHeaderRejected
	cmpi.w #1,%d0
	beq nativeFeedHeaderRejected
	ext.l %d0
	move.l nativeFeedInlineLength,%a1
	move.l %d0,(%a1)
	subq.l #2,%d0
	bpl nativeFeedHeaderSpan
	moveq #0,%d0
nativeFeedHeaderSpan:
	move.l %d0,nativeFeedInlineCount
	clr.l nativeFeedHeaderGrant
	move.l nativeFeedInlineWord,%a0
	move.w %d1,(%a0)+
	move.l %a0,nativeFeedInlineWord
	move.l nativeFeedInlinePending,%a0
	move.l #1,(%a0)
	move.l %d1,%d0
	lsr.w #8,%d0
	move.l nativeFeedInlineHigh,%a0
	move.b %d0,(%a0)
	andi.b #0xdf,nativeCachedVideoStatus
.ifdef POKERI_FEED_COUNTS
	addq.l #1,nativeFeedHeaderWords
.endif
.ifdef POKERI_TIME_LEDGER
.ifndef POKERI_LEDGER_FAST_CACHE
	| Fast-cache mode records successful recognition in the model instead.
	move.l %d1,-(%sp)
	jsr nativeFeedHeaderStarted
	move.l (%sp)+,%d1
.endif
.endif
	move.l (%sp)+,%a1
	move.l %d1,%d0
	bra nativeFeedAccepted
nativeFeedHeaderRejected:
	move.l (%sp)+,%a1
nativeFeedLoopCallModel:
.ifdef POKERI_CACHED_RASTER
	tst.l nativeRasterGrantActive
	beq 9f
	lea nativeRasterGrant,%a0
	jsr nativeCachedRasterComplete
	tst.l %d0
	beq 9f
	| CED cannot alter an enabled IRQ in a granted span. All original
	| instruction boundaries still test frames and pending promotion.
	ori.b #0x20,nativeCachedVideoStatus
	move.l nativeRasterGrant+16,nativeFeedInlineWord
	move.l nativeRasterGrant+32,nativeFeedInlinePending
	move.l nativeRasterGrant+40,nativeFeedInlineHigh
	move.l nativeRasterGrant+36,nativeFeedInlineLength
	move.l #1,nativeFeedHeaderGrant
	addq.l #1,nativeRasterHits
	move.l %d1,%d0
	rts
9:
.endif
	move.l %a1,-(%sp)
	move.l #7,-(%sp)
	move.l %d1,-(%sp)
	move.l 4(%a1),-(%sp)
	jsr nativeShortVideoWriteValue
	lea 12(%sp),%sp
	move.l (%sp)+,%a1
nativeFeedAccepted:
	rts

	.globl nativeFeedBenchmarkLoop,nativeFeedBenchmarkOpcode,nativeFeedBenchmarkWrite,nativeFeedBenchmarkTarget
nativeFeedBenchmarkLoop:
	move.l %d7,-(%sp)
	move.l nativeShortStatus+4,%a0
	move.l nativeRamBegin,%a1
	move.w #511,%d7
nativeFeedBenchmarkOpcode:
	.word 0xa000
	nop
	beq.s nativeFeedBenchmarkTarget
nativeFeedBenchmarkWrite:
	.word 0xa001,2
nativeFeedBenchmarkTarget:
	dbra %d7,nativeFeedBenchmarkOpcode
	move.l (%sp)+,%d7
	rts

	| Synthetic contiguous command ring for paired three-instruction/full-loop
	| measurements. Parameters are newly constructed WPR words, no game data.
	.globl nativeRingBenchmark,nativeRingHead,nativeRingStatus,nativeRingWrite,nativeRingExit
nativeRingBenchmark:
	move.l nativeShortStatus+4,%a0
	move.l nativeRamBegin,%a1
	move.l %a1,%d0
.ifdef POKERI_CACHED_RASTER
	add.l nativeRasterBenchBytes,%d0
.else
	addi.l #1024,%d0
.endif
	move.l %d0,%d1
nativeRingHead:
	cmpa.l %d1,%a1
	beq.s nativeRingExit
nativeRingStatus:
	.word 0xa000
	nop
	beq.s nativeRingExit
nativeRingWrite:
	.word 0xa001,2
	cmpa.l %d0,%a1
	bcs.s nativeRingHead
	cmp.l %d0,%d1
	beq.s nativeRingExit
	movea.l -30530(%a6),%a1
	bra.s nativeRingHead
nativeRingExit:
	rts

	| Pure register-state kernel for the approved delay-loop experiment.
	| C ABI: (Registers*, instruction count), count in 1..2*(D6.w or 65536).
	| Return exact original cycles. No clock/device effects occur here.
	.globl nativeDelayApply,nativeDelayApplyEnd
nativeDelayApply:
	move.l 4(%sp),%a0
	move.l 8(%sp),%d0
	move.l %d0,%a1
	addq.l #1,%d0
	lsr.l #1,%d0
	subq.l #1,%d0
	sub.w %d0,26(%a0)
	| The final real decrement supplies all five original CCR bits, including
	| borrow from zero and signed overflow at $8000. Upper D6 is untouched.
	subq.w #1,26(%a0)
	move.w %sr,%d1
	andi.w #31,%d1
	andi.w #0xffe0,68(%a0)
	or.w %d1,68(%a0)
	move.l %a1,%d0
	btst #0,%d0
	bne nativeDelayOdd
	tst.w 26(%a0)
	bne nativeDelayCycles
	addq.l #4,64(%a0)
	bra nativeDelayCycles
nativeDelayOdd:
	addq.l #2,64(%a0)
nativeDelayCycles:
	lsr.l #1,%d0
	move.l %d0,%d1
	lsl.l #3,%d1
	lsl.l #1,%d0
	add.l %d0,%d1
	lsl.l #1,%d0
	add.l %d0,%d1
	move.l %a1,%d0
	btst #0,%d0
	beq nativeDelayEvenCycles
	addq.l #4,%d1
	bra nativeDelayResult
nativeDelayEvenCycles:
	tst.w 26(%a0)
	bne nativeDelayResult
	subq.l #2,%d1
nativeDelayResult:
	move.l %d1,%d0
	rts
nativeDelayApplyEnd:

	| Exec audio server: A1 = PaulaStream. Queue a DMA slice, no synthesis.
	| Exec permits D0/D1/A0/A1 scratch. Remaining registers are untouched.
	.globl pokeriPaulaStream
pokeriPaulaStream:
	movea.l 8(%a1),%a0
	cmpa.l 4(%a1),%a0
	bcs paulaStreamWithin
	movea.l (%a1),%a0
paulaStreamWithin:
	move.l 4(%a1),%d0
	sub.l %a0,%d0
	cmpi.l #256,%d0
	bls paulaStreamTail
	move.l #256,%d0
paulaStreamTail:
	move.l %a0,%d1
	adda.l %d0,%a0
	move.l %a0,8(%a1)
	movea.l 12(%a1),%a0
	move.l %d1,(%a0)
	lsr.l #1,%d0
	move.w %d0,4(%a0)
	move.w 16(%a1),%d0
	move.w %d0,0xdff09c
	move.w %d0,0xdff09c
	addq.l #1,20(%a1)
	moveq #0,%d0
	rts

	.globl pokeriPaulaStreamEnd
pokeriPaulaStreamEnd:
