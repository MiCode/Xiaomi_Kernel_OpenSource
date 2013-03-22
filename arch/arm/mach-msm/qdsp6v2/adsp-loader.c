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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <mach/subsystem_restart.h>
#include <mach/qdsp6v2/apr.h>
#include <linux/of_device.h>

#define Q6_PIL_GET_DELAY_MS 100

struct adsp_loader_private {
	void *pil_h;
};

static int adsp_loader_probe(struct platform_device *pdev)
{
	struct adsp_loader_private *priv;
	int rc = 0;
	const char *adsp_dt = "qcom,adsp-state";
	u32 adsp_state;

	rc = of_property_read_u32(pdev->dev.of_node, adsp_dt, &adsp_state);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: ADSP state = %x\n", __func__, adsp_state);
		return rc;
	}

	if (adsp_state == APR_SUBSYS_DOWN) {
		priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
		if (!priv)
			return -ENOMEM;

		platform_set_drvdata(pdev, priv);

		priv->pil_h = subsystem_get("adsp");
		if (IS_ERR(priv->pil_h)) {
			pr_err("%s: pil get adsp failed, error:%d\n",
				__func__, rc);
			devm_kfree(&pdev->dev, priv);
			goto fail;
		}

		/* Set the state of the ADSP in APR driver */
		apr_set_q6_state(APR_SUBSYS_LOADED);
	} else if (adsp_state == APR_SUBSYS_LOADED) {
		dev_dbg(&pdev->dev,
			"%s:MDM9x25 ADSP state = %x\n", __func__, adsp_state);
		apr_set_q6_state(APR_SUBSYS_LOADED);
	}

	/* Query for MMPM API */

	pr_info("%s: Q6/ADSP image is loaded\n", __func__);
fail:
	return rc;
}

static int adsp_loader_remove(struct platform_device *pdev)
{
	struct adsp_loader_private *priv;

	priv = platform_get_drvdata(pdev);
	if (priv != NULL)
		subsystem_put(priv->pil_h);
	pr_info("%s: Q6/ADSP image is unloaded\n", __func__);

	return 0;
}

static const struct of_device_id adsp_loader_dt_match[] = {
	{ .compatible = "qcom,adsp-loader" },
	{ }
};
MODULE_DEVICE_TABLE(of, adsp_loader_dt_match);

static struct platform_driver adsp_loader_driver = {
	.driver = {
		.name = "adsp-loader",
		.owner = THIS_MODULE,
		.of_match_table = adsp_loader_dt_match,
	},
	.probe = adsp_loader_probe,
	.remove = __devexit_p(adsp_loader_remove),
};

static int __init adsp_loader_init(void)
{
	return platform_driver_register(&adsp_loader_driver);
}
module_init(adsp_loader_init);

static void __exit adsp_loader_exit(void)
{
	platform_driver_unregister(&adsp_loader_driver);
}
module_exit(adsp_loader_exit);

MODULE_DESCRIPTION("ADSP Loader module");
MODULE_LICENSE("GPL v2");
