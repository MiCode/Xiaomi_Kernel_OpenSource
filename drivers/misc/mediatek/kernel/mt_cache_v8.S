#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>

	.text 
.equ SCTLR_C_BIT,  0x00000004
.equ SCTLR_I_BIT,  0x00001000
.equ DCISW,		0x0
.equ DCCISW,		0x1
.equ DCCSW,		0x2
.equ LOC_SHIFT, 	24
.equ CLIDR_FIELD_WIDTH, 3
.equ LEVEL_SHIFT, 	1

ENTRY(__enable_icache)
	mrs	x0, SCTLR_EL1
	orr	x0, x0, #SCTLR_I_BIT
	msr	SCTLR_EL1, x0
	ret
ENDPROC(__enable_icache)

ENTRY(__disable_icache)
	mrs	x0, SCTLR_EL1
	bic	x0, x0, #SCTLR_I_BIT
	msr	SCTLR_EL1, x0
	ret
ENDPROC(__disable_icache)

/* might pollute x0 */
.macro __dis_D
	mrs	x0, SCTLR_EL1
	bic	x0, x0, #SCTLR_C_BIT
	dsb	sy
	msr	SCTLR_EL1, x0
	dsb	sy
	isb	sy
.endm

ENTRY(__enable_dcache)
	mrs	x0, SCTLR_EL1
	orr	x0, x0, #SCTLR_C_BIT
	dsb	sy
	msr	SCTLR_EL1, x0
	dsb	sy
	isb	sy
	ret
ENDPROC(__enable_dcache)

ENTRY(__disable_dcache)
	mrs	x0, SCTLR_EL1
	bic	x0, x0, #SCTLR_C_BIT
	dsb	sy
	msr	SCTLR_EL1, x0
	dsb	sy
	isb	sy
	ret
ENDPROC(__disable_dcache)

ENTRY(__enable_cache)
	mrs	x0, SCTLR_EL1
	orr	x0, x0, #SCTLR_I_BIT
	orr	x0, x0, #SCTLR_C_BIT
	dsb	sy
	msr	SCTLR_EL1, x0
	dsb	sy
	isb	sy
	ret
ENDPROC(__enable_cache)

ENTRY(__disable_cache)
	mrs	x0, SCTLR_EL1
	bic	x0, x0, #SCTLR_I_BIT
	bic	x0, x0, #SCTLR_C_BIT
	dsb	sy
	msr	SCTLR_EL1, x0
	dsb	sy
	isb	sy
	ret
ENDPROC(__disable_cache)

/* ---------------------------------------------------------------
* Data cache operations by set/way to the level specified
*
* The main function, do_dcsw_op requires:
* x0: The operation type (0-2), as defined in arch.h
* x1: The first cache level to operate on
* x3: The last cache level to operate on
* x9: clidr_el1
* and will carry out the operation on each data cache from level 0
* to the level in x3 in sequence
*
* The dcsw_op macro sets up the x3 and x9 parameters based on
* clidr_el1 cache information before invoking the main function
* ---------------------------------------------------------------
*/
ENTRY(do_dcsw_op)
	lsl	x3, x3, #1
	cbz	x3, exit
	sub	x1, x1, #1
	lsl	x1, x1, #1
	mov	x10, x1
	adr	x14, dcsw_loop_table	// compute inner loop address
	add	x14, x14, x0, lsl #5	// inner loop is 8x32-bit instructions
	mov	x0, x9
	mov	w8, #1
loop:
	add	x2, x10, x10, lsr #1	// work out 3x current cache level
	lsr	x1, x0, x2		// extract cache type bits from clidr
	and	x1, x1, #7		// mask the bits for current cache only
	cmp	x1, #2			// see what cache we have at this level
	b.lt	level_done		// nothing to do if no cache or icache

	msr	csselr_el1, x10		// select current cache level in csselr
	isb				// isb to sych the new cssr&csidr
	mrs	x1, ccsidr_el1		// read the new ccsidr
	and	x2, x1, #7		// extract the length of the cache lines
	add	x2, x2, #4		// add 4 (line length offset)
	ubfx	x4, x1, #3, #10		// maximum way number
	clz	w5, w4			// bit position of way size increment
	lsl	w9, w4, w5		// w9 = aligned max way number
	lsl	w16, w8, w5		// w16 = way number loop decrement
	orr	w9, w10, w9		// w9 = combine way and cache number
	ubfx	w6, w1, #13, #15	// w6 = max set number
	lsl	w17, w8, w2		// w17 = set number loop decrement
	dsb	sy			// barrier before we start this level
	br	x14			// jump to DC operation specific loop

	.macro	dcsw_loop _op
loop2_\_op:
	lsl	w7, w6, w2		// w7 = aligned max set number

loop3_\_op:
	orr	w11, w9, w7		// combine cache, way and set number
	dc	\_op, x11
	subs	w7, w7, w17		// decrement set number
	b.ge	loop3_\_op

	subs	x9, x9, x16		// decrement way number
	b.ge	loop2_\_op

	b	level_done
	.endm

level_done:
	add	x10, x10, #2		// increment cache number
	cmp	x3, x10
	b.gt    loop
	msr	csselr_el1, xzr		// select cache level 0 in csselr
	dsb	sy			// barrier to complete final cache operation
	isb
exit:
	ret
ENDPROC(do_dcsw_op)

dcsw_loop_table:
	dcsw_loop isw
	dcsw_loop cisw
	dcsw_loop csw

.macro __inner_dcache_all mode
	mov	x0, \mode
	mov	x1, #1
	mrs	x9, clidr_el1
	ubfx	x3, x9, #24, #0x7 /* LOC as last cache level */
	b	do_dcsw_op
.endm

.macro __inner_dcache_L1 mode
	mov	x0, \mode
	mov	x1, #1
	mov	x3, #1
	mrs     x9, clidr_el1
	b	do_dcsw_op	
.endm

.macro __inner_dcache_L2 mode
	mov	x0, \mode
	mov	x1, #2
	mov	x3, #2
	mrs     x9, clidr_el1
	b	do_dcsw_op	
.endm

.macro __inner_dcache_L1_L2 mode
	mov	x0, \mode
	mov	x1, #1
	mov	x3, #2
	mrs     x9, clidr_el1
	b	do_dcsw_op	
.endm

ENTRY(__inner_flush_dcache_all)
	__inner_dcache_all #DCCISW
ENDPROC(__inner_flush_dcache_all)
	
ENTRY(__inner_flush_dcache_L1)
	__inner_dcache_L1 #DCCISW
ENDPROC(__inner_flush_dcache_L1)

ENTRY(__inner_flush_dcache_L2)
	__inner_dcache_L2 #DCCISW
ENDPROC(__inner_flush_dcache_L2)

ENTRY(__inner_clean_dcache_all)
	__inner_dcache_all #DCCSW
ENDPROC(__inner_clean_dcache_all)

ENTRY(__inner_clean_dcache_L1)
	__inner_dcache_L1 #DCCSW
ENDPROC(__inner_clean_dcache_L1)

ENTRY(__inner_clean_dcache_L2)
	__inner_dcache_L2 #DCCSW
ENDPROC(__inner_clean_dcache_L2)

ENTRY(__inner_inv_dcache_all)
	__inner_dcache_all #DCISW
ENDPROC(__inner_inv_dcache_all)

ENTRY(__inner_inv_dcache_L1)
	__inner_dcache_L1 #DCISW
ENDPROC(__inner_clean_dcache_L1)

ENTRY(__inner_inv_dcache_L2)
	__inner_dcache_L2 #DCISW
ENDPROC(__inner_clean_dcache_L2)

ENTRY(__disable_dcache__inner_flush_dcache_L1)
	__dis_D
	__inner_dcache_L1 #DCCISW
ENDPROC(__disable_dcache__inner_flush_dcache_L1)

ENTRY(__disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2)
	__dis_D
	__inner_dcache_L1_L2 #DCCISW
ENDPROC(__disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2)

ENTRY(__disable_dcache__inner_clean_dcache_L1__inner_clean_dcache_L2)
	__dis_D
	__inner_dcache_L1_L2 #DCCSW
ENDPROC(__disable_dcache__inner_clean_dcache_L1__inner_clean_dcache_L2)

ENTRY(__disable_dcache__inner_flush_dcache_L1__inner_clean_dcache_L2)
	__dis_D
	/* since we need to do different operations for L1/L2,
	   and our current implementation would jump from do_dcsw_op to caller(who invokes the last bl) directly,
	   we need to construct stack frame by ourself here.
	   We use two caller-saved registers, x12 & x13, to save lr & sp,
	   to prevent any memory access during cache operation
	   NOTICE: any macro or function MUST not corrupt x12 & x13 here
	*/
	mov	x12, x29
	mov	x13, x30
	mov	x29, sp
	bl	__inner_flush_dcache_L1
	mov	x29, x12
	mov	x30, x13
	__inner_dcache_L2 #DCCSW
ENDPROC(__disable_dcache__inner_flush_dcache_L1__inner_clean_dcache_L2)
