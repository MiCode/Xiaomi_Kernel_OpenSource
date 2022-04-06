// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <typec.h>
#include <pi3dpx1205azlbex.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>

#include <linux/gpio.h>
#include "tcpm.h"

#include "tcpm.h"
#define IDET_VTH 0
#define DP_HPD_PIN_ENB 0
#define AUX_ENB 1
#define RX1_EQ 0
#define RX1_FG 0
#define RX1_SW 0
#define TX1_EQ 0
#define TX1_FG 0
#define TX1_SW 0
#define TX2_EQ 0
#define TX2_FG 0
#define TX2_SW 0
#define RX2_EQ 0
#define RX2_FG 0
#define RX2_SW 0
#define PI3DPX1205A1_BYTE12 ((IDET_VTH) << 6 | (DP_HPD_PIN_ENB) << 2 | (AUX_ENB) << 1)
#define PI3DPX1205A1_BYTE8 ((RX1_EQ) <<4 | (RX1_FG) <<2 | RX1_SW)
#define PI3DPX1205A1_BYTE7 ((TX1_EQ) <<4 | (TX1_FG) <<2 | TX1_SW)
#define PI3DPX1205A1_BYTE6 ((TX2_EQ) <<4 | (TX2_FG) <<2 | TX2_SW)
#define PI3DPX1205A1_BYTE5 ((RX2_EQ) <<4 | (RX2_FG) <<2 | RX2_SW)

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
		if (1) { \
			pr_info("[USB_DP]" fmt, ## args); \
		} \
	} while (0)

//static u32 debug_level = (255 - K_DEBUG);

static u8 eq_fg_sw[8][6];
static struct i2c_client *usbdp_client;
static struct notifier_block dp_nb;
static struct tcpc_device *dp_tcpc_dev;
static int hdp_state;
static bool dp_sw_connect;

#define CHECK_HPD_DELAY 2000
static struct delayed_work check_wk;

/*
 * Read PI3DPX1205A I2C reg from BYTE0 to BYTE len-1
 * return value: no of byte read
 */
static int pi3dpx1205a_readn(struct i2c_client *client, u8 len)
{
	pr_info("I2C readn : len=%d\n",len);

	if (!client) {
		usbc_dbg(K_ERR, "Null client\n");
		return -1;
	}

	/* Read I2C 0x06 */
	pr_info("%s====%d\n", __func__, i2c_smbus_read_byte_data (client,0x06));

	return i2c_smbus_read_byte_data (client,0x06);
}

/*
 * Write PI3DPX1205A I2C reg from BYTE0 to BYTE len-1
 * return value: no of byte written
 */
static int pi3dpx1205a_writen(struct i2c_client *client,u8 val)
{
	if (!client) {
		usbc_dbg(K_ERR, "Null client\n");
		return -1;
	}

	/* Write I2C 0x06 */
	return i2c_smbus_write_byte_data(client,0x06,val);
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
	/* Read byte 3 */
	int reg = pi3dpx1205a_readn(client, 3);
	int res;
	u8 data[BYTE_SIZE] = {0};

	/* Read I2C Byte0 to Byte 12 */
	res = pi3dpx1205a_readn(client,3);
	if (res < 0)
		return -1;
	data[3] = conf;
	res = pi3dpx1205a_writen(client, conf);
	pr_info("%s:%d\n", __func__, pi3dpx1205a_readn(client,3));

	return reg;
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
	int res=0;

	res = pi3dpx1205a_writen(client,0xA8);
	pr_info("yds-:res =%d\n",res);
	for (res = 0; res < 3; res++) {
		pi3dpx1205a_readn(client,3);
		msleep(50);
		pi3dpx1205a_writen(client,0xA8);
		msleep(50);
	}

	pr_info("yds-%s:start!!\n",__func__);
	res = pi3dpx1205a_readn(client,3);
	if (res < 0) {
		pr_info("yds:IIC READ fail");
		return -1;
	}

	res = pi3dpx1205a_writen(client,0xA8);
	if (res < 0)
		return -1; /* Fail */
	res = pi3dpx1205a_readn(client,3);
	pr_info("yds-%s:RES==%d\n",__func__,res);

	return res;
}

#ifdef USB_DP_DEBUG
/*
 * Read PI3DPX1205A Byte 3 to get channel mapping state
 *return value: Byte3
 */
int pi3dpx1205a_get_conf(struct i2c_client *client)
{
	int reg = pi3dpx1205a_readn(client, 3)

	/* Read fail */
	if (reg < 0)
		return -1;

	reg &= 0xF0;
	return (reg >> 4); /* return bit[7:4] of byte3 */
}
#endif

/*
 * Write PI3DPX1205A Byte 4 and Byte 12 to set HPD state
 * return value: no of byte written
 */
int pi3dpx1205a_hpd(struct i2c_client *client, u8 hpd)
{
	int res;

	usbc_dbg(K_INFO, "hpd=%d\n", hpd);

	/* Read I2C */
	pr_info("yds-%s:start!!\n",__func__);
	res = pi3dpx1205a_readn(client,3);
	if (res < 0)
		return -1;
	res = pi3dpx1205a_writen(client,0xFB);

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

static int usbdp_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret;

	ret = i2c_check_functionality(client->adapter,
						  I2C_FUNC_SMBUS_I2C_BLOCK |
						  I2C_FUNC_SMBUS_BYTE_DATA);
	usbc_dbg(K_INFO, "%s I2C functionality : %s\n", __func__,
		ret ? "ok" : "fail");

	usbdp_client = client;

	return pi3dpx1205a_init(client);
}

static int usbdp_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static void usbdp_shutdown(struct i2c_client *client)
{
}

/*
 * xxxxx1xx = Pin Assignment C is supported. 4 lanes
 * xxx1xxxx = Pin Assignment E is supported. 4 lanes
 * xxxx1xxx = Pin Assignment D is supported. 2 lanes
 * xx1xxxxx = Pin Assignment F is supported. 2 lanes
 */
static int chg_tcp_notifier_call(struct notifier_block *nb,
	unsigned long event, void *data)
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
	struct tcp_notify *noti = data;
	/* Debug */
	/* Debug */

	usbc_dbg(K_INFO, "%s event=%x", __func__, event);
	if (event == TCP_NOTIFY_AMA_DP_STATE) {
		uint8_t signal = noti->ama_dp_state.signal;
		uint8_t pin = noti->ama_dp_state.pin_assignment;
		uint8_t polarity = noti->ama_dp_state.polarity;
		uint8_t active = noti->ama_dp_state.active;

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
				pi3dpx1205a_writen(usbdp_client, 0xFB);
				pi3dpx1205a_readn(usbdp_client,0x06);
				pr_info("pi3dpx1205a_set_conf(usbdp_client, 0XF8)zhengcha\n");
				break;
			case 8:
			case 32:
				pi3dpx1205a_writen(usbdp_client, 0xFB);
				pi3dpx1205a_readn(usbdp_client,0x06);
				pr_info("pi3dpx1205a_set_conf(usbdp_client,0XF8)zhengcha\n");
				break;
			default:
				usbc_dbg(K_INFO, "%s: pin_assignment not support\n",
					__func__);
			}
		} else {
			switch (pin) {
			case 4:
			case 16:
				pi3dpx1205a_writen(usbdp_client, 0xFF);
				pi3dpx1205a_readn(usbdp_client,0x06);
				pr_info("pi3dpx1205a_set_conf(usbdp_client, 0XFC)FANcha\n");
				break;
			case 8:
			case 32:
				pr_info("%s:pin32:[%d]\n",__func__,pin);
				pi3dpx1205a_writen(usbdp_client, 0xFF);
				pi3dpx1205a_readn(usbdp_client,0x06);
				pr_info("pi3dpx1205a_set_conf(usbdp_client, 0XFC)FANcha\n");
				break;
			default:
				usbc_dbg(K_INFO, "%s: pin_assignment not support\n",
					__func__);
			}
		}

		hdp_state = 0;
		schedule_delayed_work(&check_wk, msecs_to_jiffies(CHECK_HPD_DELAY));
	} else if (event == TCP_NOTIFY_AMA_DP_HPD_STATE) {
		uint8_t irq = noti->ama_dp_hpd_state.irq;
		uint8_t state = noti->ama_dp_hpd_state.state;

		usbc_dbg(K_INFO, "TCP_NOTIFY_AMA_DP_HPD_STATE irq:%x state:%x\n",
			irq, state);

		hdp_state = state;

		if (state) {
			if (irq) {
				if (dp_sw_connect == false) {
					usbc_dbg(K_INFO, "Force connect\n");
					//mtk_dp_SWInterruptSet(0x4);
					dp_sw_connect = true;
				}
				//mtk_dp_SWInterruptSet(0x8);
			} else {
				//mtk_dp_SWInterruptSet(0x4);
				dp_sw_connect = true;
			}
		} else {
			//mtk_dp_SWInterruptSet(0x2);
			dp_sw_connect = false;
		}
	} else if (event == TCP_NOTIFY_TYPEC_STATE) {
		if (noti->typec_state.polarity == 0){
			pi3dpx1205a_writen(usbdp_client, 0xFB);
			//pi3dpx1205a_set_conf(usbdp_client, 0xFB);
			pr_info("pi3dpx1205a_set_conf(usbdp_client, 0XFB)zc\n");
		}else{
			pi3dpx1205a_writen(usbdp_client, 0xFF);
			//pi3dpx1205a_set_conf(usbdp_client, 0XFF);
			pr_info("pi3dpx1205a_set_conf(usbdp_client, 0XFF)fc\n");
			}
		if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			pr_info("P537 typec Plug out\n");
			pi3dpx1205a_writen(usbdp_client, 0xA8);
			pi3dpx1205a_readn(usbdp_client,0x06);
			//mtk_dp_SWInterruptSet(0x2);
			dp_sw_connect = false;
		}
		pr_info("yds_event == TCP_NOTIFY_TYPEC_STATE!!!!!!\n");
	}
	/* Debug */
	/* Debug */
	return NOTIFY_OK;
}

static const struct i2c_device_id pi3dpx1205a_id_table[] = {
	{"PI3DPX1205A", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, pi3dpx1205a_id_table);

static const struct of_device_id usb_dp_ids[] = {
	{.compatible = "mediatek,usb_dp",},
	{},
};

static struct i2c_driver usb_dp_driver = {
	.driver = {
		.name = "usb_dp",
		.owner = THIS_MODULE,
		.of_match_table = usb_dp_ids,
		.pm = (NULL),
	},
	.probe = usbdp_i2c_probe,
	.remove = usbdp_i2c_remove,
	.shutdown = usbdp_shutdown,
	.id_table = pi3dpx1205a_id_table,
};

static void check_hpd(struct work_struct *work)
{
	if (hdp_state == 0) {
		usbc_dbg(K_INFO, "%s No HPD connection event", __func__);
		pi3dpx1205a_hpd(usbdp_client, 1);
		//mtk_dp_SWInterruptSet(0x4);
	}
}

static int __init usb_dp_init(void)
{
	struct device_node *np;

	usbc_dbg(K_INFO, "%s: initializing...\n", __func__);

	dp_tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!dp_tcpc_dev) {
		usbc_dbg(K_ERR, "%s get tcpc device type_c_port0 fail\n",
			__func__);
		return -ENODEV;
	}

	dp_nb.notifier_call = chg_tcp_notifier_call;
	register_tcp_dev_notifier(dp_tcpc_dev, &dp_nb, TCP_NOTIFY_TYPE_MODE |
		TCP_NOTIFY_TYPE_VBUS | TCP_NOTIFY_TYPE_USB |
		TCP_NOTIFY_TYPE_MISC);

	np = of_find_node_by_name(NULL, "usb_dp");
	if (np != NULL)
		usbc_dbg(K_INFO, "usb_dp node found...\n");
	else
		usbc_dbg(K_ERR, "usb_dp node not found...\n");

	INIT_DEFERRABLE_WORK(&check_wk, check_hpd);
	dp_sw_connect = false;

	return i2c_add_driver(&usb_dp_driver);
}
late_initcall(usb_dp_init);

static void __exit usb_dp_exit(void)
{
	i2c_del_driver(&usb_dp_driver);
}
module_exit(usb_dp_exit);

void usb3_switch_dps_en(bool en)
{
	usbc_dbg(K_DEBUG, "%s en=%d\n", __func__, en);
}

void usb3_switch_ctrl_sel(int sel)
{
	int res;

	usbc_dbg(K_INFO, "en=%d\n", sel);

	if (sel == CC1_SIDE) {
		res = pi3dpx1205a_set_eq_fg_sw(usbdp_client, 4);
		res = pi3dpx1205a_set_conf(usbdp_client, 4);
	} else if (sel == CC2_SIDE) {
		/* U3 mux test purpose */
		res = pi3dpx1205a_set_eq_fg_sw(usbdp_client, 5);
		res = pi3dpx1205a_set_conf(usbdp_client, 5);
	}

	if (res < 0)
		usbc_dbg(K_ERR, "%s fail\n", __func__);
}

struct usbdp_pin_ctrl dp_pctrl;

static int usbdp_pinctrl_probe(struct platform_device *pdev)
{
	int i, j, ret = 0;
	struct device_node *np;
	const char *platform;

	pr_info("%s: initializing...\n", __func__);

	dp_pctrl.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(dp_pctrl.pinctrl)) {
		usbc_dbg(K_ERR, "Cannot find usb pinctrl!\n");
		return -EINVAL;
	}

	dp_pctrl.pwr_en = pinctrl_lookup_state(dp_pctrl.pinctrl, "dp_pwr_high");

	if (IS_ERR(dp_pctrl.pwr_en)) {
		usbc_dbg(K_ERR, "Can *NOT* find pwr_en\n");
		ret = -1;
	} else {
		usbc_dbg(K_DEBUG, "Find pwr_en\n");
		pinctrl_select_state(dp_pctrl.pinctrl, dp_pctrl.pwr_en);
	}

	dp_pctrl.ext_pwr_en = pinctrl_lookup_state(dp_pctrl.pinctrl,
		"dp_ext_pwr_high");

	if (IS_ERR(dp_pctrl.ext_pwr_en)) {
		usbc_dbg(K_ERR, "Can *NOT* find ext_pwr_en\n");
		ret = -1;
	} else {
		usbc_dbg(K_DEBUG, "Find ext_pwr_en\n");
		pinctrl_select_state(dp_pctrl.pinctrl, dp_pctrl.ext_pwr_en);
	}
		pinctrl_select_state(dp_pctrl.pinctrl, dp_pctrl.pwr_en);
		pr_info("zzz-%s:dp_pctrl.pwr_en\n",__func__);
	np = of_find_node_by_name(pdev->dev.of_node, "usb_dp-data");
	if (np) {
		usbc_dbg(K_DEBUG, "%s: find usb_dp-data\n", __func__);
		ret = of_property_read_string(np, "platform", &platform);

		if (ret < 0)
			usbc_dbg(K_ERR, "Get platform info fail\n");

		usbc_dbg(K_DEBUG, "%s: find platform info %s\n",
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

static const struct of_device_id usb_pinctrl_ids[] = {
	{.compatible = "mediatek,usb_dp_pinctrl",},
	{},
};

static struct platform_driver usb_dp_pinctrl_driver = {
	.probe = usbdp_pinctrl_probe,
	.driver = {
		.name = "usbdp_pinctrl",
#ifdef CONFIG_OF
		.of_match_table = usb_pinctrl_ids,
#endif
	},
};

module_platform_driver(usb_dp_pinctrl_driver);
