/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/err.h>
#include <linux/of.h>

#include "peripheral-loader.h"
#include "pil-q6v5.h"

/* Register Offsets */
#define QDSP6SS_RST_EVB			0x010
#define LPASS_Q6SS_BCR			0x06000
#define LPASS_Q6SS_AHB_LFABIF_CBCR	0x22000
#define LPASS_Q6SS_XO_CBCR		0x26000
#define AXI_HALTREQ			0x0
#define AXI_HALTACK			0x4
#define AXI_IDLE			0x8

#define HALT_ACK_TIMEOUT_US		100000

static void clk_reg_enable(void __iomem *reg)
{
	u32 val;
	val = readl_relaxed(reg);
	val |= BIT(0);
	writel_relaxed(val, reg);
}

static void clk_reg_disable(void __iomem *reg)
{
	u32 val;
	val = readl_relaxed(reg);
	val &= ~BIT(0);
	writel_relaxed(val, reg);
}

static int pil_lpass_shutdown(struct pil_desc *pil)
{
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);
	int ret;
	u32 status;

	writel_relaxed(1, drv->axi_halt_base + AXI_HALTREQ);
	ret = readl_poll_timeout(drv->axi_halt_base + AXI_HALTACK,
		status,	status, 50, HALT_ACK_TIMEOUT_US);
	if (ret)
		dev_err(pil->dev, "Port halt timeout\n");
	else if (!readl_relaxed(drv->axi_halt_base + AXI_IDLE))
		dev_err(pil->dev, "Port halt failed\n");
	writel_relaxed(0, drv->axi_halt_base + AXI_HALTREQ);

	/* Make sure Q6 registers are accessible */
	writel_relaxed(0, drv->clk_base + LPASS_Q6SS_BCR);
	clk_reg_enable(drv->clk_base + LPASS_Q6SS_AHB_LFABIF_CBCR);
	mb();

	pil_q6v5_shutdown(pil);

	/* Disable clocks and assert subsystem resets. */
	clk_reg_disable(drv->clk_base + LPASS_Q6SS_AHB_LFABIF_CBCR);
	clk_reg_disable(drv->clk_base + LPASS_Q6SS_XO_CBCR);
	writel_relaxed(1, drv->clk_base + LPASS_Q6SS_BCR);

	return 0;
}

static int pil_lpass_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);

	/*
	 * Bring subsystem out of reset and enable required
	 * regulators and clocks.
	 */
	writel_relaxed(0, drv->clk_base + LPASS_Q6SS_BCR);
	clk_reg_enable(drv->clk_base + LPASS_Q6SS_XO_CBCR);
	clk_reg_enable(drv->clk_base + LPASS_Q6SS_AHB_LFABIF_CBCR);
	mb();

	/* Program Image Address */
	writel_relaxed(((drv->start_addr >> 4) & 0x0FFFFFF0),
				drv->reg_base + QDSP6SS_RST_EVB);

	return pil_q6v5_reset(pil);
}

static struct pil_reset_ops pil_lpass_ops = {
	.init_image = pil_q6v5_init_image,
	.proxy_vote = pil_q6v5_make_proxy_votes,
	.proxy_unvote = pil_q6v5_remove_proxy_votes,
	.auth_and_reset = pil_lpass_reset,
	.shutdown = pil_lpass_shutdown,
};

static int __devinit pil_lpass_driver_probe(struct platform_device *pdev)
{
	struct q6v5_data *drv;
	struct pil_desc *desc;
	struct resource *res;

	desc = pil_q6v5_init(pdev);
	drv = platform_get_drvdata(pdev);

	desc->ops = &pil_lpass_ops;
	desc->owner = THIS_MODULE;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	drv->axi_halt_base = devm_ioremap(&pdev->dev, res->start,
					  resource_size(res));
	if (!drv->axi_halt_base)
		return -ENOMEM;

	drv->pil = msm_pil_register(desc);
	if (IS_ERR(drv->pil))
		return PTR_ERR(drv->pil);

	return 0;
}

static int __devexit pil_lpass_driver_exit(struct platform_device *pdev)
{
	struct q6v5_data *drv = platform_get_drvdata(pdev);
	msm_pil_unregister(drv->pil);
	return 0;
}

static struct of_device_id lpass_match_table[] = {
	{ .compatible = "qcom,pil-q6v5-lpass" },
	{}
};

static struct platform_driver pil_lpass_driver = {
	.probe = pil_lpass_driver_probe,
	.remove = __devexit_p(pil_lpass_driver_exit),
	.driver = {
		.name = "pil-q6v5-lpass",
		.of_match_table = lpass_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init pil_lpass_init(void)
{
	return platform_driver_register(&pil_lpass_driver);
}
module_init(pil_lpass_init);

static void __exit pil_lpass_exit(void)
{
	platform_driver_unregister(&pil_lpass_driver);
}
module_exit(pil_lpass_exit);

MODULE_DESCRIPTION("Support for booting low-power audio subsystems with QDSP6v5 (Hexagon) processors");
MODULE_LICENSE("GPL v2");
