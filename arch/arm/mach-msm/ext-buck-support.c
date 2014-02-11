/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <mach/rpm-smd.h>

#define RPM_REQUEST_TYPE_GPIO  0x6f697067 /* gpio */
#define RPM_GPIO_NUMB_KEY      0x626d756e /* numb */
#define RPM_GPIO_STAT_KEY      0x74617473 /* stat */
#define RPM_GPIO_SETT_KEY      0x74746573 /* sett */
#define RPM_GPIO_RESOURCE_ID   3
#define GPIO_ON                1
#define GPIO_OFF               0

static int msm_send_ext_buck_votes(int gpio_num, int settling_time)
{
	int rc;
	int gpio_status_sleep = GPIO_OFF;
	int gpio_status_active = GPIO_ON;

	struct msm_rpm_kvp kvp_sleep[] = {
		{
			.key = RPM_GPIO_STAT_KEY,
			.data = (void *)&gpio_status_sleep,
			.length = sizeof(gpio_status_sleep),
		}
	};

	struct msm_rpm_kvp kvp_active[] = {
		{
			.key = RPM_GPIO_NUMB_KEY,
			.data = (void *)&gpio_num,
			.length = sizeof(gpio_num),
		},
		{
			.key = RPM_GPIO_STAT_KEY,
			.data = (void *)&gpio_status_active,
			.length = sizeof(gpio_status_active),
		},
		{
			.key = RPM_GPIO_SETT_KEY,
			.data = (void *)&settling_time,
			.length = sizeof(settling_time),
		},
	};

	rc = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
		RPM_REQUEST_TYPE_GPIO, RPM_GPIO_RESOURCE_ID, kvp_active,
							ARRAY_SIZE(kvp_active));
	WARN(rc < 0, "RPM GPIO toggling (active set) did not enable!\n");

	rc = msm_rpm_send_message(MSM_RPM_CTX_SLEEP_SET,
		RPM_REQUEST_TYPE_GPIO, RPM_GPIO_RESOURCE_ID, kvp_sleep,
							ARRAY_SIZE(kvp_sleep));
	WARN(rc < 0, "RPM GPIO toggling (sleep set) did not enable!\n");

	return rc;
}

static int msm_ext_buck_probe(struct platform_device *pdev)
{
	char *key = NULL;
	int gpio_num;
	int settling_time;
	int ret = 0;

	key = "qcom,gpio-num";
	ret = of_property_read_u32(pdev->dev.of_node, key, &gpio_num);
	if (ret) {
		pr_debug("%s: Cannot read %s from dt", __func__, key);
		return ret;
	}

	key = "qcom,settling-time";
	ret = of_property_read_u32(pdev->dev.of_node, key,
					&settling_time);
	if (ret) {
		pr_debug("%s: Cannot read %s from dt", __func__, key);
		return ret;
	}

	ret = msm_send_ext_buck_votes(gpio_num, settling_time);

	return ret;
}

static struct of_device_id msm_ext_buck_table[] = {
	{.compatible = "qcom,ext-buck-support"},
	{},
};

static struct platform_driver msm_ext_buck_driver = {
	.probe = msm_ext_buck_probe,
	.driver = {
		.name = "ext-buck-support",
		.owner = THIS_MODULE,
		.of_match_table = msm_ext_buck_table,
	},
};

static int __init msm_ext_buck_init(void)
{
	return platform_driver_register(&msm_ext_buck_driver);
}
late_initcall(msm_ext_buck_init);
