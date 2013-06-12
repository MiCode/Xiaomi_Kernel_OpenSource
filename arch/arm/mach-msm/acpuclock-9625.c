/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/clk-provider.h>
#include <mach/rpm-regulator-smd.h>

#include "acpuclock-cortex.h"

#define RCG_CONFIG_PGM_DATA_BIT		BIT(11)
#define RCG_CONFIG_PGM_ENA_BIT		BIT(10)
#define GPLL0_TO_A5_ALWAYS_ENABLE	BIT(18)

static struct msm_bus_paths bw_level_tbl[] = {
	[0] =  BW_MBPS(152), /* At least 19 MHz on bus. */
	[1] =  BW_MBPS(264), /* At least 33 MHz on bus. */
	[2] =  BW_MBPS(528), /* At least 66 MHz on bus. */
	[3] =  BW_MBPS(664), /* At least 83 MHz on bus. */
	[4] = BW_MBPS(1064), /* At least 133 MHz on bus. */
	[5] = BW_MBPS(1328), /* At least 166 MHz on bus. */
	[6] = BW_MBPS(2128), /* At least 266 MHz on bus. */
	[7] = BW_MBPS(2664), /* At least 333 MHz on bus. */
};

static struct msm_bus_scale_pdata bus_client_pdata = {
	.usecase = bw_level_tbl,
	.num_usecases = ARRAY_SIZE(bw_level_tbl),
	.active_only = 1,
	.name = "acpuclock",
};

/* TODO:
 * 1) Update MX voltage when they are avaiable
 * 2) Update bus bandwidth
 */
static struct clkctl_acpu_speed acpu_freq_tbl[] = {
	{ 0,  19200, CXO,     0, 0,   LVL_LOW,    950000, 0 },
	{ 1, 300000, PLL0,    1, 2,   LVL_LOW,    950000, 4 },
	{ 1, 600000, PLL0,    1, 0,   LVL_NOM,    950000, 4 },
	{ 1, 748800, ACPUPLL, 5, 0,   LVL_HIGH,  1050000, 7 },
	{ 1, 998400, ACPUPLL, 5, 0,   LVL_HIGH,  1050000, 7 },
	{ 0 }
};

static struct acpuclk_drv_data drv_data = {
	.freq_tbl = acpu_freq_tbl,
	.bus_scale = &bus_client_pdata,
	.vdd_max_cpu = LVL_HIGH,
	.vdd_max_mem = 1050000,
	.src_clocks = {
		[PLL0].name = "pll0",
		[ACPUPLL].name = "pll14",
	},
	.reg_data = {
		.cfg_src_mask = BM(2, 0),
		.cfg_src_shift = 0,
		.cfg_div_mask = BM(7, 3),
		.cfg_div_shift = 3,
		.update_mask = RCG_CONFIG_PGM_DATA_BIT | RCG_CONFIG_PGM_ENA_BIT,
		.poll_mask = RCG_CONFIG_PGM_DATA_BIT,
	},
	.power_collapse_khz = 300000,
	.wait_for_irq_khz = 300000,
};

static int __init acpuclk_9625_probe(struct platform_device *pdev)
{
	struct resource *res;
	u32 regval, i;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rcg_base");
	if (!res)
		return -EINVAL;

	drv_data.apcs_rcg_config = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));
	if (!drv_data.apcs_rcg_config)
		return -ENOMEM;

	drv_data.apcs_rcg_cmd = drv_data.apcs_rcg_config;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pwr_base");
	if (!res)
		return -EINVAL;

	drv_data.apcs_cpu_pwr_ctl = ioremap(res->start, resource_size(res));
	if (!drv_data.apcs_cpu_pwr_ctl)
		return -ENOMEM;

	drv_data.vdd_cpu = devm_regulator_get(&pdev->dev, "a5_cpu");
	if (IS_ERR(drv_data.vdd_cpu)) {
		dev_err(&pdev->dev, "regulator for %s get failed\n", "a5_cpu");
		return PTR_ERR(drv_data.vdd_cpu);
	}

	drv_data.vdd_mem = devm_regulator_get(&pdev->dev, "a5_mem");
	if (IS_ERR(drv_data.vdd_mem)) {
		dev_err(&pdev->dev, "regulator for %s get failed\n", "a5_mem");
		return PTR_ERR(drv_data.vdd_mem);
	}

	for (i = 0; i < NUM_SRC; i++) {
		if (!drv_data.src_clocks[i].name)
			continue;
		drv_data.src_clocks[i].clk =
			devm_clk_get(&pdev->dev, drv_data.src_clocks[i].name);
		if (IS_ERR(drv_data.src_clocks[i].clk)) {
			dev_err(&pdev->dev, "Unable to get clock %s\n",
				drv_data.src_clocks[i].name);
			return -EPROBE_DEFER;
		}
	}

	/* Disable hardware gating of gpll0 to A5SS */
	regval = readl_relaxed(drv_data.apcs_cpu_pwr_ctl);
	regval |= GPLL0_TO_A5_ALWAYS_ENABLE;
	writel_relaxed(regval, drv_data.apcs_cpu_pwr_ctl);

	/* Enable the always on source */
	clk_prepare_enable(drv_data.src_clocks[PLL0].clk);

	return acpuclk_cortex_init(pdev, &drv_data);
}

static struct of_device_id acpuclk_9625_match_table[] = {
	{.compatible = "qcom,acpuclk-9625"},
	{}
};

static struct platform_driver acpuclk_9625_driver = {
	.driver = {
		.name = "acpuclk-9625",
		.of_match_table = acpuclk_9625_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_9625_init(void)
{
	return platform_driver_probe(&acpuclk_9625_driver, acpuclk_9625_probe);
}
device_initcall(acpuclk_9625_init);
