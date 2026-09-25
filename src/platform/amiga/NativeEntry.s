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
	lsl.l #4,%d0
	lea nativeShortStatus,%a1
	adda.l %d0,%a1
	cmpa.l (%a1),%a0
	bne nativeShortDecline
	tst.w 8(%a1)
	bmi nativeShortSentinelGuard
	move.l 8(%sp),%d0
	cmp.l 4(%a1),%d0
	bne nativeShortDecline
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
	tst.w 8(%a1)
	bmi nativeShortSentinelRead
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
nativeShortDone:
.ifdef POKERI_DISPATCH_COUNTS
	tst.w nativeProfileEnabled
	beq nativeShortUncounted
	addq.l #1,12(%a1)
nativeShortUncounted:
.endif
	addq.l #2,18(%sp)
	tst.w 8(%a1)
	bpl nativeShortFourBytes
	btst #0,9(%a1)
	bne nativeShortLengthDone
nativeShortFourBytes:
	addq.l #2,18(%sp)
nativeShortLengthDone:
	move.l 18(%sp),nativeClockResumePc
	addq.l #1,nativeShortCalls
	movem.l (%sp)+,%d0-%d1/%a0-%a1
	tst.w nativeDiagnostic
	bne nativeShortPromote
	| Live mode owns/enables this timer; diagnostic mode promoted above.
	move.b #0x11,0xbfee01
nativeShortReturn:
	rte
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
	movem.l %d0-%d7/%a0-%a6,nativeRegisters
	jsr nativeClockEnter
	moveq #(32+\number),%d0
	bra nativeSave
	.endm
	.irp number,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
	trapentry \number
	.endr
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
