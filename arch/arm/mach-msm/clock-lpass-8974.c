/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk/msm-clock-generic.h>
#include <soc/qcom/clock-local2.h>

#include <dt-bindings/clock/msm-clocks-8974.h>

#include "clock.h"

enum {
	LPASS_BASE,
	N_BASES,
};

static void __iomem *virt_bases[N_BASES];

#define LPASS_Q6SS_AHB_LFABIF_CBCR               0x22000
#define LPASS_Q6SS_XO_CBCR                       0x26000
#define LPASS_Q6_AXI_CBCR			 0x11C0
#define Q6SS_AHBM_CBCR				 0x22004
#define AUDIO_WRAPPER_BR_CBCR			 0x24000
#define LPASS_Q6SS_BCR				 0x6000

static struct branch_clk q6ss_ahb_lfabif_clk = {
	.cbcr_reg = LPASS_Q6SS_AHB_LFABIF_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "q6ss_ahb_lfabif_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(q6ss_ahb_lfabif_clk.c),
	},
};

static struct branch_clk q6ss_xo_clk = {
	.cbcr_reg = LPASS_Q6SS_XO_CBCR,
	.bcr_reg = LPASS_Q6SS_BCR,
	.has_sibling = 1,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "q6ss_xo_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(q6ss_xo_clk.c),
	},
};

static struct branch_clk q6ss_ahbm_clk = {
	.cbcr_reg = Q6SS_AHBM_CBCR,
	.has_sibling = 1,
	.base = &virt_bases[LPASS_BASE],
	.c = {
		.dbg_name = "q6ss_ahbm_clk",
		.ops = &clk_ops_branch,
		CLK_INIT(q6ss_ahbm_clk.c),
	},
};

static struct clk_lookup msm_clocks_lpass_8974[] = {
	CLK_LOOKUP_OF("core_clk",         q6ss_xo_clk,  "fe200000.qcom,lpass"),
	CLK_LOOKUP_OF("iface_clk", q6ss_ahb_lfabif_clk, "fe200000.qcom,lpass"),
	CLK_LOOKUP_OF("reg_clk",        q6ss_ahbm_clk,  "fe200000.qcom,lpass"),
};

static struct of_device_id msm_clock_lpasscc_match_table[] = {
	{ .compatible = "qcom,lpasscc-8974" },
	{}
};

static int msm_lpasscc_8974_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	if (!res) {
		dev_err(&pdev->dev, "Unable to retrieve register base.\n");
		return -ENOMEM;
	}
	virt_bases[LPASS_BASE] = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!virt_bases[LPASS_BASE])
		return -ENOMEM;

	ret = of_msm_clock_register(pdev->dev.of_node, msm_clocks_lpass_8974,
				ARRAY_SIZE(msm_clocks_lpass_8974));
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Registered LPASS clocks.\n");
	return 0;
}

static struct platform_driver msm_clock_lpasscc_driver = {
	.probe = msm_lpasscc_8974_probe,
	.driver = {
		.name = "lpasscc",
		.of_match_table = msm_clock_lpasscc_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_lpasscc_8974_init(void)
{
	return platform_driver_register(&msm_clock_lpasscc_driver);
}
arch_initcall(msm_lpasscc_8974_init);

