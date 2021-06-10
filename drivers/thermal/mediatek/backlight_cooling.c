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
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include <linux/leds-mtk.h>
#include "backlight_cooling.h"

/*==================================================
 * cooler callback functions
 *==================================================
 */
static int backlight_throttle(struct backlight_cooling_device *backlight_cdev, unsigned long state)
{
	struct device *dev = backlight_cdev->dev;
	unsigned long bl_percent;

	int enable = (state == BACKLIGHT_COOLING_UNLIMITED_STATE) ? 0 : 1;
	bl_percent = BACKLIGHT_COOLING_MAX_STATE - state;

	setMaxBrightness(backlight_cdev->name, bl_percent, enable);
	backlight_cdev->target_state = state;
	dev_info(dev, "%s: set lv = %ld, bl percent = %ld done\n", backlight_cdev->name, state, bl_percent);
	return 0;
}

static int backlight_cooling_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct backlight_cooling_device *backlight_cdev = cdev->devdata;

	*state = backlight_cdev->max_state;

	return 0;
}

static int backlight_cooling_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct backlight_cooling_device *backlight_cdev = cdev->devdata;

	*state = backlight_cdev->target_state;

	return 0;
}

static int backlight_cooling_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct backlight_cooling_device *backlight_cdev = cdev->devdata;
	int ret;

	/* Request state should be less than max_state */
	if (WARN_ON(state > backlight_cdev->max_state || !backlight_cdev->throttle))
		return -EINVAL;

	if (backlight_cdev->target_state == state)
		return 0;

	ret = backlight_cdev->throttle(backlight_cdev, state);

	return ret;
}

/*==================================================
 * platform data and platform driver callbacks
 *==================================================
 */
static const struct of_device_id backlight_cooling_of_match[] = {
	{ .compatible = "mediatek,backlight-cooler", },
	{},
};
MODULE_DEVICE_TABLE(of, backlight_cooling_of_match);

static struct thermal_cooling_device_ops backlight_cooling_ops = {
	.get_max_state		= backlight_cooling_get_max_state,
	.get_cur_state		= backlight_cooling_get_cur_state,
	.set_cur_state		= backlight_cooling_set_cur_state,
};

static int init_bl_cooling_device(struct device *dev, struct backlight_cooling_device *bl_cdev)
{
	struct thermal_cooling_device *cdev;

	bl_cdev->target_state = BACKLIGHT_COOLING_UNLIMITED_STATE;
	bl_cdev->max_state = BACKLIGHT_COOLING_MAX_STATE;
	bl_cdev->throttle = backlight_throttle;
	bl_cdev->dev = dev;

	cdev = thermal_of_cooling_device_register(dev->of_node, bl_cdev->name,
			bl_cdev, &backlight_cooling_ops);
	if (IS_ERR(cdev))
		goto init_fail;
	bl_cdev->cdev = cdev;

	dev_info(dev, "register %s done\n", bl_cdev->name);

	return 0;

init_fail:
	return -EINVAL;
}

static int parse_dt(struct platform_device *pdev, struct backlight_cooling_device *bl_cdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	const char *bl_names = NULL;
	int ret;

	ret = of_property_read_string(np, "backlight-names", &(bl_names));
	if (ret) {
		dev_notice(dev, "failed to parse cooler name from DT!\n");
		return ret;
	}

	snprintf(bl_cdev->name, MAX_BACKLIGHT_COOLER_NAME_LEN, "%s", bl_names);

	return 0;
}

static int backlight_cooling_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct backlight_cooling_device *bl_cdev;
	int ret = 0;

	bl_cdev = devm_kzalloc(dev, sizeof(*bl_cdev), GFP_KERNEL);
	if (!bl_cdev)
		return -ENOMEM;

	ret = parse_dt(pdev, bl_cdev);
	if (ret) {
		dev_notice(dev, "failed to parse cooler nodes from DT!\n");
		return ret;
	}

	ret = init_bl_cooling_device(dev, bl_cdev);
	if (ret) {
		dev_notice(dev, "failed to init backlight cooler!\n");
		return ret;
	}

	platform_set_drvdata(pdev, bl_cdev);

	return 0;
}

static int backlight_cooling_remove(struct platform_device *pdev)
{
	struct backlight_cooling_device *bl_cdev;

	bl_cdev = (struct backlight_cooling_device *)platform_get_drvdata(pdev);
	thermal_cooling_device_unregister(bl_cdev->cdev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver backlight_cooling_driver = {
	.probe = backlight_cooling_probe,
	.remove = backlight_cooling_remove,
	.driver = {
		.name = "mtk-backlight-cooling",
		.of_match_table = backlight_cooling_of_match,
	},
};
module_platform_driver(backlight_cooling_driver);

MODULE_AUTHOR("Ming-Hao Chou <minghao.chou@mediatek.com>");
MODULE_DESCRIPTION("Mediatek backlight cooling driver");
MODULE_LICENSE("GPL v2");
