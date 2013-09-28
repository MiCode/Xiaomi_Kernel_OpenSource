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
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/cpr-regulator.h>

#include <mach/clk-provider.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/socinfo.h>

#include "acpuclock-cortex.h"

#define RCG_CONFIG_UPDATE_BIT		BIT(0)

static struct msm_bus_paths bw_level_tbl_8226[] = {
	[0] =  BW_MBPS(152), /* At least 19 MHz on bus. */
	[1] =  BW_MBPS(300), /* At least 37.5 MHz on bus. */
	[2] =  BW_MBPS(400), /* At least 50 MHz on bus. */
	[3] =  BW_MBPS(800), /* At least 100 MHz on bus. */
	[4] = BW_MBPS(1600), /* At least 200 MHz on bus. */
	[5] = BW_MBPS(2128), /* At least 266 MHz on bus. */
	[6] = BW_MBPS(3200), /* At least 400 MHz on bus. */
	[7] = BW_MBPS(4264), /* At least 533 MHz on bus. */
};

static struct msm_bus_paths bw_level_tbl_8610[] = {
	[0] =  BW_MBPS(152), /* At least 19 MHz on bus. */
	[1] =  BW_MBPS(300), /* At least 37.5 MHz on bus. */
	[2] =  BW_MBPS(400), /* At least 50 MHz on bus. */
	[3] =  BW_MBPS(800), /* At least 100 MHz on bus. */
	[4] = BW_MBPS(1600), /* At least 200 MHz on bus. */
	[5] = BW_MBPS(2664), /* At least 333 MHz on bus. */
};

static struct msm_bus_scale_pdata bus_client_pdata = {
	.usecase = bw_level_tbl_8226,
	.num_usecases = ARRAY_SIZE(bw_level_tbl_8226),
	.active_only = 1,
	.name = "acpuclock",
};

static struct clkctl_acpu_speed acpu_freq_tbl_8226_1p1[] = {
	{ 1,  300000, PLL0,    4, 2,   CPR_CORNER_SVS,    0, 4 },
	{ 1,  384000, ACPUPLL, 5, 2,   CPR_CORNER_SVS,    0, 4 },
	{ 1,  600000, PLL0,    4, 0,   CPR_CORNER_NORMAL, 0, 6 },
	{ 1,  787200, ACPUPLL, 5, 0,   CPR_CORNER_NORMAL, 0, 6 },
	{ 1,  998400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1094400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 0, 1190400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 0 }
};

static struct clkctl_acpu_speed acpu_freq_tbl_8226_1p2[] = {
	{ 1,  300000, PLL0,    4, 2,   CPR_CORNER_SVS,    0, 4 },
	{ 1,  384000, ACPUPLL, 5, 2,   CPR_CORNER_SVS,    0, 4 },
	{ 1,  600000, PLL0,    4, 0,   CPR_CORNER_NORMAL, 0, 6 },
	{ 1,  787200, ACPUPLL, 5, 0,   CPR_CORNER_NORMAL, 0, 6 },
	{ 1,  998400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1094400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1190400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 0 }
};

static struct clkctl_acpu_speed acpu_freq_tbl_8226_1p4[] = {
	{ 1,  300000, PLL0,    4, 2,   CPR_CORNER_SVS,    0, 4 },
	{ 1,  384000, ACPUPLL, 5, 2,   CPR_CORNER_SVS,    0, 4 },
	{ 1,  600000, PLL0,    4, 0,   CPR_CORNER_NORMAL, 0, 6 },
	{ 1,  787200, ACPUPLL, 5, 0,   CPR_CORNER_NORMAL, 0, 6 },
	{ 1,  998400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1094400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1190400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1305600, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1344000, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1401600, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 0 }
};

static struct clkctl_acpu_speed acpu_freq_tbl_8226_1p5[] = {
	{ 1,  300000, PLL0,    4, 2,   CPR_CORNER_SVS,    0, 4 },
	{ 1,  384000, ACPUPLL, 5, 2,   CPR_CORNER_SVS,    0, 4 },
	{ 1,  600000, PLL0,    4, 0,   CPR_CORNER_NORMAL, 0, 6 },
	{ 1,  787200, ACPUPLL, 5, 0,   CPR_CORNER_NORMAL, 0, 6 },
	{ 1,  998400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1094400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1190400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1305600, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1344000, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1401600, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1497600, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 0 }
};

static struct clkctl_acpu_speed acpu_freq_tbl_8226_1p6[] = {
	{ 1,  300000, PLL0,    4, 2,   CPR_CORNER_SVS,    0, 4 },
	{ 1,  384000, ACPUPLL, 5, 2,   CPR_CORNER_SVS,    0, 4 },
	{ 1,  600000, PLL0,    4, 0,   CPR_CORNER_NORMAL, 0, 6 },
	{ 1,  787200, ACPUPLL, 5, 0,   CPR_CORNER_NORMAL, 0, 6 },
	{ 1,  998400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1094400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1190400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1305600, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1344000, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1401600, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1497600, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 1, 1593600, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 7 },
	{ 0 }
};

static struct clkctl_acpu_speed acpu_freq_tbl_8610[] = {
	{ 1,  300000, PLL0,    4, 2,   CPR_CORNER_SVS,    0, 3 },
	{ 1,  384000, ACPUPLL, 5, 2,   CPR_CORNER_SVS,    0, 3 },
	{ 1,  600000, PLL0,    4, 0,   CPR_CORNER_NORMAL, 0, 4 },
	{ 1,  787200, ACPUPLL, 5, 0,   CPR_CORNER_NORMAL, 0, 4 },
	{ 1,  998400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 5 },
	{ 1, 1190400, ACPUPLL, 5, 0,   CPR_CORNER_TURBO,  0, 5 },
	{ 0 }
};

static struct clkctl_acpu_speed *pvs_tables_8226[NUM_SPEED_BIN] = {
	[0] = acpu_freq_tbl_8226_1p2,
	[6] = acpu_freq_tbl_8226_1p2,
	[2] = acpu_freq_tbl_8226_1p4,
	[5] = acpu_freq_tbl_8226_1p4,
	[4] = acpu_freq_tbl_8226_1p5,
	[7] = acpu_freq_tbl_8226_1p5,
	[1] = acpu_freq_tbl_8226_1p6,
};

static struct acpuclk_drv_data drv_data = {
	.freq_tbl = acpu_freq_tbl_8226_1p1,
	.pvs_tables = pvs_tables_8226,
	.bus_scale = &bus_client_pdata,
	.vdd_max_cpu = CPR_CORNER_TURBO,
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
	.power_collapse_khz = 300000,
	.wait_for_irq_khz = 300000,
};

static int __init acpuclk_a7_probe(struct platform_device *pdev)
{
	struct resource *res;
	u32 i;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rcg_base");
	if (!res)
		return -EINVAL;

	drv_data.apcs_rcg_cmd = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));
	if (!drv_data.apcs_rcg_cmd)
		return -ENOMEM;

	drv_data.apcs_rcg_config = drv_data.apcs_rcg_cmd + 4;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pte_efuse");
	if (res) {
		drv_data.pte_efuse_base = devm_ioremap(&pdev->dev, res->start,
			resource_size(res));
		if (!drv_data.pte_efuse_base)
			return -ENOMEM;
	}

	drv_data.vdd_cpu = devm_regulator_get(&pdev->dev, "a7_cpu");
	if (IS_ERR(drv_data.vdd_cpu)) {
		dev_err(&pdev->dev, "regulator for %s get failed\n", "a7_cpu");
		return PTR_ERR(drv_data.vdd_cpu);
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

	/* Enable the always on source */
	clk_prepare_enable(drv_data.src_clocks[PLL0].clk);

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

void msm8610_acpu_init(void)
{
	drv_data.bus_scale->usecase = bw_level_tbl_8610;
	drv_data.bus_scale->num_usecases = ARRAY_SIZE(bw_level_tbl_8610);
	drv_data.freq_tbl = acpu_freq_tbl_8610;
}

static int __init acpuclk_a7_init(void)
{
	if (cpu_is_msm8610())
		msm8610_acpu_init();

	return platform_driver_probe(&acpuclk_a7_driver, acpuclk_a7_probe);
}
device_initcall(acpuclk_a7_init);
