// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/ptrace.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>
#include <asm/traps.h>
#include <mt-plat/mrdump.h>
#include "mrdump_private.h"

void mrdump_save_control_register(void *creg)
{
	struct aarch64_ctrl_regs *cregs = (struct aarch64_ctrl_regs *)creg;
	u64 tmp;

	asm volatile ("mrs %0, sctlr_el1\n\t"
		      "mrs %1, tcr_el1\n\t"
		      "mrs %2, ttbr0_el1\n\t"
		      "mrs %3, ttbr1_el1\n\t"
		      "mrs %4, sp_el0\n\t"
		      "mov %5, sp\n\t"
		      : "=&r"(tmp), "=r"(cregs->tcr_el1),
		      "=r"(cregs->ttbr0_el1), "=r"(cregs->ttbr1_el1),
		      "=r"(cregs->sp_el[0]), "=r"(cregs->sp_el[1])
		      : : "memory");
	cregs->sctlr_el1 = (uint32_t) tmp;
}
