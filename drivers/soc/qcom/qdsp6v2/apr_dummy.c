/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include <linux/qdsp6v2/apr.h>

static int apr_dummy_probe(struct platform_device *pdev)
{
	return 0;
}

static int apr_dummy_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id apr_dummy_dt_match[] = {
	{.compatible = "qcom,msm-audio-apr-dummy"},
	{}
};

static struct platform_driver apr_dummy_driver = {
	.driver = {
		.name = "apr_dummy",
		.owner = THIS_MODULE,
		.of_match_table = apr_dummy_dt_match,
	},
	.probe = apr_dummy_probe,
	.remove = apr_dummy_remove,
};

int __init apr_dummy_init(void)
{
	platform_driver_register(&apr_dummy_driver);
	return 0;
}

void apr_dummy_exit(void)
{
	platform_driver_unregister(&apr_dummy_driver);
}

MODULE_DESCRIPTION("APR dummy module driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, apr_dummy_dt_match);
