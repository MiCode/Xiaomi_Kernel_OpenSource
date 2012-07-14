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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/cpu.h>
#include <linux/smp.h>

#include <mach/msm_bus.h>
#include <mach/msm-krait-l2-accessors.h>

#include "acpuclock-krait.h"

static struct drv_data *drv;
static DEFINE_MUTEX(debug_lock);

struct acg_action {
	bool set;
	bool enable;
};
static int l2_acg_en_val[MAX_SCALABLES];
static struct dentry *base_dir;
static struct dentry *sc_dir[MAX_SCALABLES];

static void cpu_action(void *info)
{
	struct acg_action *action = info;

	u32 val;
	asm volatile ("mrc p15, 7, %[cpmr0], c15, c0, 5\n\t"
			: [cpmr0]"=r" (val));
	if (action->set) {
		if (action->enable)
			val &= ~BIT(0);
		else
			val |= BIT(0);
		asm volatile ("mcr p15, 7, %[l2cpdr], c15, c0, 5\n\t"
				: : [l2cpdr]"r" (val));
	} else {
		action->enable = !(val & BIT(0));
	}
}

/* Disable auto clock-gating for a scalable. */
static void disable_acg(int sc_id)
{
	u32 regval;

	if (sc_id == L2) {
		regval = get_l2_indirect_reg(drv->scalable[sc_id].l2cpmr_iaddr);
		l2_acg_en_val[sc_id] = regval & (0x3 << 10);
		regval |= (0x3 << 10);
		set_l2_indirect_reg(drv->scalable[sc_id].l2cpmr_iaddr, regval);
	} else {
		struct acg_action action = { .set = true, .enable = false };
		smp_call_function_single(sc_id, cpu_action, &action, 1);
	}
}

/* Enable auto clock-gating for a scalable. */
static void enable_acg(int sc_id)
{
	u32 regval;

	if (sc_id == L2) {
		regval = get_l2_indirect_reg(drv->scalable[sc_id].l2cpmr_iaddr);
		regval &= ~(0x3 << 10);
		regval |= l2_acg_en_val[sc_id];
		set_l2_indirect_reg(drv->scalable[sc_id].l2cpmr_iaddr, regval);
	} else {
		struct acg_action action = { .set = true, .enable = true };
		smp_call_function_single(sc_id, cpu_action, &action, 1);
	}
}

/* Check if auto clock-gating for a scalable. */
static bool acg_is_enabled(int sc_id)
{
	u32 regval;

	if (sc_id == L2) {
		regval = get_l2_indirect_reg(drv->scalable[sc_id].l2cpmr_iaddr);
		return ((regval >> 10) & 0x3) != 0x3;
	} else {
		struct acg_action action = { .set = false };
		smp_call_function_single(sc_id, cpu_action, &action, 1);
		return action.enable;
	}
}

/* Enable/Disable auto clock gating. */
static int acg_set(void *data, u64 val)
{
	int ret = 0;
	int sc_id = (int)data;

	mutex_lock(&debug_lock);
	get_online_cpus();
	if (!sc_dir[sc_id]) {
		ret = -ENODEV;
		goto out;
	}

	if (val == 0 && acg_is_enabled(sc_id))
		disable_acg(sc_id);
	else if (val == 1)
		enable_acg(sc_id);
out:
	put_online_cpus();
	mutex_unlock(&debug_lock);

	return ret;
}

/* Get auto clock-gating state. */
static int acg_get(void *data, u64 *val)
{
	int ret = 0;
	int sc_id = (int)data;

	mutex_lock(&debug_lock);
	get_online_cpus();
	if (!sc_dir[sc_id]) {
		ret = -ENODEV;
		goto out;
	}

	*val = acg_is_enabled(sc_id);
out:
	put_online_cpus();
	mutex_unlock(&debug_lock);

	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(acgd_fops, acg_get, acg_set, "%lld\n");

/* Get the rate */
static int rate_get(void *data, u64 *val)
{
	int sc_id = (int)data;
	*val = drv->scalable[sc_id].cur_speed->khz;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rate_fops, rate_get, NULL, "%lld\n");

/* Get the HFPLL's L-value. */
static int hfpll_l_get(void *data, u64 *val)
{
	int sc_id = (int)data;
	*val = drv->scalable[sc_id].cur_speed->pll_l_val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(hfpll_l_fops, hfpll_l_get, NULL, "%lld\n");

/* Get the L2 rate vote. */
static int l2_vote_get(void *data, u64 *val)
{
	int level, sc_id = (int)data;

	level = drv->scalable[sc_id].l2_vote;
	*val = drv->l2_freq_tbl[level].speed.khz;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(l2_vote_fops, l2_vote_get, NULL, "%lld\n");

/* Get the bandwidth vote. */
static int bw_vote_get(void *data, u64 *val)
{
	struct l2_level *l;

	l = container_of(drv->scalable[L2].cur_speed,
			 struct l2_level, speed);
	*val = drv->bus_scale->usecase[l->bw_level].vectors->ib;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(bw_vote_fops, bw_vote_get, NULL, "%lld\n");

/* Get the name of the currently-selected clock source. */
static int src_name_show(struct seq_file *m, void *unused)
{
	const char *const src_names[NUM_SRC_ID] = {
		[PLL_0] = "PLL0",
		[HFPLL] = "HFPLL",
		[PLL_8] = "PLL8",
	};
	int src, sc_id = (int)m->private;

	src = drv->scalable[sc_id].cur_speed->src;
	if (src > ARRAY_SIZE(src_names))
		return -EINVAL;

	seq_printf(m, "%s\n", src_names[src]);

	return 0;
}

static int src_name_open(struct inode *inode, struct file *file)
{
	return single_open(file, src_name_show, inode->i_private);
}

static const struct file_operations src_name_fops = {
	.open		= src_name_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

/* Get speed_bin ID */
static int speed_bin_get(void *data, u64 *val)
{
	*val = drv->speed_bin;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(speed_bin_fops, speed_bin_get, NULL, "%lld\n");

/* Get pvs_bin ID */
static int pvs_bin_get(void *data, u64 *val)
{
	*val = drv->pvs_bin;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(pvs_bin_fops, pvs_bin_get, NULL, "%lld\n");

/* Get boost_uv */
static int boost_get(void *data, u64 *val)
{
	*val = drv->boost_uv;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(boost_fops, boost_get, NULL, "%lld\n");

static void __cpuinit add_scalable_dir(int sc_id)
{
	char sc_name[8];

	if (sc_id == L2)
		snprintf(sc_name, sizeof(sc_name), "l2");
	else
		snprintf(sc_name, sizeof(sc_name), "cpu%d", sc_id);

	sc_dir[sc_id] = debugfs_create_dir(sc_name, base_dir);
	if (!sc_dir[sc_id])
		return;

	debugfs_create_file("auto_gating", S_IRUGO | S_IWUSR,
			sc_dir[sc_id], (void *)sc_id, &acgd_fops);

	debugfs_create_file("rate", S_IRUGO,
			sc_dir[sc_id], (void *)sc_id, &rate_fops);

	debugfs_create_file("hfpll_l", S_IRUGO,
			sc_dir[sc_id], (void *)sc_id, &hfpll_l_fops);

	debugfs_create_file("src", S_IRUGO,
			sc_dir[sc_id], (void *)sc_id, &src_name_fops);

	if (sc_id == L2)
		debugfs_create_file("bw_ib_vote", S_IRUGO,
			sc_dir[sc_id], (void *)sc_id, &bw_vote_fops);
	else
		debugfs_create_file("l2_vote", S_IRUGO,
			sc_dir[sc_id], (void *)sc_id, &l2_vote_fops);
}

static void __cpuinit remove_scalable_dir(int sc_id)
{
	debugfs_remove_recursive(sc_dir[sc_id]);
	sc_dir[sc_id] = NULL;
}

static int __cpuinit debug_cpu_callback(struct notifier_block *nfb,
			unsigned long action, void *hcpu)
{
	int cpu = (int)hcpu;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_DOWN_FAILED:
	case CPU_UP_PREPARE:
		add_scalable_dir(cpu);
		break;
	case CPU_UP_CANCELED:
	case CPU_DOWN_PREPARE:
		remove_scalable_dir(cpu);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata debug_cpu_notifier = {
	.notifier_call = debug_cpu_callback,
};

void __init acpuclk_krait_debug_init(struct drv_data *drv_data)
{
	int cpu;
	drv = drv_data;

	base_dir = debugfs_create_dir("acpuclk", NULL);
	if (!base_dir)
		return;

	debugfs_create_file("speed_bin", S_IRUGO, base_dir, NULL,
							&speed_bin_fops);
	debugfs_create_file("pvs_bin", S_IRUGO, base_dir, NULL, &pvs_bin_fops);
	debugfs_create_file("boost_uv", S_IRUGO, base_dir, NULL, &boost_fops);

	for_each_online_cpu(cpu)
		add_scalable_dir(cpu);
	add_scalable_dir(L2);

	register_hotcpu_notifier(&debug_cpu_notifier);
}
