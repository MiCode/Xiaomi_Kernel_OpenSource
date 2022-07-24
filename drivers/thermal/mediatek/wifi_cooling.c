// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include "wifi_cooling.h"
#include "conn_power_throttling.h"


static DEFINE_MUTEX(wifi_cdev_list_lock);
static DEFINE_MUTEX(wifi_cdata_lock);
static LIST_HEAD(wifi_cdev_list);


/*==================================================
 * cooler callback functions
 *==================================================
 */
static int wifi_cooling_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct wifi_cooling_device *wifi_cdev = cdev->devdata;

	*state = wifi_cdev->max_state;

	return 0;
}

static int wifi_cooling_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct wifi_cooling_device *wifi_cdev = cdev->devdata;

	*state = wifi_cdev->target_state;

	return 0;
}

static int wifi_cooling_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct wifi_cooling_device *wifi_cdev = cdev->devdata;
	struct device *dev = wifi_cdev->dev;
	int ret;

	/* Request state should be less than max_state */
	if (WARN_ON(state > wifi_cdev->max_state ||
		!wifi_cdev->throttle->state_to_wifi_limit))
		return -EINVAL;

	if (wifi_cdev->target_state == state)
		return 0;

	wifi_cdev->target_state = state;

	ret = wifi_cdev->throttle->state_to_wifi_limit(wifi_cdev);

	if (ret < 0) {
		dev_err(dev, "wifi limit fail\n");
		return ret;
	}

	dev_info(dev, "%s: set lv = %ld done\n", wifi_cdev->name, state);

	return ret;
}

/*==================================================
 * platform data and platform driver callbacks
 *==================================================
 */

static int cooling_state_to_wifi_level_limit(struct wifi_cooling_device *wifi_cdev)
{
	int ret = -1;
	struct device *dev = wifi_cdev->dev;

	ret = conn_pwr_set_thermal_level(wifi_cdev->target_state);
	if (ret < 0) {
		dev_err(dev, "set conn pwr thermal level fail\n");
		return ret;
	}

	dev_info(dev, "%s:set connsys lv to %ld done\n",
		wifi_cdev->name, wifi_cdev->target_state);

	return ret;

}

static struct thermal_cooling_device_ops wifi_cooling_ops = {
	.get_max_state		= wifi_cooling_get_max_state,
	.get_cur_state		= wifi_cooling_get_cur_state,
	.set_cur_state		= wifi_cooling_set_cur_state,
};

static const struct wifi_cooling_platform_data wifi_level_throttle = {
	.state_to_wifi_limit = cooling_state_to_wifi_level_limit,
};

static const struct of_device_id wifi_cooling_of_match[] = {
	{
		.compatible = "mediatek,wifi-level-cooler",
		.data = (void *)&wifi_level_throttle,
	},
	{},
};
MODULE_DEVICE_TABLE(of, wifi_cooling_of_match);

static int wifi_cooling_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct thermal_cooling_device *cdev;
	struct wifi_cooling_device *wifi_cdev;
	struct device_node *np = pdev->dev.of_node;
	unsigned int len;

	wifi_cdev = devm_kzalloc(dev, sizeof(*wifi_cdev), GFP_KERNEL);
	if (!wifi_cdev)
		return -ENOMEM;

	len = (strlen(np->name) > MAX_WIFI_COOLER_NAME_LEN) ?
		MAX_WIFI_COOLER_NAME_LEN : strlen(np->name);

	strncpy(wifi_cdev->name, np->name, len);
	wifi_cdev->name[len] = '\0';

	wifi_cdev->target_state = WIFI_COOLING_UNLIMITED_STATE;
	wifi_cdev->max_state = WIFI_COOLING_MAX_STATE;
	wifi_cdev->throttle = of_device_get_match_data(dev);
	wifi_cdev->dev = dev;

	cdev = thermal_of_cooling_device_register(dev->of_node, wifi_cdev->name,
			wifi_cdev, &wifi_cooling_ops);
	if (IS_ERR(cdev))
		return -EINVAL;

	wifi_cdev->cdev = cdev;

	platform_set_drvdata(pdev, wifi_cdev);
	dev_info(dev, "register %s done\n", wifi_cdev->name);

	return 0;
}

static int wifi_cooling_remove(struct platform_device *pdev)
{
	struct wifi_cooling_device *wifi_cdev;

	wifi_cdev = (struct wifi_cooling_device *)platform_get_drvdata(pdev);
	thermal_cooling_device_unregister(wifi_cdev->cdev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver wifi_cooling_driver = {
	.probe = wifi_cooling_probe,
	.remove = wifi_cooling_remove,
	.driver = {
		.name = "mtk-wifi-cooler",
		.of_match_table = wifi_cooling_of_match,
	},
};
module_platform_driver(wifi_cooling_driver);

MODULE_AUTHOR("Jerry-SC.Wu <jerry-sc.wu@mediatek.com>");
MODULE_DESCRIPTION("Mediatek wifi cooling driver");
MODULE_LICENSE("GPL v2");
