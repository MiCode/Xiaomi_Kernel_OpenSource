/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/init.h>
#include <linux/io.h>
#include <mach/board.h>

#include "acpuclock.h"

/* Registers */
#define PLL1_CTL_ADDR		(MSM_CLK_CTL_BASE + 0x604)

unsigned long acpuclk_get_rate(int cpu)
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

void __init msm_acpu_clock_init(struct msm_acpu_clock_platform_data *clkdata)
{
	pr_info("ACPU running at %lu KHz\n", acpuclk_get_rate(0));
}
