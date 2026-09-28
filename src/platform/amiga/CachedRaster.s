	| Guarded cached raster completion. A0: model-granted RasterGrant, D1: final word.
	| D0 returns canonical bool. Preserve D1 and every other register except A0.
	| No global model offsets: pointer fields are checked by native ABI asserts.
	.text
	.globl nativeCachedRasterComplete
nativeCachedRasterComplete:
	movem.l %d2-%d5/%a1-%a4,-(%sp)
	move.l %a0,%a1
	move.l 24(%a1),%a0
	move.l (%a0),%d3
	cmpi.l #6,%d3
	bls .Lrefuse
	cmpi.l #78,%d3
	bcc .Lrefuse
	move.l 32(%a1),%a0
	move.l (%a0),%d2
	addq.l #1,%d2
	cmpi.l #64,%d2
	bhi .Lrefuse
	move.l 4(%a1),%a0
	move.l %d3,%d0
	add.l %d0,%d0
	moveq #0,%d4
	move.w (%a0,%d0.l),%d4
	moveq #0,%d5
	move.w 2(%a0,%d0.l),%d5
	sub.l %d4,%d5
	cmp.l %d2,%d5
	bne .Lrefuse
	move.l 36(%a1),%a0
	cmpi.l #1,%d2
	bne .Llength
	tst.l (%a0)
	bne .Lrefuse
	bra .Lopcode
.Llength:
	cmp.l (%a0),%d2
	bne .Lrefuse
.Lopcode:
	move.l (%a1),%a2
	add.l %d4,%d4
	adda.l %d4,%a2
	moveq #0,%d5
	move.w (%a2),%d5
	lsr.w #8,%d5
	lsr.w #2,%d5
	cmpi.w #2,%d5
	beq .Lrefuse
	cmpi.w #32,%d5
	beq .Lrefuse
	cmpi.w #33,%d5
	beq .Lrefuse
	move.l 16(%a1),%a3
	move.l %d2,%d0
	subq.w #2,%d0
	bmi .Llast
.Lcompare:
	move.w (%a2)+,%d4
	cmp.w (%a3)+,%d4
	bne .Lrefuse
	dbra %d0,.Lcompare
.Llast:
	cmp.w (%a2),%d1
	bne .Lrefuse
	| All refusal checks precede the first mutation.
	move.w %d1,(%a3)
	move.l 40(%a1),%a0
	move.w %d1,%d0
	lsr.w #8,%d0
	move.b %d0,(%a0)
	move.l 28(%a1),%a0
	move.l (%a0),%d0
	add.l %d2,(%a0)
	add.l %d0,%d0
	move.l 12(%a1),%a4
	adda.l %d0,%a4
	move.l 16(%a1),%a3
	move.w %d2,%d0
	subq.w #1,%d0
.Lcopy:
	move.w (%a3)+,(%a4)+
	dbra %d0,.Lcopy
	move.l 24(%a1),%a0
	addq.l #1,(%a0)
	move.l %d3,%d0
	add.l %d3,%d3
	add.l %d0,%d3
	lsl.l #2,%d3
	move.l 8(%a1),%a2
	adda.l %d3,%a2
	move.l 20(%a1),%a4
	move.w (%a2),%d3
	ext.l %d3
	add.l 68(%a1),%d3
	move.w %d3,36(%a4)
	ext.l %d3
	move.w 2(%a2),%d2
	ext.l %d2
	add.l 72(%a1),%d2
	move.w %d2,38(%a4)
	muls.w #152,%d2
	move.l 76(%a1),%d0
	andi.l #15,%d0
	lsr.l #2,%d0
	add.l %d0,%d3
	move.l %d3,%d4
	asr.l #2,%d4
	move.l 76(%a1),%d0
	lsr.l #4,%d0
	add.l %d4,%d0
	sub.l %d2,%d0
	andi.l #0xfffff,%d0
	move.l %d0,%d4
	lsr.l #8,%d4
	lsr.l #4,%d4
	move.l 76(%a1),%d2
	swap %d2
	andi.w #0xc000,%d2
	or.w %d2,%d4
	move.w %d4,32(%a4)
	lsl.w #4,%d0
	andi.w #3,%d3
	lsl.w #2,%d3
	or.w %d3,%d0
	move.w %d0,34(%a4)
	move.l 4(%a2),%d0
	tst.l 80(%a1)
	beq .Lwork
	move.l 8(%a2),%d0
.Lwork:
	move.l 48(%a1),%a0
	move.l %d0,(%a0)
	move.l 52(%a1),%a0
	clr.b (%a0)
	move.l 56(%a1),%a0
	clr.b (%a0)
	move.l 60(%a1),%a0
	clr.l (%a0)
	move.l 64(%a1),%a0
	lsl.l #2,%d5
	addq.l #1,(%a0,%d5.l)
	move.l 32(%a1),%a0
	clr.l (%a0)
	move.l 36(%a1),%a0
	clr.l (%a0)
	move.l 44(%a1),%a0
	ori.b #0x20,(%a0)
	moveq #1,%d0
	bra .Lreturn
.Lrefuse:
	moveq #0,%d0
.Lreturn:
	movem.l (%sp)+,%d2-%d5/%a1-%a4
	rts
