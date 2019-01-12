// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2014, 2017-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/sysfs.h>
#include <soc/qcom/subsystem_restart.h>

#define BOOT_CMD 1
#define IMAGE_UNLOAD_CMD 0

#define CDSP_SUBSYS_DOWN 0
#define CDSP_SUBSYS_LOADED 1

static ssize_t cdsp_boot_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count);

struct cdsp_loader_private {
	void *pil_h;
	struct kobject *boot_cdsp_obj;
	struct attribute_group *attr_group;
};

static struct kobj_attribute cdsp_boot_attribute =
	__ATTR(boot, 0220, NULL, cdsp_boot_store);

static struct attribute *attrs[] = {
	&cdsp_boot_attribute.attr,
	NULL,
};

static u32 cdsp_state = CDSP_SUBSYS_DOWN;
static struct platform_device *cdsp_private;
static void cdsp_loader_unload(struct platform_device *pdev);

static int cdsp_loader_do(struct platform_device *pdev)
{
	struct cdsp_loader_private *priv = NULL;

	int rc = 0;
	const char *img_name;

	if (!pdev) {
		pr_err("%s: Platform device null\n", __func__);
		goto fail;
	}

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev,
			"%s: Device tree information missing\n", __func__);

		goto fail;
	}

	rc = of_property_read_string(pdev->dev.of_node,
					"qcom,proc-img-to-load",
					&img_name);
	if (rc)
		goto fail;

	if (!strcmp(img_name, "cdsp")) {
		/* cdsp_state always returns "0".*/
		if (cdsp_state == CDSP_SUBSYS_DOWN) {
			priv = platform_get_drvdata(pdev);
			if (!priv) {
				dev_err(&pdev->dev,
				"%s: Private data get failed\n", __func__);
				goto fail;
			}

			dev_dbg(&pdev->dev, "%s: calling subsystem_get on %s\n",
					__func__, img_name);
			priv->pil_h = subsystem_get("cdsp");
			if (IS_ERR(priv->pil_h)) {
				dev_err(&pdev->dev, "%s: subsystem_get failed with error %d\n",
					__func__, (int)(PTR_ERR(priv->pil_h)));
				goto fail;
			}

			/* Set the state of the CDSP.*/
			cdsp_state = CDSP_SUBSYS_LOADED;
		} else if (cdsp_state == CDSP_SUBSYS_LOADED) {
			dev_dbg(&pdev->dev,
			"%s: CDSP state = 0x%x\n", __func__, cdsp_state);
		}

		dev_dbg(&pdev->dev, "%s: CDSP image is loaded\n", __func__);
		return rc;
	}

fail:
	if (pdev)
		dev_err(&pdev->dev,
			"%s: CDSP image loading failed\n", __func__);
	else
		pr_err("%s: CDSP image loading failed\n", __func__);
	return rc;
}


static ssize_t cdsp_boot_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int boot = 0, ret = 0;

	ret = sscanf(buf, "%du", &boot);

	if (ret != 1)
		pr_debug("%s: invalid arguments for cdsp_loader.\n", __func__);

	if (boot == BOOT_CMD) {
		pr_debug("%s: going to call cdsp_loader_do\n", __func__);
		cdsp_loader_do(cdsp_private);
	} else if (boot == IMAGE_UNLOAD_CMD) {
		pr_debug("%s: going to call cdsp_unloader\n", __func__);
		cdsp_loader_unload(cdsp_private);
	}
	return count;
}

static void cdsp_loader_unload(struct platform_device *pdev)
{
	struct cdsp_loader_private *priv = NULL;

	priv = platform_get_drvdata(pdev);

	if (!priv)
		return;

	if (priv->pil_h) {
		dev_dbg(&pdev->dev, "%s: calling subsystem_put\n", __func__);
		subsystem_put(priv->pil_h);
		priv->pil_h = NULL;
	}
}

static int cdsp_loader_init_sysfs(struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct cdsp_loader_private *priv = NULL;

	cdsp_private = NULL;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	priv->pil_h = NULL;
	priv->boot_cdsp_obj = NULL;
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

	priv->boot_cdsp_obj = kobject_create_and_add("boot_cdsp", kernel_kobj);
	if (!priv->boot_cdsp_obj) {
		dev_err(&pdev->dev, "%s: sysfs create and add failed\n",
						__func__);
		ret = -ENOMEM;
		goto error_return;
	}

	ret = sysfs_create_group(priv->boot_cdsp_obj, priv->attr_group);
	if (ret) {
		dev_err(&pdev->dev, "%s: sysfs create group failed %d\n",
							__func__, ret);
		goto error_return;
	}

	cdsp_private = pdev;

	return 0;

error_return:

	if (priv->boot_cdsp_obj) {
		kobject_del(priv->boot_cdsp_obj);
		priv->boot_cdsp_obj = NULL;
	}

	return ret;
}

static int cdsp_loader_remove(struct platform_device *pdev)
{
	struct cdsp_loader_private *priv = NULL;

	priv = platform_get_drvdata(pdev);

	if (!priv)
		return 0;

	if (priv->pil_h) {
		subsystem_put(priv->pil_h);
		priv->pil_h = NULL;
	}

	if (priv->boot_cdsp_obj) {
		sysfs_remove_group(priv->boot_cdsp_obj, priv->attr_group);
		kobject_del(priv->boot_cdsp_obj);
		priv->boot_cdsp_obj = NULL;
	}

	return 0;
}

static int cdsp_loader_probe(struct platform_device *pdev)
{
	int ret = cdsp_loader_init_sysfs(pdev);

	if (ret != 0) {
		dev_err(&pdev->dev, "%s: Error in initing sysfs\n", __func__);
		return ret;
	}

	return 0;
}

static const struct of_device_id cdsp_loader_dt_match[] = {
	{ .compatible = "qcom,cdsp-loader" },
	{ }
};
MODULE_DEVICE_TABLE(of, cdsp_loader_dt_match);

static struct platform_driver cdsp_loader_driver = {
	.driver = {
		.name = "cdsp-loader",
		.of_match_table = cdsp_loader_dt_match,
	},
	.probe = cdsp_loader_probe,
	.remove = cdsp_loader_remove,
};

static int __init cdsp_loader_init(void)
{
	return platform_driver_register(&cdsp_loader_driver);
}
module_init(cdsp_loader_init);

static void __exit cdsp_loader_exit(void)
{
	platform_driver_unregister(&cdsp_loader_driver);
}
module_exit(cdsp_loader_exit);

MODULE_DESCRIPTION("CDSP Loader module");
MODULE_LICENSE("GPL v2");
