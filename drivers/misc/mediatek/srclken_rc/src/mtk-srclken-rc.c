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
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/syscore_ops.h>

#include <mtk-srclken-bridge.h>
#include <mtk-srclken-rc.h>
#include <mtk-srclken-rc-common.h>

bool is_srclken_initiated;

enum srclken_config __attribute__((weak)) srclken_hw_get_stage(void)
{
	pr_info("%s: dummy func\n", __func__);
	return SRCLKEN_NOT_SUPPORT;
}

bool __attribute__((weak)) srclken_hw_get_debug_cfg(void)
{
	pr_info("%s: dummy func\n", __func__);
	return false;
}

void __attribute__((weak)) srclken_hw_dump_cfg_log(void)
{
	pr_info("%s: dummy func\n", __func__);
}

void __attribute__((weak)) srclken_hw_dump_sta_log(void)
{
	pr_info("%s: dummy func\n", __func__);
}

void __attribute__((weak)) srclken_hw_dump_last_sta_log(void)
{
	pr_info("%s: dummy func\n", __func__);
}

static int srclken_chk_syscore_suspend(void)
{
	if (srclken_hw_get_debug_cfg()) {
		srclken_hw_dump_cfg_log();
		srclken_hw_dump_sta_log();
	}

	return 0;
}

static void srclken_chk_syscore_resume(void)
{
	if (srclken_get_debug_cfg()) {
		srclken_hw_dump_cfg_log();
		srclken_hw_dump_sta_log();
		srclken_hw_dump_last_sta_log();
	}
}

static struct syscore_ops srclken_chk_syscore_ops = {
	.suspend = srclken_chk_syscore_suspend,
	.resume = srclken_chk_syscore_resume,
};

static int mtk_srclken_probe(struct platform_device *pdev)
{
	struct srclken_bridge pbridge;

	if (srclken_dts_map(pdev)) {
		pr_err("%s: failed due to DTS failed\n", __func__);
		return -1;
	}

	srclken_stage_init();

	if (srclken_hw_get_stage() == SRCLKEN_NOT_SUPPORT)
		return 0;

	if (srclken_hw_get_stage() == SRCLKEN_BRINGUP) {
		srclken_fs_init();
		return 0;
	}

	if (srclken_hw_get_stage() == SRCLKEN_ERR)
		return -1;

	if (is_srclken_initiated)
		return -1;

	if (srclken_fs_init())
		return -1;

	register_syscore_ops(&srclken_chk_syscore_ops);

	pbridge.get_stage_cb = srclken_hw_get_stage;
	pbridge.dump_sta_cb = srclken_hw_dump_sta_log;
	pbridge.dump_cfg_cb = srclken_hw_dump_cfg_log;
	pbridge.dump_last_sta_cb = srclken_hw_dump_last_sta_log;
	srclken_export_platform_bridge_register(&pbridge);

	is_srclken_initiated = true;

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
	},
	.probe = mtk_srclken_probe,
	.id_table = mtk_srclken_ids,
};

module_platform_driver(mtk_srclken_driver);
MODULE_AUTHOR("Owen Chen <owen.chen@mediatek.com>");
MODULE_DESCRIPTION("SOC Driver for MediaTek SRCLKEN RC");
MODULE_LICENSE("GPL");
