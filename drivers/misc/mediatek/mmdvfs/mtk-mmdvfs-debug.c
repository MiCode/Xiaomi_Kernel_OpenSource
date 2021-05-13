// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

static int mmdvfs_debug_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct regulator *reg = NULL;
	int volt_cnt, volt;
	struct device_node *disp_node;
	struct platform_device *disp_dev;
	unsigned int force_step0 = 0, release_step0 = 0;

 	of_property_read_u32(node, "force-step0", &force_step0);

	if (force_step0 == 0) {
		/* It means platform porting is not done. */
		pr_notice("%s: force_step0 not set\n", __func__);
		return 0;
	}

	/* Force set step 0 during first probe */
	if (!reg) {
		reg = devm_regulator_get(dev, "dvfsrc-vcore");
		if (IS_ERR(reg)) {
			pr_notice("%s: get regulator fail(%d)\n",
				__func__, PTR_ERR(reg));
			return PTR_ERR(reg);
		}
		volt_cnt = regulator_count_voltages(reg);
		if (volt_cnt <= 0) {
			pr_notice("%s: regulator_count_voltages fail(%d)\n",
				__func__, volt_cnt);
			return -EINVAL;
		}
		volt = regulator_list_voltage(reg, volt_cnt - 1);

		pr_notice("%s: set vcore voltage(%d)\n", __func__, volt);
		regulator_set_voltage(reg, volt, INT_MAX);
	}

	/* Return -EPROBE_DEFFER until display is probed */
	disp_node = of_parse_phandle(node, "disp-dev", 0);

	if (!disp_node) {
		pr_notice("disp-dev phandle is not found\n");
		return -EINVAL;
	}
	disp_dev = of_find_device_by_node(disp_node);
	of_node_put(disp_node);
	if (!disp_dev || !disp_dev->dev.driver) {
		pr_notice("disp-dev driver is not ready\n");
		return -EPROBE_DEFER;
	}

	of_property_read_u32(node, "release-step0", &release_step0);
	if (release_step0) {
		regulator_set_voltage(reg, 0, INT_MAX);
		pr_notice("%s: set vcore voltage(%d)\n", __func__, 0);
	}

	return 0;
}

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
