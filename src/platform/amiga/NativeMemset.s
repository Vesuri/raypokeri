| Exact C memset ABI; no OS calls and no accesses outside the requested span.
| Align byte/word heads, then fill longwords. Size and return value stay 32-bit.
	.section .text.pokeriMemset,"ax"
	.balign 2
	.globl pokeriMemset
	.globl __wrap_memset
pokeriMemset:
__wrap_memset:
	move.l 4(%sp),%a0
	move.l 8(%sp),%d0
	move.l 12(%sp),%d1
	beq .Ldone
	cmp.l #4,%d1
	bcs .Lbytes
	btst #0,7(%sp)
	beq .Leven
	move.b %d0,(%a0)+
	subq.l #1,%d1
.Leven:
	move.l %d1,%a1
	and.l #255,%d0
	move.w %d0,%d1
	lsl.w #8,%d0
	or.w %d1,%d0
	move.w %d0,%d1
	swap %d0
	move.w %d1,%d0
	move.l %a0,%d1
	btst #1,%d1
	beq .Laligned
	move.w %d0,(%a0)+
	subq.l #2,%a1
.Laligned:
	move.l %a1,%d1
	cmp.l #32,%d1
	bcs .Llongs
.Lblock:
	.rept 8
	move.l %d0,(%a0)+
	.endr
	sub.l #32,%d1
	cmp.l #32,%d1
	bcc .Lblock
.Llongs:
	cmp.l #4,%d1
	bcs .Ltail
	move.l %d0,(%a0)+
	subq.l #4,%d1
	bra .Llongs
.Ltail:
	tst.l %d1
	beq .Ldone
.Lbytes:
	move.b %d0,(%a0)+
	subq.l #1,%d1
	bne .Lbytes
.Ldone:
	move.l 4(%sp),%d0
	rts
