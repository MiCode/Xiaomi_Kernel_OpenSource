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
#include "tcpm.h"

#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
#include "mux_switch.h"
#endif

struct ps5170 {
	struct device *dev;
	struct i2c_client *i2c;
	struct typec_switch *sw;
	struct typec_mux *mux;
	struct pinctrl *pinctrl;
	struct pinctrl_state *enable;
	struct pinctrl_state *disable;
	struct mutex lock;
};

#define ps5170_ORIENTATION_NONE                 0x80

/* USB Only */
#define ps5170_ORIENTATION_NORMAL               0xc0
#define ps5170_ORIENTATION_FLIP                 0xd0
/* DP Only */
#define ps5170_ORIENTATION_NORMAL_DP            0xa0
#define ps5170_ORIENTATION_FLIP_DP              0xb0
/* USB + DP */
#define ps5170_ORIENTATION_NORMAL_USBDP         0xe0
#define ps5170_ORIENTATION_FLIP_USBDP           0xf0

static int ps5170_init(struct ps5170 *ps)
{
	/* Configure PS5170 redriver */

	i2c_smbus_write_byte_data(ps->i2c, 0x9d, 0x80);
	/* add a delay */
	mdelay(20);
	i2c_smbus_write_byte_data(ps->i2c, 0x9d, 0x00);
	/* auto power down */
	i2c_smbus_write_byte_data(ps->i2c, 0x40, 0x80);
	/* Force AUX RX data reverse */
	i2c_smbus_write_byte_data(ps->i2c, 0x9F, 0x02);
	/* Fine tune LFPS swing */
	i2c_smbus_write_byte_data(ps->i2c, 0x8d, 0x01);
	/* Fine tune LFPS swing */
	i2c_smbus_write_byte_data(ps->i2c, 0x90, 0x01);
	i2c_smbus_write_byte_data(ps->i2c, 0x51, 0x87);
	i2c_smbus_write_byte_data(ps->i2c, 0x50, 0x20);
	i2c_smbus_write_byte_data(ps->i2c, 0x54, 0x11);
	i2c_smbus_write_byte_data(ps->i2c, 0x5d, 0x66);
	i2c_smbus_write_byte_data(ps->i2c, 0x52, 0x20);
	i2c_smbus_write_byte_data(ps->i2c, 0x55, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x56, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x57, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x58, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x59, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x5a, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x5b, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x5e, 0x06);
	i2c_smbus_write_byte_data(ps->i2c, 0x5f, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x60, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x61, 0x03);
	i2c_smbus_write_byte_data(ps->i2c, 0x65, 0x40);
	i2c_smbus_write_byte_data(ps->i2c, 0x66, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x67, 0x03);
	i2c_smbus_write_byte_data(ps->i2c, 0x75, 0x0C);
	i2c_smbus_write_byte_data(ps->i2c, 0x77, 0x00);
	i2c_smbus_write_byte_data(ps->i2c, 0x78, 0x7C);
	return 0;
}

/*
 * 0 USB only mode        NORMAL
 * 1 USB only mode        FLIP
 * 2 DP only mode 4-lane  NORMAL
 * 3 DP only mode 4-lane  FLIP
 * 4 DP 2 lane + USB mode NORMAL
 * 5 DP 2 lane + USB mode FLIP
 * polarity == 0 NORMAL
 */

static int ps5170_set_conf(struct ps5170 *ps, u8 new_conf, u8 polarity)
{

	if (ps->enable) {
		pinctrl_select_state(ps->pinctrl, ps->enable);
		mdelay(20);
	}
	ps5170_init(ps);
	mdelay(20);

	switch (new_conf) {
	case 2:
		/* DP Only mode 4-lane set orientation*/
		if (!polarity)
			i2c_smbus_write_byte_data(ps->i2c, 0x40, ps5170_ORIENTATION_NORMAL_DP);
		else
			i2c_smbus_write_byte_data(ps->i2c, 0x40, ps5170_ORIENTATION_FLIP_DP);

		/* Enable AUX channel */
		i2c_smbus_write_byte_data(ps->i2c, 0xa0, 0x00);
		/* HPD */
		i2c_smbus_write_byte_data(ps->i2c, 0xa1, 0x04);
		break;
	case 4:
		/*  DP 2 lane + USB mode set orientation*/
		if (!polarity)
			i2c_smbus_write_byte_data(ps->i2c, 0x40, ps5170_ORIENTATION_NORMAL_USBDP);
		else
			i2c_smbus_write_byte_data(ps->i2c, 0x40, ps5170_ORIENTATION_FLIP_USBDP);

		/* Enable AUX channel */
		i2c_smbus_write_byte_data(ps->i2c, 0xa0, 0x00);
		/* HPD */
		i2c_smbus_write_byte_data(ps->i2c, 0xa1, 0x04);
		break;
	default:
		/* switch off */
		i2c_smbus_write_byte_data(ps->i2c, 0x40, 0x80);
		/* Disable AUX channel */
		i2c_smbus_write_byte_data(ps->i2c, 0xa0, 0x02);
		/* HPD low */
		i2c_smbus_write_byte_data(ps->i2c, 0xa1, 0x00);
		if (ps->disable)
			pinctrl_select_state(ps->pinctrl, ps->disable);
		break;
	}

	return 0;
}

static int ps5170_switch_set(struct typec_switch *sw,
			enum typec_orientation orientation)
{
	struct ps5170 *ps = typec_switch_get_drvdata(sw);

	dev_info(ps->dev, "%s %d\n", __func__, orientation);

	switch (orientation) {
	case TYPEC_ORIENTATION_NONE:
		/* switch off */
		i2c_smbus_write_byte_data(ps->i2c, 0x40, 0x80);
		if (ps->disable)
			pinctrl_select_state(ps->pinctrl, ps->disable);
		break;
	case TYPEC_ORIENTATION_NORMAL:
		/* switch cc1 side */
		if (ps->enable) {
			pinctrl_select_state(ps->pinctrl, ps->enable);
			mdelay(20);
		}
		ps5170_init(ps);
		/* NORMAL Side */
		i2c_smbus_write_byte_data(ps->i2c, 0x40,
			ps5170_ORIENTATION_NORMAL);
		break;
	case TYPEC_ORIENTATION_REVERSE:
		/* switch cc2 side */
		if (ps->enable) {
			pinctrl_select_state(ps->pinctrl, ps->enable);
			mdelay(20);
		}
		ps5170_init(ps);
		/* FLIP Side */
		i2c_smbus_write_byte_data(ps->i2c, 0x40,
			ps5170_ORIENTATION_FLIP);
		break;
	default:
		break;
	}

	return 0;
}

/*
 * case
 *  4 Pin Assignment C 4-lans
 * 16 Pin Assignment E 4-lans
 *  8 Pin Assignment D 2-lans
 * 32 Pin Assignment F 2-lans
 */

static int ps5170_mux_set(struct typec_mux *mux, struct typec_mux_state *state)
{
	struct ps5170 *ps = typec_mux_get_drvdata(mux);
	struct tcp_notify *data = state->data;
	int ret = 0;

	mutex_lock(&ps->lock);

	/* Debug Message
	 *dev_info(ps->dev, "ps5170_mux_set\n");
	 *dev_info(ps->dev, "EVENT = %lu", data->event_type);
	 *dev_info(ps->dev, "state->mode : %d\n", state->mode);
	 *dev_info(ps->dev, "data-> polarity : %d\n", data->ama_dp_state.polarity);
	 *dev_info(ps->dev, "data-> signal : %d\n", data->ama_dp_state.signal);
	 *dev_info(ps->dev, "data-> pin_assignment : %d\n", data->ama_dp_state.pin_assignment);
	 *dev_info(ps->dev, "data-> active : %d\n", data->ama_dp_state.active);
	 */

	if (state->mode == TCP_NOTIFY_AMA_DP_STATE) {
		switch (data->ama_dp_state.pin_assignment) {
		case 4:
		case 16:
			ps5170_set_conf(ps, 2, data->ama_dp_state.polarity);
			break;
		case 8:
		case 32:
			ps5170_set_conf(ps, 4, data->ama_dp_state.polarity);
			break;
		default:
			/* dev_info(ps->dev, "%s Pin Assignment not support\n", __func__,); */
			break;
		}
	} else if (state->mode == TCP_NOTIFY_AMA_DP_HPD_STATE) {
		/* uint8_t irq = data->ama_dp_hpd_state.irq; */
		/* uint8_t state = data->ama_dp_hpd_state.state; */
		/* Call HPD Event Not Ready */
	} else if (state->mode == TCP_NOTIFY_TYPEC_STATE) {
		if ((data->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			data->typec_state.old_state == TYPEC_ATTACHED_SNK) &&
			data->typec_state.new_state == TYPEC_UNATTACHED) {
			/* Call DP Event API Not Ready */
			ps5170_set_conf(ps, 0, 0);
		}
	}

	mutex_unlock(&ps->lock);

	return ret;
}

static int ps5170_pinctrl_init(struct ps5170 *ps)
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

static int ps5170_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct typec_switch_desc sw_desc;
	struct typec_mux_desc mux_desc;
	struct ps5170 *ps;
	int ret = 0;

	ps = devm_kzalloc(dev, sizeof(*ps), GFP_KERNEL);
	if (!ps)
		return -ENOMEM;

	ps->i2c = client;
	ps->dev = dev;

	/* Setting Switch callback */
	sw_desc.drvdata = ps;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = ps5170_switch_set;
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

	/* Setting MUX callback */
	mux_desc.drvdata = ps;
	mux_desc.fwnode = dev->fwnode;
	mux_desc.set = ps5170_mux_set;
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
	ps->mux = mtk_typec_mux_register(dev, &mux_desc);
#else
	ps->mux = typec_switch_register(dev, &mux_desc);
#endif
	if (IS_ERR(ps->mux)) {
		dev_info(dev, "error registering typec mux: %ld\n",
			PTR_ERR(ps->mux));
		return PTR_ERR(ps->mux);
	}

	i2c_set_clientdata(client, ps);

	ret = ps5170_pinctrl_init(ps);
	if (ret < 0) {
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
		mtk_typec_switch_unregister(ps->sw);
#else
		mtk_typec_switch_unregister(ps->sw);
#endif
	}
	/* switch off after init done */
	ps5170_switch_set(ps->sw, TYPEC_ORIENTATION_NONE);
	dev_info(dev, "%s done\n", __func__);
	return ret;
}

static int ps5170_remove(struct i2c_client *client)
{
	struct ps5170 *ps = i2c_get_clientdata(client);

	mtk_typec_switch_unregister(ps->sw);
	typec_mux_unregister(ps->mux);
	/* typec_switch_unregister(pi->sw); */
	return 0;
}

static const struct i2c_device_id ps5170_table[] = {
	{ "ps5170" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ps5170_table);

static const struct of_device_id ps5170_of_match[] = {
	{.compatible = "parade,ps5170"},
	{ },
};
MODULE_DEVICE_TABLE(of, ps5170_of_match);

static struct i2c_driver ps5170_driver = {
	.driver = {
		.name = "ps5170",
		.of_match_table = ps5170_of_match,
	},
	.probe_new = ps5170_probe,
	.remove	= ps5170_remove,
	.id_table = ps5170_table,
};
module_i2c_driver(ps5170_driver);

MODULE_DESCRIPTION("ps5170 Type-C Redriver");
MODULE_LICENSE("GPL v2");
