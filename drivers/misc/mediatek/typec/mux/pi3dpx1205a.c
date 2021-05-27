// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>
#include "tcpm.h"

#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
#include "mux_switch.h"
#endif

#include <pi3dpx1205a.h>

#define K_EMERG	(1<<7)
#define K_QMU	(1<<7)
#define K_ALET		(1<<6)
#define K_CRIT		(1<<5)
#define K_ERR		(1<<4)
#define K_WARNIN	(1<<3)
#define K_NOTICE	(1<<2)
#define K_INFO		(1<<1)
#define K_DEBUG	(1<<0)

#define BYTE_SIZE 13

#define usbc_dbg(level, fmt, args...) do { \
		if (debug_level & level) { \
			pr_info("[USB_DP]" fmt, ## args); \
		} \
	} while (0)

static u32 debug_level = (255 - K_DEBUG);

static u8 eq_fg_sw[8][6];

struct pi3dpx1205a {
	struct device *dev;
	struct i2c_client *i2c;
	struct typec_switch *sw;
	struct typec_mux *mux;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pwr_en;
	struct pinctrl_state *ext_pwr_en;
	char *platform;
	u8 conf;
};

void __attribute__ ((weak)) mtk_dp_SWInterruptSet(int bstatus)
{
}

/*
 * Read PI3DPX1205A I2C reg from BYTE0 to BYTE len-1
 * return value: no of byte read
 */
static int pi3dpx1205a_readn(struct i2c_client *client, u8 len, u8 *val)
{
	usbc_dbg(K_INFO, "I2C readn : len=%u\n", len);

	if (!client) {
		usbc_dbg(K_ERR, "Null client\n");
		return -1;
	}

	//Read I2C Byte0 to Byte len-1
	return i2c_smbus_read_i2c_block_data(client, 0, len, val);
}

/*
 * Read PI3DPX1205A I2C reg Byte N
 * return value: Byte N
 * Max size : 13
 */
static int pi3dpx1205a_read(struct i2c_client *client, u8 index)
{
	u8 data[BYTE_SIZE];
	int res;

	//Exceed BYTE definition
	if ((index > BYTE_SIZE - 1) || (index < 0)) {
		usbc_dbg(K_ERR, "%s Exceed BYTE definition\n", __func__);
		return -1;
	}

	//Read I2C Byte0 to Byte 12
	res = pi3dpx1205a_readn(client, BYTE_SIZE, data);

	if (res > 0)
		return data[index];

	return res;
}

/*
 * Write PI3DPX1205A I2C reg from BYTE0 to BYTE len-1
 * return value: no of byte written
 */
static int pi3dpx1205a_writen(struct i2c_client *client, u8 len, u8 *val)
{
	if (!client) {
		usbc_dbg(K_ERR, "Null client\n");
		return -1;
	}

	//Write I2C Byte0 to Byte len-1
	if (len > 1)
		return i2c_smbus_write_i2c_block_data(client,
			val[0], len-1, val+1);

	return i2c_smbus_write_byte(client, val[0]);
}

/*
 * Write PI3DPX1205A I2C reg Byte N
 * return value: no of byte written
 */
int pi3dpx1205a_write(struct i2c_client *client, u8 index, u8 val)
{
	u8 data[BYTE_SIZE];
	int res;

	//Read I2C Byte0 to Byte N
	res = pi3dpx1205a_readn(client, index+1, data);

	if (res > 0)	{
		data[index] = val;
		return pi3dpx1205a_writen(client, index+1, data);
	}

	return res;
}

/*
 * Write PI3DPX1205A Byte 3 to set channel mapping control
 * input: conf
 * return value: no of byte written
 * 0000 Safe State
 * 0001 Safe State
 * 0010 4 lane DP1.4 + AUX
 * 0011 4 lane DP1.4 + AUX Flipped
 * 0100 1 lane USB3.x (AP_CH1)
 * 0101 1 lane USB3.x (AP_CH1) Flipped
 * 0110 USB3 (AP_CH1) + 2 lane DP1.4 (AP_CH2) + AUX
 * 0111 USB3 (AP_CH1) + 2 lane DP1.4 (AP_CH2) + AUX Flipped
 */
int pi3dpx1205a_set_conf(struct i2c_client *client, u8 conf)
{
	//Read byte 3
	int reg = pi3dpx1205a_read(client, 3);

	if (reg < 0)
		return -1;

	reg &= 0x0F;
	reg |= (conf << 4);

	return pi3dpx1205a_write(client, 3, reg);
}

/*
 * Write PI3DPX1205A Byte 3 to enter safe state and
 * Byte 4 to Byte 8 and Byte 12 to set Equalization,
 * Flat gain, Swing and features.
 * return -1 if fail
 * input: conf
 */
static int pi3dpx1205a_set_eq_fg_sw(struct i2c_client *client, u8 conf)
{
	u8 data[BYTE_SIZE] = {0};
	int res;

	//Read I2C Byte0 to Byte 12
	res = pi3dpx1205a_readn(client, BYTE_SIZE, data);

	if (res < 0)
		return -1;

	//Set CONF to 0000b to enter safe state
	data[3] &= 0x0F;
	data[4] = eq_fg_sw[conf][0];
	data[5] = eq_fg_sw[conf][1];
	data[6] = eq_fg_sw[conf][2];
	data[7] = eq_fg_sw[conf][3];
	data[8] = eq_fg_sw[conf][4];
	data[12] = eq_fg_sw[conf][5];

	res = pi3dpx1205a_writen(client, BYTE_SIZE, data);

	return res;
}

#ifdef USB_DP_DEBUG
/*
 * Read PI3DPX1205A Byte 3 to get channel mapping state
 *return value: Byte3
 */
int pi3dpx1205a_get_conf(struct i2c_client *client)
{
	int reg = pi3dpx1205a_read(client, 3)

	//Read fail
	if (reg < 0)
		return -1;

	reg &= 0xF0;
	return (reg >> 4); //return bit[7:4] of byte3
}
#endif

/*
 * Write PI3DPX1205A Byte 4 and Byte 12 to set HPD state
 * return value: no of byte written
 */
int pi3dpx1205a_hpd(struct i2c_client *client, u8 hpd)
{
	u8 data[13];
	int conf = 0;
	int res;

	usbc_dbg(K_INFO, "hpd=%d\n", hpd);

	//Read I2C Byte0 to Byte 12
	res = pi3dpx1205a_readn(client, 13, data);
	if (res < 0)
		return -1;

	//Get CONF
	conf = data[3];
	conf &= 0xF0;
	conf = conf >> 4;

	data[4] = eq_fg_sw[conf][0];

	if (hpd) {
		//If HPD is high, power on DP channels by
		//clear bits[7:4] of byte 4 (corresponding DP channels).
		data[4] = eq_fg_sw[conf][0] & 0x0F;
	} else {
		if (conf == 2)
			data[4] = eq_fg_sw[conf][0] | 0xE0;
		else if (conf == 3)
			data[4] = eq_fg_sw[conf][0] | 0x70;
		else if (conf == 6)
			data[4] = eq_fg_sw[conf][0] | 0x20;
		else if (conf == 7)
			data[4] = eq_fg_sw[conf][0] | 0x40;
	}

	if (hpd) {
		if ((conf == 2) || (conf == 3) || (conf == 6) || (conf == 7))
			//If HPD is high,
			//enable IN_HPD pin by clear bit2 of byte12
			data[12] = eq_fg_sw[conf][5] | 0x04;
	} else
		data[12] = eq_fg_sw[conf][5] & 0xFB;

	res = pi3dpx1205a_writen(client, 13, data);

	return res;
}

static int pi3dpx1205a_init(struct i2c_client *client)
{
	int res;

	res = pi3dpx1205a_set_eq_fg_sw(client, 0);

	if (res < 0)
		return -1;

	return 0;
}

/*
 * xxxxx1xx = Pin Assignment C is supported. 4 lanes
 * xxx1xxxx = Pin Assignment E is supported. 4 lanes
 * xxxx1xxx = Pin Assignment D is supported. 2 lanes
 * xx1xxxxx = Pin Assignment F is supported. 2 lanes
 */
static int pi3dpx1205a_mux_set(struct typec_mux *mux, struct typec_mux_state *state)
{
	/*
	 * struct tcp_ny_ama_dp_state {
	 * uint8_t sel_config; sel_config: 0(SW_USB) / 1(SW_DFP_D) / 2(SW_UFP_D)
	 * uint8_t signal;
	 * uint8_t pin_assignment;
	 * uint8_t polarity; polarity: 0 for up side, 1 back side.
	 * uint8_t active;
	 * };
	 */
	struct pi3dpx1205a *pi3dpx = typec_mux_get_drvdata(mux);
	struct tcp_notify *data = state->data;
	int ret = 0;

	usbc_dbg(K_INFO, "%s event=%x", __func__, state->mode);

	if (state->mode == TCP_NOTIFY_AMA_DP_STATE) {
		uint8_t signal = data->ama_dp_state.signal;
		uint8_t pin = data->ama_dp_state.pin_assignment;
		uint8_t polarity = data->ama_dp_state.polarity;
		uint8_t active = data->ama_dp_state.active;

		if (!active) {
			usbc_dbg(K_INFO, "%s Not active", __func__);
			return NOTIFY_OK;
		}

		usbc_dbg(K_INFO, "TCP_NOTIFY_AMA_DP_STATE signal:%x pin:%x polarity:%x\n",
			signal, pin, polarity);

		if (!polarity) {
			switch (pin) {
			case 4:
			case 16:
				ret = pi3dpx1205a_set_eq_fg_sw(pi3dpx->i2c, 2);
				ret |= pi3dpx1205a_set_conf(pi3dpx->i2c, 2);
				break;
			case 8:
			case 32:
				ret = pi3dpx1205a_set_eq_fg_sw(pi3dpx->i2c, 6);
				ret |= pi3dpx1205a_set_conf(pi3dpx->i2c, 6);
				break;
			default:
				usbc_dbg(K_INFO, "%s: pin_assignment not support\n",
					__func__);
			}
		} else {
			switch (pin) {
			case 4:
			case 16:
				ret = pi3dpx1205a_set_eq_fg_sw(pi3dpx->i2c, 3);
				ret |= pi3dpx1205a_set_conf(pi3dpx->i2c, 3);
				break;
			case 8:
			case 32:
				ret = pi3dpx1205a_set_eq_fg_sw(pi3dpx->i2c, 7);
				ret |= pi3dpx1205a_set_conf(pi3dpx->i2c, 7);
				break;
			default:
				usbc_dbg(K_INFO, "%s: pin_assignment not support\n",
					__func__);
			}
		}
	} else if (state->mode == TCP_NOTIFY_AMA_DP_HPD_STATE) {
		uint8_t irq = data->ama_dp_hpd_state.irq;
		uint8_t state = data->ama_dp_hpd_state.state;

		usbc_dbg(K_INFO, "TCP_NOTIFY_AMA_DP_HPD_STATE irq:%x state:%x\n",
			irq, state);

		if (state) {
			ret = pi3dpx1205a_hpd(pi3dpx->i2c, 1);
			if (irq)
				mtk_dp_SWInterruptSet(0x8);
			else
				mtk_dp_SWInterruptSet(0x4);
		} else {
			ret = pi3dpx1205a_hpd(pi3dpx->i2c, 0);
			mtk_dp_SWInterruptSet(0x2);
		}
	} else if (state->mode == TCP_NOTIFY_TYPEC_STATE) {
		if ((data->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			data->typec_state.old_state == TYPEC_ATTACHED_SNK) &&
			data->typec_state.new_state == TYPEC_UNATTACHED) {
			usbc_dbg(K_INFO, "Plug out\n");
			mtk_dp_SWInterruptSet(0x2);
			ret = pi3dpx1205a_set_eq_fg_sw(pi3dpx->i2c, 0);
		}
	}

	return ret;
}

static int pi3dpx1205a_switch_set(struct typec_switch *sw,
	enum typec_orientation orientation)
{
	struct pi3dpx1205a *pi3dpx = typec_switch_get_drvdata(sw);

	usbc_dbg(K_INFO, "%s %d\n", __func__, orientation);

	switch (orientation) {
	case TYPEC_ORIENTATION_NORMAL:
		/* switch cc1 side */
		pi3dpx1205a_set_eq_fg_sw(pi3dpx->i2c, 4);
		pi3dpx1205a_set_conf(pi3dpx->i2c, 4);
		break;
	case TYPEC_ORIENTATION_REVERSE:
		/* switch cc2 side */
		pi3dpx1205a_set_eq_fg_sw(pi3dpx->i2c, 5);
		pi3dpx1205a_set_conf(pi3dpx->i2c, 5);
		break;
	default:
		break;
	}

	return 0;
}

static int pi3dpx1205a_pinctrl(struct pi3dpx1205a *pi3dpx)
{
	int i, j, ret = 0;
	struct device *dev = pi3dpx->dev;
	struct device_node *np;
	const char *platform;

	usbc_dbg(K_INFO, "%s: initializing...\n", __func__);

	pi3dpx->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pi3dpx->pinctrl)) {
		ret = PTR_ERR(pi3dpx->pinctrl);
		usbc_dbg(K_ERR, "failed to get pinctrl, ret=%d\n", ret);
		return -EINVAL;
	}

	pi3dpx->pwr_en = pinctrl_lookup_state(pi3dpx->pinctrl, "dp_pwr_high");

	if (IS_ERR(pi3dpx->pwr_en)) {
		usbc_dbg(K_ERR, "Can *NOT* find pwr_en\n");
		ret = -1;
	} else {
		usbc_dbg(K_DEBUG, "Find pwr_en\n");
		pinctrl_select_state(pi3dpx->pinctrl, pi3dpx->pwr_en);
	}

	pi3dpx->ext_pwr_en = pinctrl_lookup_state(pi3dpx->pinctrl,
		"dp_ext_pwr_high");

	if (IS_ERR(pi3dpx->ext_pwr_en)) {
		usbc_dbg(K_ERR, "Can *NOT* find ext_pwr_en\n");
		ret = -1;
	} else {
		usbc_dbg(K_DEBUG, "Find ext_pwr_en\n");
		pinctrl_select_state(pi3dpx->pinctrl, pi3dpx->ext_pwr_en);
	}

	np = of_find_node_by_name(dev->of_node, "usb_dp-data");
	if (np) {
		usbc_dbg(K_DEBUG, "%s: find usb_dp-data\n", __func__);
		ret = of_property_read_string(np, "platform", &platform);

		if (ret < 0)
			usbc_dbg(K_ERR, "Get platform info fail\n");

		usbc_dbg(K_INFO, "%s: find platform info %s\n",
			__func__, platform);
		if (strcmp("evb", platform) == 0) {
			for (i = 0; i < 8; ++i) {
				for (j = 0; j < 6; ++j) {
					usbc_dbg(K_DEBUG, "evb\n");
					eq_fg_sw[i][j] = eq_fg_sw_evb[i][j];
				}
			}
		} else {
			for (i = 0; i < 8; ++i) {
				for (j = 0; j < 6; ++j) {
					usbc_dbg(K_DEBUG, "non-evb\n");
					eq_fg_sw[i][j] = eq_fg_sw_evb[i][j];
				}
			}
		};
	}

	return ret;
}

static int pi3dpx1205a_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct typec_switch_desc sw_desc;
	struct typec_mux_desc mux_desc;
	struct pi3dpx1205a *pi3dpx;
	int ret = 0;

	ret = i2c_check_functionality(client->adapter,
						  I2C_FUNC_SMBUS_I2C_BLOCK |
						  I2C_FUNC_SMBUS_BYTE_DATA);
	usbc_dbg(K_INFO, "%s I2C functionality : %s\n", __func__,
		ret ? "ok" : "fail");

	pi3dpx = devm_kzalloc(dev, sizeof(*pi3dpx), GFP_KERNEL);
	if (!pi3dpx)
		return -ENOMEM;

	pi3dpx->i2c = client;
	pi3dpx->dev = dev;

	/* Setting Switch callback */
	sw_desc.drvdata = pi3dpx;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = pi3dpx1205a_switch_set;
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
	pi3dpx->sw = mtk_typec_switch_register(dev, &sw_desc);
#else
	pi3dpx->sw = typec_switch_register(dev, &sw_desc);
#endif
	if (IS_ERR(pi3dpx->sw)) {
		usbc_dbg(K_ERR, "error registering typec switch: %ld\n",
			PTR_ERR(pi3dpx->sw));
		return PTR_ERR(pi3dpx->sw);
	}

	/* Setting MUX callback */
	mux_desc.drvdata = pi3dpx;
	mux_desc.fwnode = dev->fwnode;
	mux_desc.set = pi3dpx1205a_mux_set;
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
	pi3dpx->mux = mtk_typec_mux_register(dev, &mux_desc);
#else
	pi3dpx->mux = typec_switch_register(dev, &mux_desc);
#endif
	if (IS_ERR(pi3dpx->mux)) {
		usbc_dbg(K_ERR, "Error registering typec mux: %ld\n",
			PTR_ERR(pi3dpx->mux));
		return PTR_ERR(pi3dpx->mux);
	}

	i2c_set_clientdata(client, pi3dpx);

	ret = pi3dpx1205a_pinctrl(pi3dpx);
	if (ret < 0) {
#if IS_ENABLED(CONFIG_MTK_USB_TYPEC_MUX)
		mtk_typec_switch_unregister(pi3dpx->sw);
#else
		mtk_typec_switch_unregister(pi3dpx->sw);
#endif
	}

	pi3dpx1205a_init(client);

	return ret;
}

static int pi3dpx1205a_remove(struct i2c_client *client)
{
	struct pi3dpx1205a *pi3dpx = i2c_get_clientdata(client);

	mtk_typec_switch_unregister(pi3dpx->sw);
	typec_mux_unregister(pi3dpx->mux);
	/* typec_switch_unregister(pi->sw); */
	return 0;
}

static const struct i2c_device_id pi3dpx1205a_id_table[] = {
	{"PI3DPX1205A", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, pi3dpx1205a_id_table);

static const struct of_device_id pi3dpx1205a_ids[] = {
	{.compatible = "mediatek,pi3dpx1205a",},
	{},
};
MODULE_DEVICE_TABLE(of, pi3dpx1205a_ids);

static struct i2c_driver usb_dp_driver = {
	.driver = {
		.name = "pi3dpx1205a",
		.of_match_table = pi3dpx1205a_ids,
	},
	.probe = pi3dpx1205a_probe,
	.remove = pi3dpx1205a_remove,
	.id_table = pi3dpx1205a_id_table,
};

module_i2c_driver(usb_dp_driver);

MODULE_DESCRIPTION("PI3DPX1205A Type-C Redriver");
MODULE_LICENSE("GPL v2");

