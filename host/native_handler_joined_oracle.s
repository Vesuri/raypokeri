    | Authored CPU fixture for the joined T13 delivery entry instructions.
    | No original ROM bytes or data are included.
    .text
    .globl oracle_delivery,oracle_delivery_end
oracle_delivery:
    movem.l %d0-%d1/%a0-%a1,-(%sp)
    movea.l #0x180000,%a0
oracle_delivery_end:
    nop
