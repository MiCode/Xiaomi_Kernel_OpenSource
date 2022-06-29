// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/ptrace.h>
#include <mt-plat/mrdump.h>
#include "mrdump_private.h"

void mrdump_arch_fill_machdesc(const mrdump_machdesc *machdesc_p)
{
	u32 ttbr0;
	asm volatile ("mrc p15, 0, %0, c2, c0, 1\n\t"
		      :"=r"(ttbr0) : : "memory");
	machdesc_p->master_page_table = ttbr0;
}

void mrdump_save_control_register(void *creg)
{
	struct arm32_ctrl_regs *cregs = (struct arm32_ctrl_regs *)creg;

	asm volatile ("mrc p15, 0, %0, c1, c0, 0\n\t"
		      "mrc p15, 0, %1, c2, c0, 2\n\t"
		      "mrc p15, 0, %2, c2, c0, 0\n\t"
		      "mrc p15, 0, %3, c2, c0, 1\n\t"
		      : "=r"(cregs->sctlr), "=r"(cregs->ttbcr),
		      "=r"(cregs->ttbr0), "=r"(cregs->ttbr1) : : "memory");
}

void mrdump_arch_show_regs(const struct pt_regs *regs)
{
	pr_info("PC is at %pS\n", (void *)instruction_pointer(regs));
	pr_info("LR is at %pS\n", (void *)regs->ARM_lr);
	pr_info("pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n",
		regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr);
	pr_info("sp : %08lx  ip : %08lx  fp : %08lx\n",
		regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	pr_info("r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9,
		regs->ARM_r8);
	pr_info("r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6,
		regs->ARM_r5, regs->ARM_r4);
	pr_info("r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2,
		regs->ARM_r1, regs->ARM_r0);
}
