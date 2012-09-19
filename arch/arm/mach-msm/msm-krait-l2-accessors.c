/*
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <asm/mach-types.h>
#include <asm/cputype.h>

DEFINE_RAW_SPINLOCK(l2_access_lock);

u32 set_get_l2_indirect_reg(u32 reg_addr, u32 val)
{
	unsigned long flags;
	u32 ret_val;

	raw_spin_lock_irqsave(&l2_access_lock, flags);
	mb();
	asm volatile ("mcr     p15, 3, %[l2cpselr], c15, c0, 6\n\t"
		      "isb\n\t"
		      "mcr     p15, 3, %[l2cpdr],   c15, c0, 7\n\t"
		      "isb\n\t"
		      "mrc p15, 3, %[l2cpdr_read], c15, c0, 7\n\t"
			: [l2cpdr_read]"=r" (ret_val)
			: [l2cpselr]"r" (reg_addr), [l2cpdr]"r" (val)
	);
	raw_spin_unlock_irqrestore(&l2_access_lock, flags);

	return ret_val;
}
EXPORT_SYMBOL(set_get_l2_indirect_reg);

void set_l2_indirect_reg(u32 reg_addr, u32 val)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&l2_access_lock, flags);
	mb();
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
