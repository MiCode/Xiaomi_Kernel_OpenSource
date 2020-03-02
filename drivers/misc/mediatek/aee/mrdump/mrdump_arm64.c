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

#include <linux/ptrace.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>
#include <asm/traps.h>
#include <mt-plat/mrdump.h>
#include "mrdump_private.h"

void mrdump_save_current_backtrace(struct pt_regs *regs)
{
	asm volatile ("stp x0, x1, [%0]\n\t"
		      "stp x2, x3, [%0, #16]\n\t"
		      "stp x4, x5, [%0, #32]\n\t"
		      "stp x6, x7, [%0, #48]\n\t"
		      "stp x8, x9, [%0, #64]\n\t"
		      "stp x10, x11, [%0, #80]\n\t"
		      "stp x12, x13, [%0, #96]\n\t"
		      "stp x14, x15, [%0, #112]\n\t"
		      "stp x16, x17, [%0, #128]\n\t"
		      "stp x18, x19, [%0, #144]\n\t"
		      "stp x20, x21, [%0, #160]\n\t"
		      "stp x22, x23, [%0, #176]\n\t"
		      "stp x24, x25, [%0, #192]\n\t"
		      "stp x26, x27, [%0, #208]\n\t"
		      "stp x28, x29, [%0, #224]\n\t"
		      "str x30, [%0, #240]\n\t" : :
		      "r" (&regs->user_regs) : "memory");
	asm volatile ("mrs x8, currentel\n\t"
		      "mrs x9, daif\n\t"
		      "orr x8, x8, x9\n\t"
		      "mrs x9, nzcv\n\t"
		      "orr x8, x8, x9\n\t"
		      "mrs x9, spsel\n\t"
		      "orr x8, x8, x9\n\t"
		      "str x8, [%2]\n\t"
		      "mov x8, sp\n\t"
		      "str x8, [%0]\n\t"
		      "1:\n\t"
		      "adr x8, 1b\n\t"
		      "str x8, [%1]\n\t"
		      : : "r" (&regs->user_regs.sp), "r"(&regs->user_regs.pc),
		      "r"(&regs->user_regs.pstate)  : "x8", "x9", "memory");
}

void mrdump_save_control_register(void *creg)
{
	struct aarch64_ctrl_regs *cregs = (struct aarch64_ctrl_regs *)creg;

	asm volatile ("mrs %0, sctlr_el1\n\t"
		      "mrs %1, tcr_el1\n\t"
		      "mrs %2, ttbr0_el1\n\t"
		      "mrs %3, ttbr1_el1\n\t"
		      "mrs %4, sp_el0\n\t"
		      "mov %5, sp\n\t"
		      : "=r"(cregs->sctlr_el1), "=r"(cregs->tcr_el1),
		      "=r"(cregs->ttbr0_el1), "=r"(cregs->ttbr1_el1),
		      "=r"(cregs->sp_el[0]), "=r"(cregs->sp_el[1])
		      : : "memory");
}
