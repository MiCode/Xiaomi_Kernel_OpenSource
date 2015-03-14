/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define DEVICE_NAME "msm_test_pacman"

#include <linux/module.h>
#include <linux/platform_device.h>

static int pacman_test_probe(struct platform_device *pdev)
{
	pr_info("%s: %s\n", __func__, DEVICE_NAME);
	return 0;
}

static int pacman_test_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	return 0;
}

static const struct of_device_id pacman_test_dt_match[] = {
	{.compatible = "qcom,msm-test-pacman"},
	{},
};
MODULE_DEVICE_TABLE(of, pacman_test_dt_match);

static struct platform_driver pacman_test_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = pacman_test_dt_match,
	},
	.probe = pacman_test_probe,
	.remove = pacman_test_remove,
};

static int __init pacman_test_init(void)
{
	int rc = 0;
	pr_info("%s\n", __func__);

	rc = platform_driver_register(&pacman_test_driver);
	if (rc)
		pr_err("%s: ERROR Failed to register driver\n", __func__);

	return rc;
}

static void __exit pacman_test_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&pacman_test_driver);
}

MODULE_DESCRIPTION("Peripheral Access Control Manager (PACMan) Test");
MODULE_LICENSE("GPL v2");
module_init(pacman_test_init);
module_exit(pacman_test_exit);
