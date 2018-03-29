/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

	.global write_cntpctl
write_cntpctl:
	.func
	mcr	p15, 0, r0, c14, c2, 1
	bx	lr
	.endfunc

	.global read_cntpctl
read_cntpctl:
	.func
	mrc	p15, 0, r0, c14, c2, 1
	bx	lr
	.endfunc

	.global read_cpu_id
read_cpu_id:
	.func
	mrc	p15, 0, r0, c0, c0, 5
	ands	r0, r0, #0xff
	bx	lr
	.endfunc

	.global read_cluster_id
read_cluster_id:
	.func
	mrc	p15, 0, r0, c0, c0, 5
	lsr	r0, r0, #0x8
	ands	r0, r0, #0xff
	bx	lr
	.endfunc

	.global mt_save_generic_timer
mt_save_generic_timer:
	.func
	mrc	p15, 0, r2, c14, c2, 1	@ read CNTP_CTL
	mrc	p15, 0, r3, c14, c2, 0	@ read CNTP_TVAL
	mrc	p15, 0, r12, c14, c1, 0	@ read CNTKCTL
	stm	r0!, {r2, r3, r12}
	bx	lr
	.endfunc

	.global mt_restore_generic_timer
mt_restore_generic_timer:
	.func
	ldm	r0!, {r2, r3, r12}
	mcr	p15, 0, r3, c14, c2, 0	@ write CNTP_TVAL
	mcr	p15, 0, r12, c14, c1, 0	@ write CNTKCTL
	mcr	p15, 0, r2, c14, c2, 1	@ write CNTP_CTL
	bx	lr
	.endfunc

	.global mt_save_l2ctlr
mt_save_l2ctlr:
	.func
	mrc	p15, 1, r0, c9, c0, 2
	isb
	dsb
	bx	lr
	.endfunc

	.global mt_restore_l2ctlr
mt_restore_l2ctlr:
	.func
	isb
	dsb
	mrc	p15, 1, r1, c9, c0, 2
	dsb
	isb
	ubfx	r1, r1, #18, #14
	orr	r1, r1, r0
	mcr	p15, 1, r1, c9, c0, 2
	dsb
	isb
	bx	lr
	.endfunc

	.global mt_goto_cpu_resume
mt_goto_cpu_resume:
	.func
	adr	r1, 1f
	ldr	r2, [r1]
	sub	r2, r0, r2
	add	r3, r1, r2
	ldr	r3, [r3]
	bx	r3
1:	.long
	bx	lr
	.endfunc

	.global mt_get_data_nommu
mt_get_data_nommu:
	.func
	adr	r2, 1f
	ldr	r3, [r2]
	sub	r3, r1, r2
	add	r4, r2, r3
	ldr	r0, [r4]
	b	3f
1:	.long
3:	bx	lr
	.endfunc

	.global smp
smp:
	.func
	isb
	dsb
	mrc	p15, 0, r0, c1, c0, 1
	isb
	dsb
	orr	r0, r0, #0x00000040
	mcr	p15, 0, r0, c1, c0, 1
	isb
	dsb
	bx	lr
	.endfunc

	.global amp
amp:
	.func
	clrex
	isb
	dsb
	mrc	p15, 0, r0, c1, c0, 1
	isb
	dsb
	bic	r0, r0, #0x00000040
	mcr	p15, 0, r0, c1, c0, 1
	isb
	dsb
	bx	lr
	.endfunc

	.global disable_dcache_safe
disable_dcache_safe:
	.func
	mrc	p15, 0, r1, c1, c0, 0
	dsb
	bic	r1, r1, #4
	mcr	p15, 0, r1, c1, c0, 0
	dsb
	isb

	mcr	p15, 0, r1, c8, c7, 1
	dsb

	@ flush LOUIS
	mrc	p15, 1, r1, c0, c0, 1		@ read clidr
	ands    r4, r1, #0x7000000		@ extract loc from clidr
	mov	r4, r4, lsr #23			@ left align loc bit field
	beq	L1_finished			@ if loc is 0, then no need to clean
	mov	r10, #0				@ start clean at cache level 1
L1_loop1:
	add	r3, r10, r10, lsr #1		@ work out 3x current cache level
	mov	r2, r1, lsr r3			@ extract cache type bits from clidr
	and	r2, r2, #7			@ mask of the bits for current cache only
	cmp	r2, #2				@ see what cache we have at this level
	blt	L1_skip				@ skip if no cache, or just i-cache
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	isb					@ isb to sych the new cssr&csidr
	mrc	p15, 1, r2, c0, c0, 0		@ read the new csidr
	and	r3, r2, #7			@ extract the length of the cache lines
	add	r3, r3, #4			@ add 4 (line length offset)
	@ldr	r5, =0x3ff
	mov	r5, #0x400
	sub	r5, #1
	ands	r5, r5, r2, lsr #3		@ find maximum number on the way size
	clz	r6, r5				@ find bit position of way size increment
	@ldr	r8, =0x7fff
	mov	r8, #0x8000
	sub	r8, #1
	ands	r8, r8, r2, lsr #13		@ extract max number of the index size
L1_loop2:
	mov	r9, r5				@ create working copy of max way size
L1_loop3:
	orr	r7, r10, r9, lsl r6		@ factor way and cache number into r7
	orr	r7, r7, r8, lsl r3		@ factor index number into r7
	mcr	p15, 0, r7, c7, c14, 2		@ clean & invalidate by set/way
	@mcr	p15, 0, r7, c7, c10, 2		@ clean by set/way
	@mcr	p15, 0, r7, c7, c6, 2		@  invalidate by set/way

	subs	r9, r9, #1			@ decrement the way
	bge	L1_loop3
	subs	r8, r8, #1			@ decrement the index
	bge	L1_loop2
L1_skip:
	@add	r10, r10, #2			@ increment cache number
	@cmp	r4, r10
	@bgt	L1_loop1
L1_finished:
	mov	r10, #0				@ swith back to cache level
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	dsb
	isb

	@clean or flush L2
	mrc	p15, 1, r1, c0, c0, 1		@ read clidr
	isb
	ands	r4, r1, #0x7000000		@ extract loc from clidr
	mov	r4, r4, lsr #23			@ left align loc bit field
	beq	L2_cl_finished			@ if loc is 0, then no need to clean
	mov	r10, #2				@ start clean at cache level 2
L2_cl_loop1:
	add	r3, r10, r10, lsr #1		@ work out 3x current cache level
	mov	r2, r1, lsr r3			@ extract cache type bits from clidr
	and	r2, r2, #7			@ mask of the bits for current cache only
	cmp	r2, #2				@ see what cache we have at this level
	blt	L2_cl_skip			@ skip if no cache, or just i-cache
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	isb					@ isb to sych the new cssr&csidr
	mrc	p15, 1, r2, c0, c0, 0		@ read the new csidr
	isb
	and	r3, r2, #7			@ extract the length of the cache lines
	add	r3, r3, #4			@ add 4 (line length offset)
	@ldr	r5, =0x3ff
	mov	r5, #0x400
	sub	r5, #1
	ands	r5, r5, r2, lsr #3		@ find maximum number on the way size
	clz	r6, r5				@ find bit position of way size increment
	@ldr	r8, =0x7fff
	mov	r8, #0x8000
	sub	r8, #1
	ands	r8, r8, r2, lsr #13		@ extract max number of the index size
L2_cl_loop2:
	mov	r9, r5				@ create working copy of max way size
L2_cl_loop3:
	orr	r7, r10, r9, lsl r6		@ factor way and cache number into r7
	orr	r7, r7, r8, lsl r3		@ factor index number into r7
	teq	r0, #0
	mcreq	p15, 0, r7, c7, c10, 2		@ clean by set/way
	mcrne	p15, 0, r7, c7, c14, 2		@ flush by set/way

	subs	r9, r9, #1			@ decrement the way
	bge	L2_cl_loop3
	subs	r8, r8, #1			@ decrement the index
	bge	L2_cl_loop2
L2_cl_skip:
	@add	r10, r10, #2			@ increment cache number
	@cmp	r4, r10
	@bgt	L2_cl_loop1
L2_cl_finished:
	mov	r10, #0				@ swith back to cache level 0
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	dsb
	isb

	bx	lr
	.endfunc

	.global cpu_wake_up_errata_802022
cpu_wake_up_errata_802022:
	.func
@ Before Slave CPU goes to Errata WFI, we turn off GIC CPU interface to the CPU first.
	ldr	r0, =0x10212000
	mov	r1, #0
	str	r1, [r0]
1:
	isb
	dsb
	wfi
	b	1b
        .endfunc

@ This function takes three arguments
@ r0: Destination start address (must be word aligned)
@ r1: Source start address (must be word aligned)
@ r2: Number of words to copy
@ Return value is updated destination pointer (first unwritten word)

        .global copy_words
copy_words:
        .func
        push    {r3}
        cmp     r2, #0
        beq     1f
2:
        ldr     r3, [r1], #4
        str     r3, [r0], #4
        subs    r2, r2, #1
        bne     2b
1:
        pop     {r3}
        bx      lr

        .endfunc
