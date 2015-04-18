/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/qdsp6v2/apr.h>
#include <linux/of_device.h>
#include <linux/sysfs.h>
#include <soc/qcom/subsystem_restart.h>

#define Q6_PIL_GET_DELAY_MS 100
#define BOOT_CMD 1
#define IMAGE_UNLOAD_CMD 0

static ssize_t adsp_boot_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count);

struct adsp_loader_private {
	void *pil_h;
	struct kobject *boot_adsp_obj;
	struct attribute_group *attr_group;
};

static struct kobj_attribute adsp_boot_attribute =
	__ATTR(boot, 0220, NULL, adsp_boot_store);

static struct attribute *attrs[] = {
	&adsp_boot_attribute.attr,
	NULL,
};

static struct platform_device *adsp_private;
static void adsp_loader_unload(struct platform_device *pdev);

static void adsp_loader_do(struct platform_device *pdev)
{

	struct adsp_loader_private *priv = NULL;

	const char *adsp_dt = "qcom,adsp-state";
	int rc = 0;
	u32 adsp_state;
	const char *img_name;

	if (!pdev) {
		dev_err(&pdev->dev, "%s: Platform device null\n", __func__);
		goto fail;
	}

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev,
			"%s: Device tree information missing\n", __func__);
		goto fail;
	}

	rc = of_property_read_u32(pdev->dev.of_node, adsp_dt, &adsp_state);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: ADSP state = %x\n", __func__, adsp_state);
		goto fail;
	}

	rc = of_property_read_string(pdev->dev.of_node,
					"qcom,proc-img-to-load",
					&img_name);

	if (rc) {
		dev_dbg(&pdev->dev,
			"%s: loading default image ADSP\n", __func__);
		goto load_adsp;
	}
	if (!strcmp(img_name, "modem")) {
		/* adsp_state always returns "0". So load modem image based on
		apr_modem_state to prevent loading of image twice */
		adsp_state = apr_get_modem_state();
		if (adsp_state == APR_SUBSYS_DOWN) {
			priv = platform_get_drvdata(pdev);
			if (!priv) {
				dev_err(&pdev->dev,
				" %s: Private data get failed\n", __func__);
				goto fail;
			}

			priv->pil_h = subsystem_get("modem");
			if (IS_ERR(priv->pil_h)) {
				dev_err(&pdev->dev, "%s: pil get failed,\n",
					__func__);
				goto fail;
			}

			/* Set the state of the ADSP in APR driver */
			apr_set_modem_state(APR_SUBSYS_LOADED);
		} else if (adsp_state == APR_SUBSYS_LOADED) {
			dev_dbg(&pdev->dev,
			"%s: MDSP state = %x\n", __func__, adsp_state);
		}

		dev_dbg(&pdev->dev, "%s: Q6/MDSP image is loaded\n", __func__);
		return;
	}
load_adsp:
	{
		adsp_state = apr_get_q6_state();
		if (adsp_state == APR_SUBSYS_DOWN) {
			priv = platform_get_drvdata(pdev);
			if (!priv) {
				dev_err(&pdev->dev,
				" %s: Private data get failed\n", __func__);
				goto fail;
			}

			priv->pil_h = subsystem_get("adsp");
			if (IS_ERR(priv->pil_h)) {
				dev_err(&pdev->dev, "%s: pil get failed,\n",
					__func__);
				goto fail;
			}

			/* Set the state of the ADSP in APR driver */
			apr_set_q6_state(APR_SUBSYS_LOADED);
		} else if (adsp_state == APR_SUBSYS_LOADED) {
			dev_dbg(&pdev->dev,
			"%s: ADSP state = %x\n", __func__, adsp_state);
		}

		dev_dbg(&pdev->dev, "%s: Q6/ADSP image is loaded\n", __func__);
		return;
	}
fail:
	dev_err(&pdev->dev, "%s: Q6 image loading failed\n", __func__);
	return;
}


static ssize_t adsp_boot_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int boot = 0;
	sscanf(buf, "%du", &boot);

	if (boot == BOOT_CMD) {
		pr_debug("%s: going to call adsp_loader_do\n", __func__);
		adsp_loader_do(adsp_private);
	} else if (boot == IMAGE_UNLOAD_CMD) {
		pr_debug("%s: going to call adsp_unloader\n", __func__);
		adsp_loader_unload(adsp_private);
	}
	return count;
}

static void adsp_loader_unload(struct platform_device *pdev)
{
	struct adsp_loader_private *priv = NULL;

	priv = platform_get_drvdata(pdev);

	if (!priv)
		return;

	if (priv->pil_h) {
		dev_dbg(&pdev->dev, "%s: calling subsystem put\n", __func__);
		subsystem_put(priv->pil_h);
		priv->pil_h = NULL;
	}
}

static int adsp_loader_init_sysfs(struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct adsp_loader_private *priv = NULL;
	adsp_private = NULL;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "%s: memory alloc failed\n", __func__);
		ret = -ENOMEM;
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	priv->pil_h = NULL;
	priv->boot_adsp_obj = NULL;
	priv->attr_group = devm_kzalloc(&pdev->dev,
				sizeof(*(priv->attr_group)),
				GFP_KERNEL);
	if (!priv->attr_group) {
		dev_err(&pdev->dev, "%s: malloc attr_group failed\n",
						__func__);
		ret = -ENOMEM;
		goto error_return;
	}

	priv->attr_group->attrs = attrs;

	priv->boot_adsp_obj = kobject_create_and_add("boot_adsp", kernel_kobj);
	if (!priv->boot_adsp_obj) {
		dev_err(&pdev->dev, "%s: sysfs create and add failed\n",
						__func__);
		ret = -ENOMEM;
		goto error_return;
	}

	ret = sysfs_create_group(priv->boot_adsp_obj, priv->attr_group);
	if (ret) {
		dev_err(&pdev->dev, "%s: sysfs create group failed %d\n",
							__func__, ret);
		goto error_return;
	}

	adsp_private = pdev;

	return 0;

error_return:

	if (priv->boot_adsp_obj) {
		kobject_del(priv->boot_adsp_obj);
		priv->boot_adsp_obj = NULL;
	}

	return ret;
}

static int adsp_loader_remove(struct platform_device *pdev)
{
	struct adsp_loader_private *priv = NULL;

	priv = platform_get_drvdata(pdev);

	if (!priv)
		return 0;

	if (priv->pil_h) {
		subsystem_put(priv->pil_h);
		priv->pil_h = NULL;
	}

	if (priv->boot_adsp_obj) {
		sysfs_remove_group(priv->boot_adsp_obj, priv->attr_group);
		kobject_del(priv->boot_adsp_obj);
		priv->boot_adsp_obj = NULL;
	}

	return 0;
}

static int adsp_loader_probe(struct platform_device *pdev)
{
	int ret = adsp_loader_init_sysfs(pdev);
	if (ret != 0) {
		dev_err(&pdev->dev, "%s: Error in initing sysfs\n", __func__);
		return ret;
	}

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
	.remove = adsp_loader_remove,
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
