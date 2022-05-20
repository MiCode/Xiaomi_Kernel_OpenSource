// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <soc/mediatek/mmdvfs.h>

static struct device *dev;
static struct regulator *reg;
static struct clk *vcore_clk;
static unsigned int force_step0 = 0xff;
static int vcore_step_cnt;

static int wait_mmdvfs_init_thread(void *data)
{
	struct device_node *node = dev->of_node;
	unsigned int release_step0 = 0;

	while (!mtk_is_mmdvfs_init_done()) {
		pr_notice("%s: mmdvfs is not ready yet\n", __func__);
		msleep(2000);
	}

	clk_set_rate(vcore_clk, vcore_step_cnt - 1 - force_step0);
	pr_notice("%s: set vcore clk(%d)\n", __func__, vcore_step_cnt - 1 - force_step0);

	of_property_read_u32(node, "release-step0", &release_step0);
	if (release_step0) {
		if (!IS_ERR_OR_NULL(reg)) {
			regulator_set_voltage(reg, 0, INT_MAX);
			pr_notice("%s: set vcore voltage(0)\n", __func__);
		}

		clk_set_rate(vcore_clk, 0);
		pr_notice("%s: set vcore_clk(0)\n", __func__);
	}

	return 0;
}

static int mmdvfs_debug_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int volt;
	struct task_struct *kthr;

	dev = &pdev->dev;
	of_property_read_u32(node, "force-step0", &force_step0);

	if (force_step0 == 0xff) {
		/* It means platform porting is not done. */
		pr_notice("%s: force_step0 not set\n", __func__);
		return 0;
	}

	/* Force set step 0 during first probe */
	if (!reg) {
		reg = devm_regulator_get(dev, "dvfsrc-vcore");
		if (IS_ERR(reg)) {
			reg = NULL;
			pr_notice("%s: get regulator fail(%d)\n",
				__func__, PTR_ERR(reg));
			return PTR_ERR(reg);
		}
		vcore_step_cnt = regulator_count_voltages(reg);
		if (vcore_step_cnt <= 0) {
			pr_notice("%s: regulator_count_voltages fail(%d)\n",
				__func__, vcore_step_cnt);
			return -EINVAL;
		}

		if (force_step0 >= vcore_step_cnt) {
			pr_notice("%s: force_step0(%u) >= vcore_step_cnt(%d)\n",
				__func__, force_step0, vcore_step_cnt);
			return -EINVAL;
		}
		volt = regulator_list_voltage(reg, vcore_step_cnt - 1 - force_step0);

		pr_notice("%s: set vcore voltage(%d)\n", __func__, volt);
		regulator_set_voltage(reg, volt, INT_MAX);
	}

	vcore_clk = devm_clk_get(dev, "vcore");
	if (IS_ERR_OR_NULL(vcore_clk)) {
		pr_notice("%s: get vcore_clk fail(%d)\n",
			__func__, PTR_ERR(vcore_clk));
		vcore_clk = NULL;
	} else {
		kthr = kthread_run(wait_mmdvfs_init_thread, NULL, "mmdvfs-dbg-vcp");
	}

	return 0;
}

void mtk_mmdvfs_debug_release_step0(void)
{
	struct device_node *node = dev->of_node;
	unsigned int release_step0 = 0;

	if (!vcore_clk) {
		of_property_read_u32(node, "release-step0", &release_step0);
		if (release_step0) {
			if (reg) {
				regulator_set_voltage(reg, 0, INT_MAX);
				pr_notice("%s: set vcore voltage(0)\n", __func__);
			}
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_mmdvfs_debug_release_step0);

static const struct of_device_id of_mmdvfs_debug_match_tbl[] = {
	{
		.compatible = "mediatek,mmdvfs-debug",
	},
	{}
};

static struct platform_driver mmdvfs_debug_drv = {
	.probe = mmdvfs_debug_probe,
	.driver = {
		.name = "mtk-mmdvfs-debug",
		.of_match_table = of_mmdvfs_debug_match_tbl,
	},
};

static int __init mtk_mmdvfs_debug_init(void)
{
	s32 status;

	status = platform_driver_register(&mmdvfs_debug_drv);
	if (status) {
		pr_notice("Failed to register MMDVFS debug driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_mmdvfs_debug_exit(void)
{
	platform_driver_unregister(&mmdvfs_debug_drv);
}

module_init(mtk_mmdvfs_debug_init);
module_exit(mtk_mmdvfs_debug_exit);
MODULE_DESCRIPTION("MTK MMDVFS Debug driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");

