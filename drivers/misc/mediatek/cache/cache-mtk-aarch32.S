/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>
#include <asm/errno.h>
#include <asm/unwind.h>

#include "proc-macros.S"

/*
 *      __xxxx_dcache_user_area(start,size)
 *
 *      Ensure that any D-cache lines for the interval [start, start+size)
 *      are cleaned or/and invalidated to the PoC.
 *
 *      - start        - virtual start address of region (EL0/EL1)
 *      - size         - size in question
 */



ENTRY(__clean_dcache_user_area)
	uaccess_enable r2
	add	r1, r1, r0
	dcache_line_size r2, r3
	sub	r3, r2, #1
	bic	r0, r0, r3
#ifdef CONFIG_ARM_ERRATA_764369
	ALT_SMP(W(dsb))
	ALT_UP(W(nop))
#endif
1:
#if defined(CONFIG_ARM_ERRATA_855873) || defined(CONFIG_ARM_ERRATA_824069)
	mcr	p15, 0, r0, c7, c14, 1	@ clean and invalidate D / U line
#else
	mcr	p15, 0, r0, c7, c10, 1	@ clean D / U line
#endif
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb	st
	uaccess_disable r2
	ret	lr
ENDPROC(__clean_dcache_user_area)

ENTRY(__clean_dcache_area_poc)
	add	r1, r1, r0
	b	v7_dma_clean_range
ENDPROC(__clean_dcache_area_poc)

ENTRY(__flush_dcache_user_area)
	uaccess_enable r2
	add	r1, r1, r0
	dcache_line_size r2, r3
	sub	r3, r2, #1
	bic	r0, r0, r3
#ifdef CONFIG_ARM_ERRATA_764369
	ALT_SMP(W(dsb))
	ALT_UP(W(nop))
#endif
1:
	mcr	p15, 0, r0, c7, c14, 1	@ clean & invalidate D / U line
	add	r0, r0, r2
	cmp	r0, r1
	blo	1b
	dsb	st
	uaccess_disable r2
	ret	lr
ENDPROC(__flush_dcache_user_area)

ENTRY(__flush_dcache_area)
	add	r1, r1, r0
	b	v7_dma_flush_range
ENDPROC(__flush_dcache_area)

ENTRY(__inval_dcache_user_area)
	b	__flush_dcache_user_area
ENDPROC(__inval_dcache_user_area)

ENTRY(__inval_dcache_area)
	add	r1, r1, r0
	b	v7_dma_inv_range
ENDPROC(__inval_dcache_area)

	.text
	.global __inner_flush_dcache_all
	.global __inner_flush_dcache_L1
	.global __inner_flush_dcache_L2
	.global __inner_clean_dcache_all
	.global __inner_clean_dcache_L1
	.global __inner_clean_dcache_L2
	.global __inner_inv_dcache_all
	.global __inner_inv_dcache_L1
	.global __inner_inv_dcache_L2
	.global __enable_dcache
	.global __enable_icache
	.global __enable_cache
	.global __disable_dcache
	.global __disable_icache
	.global __disable_cache
	.global __disable_dcache__inner_flush_dcache_L1
	.global __disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2
	.global __disable_dcache__inner_flush_dcache_L1__inner_clean_dcache_L2
	.global dis_D_inner_fL1L2
	.global d_i_dis_flush_all
	.global dis_D_inner_flush_all
.equ C1_IBIT ,  0x00001000
.equ C1_CBIT ,  0x00000004

__enable_icache:
	MRC p15,0,r0,c1,c0,0
	ORR r0,r0,#C1_IBIT
	MCR p15,0,r0,c1,c0,0
	BX lr
__disable_icache:
	MRC p15,0,r0,c1,c0,0
	BIC r0,r0,#C1_IBIT
	MCR p15,0,r0,c1,c0,0
	BX lr
__enable_dcache:
	MRC p15,0,r0,c1,c0,0
	ORR r0,r0,#C1_CBIT
	dsb
	MCR p15,0,r0,c1,c0,0
	dsb
	isb
	BX lr
__disable_dcache:
	MRC p15,0,r0,c1,c0,0
	BIC r0,r0,#C1_CBIT
	dsb
	MCR p15,0,r0,c1,c0,0
	dsb
	isb
/*
 * Erratum:794322,An instruction fetch can be allocated into
 * the L2 cache after the cache is disabled Status
 * This erratum can be avoided by inserting both of the following
 * after the SCTLR.C bit is cleared to 0,
 * and before the caches are cleaned or invalidated:
 * 1) A TLBIMVA operation to any address.
 * 2) A DSB instruction.
 */
	MCR p15,0,r0,c8,c7,1
	dsb
	isb
	BX lr
__enable_cache:
	MRC p15,0,r0,c1,c0,0
	ORR r0,r0,#C1_IBIT
	ORR r0,r0,#C1_CBIT
	MCR p15,0,r0,c1,c0,0
	BX lr
__disable_cache:
	MRC p15,0,r0,c1,c0,0
	BIC r0,r0,#C1_IBIT
	BIC r0,r0,#C1_CBIT
	MCR p15,0,r0,c1,c0,0
/*
 * Erratum:794322,An instruction fetch can be allocated
 * into the L2 cache after the cache is disabled Status
 * This erratum can be avoided by inserting both of the
 * following after the SCTLR.C bit is cleared to 0,
 * and before the caches are cleaned or invalidated:
 * 1) A TLBIMVA operation to any address.
 * 2) A DSB instruction.
 */
	MCR p15,0,r0,c8,c7,1
	dsb
	BX lr


__inner_flush_dcache_all:
	push    {r0,r1,r2,r3,r4,r5,r7,r8,r9,r10,r11,r14}
	dmb
	mrc     p15, 1, r0, c0, c0, 1
	ands    r3, r0, #0x7000000
	mov     r3, r3, lsr #23
	beq     all_finished
	mov     r10, #0
all_loop1:
	add     r2, r10, r10, lsr #1
	mov     r1, r0, lsr r2
	and     r1, r1, #7                      @
	cmp     r1, #2
	blt     all_skip
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
all_loop2:
	mov     r9, r4                          @
all_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
#ifdef CONFIG_L1C_OPT
#replace DCCISW by DCISW+DCCSW
	cmp     r10, #2
	mrsne   r1, cpsr                      @
	orrne   r8, r1, #PSR_I_BIT | PSR_F_BIT
	msrne   cpsr_c, r8
	mcrne   p15, 0, r11, c7, c10, 2         @
	mcrne   p15, 0, r11, c7, c6, 2         @
	msrne   cpsr_c, r1
	mcreq   p15, 0, r11, c7, c14, 2         @
#else
	mcr     p15, 0, r11, c7, c14, 2         @
#endif
	subs    r9, r9, #1                      @
	bge     all_loop3
	subs    r7, r7, #1                      @
	bge     all_loop2
all_skip:
	add     r10, r10, #2                    @
	cmp     r3, r10
	bgt     all_loop1
all_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
	pop     {r0,r1,r2,r3,r4,r5,r7,r8,r9,r10,r11,r14}
	bx      lr

__inner_flush_dcache_L1:
	push    {r0,r1,r2,r3,r4,r5,r7,r8,r9,r10,r11,r14}
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     L1_finished
	mov     r10, #0                         @
L1_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     L1_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
L1_loop2:
	mov     r9, r4                          @
L1_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
#ifdef CONFIG_L1C_OPT
#replace DCCISW by DCISW+DCCSW
	mrs   	r1, cpsr                      @
	orr     r8, r1, #PSR_I_BIT | PSR_F_BIT
	msr     cpsr_c, r8
	mcr     p15, 0, r11, c7, c10, 2         @
	mcr     p15, 0, r11, c7, c6, 2         @
	msr   	cpsr_c, r1
#else
	mcr     p15, 0, r11, c7, c14, 2         @
#endif
	subs    r9, r9, #1                      @
	bge     L1_loop3
	subs    r7, r7, #1                      @
	bge     L1_loop2
L1_skip:
	@
	@
	@
L1_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
	pop     {r0,r1,r2,r3,r4,r5,r7,r8,r9,r10,r11,r14}
	bx      lr

__inner_flush_dcache_L2:
	push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	@
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     L2_finished                        @
	mov     r10, #2                         @
L2_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     L2_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
L2_loop2:
	mov     r9, r4                          @
L2_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
	mcr     p15, 0, r11, c7, c14, 2         @
	subs    r9, r9, #1                      @
	bge     L2_loop3
	subs    r7, r7, #1                      @
	bge     L2_loop2
L2_skip:
	@
	@
	@
L2_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
	@
	pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	bx      lr

 __inner_clean_dcache_all:
	push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	@
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     all_cl_finished
	mov     r10, #0                         @
all_cl_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     all_cl_skip
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
all_cl_loop2:
	mov     r9, r4                          @
all_cl_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
	mcr     p15, 0, r11, c7, c10, 2         @

	subs    r9, r9, #1                      @
	bge     all_cl_loop3
	subs    r7, r7, #1                      @
	bge     all_cl_loop2
all_cl_skip:
	add     r10, r10, #2                    @
	cmp     r3, r10
	bgt     all_cl_loop1
all_cl_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
	@
	pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	bx      lr

__inner_clean_dcache_L1:
	push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	@
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     L1_cl_finished                        @
	mov     r10, #0                         @
L1_cl_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     L1_cl_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
L1_cl_loop2:
	mov     r9, r4                          @
L1_cl_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
	mcr     p15, 0, r11, c7, c10, 2         @

	subs    r9, r9, #1                      @
	bge     L1_cl_loop3
	subs    r7, r7, #1                      @
	bge     L1_cl_loop2
L1_cl_skip:
	@
	@
	@
L1_cl_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
	@
	pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	bx      lr

__inner_clean_dcache_L2:
#if 0
	mov     r0, sp
	mcr     p15, 0, r0, c7, c14, 1          @
	dsb
	sub     r0, r0, #64
	mcr     p15, 0, r0, c7, c14, 1          @
	dsb
#endif
	push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	@
#if 0
	mov     r0, sp
	mcr     p15, 0, r0, c7, c14, 1          @
	dsb
	sub     r0, r0, #64
	mcr     p15, 0, r0, c7, c14, 1          @
	dsb
#endif
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     L2_cl_finished                        @
	mov     r10, #2                         @
L2_cl_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     L2_cl_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
L2_cl_loop2:
	mov     r9, r4                          @
L2_cl_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
	mcr     p15, 0, r11, c7, c10, 2         @
	subs    r9, r9, #1                      @
	bge     L2_cl_loop3
	subs    r7, r7, #1                      @
	bge     L2_cl_loop2
L2_cl_skip:
	@
	@
	@
L2_cl_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
	@
	pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	bx      lr
 __inner_inv_dcache_all:
	push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	@
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     all_inv_finished                    @
	mov     r10, #0                         @
all_inv_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     all_inv_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
all_inv_loop2:
	mov     r9, r4                          @
all_inv_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
	mcr     p15, 0,  r11, c7, c6, 2         @

	subs    r9, r9, #1                      @
	bge     all_inv_loop3
	subs    r7, r7, #1                      @
	bge     all_inv_loop2
all_inv_skip:
	add     r10, r10, #2                    @
	cmp     r3, r10
	bgt     all_inv_loop1
all_inv_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
	@
	pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	bx      lr

__inner_inv_dcache_L1:
	push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	@
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     L1_inv_finished                        @
	mov     r10, #0                         @
L1_inv_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     L1_inv_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
L1_inv_loop2:
	mov     r9, r4                          @
L1_inv_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
	mcr     p15, 0,  r11, c7, c6, 2         @
	subs    r9, r9, #1                      @
	bge     L1_inv_loop3
	subs    r7, r7, #1                      @
	bge     L1_inv_loop2
L1_inv_skip:
	@
	@
	@
L1_inv_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
	@
	pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	bx      lr

__inner_inv_dcache_L2:
	push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	@
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     L2_inv_finished                        @
	mov     r10, #2                         @
L2_inv_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     L2_inv_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
L2_inv_loop2:
	mov     r9, r4                          @
L2_inv_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
	mcr     p15, 0,  r11, c7, c6, 2         @
	subs    r9, r9, #1                      @
	bge     L2_inv_loop3
	subs    r7, r7, #1                      @
	bge     L2_inv_loop2
L2_inv_skip:
	@
	@
	@
L2_inv_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
	@
	pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	bx      lr

__disable_dcache__inner_flush_dcache_L1:
/*******************************************************************************
 * push stack                                                                  *
 ******************************************************************************/
	push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
/*******************************************************************************
 * __disable_dcache                                                            *
 ******************************************************************************/
	MRC p15,0,r0,c1,c0,0
	BIC r0,r0,#C1_CBIT
	dsb
	MCR p15,0,r0,c1,c0,0
	dsb
	isb
/*
 * Erratum:794322,An instruction fetch can be allocated
 * into the L2 cache after the cache is disabled Status
 * This erratum can be avoided by inserting both of the
 * following after the SCTLR.C bit is cleared to 0,
 * and before the caches are cleaned or invalidated:
 * 1) A TLBIMVA operation to any address.
 * 2) A DSB instruction.
 */
	MCR p15,0,r0,c8,c7,1
	dsb
	isb
/*******************************************************************************
 * __inner_flush_dcache_L1                                                     *
 ******************************************************************************/
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     DF1_L1_finished                        @
	mov     r10, #0                         @
DF1_L1_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     DF1_L1_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
DF1_L1_loop2:
	mov     r9, r4                          @
DF1_L1_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
#if 1
	mcr     p15, 0, r11, c7, c10, 2         @
	mcr     p15, 0, r11, c7, c6, 2         @
#endif

#if 0
	mcr     p15, 0, r11, c7, c14, 2         @
#endif
	subs    r9, r9, #1                      @
	bge     DF1_L1_loop3
	subs    r7, r7, #1                      @
	bge     DF1_L1_loop2
DF1_L1_skip:
	@
	@
	@
DF1_L1_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
/*******************************************************************************
 * pop  stack                                                                  *
 ******************************************************************************/
	pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	bx      lr

__disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2:
/*******************************************************************************
 * push stack                                                                  *
 ******************************************************************************/
	push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
/*******************************************************************************
 * __disable_dcache                                                            *
 ******************************************************************************/
	MRC p15,0,r0,c1,c0,0
	BIC r0,r0,#C1_CBIT
	dsb
	MCR p15,0,r0,c1,c0,0
	dsb
	isb
/*
 * Erratum:794322,An instruction fetch can be allocated
 * into the L2 cache after the cache is disabled Status
 * This erratum can be avoided by inserting both of the
 * following after the SCTLR.C bit is cleared to 0,
 * and before the caches are cleaned or invalidated:
 * 1) A TLBIMVA operation to any address.
 * 2) A DSB instruction.
 */
	MCR p15,0,r0,c8,c7,1
	dsb
	isb
/*******************************************************************************
 * __inner_flush_dcache_L1                                                     *
 ******************************************************************************/
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     DF1F2_L1_finished                        @
	mov     r10, #0                         @
DF1F2_L1_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     DF1F2_L1_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
DF1F2_L1_loop2:
	mov     r9, r4                          @
DF1F2_L1_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
#if 1
	mcr     p15, 0, r11, c7, c10, 2         @
	mcr     p15, 0, r11, c7, c6, 2         @
#endif

#if 0
	mcr     p15, 0, r11, c7, c14, 2         @
#endif
	subs    r9, r9, #1                      @
	bge     DF1F2_L1_loop3
	subs    r7, r7, #1                      @
	bge     DF1F2_L1_loop2
DF1F2_L1_skip:
	@
	@
	@
DF1F2_L1_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
/*******************************************************************************
 * clrex                                                                       *
 ******************************************************************************/
	clrex
/*******************************************************************************
 * __inner_flush_dcache_L2                                                     *
 ******************************************************************************/
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     DF1F2_L2_finished                        @
	mov     r10, #2                         @
DF1F2_L2_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     DF1F2_L2_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
DF1F2_L2_loop2:
	mov     r9, r4                          @
DF1F2_L2_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
	mcr     p15, 0, r11, c7, c14, 2         @
	subs    r9, r9, #1                      @
	bge     DF1F2_L2_loop3
	subs    r7, r7, #1                      @
	bge     DF1F2_L2_loop2
DF1F2_L2_skip:
	@
	@
	@
DF1F2_L2_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
/*******************************************************************************
 * pop stack                                                                   *
 ******************************************************************************/
	pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	bx      lr

dis_D_inner_fL1L2:
__disable_dcache__inner_flush_dcache_L1__inner_clean_dcache_L2:
/*******************************************************************************
 * push stack                                                                  *
 ******************************************************************************/
	push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
/*******************************************************************************
 * __disable_dcache                                                            *
 ******************************************************************************/
	MRC p15,0,r0,c1,c0,0
	BIC r0,r0,#C1_CBIT
	dsb
	MCR p15,0,r0,c1,c0,0
	dsb
	isb
/*
 * Erratum:794322,An instruction fetch can be allocated
 * into the L2 cache after the cache is disabled Status
 * This erratum can be avoided by inserting both of the
 * following after the SCTLR.C bit is cleared to 0,
 * and before the caches are cleaned or invalidated:
 * 1) A TLBIMVA operation to any address.
 * 2) A DSB instruction.
 */
	MCR p15,0,r0,c8,c7,1
	dsb
	isb
/*******************************************************************************
 * __inner_flush_dcache_L1                                                     *
 ******************************************************************************/
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     DF1C2_L1_finished                        @
	mov     r10, #0                         @
DF1C2_L1_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     DF1C2_L1_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
DF1C2_L1_loop2:
	mov     r9, r4                          @
DF1C2_L1_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
#if 1
	mcr     p15, 0, r11, c7, c10, 2         @
	mcr     p15, 0, r11, c7, c6, 2         @
#endif

#if 0
	mcr     p15, 0, r11, c7, c14, 2         @
#endif
	subs    r9, r9, #1                      @
	bge     DF1C2_L1_loop3
	subs    r7, r7, #1                      @
	bge     DF1C2_L1_loop2
DF1C2_L1_skip:
	@
	@
	@
DF1C2_L1_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
/*******************************************************************************
 * clrex                                                                       *
 ******************************************************************************/
	clrex
/*******************************************************************************
 * __inner_clean_dcache_L2                                                     *
 ******************************************************************************/
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     DF1C2_L2_cl_finished                        @
	mov     r10, #2                         @
DF1C2_L2_cl_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     DF1C2_L2_cl_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
DF1C2_L2_cl_loop2:
	mov     r9, r4                          @
DF1C2_L2_cl_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
	mcr     p15, 0, r11, c7, c10, 2         @
	subs    r9, r9, #1                      @
	bge     DF1C2_L2_cl_loop3
	subs    r7, r7, #1                      @
	bge     DF1C2_L2_cl_loop2
DF1C2_L2_cl_skip:
	@
	@
	@
DF1C2_L2_cl_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
/*******************************************************************************
 * pop  stack                                                                  *
 ******************************************************************************/
	pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	bx      lr


d_i_dis_flush_all:
/*******************************************************************************
 * push stack                                                                  *
 ******************************************************************************/
	push    {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
/*******************************************************************************
 * __disable_dcache                                                            *
 ******************************************************************************/
	MRC p15,0,r0,c1,c0,0
	BIC r0,r0,#C1_CBIT
	BIC r0,r0,#C1_IBIT
	dsb
	MCR p15,0,r0,c1,c0,0
	dsb
	isb
/*
 * Erratum:794322,An instruction fetch can be allocated
 * into the L2 cache after the cache is disabled Status
 * This erratum can be avoided by inserting both of the
 * following after the SCTLR.C bit is cleared to 0,
 * and before the caches are cleaned or invalidated:
 * 1) A TLBIMVA operation to any address.
 * 2) A DSB instruction.
 */
	MCR p15,0,r0,c8,c7,1
	dsb
	isb
/*******************************************************************************
 * __inner_flush_dcache_L1                                                     *
 ******************************************************************************/
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     DIF1F2_L1_finished               @
	mov     r10, #0                         @
DIF1F2_L1_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     DIF1F2_L1_skip                   @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
DIF1F2_L1_loop2:
	mov     r9, r4                          @
DIF1F2_L1_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
#if 1
	mcr     p15, 0, r11, c7, c10, 2         @
	mcr     p15, 0, r11, c7, c6, 2         @
#endif

#if 0
	mcr     p15, 0, r11, c7, c14, 2         @
#endif
	subs    r9, r9, #1                      @
	bge     DIF1F2_L1_loop3
	subs    r7, r7, #1                      @
	bge     DIF1F2_L1_loop2
DIF1F2_L1_skip:
	@
	@
	@
DIF1F2_L1_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
/*******************************************************************************
 * clrex                                                                       *
 ******************************************************************************/
	clrex
/*******************************************************************************
 * __inner_flush_dcache_L2                                                     *
 ******************************************************************************/
	dmb                                     @
	mrc     p15, 1, r0, c0, c0, 1           @
	ands    r3, r0, #0x7000000              @
	mov     r3, r3, lsr #23                 @
	beq     DIF1F2_L2_finished                        @
	mov     r10, #2                         @
DIF1F2_L2_loop1:
	add     r2, r10, r10, lsr #1            @
	mov     r1, r0, lsr r2                  @
	and     r1, r1, #7                      @
	cmp     r1, #2                          @
	blt     DIF1F2_L2_skip                            @
#ifdef CONFIG_ARM_ERRATA_814220
	dsb
#endif
	mcr     p15, 2, r10, c0, c0, 0          @
	isb                                     @
	mrc     p15, 1, r1, c0, c0, 0           @
	and     r2, r1, #7                      @
	add     r2, r2, #4                      @
	ldr     r4, =0x3ff
	ands    r4, r4, r1, lsr #3              @
	clz     r5, r4                          @
	ldr     r7, =0x7fff
	ands    r7, r7, r1, lsr #13             @
DIF1F2_L2_loop2:
	mov     r9, r4                          @
DIF1F2_L2_loop3:
	orr     r11, r10, r9, lsl r5            @
	orr     r11, r11, r7, lsl r2            @
	mcr     p15, 0, r11, c7, c14, 2         @
	subs    r9, r9, #1                      @
	bge     DIF1F2_L2_loop3
	subs    r7, r7, #1                      @
	bge     DIF1F2_L2_loop2
DIF1F2_L2_skip:
	@
	@
	@
DIF1F2_L2_finished:
	mov     r10, #0                         @
	mcr     p15, 2, r10, c0, c0, 0          @
	dsb
	isb
/*******************************************************************************
 * pop stack                                                                   *
 ******************************************************************************/
	pop     {r0,r1,r2,r3,r4,r5,r7,r9,r10,r11,r14}
	bx      lr


dis_D_inner_flush_all:
	/* disable data cache*/

	MRC p15,0,r0,c1,c0,0
	BIC r0,r0,#C1_CBIT
	dsb
	MCR p15,0,r0,c1,c0,0
	dsb
	isb

	b	v7_flush_dcache_all

	.end
