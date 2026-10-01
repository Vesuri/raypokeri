    | Authored CPU fixtures for the joined T13 handler: the selector and queue
    | setup with its branch cases, then the delivery entry instructions.
    | No original ROM bytes or data are included.
    .text
    .globl oracle_setup,oracle_setup_feed,oracle_setup_empty,oracle_setup_exit
oracle_setup:
    move.b #0,(%a0)
    move.l -30682(%a6),%d1
    movea.l -30526(%a6),%a1
    move.l %a1,%d0
    movea.l -30678(%a6),%a1
    cmpa.l %d1,%a1
    beq.s oracle_setup_empty
    cmpa.l %d0,%a1
    bne.s 1f
    movea.l -30530(%a6),%a1
1:  cmpa.l %d1,%a1
    beq.s oracle_setup_exit
oracle_setup_feed:
    nop
    .org oracle_setup+0x3a
oracle_setup_empty:
    nop
    .org oracle_setup+0x48
oracle_setup_exit:
    nop
    .globl oracle_delivery,oracle_delivery_end
oracle_delivery:
    movem.l %d0-%d1/%a0-%a1,-(%sp)
    movea.l #0x180000,%a0
oracle_delivery_end:
    nop
