; Minimal host-assembled WHDLoad test: no ROM, DOS, game or game data.
        INCLUDE whdload.i
        INCLUDE whdmacros.i
_base   SLAVE_HEADER
        dc.w 17
        IFD EXAMINE_SAVE
        dc.w WHDLF_Examine
        ELSE
        dc.w 0
        ENDC
        dc.l $10000,0
        dc.w _start-_base,0,0
        dc.b 0,$59
        dc.l 0
        dc.w _name-_base,_copy-_base,_info-_base
        dc.w 0
        dc.l 0
        dc.w 0,0
_name   dc.b "Pokeri slave smoke test",0
_copy   dc.b "2026",0
_info   dc.b "No game code is loaded",0
_file   dc.b "smoke-passed",0
        EVEN
_start  move.l a0,a2
        IFD OFFSET_SAVE
        lea (_file,pc),a0
        lea (_data,pc),a1
        moveq #0,d0
        jsr (resload_SaveFile,a2)
        IFD EXAMINE_SAVE
        lea (_file,pc),a0
        lea (_fib,pc),a1
        jsr (resload_Examine,a2)
        ENDC
        ENDC
        lea (_file,pc),a0
        lea (_data,pc),a1
        IFD BIG_SAVE
        move.l #32768,d0
        ELSE
        moveq #4,d0
        ENDC
        IFD OFFSET_SAVE
        moveq #0,d1
        jsr (resload_SaveFileOffset,a2)
        ELSE
        jsr (resload_SaveFile,a2)
        IFD EXAMINE_SAVE
        lea (_file,pc),a0
        lea (_fib,pc),a1
        jsr (resload_Examine,a2)
        ENDC
        ENDC
        pea TDREASON_OK
        jmp (resload_Abort,a2)
        IFD EXAMINE_SAVE
_fib    dcb.b 260,0
        ENDC
_data   dc.b "PASS"
        IFD BIG_SAVE
        dcb.b 32764,$5a
        ENDC
