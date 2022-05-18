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

#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
#include "mux_switch.h"
#endif

struct ptn36241g {
	struct device *dev;
	struct typec_switch *sw;
	struct pinctrl *pinctrl;
	struct pinctrl_state *c1_active;
	struct pinctrl_state *c1_sleep;
	struct pinctrl_state *c2_active;
	struct pinctrl_state *c2_sleep;
	struct mutex lock;
};

static int ptn36241g_switch_set(struct typec_switch *sw,
			      enum typec_orientation orientation)
{
	struct ptn36241g *ptn = typec_switch_get_drvdata(sw);

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

static int ptn36241g_pinctrl_init(struct ptn36241g *ptn)
{
	struct device *dev = ptn->dev;
	int ret = 0;

	ptn->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ptn->pinctrl)) {
		ret = PTR_ERR(ptn->pinctrl);
		dev_info(dev, "failed to get pinctrl, ret=%d\n", ret);
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

	ptn36241g_switch_set(ptn->sw, TYPEC_ORIENTATION_NONE);

	return ret;
}

static int ptn36241g_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ptn36241g *ptn;
	struct typec_switch_desc sw_desc = { };
	int ret = 0;

	ptn = devm_kzalloc(&pdev->dev, sizeof(*ptn), GFP_KERNEL);
	if (!ptn)
		return -ENOMEM;

	ptn->dev = dev;

	sw_desc.drvdata = ptn;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = ptn36241g_switch_set;

#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
	ptn->sw = mtk_typec_switch_register(dev, &sw_desc);
#else
	ptn->sw = typec_switch_register(dev, &sw_desc);
#endif
	if (IS_ERR(ptn->sw)) {
		dev_info(dev, "error registering typec switch: %ld\n",
			PTR_ERR(ptn->sw));
		return PTR_ERR(ptn->sw);
	}

	platform_set_drvdata(pdev, ptn);

	ret = ptn36241g_pinctrl_init(ptn);
	if (ret < 0) {
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
		mtk_typec_switch_unregister(ptn->sw);
#else
		typec_switch_unregister(ptn->sw);
#endif
	}

	dev_info(dev, "%s done\n", __func__);
	return ret;
}

static int ptn36241g_remove(struct platform_device *pdev)
{
	struct ptn36241g *ptn = platform_get_drvdata(pdev);

	mtk_typec_switch_unregister(ptn->sw);
	return 0;
}

static const struct of_device_id ptn36241g_ids[] = {
	{.compatible = "mediatek,ptn36241g",},
	{},
};

static struct platform_driver ptn36241g_driver = {
	.driver = {
		.name = "ptn36241g",
		.of_match_table = ptn36241g_ids,
	},
	.probe = ptn36241g_probe,
	.remove = ptn36241g_remove,
};

module_platform_driver(ptn36241g_driver);

MODULE_DESCRIPTION("PTN36241G USB redriver driver");
MODULE_LICENSE("GPL v2");

