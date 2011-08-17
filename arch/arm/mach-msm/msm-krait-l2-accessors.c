/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

DEFINE_RAW_SPINLOCK(l2_access_lock);

u32 set_get_l2_indirect_reg(u32 reg_addr, u32 val)
{
	unsigned long flags;
	u32 ret_val;

	raw_spin_lock_irqsave(&l2_access_lock, flags);

	asm volatile ("mcr p15, 3, %0, c15, c0, 6" : : "r" (reg_addr));

	asm volatile ("mcr p15, 3, %0, c15, c0, 7" : : "r" (val));

	/* Ensure the value took */
	asm volatile ("mrc p15, 3, %0, c15, c0, 7" : "=r" (ret_val));

	raw_spin_unlock_irqrestore(&l2_access_lock, flags);

	return ret_val;
}

void set_l2_indirect_reg(u32 reg_addr, u32 val)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&l2_access_lock, flags);

	asm volatile ("mcr p15, 3, %0, c15, c0, 6" : : "r" (reg_addr));

	asm volatile ("mcr p15, 3, %0, c15, c0, 7" : : "r" (val));

	raw_spin_unlock_irqrestore(&l2_access_lock, flags);
}

u32 get_l2_indirect_reg(u32 reg_addr)
{
	u32 val;
	unsigned long flags;

	raw_spin_lock_irqsave(&l2_access_lock, flags);

	asm volatile ("mcr p15, 3, %0, c15, c0, 6" : : "r" (reg_addr));

	asm volatile ("mrc p15, 3, %0, c15, c0, 7" : "=r" (val));

	raw_spin_unlock_irqrestore(&l2_access_lock, flags);

	return val;
}
