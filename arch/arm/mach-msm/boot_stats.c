/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/msm_iomap.h>

#include "boot_stats.h"

#define MSM_BOOT_STATS_IMEM_START	(MSM_IMEM_BASE+0x6b0)

static void __iomem *mpm_counter_base;
static uint32_t mpm_counter_freq;
static struct boot_stats *boot_stats =
				(void __iomem *)(MSM_BOOT_STATS_IMEM_START);

static const struct of_device_id mpm_counter_of_match[]	= {
	{ .compatible	= "qcom,mpm2-sleep-counter",	},
	{},
};

static int mpm_parse_dt(void)
{
	struct device_node *np;
	u32 freq;

	np = of_find_matching_node(NULL, mpm_counter_of_match);
	if (!np) {
		pr_err("mpm_counter: can't find DT node\n");
		return -ENODEV;
	}

	if (!of_property_read_u32(np, "clock-frequency", &freq))
		mpm_counter_freq = freq;
	else
		return -ENODEV;

	if (of_get_address(np, 0, NULL, NULL)) {
		mpm_counter_base = of_iomap(np, 0);
		if (!mpm_counter_base) {
			pr_err("mpm_counter: cant map counter base\n");
			return -ENODEV;
		}
	}

	return 0;
}

static void print_boot_stats(void)
{
	pr_info("KPI: Bootloader start count = %u\n",
			boot_stats->bootloader_start);
	pr_info("KPI: Bootloader end count = %u\n",
			boot_stats->bootloader_end);
	pr_info("KPI: Bootloader display count = %u\n",
			boot_stats->bootloader_display);
	pr_info("KPI: Bootloader load kernel count = %u\n",
			boot_stats->bootloader_load_kernel);
	pr_info("KPI: Kernel MPM timestamp = %u\n",
			__raw_readl(mpm_counter_base));
	pr_info("KPI: Kernel MPM Clock frequency = %u\n",
			mpm_counter_freq);
}

int boot_stats_init(void)
{
	int ret;

	if (!boot_stats)
		return -ENODEV;

	ret = mpm_parse_dt();
	if (ret < 0)
		return -ENODEV;

	print_boot_stats();

	return 0;
}

