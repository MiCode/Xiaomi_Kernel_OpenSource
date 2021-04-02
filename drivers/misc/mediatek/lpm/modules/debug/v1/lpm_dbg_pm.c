// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>

#include <lpm_dbg_common_v1.h>

static int lpm_dbg_probe(struct platform_device *pdev)
{
	return 0;
}


static int lpm_dbg_suspend_noirq(struct device *dev)
{
	int ret = 0;

	if (pm_suspend_default_s2idle())
		ret = spm_common_dbg_dump();

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

	ret = platform_driver_register(&lpm_dbg_driver);

	if (ret)
		return -1;

	return 0;
}
EXPORT_SYMBOL(lpm_dbg_pm_init);

void lpm_dbg_pm_exit(void)
{
	platform_driver_unregister(&lpm_dbg_driver);
}
EXPORT_SYMBOL(lpm_dbg_pm_exit);
