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
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/smp.h>
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/msm_mpmctr.h>

static void __iomem *mpm_timer_base;

uint32_t msm_mpm_get_count(void)
{
	uint32_t count;
	if (!mpm_timer_base)
		return 0;

	count = __raw_readl_no_log(mpm_timer_base);
	pr_debug("mpm sclk sync:(%u)", count);
	return count;
}
EXPORT_SYMBOL(msm_mpm_get_count);

static inline void msm_mpmctr_show_count(void)
{
	unsigned long long t;
	unsigned long nsec_rem;

	t = sched_clock();

	nsec_rem = do_div(t, 1000000000)/1000;

	printk(KERN_INFO "mpm_counter: [%5lu.%06lu]:(%u)\n",
		   (unsigned long)t, nsec_rem,
		   msm_mpm_get_count());

}

static struct of_device_id msm_mpmctr_of_match[] = {
	{.compatible = "qcom,mpm2-sleep-counter"},
	{}
};

static struct platform_driver msm_mpmctr_driver = {
	.driver         = {
		.name = "msm_mpctr",
		.owner = THIS_MODULE,
		.of_match_table = msm_mpmctr_of_match,
	},
};

static int __init mpmctr_set_register(struct device_node *np)
{
	if (of_get_address(np, 0, NULL, NULL)) {
		mpm_timer_base = of_iomap(np, 0);
		if (!mpm_timer_base) {
			pr_err("%s: cannot map timer base\n", __func__);
			return -ENOMEM;
		}
	}
	return 0;
}

static int __init msm_mpmctr_probe(struct platform_device *pdev)
{
	if (!pdev->dev.of_node)
		return -ENODEV;

	if (mpmctr_set_register(pdev->dev.of_node))
		return -ENODEV;

	msm_mpmctr_show_count();

	return 0;
}

static int __init mpmctr_init(void)
{
	return platform_driver_probe(&msm_mpmctr_driver, msm_mpmctr_probe);
}

module_init(mpmctr_init)
