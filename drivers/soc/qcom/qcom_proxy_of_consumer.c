// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>

/*
 * of_match table that contains a list of compatible strings for the
 * drivers that are disabled, but needed to make an appearance to
 * satisfy of_devlink.
 */
static const struct of_device_id qcom_proxy_of_consumer_match[] = {
#if !IS_ENABLED(CONFIG_SERIAL_MSM_GENI_CONSOLE)
	{ .compatible = "qcom,msm-geni-console"},
#endif
	{}
};

static int qcom_proxy_of_consumer_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "Proxy probing\n");
	return 0;
}

static struct platform_driver qcom_proxy_of_consumer_driver = {
	.probe = qcom_proxy_of_consumer_probe,
	.driver = {
		.name = "qcom_proxy_of_consumer",
		.of_match_table = qcom_proxy_of_consumer_match,
	},
};

module_platform_driver(qcom_proxy_of_consumer_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, of proxy consumer driver");
