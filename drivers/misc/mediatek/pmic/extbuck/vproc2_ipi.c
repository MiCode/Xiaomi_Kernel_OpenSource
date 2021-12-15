/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include "isl91302a-spi.h"


#define VPROC2_DRV_VERSION "1.0.0_PLAT"
static int vproc2_plat_probe(struct platform_device *pdev)
{
	struct isl91302a_chip *chip;
	int ret = 0;

	pr_info("%s ver(%s)\n", __func__, VPROC2_DRV_VERSION);
	chip = devm_kzalloc(&pdev->dev,
		sizeof(struct isl91302a_chip), GFP_KERNEL);
	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);
	ret = isl91302a_regulator_init(chip);
	if (ret < 0) {
		pr_notice("%s regulator init fail\n", __func__);
		return -EINVAL;
	}
	return ret;
}

static int vproc2_plat_remove(struct platform_device *pdev)
{
	struct isl91302a_chip *chip = platform_get_drvdata(pdev);

	if (chip)
		isl91302a_regulator_deinit(chip);
	return 0;
}

static const struct of_device_id vproc2_id_table[] = {
	{.compatible = "mediatek,vproc2_sspm_plat",},
};

static struct platform_driver vproc2_plat_driver = {
	.driver = {
		.name = "vproc2_plat",
		.owner = THIS_MODULE,
	},
	.probe = vproc2_plat_probe,
	.remove = vproc2_plat_remove,
};

static int __init vproc2_init(void)
{
	pr_info("%s ver(%s)\n", __func__, VPROC2_DRV_VERSION);
	return platform_driver_register(&vproc2_plat_driver);
}

static void __exit vproc2_exit(void)
{
	platform_driver_unregister(&vproc2_plat_driver);
}

subsys_initcall(vproc2_init);
module_exit(vproc2_exit);

MODULE_DESCRIPTION("Vproc2 IPI Driver");
MODULE_VERSION(VPROC2_DRV_VERSION);
MODULE_AUTHOR("Sakya <jeff_chang@richtek.com>");
MODULE_LICENSE("GPL");
