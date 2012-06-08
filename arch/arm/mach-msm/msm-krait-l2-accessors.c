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

#define L2CPMR		0x500
#define L2CPUCPMR	0x501
#define L2CPUVRF8	0x708
#define CPUNDX_MASK	(0x7 << 12)

/*
 * For Krait versions found in APQ8064v1.x, save L2CPUVRF8 before
 * L2CPMR or L2CPUCPMR writes and restore it after to work around an
 * issue where L2CPUVRF8 becomes corrupt.
 */
static bool l2cpuvrf8_needs_fix(u32 reg_addr)
{
	switch (read_cpuid_id()) {
	case 0x510F06F0: /* KR28M4A10  */
	case 0x510F06F1: /* KR28M4A10B */
	case 0x510F06F2: /* KR28M4A11  */
		break;
	default:
		return false;
	};

	switch (reg_addr & ~CPUNDX_MASK) {
	case L2CPMR:
	case L2CPUCPMR:
		return true;
	default:
		return false;
	}
}

static u32 l2cpuvrf8_fix_save(u32 reg_addr, u32 *l2cpuvrf8_val)
{
	u32 l2cpuvrf8_addr = L2CPUVRF8 | (reg_addr & CPUNDX_MASK);

	mb();
	asm volatile ("mcr     p15, 3, %[l2cpselr], c15, c0, 6\n\t"
		      "isb\n\t"
		      "mrc     p15, 3, %[l2cpdr],   c15, c0, 7\n\t"
			: [l2cpdr]"=r" (*l2cpuvrf8_val)
			: [l2cpselr]"r" (l2cpuvrf8_addr)
	);

	return l2cpuvrf8_addr;
}

static void l2cpuvrf8_fix_restore(u32 l2cpuvrf8_addr, u32 l2cpuvrf8_val)
{
	mb();
	asm volatile ("mcr     p15, 3, %[l2cpselr], c15, c0, 6\n\t"
		      "isb\n\t"
		      "mcr     p15, 3, %[l2cpdr],   c15, c0, 7\n\t"
		      "isb\n\t"
			:
			: [l2cpselr]"r" (l2cpuvrf8_addr),
			  [l2cpdr]"r" (l2cpuvrf8_val)
	);
}

u32 set_get_l2_indirect_reg(u32 reg_addr, u32 val)
{
	unsigned long flags;
	u32 uninitialized_var(l2cpuvrf8_val), l2cpuvrf8_addr = 0;
	u32 ret_val;

	/* CP15 registers are not emulated on RUMI3. */
	if (machine_is_msm8960_rumi3())
		return 0;

	raw_spin_lock_irqsave(&l2_access_lock, flags);

	if (l2cpuvrf8_needs_fix(reg_addr))
		l2cpuvrf8_addr = l2cpuvrf8_fix_save(reg_addr, &l2cpuvrf8_val);

	mb();
	asm volatile ("mcr     p15, 3, %[l2cpselr], c15, c0, 6\n\t"
		      "isb\n\t"
		      "mcr     p15, 3, %[l2cpdr],   c15, c0, 7\n\t"
		      "isb\n\t"
		      "mrc p15, 3, %[l2cpdr_read], c15, c0, 7\n\t"
			: [l2cpdr_read]"=r" (ret_val)
			: [l2cpselr]"r" (reg_addr), [l2cpdr]"r" (val)
	);

	if (l2cpuvrf8_addr)
		l2cpuvrf8_fix_restore(l2cpuvrf8_addr, l2cpuvrf8_val);

	raw_spin_unlock_irqrestore(&l2_access_lock, flags);

	return ret_val;
}
EXPORT_SYMBOL(set_get_l2_indirect_reg);

void set_l2_indirect_reg(u32 reg_addr, u32 val)
{
	unsigned long flags;
	u32 uninitialized_var(l2cpuvrf8_val), l2cpuvrf8_addr = 0;

	/* CP15 registers are not emulated on RUMI3. */
	if (machine_is_msm8960_rumi3())
		return;

	raw_spin_lock_irqsave(&l2_access_lock, flags);

	if (l2cpuvrf8_needs_fix(reg_addr))
		l2cpuvrf8_addr = l2cpuvrf8_fix_save(reg_addr, &l2cpuvrf8_val);

	mb();
	asm volatile ("mcr     p15, 3, %[l2cpselr], c15, c0, 6\n\t"
		      "isb\n\t"
		      "mcr     p15, 3, %[l2cpdr],   c15, c0, 7\n\t"
		      "isb\n\t"
			:
			: [l2cpselr]"r" (reg_addr), [l2cpdr]"r" (val)
	);

	if (l2cpuvrf8_addr)
		l2cpuvrf8_fix_restore(l2cpuvrf8_addr, l2cpuvrf8_val);

	raw_spin_unlock_irqrestore(&l2_access_lock, flags);
}
EXPORT_SYMBOL(set_l2_indirect_reg);

u32 get_l2_indirect_reg(u32 reg_addr)
{
	u32 val;
	unsigned long flags;
	/* CP15 registers are not emulated on RUMI3. */
	if (machine_is_msm8960_rumi3())
		return 0;

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
