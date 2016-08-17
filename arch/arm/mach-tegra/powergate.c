/*
 * arch/arm/mach-tegra/powergate.c
 *
 * Copyright (c) 2010 Google, Inc
 * Copyright (c) 2011 - 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <trace/events/power.h>
#include <asm/atomic.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/powergate.h>

#include "clock.h"
#include "fuse.h"
#include "powergate-priv.h"

static struct powergate_ops *pg_ops;

static spinlock_t *tegra_get_powergate_lock(void)
{
	if (pg_ops && pg_ops->get_powergate_lock)
		return pg_ops->get_powergate_lock();
	else
		WARN_ON_ONCE("This SOC does not export powergate lock");

	return NULL;
}

int tegra_powergate_set(int id, bool new_state)
{
#ifndef CONFIG_TEGRA_SIMULATION_PLATFORM
	bool status;
	unsigned long flags;
	spinlock_t *lock = tegra_get_powergate_lock();

	/* 10us timeout for toggle operation if it takes affect*/
	int toggle_timeout = 10;

	/* 100 * 10 = 1000us timeout for toggle command to take affect in case
	   of contention with h/w initiated CPU power gating */
	int contention_timeout = 100;

	spin_lock_irqsave(lock, flags);

	status = !!(pmc_read(PWRGATE_STATUS) & (1 << id));

	if (status == new_state) {
		spin_unlock_irqrestore(lock, flags);
		return 0;
	}

	if (TEGRA_IS_CPU_POWERGATE_ID(id)) {
		/* CPU ungated in s/w only during boot/resume with outer
		   waiting loop and no contention from other CPUs */
		pmc_write(PWRGATE_TOGGLE_START | id, PWRGATE_TOGGLE);
		spin_unlock_irqrestore(lock, flags);
		return 0;
	}

	pmc_write(PWRGATE_TOGGLE_START | id, PWRGATE_TOGGLE);
	do {
		do {
			udelay(1);
			status = !!(pmc_read(PWRGATE_STATUS) & (1 << id));

			toggle_timeout--;
		} while ((status != new_state) && (toggle_timeout > 0));

		contention_timeout--;
	} while ((status != new_state) && (contention_timeout > 0));

	spin_unlock_irqrestore(lock, flags);

	if (status != new_state) {
		WARN(1, "Could not set powergate %d to %d", id, new_state);
		return -EBUSY;
	}

	trace_power_domain_target(tegra_powergate_get_name(id), new_state,
			smp_processor_id());
#endif

	return 0;
}

int is_partition_clk_disabled(struct powergate_partition_info *pg_info)
{
	u32 idx;
	struct clk *clk;
	struct partition_clk_info *clk_info;
	int ret = 0;

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		clk_info = &pg_info->clk_info[idx];
		clk = clk_info->clk_ptr;

		if (!clk)
			break;

		if (clk_info->clk_type != RST_ONLY) {
			if (tegra_is_clk_enabled(clk)) {
				ret = -1;
				break;
			}
		}
	}

	return ret;
}

int powergate_module(int id)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains)
		return -EINVAL;

	tegra_powergate_mc_flush(id);

	return tegra_powergate_set(id, false);
}

int unpowergate_module(int id)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains)
		return -EINVAL;

	return tegra_powergate_set(id, true);
}

int partition_clk_enable(struct powergate_partition_info *pg_info)
{
	int ret;
	u32 idx;
	struct clk *clk;
	struct partition_clk_info *clk_info;

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		clk_info = &pg_info->clk_info[idx];
		clk = clk_info->clk_ptr;
		if (!clk)
			break;

		if (clk_info->clk_type != RST_ONLY) {
			ret = tegra_clk_prepare_enable(clk);
			if (ret)
				goto err_clk_en;
		}
	}

	return 0;

err_clk_en:
	WARN(1, "Could not enable clk %s, error %d", clk->name, ret);
	while (idx--) {
		clk_info = &pg_info->clk_info[idx];
		if (clk_info->clk_type != RST_ONLY)
			tegra_clk_disable_unprepare(clk_info->clk_ptr);
	}

	return ret;
}

void partition_clk_disable(struct powergate_partition_info *pg_info)
{
	u32 idx;
	struct clk *clk;
	struct partition_clk_info *clk_info;

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		clk_info = &pg_info->clk_info[idx];
		clk = clk_info->clk_ptr;

		if (!clk)
			break;

		if (clk_info->clk_type != RST_ONLY)
			tegra_clk_disable_unprepare(clk);
	}
}

void get_clk_info(struct powergate_partition_info *pg_info)
{
	int idx;

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		if (!pg_info->clk_info[idx].clk_name)
			break;

		pg_info->clk_info[idx].clk_ptr = tegra_get_clock_by_name(
			pg_info->clk_info[idx].clk_name);
	}
}

void powergate_partition_assert_reset(struct powergate_partition_info *pg_info)
{
	u32 idx;
	struct clk *clk_ptr;
	struct partition_clk_info *clk_info;

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		clk_info = &pg_info->clk_info[idx];
		clk_ptr = clk_info->clk_ptr;

		if (!clk_ptr)
			break;

		if (clk_info->clk_type != CLK_ONLY)
			tegra_periph_reset_assert(clk_ptr);
	}
}

void powergate_partition_deassert_reset(struct powergate_partition_info *pg_info)
{
	u32 idx;
	struct clk *clk_ptr;
	struct partition_clk_info *clk_info;

	for (idx = 0; idx < MAX_CLK_EN_NUM; idx++) {
		clk_info = &pg_info->clk_info[idx];
		clk_ptr = clk_info->clk_ptr;

		if (!clk_ptr)
			break;

		if (clk_info->clk_type != CLK_ONLY)
			tegra_periph_reset_deassert(clk_ptr);
	}
}

int tegra_powergate_reset_module(struct powergate_partition_info *pg_info)
{
	int ret;

	powergate_partition_assert_reset(pg_info);

	udelay(10);

	ret = partition_clk_enable(pg_info);
	if (ret)
		return ret;

	udelay(10);

	powergate_partition_deassert_reset(pg_info);

	partition_clk_disable(pg_info);

	return 0;
}

bool tegra_powergate_check_clamping(int id)
{
	if (!pg_ops || !pg_ops->powergate_check_clamping) {
		pr_info("This SOC can't check clamping status\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains)
		return -EINVAL;

	return pg_ops->powergate_check_clamping(id);
}

int tegra_powergate_remove_clamping(int id)
{
	u32 mask;
	int contention_timeout = 100;

	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains)
		return -EINVAL;

	/*
	 * PCIE and VDE clamping masks are swapped with respect to their
	 * partition ids
	 */
	if (id ==  TEGRA_POWERGATE_VDEC)
		mask = (1 << TEGRA_POWERGATE_PCIE);
	else if (id == TEGRA_POWERGATE_PCIE)
		mask = (1 << TEGRA_POWERGATE_VDEC);
	else
		mask = (1 << id);

	pmc_write(mask, REMOVE_CLAMPING);
	/* Wait until clamp is removed */
	do {
		udelay(1);
		contention_timeout--;
	} while ((contention_timeout > 0)
			&& (pmc_read(REMOVE_CLAMPING) & mask));

	WARN(contention_timeout <= 0, "Couldn't remove clamping");

	return 0;
}

/* EXTERNALY VISIBLE APIS */

bool tegra_powergate_is_powered(int id)
{
	u32 status;

	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains)
		return -EINVAL;

	status = pmc_read(PWRGATE_STATUS) & (1 << id);

	return !!status;
}
EXPORT_SYMBOL(tegra_powergate_is_powered);

int tegra_cpu_powergate_id(int cpuid)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (cpuid < 0 || cpuid >= pg_ops->num_cpu_domains) {
		pr_info("%s: invalid powergate id\n", __func__);
		return -EINVAL;
	}

	if (pg_ops->cpu_domains)
		return pg_ops->cpu_domains[cpuid];
	else
		WARN_ON_ONCE("This SOC does not support CPU powergate\n");

	return -EINVAL;
}
EXPORT_SYMBOL(tegra_cpu_powergate_id);

int tegra_powergate_partition(int id)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains) {
		pr_info("%s: invalid powergate id\n", __func__);
		return -EINVAL;
	}

	if (pg_ops->powergate_partition)
		return pg_ops->powergate_partition(id);
	else
		WARN_ON_ONCE("This SOC doesn't support powergating");

	return -EINVAL;
}
EXPORT_SYMBOL(tegra_powergate_partition);

int tegra_unpowergate_partition(int id)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains) {
		pr_info("%s: invalid powergate id\n", __func__);
		return -EINVAL;
	}

	if (pg_ops->unpowergate_partition)
		return pg_ops->unpowergate_partition(id);
	else
		WARN_ON_ONCE("This SOC doesn't support un-powergating");

	return -EINVAL;
}
EXPORT_SYMBOL(tegra_unpowergate_partition);

int tegra_powergate_partition_with_clk_off(int id)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains) {
		pr_info("%s: invalid powergate id\n", __func__);
		return -EINVAL;
	}

	if (pg_ops->powergate_partition_with_clk_off)
		return pg_ops->powergate_partition_with_clk_off(id);
	else
		WARN_ON_ONCE("This SOC doesn't support powergating with clk off");

	return -EINVAL;
}
EXPORT_SYMBOL(tegra_powergate_partition_with_clk_off);

int tegra_unpowergate_partition_with_clk_on(int id)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains) {
		pr_info("%s: invalid powergate id\n", __func__);
		return -EINVAL;
	}

	if (pg_ops->unpowergate_partition_with_clk_on)
		return pg_ops->unpowergate_partition_with_clk_on(id);
	else
		WARN_ON_ONCE("This SOC doesn't support power un-gating with clk on");

	return -EINVAL;
}
EXPORT_SYMBOL(tegra_unpowergate_partition_with_clk_on);

int tegra_powergate_mc_enable(int id)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains) {
		pr_info("%s: invalid powergate id\n", __func__);
		return -EINVAL;
	}

	if (pg_ops->powergate_mc_enable)
		return pg_ops->powergate_mc_enable(id);
	else
		WARN_ON_ONCE("This SOC does not support powergate mc enable");

	return -EINVAL;
}
EXPORT_SYMBOL(tegra_powergate_mc_enable);

int tegra_powergate_mc_disable(int id)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains) {
		pr_info("%s: invalid powergate id\n", __func__);
		return -EINVAL;
	}

	if (pg_ops->powergate_mc_disable)
		return pg_ops->powergate_mc_disable(id);
	else
		WARN_ON_ONCE("This SOC does not support powergate mc disable");

	return -EINVAL;
}
EXPORT_SYMBOL(tegra_powergate_mc_disable);

int tegra_powergate_mc_flush(int id)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains) {
		pr_info("%s: invalid powergate id\n", __func__);
		return -EINVAL;
	}

	if (pg_ops->powergate_mc_flush)
		return pg_ops->powergate_mc_flush(id);
	else
		WARN_ON_ONCE("This SOC does not support powergate mc flush");

	return -EINVAL;
}
EXPORT_SYMBOL(tegra_powergate_mc_flush);

int tegra_powergate_mc_flush_done(int id)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains) {
		pr_info("%s: invalid powergate id\n", __func__);
		return -EINVAL;
	}

	if (pg_ops->powergate_mc_flush_done)
		return pg_ops->powergate_mc_flush_done(id);
	else
		WARN_ON_ONCE("This SOC does not support powergate mc flush done");

	return -EINVAL;
}
EXPORT_SYMBOL(tegra_powergate_mc_flush_done);

const char *tegra_powergate_get_name(int id)
{
	if (!pg_ops) {
		pr_info("This SOC doesn't support powergating\n");
		return NULL;
	}

	if (id < 0 || id >= pg_ops->num_powerdomains) {
		pr_info("invalid powergate id\n");
		return "invalid";
	}

	if (pg_ops->get_powergate_domain_name)
		return pg_ops->get_powergate_domain_name(id);
	else
		WARN_ON_ONCE("This SOC does not support CPU powergate");

	return "invalid";
}
EXPORT_SYMBOL(tegra_powergate_get_name);

int tegra_powergate_init_refcount(void)
{
	if (!pg_ops->powergate_init_refcount)
		return 0;

	return pg_ops->powergate_init_refcount();
}

int __init tegra_powergate_init(void)
{
	switch (tegra_chip_id) {
		case TEGRA20:
			pg_ops = tegra2_powergate_init_chip_support();
			break;

		case TEGRA30:
			pg_ops = tegra3_powergate_init_chip_support();
			break;

		case TEGRA11X:
			pg_ops = tegra11x_powergate_init_chip_support();
			break;

		default:
			pg_ops = NULL;
			pr_info("%s: Unknown Tegra variant. Disabling powergate\n", __func__);
			break;
	}

	tegra_powergate_init_refcount();

	pr_info("%s: DONE\n", __func__);

	return (pg_ops ? 0 : -EINVAL);
}

#ifdef CONFIG_DEBUG_FS

static int powergate_show(struct seq_file *s, void *data)
{
	int i;
	const char *name;

	if (!pg_ops) {
		seq_printf(s, "This SOC doesn't support powergating\n");
		return -EINVAL;
	}

	seq_printf(s, " powergate powered\n");
	seq_printf(s, "------------------\n");

	for (i = 0; i < pg_ops->num_powerdomains; i++) {
		name = tegra_powergate_get_name(i);
		if (name)
			seq_printf(s, " %9s %7s\n", name,
				tegra_powergate_is_powered(i) ? "yes" : "no");
	}

	return 0;
}

static int powergate_open(struct inode *inode, struct file *file)
{
	return single_open(file, powergate_show, inode->i_private);
}

static const struct file_operations powergate_fops = {
	.open		= powergate_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init powergate_debugfs_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("powergate", S_IRUGO, NULL, NULL,
		&powergate_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}

late_initcall(powergate_debugfs_init);

#endif
