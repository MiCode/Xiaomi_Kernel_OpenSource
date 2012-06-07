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

#include "peripheral-loader.h"
#include "pil-q6v5.h"

#define QDSP6SS_RST_EVB			0x010
#define PROXY_TIMEOUT_MS		10000

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
		pil_q6v5_enable_clks(pil);

	pil_q6v5_shutdown(pil);
	pil_q6v5_disable_clks(pil);

	drv->is_booted = false;

	return 0;
}

static int pil_lpass_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = dev_get_drvdata(pil->dev);
	int ret;

	ret = pil_q6v5_enable_clks(pil);
	if (ret)
		return ret;

	/* Program Image Address */
	writel_relaxed(((drv->start_addr >> 4) & 0x0FFFFFF0),
				drv->reg_base + QDSP6SS_RST_EVB);

	ret = pil_q6v5_reset(pil);
	if (ret) {
		pil_q6v5_disable_clks(pil);
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
