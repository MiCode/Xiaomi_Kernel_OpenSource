// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>

#include "typec_switch.h"

int fusb340_set_conf(struct fusb340 *fusb, int orientation)
{
	if (!fusb->pinctrl) {
		dev_err(fusb->dev, "ptn pinctrl not ready\n");
		return 0;
	}

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

int fusb340_init(struct fusb340 *fusb)
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

	return ret;
}
