/*
 * arch/arm/mach-tegra/latency_allowance.c
 *
 * Copyright (C) 2011-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/moduleparam.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/stringify.h>
#include <linux/clk.h>
#include <linux/clk/tegra.h>
#include <linux/syscore_ops.h>
#include <linux/tegra-soc.h>
#include <asm/bug.h>
#include <asm/io.h>
#include <asm/string.h>
#include <mach/latency_allowance.h>
#include "la_priv.h"

#define TEST_LA_CODE		0

/* Bug 995270 */
#define HACK_LA_FIFO 1
static int default_set_la(enum tegra_la_id id, unsigned int bw_mbps);

static struct la_chip_specific cs;
module_param_named(disable_la, cs.disable_la, bool, S_IRUGO | S_IWUSR);
module_param_named(disable_ptsa, cs.disable_ptsa, bool, S_IRUGO | S_IWUSR);
module_param_named(disable_disp_ptsa,
	cs.disable_disp_ptsa, bool, S_IRUGO | S_IWUSR);
module_param_named(disable_bbc_ptsa,
	cs.disable_bbc_ptsa, bool, S_IRUGO | S_IWUSR);

/* FIXME!!:- This function needs to be implemented properly. */
unsigned int tegra_get_dvfs_time_nsec(unsigned long emc_freq_mhz)
{
	if ((emc_freq_mhz >= 100) && (emc_freq_mhz <= 120))
		return 6679;
	else if ((emc_freq_mhz >= 200) && (emc_freq_mhz <= 220))
		return 3954;
	else if ((emc_freq_mhz >= 400) && (emc_freq_mhz <= 420))
		return 2396;
	else if ((emc_freq_mhz >= 780) && (emc_freq_mhz <= 800))
		return 1583;
	else if ((emc_freq_mhz >= 920) && (emc_freq_mhz <= 940))
		return 1452;
	else
		return 3000;
}

static void init_chip_specific(void)
{
	enum tegra_chipid cid;

	if (!tegra_platform_is_silicon())
		return;

	cs.set_la = default_set_la;
	memset(&cs.id_to_index[0], 0xFF, sizeof(cs.id_to_index));
	spin_lock_init(&cs.lock);

	cid = tegra_get_chipid();

	switch (cid) {
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	case TEGRA_CHIPID_TEGRA3:
		tegra_la_get_t3_specific(&cs);
		break;
#endif
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	case TEGRA_CHIPID_TEGRA11:
		tegra_la_get_t11x_specific(&cs);
		break;
#endif
#if defined(CONFIG_ARCH_TEGRA_14x_SOC)
	case TEGRA_CHIPID_TEGRA14:
		tegra_la_get_t14x_specific(&cs);
		break;
#endif
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	case TEGRA_CHIPID_TEGRA12:
		tegra_la_get_t12x_specific(&cs);
		break;
#endif
	default:
		cs.set_la = NULL;
	}
}

struct la_to_dc_params tegra_get_la_to_dc_params(void)
{
	return cs.la_params;
}

static void set_la(struct la_client_info *ci, int la)
{
	unsigned long reg_read;
	unsigned long reg_write;
	int idx = cs.id_to_index[ci->id];

	spin_lock(&cs.lock);
	reg_read = readl(ci->reg_addr);
	reg_write = (reg_read & ~ci->mask) |
			(la << ci->shift);
	writel(reg_write, ci->reg_addr);
	cs.scaling_info[idx].la_set = la;
	ci->la_set = la;
	la_debug("reg_addr=0x%x, read=0x%x, write=0x%x",
		(u32)ci->reg_addr, (u32)reg_read, (u32)reg_write);
	spin_unlock(&cs.lock);
}

static int default_set_la(enum tegra_la_id id, unsigned int bw_mbps)
{
	int ideal_la;
	int la_to_set;
	unsigned int fifo_size_in_atoms;
	int bytes_per_atom = cs.atom_size;
	const int fifo_scale = 4;		/* 25% of the FIFO */
	struct la_client_info *ci;
	int idx = cs.id_to_index[id];

	if (!tegra_platform_is_silicon())
		return 0;

	VALIDATE_ID(id, &cs);
	VALIDATE_BW(bw_mbps);

	ci = &cs.la_info_array[idx];
	fifo_size_in_atoms = ci->fifo_size_in_atoms;

#ifdef CONFIG_TEGRA_MC_PTSA
	if (id >= TEGRA_LA_DISPLAY_0A && id <= TEGRA_LA_DISPLAY_HCB) {
		cs.disp_bw_array[id - TEGRA_LA_DISPLAY_0A] = bw_mbps;
		if (cs.update_display_ptsa_rate)
			cs.update_display_ptsa_rate(cs.disp_bw_array);
	}
#endif
#if HACK_LA_FIFO
	/* pretend that our FIFO is only as deep as the lowest fullness
	 * we expect to see */
	if (id >= ID(DISPLAY_0A) && id <= ID(DISPLAY_HCB))
		fifo_size_in_atoms /= fifo_scale;
#endif

	if (bw_mbps == 0) {
		la_to_set = cs.la_max_value;
	} else {
		ideal_la = (fifo_size_in_atoms * bytes_per_atom * 1000) /
			   (bw_mbps * cs.ns_per_tick);
		la_to_set = ideal_la -
			    (ci->expiration_in_ns / cs.ns_per_tick) - 1;
	}

	la_debug("\n%s:id=%d,idx=%d, bw=%dmbps, la_to_set=%d",
		__func__, id, idx, bw_mbps, la_to_set);
	la_to_set = (la_to_set < 0) ? 0 : la_to_set;
	cs.scaling_info[idx].actual_la_to_set = la_to_set;
	la_to_set = (la_to_set > cs.la_max_value) ? cs.la_max_value : la_to_set;

	set_la(ci, la_to_set);
	return 0;
}

int tegra_set_disp_latency_allowance(enum tegra_la_id id,
					unsigned int bw_mbps,
					struct dc_to_la_params disp_params) {
	if (cs.set_disp_la)
		return cs.set_disp_la(id, bw_mbps, disp_params);
	else if (cs.set_la)
		return cs.set_la(id, bw_mbps);
	return 0;
}

/* Sets latency allowance based on clients memory bandwitdh requirement.
 * Bandwidth passed is in mega bytes per second.
 */
int tegra_set_latency_allowance(enum tegra_la_id id, unsigned int bw_mbps)
{
	if (cs.set_la)
		return cs.set_la(id, bw_mbps);
	return 0;
}

int tegra_set_camera_ptsa(enum tegra_la_id id,
			unsigned int bw_mbps,
			int is_hiso)
{
	if (cs.update_camera_ptsa_rate)
		return cs.update_camera_ptsa_rate(id, bw_mbps, is_hiso);
	else if (cs.set_la)
		return cs.set_la(id, bw_mbps);
	return 0;
}

/* Thresholds for scaling are specified in % of fifo freeness.
 * If threshold_low is specified as 20%, it means when the fifo free
 * between 0 to 20%, use la as programmed_la.
 * If threshold_mid is specified as 50%, it means when the fifo free
 * between 20 to 50%, use la as programmed_la/2 .
 * If threshold_high is specified as 80%, it means when the fifo free
 * between 50 to 80%, use la as programmed_la/4.
 * When the fifo is free between 80 to 100%, use la as 0(highest priority).
 */
int tegra_enable_latency_scaling(enum tegra_la_id id,
				    unsigned int threshold_low,
				    unsigned int threshold_mid,
				    unsigned int threshold_high)
{
	if (cs.enable_la_scaling)
		return cs.enable_la_scaling(id, threshold_low,
			threshold_mid, threshold_high);
	return 0;
}

void tegra_disable_latency_scaling(enum tegra_la_id id)
{
	if (cs.disable_la_scaling) {
		cs.disable_la_scaling(id);
	}
}

void tegra_latency_allowance_update_tick_length(unsigned int new_ns_per_tick)
{
	int i = 0;
	int la;
	unsigned long reg_read;
	unsigned long reg_write;
	unsigned long scale_factor = new_ns_per_tick / cs.ns_per_tick;

	if (scale_factor > 1) {
		spin_lock(&cs.lock);
		cs.ns_per_tick = new_ns_per_tick;
		for (i = 0; i < cs.la_info_array_size - 1; i++) {
			reg_read = readl(cs.la_info_array[i].reg_addr);
			la = ((reg_read & cs.la_info_array[i].mask) >>
				cs.la_info_array[i].shift) / scale_factor;

			reg_write = (reg_read & ~cs.la_info_array[i].mask) |
					(la << cs.la_info_array[i].shift);
			writel(reg_write, cs.la_info_array[i].reg_addr);
			cs.scaling_info[i].la_set = la;
		}
		spin_unlock(&cs.lock);

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
		/* Re-scale G2PR, G2SR, G2DR, G2DW with updated ns_per_tick */
		tegra_set_latency_allowance(TEGRA_LA_G2PR, 20);
		tegra_set_latency_allowance(TEGRA_LA_G2SR, 20);
		tegra_set_latency_allowance(TEGRA_LA_G2DR, 20);
		tegra_set_latency_allowance(TEGRA_LA_G2DW, 20);
#endif
	}
}

static int la_regs_show(struct seq_file *s, void *unused)
{
	int i;
	unsigned long la;

	/* iterate the list, but don't print MAX_ID */
	for (i = 0; i < cs.la_info_array_size - 1; i++) {
		la = (readl(cs.la_info_array[i].reg_addr) &
			cs.la_info_array[i].mask) >> cs.la_info_array[i].shift;
		seq_printf(s, "%-16s: %4lu\n", cs.la_info_array[i].name, la);
	}

	return 0;
}

static int dbg_la_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, la_regs_show, inode->i_private);
}

static const struct file_operations regs_fops = {
	.open           = dbg_la_regs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int __init tegra_latency_allowance_debugfs_init(void)
{
	if (cs.latency_debug_dir)
		return 0;

	cs.latency_debug_dir = debugfs_create_dir("tegra_latency", NULL);

	debugfs_create_file("la_info", S_IRUGO, cs.latency_debug_dir, NULL,
		&regs_fops);

	return 0;
}

static int tegra_la_suspend(void)
{
	if (cs.suspend)
		return cs.suspend();
	return 0;
}

static void tegra_la_resume(void)
{
	int i;

	if (cs.resume) {
		cs.resume();
		return;
	}
	for (i = 0; i < cs.la_info_array_size; i++) {
		if (cs.la_info_array[i].la_set)
			set_la(&cs.la_info_array[i],
				cs.la_info_array[i].la_set);
	}
	if (cs.init_ptsa)
		cs.init_ptsa();
}

static struct syscore_ops tegra_la_syscore_ops = {
	.suspend = tegra_la_suspend,
	.resume = tegra_la_resume,
};

static __init int tegra_la_syscore_init(void)
{
	register_syscore_ops(&tegra_la_syscore_ops);
	return 0;
}

static int __init tegra_latency_allowance_init(void)
{
	unsigned int i;
	enum tegra_chipid cid = tegra_get_chipid();

	init_chip_specific();

	for (i = 0; i < cs.la_info_array_size; i++)
		cs.id_to_index[cs.la_info_array[i].id] = i;

	for (i = 0; i < cs.la_info_array_size; i++) {
		if (cs.la_info_array[i].init_la) {
			unsigned int la_to_set = 0;

			if ((cid == TEGRA_CHIPID_TEGRA12) &&
				(cs.la_info_array[i].id == ID(MSENCSRD))) {
				/* This is a special case. */
				struct clk *emc_clk = clk_get(NULL, "emc");
				unsigned long emc_freq_mhz =
							clk_get_rate(emc_clk) /
							1000000;
				unsigned int val_1 = 53;
				unsigned int val_2 = 24;

				if (210 > emc_freq_mhz)
					val_1 = val_1 * 210 / emc_freq_mhz;

				if (574 > emc_freq_mhz)
					val_2 = val_2 * 574 / emc_freq_mhz;

				la_to_set = min3((unsigned int)
							T12X_MC_LA_MAX_VALUE,
						val_1,
						val_2);
			} else if (cs.la_info_array[i].la_ref_clk_mhz != 0) {
				/* In this case we need to scale LA with emc
				   frequency. */
				struct clk *emc_clk = clk_get(NULL, "emc");
				unsigned long emc_freq_mhz =
							clk_get_rate(emc_clk) /
							1000000;

				if (cs.la_info_array[i].la_ref_clk_mhz <=
					emc_freq_mhz) {
					la_to_set =
						min((unsigned long)
						cs.la_info_array[i].init_la,
						(unsigned long)
							T12X_MC_LA_MAX_VALUE);
				} else {
					la_to_set =
					min(cs.la_info_array[i].init_la *
					cs.la_info_array[i].la_ref_clk_mhz /
					emc_freq_mhz,
					(unsigned long)T12X_MC_LA_MAX_VALUE);
				}
			} else
				la_to_set = cs.la_info_array[i].init_la;

			set_la(&cs.la_info_array[i],
				la_to_set);
		}
	}
#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
	tegra_set_latency_allowance(TEGRA_LA_G2PR, 20);
	tegra_set_latency_allowance(TEGRA_LA_G2SR, 20);
	tegra_set_latency_allowance(TEGRA_LA_G2DR, 20);
	tegra_set_latency_allowance(TEGRA_LA_G2DW, 20);
#endif

	if (cs.init_ptsa)
		cs.init_ptsa();
	return 0;
}

late_initcall(tegra_latency_allowance_debugfs_init);
subsys_initcall(tegra_la_syscore_init);
core_initcall(tegra_latency_allowance_init);

#if TEST_LA_CODE
#define PRINT_ID_IDX_MAPPING 0
static int __init test_la(void)
{
	int i;
	int err;
	enum tegra_la_id id = 0;
	int repeat_count = 5;

#if PRINT_ID_IDX_MAPPING
	for (i = 0; i < ID(MAX_ID); i++)
		pr_info("ID=0x%x, Idx=0x%x", i, cs.id_to_index[i]);
#endif

	do {
		for (id = 0; id < TEGRA_LA_MAX_ID; id++) {
			err = tegra_set_latency_allowance(id, 200);
			if (err)
				la_debug("\n***tegra_set_latency_allowance,"
					" err=%d", err);
		}

		for (id = 0; id < TEGRA_LA_MAX_ID; id++) {
			if (id >= ID(DISPLAY_0AB) && id <= ID(DISPLAY_HCB))
				continue;
			if (id >= ID(VI_WSB) && id <= ID(VI_WY))
				continue;
			err = tegra_enable_latency_scaling(id, 20, 50, 80);
			if (err)
				la_debug("\n***tegra_enable_latency_scaling,"
					" err=%d", err);
		}

		la_debug("la_scaling_enable_count =%d",
			cs.la_scaling_enable_count);
		for (id = 0; id < TEGRA_LA_MAX_ID; id++) {
			if (id >= ID(DISPLAY_0AB) && id <= ID(DISPLAY_HCB))
				continue;
			if (id >= ID(VI_WSB) && id <= ID(VI_WY))
				continue;
			tegra_disable_latency_scaling(id);
		}
		la_debug("la_scaling_enable_count=%d",
			cs.la_scaling_enable_count);
	} while (--repeat_count);
	return 0;
}

late_initcall(test_la);
#endif
