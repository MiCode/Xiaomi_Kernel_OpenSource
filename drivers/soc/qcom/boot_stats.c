/* Copyright (c) 2013-2014,2019 The Linux Foundation. All rights reserved.
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
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <soc/qcom/boot_stats.h>

static void __iomem *mpm_counter_base;
static phys_addr_t mpm_counter_pa;
static uint32_t mpm_counter_freq;
struct boot_stats __iomem *boot_stats;

static int mpm_parse_dt(void)
{
	struct device_node *np;
	u32 freq;

	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem-boot_stats");
	if (!np) {
		pr_err("can't find qcom,msm-imem node\n");
		return -ENODEV;
	}
	boot_stats = of_iomap(np, 0);
	if (!boot_stats) {
		pr_err("boot_stats: Can't map imem\n");
		return -ENODEV;
	}

	np = of_find_compatible_node(NULL, NULL, "qcom,mpm2-sleep-counter");
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
			readl_relaxed(&boot_stats->bootloader_start));
	pr_info("KPI: Bootloader end count = %u\n",
			readl_relaxed(&boot_stats->bootloader_end));
	pr_info("KPI: Bootloader display count = %u\n",
			readl_relaxed(&boot_stats->bootloader_display));
	pr_info("KPI: Bootloader load kernel count = %u\n",
			readl_relaxed(&boot_stats->bootloader_load_kernel));
	pr_info("KPI: Kernel MPM timestamp = %u\n",
			readl_relaxed(mpm_counter_base));
	pr_info("KPI: Kernel MPM Clock frequency = %u\n",
			mpm_counter_freq);
}

unsigned long long int msm_timer_get_sclk_ticks(void)
{
	unsigned long long int t1, t2;
	int loop_count = 10;
	int loop_zero_count = 3;
	u64 tmp = USEC_PER_SEC;
	void __iomem *sclk_tick;

	do_div(tmp, TIMER_KHZ);
	tmp /= (loop_zero_count-1);
	sclk_tick = mpm_counter_base;
	if (!sclk_tick)
		return -EINVAL;
	while (loop_zero_count--) {
		t1 = __raw_readl_no_log(sclk_tick);
		do {
			udelay(1);
			t2 = t1;
			t1 = __raw_readl_no_log(sclk_tick);
		} while ((t2 != t1) && --loop_count);
		if (!loop_count) {
			pr_err("boot_stats: SCLK  did not stabilize\n");
			return 0;
		}
		if (t1)
			break;

		udelay(tmp);
	}
	if (!loop_zero_count) {
		pr_err("boot_stats: SCLK reads zero\n");
		return 0;
	}
	return t1;
}

phys_addr_t msm_timer_get_pa(void)
{
	return mpm_counter_pa;
}

int boot_stats_init(void)
{
	int ret;

	ret = mpm_parse_dt();
	if (ret < 0)
		return -ENODEV;

	print_boot_stats();

	if (!(boot_marker_enabled()))
		boot_stats_exit();

	return 0;
}

int boot_stats_exit(void)
{
	iounmap(boot_stats);
	iounmap(mpm_counter_base);
	return 0;
}
