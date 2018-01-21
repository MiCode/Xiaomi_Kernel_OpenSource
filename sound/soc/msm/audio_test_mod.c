/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

static int audio_test_mod_probe(struct platform_device *pdev)
{
	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return 0;
}

static int audio_test_mod_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id audio_test_mod_dt_match[] = {
	{.compatible = "qcom,audio-test-mod"},
	{}
};

static struct platform_driver audio_test_mod_driver = {
	.driver = {
		.name = "audio-test-mod",
		.owner = THIS_MODULE,
		.of_match_table = audio_test_mod_dt_match,
	},
	.probe = audio_test_mod_probe,
	.remove = audio_test_mod_remove,
};

static int __init audio_test_mod_init(void)
{
	platform_driver_register(&audio_test_mod_driver);
	return 0;
}

static void __exit audio_test_mod_exit(void)
{
	platform_driver_unregister(&audio_test_mod_driver);
}

module_init(audio_test_mod_init);
module_exit(audio_test_mod_exit);

MODULE_DESCRIPTION("Audio test module driver");
MODULE_LICENSE("GPL v2");
