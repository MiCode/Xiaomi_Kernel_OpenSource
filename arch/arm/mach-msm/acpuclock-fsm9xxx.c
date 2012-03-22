/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <mach/board.h>

#include "acpuclock.h"

/* Registers */
#define PLL1_CTL_ADDR		(MSM_CLK_CTL_BASE + 0x604)

static unsigned long acpuclk_9xxx_get_rate(int cpu)
{
	unsigned int pll1_ctl;
	unsigned int pll1_l, pll1_div2;
	unsigned int pll1_khz;

	pll1_ctl = readl_relaxed(PLL1_CTL_ADDR);
	pll1_l = ((pll1_ctl >> 3) & 0x3f) * 2;
	pll1_div2 = pll1_ctl & 0x20000;
	pll1_khz = 19200 * pll1_l;
	if (pll1_div2)
		pll1_khz >>= 1;

	return pll1_khz;
}

static struct acpuclk_data acpuclk_9xxx_data = {
	.get_rate = acpuclk_9xxx_get_rate,
};

static int __init acpuclk_9xxx_probe(struct platform_device *pdev)
{
	acpuclk_register(&acpuclk_9xxx_data);
	pr_info("ACPU running at %lu KHz\n", acpuclk_get_rate(0));
	return 0;
}

static struct platform_driver acpuclk_9xxx_driver = {
	.driver = {
		.name = "acpuclk-9xxx",
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_9xxx_init(void)
{
	return platform_driver_probe(&acpuclk_9xxx_driver, acpuclk_9xxx_probe);
}
device_initcall(acpuclk_9xxx_init);
