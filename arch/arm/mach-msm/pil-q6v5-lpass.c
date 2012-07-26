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
#include <linux/err.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <mach/clk.h>
#include "peripheral-loader.h"
#include "pil-q6v5.h"
#include "scm-pas.h"

#define QDSP6SS_RST_EVB			0x010
#define PROXY_TIMEOUT_MS		10000

static int pil_lpass_enable_clks(struct q6v5_data *drv)
{
	int ret;

	ret = clk_reset(drv->core_clk, CLK_RESET_DEASSERT);
	if (ret)
		goto err_reset;
	ret = clk_prepare_enable(drv->core_clk);
	if (ret)
		goto err_core_clk;
	ret = clk_prepare_enable(drv->ahb_clk);
	if (ret)
		goto err_ahb_clk;
	ret = clk_prepare_enable(drv->axi_clk);
	if (ret)
		goto err_axi_clk;
	ret = clk_prepare_enable(drv->reg_clk);
	if (ret)
		goto err_reg_clk;

	return 0;

err_reg_clk:
	clk_disable_unprepare(drv->axi_clk);
err_axi_clk:
	clk_disable_unprepare(drv->ahb_clk);
err_ahb_clk:
	clk_disable_unprepare(drv->core_clk);
err_core_clk:
	clk_reset(drv->core_clk, CLK_RESET_ASSERT);
err_reset:
	return ret;
}

static void pil_lpass_disable_clks(struct q6v5_data *drv)
{
	clk_disable_unprepare(drv->reg_clk);
	clk_disable_unprepare(drv->axi_clk);
	clk_disable_unprepare(drv->ahb_clk);
	clk_disable_unprepare(drv->core_clk);
	clk_reset(drv->core_clk, CLK_RESET_ASSERT);
}

static int pil_lpass_shutdown(struct pil_desc *pil)
{
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);

	pil_q6v5_halt_axi_port(pil, drv->axi_halt_base);

	/*
	 * If the shutdown function is called before the reset function, clocks
	 * will not be enabled yet. Enable them here so that register writes
	 * performed during the shutdown succeed.
	 */
	if (drv->is_booted == false)
		pil_lpass_enable_clks(drv);

	pil_q6v5_shutdown(pil);
	pil_lpass_disable_clks(drv);

	drv->is_booted = false;

	return 0;
}

static int pil_lpass_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);
	int ret;

	ret = pil_lpass_enable_clks(drv);
	if (ret)
		return ret;

	/* Program Image Address */
	writel_relaxed(((drv->start_addr >> 4) & 0x0FFFFFF0),
				drv->reg_base + QDSP6SS_RST_EVB);

	ret = pil_q6v5_reset(pil);
	if (ret) {
		pil_lpass_disable_clks(drv);
		return ret;
	}

	drv->is_booted = true;

	return 0;
}

static struct pil_reset_ops pil_lpass_ops = {
	.init_image = pil_q6v5_init_image,
	.proxy_vote = pil_q6v5_make_proxy_votes,
	.proxy_unvote = pil_q6v5_remove_proxy_votes,
	.auth_and_reset = pil_lpass_reset,
	.shutdown = pil_lpass_shutdown,
};

static int pil_lpass_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	return pas_init_image(PAS_Q6, metadata, size);
}

static int pil_lpass_reset_trusted(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_Q6);
}

static int pil_lpass_shutdown_trusted(struct pil_desc *pil)
{
	return pas_shutdown(PAS_Q6);
}

static struct pil_reset_ops pil_lpass_ops_trusted = {
	.init_image = pil_lpass_init_image_trusted,
	.proxy_vote = pil_q6v5_make_proxy_votes,
	.proxy_unvote = pil_q6v5_remove_proxy_votes,
	.auth_and_reset = pil_lpass_reset_trusted,
	.shutdown = pil_lpass_shutdown_trusted,
};

static int __devinit pil_lpass_driver_probe(struct platform_device *pdev)
{
	struct q6v5_data *drv;
	struct pil_desc *desc;

	desc = pil_q6v5_init(pdev);
	if (IS_ERR(desc))
		return PTR_ERR(desc);

	drv = platform_get_drvdata(pdev);
	if (drv == NULL)
		return -ENODEV;

	desc->ops = &pil_lpass_ops;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = PROXY_TIMEOUT_MS;

	drv->core_clk = devm_clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(drv->core_clk))
		return PTR_ERR(drv->core_clk);

	drv->ahb_clk = devm_clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(drv->ahb_clk))
		return PTR_ERR(drv->ahb_clk);

	drv->axi_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (IS_ERR(drv->axi_clk))
		return PTR_ERR(drv->axi_clk);

	drv->reg_clk = devm_clk_get(&pdev->dev, "reg_clk");
	if (IS_ERR(drv->reg_clk))
		return PTR_ERR(drv->reg_clk);

	if (pas_supported(PAS_Q6) > 0) {
		desc->ops = &pil_lpass_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_lpass_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}

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
