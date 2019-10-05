/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/linkage.h>
#include <asm/assembler.h>
	.text

@ fp is 0 or stack frame

#define frame	r4
#define sv_fp	r5
#define sv_pc	r6
#define mask	r7
#define offset	r8

ENTRY(c_backtrace_ramconsole_print)

#if !defined(CONFIG_FRAME_POINTER) || !defined(CONFIG_PRINTK)
		mov	pc, lr
ENDPROC(c_backtrace_ramconsole_print)
#else
		stmfd	sp!, {r4 - r8, lr}
		movs	frame, r0
		beq	no_frame

		tst	r1, #0x10
 ARM(		moveq	mask, #0xfc000003	)
 THUMB(		moveq	mask, #0xfc000000	)
 THUMB(		orreq	mask, #0x03		)
		movne	mask, #0

1:		stmfd	sp!, {pc}
		ldr	r0, [sp], #4
		adr	r1, 1b
		sub	offset, r0, r1

/*
 * Stack frame layout:
 *             optionally saved caller registers (r4 - r10)
 *             saved fp
 *             saved sp
 *             saved lr
 *    frame => saved pc
 *             optionally saved arguments (r0 - r3)
 * saved sp => <next word>
 *
 * Functions start with the following code sequence:
 *                  mov   ip, sp
 *                  stmfd sp!, {r0 - r3} (optional)
 * corrected pc =>  stmfd sp!, {..., fp, ip, lr, pc}
 */
for_each_frame:	tst	frame, mask
		bne	no_frame

1001:		ldr	sv_pc, [frame, #0]
1002:		ldr	sv_fp, [frame, #-12]

		sub	sv_pc, sv_pc, offset
		bic	sv_pc, sv_pc, mask

1003:		ldr	r2, [sv_pc, #-4]
		ldr	r3, .Ldsi+4
		teq	r3, r2, lsr #10
		subne	r0, sv_pc, #4
		subeq	r0, sv_pc, #8

		ldr	r1, [frame, #-4]
		mov	r2, frame
		bic	r1, r1, mask
		bl	dump_backtrace_entry_ramconsole_print

		ldr	r1, [sv_pc, #-4]
		ldr	r3, .Ldsi+4
		teq	r3, r1, lsr #10
		ldreq	r0, [frame, #-8]
		subeq	r0, r0, #4
		bleq	.Ldumpstm

1004:		ldr	r1, [sv_pc, #0]
		ldr	r3, .Ldsi
		teq	r3, r1, lsr #10
		subeq	r0, frame, #16
		bleq	.Ldumpstm

		teq	sv_fp, #0
		beq	no_frame

		cmp	sv_fp, frame
		mov	frame, sv_fp
		bhi	for_each_frame

1006:		adr	r0, .Lbad
		mov	r1, frame
#ifdef CONFIG_MTK_RAM_CONSOLE
		bl	aee_sram_fiq_log
#endif
no_frame:	ldmfd	sp!, {r4 - r8, pc}
ENDPROC(c_backtrace_ramconsole_print)

		.pushsection __ex_table,"a"
		.align	3
		.long	1001b, 1006b
		.long	1002b, 1006b
		.long	1003b, 1006b
		.long	1004b, 1006b
		.popsection

#define instr r4
#define reg   r5
#define stack r6

.Ldumpstm:	stmfd	sp!, {instr, reg, stack, r7, lr}
		mov	stack, r0
		mov	instr, r1
		mov	reg, #10
		mov	r7, #0
1:		mov	r3, #1
 ARM(		tst	instr, r3, lsl reg	)
 THUMB(		lsl	r3, reg			)
 THUMB(		tst	instr, r3		)
		beq	2f
		add	r7, r7, #1
		teq	r7, #6
		moveq	r7, #1
		moveq	r1, #'\n'
		movne	r1, #' '
		ldr	r3, [stack], #-4
		mov	r2, reg
		adr	r0, .Lfp
		bl	dump_regs
2:		subs	reg, reg, #1
		bpl	1b
		teq	r7, #0
		adrne	r0, .Lcr
#ifdef CONFIG_MTK_RAM_CONSOLE
		blne	aee_sram_fiq_log
#endif
		ldmfd	sp!, {instr, reg, stack, r7, pc}

.Lfp:		.asciz	"%cr%d:%08x"
.Lcr:		.asciz	"\n"
.Lbad:		.asciz	"Backtrace aborted due to bad frame pointer <%p>\n"
		.align
.Ldsi:		.word	0xe92dd800 >> 10
		.word	0xe92d0000 >> 10

#endif
