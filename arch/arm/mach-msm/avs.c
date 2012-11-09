/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/mach-types.h>
#include <asm/cputype.h>
#include "avs.h"

u32 avs_get_avscsr(void)
{
	u32 val = 0;

	asm volatile ("mrc p15, 7, %[avscsr], c15, c1, 7\n\t"
			: [avscsr]"=r" (val)
	);

	return val;
}
EXPORT_SYMBOL(avs_get_avscsr);

void avs_set_avscsr(u32 avscsr)
{
	asm volatile ("mcr p15, 7, %[avscsr], c15, c1, 7\n\t"
		      "isb\n\t"
			:
			: [avscsr]"r" (avscsr)
	);
}
EXPORT_SYMBOL(avs_set_avscsr);

u32 avs_get_avsdscr(void)
{
	u32 val = 0;

	asm volatile ("mrc p15, 7, %[avsdscr], c15, c0, 6\n\t"
			: [avsdscr]"=r" (val)
	);

	return val;
}
EXPORT_SYMBOL(avs_get_avsdscr);

void avs_set_avsdscr(u32 avsdscr)
{
	asm volatile("mcr p15, 7, %[avsdscr], c15, c0, 6\n\t"
		     "isb\n\t"
			:
			: [avsdscr]"r" (avsdscr)
	);
}
EXPORT_SYMBOL(avs_set_avsdscr);

static void avs_enable_local(void *data)
{
	u32 avsdscr = (u32) data;
	u32 avscsr_enable = 0x61;

	avs_set_avsdscr(avsdscr);
	avs_set_avscsr(avscsr_enable);
}

static void avs_disable_local(void *data)
{
	avs_set_avscsr(0);
}

void avs_enable(int cpu, u32 avsdscr)
{
	int ret;

	ret = smp_call_function_single(cpu, avs_enable_local,
			(void *)avsdscr, true);
	WARN_ON(ret);
}
EXPORT_SYMBOL(avs_enable);

void avs_disable(int cpu)
{
	int ret;

	ret = smp_call_function_single(cpu, avs_disable_local,
			(void *) 0, true);
	WARN_ON(ret);
}
EXPORT_SYMBOL(avs_disable);
