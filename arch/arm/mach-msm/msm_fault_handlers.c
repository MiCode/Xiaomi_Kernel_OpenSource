/*
 *  Copyright (C) 1995  Linus Torvalds
 *  Modifications for ARM processor (c) 1995-2004 Russell King
 *  Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/cpumask.h>
#include "acpuclock.h"

#define __str(x) #x
#define MRC(x, v1, v2, v4, v5, v6) do {					\
	unsigned int __##x;						\
	asm("mrc " __str(v1) ", " __str(v2) ", %0, " __str(v4) ", "	\
		__str(v5) ", " __str(v6) "\n" \
		: "=r" (__##x));					\
	pr_info("%s: %s = 0x%.8x\n", __func__, #x, __##x);		\
} while (0)

static int msm_imp_ext_abort(unsigned long addr, unsigned int fsr,
			     struct pt_regs *regs)
{
	int cpu;
	unsigned int regval;
	static unsigned char flush_toggle;

	asm("mrc p15, 7, %0, c15, c0, 1\n" /* read EFSR for fault status */
	    : "=r" (regval));
	if (regval == 0x2) {
		/* Fault was caused by icache parity error. Alternate
		 * simply retrying the access and flushing the icache. */
		flush_toggle ^= 1;
		if (flush_toggle)
			asm("mcr p15, 0, %0, c7, c5, 0\n"
				:
				: "r" (regval)); /* input value is ignored */
		/* Clear fault in EFSR. */
		asm("mcr p15, 7, %0, c15, c0, 1\n"
			:
			: "r" (regval));
		/* Clear fault in ADFSR. */
		regval = 0;
		asm("mcr p15, 0, %0, c5, c1, 0\n"
			:
			: "r" (regval));
		return 0;
	}

	MRC(ADFSR,    p15, 0,  c5, c1, 0);
	MRC(DFSR,     p15, 0,  c5, c0, 0);
	MRC(ACTLR,    p15, 0,  c1, c0, 1);
	MRC(EFSR,     p15, 7, c15, c0, 1);
	MRC(L2SR,     p15, 3, c15, c1, 0);
	MRC(L2CR0,    p15, 3, c15, c0, 1);
	MRC(L2CPUESR, p15, 3, c15, c1, 1);
	MRC(L2CPUCR,  p15, 3, c15, c0, 2);
	MRC(SPESR,    p15, 1,  c9, c7, 0);
	MRC(SPCR,     p15, 0,  c9, c7, 0);
	MRC(DMACHSR,  p15, 1, c11, c0, 0);
	MRC(DMACHESR, p15, 1, c11, c0, 1);
	MRC(DMACHCR,  p15, 0, c11, c0, 2);
	for_each_online_cpu(cpu)
		pr_info("cpu %d, acpuclk rate: %lu kHz\n", cpu,
			acpuclk_get_rate(cpu));

	return 1;
}

static int __init msm_register_fault_handlers(void)
{
	/* hook in our handler for imprecise abort for when we get
	   i-cache parity errors */
	hook_fault_code(22, msm_imp_ext_abort, SIGBUS, 0,
			"imprecise external abort");

	return 0;
}
arch_initcall(msm_register_fault_handlers);
