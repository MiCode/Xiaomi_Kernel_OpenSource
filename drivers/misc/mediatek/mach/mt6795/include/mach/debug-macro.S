/* linux/include/asm-arm/arch-mt6575/debug-macro.S
 *
 * Debugging macro include header
 *
 *  Copyright (C) 2006 MediaTek Inc.
 *
 */

@#include <asm/arch/hardware.h>

	.macro  addruart, rp, rv, tmp
	mov	\rp, #0x2000
	movt	\rp, #0x1100
	mov	\rv, #0x2000
	movt	\rv, #0xF100
	.endm
	
	.macro  senduart,rd,rx
	strb	\rd, [\rx, #0x0]
	.endm
	
	.macro  waituart,rd,rx
1001:	ldr	\rd, [\rx, #0x14]	@ UART0 status
	tst	\rd, #0x40		@ write allow
	beq	1001b
	.endm
	
	.macro  busyuart,rd,rx
1002:   ldr	\rd, [\rx, #0x14]	@ UART0 status
	tst	\rd, #0x40		@ uart full .eq. busy
	beq	1002b
	.endm
