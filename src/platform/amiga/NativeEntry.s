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
	| Only the common virtual-supervisor forms. Stack/trace transitions
	| retain the checked full handler, before any instruction side effect.
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
	andi.w #0xa000,%d0
	cmpi.w #0x2000,%d0
	bne nativeShortDecline
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
	andi.w #0xa000,%d0
	cmpi.w #0x2000,%d0
	bne nativeShortDecline
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
	bra nativeShortDone
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
	addq.l #1,nativeShortCalls
	move.l pendingFrames,%d0
	cmp.l seenFrames,%d0
	bne nativeShortControlPromote
	move.w nativeShortPending,%d0
	and.w 26(%a1),%d0
	bne nativeShortControlPromote
nativeShortNoControlDue:
	movem.l (%sp)+,%d0-%d1/%a0-%a1
	tst.w nativeDiagnostic
	bne nativeShortPromote
	| Live mode owns/enables this timer; diagnostic mode promoted above.
	move.b #0x11,0xbfee01
nativeShortReturn:
	rte
nativeShortControlPromote:
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
	| Only the virtual-supervisor case; no hidden writes on fallback.
	move.w nativeRegisters+68,%d0
	andi.w #0xa000,%d0
	cmpi.w #0x2000,%d0
	bne nativeTrapDecline
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
	move.l 18(%sp),-(%a0)
	move.w nativeRegisters+68,%d0
	andi.w #0xa700,%d0
	move.w 16(%sp),%d1
	andi.w #31,%d1
	or.w %d1,%d0
	move.w %d0,-(%a0)
	move.l %a0,%usp
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
nativeShortBenchmarkControl:
	move.l %d7,-(%sp)
	move.l nativeShortStatus+4,%a0
	move.w #511,%d7
nativeShortControlLoop:
	dbra %d7,nativeShortControlLoop
	move.l (%sp)+,%d7
	rts
