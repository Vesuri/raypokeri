; Authored DOS save reproducer. No original ROM data or native runner.
        INCLUDE lvo/exec_lib.i
        INCLUDE lvo/dos_lib.i
        SECTION code,CODE
_start  movem.l d2-d7/a2-a6,-(sp)
        move.l 4.w,a6
        lea dosname(pc),a1
        jsr _LVOOldOpenLibrary(a6)
        tst.l d0
        beq .failed
        move.l d0,a6
        lea image,a0
        move.w #8191,d0
.fill   move.l #$5a1234a5,(a0)+
        dbra d0,.fill
        lea nvram(pc),a3
        lea nvbackup(pc),a4
        move.l #32768,d4
        bsr save
        tst.l d0
        bne .close
        lea account(pc),a3
        lea acbackup(pc),a4
        moveq #32,d4
        bsr save
.close  move.l d0,d7
        move.l a6,a1
        move.l 4.w,a6
        jsr _LVOCloseLibrary(a6)
        move.l d7,d0
        bra .return
.failed moveq #20,d0
.return movem.l (sp)+,d2-d7/a2-a6
        rts

; a3=name, a4=backup, d4=size. Same old-image backup / truncate / write
; sequence as saveKickfs, using only Open/Read/Write/Close.
save    move.l a3,d1
        move.l #1005,d2          ; MODE_OLDFILE
        jsr _LVOOpen(a6)
        tst.l d0
        beq .missing
        move.l d0,d5
        move.l d5,d1
        lea previous,a0
        move.l a0,d2
        move.l d4,d3
        addq.l #1,d3
        jsr _LVORead(a6)
        move.l d0,d6
        move.l d5,d1
        jsr _LVOClose(a6)
        cmp.l d4,d6
        bne .error
        move.l a4,d1
        lea previous,a2
        bsr write
        tst.l d0
        bne .return
        bra .new
.missing
        jsr _LVOIoErr(a6)
        cmp.l #205,d0
        bne .error
.new    move.l a3,d1
        lea image,a2
        bsr write
.return rts
.error  moveq #20,d0
        rts

; d1=name, a2=bytes, d4=size.
write
        IFD UPDATE_SAVE
        move.l #1004,d2          ; MODE_READWRITE, fixed-size update
        ELSE
        move.l #1006,d2          ; MODE_NEWFILE
        ENDC
        jsr _LVOOpen(a6)
        tst.l d0
        beq .error
        move.l d0,d5
        move.l d5,d1
        move.l a2,d2
        move.l d4,d3
        jsr _LVOWrite(a6)
        move.l d0,d6
        move.l d5,d1
        jsr _LVOClose(a6)
        tst.l d0
        beq .error
        cmp.l d4,d6
        bne .error
        moveq #0,d0
        rts
.error  moveq #20,d0
        rts

dosname dc.b "dos.library",0
nvram   dc.b "nvram.bin",0
nvbackup dc.b "nvram.bak",0
account dc.b "accounting.bin",0
acbackup dc.b "accounting.bak",0
        EVEN
        SECTION config,DATA
        dc.b "POK!SAVE"
        dc.w 0,0
        SECTION buffers,BSS
image   ds.b 32768
previous ds.b 32770
