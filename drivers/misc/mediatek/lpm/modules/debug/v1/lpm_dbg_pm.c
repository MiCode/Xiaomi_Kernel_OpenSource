// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/delay.h>

#include <lpm.h>
#include <lpm_dbg_common_v1.h>


static int lpm_dbg_probe(struct platform_device *pdev)
{
	return 0;
}

#define LPM_KERNEL_SUSPEND "lpm-kernel-suspend"
static int kernel_suspend_only;
static int lpm_dbg_suspend_noirq(struct device *dev)
{
	int ret = 0;

	ret = spm_common_dbg_dump();

	if (kernel_suspend_only == 1) {
		pr_info("[LPM] kernel suspend only ....\n");
		pm_system_wakeup();
	}

	return ret;
}

static const struct dev_pm_ops lpm_dbg_pm_ops = {
	.suspend_noirq = lpm_dbg_suspend_noirq,
};

static const struct of_device_id lpm_dbg_match[] = {
	{ .compatible = "mediatek,mtk-lpm", .data = NULL },
	{},
};
MODULE_DEVICE_TABLE(of, lpm_dbg_match);

static struct platform_driver lpm_dbg_driver = {
	.probe		= lpm_dbg_probe,
	.driver		= {
		.name	= "lpm_dbg",
		.owner = THIS_MODULE,
		.of_match_table	= lpm_dbg_match,
		.pm	= &lpm_dbg_pm_ops,
	}
};


int lpm_dbg_pm_init(void)
{
	int ret = 0;

	unsigned int val = 0;
	struct device_node *lpm_node;

	ret = platform_driver_register(&lpm_dbg_driver);

	if (ret)
		return -1;

	lpm_node = of_find_compatible_node(NULL, NULL, MTK_LPM_DTS_COMPATIBLE);

	if (lpm_node) {

		of_property_read_u32(lpm_node, LPM_KERNEL_SUSPEND, &val);

		if (val == 1) {
			kernel_suspend_only = 1;
		} else {
			kernel_suspend_only = 0;
		}

		of_node_put(lpm_node);
	}

	pr_info("[name:mtk_lpm][P] - kernel suspend only = %d (%s:%d)\n",
					kernel_suspend_only, __func__, __LINE__);

	return 0;
}
EXPORT_SYMBOL(lpm_dbg_pm_init);

void lpm_dbg_pm_exit(void)
{
	platform_driver_unregister(&lpm_dbg_driver);
}
EXPORT_SYMBOL(lpm_dbg_pm_exit);
