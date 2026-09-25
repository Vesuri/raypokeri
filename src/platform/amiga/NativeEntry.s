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
	jsr nativeClockCalibrateBegin
	bra nativeResume
nativeLineA:
	move.w #0x2700,%sr
	stopclock
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
