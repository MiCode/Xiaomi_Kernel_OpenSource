// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>

#include "mux_switch.h"

struct fusb304 {
	struct device *dev;
	struct typec_switch *sw;
	struct pinctrl *pinctrl;
	struct pinctrl_state *sel_up;
	struct pinctrl_state *sel_down;
	struct pinctrl_state *enable;
	struct pinctrl_state *disable;
	struct mutex lock;
};

static int fusb304_switch_set(struct typec_switch *sw,
			      enum typec_orientation orientation)
{
	struct fusb304 *fusb = typec_switch_get_drvdata(sw);

	dev_info(fusb->dev, "%s %d\n", __func__, orientation);

	switch (orientation) {
	case TYPEC_ORIENTATION_NONE:
		/* switch off */
		if (fusb->disable)
			pinctrl_select_state(fusb->pinctrl, fusb->disable);
		break;
	case TYPEC_ORIENTATION_NORMAL:
		/* switch cc1 side */
		if (fusb->enable)
			pinctrl_select_state(fusb->pinctrl, fusb->enable);
		if (fusb->sel_up)
			pinctrl_select_state(fusb->pinctrl, fusb->sel_up);
		break;
	case TYPEC_ORIENTATION_REVERSE:
		/* switch cc2 side */
		if (fusb->enable)
			pinctrl_select_state(fusb->pinctrl, fusb->enable);
		if (fusb->sel_down)
			pinctrl_select_state(fusb->pinctrl, fusb->sel_down);
		break;
	default:
		break;
	}

	return 0;
}

static int fusb304_pinctrl_init(struct fusb304 *fusb)
{
	struct device *dev = fusb->dev;
	int ret = 0;

	fusb->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(fusb->pinctrl)) {
		ret = PTR_ERR(fusb->pinctrl);
		dev_info(dev, "failed to get pinctrl, ret=%d\n", ret);
		return ret;
	}

	fusb->sel_up =
		pinctrl_lookup_state(fusb->pinctrl, "sel_up");

	if (IS_ERR(fusb->sel_up)) {
		dev_info(dev, "Can *NOT* find sel_up\n");
		fusb->sel_up = NULL;
	} else
		dev_info(dev, "Find sel_up\n");

	fusb->sel_down =
		pinctrl_lookup_state(fusb->pinctrl, "sel_down");

	if (IS_ERR(fusb->sel_down)) {
		dev_info(dev, "Can *NOT* find sel_down\n");
		fusb->sel_down = NULL;
	} else
		dev_info(dev, "Find sel_down\n");

	fusb->enable =
		pinctrl_lookup_state(fusb->pinctrl, "enable");

	if (IS_ERR(fusb->enable)) {
		dev_info(dev, "Can *NOT* find enable\n");
		fusb->enable = NULL;
	} else
		dev_info(dev, "Find enable\n");

	fusb->disable =
		pinctrl_lookup_state(fusb->pinctrl, "disable");

	if (IS_ERR(fusb->disable)) {
		dev_info(dev, "Can *NOT* find disable\n");
		fusb->disable = NULL;
	} else
		dev_info(dev, "Find disable\n");

	fusb304_switch_set(fusb->sw, TYPEC_ORIENTATION_NONE);

	return ret;
}

static int fusb304_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fusb304 *fusb;
	struct typec_switch_desc sw_desc;
	int ret = 0;

	fusb = devm_kzalloc(&pdev->dev, sizeof(*fusb), GFP_KERNEL);
	if (!fusb)
		return -ENOMEM;

	fusb->dev = dev;

	sw_desc.drvdata = fusb;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = fusb304_switch_set;

	fusb->sw = mtk_typec_switch_register(dev, &sw_desc);
	if (IS_ERR(fusb->sw)) {
		dev_info(dev, "error registering typec switch: %ld\n",
			PTR_ERR(fusb->sw));
		return PTR_ERR(fusb->sw);
	}

	platform_set_drvdata(pdev, fusb);

	ret = fusb304_pinctrl_init(fusb);
	if (ret < 0)
		mtk_typec_switch_unregister(fusb->sw);

	dev_info(dev, "%s done\n", __func__);
	return ret;
}

static int fusb304_remove(struct platform_device *pdev)
{
	struct fusb304 *fusb = platform_get_drvdata(pdev);

	mtk_typec_switch_unregister(fusb->sw);
	return 0;
}

static const struct of_device_id fusb304_ids[] = {
	{.compatible = "mediatek,fusb304",},
	{.compatible = "mediatek,fusb340",},
	{},
};

static struct platform_driver fusb304_driver = {
	.driver = {
		.name = "fusb304",
		.of_match_table = fusb304_ids,
	},
	.probe = fusb304_probe,
	.remove = fusb304_remove,
};

module_platform_driver(fusb304_driver);

MODULE_DESCRIPTION("FUSB304 Type-C switch driver");
MODULE_LICENSE("GPL v2");

