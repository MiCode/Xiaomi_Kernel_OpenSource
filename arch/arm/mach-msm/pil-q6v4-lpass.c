/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>

#include "peripheral-loader.h"
#include "pil-q6v4.h"
#include "scm-pas.h"

static int pil_q6v4_lpass_boot(struct pil_desc *pil)
{
	struct q6v4_data *drv = pil_to_q6v4_data(pil);
	int err;

	err = pil_q6v4_power_up(drv);
	if (err)
		return err;

	return pil_q6v4_boot(pil);
}

static int pil_q6v4_lpass_shutdown(struct pil_desc *pil)
{
	struct q6v4_data *drv = pil_to_q6v4_data(pil);
	int ret;

	ret = pil_q6v4_shutdown(pil);
	if (ret)
		return ret;
	pil_q6v4_power_down(drv);
	return 0;
}

static struct pil_reset_ops pil_q6v4_lpass_ops = {
	.init_image = pil_q6v4_init_image,
	.auth_and_reset = pil_q6v4_lpass_boot,
	.shutdown = pil_q6v4_lpass_shutdown,
	.proxy_vote = pil_q6v4_make_proxy_votes,
	.proxy_unvote = pil_q6v4_remove_proxy_votes,
};

static struct pil_reset_ops pil_q6v4_lpass_ops_trusted = {
	.init_image = pil_q6v4_init_image_trusted,
	.auth_and_reset = pil_q6v4_boot_trusted,
	.shutdown = pil_q6v4_shutdown_trusted,
	.proxy_vote = pil_q6v4_make_proxy_votes,
	.proxy_unvote = pil_q6v4_remove_proxy_votes,
};

static int __devinit pil_q6v4_lpass_driver_probe(struct platform_device *pdev)
{
	const struct pil_q6v4_pdata *pdata = pdev->dev.platform_data;
	struct q6v4_data *drv;
	struct pil_desc *desc;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	drv->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!drv->base)
		return -ENOMEM;

	drv->vreg = devm_regulator_get(&pdev->dev, "core_vdd");
	if (IS_ERR(drv->vreg))
		return PTR_ERR(drv->vreg);

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return PTR_ERR(drv->xo);

	desc = &drv->desc;
	desc->name = pdata->name;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = 10000;
	pil_q6v4_init(drv, pdata);

	if (pas_supported(pdata->pas_id) > 0) {
		desc->ops = &pil_q6v4_lpass_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_q6v4_lpass_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}

	drv->pil = msm_pil_register(desc);
	if (IS_ERR(drv->pil))
		return PTR_ERR(drv->pil);
	return 0;
}

static int __devexit pil_q6v4_lpass_driver_exit(struct platform_device *pdev)
{
	struct q6v4_data *drv = platform_get_drvdata(pdev);
	msm_pil_unregister(drv->pil);
	return 0;
}

static struct platform_driver pil_q6v4_lpass_driver = {
	.probe = pil_q6v4_lpass_driver_probe,
	.remove = __devexit_p(pil_q6v4_lpass_driver_exit),
	.driver = {
		.name = "pil-q6v4-lpass",
		.owner = THIS_MODULE,
	},
};

static int __init pil_q6v4_lpass_init(void)
{
	return platform_driver_register(&pil_q6v4_lpass_driver);
}
module_init(pil_q6v4_lpass_init);

static void __exit pil_q6v4_lpass_exit(void)
{
	platform_driver_unregister(&pil_q6v4_lpass_driver);
}
module_exit(pil_q6v4_lpass_exit);

MODULE_DESCRIPTION("Support for booting QDSP6v4 (Hexagon) processors");
MODULE_LICENSE("GPL v2");
