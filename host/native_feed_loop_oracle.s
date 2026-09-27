	.text
	.globl oracle_head,oracle_head_exit,oracle_status,oracle_ready,oracle_write
	.globl oracle_end,oracle_wrap_branch,oracle_producer,oracle_producer_exit
	.globl oracle_wrap,oracle_again,oracle_exit
oracle_head:
	cmpa.l %d1,%a1
oracle_head_exit:
	beq.w oracle_exit
oracle_status:
	btst #1,(%a0)
oracle_ready:
	beq.w oracle_exit
oracle_write:
	move.w (%a1)+,2(%a0)
oracle_end:
	cmpa.l %d0,%a1
oracle_wrap_branch:
	bcs.w oracle_head
oracle_producer:
	cmp.l %d0,%d1
oracle_producer_exit:
	beq.w oracle_exit
oracle_wrap:
	movea.l (%a6),%a1
oracle_again:
	bra.w oracle_head
oracle_exit:
	nop
