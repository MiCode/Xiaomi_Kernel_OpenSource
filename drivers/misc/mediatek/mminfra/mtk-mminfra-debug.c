// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/scmi_protocol.h>
#include <linux/slab.h>
#include "tinysys-scmi.h"

static struct notifier_block mtk_pd_notifier;
static struct scmi_tinysys_info_st *tinfo;
static int feature_id;


static bool mminfra_check_scmi_status(void)
{
	if (tinfo)
		return true;

	tinfo = get_scmi_tinysys_info();

	if (IS_ERR_OR_NULL(tinfo)) {
		pr_notice("%s: tinfo is wrong!!\n", __func__);
		tinfo = NULL;
		return false;
	}

	if (IS_ERR_OR_NULL(tinfo->ph)) {
		pr_notice("%s: tinfo->ph is wrong!!\n", __func__);
		tinfo = NULL;
		return false;
	}

	of_property_read_u32(tinfo->sdev->dev.of_node, "scmi_smi", &feature_id);
	pr_notice("%s: get scmi_smi succeed!!\n", __func__);
	return true;
}

static void do_mminfra_bkrs(bool is_restore)
{
	int err;

	if (mminfra_check_scmi_status()) {
		err = scmi_tinysys_common_set(tinfo->ph, feature_id,
				2, (is_restore)?0:1, 0, 0, 0);
		pr_notice("%s: call scmi_tinysys_common_set(%d) err=%d\n",
			__func__, is_restore, err);
	}
}

static int mtk_mminfra_pd_callback(struct notifier_block *nb,
			unsigned long flags, void *data)
{
	if (flags == GENPD_NOTIFY_ON)
		do_mminfra_bkrs(true);
	else if (flags == GENPD_NOTIFY_PRE_OFF)
		do_mminfra_bkrs(false);

	return NOTIFY_OK;
}

int mminfra_scmi_test(const char *val, const struct kernel_param *kp)
{
	int ret, arg0;
	unsigned int test_case;
	void __iomem *test_base = ioremap(0x1e800280, 4);

	ret = sscanf(val, "%u %d", &test_case, &arg0);
	if (ret != 2) {
		pr_notice("%s: invalid input: %s, result(%d)\n", __func__, val, ret);
		return -EINVAL;
	}
	if (mminfra_check_scmi_status()) {
		if (test_case == 2 && arg0 == 0) {
			writel(0x123, test_base);
			pr_notice("%s: before BKRS read 0x1e800280 = 0x%x\n",
				__func__, readl_relaxed(test_base));
		}
		pr_notice("%s: feature_id=%d test_case=%d arg0=%d\n",
			__func__, feature_id, test_case, arg0);
		ret = scmi_tinysys_common_set(tinfo->ph, feature_id, test_case, arg0, 0, 0, 0);
		pr_notice("%s: scmi return %d\n", __func__, ret);
		if (test_case == 2 && arg0 == 1)
			pr_notice("%s after BKRS read 0x1e800280 = 0x%x\n",
				__func__, readl_relaxed(test_base));
	}

	iounmap(test_base);

	return 0;
}

static struct kernel_param_ops scmi_test_ops = {
	.set = mminfra_scmi_test,
	.get = param_get_int,
};
module_param_cb(scmi_test, &scmi_test_ops, NULL, 0644);
MODULE_PARM_DESC(scmi_test, "scmi test");

static int mminfra_debug_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	u32 mminfra_bkrs = 0;
	int ret = 0;

	of_property_read_u32(node, "mminfra-bkrs", &mminfra_bkrs);

	mminfra_check_scmi_status();

	pm_runtime_enable(dev);
	if (mminfra_bkrs == 1) {
		mtk_pd_notifier.notifier_call = mtk_mminfra_pd_callback;
		ret = dev_pm_genpd_add_notifier(dev, &mtk_pd_notifier);
	}
	return ret;
}

static const struct of_device_id of_mminfra_debug_match_tbl[] = {
	{
		.compatible = "mediatek,mminfra-debug",
	},
	{}
};

static struct platform_driver mminfra_debug_drv = {
	.probe = mminfra_debug_probe,
	.driver = {
		.name = "mtk-mminfra-debug",
		.of_match_table = of_mminfra_debug_match_tbl,
	},
};

static int __init mtk_mminfra_debug_init(void)
{
	s32 status;

	status = platform_driver_register(&mminfra_debug_drv);
	if (status) {
		pr_notice("Failed to register MMInfra debug driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_mminfra_debug_exit(void)
{
	platform_driver_unregister(&mminfra_debug_drv);
}

module_init(mtk_mminfra_debug_init);
module_exit(mtk_mminfra_debug_exit);
MODULE_DESCRIPTION("MTK MMInfra Debug driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL v2");
