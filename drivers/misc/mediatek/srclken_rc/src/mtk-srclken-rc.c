// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: owen.chen <owen.chen@mediatek.com>
 */

/*
 * @file    mtk-srclken-rc.c
 * @brief   Driver for subys request resource control
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "mtk-srclken-rc.h"
#include "mtk-srclken-rc-common.h"
#include "mtk-srclken-rc-hw.h"

bool is_srclken_initiated;

int __attribute__((weak)) srclken_hw_get_cfg(void)
{
	pr_info("%s: dummy func\n", __func__);
	return SRCLKEN_NOT_SUPPORT;
}

bool __attribute__((weak)) srclken_hw_get_debug_cfg(void)
{
	pr_info("%s: dummy func\n", __func__);
	return SRCLKEN_NOT_SUPPORT;
}

int __attribute__((weak)) srclken_hw_dump_cfg_log(void)
{
	pr_info("%s: dummy func\n", __func__);
	return SRCLKEN_NOT_SUPPORT;
}

int __attribute__((weak)) srclken_hw_dump_sta_log(void)
{
	pr_info("%s: dummy func\n", __func__);
	return SRCLKEN_NOT_SUPPORT;
}

int __attribute__((weak)) srclken_hw_dump_last_sta_log(void)
{
	pr_info("%s: dummy func\n", __func__);
	return SRCLKEN_NOT_SUPPORT;
}

static bool _srclken_check(void)
{
	if (!is_srclken_initiated)
		return false;

	if (srclken_get_bringup_sta())
		return false;

	if (srclken_hw_get_cfg() == NOT_SUPPORT_CFG)
		return false;

	return true;
}

bool srclken_get_debug_cfg(void)
{
	return srclken_hw_get_debug_cfg();
}

int srclken_dump_sta_log(void)
{
	if (_srclken_check())
		return srclken_hw_dump_sta_log();

	pr_notice("dump sta log not registered\n");
	return SRCLKEN_NOT_READY;
}
EXPORT_SYMBOL(srclken_dump_sta_log);

int srclken_dump_cfg_log(void)
{
	if (_srclken_check())
		return srclken_hw_dump_cfg_log();

	pr_notice("dump cfg log not registered\n");
	return SRCLKEN_NOT_READY;
}
EXPORT_SYMBOL(srclken_dump_cfg_log);

int srclken_dump_last_sta_log(void)
{
	if (_srclken_check())
		return srclken_hw_dump_last_sta_log();

	pr_err("dump last sta log not registered\n");
	return SRCLKEN_NOT_READY;
}
EXPORT_SYMBOL(srclken_dump_last_sta_log);

static int srclken_dev_pm_suspend(struct device *dev)
{
	if (srclken_hw_get_debug_cfg()) {
		srclken_hw_dump_cfg_log();
		srclken_hw_dump_sta_log();
	}

	return 0;
}

static int srclken_dev_pm_resume(struct device *dev)
{
	if (srclken_get_debug_cfg()) {
		srclken_hw_dump_cfg_log();
		srclken_hw_dump_sta_log();
		srclken_hw_dump_last_sta_log();
	}

	return 0;
}

static const struct dev_pm_ops srclken_dev_pm_ops = {
	.suspend_noirq = srclken_dev_pm_suspend,
	.resume_noirq = srclken_dev_pm_resume,
};

static int mtk_srclken_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (is_srclken_initiated)
		return 0;

	if (srclken_hw_is_ready() == SRCLKEN_NOT_READY)
		return -EPROBE_DEFER;

	srclken_get_bringup_node(pdev);
	if (srclken_get_bringup_sta())
		return SRCLKEN_BRINGUP;

	if (srclken_dts_map(pdev)) {
		pr_err("%s: failed due to DTS failed\n", __func__);
		return -1;
	}

	ret = srclken_cfg_init();
	if (ret)
		return 0;

	if (srclken_fs_init())
		return -1;

	is_srclken_initiated = true;

	pr_notice("%s: init done\n", __func__);

	return 0;
}

static const struct platform_device_id mtk_srclken_ids[] = {
	{"mtk-srclken-rc", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mtk_srclken_ids);

static const struct of_device_id mtk_srclken_of_match[] = {
	{
		.compatible = "mediatek,srclken",
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mtk_srclken_of_match);

static struct platform_driver mtk_srclken_driver = {
	.driver = {
		.name = "mtk-srclken-rc",
		.of_match_table = of_match_ptr(mtk_srclken_of_match),
		.pm = &srclken_dev_pm_ops,
	},
	.probe = mtk_srclken_probe,
	.id_table = mtk_srclken_ids,
};

module_platform_driver(mtk_srclken_driver);
MODULE_AUTHOR("Owen Chen <owen.chen@mediatek.com>");
MODULE_DESCRIPTION("SOC Driver for MediaTek SRCLKEN RC");
MODULE_LICENSE("GPL");
