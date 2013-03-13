/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <mach/clk-provider.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/rpm-regulator-smd.h>

#include "acpuclock-cortex.h"

#define RCG_CONFIG_UPDATE_BIT		BIT(0)

static struct msm_bus_paths bw_level_tbl[] = {
	[0] =  BW_MBPS(152), /* At least 19 MHz on bus. */
	[1] =  BW_MBPS(300), /* At least 37.5 MHz on bus. */
	[2] =  BW_MBPS(400), /* At least 50 MHz on bus. */
	[3] =  BW_MBPS(800), /* At least 100 MHz on bus. */
	[4] = BW_MBPS(1600), /* At least 200 MHz on bus. */
	[5] = BW_MBPS(2128), /* At least 266 MHz on bus. */
	[6] = BW_MBPS(3200), /* At least 400 MHz on bus. */
	[7] = BW_MBPS(4264), /* At least 533 MHz on bus. */
};

static struct msm_bus_scale_pdata bus_client_pdata = {
	.usecase = bw_level_tbl,
	.num_usecases = ARRAY_SIZE(bw_level_tbl),
	.active_only = 1,
	.name = "acpuclock",
};

/* TODO:
 * 1) Update MX voltage when data is avaiable
 * 2) Update bus bandwidth
 * 3) Depending on Frodo version, may need minimum of LVL_NOM
 */
static struct clkctl_acpu_speed acpu_freq_tbl[] = {
	{ 0,   19200, CXO,     0, 0,   1150000,   1150000, 0 },
	{ 1,  300000, PLL0,    4, 2,   1150000,   1150000, 4 },
	{ 1,  384000, ACPUPLL, 5, 0,   1150000,   1150000, 4 },
	{ 1,  600000, PLL0,    4, 0,   1150000,   1150000, 6 },
	{ 1,  787200, ACPUPLL, 5, 0,   1150000,   1150000, 6 },
	{ 0,  998400, ACPUPLL, 5, 0,   1150000,   1150000, 7 },
	{ 0, 1190400, ACPUPLL, 5, 0,   1150000,   1150000, 7 },
	{ 0 }
};

static struct acpuclk_drv_data drv_data = {
	.freq_tbl = acpu_freq_tbl,
	.current_speed = &(struct clkctl_acpu_speed){ 0 },
	.bus_scale = &bus_client_pdata,
	/* FIXME regulator doesn't support corners yet */
	.vdd_max_cpu = 1150000,
	.vdd_max_mem = 1150000,
	.src_clocks = {
		[PLL0].name = "gpll0",
		[ACPUPLL].name = "a7sspll",
	},
	.reg_data = {
		.cfg_src_mask = BM(10, 8),
		.cfg_src_shift = 8,
		.cfg_div_mask = BM(4, 0),
		.cfg_div_shift = 0,
		.update_mask = RCG_CONFIG_UPDATE_BIT,
		.poll_mask = RCG_CONFIG_UPDATE_BIT,
	},
};

static int __init acpuclk_a7_probe(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rcg_base");
	if (!res)
		return -EINVAL;

	drv_data.apcs_rcg_cmd = ioremap(res->start, resource_size(res));
	if (!drv_data.apcs_rcg_cmd)
		return -ENOMEM;

	drv_data.apcs_rcg_config = drv_data.apcs_rcg_cmd + 4;

	drv_data.vdd_cpu = devm_regulator_get(&pdev->dev, "a7_cpu");
	if (IS_ERR(drv_data.vdd_cpu)) {
		dev_err(&pdev->dev, "regulator for %s get failed\n", "a7_cpu");
		return PTR_ERR(drv_data.vdd_cpu);
	}

	drv_data.vdd_mem = devm_regulator_get(&pdev->dev, "a7_mem");
	if (IS_ERR(drv_data.vdd_mem)) {
		dev_err(&pdev->dev, "regulator for %s get failed\n", "a7_mem");
		return PTR_ERR(drv_data.vdd_mem);
	}

	return acpuclk_cortex_init(pdev, &drv_data);
}

static struct of_device_id acpuclk_a7_match_table[] = {
	{.compatible = "qcom,acpuclk-a7"},
	{}
};

static struct platform_driver acpuclk_a7_driver = {
	.driver = {
		.name = "acpuclk-a7",
		.of_match_table = acpuclk_a7_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init acpuclk_a7_init(void)
{
	return platform_driver_probe(&acpuclk_a7_driver, acpuclk_a7_probe);
}
device_initcall(acpuclk_a7_init);
