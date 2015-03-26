/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <soc/qcom/kryo-l2-accessors.h>

#define	L2CPUSRSELR_EL1	S3_3_c15_c0_6
#define	L2CPUSRDR_EL1	S3_3_c15_c0_7

#define arm64_sys_reg_read(reg) ({					\
	u64 __val;							\
	asm volatile("mrs %0, " __stringify(reg) : "=r" (__val));	\
	__val;								\
})

#define arm64_sys_reg_write(reg, val) {					\
	asm volatile("msr " __stringify(reg) ", %0" : : "r" (val));	\
}

static DEFINE_RAW_SPINLOCK(l2_access_lock);

/**
 * set_l2_indirect_reg: write value to an L2 register
 * @reg: Address of L2 register.
 * @value: Value to be written to register.
 *
 * Use architecturally required barriers for ordering between system register
 * accesses, and system registers with respect to device memory
 */
void set_l2_indirect_reg(u64 reg, u64 val)
{
	unsigned long flags;
	mb();
	raw_spin_lock_irqsave(&l2_access_lock, flags);
	uncached_logk(LOGK_L2CPWRITE, (void *)reg);
	arm64_sys_reg_write(L2CPUSRSELR_EL1, reg);
	isb();
	arm64_sys_reg_write(L2CPUSRDR_EL1, val);
	isb();
	raw_spin_unlock_irqrestore(&l2_access_lock, flags);
}
EXPORT_SYMBOL(set_l2_indirect_reg);

/**
 * get_l2_indirect_reg: read an L2 register value
 * @reg: Address of L2 register.
 *
 * Use architecturally required barriers for ordering between system register
 * accesses, and system registers with respect to device memory
 */
u64 get_l2_indirect_reg(u64 reg)
{
	u64 val;
	unsigned long flags;

	raw_spin_lock_irqsave(&l2_access_lock, flags);
	uncached_logk(LOGK_L2CPREAD, (void *)reg);
	arm64_sys_reg_write(L2CPUSRSELR_EL1, reg);
	isb();
	val = arm64_sys_reg_read(L2CPUSRDR_EL1);
	raw_spin_unlock_irqrestore(&l2_access_lock, flags);

	return val;
}
EXPORT_SYMBOL(get_l2_indirect_reg);
