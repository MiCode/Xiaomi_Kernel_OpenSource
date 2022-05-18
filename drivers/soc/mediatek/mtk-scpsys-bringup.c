// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>

static int pd_bring_up_probe(struct platform_device *pdev)
{
	pm_runtime_enable(&pdev->dev);

	/* always enabled in lifetime */
	pm_runtime_get_sync(&pdev->dev);

	return 0;
}

static int pd_post_ao_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	u32 enabled = 0;

	of_property_read_u32(node, "mediatek,post_ao", &enabled);

	pm_runtime_enable(&pdev->dev);

	if (enabled != 1) {
		pr_notice("btypass_pd_post_ao\n");
		return 0;
	}

	/* always enabled in lifetime */
	pm_runtime_get_sync(&pdev->dev);

	return 0;
}

static const struct of_device_id scpsys_bring_up_id_table[] = {
	{
		.compatible = "mediatek,scpsys-bringup",
		.data = pd_bring_up_probe,
	}, {
		.compatible = "mediatek,scpsys-bring-up",
		.data = pd_bring_up_probe,
	}, {
		.compatible = "mediatek,scpsys-post-ao",
		.data = pd_post_ao_probe,
	}, {
		/* sentinel */
	}
};

static int scpsys_bring_up_probe(struct platform_device *pdev)
{
	int (*pd_probe)(struct platform_device *pd);
	int r;

	pd_probe = of_device_get_match_data(&pdev->dev);
	if (!pd_probe)
		return -EINVAL;

	r = pd_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register power-domain provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static int scpsys_bring_up_remove(struct platform_device *pdev)
{
	pm_runtime_put_sync(&pdev->dev);
	return 0;
}

static struct platform_driver scpsys_bring_up = {
	.probe          = scpsys_bring_up_probe,
	.remove         = scpsys_bring_up_remove,
	.driver         = {
		.name   = "scpsys_bring_up",
		.owner  = THIS_MODULE,
		.of_match_table = scpsys_bring_up_id_table,
	},
};
module_platform_driver(scpsys_bring_up);
MODULE_LICENSE("GPL");
