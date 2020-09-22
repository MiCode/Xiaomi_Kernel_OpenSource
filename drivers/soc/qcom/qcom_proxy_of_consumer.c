// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/amba/bus.h>

#define QCOM_PROXY_CONSUMER_AMBA_ID(pid)			\
	{				\
		.id	= pid,		\
		.mask	= 0x000fffff,	\
	}

/*
 * of_match table that contains a list of compatible strings for the
 * drivers that are disabled, but needed to make an appearance to
 * satisfy of_devlink.
 */
static const struct of_device_id qcom_proxy_of_consumer_match[] = {
#if !IS_ENABLED(CONFIG_SERIAL_MSM_GENI_CONSOLE)
	{ .compatible = "qcom,msm-geni-console"},
#endif
#if !IS_ENABLED(CONFIG_MSM_JTAGV8)
	{ .compatible = "qcom,jtagv8-mm"},
#endif
	{}
};

static const struct amba_id qcom_proxy_amba_of_consumer_match[] = {
#if !IS_ENABLED(CONFIG_CORESIGHT_SOURCE_ETM4X)
	QCOM_PROXY_CONSUMER_AMBA_ID(0x000bb95d), /* Cortex-A53 */
	QCOM_PROXY_CONSUMER_AMBA_ID(0x000bb95e), /* Cortex-A57 */
	QCOM_PROXY_CONSUMER_AMBA_ID(0x000bb95a), /* Cortex-A72 */
	QCOM_PROXY_CONSUMER_AMBA_ID(0x000bb959), /* Cortex-A73 */
	QCOM_PROXY_CONSUMER_AMBA_ID(0x000bb9da), /* Cortex-A35 */
	QCOM_PROXY_CONSUMER_AMBA_ID(0x000f0205), /* QCOM Kryo */
	QCOM_PROXY_CONSUMER_AMBA_ID(0x000f0211), /* QCOM Kryo */
	QCOM_PROXY_CONSUMER_AMBA_ID(0x000bb802), /* QCOM Kryo 385 Cortex-A55 */
	QCOM_PROXY_CONSUMER_AMBA_ID(0x000bb803), /* QCOM Kryo 385 Cortex-A75 */
#endif
	{}
};

static int qcom_proxy_of_consumer_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "Proxy probing\n");
	return 0;
}

static int qcom_proxy_amba_of_consumer_probe(struct amba_device *adev,
						const struct amba_id *id)
{
	dev_dbg(&adev->dev, "Proxy probing\n");

	/* Before calling this probe, the amba framework votes for the clk
	 * by default. It removes the vote only during the runtime suspend
	 * of the device. Hence, explicilty decrement the usage count of
	 * the device for the suspend to happen.
	 */
	pm_runtime_put(&adev->dev);

	return 0;
}

static struct platform_driver qcom_proxy_of_consumer_driver = {
	.probe = qcom_proxy_of_consumer_probe,
	.driver = {
		.name = "qcom_proxy_of_consumer",
		.of_match_table = qcom_proxy_of_consumer_match,
	},
};

static struct amba_driver qcom_proxy_amba_of_consumer_driver = {
	.probe = qcom_proxy_amba_of_consumer_probe,
	.id_table = qcom_proxy_amba_of_consumer_match,
	.drv = {
		.name = "qcom_proxy_amba_of_consumer",
	},
};

static int __init qcom_proxy_of_init(void)
{
	int ret;

	ret = platform_driver_register(&qcom_proxy_of_consumer_driver);
	if (ret < 0)
		return ret;

	ret = amba_driver_register(&qcom_proxy_amba_of_consumer_driver);
	if (ret < 0)
		goto amba_fail;

	return 0;

amba_fail:
	platform_driver_unregister(&qcom_proxy_of_consumer_driver);
	return ret;
}

static void __exit qcom_proxy_of_exit(void)
{
	amba_driver_unregister(&qcom_proxy_amba_of_consumer_driver);
	platform_driver_unregister(&qcom_proxy_of_consumer_driver);
}

module_init(qcom_proxy_of_init);
module_exit(qcom_proxy_of_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, of proxy consumer driver");
