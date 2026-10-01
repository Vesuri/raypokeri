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
        IFD GAME_MEMORY
        dc.l $100000,0
        ELSE
        dc.l $10000,0
        ENDC
        IFD DATA_DIRECTORY
        dc.w _start-_base,_directory-_base,0
        ELSE
        dc.w _start-_base,0,0
        ENDC
        IFD PROBE_KEYEXIT
        dc.b 0,PROBE_KEYEXIT
        ELSE
        dc.b 0,$59
        ENDC
        IFD GAME_MEMORY
        dc.l $400000
        ELSE
        dc.l 0
        ENDC
        dc.w _name-_base,_copy-_base,_info-_base
        dc.w 0
        dc.l 0
        dc.w 0,0
_name   dc.b "RAY Pokeri slave smoke test",0
_copy   dc.b "2026",0
_info   dc.b "No game code is loaded",0
_file   dc.b "smoke-passed",0
        IFD PROBE_KEYEXIT
_keyfile dc.b "keyexit-value",0
_keydata dc.b 0
        ENDC
        IFD SECOND_SAVE
_second dc.b "second-passed",0
        ENDC
        IFD DATA_DIRECTORY
_directory dc.b "data",0
        ENDC
        EVEN
_start  move.l a0,a2
        IFD PROBE_KEYEXIT
        lea (_keydata,pc),a1
        move.b (_base+ws_keyexit,pc),(a1)
        lea (_keyfile,pc),a0
        moveq #1,d0
        jsr (resload_SaveFile,a2)
        ENDC
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
        IFD SECOND_SAVE
        lea (_second,pc),a0
        lea (_data,pc),a1
        moveq #32,d0
        jsr (resload_SaveFile,a2)
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
