; Authored diagnostic only: no game code, ROM data, or release dependency.
; Four paired batches per vector, 128 exceptions per batch. CIA-A timer B
; counts E-clock ticks. Any timer wrap or wrong delivery count rejects the run.
        INCLUDE whdload.i
        INCLUDE whdmacros.i
_base   SLAVE_HEADER
        dc.w 17
        dc.w WHDLF_Req68020|WHDLF_EmulLineA|WHDLF_EmulTrap|WHDLF_EmulPriv
        dc.l $10000,0
        dc.w _start-_base,0,0
        dc.b 0,$59
        dc.l 0
        dc.w _name-_base,_copy-_base,_info-_base
        dc.w 0
        dc.l 0
        dc.w 0,0
_name   dc.b "Pokeri exception benchmark",0
_copy   dc.b "2026",0
_info   dc.b "Diagnostic only; 68020 required",0
_file   dc.b "data/exception-timing.bin",0
_pass   dc.b "smoke-passed",0
_error  dc.b "Exception benchmark count or timer overflow",0
        EVEN
_start  lea (_state,pc),a6
        move.l a0,4(a6)
        move.l sp,8(a6)
        move.w #$7fff,$dff09a
        move.w #$7fff,$dff096
        move.b #$7f,$bfed01
        move.b #$7f,$bfdd00
        lea (_vectors,pc),a0
        lea (12,a6),a1
        moveq #5,d0
.save   move.w (a0)+,d1
        move.l (0,d1.w),(a1)+
        dbra d0,.save
        lea (_handlers,pc),a0
        move.l #$1000,a1
        move.w #_handlersEnd-_handlers-1,d0
.copy   move.b (a0)+,(a1)+
        dbra d0,.copy
        move.l #$1000+_linea-_handlers,a0
        move.l a0,$28
        move.l #$1000+_trap-_handlers,a0
        move.l a0,$80
        move.l #$1000+_priv-_handlers,a0
        move.l a0,$20
        move.l #$1000+_level2-_handlers,a0
        move.l a0,$68
        move.l #$1000+_level3-_handlers,a0
        move.l a0,$6c
        lea (_finish,pc),a0
        move.l a0,$bc
        move.w #$7fff,$dff09c
        move.w #$c048,$dff09a
        move.l #$ff00,sp
        move.l #$f000,a0
        move.l a0,usp
        clr.w -(sp)
        pea (_user,pc)
        clr.w -(sp)
        rte
_user   lea (_results,pc),a4
        lea (_tests,pc),a5
        moveq #4,d5
.kind   move.w (a5)+,d0
        lea (_tests,pc),a3
        adda.w d0,a3
        moveq #3,d6
.trial  lea (_noop,pc),a0
        bsr _timerStart
        move.w #127,d7
.base   jsr (a0)
        dbra d7,.base
        bsr _timerStop
        move.l d0,(a4)+
        clr.l (a6)
        bsr _timerStart
        move.w #127,d7
.test   jsr (a3)
        dbra d7,.test
        bsr _timerStop
        move.l d0,(a4)+
        move.l (a6),(a4)+
        cmp.l #128,(a6)
        bne _failed
        dbra d6,.trial
        dbra d5,.kind
        trap #15
_noop   rts
_timerStart
        clr.b $bfef01
        move.b $bfed01,d0
        move.b #$ff,$bfe601
        move.b #$ff,$bfe701
        move.b #$11,$bfef01
        rts
_timerStop
        clr.b $bfef01
        moveq #0,d0
        move.b $bfe701,d0
        lsl.w #8,d0
        move.b $bfe601,d0
        not.w d0
        btst #1,$bfed01
        bne _failed
        rts
_doLine dc.w $a000
        rts
_doTrap trap #0
        rts
_doPriv ori.w #0,sr
        rts
_doL2   move.l (a6),d0
        move.w #$8008,$dff09c
.wait   cmp.l (a6),d0
        beq .wait
        rts
_doL3   move.l (a6),d0
        move.w #$8040,$dff09c
.wait   cmp.l (a6),d0
        beq .wait
        rts
_handlers
_priv   addq.l #2,2(sp)
_linea  addq.l #2,2(sp)
_trap   addq.l #1,(a6)
        rte
_level2 move.w #8,$dff09c
        addq.l #1,(a6)
        rte
_level3 move.w #$40,$dff09c
        addq.l #1,(a6)
        rte
_handlersEnd
_failed move.l #1,36(a6)
        trap #15
_finish move.w #$7fff,$dff09a
        clr.b $bfef01
        lea (_vectors,pc),a0
        lea (12,a6),a1
        moveq #5,d0
.restore move.w (a0)+,d1
        move.l (a1)+,(0,d1.w)
        dbra d0,.restore
        move.l 8(a6),sp
        move.l 4(a6),a2
        tst.l 36(a6)
        bne .fail
        lea (_file,pc),a0
        lea (_report,pc),a1
        move.l #_endReport-_report,d0
        jsr (resload_SaveFile,a2)
        lea (_pass,pc),a0
        lea (_passData,pc),a1
        moveq #4,d0
        jsr (resload_SaveFile,a2)
        pea TDREASON_OK
        jmp (resload_Abort,a2)
.fail   pea (_error,pc)
        pea TDREASON_FAILMSG
        jmp (resload_Abort,a2)
_vectors dc.w $28,$80,$20,$68,$6c,$bc
_tests  dc.w _doLine-_tests,_doTrap-_tests,_doPriv-_tests
        dc.w _doL2-_tests,_doL3-_tests
_passData dc.b "PASS"
_state  dcb.l 10,0
_report dc.b "PKEX0001"
        dc.l 709379,128,4,5
_results dcb.l 60,0
_endReport
