/* Diagnostic AmigaDOS command run AFTER WHDLoad returns. Preserves ABI.
 * Register markers remain visible when FS-UAE omits out-of-hunk PCs.
 * Build separately; never link this into Pokeri or the slave.
 */
.text
.globl _start
_start:
 movem.l %d6-%d7,-(%sp)
 move.l #0x504f4b21,%d6
 move.l #0x45584954,%d7
 nop
 nop
 movem.l (%sp)+,%d6-%d7
 moveq #0,%d0
 rts
