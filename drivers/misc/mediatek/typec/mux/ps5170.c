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
#include <linux/atomic.h>
#include <linux/interrupt.h>
#include "tcpm.h"

#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
#include "mux_switch.h"
#endif

#define CHECK_HPD_DELAY 2000

struct ps5170 {
	struct device *dev;
	struct i2c_client *i2c;
	struct typec_switch *sw;
	struct typec_mux *mux;
	struct pinctrl *pinctrl;
	struct pinctrl_state *enable;
	struct pinctrl_state *disable;
	struct mutex lock;
	struct typec_mux_state *dp_state;
	struct tcp_notify dp_data;
	int mode;
	int hdp_state;
	bool dp_sw_connect;

	struct work_struct set_usb_work;
	struct work_struct set_dp_work;
	struct delayed_work check_wk;

	enum typec_orientation orientation;
	struct work_struct reconfig_dp_work;

	uint8_t pin_assign;
	u8 polarity;
	atomic_t in_sleep;
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
	/* i2c_smbus_write_byte_data(ps->i2c, 0x8d, 0x01); */
	/* Fine tune LFPS swing */
	/* i2c_smbus_write_byte_data(ps->i2c, 0x90, 0x01); */
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

	ps->pin_assign = new_conf;
	ps->polarity = polarity;

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

static void ps5170_switch_set_work(struct work_struct *data)
{

	struct ps5170 *ps = container_of(data, struct ps5170, set_usb_work);

	if (atomic_read(&ps->in_sleep)) {
		dev_info(ps->dev, "%s in sleep\n", __func__);
		schedule_work(&ps->set_usb_work);
		return;
	}

	switch (ps->orientation) {
	case TYPEC_ORIENTATION_NONE:
		/* switch off */
		i2c_smbus_write_byte_data(ps->i2c, 0x40, 0x80);
		if (ps->disable)
			pinctrl_select_state(ps->pinctrl, ps->disable);
		ps->pin_assign = 0;
		break;
	case TYPEC_ORIENTATION_NORMAL:
		/* switch cc1 side */
		if (ps->enable) {
			pinctrl_select_state(ps->pinctrl, ps->enable);
			mdelay(20);
		}
		ps5170_init(ps);
		/* FLIP Side */
		i2c_smbus_write_byte_data(ps->i2c, 0x40,
			ps5170_ORIENTATION_FLIP);
		break;
	case TYPEC_ORIENTATION_REVERSE:
		/* switch cc2 side */
		if (ps->enable) {
			pinctrl_select_state(ps->pinctrl, ps->enable);
			mdelay(20);
		}
		ps5170_init(ps);
		/* NORMAL Side */
		i2c_smbus_write_byte_data(ps->i2c, 0x40,
			ps5170_ORIENTATION_NORMAL);
		break;
	default:
		break;
	}

}

static int ps5170_switch_set(struct typec_switch *sw,
			enum typec_orientation orientation)
{
	struct ps5170 *ps = typec_switch_get_drvdata(sw);

	dev_info(ps->dev, "%s %d\n", __func__, orientation);

	ps->orientation = orientation;
	schedule_work(&ps->set_usb_work);
	return 0;
}

/*
 * case
 *  4 Pin Assignment C 4-lans
 * 16 Pin Assignment E 4-lans
 *  8 Pin Assignment D 2-lans
 * 32 Pin Assignment F 2-lans
 */

static void ps5170_reconfig_dp_work(struct work_struct *data)
{
	struct ps5170 *ps = container_of(data, struct ps5170, reconfig_dp_work);

	dev_info(ps->dev, "Reconfig DP channel, pin_assign = %d, polarity = %d.\n"
				, ps->pin_assign, ps->polarity);

	ps5170_set_conf(ps, ps->pin_assign, ps->polarity);

}

static void ps5170_mux_set_work(struct work_struct *data)
{

	struct ps5170 *ps = container_of(data, struct ps5170, set_dp_work);
	struct tcp_notify dp_data = ps->dp_data;

	 /*Debug Message
	  *dev_info(ps->dev, "ps5170_mux_set\n");
	  *dev_info(ps->dev, "state->mode : %d\n", state->mode);
	  *dev_info(ps->dev, "ps->mode : %d\n", ps->mode);
	  *dev_info(ps->dev, "data-> polarity : %d\n", dp_data.ama_dp_state.polarity);
	  *dev_info(ps->dev, "data-> signal : %d\n", dp_data.ama_dp_state.signal);
	  *dev_info(ps->dev, "data-> pin_assignment : %d\n", dp_data.ama_dp_state.pin_assignment);
	  *dev_info(ps->dev, "data-> active : %d\n", dp_data.ama_dp_state.active);
	  */

	if (atomic_read(&ps->in_sleep)) {
		dev_info(ps->dev, "%s in sleep\n", __func__);
		schedule_work(&ps->set_dp_work);
		return;
	}

	if (ps->mode == TCP_NOTIFY_AMA_DP_STATE) {

		if (!dp_data.ama_dp_state.active) {
			dev_info(ps->dev, "%s Not active\n", __func__);
			return;
		}

		switch (dp_data.ama_dp_state.pin_assignment) {
		case 4:
		case 16:
			ps5170_set_conf(ps, 2, dp_data.ama_dp_state.polarity);
			break;
		case 8:
		case 32:
			ps5170_set_conf(ps, 4, dp_data.ama_dp_state.polarity);
			break;
		default:
			dev_info(ps->dev, "%s Pin Assignment not support\n", __func__);
			break;
		}

		ps->hdp_state = 0;
		schedule_delayed_work(&ps->check_wk, msecs_to_jiffies(CHECK_HPD_DELAY));
	} else if (ps->mode == TCP_NOTIFY_AMA_DP_HPD_STATE) {
		uint8_t irq = dp_data.ama_dp_hpd_state.irq;
		uint8_t state = dp_data.ama_dp_hpd_state.state;

		dev_info(ps->dev, "TCP_NOTIFY_AMA_DP_HPD_STATE irq:%x state:%x\n",
			irq, state);

		ps->hdp_state = state;

		/* Call DP API */
		dev_info(ps->dev, "[%s][%d]\n", __func__, __LINE__);
		if (state) {
			if (irq) {
				if (ps->dp_sw_connect == false) {
					dev_info(ps->dev, "Force connect\n");
					mtk_dp_SWInterruptSet(0x4);
					ps->dp_sw_connect = true;
				}
				mtk_dp_SWInterruptSet(0x8);
			} else {
				mtk_dp_SWInterruptSet(0x4);
				ps->dp_sw_connect = true;
			}
		} else {
			mtk_dp_SWInterruptSet(0x2);
			ps->dp_sw_connect = false;
		}
	} else if (ps->mode == TCP_NOTIFY_TYPEC_STATE) {
		if ((dp_data.typec_state.old_state == TYPEC_ATTACHED_SRC ||
			dp_data.typec_state.old_state == TYPEC_ATTACHED_SNK) &&
			dp_data.typec_state.new_state == TYPEC_UNATTACHED) {
			/* Call DP Event API Ready */
			dev_info(ps->dev, "Plug Out\n");
			mtk_dp_SWInterruptSet(0x2);
			ps->dp_sw_connect = false;
			ps5170_set_conf(ps, 0, 0);
		}
	}

}

static int ps5170_mux_set(struct typec_mux *mux, struct typec_mux_state *state)
{
	struct ps5170 *ps = typec_mux_get_drvdata(mux);
	struct tcp_notify *data = state->data;

	/*dev_info(ps->dev, "B ps5170_mux_set\n");
	 *dev_info(ps->dev, "B state->mode : %d\n", state->mode);
	 *dev_info(ps->dev, "B data-> polarity : %d\n", data->ama_dp_state.polarity);
	 *dev_info(ps->dev, "B data-> signal : %d\n", data->ama_dp_state.signal);
	 *dev_info(ps->dev, "B data-> pin_assignment : %d\n", data->ama_dp_state.pin_assignment);
	 *dev_info(ps->dev, "B data-> active : %d\n", data->ama_dp_state.active);
	 */

	if (data == NULL) {
		dev_info(ps->dev, "%s data is NULL, reject.\n", __func__);
		return 0;
	}

	/* ama_dp_state */
	ps->dp_data.ama_dp_state.polarity  = data->ama_dp_state.polarity;
	ps->dp_data.ama_dp_state.signal = data->ama_dp_state.signal;
	ps->dp_data.ama_dp_state.pin_assignment = data->ama_dp_state.pin_assignment;
	ps->dp_data.ama_dp_state.active = data->ama_dp_state.active;

	/* ama_dp_hpd */
	ps->dp_data.ama_dp_hpd_state.irq = data->ama_dp_hpd_state.irq;
	ps->dp_data.ama_dp_hpd_state.state = data->ama_dp_hpd_state.state;

	/* typec_state  */
	ps->dp_data.typec_state.old_state = data->typec_state.old_state;
	ps->dp_data.typec_state.new_state = data->typec_state.new_state;

	ps->mode = state->mode;
	schedule_work(&ps->set_dp_work);
	return 0;
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

static void check_hpd(struct work_struct *work)
{
	struct delayed_work *check_wk = to_delayed_work(work);
	struct ps5170 *ps = container_of(check_wk, struct ps5170, check_wk);

	if (ps->hdp_state == 0) {
		dev_info(ps->dev, "%s: force hpd\n", __func__);
		mtk_dp_SWInterruptSet(0x4);
	}
}

static int ps5170_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct typec_switch_desc sw_desc = { };
	struct typec_mux_desc mux_desc = { };
	struct ps5170 *ps;
	int ret = 0;

	ps = devm_kzalloc(dev, sizeof(*ps), GFP_KERNEL);
	if (!ps)
		return -ENOMEM;

	ps->i2c = client;
	ps->dev = dev;
	ps->dp_sw_connect = false;
	ps->pin_assign = 0;

	atomic_set(&ps->in_sleep, 0);

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
		return ret;
	}

	INIT_WORK(&ps->set_usb_work, ps5170_switch_set_work);
	INIT_WORK(&ps->set_dp_work, ps5170_mux_set_work);
	INIT_WORK(&ps->reconfig_dp_work, ps5170_reconfig_dp_work);
	INIT_DEFERRABLE_WORK(&ps->check_wk, check_hpd);

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

static int __maybe_unused ps5170_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(i2c->irq);
	return 0;
}

static int __maybe_unused ps5170_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(i2c->irq);
	return 0;
}

static int ps5170_suspend_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct ps5170 *data = i2c_get_clientdata(i2c);

	atomic_set(&data->in_sleep, 1);

	/* pull low en pin to enter deep idle mode */
	pinctrl_select_state(data->pinctrl, data->disable);
	return 0;
}

static int ps5170_resume_noirq(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct ps5170 *data = i2c_get_clientdata(i2c);

	atomic_set(&data->in_sleep, 0);

	if (data->pin_assign) {
		schedule_work(&data->reconfig_dp_work);
	} else {
		/* pull high en pin to enter normal mode */
		pinctrl_select_state(data->pinctrl, data->enable);
	}

	return 0;
}

static const struct dev_pm_ops ps5170_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ps5170_suspend, ps5170_resume)
		SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(ps5170_suspend_noirq,
			ps5170_resume_noirq)
};

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
		.pm = &ps5170_pm_ops,
		.of_match_table = ps5170_of_match,
	},
	.probe_new = ps5170_probe,
	.remove	= ps5170_remove,
	.id_table = ps5170_table,
};
module_i2c_driver(ps5170_driver);

MODULE_DESCRIPTION("ps5170 Type-C Redriver");
MODULE_LICENSE("GPL v2");
