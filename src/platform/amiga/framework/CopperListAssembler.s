	xdef	_showBitmap__10CopperListFUlRC6BitmapUsUsssUs
	xdef	_showSprite__10CopperListFUlUsRC6Sprite

; Offsets to various C++ structures. These MUST match the C++ structures!
; -----------------------------------------------------------------------
data_				equ	4
bitmapData			equ	0
bitmapBitplanes			equ	8
bitmapWidthInBytes		equ	12
bitmapRowSizeInBytes		equ	14
bitmapBitplaneSizeInBytes	equ	16
bitmapInterleaved		equ	18
spriteData			equ	0

	section	code

_showBitmap__10CopperListFUlRC6BitmapUsUsssUs:
	movem.l	d2-d5,-(sp)

	tst.w	d3				; if (xOffset) {
	beq.s	sB.xOffsetOk
	asr.w	#3,d3				; bitplane += xOffset >> 3;
sB.xOffsetOk:
	ext.l	d3
	tst.w	d4				; if (yOffset) {
	beq.s	sB.yOffsetOk
	; The product is a longword and the sum has to stay one. Adding only its low word, which
	; is what this did, overflows on any bitmap with wide enough rows: 768 bytes a row times
	; row 63 is 48384, past a signed word, and the following ext.l then propagated the flipped
	; sign and put the bitplane pointer 64 KB low. Found in JRm-bS76, one garbage row at the
	; horizon.
	muls	bitmapRowSizeInBytes(a1),d4	; bitplane += yOffset * bitmap.rowSizeInBytes;
	add.l	d4,d3
sB.yOffsetOk:
	add.l	bitmapData(a1),d3		; uint32_t bitplane = (uint32_t)bitmap.data;

	tst.w	d5				; if (bitplaneCount == 0) {
	bne.s	sB.bitplaneCountOk
	move.w	bitmapBitplanes(a1),d5		; bitplaneCount = bitmap.bitplanes;
sB.bitplaneCountOk:

	moveq	#0,d4
	tst.b	bitmapInterleaved(a1)
	beq.s	sB.bitplaneDeltaPlaneSize
	move.w	bitmapWidthInBytes(a1),d4
	bra.s	sB.bitplaneDeltaOk
sB.bitplaneDeltaPlaneSize:
	move.w	bitmapBitplaneSizeInBytes(a1),d4
sB.bitplaneDeltaOk:

	move.l	data_(a0),a1
	add.w	d0,d0
	add.w	d0,d0
	add.w	d0,a1
	add.w	d1,d1				; uint16_t bitplanePointerRegister = bpl1pth + ((firstBitplane - 1) << 2);
	add.w	d1,d1
	add.w	#$0dc,d1
	add.w	d2,d2
	add.w	d2,d2
	subq	#2,d2
	subq	#1,d5				; for (uint16_t i = 0; i < bitplaneCount; i++, bitplanePointerRegister += (bitplaneNumberDelta << 2)) {
	; A bitmap arriving with bitplanes == 0 would make this dbra loop run 65536 times
	; where the C++ for loop runs none: half a megabyte of copperMove(register++, ...)
	; written over everything that follows the copperlist, so a bad pointer upstream
	; destroys memory instead of dropping a frame. The count is a word, so the subq
	; leaves it negative in exactly that case. (JRm-bS76 hit this, see its STATUS.md.)
	bmi.s	sB.done
sB.loop:
	move.w	d1,(a1)+			; data_[listIndex++] = copperMove(bitplanePointerRegister, (uint16_t)(bitplane >> 16));
	swap	d3
	move.w	d3,(a1)+

	addq	#2,d1				; data_[listIndex++] = copperMove(bitplanePointerRegister + 2, (uint16_t)bitplane);
	move.w	d1,(a1)+
	swap	d3
	move.w	d3,(a1)+

	add.l	d4,d3				; bitplane += bitmap.interleaved ? bitmap.widthInBytes : bitmap.bitplaneSizeInBytes;
	add.w	d2,d1
	dbra	d5,sB.loop

sB.done:
	movem.l	(sp)+,d2-d5
	rts

_showSprite__10CopperListFUlUsRC6Sprite:
	move.l	data_(a0),a0
	add.w	d0,d0
	add.w	d0,d0
	add.w	d0,a0
	move.l	spriteData(a1),d0
	add.w	d1,d1
	add.w	d1,d1
	add.w	#$122,d1
	move.w	d1,4(a0)
	move.w	d0,6(a0)
	swap	d0
	subq	#2,d1
	move.w	d1,(a0)
	move.w	d0,2(a0)
	rts

	end

