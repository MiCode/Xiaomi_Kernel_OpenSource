// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>

#include "typec_switch.h"

int ptn36241g_set_conf(struct ptn36241g *ptn, int orientation)
{
	if (!ptn->pinctrl) {
		dev_err(ptn->dev, "ptn pinctrl not ready\n");
		return 0;
	}

	dev_info(ptn->dev, "%s %d\n", __func__, orientation);

	switch (orientation) {
	case TYPEC_ORIENTATION_NONE:
		/* enter sleep mode */
		if (ptn->c1_sleep)
			pinctrl_select_state(ptn->pinctrl, ptn->c1_sleep);
		if (ptn->c2_sleep)
			pinctrl_select_state(ptn->pinctrl, ptn->c2_sleep);
		break;
	case TYPEC_ORIENTATION_NORMAL:
	case TYPEC_ORIENTATION_REVERSE:
		/* enter work mode */
		if (ptn->c1_active)
			pinctrl_select_state(ptn->pinctrl, ptn->c1_active);
		if (ptn->c2_active)
			pinctrl_select_state(ptn->pinctrl, ptn->c2_active);
		break;
	default:
		break;
	}

	return 0;
}

int ptn36241g_init(struct ptn36241g *ptn)
{
	struct device *dev = ptn->dev;
	int ret = 0;

	ptn->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ptn->pinctrl)) {
		ret = PTR_ERR(ptn->pinctrl);
		dev_err(dev, "failed to get pinctrl, ret=%d\n", ret);
		return ret;
	}

	ptn->c1_active =
		pinctrl_lookup_state(ptn->pinctrl, "c1_active");

	if (IS_ERR(ptn->c1_active)) {
		dev_info(dev, "Can *NOT* find c1_active\n");
		ptn->c1_active = NULL;
	} else
		dev_info(dev, "Find c1_active\n");

	ptn->c1_sleep =
		pinctrl_lookup_state(ptn->pinctrl, "c1_sleep");

	if (IS_ERR(ptn->c1_sleep)) {
		dev_info(dev, "Can *NOT* find c1_sleep\n");
		ptn->c1_sleep = NULL;
	} else
		dev_info(dev, "Find c1_sleep\n");

	ptn->c2_active =
		pinctrl_lookup_state(ptn->pinctrl, "c2_active");

	if (IS_ERR(ptn->c2_active)) {
		dev_info(dev, "Can *NOT* find c2_active\n");
		ptn->c2_active = NULL;
	} else
		dev_info(dev, "Find c2_active\n");

	ptn->c2_sleep =
		pinctrl_lookup_state(ptn->pinctrl, "c2_sleep");

	if (IS_ERR(ptn->c2_sleep)) {
		dev_info(dev, "Can *NOT* find c2_sleep\n");
		ptn->c2_sleep = NULL;
	} else
		dev_info(dev, "Find c2_sleep\n");

	ptn36241g_set_conf(ptn, TYPEC_ORIENTATION_NONE);

	return 0;
}
