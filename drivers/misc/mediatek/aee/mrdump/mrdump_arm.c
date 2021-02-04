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

#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/ptrace.h>
#include <mt-plat/mrdump.h>
#include "mrdump_private.h"

void mrdump_save_current_backtrace(struct pt_regs *regs)
{
	asm volatile("stmia %1, {r0 - r15}\n\t"
		     "mrs %0, cpsr\n"
		     : "=r" (regs->uregs[16]) : "r"(regs) : "memory");
	asm volatile("bx lr");
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
