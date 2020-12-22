// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/extcon.h>
#include <linux/power_supply.h>
#include <linux/usb/ch9.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/ctype.h>

/* priority: INT_MAX >= x >= 0 */
#define NOTIFIER_PRIORITY		1

/* Registers Address */
#define GEN_DEV_SET_REG			0x00
#define CHIP_VERSION_REG		0x17

#define REDRIVER_REG_MAX		0x1f

#define EQ_SET_REG_BASE			0x01
#define FLAT_GAIN_REG_BASE		0x18
#define OUT_COMP_AND_POL_REG_BASE	0x02
#define LOSS_MATCH_REG_BASE		0x19

/* Default Register Value */
#define GEN_DEV_SET_REG_DEFAULT		0xFB

/* Register bits */
/* General Device Settings Register Bits */
#define CHIP_EN		BIT(0)
#define CHNA_EN		BIT(4)
#define CHNB_EN		BIT(5)
#define CHNC_EN		BIT(6)
#define CHND_EN		BIT(7)

#define CHANNEL_NUM		4

#define OP_MODE_SHIFT		1

#define EQ_SETTING_MASK			0x07
#define OUTPUT_COMPRESSION_MASK		0x03
#define LOSS_MATCH_MASK			0x03
#define FLAT_GAIN_MASK			0x03

#define EQ_SETTING_SHIFT		0x01
#define OUTPUT_COMPRESSION_SHIFT	0x01
#define LOSS_MATCH_SHIFT		0x00
#define FLAT_GAIN_SHIFT			0x00

#define CHNA_INDEX		0
#define CHNB_INDEX		1
#define CHNC_INDEX		2
#define CHND_INDEX		3

#define CHAN_MODE_NUM		2

/* for type c cable */
enum plug_orientation {
	ORIENTATION_NONE,
	ORIENTATION_CC1,
	ORIENTATION_CC2,
};

/*
 * Three Modes of Operations:
 *  - One/Two ports of USB 3.1 Gen1/Gen2 (Default Mode)
 *  - Two lanes of DisplayPort 1.4 + One port of USB 3.1 Gen1/Gen2
 *  - Four lanes of DisplayPort 1.4
 */
enum operation_mode {
	OP_MODE_USB,	/* One/Two ports of USB */
	OP_MODE_DP,		/* DP 4 Lane and DP 2 Lane */
	OP_MODE_USB_AND_DP, /* One port of USB and DP 2 Lane */
};

/*
 * USB redriver channel mode:
 *  - USB mode
 *  - DP mode
 */
enum channel_mode {
	CHAN_MODE_USB,
	CHAN_MODE_DP,
};

/**
 * struct ssusb_redriver - representation of USB re-driver
 * @dev: struct device pointer
 * @regmap: used for I2C communication on accessing registers
 * @client: i2c client structure pointer
 * @config_work: used to configure re-driver
 * @redriver_wq: work queue used for @config_work
 * @usb_psy: structure that holds USB power supply status
 * @host_active: used to indicate USB host mode is enabled or not
 * @vbus_active: used to indicate USB device mode is enabled or not
 * @is_usb3: used to indicate USB3 or not
 * @typec_orientation: used to inditate Type C orientation
 * @op_mode: used to store re-driver operation mode
 * @extcon_usb: external connector used for USB host/device mode
 * @extcon_dp: external connector used for DP
 * @vbus_nb: used for vbus event reception
 * @id_nb: used for id event reception
 * @dp_nb: used for DP event reception
 * @panic_nb: used for panic event reception
 * @chan_mode: used to indicate re-driver's channel mode
 * @eq: equalization register value.
 *      eq[0] - eq[3]: Channel A-D parameter for USB
 *      eq[4] - eq[7]: Channel A-D parameter for DP
 * @output_comp: output compression register value
 *      output_comp[0] - output_comp[3]: Channel A-D parameter for USB
 *      output_comp[4] - output_comp[7]: Channel A-D parameter for DP
 * @loss_match: loss profile matching control register value
 *      loss_match[0] - loss_match[3]: Channel A-D parameter for USB
 *      loss_match[4] - loss_match[7]: Channel A-D parameter for DP
 * @flat_gain: flat gain control register value
 *      flat_gain[0] - flat_gain[3]: Channel A-D parameter for USB
 *      flat_gain[4] - flat_gain[7]: Channel A-D parameter for DP
 * @debug_root: debugfs entry for this context
 */
struct ssusb_redriver {
	struct device		*dev;
	struct regmap		*regmap;
	struct i2c_client	*client;

	struct work_struct	config_work;
	struct workqueue_struct *redriver_wq;

	struct power_supply	*usb_psy;
	bool host_active;
	bool vbus_active;
	bool is_usb3;
	enum plug_orientation typec_orientation;
	enum operation_mode op_mode;

	struct extcon_dev	*extcon_usb;
	struct extcon_dev	*extcon_dp;
	struct notifier_block	vbus_nb;
	struct notifier_block	id_nb;
	struct notifier_block	dp_nb;

	struct notifier_block	panic_nb;

	enum	channel_mode chan_mode[CHANNEL_NUM];

	u8	eq[CHAN_MODE_NUM][CHANNEL_NUM];
	u8	output_comp[CHAN_MODE_NUM][CHANNEL_NUM];
	u8	loss_match[CHAN_MODE_NUM][CHANNEL_NUM];
	u8	flat_gain[CHAN_MODE_NUM][CHANNEL_NUM];

	u8	gen_dev_val;

	struct dentry	*debug_root;
};

static int ssusb_redriver_channel_update(struct ssusb_redriver *redriver);
static void ssusb_redriver_debugfs_entries(struct ssusb_redriver *redriver);

static int redriver_i2c_reg_get(struct ssusb_redriver *redriver,
		u8 reg, u8 *val)
{
	int ret;
	unsigned int val_tmp;

	ret = regmap_read(redriver->regmap, (unsigned int)reg, &val_tmp);
	if (ret < 0) {
		dev_err(redriver->dev, "reading reg 0x%02x failure\n", reg);
		return ret;
	}

	*val = (u8)val_tmp;

	dev_dbg(redriver->dev, "reading reg 0x%02x=0x%02x\n", reg, *val);

	return 0;
}

static int redriver_i2c_reg_set(struct ssusb_redriver *redriver,
		u8 reg, u8 val)
{
	int ret;

	ret = regmap_write(redriver->regmap, (unsigned int)reg,
			(unsigned int)val);
	if (ret < 0) {
		dev_err(redriver->dev, "writing reg 0x%02x failure\n", reg);
		return ret;
	}

	dev_dbg(redriver->dev, "writing reg 0x%02x=0x%02x\n", reg, val);

	return 0;
}

/**
 * Handle Re-driver chip operation mode and channel settings.
 *
 * Three Modes of Operations:
 *  - One/Two ports of USB 3.1 Gen1/Gen2 (Default Mode)
 *  - Two lanes of DisplayPort 1.4 + One port of USB 3.1 Gen1/Gen2
 *  - Four lanes of DisplayPort 1.4
 *
 * @redriver - contain redriver status
 * @on - re-driver chip enable or not
 */
static void ssusb_redriver_gen_dev_set(
		struct ssusb_redriver *redriver, bool on)
{
	int ret;
	u8 val, oldval;

	val = 0;

	switch (redriver->op_mode) {
	case OP_MODE_USB:
		/* Use source side I/O mapping */
		if (redriver->typec_orientation
				== ORIENTATION_CC1) {
			/* Enable channel C and D */
			val &= ~(CHNA_EN | CHNB_EN);
			val |= (CHNC_EN | CHND_EN);
		} else if (redriver->typec_orientation
				== ORIENTATION_CC2) {
			/* Enable channel A and B*/
			val |= (CHNA_EN | CHNB_EN);
			val &= ~(CHNC_EN | CHND_EN);
		} else {
			/* Enable channel A, B, C and D */
			val |= (CHNA_EN | CHNB_EN);
			val |= (CHNC_EN | CHND_EN);
		}

		/* Set to default USB Mode */
		val |= (0x5 << OP_MODE_SHIFT);

		break;
	case OP_MODE_DP:
		/* Enable channel A, B, C and D */
		val |= (CHNA_EN | CHNB_EN);
		val |= (CHNC_EN | CHND_EN);

		/* Set to DP 4 Lane Mode (OP Mode 2) */
		val |= (0x2 << OP_MODE_SHIFT);

		break;
	case OP_MODE_USB_AND_DP:
		/* Enable channel A, B, C and D */
		val |= (CHNA_EN | CHNB_EN);
		val |= (CHNC_EN | CHND_EN);

		if (redriver->typec_orientation
				== ORIENTATION_CC1)
			/* Set to DP 4 Lane Mode (OP Mode 1) */
			val |= (0x1 << OP_MODE_SHIFT);
		else if (redriver->typec_orientation
				== ORIENTATION_CC2)
			/* Set to DP 4 Lane Mode (OP Mode 0) */
			val |= (0x0 << OP_MODE_SHIFT);
		else {
			dev_err(redriver->dev,
				"can't get orientation, op mode %d\n",
				redriver->op_mode);
			goto err_exit;
		}

		break;
	default:
		dev_err(redriver->dev,
			"Error: op mode: %d, vbus: %d, host: %d.\n",
			redriver->op_mode, redriver->vbus_active,
			redriver->host_active);
		goto err_exit;
	}

	/* exit/enter deep-sleep power mode */
	oldval = redriver->gen_dev_val;
	if (on) {
		val |= CHIP_EN;
		if (val == oldval)
			return;
	} else {
		/* no operation if already disabled */
		if (oldval && !(oldval & CHIP_EN))
			return;

		val &= ~CHIP_EN;
	}
	ret = redriver_i2c_reg_set(redriver, GEN_DEV_SET_REG, val);
	if (ret < 0)
		goto err_exit;

	dev_dbg(redriver->dev,
		"successfully (%s) the redriver chip, reg 0x00 = 0x%x\n",
		on ? "ENABLE":"DISABLE", val);

	redriver->gen_dev_val = val;
	return;

err_exit:
	dev_err(redriver->dev,
		"failure to (%s) the redriver chip, reg 0x00 = 0x%x\n",
		on ? "ENABLE":"DISABLE", val);
}

static void ssusb_redriver_config_work(struct work_struct *w)
{
	struct ssusb_redriver *redriver = container_of(w,
			struct ssusb_redriver, config_work);
	struct extcon_dev *edev = NULL;
	union extcon_property_value val;
	unsigned int extcon_id = EXTCON_NONE;
	int ret = 0;

	dev_dbg(redriver->dev, "%s: USB SS redriver config work\n",
			__func__);

	edev = redriver->extcon_usb;

	if (redriver->vbus_active)
		extcon_id = EXTCON_USB;
	else if (redriver->host_active)
		extcon_id = EXTCON_USB_HOST;

	if (edev && (extcon_id != EXTCON_NONE)
			&& extcon_get_state(edev, extcon_id)) {
		ret = extcon_get_property(edev, extcon_id,
					EXTCON_PROP_USB_SS, &val);
		if (!ret) {
			redriver->is_usb3 = (val.intval != 0);

			dev_dbg(redriver->dev, "SS Lane is used? [%s].\n",
				redriver->is_usb3 ? "true" : "false");
		} else {
			redriver->is_usb3 = true;

			dev_dbg(redriver->dev, "Default true as speed isn't reported.\n");
		}

		if (redriver->is_usb3 || (redriver->op_mode != OP_MODE_USB)) {
			ret = extcon_get_property(edev, extcon_id,
					EXTCON_PROP_USB_TYPEC_POLARITY, &val);
			if (!ret)
				redriver->typec_orientation = val.intval ?
					ORIENTATION_CC2 : ORIENTATION_CC1;
			else if (redriver->op_mode == OP_MODE_USB)
				redriver->typec_orientation = ORIENTATION_NONE;
			else
				dev_err(redriver->dev, "fail to get orientation when has DP.\n");

			ssusb_redriver_gen_dev_set(redriver, true);
		} else {
			dev_dbg(redriver->dev,
				"Disable chip when not in SS USB mode.\n");

			ssusb_redriver_gen_dev_set(redriver, false);
		}

		dev_dbg(redriver->dev, "Type C orientation code is %d.\n",
				redriver->typec_orientation);
	} else if (redriver->op_mode != OP_MODE_USB) {
		/*
		 * USB host stack will be turned off if peer doesn't
		 * support USB communication. PD driver will send
		 * id notification when disable host stack. Update
		 * redriver channel mode when operation mode changed.
		 */
		dev_dbg(redriver->dev,
				"Update redriver operation mode.\n");

		ssusb_redriver_gen_dev_set(redriver, true);
	} else {
		dev_dbg(redriver->dev, "USB Cable is disconnected.\n");

		/* Set back to USB only mode when cable disconnect */
		redriver->op_mode = OP_MODE_USB;

		ssusb_redriver_gen_dev_set(redriver, false);
	}
}

static int ssusb_redriver_dp_notifier(struct notifier_block *nb,
		unsigned long dp_lane, void *ptr)
{
	struct ssusb_redriver *redriver = container_of(nb,
			struct ssusb_redriver, dp_nb);
	enum operation_mode op_mode;
	int ret = 0;

	dev_dbg(redriver->dev,
		"redriver op mode change: %ld event received\n", dp_lane);

	switch (dp_lane) {
	case 0:
		op_mode = OP_MODE_USB;
		redriver->chan_mode[CHNA_INDEX] = CHAN_MODE_USB;
		redriver->chan_mode[CHNB_INDEX] = CHAN_MODE_USB;
		redriver->chan_mode[CHNC_INDEX] = CHAN_MODE_USB;
		redriver->chan_mode[CHND_INDEX] = CHAN_MODE_USB;
		break;
	case 2:
		op_mode = OP_MODE_USB_AND_DP;
		if (redriver->typec_orientation == ORIENTATION_CC1) {
			redriver->chan_mode[CHNA_INDEX] = CHAN_MODE_DP;
			redriver->chan_mode[CHNB_INDEX] = CHAN_MODE_DP;
			redriver->chan_mode[CHNC_INDEX] = CHAN_MODE_USB;
			redriver->chan_mode[CHND_INDEX] = CHAN_MODE_USB;
		} else {
			redriver->chan_mode[CHNA_INDEX] = CHAN_MODE_USB;
			redriver->chan_mode[CHNB_INDEX] = CHAN_MODE_USB;
			redriver->chan_mode[CHNC_INDEX] = CHAN_MODE_DP;
			redriver->chan_mode[CHND_INDEX] = CHAN_MODE_DP;
		}
		break;
	case 4:
		op_mode = OP_MODE_DP;
		redriver->chan_mode[CHNA_INDEX] = CHAN_MODE_DP;
		redriver->chan_mode[CHNB_INDEX] = CHAN_MODE_DP;
		redriver->chan_mode[CHNC_INDEX] = CHAN_MODE_DP;
		redriver->chan_mode[CHND_INDEX] = CHAN_MODE_DP;
		break;
	default:
		return 0;
	}

	if (redriver->op_mode == op_mode)
		return 0;

	redriver->op_mode = op_mode;

	ret = ssusb_redriver_channel_update(redriver);
	if (ret)
		dev_dbg(redriver->dev,
			"redriver channel mode change will continue\n");

	queue_work(redriver->redriver_wq, &redriver->config_work);

	return 0;
}

static int ssusb_redriver_vbus_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct ssusb_redriver *redriver = container_of(nb,
			struct ssusb_redriver, vbus_nb);

	dev_dbg(redriver->dev, "vbus:%ld event received\n", event);

	if (redriver->vbus_active == event)
		return NOTIFY_DONE;

	redriver->vbus_active = event;

	queue_work(redriver->redriver_wq, &redriver->config_work);

	return NOTIFY_DONE;
}

static int ssusb_redriver_id_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct ssusb_redriver *redriver = container_of(nb,
			struct ssusb_redriver, id_nb);
	bool host_active = (bool)event;

	dev_dbg(redriver->dev, "host_active:%s event received\n",
			host_active ? "true" : "false");

	if (redriver->host_active == host_active)
		return NOTIFY_DONE;

	redriver->host_active = host_active;

	queue_work(redriver->redriver_wq, &redriver->config_work);

	return NOTIFY_DONE;
}

static int ssusb_redriver_extcon_register(struct ssusb_redriver *redriver)
{
	struct device_node *node = redriver->dev->of_node;
	struct extcon_dev *edev;
	int ret = 0;

	if (!of_find_property(node, "extcon", NULL)) {
		dev_err(redriver->dev, "failed to get extcon for redriver\n");
		return 0;
	}

	edev = extcon_get_edev_by_phandle(redriver->dev, 0);
	if (IS_ERR(edev) && PTR_ERR(edev) != -ENODEV) {
		dev_err(redriver->dev, "failed to get phandle for redriver\n");
		return PTR_ERR(edev);
	}

	if (!IS_ERR(edev)) {
		redriver->extcon_usb = edev;

		redriver->vbus_nb.notifier_call = ssusb_redriver_vbus_notifier;
		redriver->vbus_nb.priority = NOTIFIER_PRIORITY;
		ret = extcon_register_notifier(edev, EXTCON_USB,
				&redriver->vbus_nb);
		if (ret < 0) {
			dev_err(redriver->dev,
				"failed to register notifier for redriver\n");
			return ret;
		}

		redriver->id_nb.notifier_call = ssusb_redriver_id_notifier;
		redriver->id_nb.priority = NOTIFIER_PRIORITY;
		ret = extcon_register_notifier(edev, EXTCON_USB_HOST,
				&redriver->id_nb);
		if (ret < 0) {
			dev_err(redriver->dev,
				"failed to register notifier for USB-HOST\n");
			goto err;
		}
	}

	edev = NULL;
	/* Use optional phandle (index 1) for DP lane events */
	if (of_count_phandle_with_args(node, "extcon", NULL) > 1) {
		edev = extcon_get_edev_by_phandle(redriver->dev, 1);
		if (IS_ERR(edev) && PTR_ERR(edev) != -ENODEV) {
			ret = PTR_ERR(edev);
			goto err1;
		}
	}

	if (!IS_ERR_OR_NULL(edev)) {
		redriver->extcon_dp = edev;
		redriver->dp_nb.notifier_call =
				ssusb_redriver_dp_notifier;
		redriver->dp_nb.priority = NOTIFIER_PRIORITY;
		ret = extcon_register_blocking_notifier(edev, EXTCON_DISP_DP,
				&redriver->dp_nb);
		if (ret < 0) {
			dev_err(redriver->dev,
				"failed to register blocking notifier\n");
			goto err1;
		}
	}

	/* Update initial VBUS/ID state from extcon */
	if (extcon_get_state(redriver->extcon_usb, EXTCON_USB))
		ssusb_redriver_vbus_notifier(&redriver->vbus_nb, true,
			redriver->extcon_usb);
	else if (extcon_get_state(redriver->extcon_usb, EXTCON_USB_HOST))
		ssusb_redriver_id_notifier(&redriver->id_nb, true,
				redriver->extcon_usb);

	return 0;

err1:
	if (redriver->extcon_usb)
		extcon_unregister_notifier(redriver->extcon_usb,
			EXTCON_USB_HOST, &redriver->id_nb);
err:
	if (redriver->extcon_usb)
		extcon_unregister_notifier(redriver->extcon_usb,
			EXTCON_USB, &redriver->vbus_nb);
	return ret;
}

static int ssusb_redriver_param_config(struct ssusb_redriver *redriver,
		u8 reg_base, u8 channel, u8 mask, u8 shift, u8 val,
		u8 (*stored_val)[CHANNEL_NUM])
{
	int i, j, ret = -EINVAL;
	u8 reg_addr, reg_val, real_channel, chan_mode;

	if (channel == CHANNEL_NUM * CHAN_MODE_NUM) {
		for (i = 0; i < CHAN_MODE_NUM; i++)
			for (j = 0; j < CHANNEL_NUM; j++) {
				if (redriver->chan_mode[j] == i) {
					reg_addr = reg_base + (j << 1);

					ret = redriver_i2c_reg_get(redriver,
							reg_addr, &reg_val);
					if (ret < 0)
						return ret;

					reg_val &= ~(mask << shift);
					reg_val |= (val << shift);

					ret = redriver_i2c_reg_set(redriver,
							reg_addr, reg_val);
					if (ret < 0)
						return ret;
				}

				stored_val[i][j] = val;
			}
	} else if (channel < CHANNEL_NUM * CHAN_MODE_NUM) {
		real_channel = channel % CHANNEL_NUM;
		chan_mode = channel / CHANNEL_NUM;

		if (redriver->chan_mode[real_channel] == chan_mode) {
			reg_addr = reg_base + (real_channel << 1);

			ret = redriver_i2c_reg_get(redriver,
					reg_addr, &reg_val);
			if (ret < 0)
				return ret;

			reg_val &= ~(mask << shift);
			reg_val |= (val << shift);

			ret = redriver_i2c_reg_set(redriver,
					reg_addr, reg_val);
			if (ret < 0)
				return ret;
		}

		stored_val[chan_mode][real_channel] = val;
	} else {
		dev_err(redriver->dev, "error channel value.\n");
		return ret;
	}

	return 0;
}

static int ssusb_redriver_eq_config(
		struct ssusb_redriver *redriver, u8 channel, u8 val)
{
	if (val <= EQ_SETTING_MASK)
		return ssusb_redriver_param_config(redriver,
				EQ_SET_REG_BASE, channel, EQ_SETTING_MASK,
				EQ_SETTING_SHIFT, val, redriver->eq);
	else
		return -EINVAL;
}

static int ssusb_redriver_flat_gain_config(
		struct ssusb_redriver *redriver, u8 channel, u8 val)
{
	if (val <= FLAT_GAIN_MASK)
		return ssusb_redriver_param_config(redriver,
				FLAT_GAIN_REG_BASE, channel, FLAT_GAIN_MASK,
				FLAT_GAIN_SHIFT, val, redriver->flat_gain);
	else
		return -EINVAL;
}

static int ssusb_redriver_output_comp_config(
		struct ssusb_redriver *redriver, u8 channel, u8 val)
{
	if (val <= OUTPUT_COMPRESSION_MASK)
		return ssusb_redriver_param_config(redriver,
				OUT_COMP_AND_POL_REG_BASE, channel,
				OUTPUT_COMPRESSION_MASK,
				OUTPUT_COMPRESSION_SHIFT, val,
				redriver->output_comp);
	else
		return -EINVAL;
}

static int ssusb_redriver_loss_match_config(
		struct ssusb_redriver *redriver, u8 channel, u8 val)
{
	if (val <= LOSS_MATCH_MASK)
		return ssusb_redriver_param_config(redriver,
				LOSS_MATCH_REG_BASE, channel, LOSS_MATCH_MASK,
				LOSS_MATCH_SHIFT, val, redriver->loss_match);
	else
		return -EINVAL;
}

static int ssusb_redriver_channel_update(struct ssusb_redriver *redriver)
{
	int ret = 0, i = 0, pos = 0;
	u8 chan_mode;

	for (i = 0; i < CHANNEL_NUM; i++) {
		chan_mode = redriver->chan_mode[i];
		pos = i + chan_mode * CHANNEL_NUM;

		ret = ssusb_redriver_eq_config(redriver, pos,
				redriver->eq[chan_mode][i]);
		if (ret)
			goto err;

		ret = ssusb_redriver_flat_gain_config(redriver, pos,
				redriver->flat_gain[chan_mode][i]);
		if (ret)
			goto err;

		ret = ssusb_redriver_output_comp_config(redriver, pos,
				redriver->output_comp[chan_mode][i]);
		if (ret)
			goto err;

		ret = ssusb_redriver_loss_match_config(redriver, pos,
				redriver->loss_match[chan_mode][i]);
		if (ret)
			goto err;
	}

	dev_dbg(redriver->dev, "redriver channel parameters updated.\n");

	return 0;

err:
	dev_err(redriver->dev, "channel parameters update failure.\n");
	return ret;
}

static int ssusb_redriver_default_config(struct ssusb_redriver *redriver)
{
	struct device_node *node = redriver->dev->of_node;
	int ret = 0;

	if (of_find_property(node, "eq", NULL)) {
		ret = of_property_read_u8_array(node, "eq",
				redriver->eq[0], sizeof(redriver->eq));
		if (ret)
			goto err;
	}

	if (of_find_property(node, "flat-gain", NULL)) {
		ret = of_property_read_u8_array(node,
				"flat-gain", redriver->flat_gain[0],
				sizeof(redriver->flat_gain));
		if (ret)
			goto err;
	}

	if (of_find_property(node, "output-comp", NULL)) {
		ret = of_property_read_u8_array(node,
				"output-comp", redriver->output_comp[0],
				sizeof(redriver->output_comp));
		if (ret)
			goto err;
	}

	if (of_find_property(node, "loss-match", NULL)) {
		ret = of_property_read_u8_array(node,
				"loss-match", redriver->loss_match[0],
				sizeof(redriver->loss_match));
		if (ret)
			goto err;
	}

	ret = ssusb_redriver_channel_update(redriver);
	if (ret)
		goto err;

	return 0;

err:
	dev_err(redriver->dev,
			"%s: set default parameters failure.\n", __func__);
	return ret;
}

static int ssusb_redriver_panic_notifier(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct ssusb_redriver *redriver = container_of(this,
			struct ssusb_redriver, panic_nb);

	pr_err("%s: op mode: %d, vbus: %d, host: %d\n", __func__,
		redriver->op_mode, redriver->vbus_active,
		redriver->host_active);

	return NOTIFY_OK;
}

static const struct regmap_config redriver_regmap = {
	.max_register = REDRIVER_REG_MAX,
	.reg_bits = 8,
	.val_bits = 8,
};

static int redriver_i2c_probe(struct i2c_client *client,
			       const struct i2c_device_id *dev_id)
{
	struct ssusb_redriver *redriver;
	union power_supply_propval pval = {0};
	int ret;

	redriver = devm_kzalloc(&client->dev, sizeof(struct ssusb_redriver),
			GFP_KERNEL);
	if (!redriver)
		return -ENOMEM;

	INIT_WORK(&redriver->config_work, ssusb_redriver_config_work);

	redriver->redriver_wq = alloc_ordered_workqueue("redriver_wq",
			WQ_HIGHPRI);
	if (!redriver->redriver_wq) {
		dev_err(&client->dev,
			"%s: Unable to create workqueue redriver_wq\n",
			__func__);
		return -ENOMEM;
	}

	redriver->dev = &client->dev;

	redriver->regmap = devm_regmap_init_i2c(client, &redriver_regmap);
	if (IS_ERR(redriver->regmap)) {
		ret = PTR_ERR(redriver->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", ret);
		goto destroy_wq;
	}

	redriver->client = client;
	i2c_set_clientdata(client, redriver);

	/* Set default parameters for A/B/C/D channels. */
	ret = ssusb_redriver_default_config(redriver);
	if (ret < 0)
		goto destroy_wq;

	/* Set id_state as float by default*/
	redriver->host_active = false;

	/* Set to USB by default */
	redriver->op_mode = OP_MODE_USB;

	redriver->usb_psy = power_supply_get_by_name("usb");
	if (!redriver->usb_psy) {
		dev_warn(&client->dev, "Could not get usb power_supply\n");
		pval.intval = -EINVAL;
	} else {
		power_supply_get_property(redriver->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);

		/* USB cable is not connected */
		if (!pval.intval)
			ssusb_redriver_gen_dev_set(redriver, false);
	}

	ret = ssusb_redriver_extcon_register(redriver);
	if (ret)
		goto put_psy;

	redriver->panic_nb.notifier_call = ssusb_redriver_panic_notifier;
	atomic_notifier_chain_register(&panic_notifier_list,
			&redriver->panic_nb);

	ssusb_redriver_debugfs_entries(redriver);

	dev_dbg(&client->dev, "USB 3.1 Gen1/Gen2 Re-Driver Probed.\n");

	return 0;

put_psy:
	if (redriver->usb_psy)
		power_supply_put(redriver->usb_psy);

destroy_wq:
	destroy_workqueue(redriver->redriver_wq);

	return ret;
}

static int redriver_i2c_remove(struct i2c_client *client)
{
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);

	debugfs_remove(redriver->debug_root);
	atomic_notifier_chain_unregister(&panic_notifier_list,
			&redriver->panic_nb);

	if (redriver->usb_psy)
		power_supply_put(redriver->usb_psy);

	destroy_workqueue(redriver->redriver_wq);

	return 0;
}

static ssize_t channel_config_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos,
		int (*config_func)(struct ssusb_redriver *redriver,
			u8 channel, u8 val))
{
	struct seq_file *s = file->private_data;
	struct ssusb_redriver *redriver = s->private;
	char buf[40];
	char *token_chan, *token_val, *this_buf;
	int store_offset = 0, ret = 0;

	memset(buf, 0, sizeof(buf));

	this_buf = buf;

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (isdigit(buf[0])) {
		ret = config_func(redriver, CHANNEL_NUM * CHAN_MODE_NUM,
				buf[0] - '0');
		if (ret < 0)
			goto err;
	} else if (isalpha(buf[0])) {
		while ((token_chan = strsep(&this_buf, " ")) != NULL) {
			switch (*token_chan) {
			case 'A':
			case 'B':
			case 'C':
			case 'D':
				store_offset = *token_chan - 'A';
				token_val = strsep(&this_buf, " ");
				if (!isdigit(*token_val))
					goto err;
				break;
			case 'a':
			case 'b':
			case 'c':
			case 'd':
				store_offset = *token_chan - 'a'
					+ CHANNEL_NUM;
				token_val = strsep(&this_buf, " ");
				if (!isdigit(*token_val))
					goto err;
				break;
			default:
				goto err;
			}

			ret = config_func(redriver, store_offset,
					*token_val - '0');
			if (ret < 0)
				goto err;
		}
	} else
		goto err;


	return count;

err:
	pr_err("Used to config redriver A/B/C/D channels' parameters\n"
		"A/B/C/D represent for re-driver parameters for USB\n"
		"a/b/c/d represent for re-driver parameters for DP\n"
		"1. Set all channels to same value(both USB and DP)\n"
		"echo n > [eq|output_comp|flat_gain|loss_match]\n"
		"- eq: Equalization, range 0-7\n"
		"- output_comp: Output Compression, range 0-3\n"
		"- loss_match: LOSS Profile Matching, range 0-3\n"
		"- flat_gain: Flat Gain, range 0-3\n"
		"Example: Set all channels to same EQ value\n"
		"echo 1 > eq\n"
		"2. Set two channels to different values leave others unchanged\n"
		"echo [A|B|C|D] n [A|B|C|D] n > [eq|output_comp|flat_gain|loss_match]\n"
		"Example2: USB mode: set channel B flat gain to 2, set channel C flat gain to 3\n"
		"echo B 2 C 3 > flat_gain\n"
		"Example3: DP mode: set channel A equalization to 6, set channel B equalization to 4\n"
		"echo a 6 b 4 > eq\n");

	return -EFAULT;
}

static int eq_status(struct seq_file *s, void *p)
{
	struct ssusb_redriver *redriver = s->private;

	seq_puts(s, "\t\t\t A(USB)\t B(USB)\t C(USB)\t D(USB)\t"
			"A(DP)\t B(DP)\t C(DP)\t D(DP)\n");
	seq_printf(s, "Equalization:\t\t %d\t %d\t %d\t %d\t"
			"%d\t %d\t %d\t %d\n",
			redriver->eq[CHAN_MODE_USB][CHNA_INDEX],
			redriver->eq[CHAN_MODE_USB][CHNB_INDEX],
			redriver->eq[CHAN_MODE_USB][CHNC_INDEX],
			redriver->eq[CHAN_MODE_USB][CHND_INDEX],
			redriver->eq[CHAN_MODE_DP][CHNA_INDEX],
			redriver->eq[CHAN_MODE_DP][CHNB_INDEX],
			redriver->eq[CHAN_MODE_DP][CHNC_INDEX],
			redriver->eq[CHAN_MODE_DP][CHND_INDEX]);
	return 0;
}

static int eq_status_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, eq_status, inode->i_private);
}

static ssize_t eq_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	return channel_config_write(file, ubuf, count, ppos,
			ssusb_redriver_eq_config);
}

static const struct file_operations eq_ops = {
	.open	= eq_status_open,
	.read	= seq_read,
	.write	= eq_write,
};

static int flat_gain_status(struct seq_file *s, void *p)
{
	struct ssusb_redriver *redriver = s->private;

	seq_puts(s, "\t\t\t A(USB)\t B(USB)\t C(USB)\t D(USB)\t"
			"A(DP)\t B(DP)\t C(DP)\t D(DP)\n");
	seq_printf(s, "TX/RX Flat Gain:\t %d\t %d\t %d\t %d\t"
			"%d\t %d\t %d\t %d\n",
			redriver->flat_gain[CHAN_MODE_USB][CHNA_INDEX],
			redriver->flat_gain[CHAN_MODE_USB][CHNB_INDEX],
			redriver->flat_gain[CHAN_MODE_USB][CHNC_INDEX],
			redriver->flat_gain[CHAN_MODE_USB][CHND_INDEX],
			redriver->flat_gain[CHAN_MODE_DP][CHNA_INDEX],
			redriver->flat_gain[CHAN_MODE_DP][CHNB_INDEX],
			redriver->flat_gain[CHAN_MODE_DP][CHNC_INDEX],
			redriver->flat_gain[CHAN_MODE_DP][CHND_INDEX]);
	return 0;
}

static int flat_gain_status_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, flat_gain_status, inode->i_private);
}

static ssize_t flat_gain_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	return channel_config_write(file, ubuf, count, ppos,
			ssusb_redriver_flat_gain_config);
}

static const struct file_operations flat_gain_ops = {
	.open	= flat_gain_status_open,
	.read	= seq_read,
	.write	= flat_gain_write,
};

static int output_comp_status(struct seq_file *s, void *p)
{
	struct ssusb_redriver *redriver = s->private;

	seq_puts(s, "\t\t\t A(USB)\t B(USB)\t C(USB)\t D(USB)\t"
			"A(DP)\t B(DP)\t C(DP)\t D(DP)\n");
	seq_printf(s, "Output Compression:\t %d\t %d\t %d\t %d\t"
			"%d\t %d\t %d\t %d\n",
			redriver->output_comp[CHAN_MODE_USB][CHNA_INDEX],
			redriver->output_comp[CHAN_MODE_USB][CHNB_INDEX],
			redriver->output_comp[CHAN_MODE_USB][CHNC_INDEX],
			redriver->output_comp[CHAN_MODE_USB][CHND_INDEX],
			redriver->output_comp[CHAN_MODE_DP][CHNA_INDEX],
			redriver->output_comp[CHAN_MODE_DP][CHNB_INDEX],
			redriver->output_comp[CHAN_MODE_DP][CHNC_INDEX],
			redriver->output_comp[CHAN_MODE_DP][CHND_INDEX]);
	return 0;
}

static int output_comp_status_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, output_comp_status, inode->i_private);
}

static ssize_t output_comp_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	return channel_config_write(file, ubuf, count, ppos,
			ssusb_redriver_output_comp_config);
}

static const struct file_operations output_comp_ops = {
	.open	= output_comp_status_open,
	.read	= seq_read,
	.write	= output_comp_write,
};

static int loss_match_status(struct seq_file *s, void *p)
{
	struct ssusb_redriver *redriver = s->private;

	seq_puts(s, "\t\t\t A(USB)\t B(USB)\t C(USB)\t D(USB)\t"
			"A(DP)\t B(DP)\t C(DP)\t D(DP)\n");
	seq_printf(s, "Loss Profile Match:\t %d\t %d\t %d\t %d\t"
			"%d\t %d\t %d\t %d\n",
			redriver->loss_match[CHAN_MODE_USB][CHNA_INDEX],
			redriver->loss_match[CHAN_MODE_USB][CHNB_INDEX],
			redriver->loss_match[CHAN_MODE_USB][CHNC_INDEX],
			redriver->loss_match[CHAN_MODE_USB][CHND_INDEX],
			redriver->loss_match[CHAN_MODE_DP][CHNA_INDEX],
			redriver->loss_match[CHAN_MODE_DP][CHNB_INDEX],
			redriver->loss_match[CHAN_MODE_DP][CHNC_INDEX],
			redriver->loss_match[CHAN_MODE_DP][CHND_INDEX]);
	return 0;
}

static int loss_match_status_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, loss_match_status, inode->i_private);
}

static ssize_t loss_match_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	return channel_config_write(file, ubuf, count, ppos,
			ssusb_redriver_loss_match_config);
}

static const struct file_operations loss_match_ops = {
	.open	= loss_match_status_open,
	.read	= seq_read,
	.write	= loss_match_write,
};

static void ssusb_redriver_debugfs_entries(
		struct ssusb_redriver *redriver)
{
	struct dentry *ent;

	redriver->debug_root = debugfs_create_dir("ssusb_redriver", NULL);
	if (!redriver->debug_root) {
		dev_warn(redriver->dev, "Couldn't create debug dir\n");
		return;
	}

	ent = debugfs_create_file("eq", 0600,
			redriver->debug_root, redriver, &eq_ops);
	if (IS_ERR_OR_NULL(ent))
		dev_warn(redriver->dev, "Couldn't create eq file\n");

	ent = debugfs_create_file("flat_gain", 0600,
			redriver->debug_root, redriver, &flat_gain_ops);
	if (IS_ERR_OR_NULL(ent))
		dev_warn(redriver->dev, "Couldn't create flat_gain file\n");

	ent = debugfs_create_file("output_comp", 0600,
			redriver->debug_root, redriver, &output_comp_ops);
	if (IS_ERR_OR_NULL(ent))
		dev_warn(redriver->dev, "Couldn't create output_comp file\n");

	ent = debugfs_create_file("loss_match", 0600,
			redriver->debug_root, redriver, &loss_match_ops);
	if (IS_ERR_OR_NULL(ent))
		dev_warn(redriver->dev, "Couldn't create loss_match file\n");
}

static int __maybe_unused redriver_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);

	dev_dbg(redriver->dev, "%s: SS USB redriver suspend.\n",
			__func__);

	/* Disable redriver chip when USB cable disconnected */
	if ((!redriver->vbus_active && !redriver->host_active &&
	     redriver->op_mode != OP_MODE_DP) ||
	    (redriver->host_active &&
	     redriver->op_mode == OP_MODE_USB_AND_DP))
		ssusb_redriver_gen_dev_set(redriver, false);

	flush_workqueue(redriver->redriver_wq);

	return 0;
}

static int __maybe_unused redriver_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);

	dev_dbg(redriver->dev, "%s: SS USB redriver resume.\n",
			__func__);

	if (redriver->host_active &&
	    redriver->op_mode == OP_MODE_USB_AND_DP)
		ssusb_redriver_gen_dev_set(redriver, true);

	flush_workqueue(redriver->redriver_wq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(redriver_i2c_pm, redriver_i2c_suspend,
			 redriver_i2c_resume);

static void redriver_i2c_shutdown(struct i2c_client *client)
{
	struct ssusb_redriver *redriver = i2c_get_clientdata(client);
	int ret;

	/* Set back to USB mode with four channel enabled */
	ret = redriver_i2c_reg_set(redriver, GEN_DEV_SET_REG,
			GEN_DEV_SET_REG_DEFAULT);
	if (ret < 0)
		dev_err(&client->dev,
			"%s: fail to set USB mode with 4 channel enabled.\n",
			__func__);
	else
		dev_dbg(&client->dev,
			"%s: successfully set back to USB mode.\n",
			__func__);
}

static const struct of_device_id redriver_match_table[] = {
	{ .compatible = "onnn,redriver",},
	{ },
};

static const struct i2c_device_id redriver_i2c_id[] = {
	{ "ssusb redriver", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, redriver_i2c_id);

static struct i2c_driver redriver_i2c_driver = {
	.driver = {
		.name	= "ssusb redriver",
		.of_match_table	= redriver_match_table,
		.pm	= &redriver_i2c_pm,
	},

	.probe		= redriver_i2c_probe,
	.remove		= redriver_i2c_remove,

	.shutdown	= redriver_i2c_shutdown,

	.id_table	= redriver_i2c_id,
};

module_i2c_driver(redriver_i2c_driver);

MODULE_DESCRIPTION("USB Super Speed Linear Re-Driver Driver");
MODULE_LICENSE("GPL v2");
