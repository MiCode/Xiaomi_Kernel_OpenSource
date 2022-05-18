// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>

#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
#include "mux_switch.h"
#endif

struct ps5169 {
	struct device *dev;
	struct i2c_client *i2c;
	struct typec_switch *sw;
	struct pinctrl *pinctrl;
	struct pinctrl_state *enable;
	struct pinctrl_state *disable;
	struct mutex lock;
};

#define PS5169_ORIENTATION_NONE                 0x80
#define PS5169_ORIENTATION_NORMAL               0xc0
#define PS5169_ORIENTATION_FLIP                 0xd0

static int ps5169_init(struct ps5169 *ps)
{
	/* do thing now */
	return 0;
}

static int ps5169_switch_set(struct typec_switch *sw,
			enum typec_orientation orientation)
{
	struct ps5169 *ps = typec_switch_get_drvdata(sw);

	dev_info(ps->dev, "%s %d\n", __func__, orientation);

	switch (orientation) {
	case TYPEC_ORIENTATION_NONE:
		/* switch off */
		if (ps->disable)
			pinctrl_select_state(ps->pinctrl, ps->disable);
		break;
	case TYPEC_ORIENTATION_NORMAL:
		/* switch cc1 side */
		if (ps->enable) {
			pinctrl_select_state(ps->pinctrl, ps->enable);
			mdelay(20);
		}
		ps5169_init(ps);

		/* i2c_smbus_write_byte_data(ps->i2c, 0x40,
			PS5169_ORIENTATION_NORMAL);
		*/
		break;
	case TYPEC_ORIENTATION_REVERSE:
		/* switch cc2 side */
		if (ps->enable) {
			pinctrl_select_state(ps->pinctrl, ps->enable);
			mdelay(20);
		}
		ps5169_init(ps);
		/* i2c_smbus_write_byte_data(ps->i2c, 0x40,
			PS5169_ORIENTATION_FLIP);
		*/
		break;
	default:
		break;
	}

	return 0;
}

static int ps5169_pinctrl_init(struct ps5169 *ps)
{
	struct device *dev = ps->dev;
	int ret = 0;

	ps->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ps->pinctrl)) {
		ret = PTR_ERR(ps->pinctrl);
		dev_info(dev, "failed to get pinctrl, ret=%d\n", ret);
		return ret;
	}

	ps->enable =
		pinctrl_lookup_state(ps->pinctrl, "enable");

	if (IS_ERR(ps->enable)) {
		dev_info(dev, "Can *NOT* find enable\n");
		ps->enable = NULL;
	} else
		dev_info(dev, "Find enable\n");

	ps->disable =
		pinctrl_lookup_state(ps->pinctrl, "disable");

	if (IS_ERR(ps->disable)) {
		dev_info(dev, "Can *NOT* find disable\n");
		ps->disable = NULL;
	} else
		dev_info(dev, "Find disable\n");

	return ret;
}

static int ps5169_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ps5169 *ps;
	struct typec_switch_desc sw_desc = {};
	int ret = 0;

	ps = devm_kzalloc(dev, sizeof(*ps), GFP_KERNEL);
	if (!ps)
		return -ENOMEM;

	ps->i2c = client;
	ps->dev = dev;

	sw_desc.drvdata = ps;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = ps5169_switch_set;
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
	ps->sw = mtk_typec_switch_register(dev, &sw_desc);
#else
	ps->sw = typec_switch_register(dev, &sw_desc);
#endif
	if (IS_ERR(ps->sw)) {
		dev_info(dev, "error registering typec switch: %ld\n",
			PTR_ERR(ps->sw));
		return PTR_ERR(ps->sw);
	}

	i2c_set_clientdata(client, ps);

	ret = ps5169_pinctrl_init(ps);
	if (ret < 0) {
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
		mtk_typec_switch_unregister(ps->sw);
#else
		mtk_typec_switch_unregister(ps->sw);
#endif
	}
	/* switch off after init done */
	ps5169_switch_set(ps->sw, TYPEC_ORIENTATION_NONE);

	dev_info(dev, "%s done\n", __func__);
	return ret;
}

static int ps5169_remove(struct i2c_client *client)
{
	struct ps5169 *ps = i2c_get_clientdata(client);

	mtk_typec_switch_unregister(ps->sw);
	return 0;
}

static const struct i2c_device_id ps5169_table[] = {
	{ "ps5169" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ps5169_table);

static const struct of_device_id ps5169_of_match[] = {
	{.compatible = "parade,ps5169"},
	{ },
};
MODULE_DEVICE_TABLE(of, ps5169_of_match);

static struct i2c_driver ps5169_driver = {
	.driver = {
		.name = "ps5169",
		.of_match_table = ps5169_of_match,
	},
	.probe_new = ps5169_probe,
	.remove	= ps5169_remove,
	.id_table = ps5169_table,
};
module_i2c_driver(ps5169_driver);

MODULE_DESCRIPTION("PS5169 Type-C Redriver");
MODULE_LICENSE("GPL v2");
