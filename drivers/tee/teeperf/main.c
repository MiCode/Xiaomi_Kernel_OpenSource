// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "user.h"

#ifndef TEEPERF_DEVICE_PROPNAME
#define TEEPERF_DEVICE_PROPNAME "mediatek,teeperf"
#endif

u32 cpu_type;
u32 cpu_map;

static struct {
	dev_t device;
	struct class *class;
	dev_t user_dev;
	struct cdev user_cdev;
} main_ctx;

static inline int teeperf_device_common_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&main_ctx.device, 0, 1, "teeperf");
	if (ret) {
		pr_info(PFX "alloc_chrdev_region failed, ret %d\n", ret);
		return ret;
	}

	main_ctx.class = class_create(THIS_MODULE, "teeperf");
	if (IS_ERR(main_ctx.class)) {
		ret = PTR_ERR(main_ctx.class);
		pr_info(PFX "class_create failed, ret %d\n", ret);
		unregister_chrdev_region(main_ctx.device, 1);
		return ret;
	}

	return 0;
}

static inline void teeperf_device_common_exit(void)
{
	class_destroy(main_ctx.class);
	unregister_chrdev_region(main_ctx.device, 1);
}

static inline int teeperf_device_user_init(void)
{
	struct device *dev;
	int ret = 0;

	main_ctx.user_dev = MKDEV(MAJOR(main_ctx.device), 1);
	/* Create the user node */
	teeperf_user_init(&main_ctx.user_cdev);
	ret = cdev_add(&main_ctx.user_cdev, main_ctx.user_dev, 1);
	if (ret) {
		pr_info(PFX "user cdev_add failed, ret %d\n", ret);
		return ret;
	}

	main_ctx.user_cdev.owner = THIS_MODULE;
	dev = device_create(main_ctx.class, NULL, main_ctx.user_dev, NULL,
			TEEPERF_DEVNODE);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		cdev_del(&main_ctx.user_cdev);
		pr_info(PFX "user device_create failed, ret %d\n", ret);
		return ret;
	}

	return 0;
}

static inline void teeperf_device_user_exit(void)
{
	device_destroy(main_ctx.class, main_ctx.user_dev);
	cdev_del(&main_ctx.user_cdev);
}

static int teeperf_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;

	if (IS_ERR(node)) {
		pr_info(PFX "cannot find device node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(node, "cpu-type", &cpu_type);
	if (ret || !cpu_type) {
		pr_info(PFX "invalid cpu type\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "cpu-map", &cpu_map);
	if (ret || !cpu_map) {
		pr_info(PFX "invalid cpu map\n");
		return -EINVAL;
	}

	ret = teeperf_device_common_init();
	if (ret)
		goto err_common;

	ret = teeperf_device_user_init();
	if (ret)
		goto err_user;

	return 0;

err_user:
	teeperf_device_common_exit();
err_common:
	return ret;
}

static const struct of_device_id of_match_table[] = {
	{ .compatible = TEEPERF_DEVICE_PROPNAME },
	{ }
};

static struct platform_driver teeperf_plat_driver = {
	.probe = teeperf_probe,
	.driver = {
		.name = "teeperf",
		.owner = THIS_MODULE,
		.of_match_table = of_match_table,
	}
};

static int __init teeperf_init(void)
{
	return platform_driver_register(&teeperf_plat_driver);
}

static void __exit teeperf_exit(void)
{
	teeperf_device_user_exit();
	teeperf_device_common_exit();
	platform_driver_unregister(&teeperf_plat_driver);
}

module_init(teeperf_init);
module_exit(teeperf_exit);

MODULE_AUTHOR("Jackson Chang <jackson-kt.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TEE perf driver");
