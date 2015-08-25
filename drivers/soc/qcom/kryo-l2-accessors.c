/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/seq_file.h>
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

#if defined(CONFIG_DEBUG_FS)

static u32 debug_addr;
static int debug_target_cpu;

static void remote_l2_ia_read(void *data)
{
	u64 *val = data;
	*val = get_l2_indirect_reg(debug_addr);
}

static void remote_l2_ia_write(void *data)
{
	u64 *val = data;

	set_l2_indirect_reg(debug_addr, *val);
}

static int l2_indirect_target_cpu_set(void *data, u64 val)
{
	if (val > num_possible_cpus())
		return -EINVAL;

	debug_target_cpu = val;
	return 0;
}

static int l2_indirect_target_cpu_get(void *data, u64 *val)
{
	*val = debug_target_cpu;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(l2_ia_cpu_fops, l2_indirect_target_cpu_get,
			l2_indirect_target_cpu_set, "%llu\n");

static int l2_indirect_addr_set(void *data, u64 val)
{
	debug_addr = val;
	return 0;
}

static int l2_indirect_addr_get(void *data, u64 *val)
{
	*val = debug_addr;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(l2_ia_addr_fops, l2_indirect_addr_get,
			l2_indirect_addr_set, "%llu\n");

static int l2_indirect_val_set(void *data, u64 val)
{
	return smp_call_function_single(debug_target_cpu, remote_l2_ia_write,
					&val, 1);
}

DEFINE_SIMPLE_ATTRIBUTE(l2_ia_set_fops, NULL,
			l2_indirect_val_set, "%llu\n");

static int l2_indirect_get(void *data, u64 *val)
{
	return smp_call_function_single(debug_target_cpu, remote_l2_ia_read,
					val, 1);
}

DEFINE_SIMPLE_ATTRIBUTE(l2_ia_get_fops, l2_indirect_get,
			NULL, "%llu\n");

/**
 * l2_ia_debug_init() - Initialize L2 indirect access register debugfs
 */
static int l2_ia_debug_init(void)
{
	static struct dentry *debugfs_base;

	debugfs_base = debugfs_create_dir("l2_indirect", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	if (!debugfs_create_file("get", S_IRUGO, debugfs_base, NULL,
				&l2_ia_get_fops))
		return -ENOMEM;

	if (!debugfs_create_file("set", S_IRUGO | S_IWUSR, debugfs_base, NULL,
				&l2_ia_set_fops))
		return -ENOMEM;

	if (!debugfs_create_file("address", S_IRUGO | S_IWUSR, debugfs_base,
				 NULL, &l2_ia_addr_fops))
		return -ENOMEM;

	if (!debugfs_create_file("target_cpu", S_IRUGO | S_IWUSR, debugfs_base,
				 NULL, &l2_ia_cpu_fops))
		return -ENOMEM;

	return 0;
}
late_initcall(l2_ia_debug_init);

#endif /* CONFIG_DEBUG_FS */
