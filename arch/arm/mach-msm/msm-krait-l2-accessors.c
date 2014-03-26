/*
 * Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/msm_rtb.h>
#include <asm/cputype.h>

DEFINE_RAW_SPINLOCK(l2_access_lock);

void set_l2_indirect_reg(u32 reg_addr, u32 val)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&l2_access_lock, flags);
	mb();
	uncached_logk(LOGK_L2CPWRITE, (void *)reg_addr);
	asm volatile ("mcr     p15, 3, %[l2cpselr], c15, c0, 6\n\t"
		      "isb\n\t"
		      "mcr     p15, 3, %[l2cpdr],   c15, c0, 7\n\t"
		      "isb\n\t"
			:
			: [l2cpselr]"r" (reg_addr), [l2cpdr]"r" (val)
	);
	raw_spin_unlock_irqrestore(&l2_access_lock, flags);
}
EXPORT_SYMBOL(set_l2_indirect_reg);

u32 get_l2_indirect_reg(u32 reg_addr)
{
	u32 val;
	unsigned long flags;

	raw_spin_lock_irqsave(&l2_access_lock, flags);
	uncached_logk(LOGK_L2CPREAD, (void *)reg_addr);
	asm volatile ("mcr     p15, 3, %[l2cpselr], c15, c0, 6\n\t"
		      "isb\n\t"
		      "mrc     p15, 3, %[l2cpdr],   c15, c0, 7\n\t"
			: [l2cpdr]"=r" (val)
			: [l2cpselr]"r" (reg_addr)
	);
	raw_spin_unlock_irqrestore(&l2_access_lock, flags);

	return val;
}
EXPORT_SYMBOL(get_l2_indirect_reg);
