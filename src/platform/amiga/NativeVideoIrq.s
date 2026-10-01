	.text
	.even
	.globl nativeVideoIrqNoProfile
	.ifdef POKERI_NO_PROFILE_SUPPORT
	.set nativeVideoIrqNoProfile,1
	.else
	.set nativeVideoIrqNoProfile,0
	.endif
	.globl nativeCheckVideoIrq
	| C ABI pc, physical USP, physical SR -> validated SSP-6, or zero.
	| IPL7 on entry. Only D0/D1/A0/A1 scratch; no original instruction executed.
	| Fields are aliases emitted from their C++ owners, never stale snapshots.
nativeCheckVideoIrq:
	move.l %d2,-(%sp)
	tst.b irqDiagnostic
	bne .Ldecline
	.ifndef POKERI_NO_PROFILE_SUPPORT
	jsr nativeVideoIrqProfile
	tst.l %d0
	bne .Ldecline
	.endif
	tst.l nativeSetupReady
	beq .Ldecline
	cmpi.l #1,nativeStatus
	bne .Ldecline
	| Refuse already-pending scheduler work before validating stack/sources.
	| Deferred totals remain untouched for the ordinary dispatcher. All checks
	| repeat after accounting, so no decision survives an intervening PAL IRQ.
	tst.l irqLiveTicks
	bne .Ldecline
	move.l pendingFrames,%d0
	cmp.l seenFrames,%d0
	bne .Ldecline
	tst.b irqCompositionPending
	bne .Ldecline
	tst.l irqScreenPending
	bpl .Ldecline
	tst.l irqShuffleCount
	bne .Ldecline
	move.l 8(%sp),%d0
	sub.l irqRomBase,%d0
	cmpi.l #0x2ebc,%d0
	bne .Ldecline
	cmpi.w #2,nativeClockMode
	bne .Ldecline
	tst.w nativeClockEnabled
	beq .Ldecline
	tst.w nativeClockCalibrating
	bne .Ldecline
	tst.l nativeClockOverhead
	beq .Ldecline
	tst.b irqScreenActive
	beq 1f
	tst.b irqDisplayCalibrated
	beq .Ldecline
1:
	btst #5,18(%sp)
	bne .Ldecline
	move.l irqStopCycles,%d0
	beq 2f
	tst.l irqLiveCycles
	bne .Ldecline
	cmp.l irqLiveCycles+4,%d0
	bls .Ldecline
2:
	tst.b irqStartupFast
	bne .Ldecline
	move.w nativeRegisters+68,%d0
	bmi .Ldecline
	andi.w #0x700,%d0
	cmpi.w #0x500,%d0
	bcc .Ldecline
	move.l nativeVirtualSsp,%d2
	btst #5,nativeRegisters+68
	beq 3f
	move.l 12(%sp),%d2
3:
	btst #0,%d2
	bne .Ldecline
	cmpi.l #6,%d2
	bls .Ldecline
	move.l irqRamBase,%d0
	addq.l #6,%d0
	cmp.l %d0,%d2
	bcs .Ldecline
	addi.l #0x10000-6,%d0	| guest RAM window size (Board::mappedMemory-$40000)
	cmp.l %d0,%d2
	bcc .Ldecline
	move.l irqRom,%a0
	move.l 0x100(%a0),%d0
	btst #0,%d0
	bne .Ldecline
	cmp.l nativeRomBegin,%d0
	bcs 4f
	cmp.l nativeRomEnd,%d0
	bcs 5f
4:
	cmp.l nativeRamBegin,%d0
	bcs .Ldecline
	cmp.l nativeRamEnd,%d0
	bcc .Ldecline
5:
	jsr nativeVideoIrqSource
	tst.l %d0
	beq .Ldecline
	move.l %d0,-(%sp)
	move.w %sr,-(%sp)
	move.w #0x2000,%sr
	| Charge through the existing clock, in its existing order. Rejection
	| after this call drains totals exactly once for full-dispatch fallback.
	jsr nativeVideoClockPause
	tst.l irqLiveTicks
	bne .LrestoreDecline
	move.l pendingFrames,%d0
	cmp.l seenFrames,%d0
	bne .LrestoreDecline
	cmp.l irqClockFrame,%d0
	bne .LrestoreDecline
	tst.l irqClockCredit
	beq 6f
	tst.l irqClockDebt
	bne .LrestoreDecline
6:
	tst.b irqQuit
	bne .LrestoreDecline
	tst.l nativeShortDrained
	bne .LrestoreDecline
	tst.l irqShuffleCount
	bne .LrestoreDecline
	tst.b irqShuffleActive
	bne .LrestoreDecline
	tst.b irqShuffleQueued
	bne .LrestoreDecline
	tst.l nativeShuffleNextPointer
	bne .LrestoreDecline
	jsr nativeVideoIrqHeld
	tst.l %d0
	bne .LrestoreDecline
	tst.l irqScreenPending
	bpl .LrestoreDecline
	tst.b irqCompositionPending
	bne .LrestoreDecline
	move.w (%sp)+,%d0
	move.w %d0,%sr
	| Close the VBI/quit race at IPL7 before committing any guest state.
	move.l pendingFrames,%d0
	cmp.l seenFrames,%d0
	bne .LpopDecline
	tst.b irqQuit
	bne .LpopDecline
	move.b 3(%sp),nativeCachedVideoStatus
	addq.l #4,%sp
	move.b #1,irqLiveActive
	clr.b irqUninterruptedPoll
	move.l %d2,%d0
	subq.l #6,%d0
	move.l (%sp)+,%d2
	rts
.LrestoreDecline:
	move.w (%sp)+,%d0
	move.w %d0,%sr
.LpopDecline:
	addq.l #4,%sp
.Ldecline:
	moveq #0,%d0
	move.l (%sp)+,%d2
	rts

	| Restricted exact implementation of the two deferred grants. No host timer
	| is running here. New wall frames, other calibration ratios/windows, queued
	| ticks and all unusual states keep the existing C clock. At unchanged wall
	| time, spending after EACH grant matters: combining additions can lose credit
	| at saturation. Preserve that order, then publish the same phase/tick count.
	.globl nativeVideoClockPause
nativeVideoClockPause:
	move.w %sr,-(%sp)
	move.w #0x2700,%sr
	tst.w nativeClockRunning
	bne .LclockFallback
	tst.l irqLiveTicks
	bne .LclockFallback
	cmpi.l #80000,irqGuestPhase
	bcc .LclockFallback
	move.l pendingFrames,%d0
	cmp.l irqClockFrame,%d0
	bne .LclockFallback
	lea irqClock,%a0
	cmpi.w #64,20(%a0)
	bne .LclockFallback
	cmpi.w #3,22(%a0)
	bne .LclockFallback
	movem.l %d2-%d4,-(%sp)
	move.l (%a0),%d2
	move.l 4(%a0),%d3
	move.l irqGuestPhase,%d4
	move.l nativeShortGuest,%d0
	beq .Lnominal
	clr.l nativeShortGuest
	cmpi.l #120000,%d0
	bcc .LsaturateGuest
	lsl.l #2,%d0
	bra .LguestGrant
.LsaturateGuest:
	move.l #480000,%d0
.LguestGrant:
	bsr nativeVideoClockGrant
.Lnominal:
	move.l nativeShortNominal,%d0
	beq .LclockStore
	clr.l nativeShortNominal
	bsr nativeVideoClockGrant
.LclockStore:
	move.l %d2,(%a0)
	move.l %d3,4(%a0)
	moveq #0,%d1
.Lquanta:
	cmpi.l #80000,%d4
	bcs .Lphase
	subi.l #80000,%d4
	addq.l #1,%d1
	bra .Lquanta
.Lphase:
	move.l %d4,irqGuestPhase
	move.l %d1,irqLiveTicks
	movem.l (%sp)+,%d2-%d4
	move.w (%sp)+,%d0
	move.w %d0,%sr
	rts
.LclockFallback:
	move.w (%sp)+,%d0
	move.w %d0,%sr
	jmp nativeClockPause
nativeVideoClockGrant:
	move.l #480000,%d1
	sub.l %d2,%d1
	cmp.l %d1,%d0
	bcc .LsaturateCredit
	add.l %d0,%d2
	bra .Lspend
.LsaturateCredit:
	move.l #480000,%d2
.Lspend:
	tst.l %d3
	beq .LnoLimited
	tst.l %d2
	bne .LnoLimited
	addq.l #1,16(%a0)
.LnoLimited:
	move.l %d2,%d0
	cmp.l %d3,%d0
	bls .Lavailable
	move.l %d3,%d0
.Lavailable:
	move.l #160000,%d1
	sub.l %d4,%d1
	cmp.l %d1,%d0
	bls .Luse
	move.l %d1,%d0
.Luse:
	sub.l %d0,%d2
	sub.l %d0,%d3
	add.l %d0,%d4
	rts
