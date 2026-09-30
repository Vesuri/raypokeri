    | W3 interrupt-return primitive. Caller runs in supervisor mode
    | and preserves D0/A0/A1. Nested IRQs carry supervisor frames. No stack-format-dependent offsets beyond
    | the common SR/PC prefix are read; all extension words remain untouched.
    | A0 = real exception frame; A1 = {stub PC, saved PC, armed word}.
    | D0 = 1 changed, 0 not applicable/already armed, -1 inconsistent slot.
    | Live integration is opt-in until the scheduler/entry gates pass.
    .section .text.nativeServiceRedirect,"ax"
    .globl nativeServiceRedirect,nativeServiceConsume,nativeServiceRedirectEnd
nativeServiceRedirect:
    moveq #0,%d0
    btst #5,(%a0)
    bne 4f
    move.l 2(%a0),%d0
    cmp.l (%a1),%d0
    beq 2f
    tst.w 8(%a1)
    bne 3f
    move.l %d0,4(%a1)
    move.w #1,8(%a1)
    move.l (%a1),2(%a0)
    moveq #1,%d0
    rts
2:
    tst.w 8(%a1)
    beq 3f
    moveq #0,%d0
4:
    rts
3:
    moveq #-1,%d0
    rts
nativeServiceConsume:
    moveq #0,%d0
    btst #5,(%a0)
    bne 4f
    move.l 2(%a0),%d0
    cmp.l (%a1),%d0
    bne 2f
    tst.w 8(%a1)
    beq 3f
    move.l 4(%a1),2(%a0)
    clr.w 8(%a1)
    moveq #1,%d0
    rts
2:
    moveq #0,%d0
4:
    rts
3:
    moveq #-1,%d0
    rts
nativeServiceRedirectEnd:
