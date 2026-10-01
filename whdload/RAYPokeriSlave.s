; WHDLoad/Kickstart 3.1 launcher for the Amiga port. Cross-assembled with vasm.
; The executable uses Exec/graphics/DOS; kickfs supplies ordinary file access.

        INCLUDE whdload.i
        INCLUDE whdmacros.i

; Reserve 864 KiB Chip and 1.44 MiB OtherMem (960 KiB game/OS + 512 KiB Kickstart).
; Release memory evidence and constrained tests: docs/memory-audit.md.
CHIPMEMSIZE = $D8000
FASTMEMSIZE = $F0000
NUMDRIVES = 0
WPDRIVES = 0
BLACKSCREEN
BOOTDOS
CACHECHIP
HDINIT
SEGTRACKER
NO68020                         ; use 68000-compatible kickemu patches
; Do not use kick31.s STACKSIZE: its A600 patch at $2305c overwrites
; the MOVE.L opcode (the immediate starts at $2305e).
        IFD TEST
BOOTEARLY
DEBUG
        ENDC

        IFD SNOOPFS
slv_Version = 18
        ELSE
slv_Version = 17
        ENDC
slv_Flags = WHDLF_NoError|WHDLF_EmulLineA|WHDLF_EmulTrap|WHDLF_EmulPriv|WHDLF_EmulIllegal|WHDLF_EmulDivZero|WHDLF_EmulChk|WHDLF_EmulTrapV|WHDLF_EmulLineF
slv_keyexit = $5f                 ; Help emergency exit; game Esc / left mouse saves
        IFD DONT_CACHE_SAVES
slv_DontCache = _save_nocache
        ENDC
        INCLUDE whdload/kick31.s

slv_CurrentDir dc.b "data",0
slv_name dc.b "RAY Pokeri",0
slv_copy dc.b "Original game: RAY",0
slv_info dc.b "Amiga port by Vesuri",10
        dc.b "Version 0.90 (30.09.2026)",10
        dc.b "Esc saves; Help emergency exit",0
slv_config dc.b 0
        IFD DONT_CACHE_SAVES
        IFD DONT_CACHE_ALL
_save_nocache dc.b "#?",0
        ELSE
_save_nocache dc.b "(nvram.bin|nvram.bak|accounting.bin|accounting.bak)",0
        ENDC
        ENDC
        dc.b "$VER: RAYPokeri.slave 0.90 (30.09.2026)",0
_program dc.b "RAYPokeri",0
_args dc.b 10,0
_argsend
        EVEN

_bootdos
        move.l (_resload,pc),a2
        IFD BOOTONLY
        pea TDREASON_OK
        jmp (resload_Abort,a2)
        ENDC
        IFD TEST
        lea (_bootmark,pc),a0
        bsr _mark
        ENDC
        lea (_dosname,pc),a1
        move.l 4.w,a6
        jsr (_LVOOldOpenLibrary,a6)
        move.l d0,a6
        tst.l d0
        beq .oserror
        ; RAY Pokeri reads DOS GetArgStr, as a normal Shell-launched program does.
        lea (_args,pc),a0
        move.l a0,d1
        jsr (_LVOSetArgStr,a6)
        lea (_oldargs,pc),a0
        move.l d0,(a0)
        lea (_program,pc),a0
        move.l a0,d1
        jsr (_LVOLoadSeg,a6)
        move.l d0,d7
        beq .readerror
        IFD TEST
        lea (_loadmark,pc),a0
        bsr _mark
        ENDC
        bsr _patch_saves
        jsr (resload_FlushCache,a2)
        ; Establish PROGDIR as a Shell would. Calling a LoadSeg entry alone
        ; does not set pr_HomeDir, which the game's resource loader uses.
        lea (_current,pc),a0
        move.l a0,d1
        moveq #-2,d2              ; ACCESS_READ, lock the actual current drawer
        jsr (_LVOLock,a6)
        tst.l d0
        beq .readerror
        move.l d0,d1
        jsr (_LVOSetProgramDir,a6)
        lea (_oldhome,pc),a0
        move.l d0,(a0)
        IFD LOADONLY
        pea TDREASON_OK
        jmp (resload_Abort,a2)
        ENDC
        ; Use Exec's public API instead of modifying Kickstart's CLI code.
        move.l a6,-(sp)
        move.l 4.w,a6
        move.l #16384,d0
        moveq #0,d1
        jsr (_LVOAllocMem,a6)
        tst.l d0
        beq .oserror
        lea (_stackmem,pc),a0
        move.l d0,(a0)
        lea (_stack,pc),a0
        move.l d0,(a0)
        add.l #16384,d0
        move.l d0,(4,a0)
        move.l d0,(8,a0)
        jsr (_LVOStackSwap,a6)
        move.l d7,a1
        add.l a1,a1
        add.l a1,a1
        move.l #_argsend-_args-1,d0
        lea (_args,pc),a0
        jsr (4,a1)
        move.l d0,d6
        move.l 4.w,a6
        lea (_stack,pc),a0
        jsr (_LVOStackSwap,a6)
        move.l (_stackmem,pc),a1
        move.l #16384,d0
        jsr (_LVOFreeMem,a6)
        move.l (sp)+,a6
        move.l (_oldhome,pc),d1
        jsr (_LVOSetProgramDir,a6)
        move.l d0,d1
        jsr (_LVOUnLock,a6)
        move.l d7,d1
        jsr (_LVOUnLoadSeg,a6)
        move.l (_oldargs,pc),d1
        jsr (_LVOSetArgStr,a6)
        move.l a6,a1
        move.l 4.w,a6
        jsr (_LVOCloseLibrary,a6)
        tst.l d6
        bne .gameerror
        IFD TEST
        lea (_exitmark,pc),a0
        bsr _mark
        ENDC
        IFD MEMFREE
        ; Diagnostic only: kickemu's lowest largest free Chip/Fast chunks.
        lea (_memfreename,pc),a0
        lea MEMFREE,a1
        moveq #8,d0
        move.l (_resload,pc),a2
        jsr (resload_SaveFile,a2)
        ENDC
        pea TDREASON_OK
        bra .abort
.readerror
        jsr (_LVOIoErr,a6)
        pea (_program,pc)
        move.l d0,-(sp)
        pea TDREASON_DOSREAD
        bra .abort
.oserror
        clr.l -(sp)
        clr.l -(sp)
        pea TDREASON_OSEMUFAIL
        bra .abort
.gameerror
        cmp.l #21,d6
        beq .replayerror
        cmp.l #22,d6
        beq .sloterror
        cmp.l #23,d6
        beq .traceerror
        pea (_failed,pc)
        pea TDREASON_FAILMSG
.abort
        move.l (_resload,pc),a2
        jmp (resload_Abort,a2)
.replayerror
        pea (_replay_failed,pc)
        pea TDREASON_FAILMSG
        bra .abort
.sloterror
        pea (_slots_failed,pc)
        pea TDREASON_FAILMSG
        bra .abort
.traceerror
        pea (_trace_failed,pc)
        pea TDREASON_FAILMSG
        bra .abort
_trace_failed dc.b "Selected service mode requires NOVBRMOVE. Remove research markers or enable NOVBRMOVE.",0
_slots_failed dc.b "Save slots missing or invalid. Run the installer with Keep to create missing slots. Invalid saves have been preserved.",0
_replay_failed dc.b "Diagnostic native-replay requires NOVBRMOVE. Remove native-replay for normal play.",0
_failed dc.b "RAY Pokeri could not start. Check the installed original data files.",0
        IFD MEMFREE
_memfreename dc.b "memfree",0
        ENDC
_current dc.b 0
        EVEN
_stackmem dc.l 0
_stack dc.l 0,0,0
_oldhome dc.l 0
_oldargs dc.l 0
        IFD TEST
_bootearly
        move.l (_resload,pc),a2
        lea (_earlymark,pc),a0
        bra _mark
_mark
        movem.l d0-d1/a0-a1,-(sp)
        lea (_marker,pc),a1
        moveq #4,d0
        jsr (resload_SaveFile,a2)
        movem.l (sp)+,d0-d1/a0-a1
        rts
_marker dc.b "PASS"
_earlymark dc.b "test-early",0
_bootmark dc.b "test-bootdos",0
_loadmark dc.b "test-loaded",0
_exitmark dc.b "test-returned",0
        EVEN
        ENDC

; Find a complete, aligned 16-byte retained block in the LoadSeg chain.
; The word is GAS .word (Motorola dc.w), with no executable offsets baked in.
_patch_saves
        move.l d7,d0
.seg    tst.l d0
        beq .missing
        add.l d0,d0
        add.l d0,d0
        move.l d0,a0
        move.l (-4,a0),d1
        move.l (a0)+,d0
        sub.l #24,d1           ; complete 16-byte descriptor
        bmi .seg
        move.l a0,a1
        add.l d1,a1
.scan   cmpa.l a1,a0
        bhi .seg
        cmp.l #$504f4b21,(a0)+
        bne .scan
        IFD CALLBACK_SAVE
        cmp.l #$43423031,(a0)   ; CB01, never matches production SAVE block
        bne .scan
        cmp.w #1,(4,a0)
        bne .scan
        cmp.w #16,(6,a0)
        bne .scan
        tst.l (8,a0)
        bne .scan
        lea (_save_callback,pc),a1
        move.l a1,(8,a0)
        rts
        ELSE
        cmp.l #$53415645,(a0)
        bne .scan
        cmp.w #1,(4,a0)
        bhi .scan
        tst.w (6,a0)
        bne .scan
        tst.l (8,a0)
        bne .scan
        move.w #1,(4,a0)
        lea (_save_file,pc),a1
        move.l a1,(8,a0)
        rts
        ENDC
.missing
        pea (_config_missing,pc)
        pea TDREASON_FAILMSG
        jmp (resload_Abort,a2)
_config_missing dc.b "RAY Pokeri save configuration block missing or invalid.",0
        EVEN

; Whole-file save: one resload call per file, never a DOS packet sequence.
; ABI: D0=size, A0=name, A1=bytes; D0=BOOL result. Preserve all other registers.
_save_file
        movem.l d1-d7/a0-a6,-(sp)
        move.l (_resload,pc),a2
        jsr (resload_SaveFile,a2)
        movem.l (sp)+,d1-d7/a0-a6
        rts

        IFD CALLBACK_SAVE
; Diagnostic only: whole-file save in the failing DOS/slave context.
; ABI: D0=size, A0=name, A1=bytes; D0=BOOL result. Preserve caller's DOS base.
_save_callback
        IFD SAVE_EARLY_ABORT
        cmp.l #32,d0           ; authored accounting payload; skip DOS teardown
        beq _save_then_abort
        ENDC
        movem.l d1-d7/a0-a6,-(sp)
        move.l (_resload,pc),a2
        jsr (resload_SaveFile,a2)
        movem.l (sp)+,d1-d7/a0-a6
        rts
        IFD SAVE_EARLY_ABORT
_save_then_abort
        move.l (_resload,pc),a2
        jsr (resload_SaveFile,a2)
        pea TDREASON_OK
        jmp (resload_Abort,a2)
        ENDC
        ENDC
