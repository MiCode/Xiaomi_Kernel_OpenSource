/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include "msm_dba_internal.h"

#define ADV7533_REG_CHIP_REVISION (0x00)
#define ADV7533_RESET_DELAY (100)

#define PINCTRL_STATE_ACTIVE    "pmx_adv7533_active"
#define PINCTRL_STATE_SUSPEND   "pmx_adv7533_suspend"

#define MDSS_MAX_PANEL_LEN      256
#define EDID_SEG_SIZE 0x100

#define HPD_INT_ENABLE           BIT(7)
#define MONITOR_SENSE_INT_ENABLE BIT(6)
#define EDID_READY_INT_ENABLE    BIT(2)
#define HDCP_BKSV_FLAG           BIT(6)
#define HDCP_ERROR               BIT(7)
#define CEC_TX_READY             BIT(5)
#define CEC_TX_ARB_LOST          BIT(4)
#define CEC_TX_RETRY_TIMEOUT     BIT(3)
#define CEC_TX_RX_BUF3_READY     BIT(2)
#define CEC_TX_RX_BUF2_READY     BIT(1)
#define CEC_TX_RX_BUF1_READY     BIT(0)

#define MAX_WAIT_TIME (100)
#define MAX_RW_TRIES (3)

#define MAX_OPERAND_SIZE	14
#define CEC_MSG_SIZE            (MAX_OPERAND_SIZE + 2)

enum adv7533_i2c_addr {
	ADV7533_MAIN = 0x39,
	ADV7533_CEC_DSI = 0x3C,
};

enum adv7533_video_mode {
	ADV7533_VIDEO_PATTERN,
	ADV7533_VIDEO_480P,
	ADV7533_VIDEO_720P,
	ADV7533_VIDEO_1080P,
};

enum adv7533_cec_buf {
	ADV7533_CEC_BUF1,
	ADV7533_CEC_BUF2,
	ADV7533_CEC_BUF3,
	ADV7533_CEC_BUF_MAX,
};

struct adv7533_reg_cfg {
	u8 i2c_addr;
	u8 reg;
	u8 val;
};

struct adv7533_cec_msg {
	u8 buf[CEC_MSG_SIZE];
	u8 timestamp;
	bool pending;
};

struct adv7533_platform_data {
	u8 main_i2c_addr;
	u8 cec_dsi_i2c_addr;
	u8 video_mode;
	int irq;
	u32 irq_gpio;
	u32 irq_flags;
	u32 hpd_irq_gpio;
	u32 hpd_irq_flags;
	u32 switch_gpio;
	u32 switch_flags;
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	bool audio;
	bool disable_gpios;
	bool adv_output;
	void *edid_data;
	u8 edid_buf[EDID_SEG_SIZE];
	struct workqueue_struct *workq;
	struct delayed_work adv7533_intr_work_id;
	struct msm_dba_device_info dev_info;
	msm_dba_cb client_cb;
	void *client_cb_data;
	struct adv7533_cec_msg cec_msg[ADV7533_CEC_BUF_MAX];
};

static struct adv7533_reg_cfg adv7533_init_setup[] = {
	/* power down */
	{ADV7533_MAIN, 0x41, 0x50},
	/* HPD override */
	{ADV7533_MAIN, 0xD6, 0x48},
	/* color space */
	{ADV7533_MAIN, 0x16, 0x20},
	/* Fixed */
	{ADV7533_MAIN, 0x9A, 0xE0},
	/* HDCP */
	{ADV7533_MAIN, 0xBA, 0x70},
	/* Fixed */
	{ADV7533_MAIN, 0xDE, 0x82},
	/* V1P2 */
	{ADV7533_MAIN, 0xE4, 0x40},
	/* Fixed */
	{ADV7533_MAIN, 0xE5, 0x80},
	/* Fixed */
	{ADV7533_CEC_DSI, 0x15, 0xD0},
	/* Fixed */
	{ADV7533_CEC_DSI, 0x17, 0xD0},
	/* Fixed */
	{ADV7533_CEC_DSI, 0x24, 0x20},
	/* Fixed */
	{ADV7533_CEC_DSI, 0x57, 0x11},
};

static struct adv7533_reg_cfg adv7533_video_setup[] = {
	/* power up */
	{ADV7533_MAIN, 0x41, 0x10},
	/* hdmi enable */
	{ADV7533_CEC_DSI, 0x03, 0x89},
	/* hdmi mode, hdcp */
	{ADV7533_MAIN, 0xAF, 0x96},
	/* color depth */
	{ADV7533_MAIN, 0x4C, 0x04},
	/* down dither */
	{ADV7533_MAIN, 0x49, 0x02},
};

static struct adv7533_reg_cfg tg_cfg_1080p[] = {
	/* 4 lanes */
	{ADV7533_CEC_DSI, 0x1C, 0x40},
	/* hsync and vsync active low */
	{ADV7533_MAIN, 0x17, 0x02},
	/* Control for Pixel Clock Divider */
	{ADV7533_CEC_DSI, 0x16, 0x00},
	/* Timing Generator Enable */
	{ADV7533_CEC_DSI, 0x27, 0xCB},
	/* h_width 0x898 2200*/
	{ADV7533_CEC_DSI, 0x28, 0x89},
	{ADV7533_CEC_DSI, 0x29, 0x80},
	/* hsync_width 0x2C 44*/
	{ADV7533_CEC_DSI, 0x2A, 0x02},
	{ADV7533_CEC_DSI, 0x2B, 0xC0},
	/* hfp 0x58 88 */
	{ADV7533_CEC_DSI, 0x2C, 0x05},
	{ADV7533_CEC_DSI, 0x2D, 0x80},
	/* hbp 0x94 148 */
	{ADV7533_CEC_DSI, 0x2E, 0x09},
	{ADV7533_CEC_DSI, 0x2F, 0x40},
	/* v_total 0x465 1125 */
	{ADV7533_CEC_DSI, 0x30, 0x46},
	{ADV7533_CEC_DSI, 0x31, 0x50},
	/* vsync_width 0x05 5*/
	{ADV7533_CEC_DSI, 0x32, 0x00},
	{ADV7533_CEC_DSI, 0x33, 0x50},
	/* vfp 0x04 4  */
	{ADV7533_CEC_DSI, 0x34, 0x00},
	{ADV7533_CEC_DSI, 0x35, 0x40},
	/* vbp 0x24 36 */
	{ADV7533_CEC_DSI, 0x36, 0x02},
	{ADV7533_CEC_DSI, 0x37, 0x40},
	/* Timing Generator Enable */
	{ADV7533_CEC_DSI, 0x27, 0xCB},
	{ADV7533_CEC_DSI, 0x27, 0x8B},
	{ADV7533_CEC_DSI, 0x27, 0xCB},
	/* Reset Internal Timing Generator */
	{ADV7533_MAIN, 0xAF, 0x16},
	/* HDMI Mode Select */
	{ADV7533_CEC_DSI, 0x78, 0x03},
	/* HDMI Output Enable */
	{ADV7533_MAIN, 0x40, 0x80},
	/* GC Packet Enable */
	{ADV7533_MAIN, 0x4C, 0x04},
	/* Colour Depth 24-bit per pixel */
	{ADV7533_MAIN, 0x49, 0x00},
	/* Down Dither Output 8-bit Colour Depth */
	{ADV7533_CEC_DSI, 0x05, 0xC8},
	/* ADI Required Write */
	{ADV7533_CEC_DSI, 0xBE, 0x3D},
	/* Test Pattern Disable (0x55[7] = 0) */
	{ADV7533_CEC_DSI, 0x55, 0x00},
};

static struct adv7533_reg_cfg adv7533_cec_en[] = {
	/* Fixed, clock gate disable */
	{ADV7533_CEC_DSI, 0x05, 0xC8},
	/* read divider(7:2) from calc */
	{ADV7533_CEC_DSI, 0xBE, 0x01},
};

static struct adv7533_reg_cfg adv7533_cec_tg_init[] = {
	/* TG programming for 19.2MHz, divider 25 */
	{ADV7533_CEC_DSI, 0xBE, 0x65},
	{ADV7533_CEC_DSI, 0xC1, 0x08},
	{ADV7533_CEC_DSI, 0xC2, 0x70},
	{ADV7533_CEC_DSI, 0xC3, 0x07},
	{ADV7533_CEC_DSI, 0xC4, 0xE0},
	{ADV7533_CEC_DSI, 0xC5, 0x09},
	{ADV7533_CEC_DSI, 0xC6, 0x00},
	{ADV7533_CEC_DSI, 0xC7, 0x06},
	{ADV7533_CEC_DSI, 0xC8, 0xF0},
	{ADV7533_CEC_DSI, 0xC9, 0x06},
	{ADV7533_CEC_DSI, 0xCA, 0x60},
	{ADV7533_CEC_DSI, 0xCB, 0x07},
	{ADV7533_CEC_DSI, 0xCC, 0x80},
	{ADV7533_CEC_DSI, 0xCD, 0x04},
	{ADV7533_CEC_DSI, 0xCE, 0x80},
	{ADV7533_CEC_DSI, 0xCF, 0x03},
	{ADV7533_CEC_DSI, 0xD0, 0xA8},
	{ADV7533_CEC_DSI, 0xD1, 0x05},
	{ADV7533_CEC_DSI, 0xD2, 0x58},
	{ADV7533_CEC_DSI, 0xD3, 0x01},
	{ADV7533_CEC_DSI, 0xD4, 0x20},
	{ADV7533_CEC_DSI, 0xD5, 0x02},
	{ADV7533_CEC_DSI, 0xD6, 0xD0},
	{ADV7533_CEC_DSI, 0xD7, 0x03},
	{ADV7533_CEC_DSI, 0xD8, 0x60},
	{ADV7533_CEC_DSI, 0xD9, 0x01},
	{ADV7533_CEC_DSI, 0xDA, 0xF8},
	{ADV7533_CEC_DSI, 0xDB, 0x06},
	{ADV7533_CEC_DSI, 0xDC, 0xC0},
	{ADV7533_CEC_DSI, 0xDE, 0x00},
	{ADV7533_CEC_DSI, 0xDF, 0x78},
	{ADV7533_CEC_DSI, 0xE1, 0x00},
	{ADV7533_CEC_DSI, 0xE2, 0x90},
	{ADV7533_CEC_DSI, 0xE3, 0x01},
	{ADV7533_CEC_DSI, 0xE4, 0xB0},
	{ADV7533_CEC_DSI, 0xE5, 0x02},
	{ADV7533_CEC_DSI, 0xE6, 0x40},
};

static struct adv7533_reg_cfg adv7533_cec_power[] = {
	/* cec power up */
	{ADV7533_MAIN, 0xE2, 0x00},
	/* hpd override */
	{ADV7533_MAIN, 0xD6, 0x48},
	/* edid reread */
	{ADV7533_MAIN, 0xC9, 0x13},
	/* read all CEC Rx Buffers */
	{ADV7533_CEC_DSI, 0xBA, 0x08},
	/* logical address0 0x04 */
	{ADV7533_CEC_DSI, 0xBC, 0x04},
	/* select logical address0 */
	{ADV7533_CEC_DSI, 0xBB, 0x10},
};

static struct adv7533_reg_cfg I2S_cfg[] = {
	{ADV7533_MAIN, 0x0D, 0x18},	/* Bit width = 16Bits*/
	{ADV7533_MAIN, 0x15, 0x20},	/* Sampling Frequency = 48kHz*/
	{ADV7533_MAIN, 0x02, 0x18},	/* N value 6144 --> 0x1800*/
	{ADV7533_MAIN, 0x14, 0x02},	/* Word Length = 16Bits*/
	{ADV7533_MAIN, 0x73, 0x01},	/* Channel Count = 2 channels */
};

static struct adv7533_reg_cfg irq_config[] = {
	{ADV7533_MAIN, 0x94, HPD_INT_ENABLE |
		MONITOR_SENSE_INT_ENABLE |
		EDID_READY_INT_ENABLE},
	{ADV7533_MAIN, 0x95, HDCP_BKSV_FLAG |
		HDCP_ERROR | CEC_TX_READY |
		CEC_TX_ARB_LOST | CEC_TX_RETRY_TIMEOUT |
		CEC_TX_RX_BUF3_READY | CEC_TX_RX_BUF2_READY |
		CEC_TX_RX_BUF1_READY},
};

static struct i2c_client *client;
static struct adv7533_platform_data *pdata;
static char mdss_mdp_panel[MDSS_MAX_PANEL_LEN];

static int adv7533_read(u8 addr, u8 reg, u8 *buf, u8 len)
{
	int ret = 0, i = 0;
	struct i2c_msg msg[2];

	if (!client) {
		pr_err("%s: no adv7533 i2c client\n", __func__);
		ret = -ENODEV;
		goto r_err;
	}

	if (NULL == buf) {
		pr_err("%s: no adv7533 i2c client\n", __func__);
		ret = -EINVAL;
		goto r_err;
	}

	client->addr = addr;

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = buf;

	do {
		if (i2c_transfer(client->adapter, msg, 2) == 2) {
			ret = 0;
			goto r_err;
		}
		msleep(MAX_WAIT_TIME);
	} while (++i < MAX_RW_TRIES);

	ret = -EIO;
	pr_err("%s adv7533 i2c read failed after %d tries\n", __func__,
		MAX_RW_TRIES);

r_err:
	return ret;
}

int adv7533_read_byte(u8 addr, u8 reg, u8 *buf)
{
	return adv7533_read(addr, reg, buf, 1);
}

static int adv7533_write_byte(u8 addr, u8 reg, u8 val)
{
	int ret = 0, i = 0;
	u8 buf[2] = {reg, val};
	struct i2c_msg msg[1];

	if (!client) {
		pr_err("%s: no adv7533 i2c client\n", __func__);
		ret = -ENODEV;
		goto w_err;
	}

	client->addr = addr;

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	do {
		if (i2c_transfer(client->adapter, msg, 1) >= 1) {
			ret = 0;
			goto w_err;
		}
		msleep(MAX_WAIT_TIME);
	} while (++i < MAX_RW_TRIES);

	ret = -EIO;
	pr_err("%s: adv7533 i2c write failed after %d tries\n", __func__,
		MAX_RW_TRIES);

w_err:
	if (ret != 0)
		pr_err("%s: Exiting with ret = %d after %d retries\n",
			__func__, ret, i);
	return ret;
}

static int adv7533_write_regs(struct adv7533_platform_data *pdata,
	struct adv7533_reg_cfg *cfg, int size)
{
	int ret = 0;
	int i;

	for (i = 0; i < size; i++) {
		switch (cfg[i].i2c_addr) {
		case ADV7533_MAIN:
			ret = adv7533_write_byte(pdata->main_i2c_addr,
				cfg[i].reg, cfg[i].val);
			if (ret != 0)
				pr_err("%s: adv7533_write_byte returned %d\n",
					__func__, ret);
			break;
		case ADV7533_CEC_DSI:
			ret = adv7533_write_byte(pdata->cec_dsi_i2c_addr,
				cfg[i].reg, cfg[i].val);
			if (ret != 0)
				pr_err("%s: adv7533_write_byte returned %d\n",
					__func__, ret);
			break;
		default:
			ret = -EINVAL;
			pr_err("%s: Default case? BUG!\n", __func__);
			break;
		}
		if (ret != 0) {
			pr_err("%s: adv7533 reg writes failed. ", __func__);
			pr_err("Last write %02X to %02X\n",
				cfg[i].val, cfg[i].reg);
			goto w_regs_fail;
		}
	}

w_regs_fail:
	if (ret != 0)
		pr_err("%s: Exiting with ret = %d after %d writes\n",
			__func__, ret, i);
	return ret;
}

static int adv7533_read_device_rev(void)
{
	u8 rev = 0;
	int ret;

	ret = adv7533_read_byte(ADV7533_MAIN, ADV7533_REG_CHIP_REVISION,
							&rev);

	if (!ret)
		pr_debug("%s: adv7533 revision 0x%X\n", __func__, rev);
	else
		pr_err("%s: adv7533 rev error\n", __func__);

	return ret;
}

static int adv7533_parse_dt(struct device *dev,
	struct adv7533_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 temp_val;
	int ret = 0;

	ret = of_property_read_u32(np, "adi,main-addr", &temp_val);
	pr_debug("%s: DT property %s is %X\n", __func__, "adi,main-addr",
		temp_val);
	if (ret)
		goto end;
	pdata->main_i2c_addr = (u8)temp_val;

	ret = of_property_read_u32(np, "adi,cec-dsi-addr", &temp_val);
	pr_debug("%s: DT property %s is %X\n", __func__, "adi,cec-dsi-addr",
		temp_val);
	if (ret)
		goto end;
	pdata->cec_dsi_i2c_addr = (u8)temp_val;

	ret = of_property_read_u32(np, "adi,video-mode", &temp_val);
	pr_debug("%s: DT property %s is %X\n", __func__, "adi,video-mode",
		temp_val);
	if (ret)
		goto end;
	pdata->video_mode = (u8)temp_val;

	pdata->audio = of_property_read_bool(np, "adi,enable-audio");

	/* Get pinctrl if target uses pinctrl */
	pdata->ts_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pdata->ts_pinctrl)) {
		ret = PTR_ERR(pdata->ts_pinctrl);
		pr_err("%s: Pincontrol DT property returned %X\n",
			__func__, ret);
	}

	pdata->pinctrl_state_active = pinctrl_lookup_state(pdata->ts_pinctrl,
		"pmx_adv7533_active");
	if (IS_ERR_OR_NULL(pdata->pinctrl_state_active)) {
		ret = PTR_ERR(pdata->pinctrl_state_active);
		pr_err("Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, ret);
	}

	pdata->pinctrl_state_suspend = pinctrl_lookup_state(pdata->ts_pinctrl,
		"pmx_adv7533_suspend");
	if (IS_ERR_OR_NULL(pdata->pinctrl_state_suspend)) {
		ret = PTR_ERR(pdata->pinctrl_state_suspend);
		pr_err("Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, ret);
	}

	pdata->disable_gpios = of_property_read_bool(np,
			"adi,disable-gpios");

	if (!(pdata->disable_gpios)) {
		pdata->irq_gpio = of_get_named_gpio_flags(np,
				"adi,irq-gpio", 0, &pdata->irq_flags);

		pdata->hpd_irq_gpio = of_get_named_gpio_flags(np,
				"adi,hpd-irq-gpio", 0,
				&pdata->hpd_irq_flags);

		pdata->switch_gpio = of_get_named_gpio_flags(np,
				"adi,switch-gpio", 0, &pdata->switch_flags);
	}

end:
	return ret;
}

static int adv7533_gpio_configure(struct adv7533_platform_data *pdata,
	bool on)
{
	int ret = 0;

	if (pdata->disable_gpios)
		return 0;

	if (on) {
		if (gpio_is_valid(pdata->irq_gpio)) {
			ret = gpio_request(pdata->irq_gpio, "adv7533_irq_gpio");
			if (ret) {
				pr_err("unable to request gpio [%d]\n",
					pdata->irq_gpio);
				goto err_none;
			}
			ret = gpio_direction_input(pdata->irq_gpio);
			if (ret) {
				pr_err("unable to set dir for gpio[%d]\n",
					pdata->irq_gpio);
				goto err_irq_gpio;
			}
		} else {
			pr_err("irq gpio not provided\n");
			goto err_none;
		}

		if (gpio_is_valid(pdata->hpd_irq_gpio)) {
			ret = gpio_request(pdata->hpd_irq_gpio,
				"adv7533_hpd_irq_gpio");
			if (ret) {
				pr_err("unable to request gpio [%d]\n",
					pdata->hpd_irq_gpio);
				goto err_irq_gpio;
			}
			ret = gpio_direction_input(pdata->hpd_irq_gpio);
			if (ret) {
				pr_err("unable to set dir for gpio[%d]\n",
					pdata->hpd_irq_gpio);
				goto err_hpd_irq_gpio;
			}
		} else {
			pr_err("hpd irq gpio not provided\n");
			goto err_irq_gpio;
		}

		if (gpio_is_valid(pdata->switch_gpio)) {
			ret = gpio_request(pdata->switch_gpio,
				"adv7533_switch_gpio");
			if (ret) {
				pr_err("unable to request gpio [%d]\n",
					pdata->switch_gpio);
				goto err_hpd_irq_gpio;
			}

			ret = gpio_direction_output(pdata->switch_gpio, 1);
			if (ret) {
				pr_err("unable to set dir for gpio [%d]\n",
					pdata->switch_gpio);
				goto err_switch_gpio;
			}

			gpio_set_value(pdata->switch_gpio, 1);
			msleep(ADV7533_RESET_DELAY);
		}

		return 0;
	} else {
		if (gpio_is_valid(pdata->irq_gpio))
			gpio_free(pdata->irq_gpio);
		if (gpio_is_valid(pdata->hpd_irq_gpio))
			gpio_free(pdata->hpd_irq_gpio);
		if (gpio_is_valid(pdata->switch_gpio))
			gpio_free(pdata->switch_gpio);

		return 0;
	}

err_switch_gpio:
	if (gpio_is_valid(pdata->switch_gpio))
		gpio_free(pdata->switch_gpio);
err_hpd_irq_gpio:
	if (gpio_is_valid(pdata->hpd_irq_gpio))
		gpio_free(pdata->hpd_irq_gpio);
err_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_none:
	return ret;
}


u32 adv7533_read_edid(u32 size, char *edid_buf)
{
	u32 ret = 0, ndx;
	u8 edid_addr;
	u32 read_size = size / 2;

	if (!edid_buf)
		return 0;

	pr_debug("%s: size %d\n", __func__, size);

	ret = adv7533_read(ADV7533_MAIN, 0x43, &edid_addr, 1);
	if (ret) {
		pr_err("%s: Error reading edid addr\n", __func__);
		goto end;
	}

	pr_debug("%s: edid address 0x%x\n", __func__, edid_addr);

	ret = adv7533_read(edid_addr >> 1, 0x00, edid_buf, read_size);
	if (ret) {
		pr_err("%s: Error reading edid 0\n", __func__);
		goto end;
	}

	ret = adv7533_read(edid_addr >> 1, read_size,
		edid_buf + read_size, read_size);
	if (ret) {
		pr_err("%s: Error reading edid 1\n", __func__);
		goto end;
	}

	for (ndx = 0; ndx < size; ndx += 4)
		pr_info("%s: EDID[%02x-%02x] %02x %02x %02x %02x\n",
			__func__, ndx, ndx + 3,
			edid_buf[ndx + 0], edid_buf[ndx + 1],
			edid_buf[ndx + 2], edid_buf[ndx + 3]);
end:
	return ret;
}

static int adv7533_cec_prepare_msg(u8 *msg, u32 size)
{
	int i, ret = -EINVAL;
	int op_sz;

	if (!msg) {
		pr_err("%s: ERROR: invalid msg\n", __func__);
		goto end;
	}

	if (size <= 0 || size > CEC_MSG_SIZE) {
		pr_err("%s: ERROR: invalid msg size\n", __func__);
		goto end;
	}

	/* operand size = total size - header size - opcode size */
	op_sz = size - 2;

	/* write header */
	ret = adv7533_write_byte(pdata->cec_dsi_i2c_addr,
		0x70, msg[0]);
	if (ret)
		goto end;

	/* write opcode */
	ret = adv7533_write_byte(pdata->cec_dsi_i2c_addr,
		0x71, msg[1]);
	if (ret)
		goto end;

	/* write operands */
	for (i = 0; i < op_sz && i < MAX_OPERAND_SIZE; i++) {
		ret = adv7533_write_byte(pdata->cec_dsi_i2c_addr,
			0x72 + i, msg[i + 2]);
		if (ret)
			goto end;
	}

	ret = adv7533_write_byte(pdata->cec_dsi_i2c_addr,
		0x80, size);
	if (ret)
		goto end;

end:
	return ret;
}

static int adv7533_rd_cec_msg(u8 *cec_buf, int msg_num)
{
	int ret = -EINVAL;

	if (!cec_buf) {
		pr_err("%s: Invalid msg buffer\n", __func__);
		goto end;
	}

	if (msg_num == ADV7533_CEC_BUF1) {
		ret = adv7533_read(ADV7533_CEC_DSI, 0x85, cec_buf,
			CEC_MSG_SIZE);
		if (ret) {
			pr_err("%s: Error reading cec buf %d\n",
				__func__, msg_num);
			goto end;
		}
	} else if (msg_num == ADV7533_CEC_BUF2) {
		ret = adv7533_read(ADV7533_CEC_DSI, 0x97, cec_buf,
			CEC_MSG_SIZE);
		if (ret) {
			pr_err("%s: Error reading cec buf %d\n",
				__func__, msg_num);
			goto end;
		}
	} else if (msg_num == ADV7533_CEC_BUF3) {
		ret = adv7533_read(ADV7533_CEC_DSI, 0xA8, cec_buf,
			CEC_MSG_SIZE);
		if (ret) {
			pr_err("%s: Error reading cec buf %d\n",
				__func__, msg_num);
			goto end;
		}
	} else {
		pr_err("%s: Invalid msg_num %d\n", __func__, msg_num);
	}
end:
	return ret;
}

static void adv7533_handle_hdcp_intr(u8 hdcp_status)
{
	/* HDCP ready for read */
	if (hdcp_status & BIT(6))
		pr_debug("%s: BKSV FLAG\n", __func__);

	/* check for HDCP error */
	if (hdcp_status & BIT(7)) {
		u8 ddc_status;
		pr_err("%s: HDCP ERROR\n", __func__);

		/* get error details */
		adv7533_read_byte(ADV7533_MAIN, 0xC8, &ddc_status);

		switch (ddc_status & 0xF0 >> 4) {
		case 0:
			pr_debug("%s: DDC: NO ERROR\n", __func__);
			break;
		case 1:
			pr_err("%s: DDC: BAD RX BKSV\n", __func__);
			break;
		case 2:
			pr_err("%s: DDC: Ri MISMATCH\n", __func__);
			break;
		case 3:
			pr_err("%s: DDC: Pj MISMATCH\n", __func__);
			break;
		case 4:
			pr_err("%s: DDC: I2C ERROR\n", __func__);
			break;
		case 5:
			pr_err("%s: DDC: TIMED OUT DS DONE\n", __func__);
			break;
		case 6:
			pr_err("%s: DDC: MAX CAS EXC\n", __func__);
			break;
		default:
			pr_debug("%s: DDC: UNKNOWN ERROR\n", __func__);
		}
	}
}

static void adv7533_handle_cec_intr(u8 cec_status)
{
	u8 cec_int_clear = 0x08;
	bool cec_rx_intr = false;
	u8 cec_rx_ready;
	u8 cec_rx_timestamp;

	if (cec_status & 0x07) {
		cec_rx_intr = true;
		if (adv7533_read_byte(ADV7533_CEC_DSI, 0xBA, &cec_int_clear)) {
			pr_err("%s: error reading cec clr int\n", __func__);
			cec_int_clear = 0x08;
		}
	}

	if (cec_status & BIT(5))
		pr_debug("%s: CEC TX READY\n", __func__);

	if (cec_status & BIT(4))
		pr_debug("%s: CEC TX Arbitration lost\n", __func__);

	if (cec_status & BIT(3))
		pr_debug("%s: CEC TX retry timout\n", __func__);

	if (!cec_rx_intr)
		return;


	if (adv7533_read_byte(ADV7533_CEC_DSI, 0xB9, &cec_rx_ready))
		return;

	if (adv7533_read_byte(ADV7533_CEC_DSI, 0x96, &cec_rx_timestamp))
		return;

	if (cec_rx_ready & BIT(0)) {
		pr_debug("%s: CEC Rx buffer 1 ready\n", __func__);
		adv7533_rd_cec_msg(
			pdata->cec_msg[ADV7533_CEC_BUF1].buf,
			ADV7533_CEC_BUF1);

		pdata->cec_msg[ADV7533_CEC_BUF1].pending = true;

		pdata->cec_msg[ADV7533_CEC_BUF1].timestamp =
			cec_rx_timestamp & (BIT(0) | BIT(1));

		pdata->client_cb(pdata->client_cb_data,
			MSM_DBA_CB_CEC_READ_PENDING);
	}

	if (cec_rx_ready & BIT(1)) {
		pr_debug("%s: CEC Rx buffer 2 ready\n", __func__);
		adv7533_rd_cec_msg(
			pdata->cec_msg[ADV7533_CEC_BUF2].buf,
			ADV7533_CEC_BUF2);

		pdata->cec_msg[ADV7533_CEC_BUF2].pending = true;

		pdata->cec_msg[ADV7533_CEC_BUF2].timestamp =
			cec_rx_timestamp & (BIT(2) | BIT(3));

		pdata->client_cb(pdata->client_cb_data,
			MSM_DBA_CB_CEC_READ_PENDING);
	}

	if (cec_rx_ready & BIT(2)) {
		pr_debug("%s: CEC Rx buffer 3 ready\n", __func__);
		adv7533_rd_cec_msg(
			pdata->cec_msg[ADV7533_CEC_BUF3].buf,
			ADV7533_CEC_BUF3);

		pdata->cec_msg[ADV7533_CEC_BUF3].pending = true;

		pdata->cec_msg[ADV7533_CEC_BUF3].timestamp =
			cec_rx_timestamp & (BIT(4) | BIT(5));

		pdata->client_cb(pdata->client_cb_data,
			MSM_DBA_CB_CEC_READ_PENDING);
	}

	if (adv7533_write_byte(pdata->cec_dsi_i2c_addr,
		0xBA, cec_int_clear | (cec_status & 0x07)))
		pr_err("%s: Failed: clear Rx intr\n", __func__);

	if (adv7533_write_byte(pdata->cec_dsi_i2c_addr,
		0xBA, cec_int_clear & ~0x07))
		pr_err("%s: Failed: clear Rx intr\n", __func__);
}

static void adv7533_intr_work(struct work_struct *work)
{
	int ret;
	u8 int_status  = 0xFF;
	u8 hdcp_cec_status = 0xFF;
	u8 int_state   = 0xFF;
	struct adv7533_platform_data *pdata;
	struct delayed_work *dw = to_delayed_work(work);

	pdata = container_of(dw, struct adv7533_platform_data,
			adv7533_intr_work_id);
	if (!pdata) {
		pr_err("%s: invalid input\n", __func__);
		goto reset;
	}

	/* READ Interrupt registers */
	if (adv7533_read_byte(ADV7533_MAIN, 0x96, &int_status))
		goto reset;

	if (adv7533_read_byte(ADV7533_MAIN, 0x97, &hdcp_cec_status))
		goto reset;

	if (adv7533_read_byte(ADV7533_MAIN, 0x42, &int_state))
		goto reset;

	if (int_status & BIT(6)) {
		if (int_state & BIT(5)) {
			pr_info("%s: Rx SENSE CABLE CONNECTED\n", __func__);

			/* initiate edid read in adv7533 */
			ret = adv7533_write_byte(pdata->main_i2c_addr,
				0xC9, 0x13);
			if (ret) {
				pr_err("%s: Failed: edid read adv %d\n",
					__func__, ret);
				pdata->client_cb(pdata->client_cb_data,
					MSM_DBA_CB_HPD_CONNECT);
			}

		} else {
			pr_info("%s: NO Rx SENSE CABLE DISCONNECTED\n",
				__func__);
			pdata->client_cb(pdata->client_cb_data,
				MSM_DBA_CB_HPD_DISCONNECT);
		}
	}

	/* EDID ready for read */
	if (int_status & BIT(2)) {
		pr_info("%s: EDID READY\n", __func__);

		ret = adv7533_read_edid(sizeof(pdata->edid_buf),
			pdata->edid_buf);
		if (ret)
			pr_err("%s: edid read failed\n", __func__);

		pdata->client_cb(pdata->client_cb_data,
			MSM_DBA_CB_HPD_CONNECT);
	}

	adv7533_handle_hdcp_intr(hdcp_cec_status);
	adv7533_handle_cec_intr(hdcp_cec_status);
reset:
	/* Clear interrupts */
	if (adv7533_write_byte(pdata->main_i2c_addr, 0x96, int_status))
		pr_err("%s: Failed: clear hpd intr\n", __func__);

	if (adv7533_write_byte(pdata->main_i2c_addr, 0x97, hdcp_cec_status))
		pr_err("%s: Failed: clear hdcp intr\n", __func__);

	/* Re-enable interrupts */
	if (adv7533_write_regs(pdata, irq_config, ARRAY_SIZE(irq_config)))
		pr_err("%s: Failed: irq config\n", __func__);
}

static irqreturn_t adv7533_irq(int irq, void *data)
{
	int ret;

	/* disable interrupts */
	ret = adv7533_write_byte(pdata->main_i2c_addr, 0x94, 0x00);
	if (ret)
		pr_err("%s: Failed: disable interrupts %d\n",
			__func__, ret);

	queue_delayed_work(pdata->workq, &pdata->adv7533_intr_work_id, 0);

	return IRQ_HANDLED;
}

static struct i2c_device_id adv7533_id[] = {
	{ "adv7533", 0},
	{}
};

static struct adv7533_platform_data *adv7533_get_platform_data(void *client)
{
	struct adv7533_platform_data *pdata = NULL;
	struct msm_dba_device_info *dev;
	struct msm_dba_client_info *cinfo =
		(struct msm_dba_client_info *)client;

	if (!cinfo) {
		pr_err("%s: invalid client data\n", __func__);
		goto end;
	}

	dev = cinfo->dev;
	if (!dev) {
		pr_err("%s: invalid device data\n", __func__);
		goto end;
	}

	pdata = container_of(dev, struct adv7533_platform_data, dev_info);
	if (!pdata)
		pr_err("%s: invalid platform data\n", __func__);

end:
	return pdata;
}

/* Device Operations */
static int adv7533_power_on(void *client, bool on, u32 flags)
{
	int ret = 0;
	struct adv7533_platform_data *pdata =
		adv7533_get_platform_data(client);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		goto end;
	}

	ret = adv7533_write_regs(pdata, adv7533_init_setup,
		ARRAY_SIZE(adv7533_init_setup));
	if (ret) {
		pr_err("%s: Failed to write common config\n",
			__func__);
		goto end;
	}

	ret = adv7533_write_regs(pdata, irq_config, ARRAY_SIZE(irq_config));
	if (ret) {
		pr_err("%s: Failed: irq config %d\n", __func__, ret);
		goto end;
	}

	queue_delayed_work(pdata->workq, &pdata->adv7533_intr_work_id, HZ/2);

end:
	return ret;
}

static int adv7533_video_on(void *client, bool on,
	struct msm_dba_video_cfg *cfg, u32 flags)
{
	int ret = 0;
	struct adv7533_platform_data *pdata =
		adv7533_get_platform_data(client);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		goto end;
	}

	ret = adv7533_write_regs(pdata, tg_cfg_1080p,
		ARRAY_SIZE(tg_cfg_1080p));
	if (ret) {
		pr_err("%s: err config 1080p %d\n", __func__, ret);
		goto end;
	}

	ret = adv7533_write_regs(pdata, adv7533_video_setup,
		ARRAY_SIZE(adv7533_video_setup));
	if (ret)
		pr_err("%s: Failed: video setup\n", __func__);
end:
	return ret;
}

static int adv7533_hdcp_enable(void *client, bool hdcp_on,
	bool enc_on, u32 flags)
{
	int ret = -EINVAL;
	u8 reg_val;

	if (adv7533_read_byte(pdata->main_i2c_addr, 0xAF, &reg_val))
		goto end;

	if (hdcp_on) {
		reg_val |= BIT(7);
		reg_val |= BIT(4);

		if (adv7533_write_byte(pdata->main_i2c_addr, 0xAF, reg_val))
			goto end;
	} else {
		reg_val &= ~BIT(7);
		reg_val &= ~BIT(4);

		if (adv7533_write_byte(pdata->main_i2c_addr, 0xAF, reg_val))
			goto end;
	}

	ret = 0;
end:
	return ret;
}

static int adv7533_configure_audio(void *client,
	struct msm_dba_audio_cfg *cfg, u32 flags)
{
	int ret = -EINVAL;
	int sampling_rate = 0;
	struct adv7533_platform_data *pdata =
		adv7533_get_platform_data(client);
	struct adv7533_reg_cfg reg_cfg[] = {
		{ADV7533_MAIN, 0x12, 0x00},
		{ADV7533_MAIN, 0x13, 0x00},
		{ADV7533_MAIN, 0x14, 0x00},
		{ADV7533_MAIN, 0x15, 0x00},
		{ADV7533_MAIN, 0x0A, 0x00},
		{ADV7533_MAIN, 0x0C, 0x00},
		{ADV7533_MAIN, 0x0D, 0x00},
		{ADV7533_MAIN, 0x03, 0x00},
		{ADV7533_MAIN, 0x02, 0x00},
		{ADV7533_MAIN, 0x01, 0x00},
		{ADV7533_MAIN, 0x09, 0x00},
		{ADV7533_MAIN, 0x08, 0x00},
		{ADV7533_MAIN, 0x07, 0x00},
		{ADV7533_MAIN, 0x73, 0x00},
		{ADV7533_MAIN, 0x76, 0x00},
	};

	if (!pdata || !cfg) {
		pr_err("%s: invalid data\n", __func__);
		goto end;
	}

	if (cfg->copyright == MSM_DBA_AUDIO_COPYRIGHT_NOT_PROTECTED)
		reg_cfg[0].val |= BIT(5);

	if (cfg->pre_emphasis == MSM_DBA_AUDIO_PRE_EMPHASIS_50_15us)
		reg_cfg[0].val |= BIT(2);

	if (cfg->clock_accuracy == MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL1)
		reg_cfg[0].val |= BIT(0);
	else if (cfg->clock_accuracy == MSM_DBA_AUDIO_CLOCK_ACCURACY_LVL3)
		reg_cfg[0].val |= BIT(1);

	reg_cfg[1].val = cfg->channel_status_category_code;

	reg_cfg[2].val = (cfg->channel_status_word_length & 0xF) << 0 |
		(cfg->channel_status_source_number & 0xF) << 4;

	if (cfg->sampling_rate == MSM_DBA_AUDIO_32KHZ)
		sampling_rate = 0x3;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_44P1KHZ)
		sampling_rate = 0x0;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_48KHZ)
		sampling_rate = 0x2;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_88P2KHZ)
		sampling_rate = 0x8;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_96KHZ)
		sampling_rate = 0xA;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_176P4KHZ)
		sampling_rate = 0xC;
	else if (cfg->sampling_rate == MSM_DBA_AUDIO_192KHZ)
		sampling_rate = 0xE;

	reg_cfg[3].val = (sampling_rate & 0xF) << 4;

	if (cfg->mode == MSM_DBA_AUDIO_MODE_MANUAL)
		reg_cfg[4].val |= BIT(7);

	if (cfg->interface == MSM_DBA_AUDIO_SPDIF_INTERFACE)
		reg_cfg[4].val |= BIT(4);

	if (cfg->interface == MSM_DBA_AUDIO_I2S_INTERFACE) {
		/* i2s enable */
		reg_cfg[5].val |= BIT(2);

		/* audio samp freq select */
		reg_cfg[5].val |= BIT(7);
	}

	/* format */
	reg_cfg[5].val |= cfg->i2s_fmt & 0x3;

	/* channel status override */
	reg_cfg[5].val |= (cfg->channel_status_source & 0x1) << 6;

	/* sample word lengths, default 24 */
	reg_cfg[6].val |= 0x18;

	/* endian order of incoming I2S data */
	if (cfg->word_endianness == MSM_DBA_AUDIO_WORD_LITTLE_ENDIAN)
		reg_cfg[6].val |= 0x1 << 7;

	/* compressed audio v - bit */
	reg_cfg[6].val |= (cfg->channel_status_v_bit & 0x1) << 5;

	/* ACR - N */
	reg_cfg[7].val |= (cfg->n & 0x000FF) >> 0;
	reg_cfg[8].val |= (cfg->n & 0x0FF00) >> 8;
	reg_cfg[9].val |= (cfg->n & 0xF0000) >> 16;

	/* ACR - CTS */
	reg_cfg[10].val |= (cfg->cts & 0x000FF) >> 0;
	reg_cfg[11].val |= (cfg->cts & 0x0FF00) >> 8;
	reg_cfg[12].val |= (cfg->cts & 0xF0000) >> 16;

	/* channel count */
	reg_cfg[13].val |= (cfg->channels & 0x3);

	/* CA */
	reg_cfg[14].val = cfg->channel_allocation;

	ret = adv7533_write_regs(pdata, reg_cfg, ARRAY_SIZE(reg_cfg));

end:
	return ret;
}

static int adv7533_hdmi_cec_write(void *client, u32 size,
	char *buf, u32 flags)
{
	int ret = -EINVAL;
	struct adv7533_platform_data *pdata;
	struct msm_dba_client_info *cinfo =
		(struct msm_dba_client_info *) client;
	struct msm_dba_device_info *dev;

	if (!cinfo || !buf) {
		pr_err("%s: invalid cinfo\n", __func__);
		goto end;
	}

	dev = cinfo->dev;

	if (!dev) {
		pr_err("%s: invalid device data\n", __func__);
		goto end;
	}

	pdata = container_of(dev, struct adv7533_platform_data,
			dev_info);
	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		goto end;
	}

	ret = adv7533_cec_prepare_msg(buf, size);
	if (ret)
		goto end;

	/* Enable CEC msg tx with NACK 3 retries */
	ret = adv7533_write_byte(pdata->cec_dsi_i2c_addr, 0x81, 0x07);
end:
	return ret;
}

static int adv7533_hdmi_cec_read(void *client,
	u32 *size, char *buf, u32 flags)
{
	int ret = -EINVAL;
	int i;
	struct adv7533_platform_data *pdata;
	struct msm_dba_client_info *cinfo =
		(struct msm_dba_client_info *)client;
	struct msm_dba_device_info *dev;

	if (!cinfo) {
		pr_err("%s: invalid cinfo\n", __func__);
		goto end;
	}

	dev = cinfo->dev;

	if (!dev) {
		pr_err("%s: invalid device data\n", __func__);
		goto end;
	}

	pdata = container_of(dev, struct adv7533_platform_data,
			dev_info);
	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		goto end;
	}

	for (i = 0; i < ADV7533_CEC_BUF_MAX; i++) {
		struct adv7533_cec_msg *msg = &pdata->cec_msg[i];

		if (msg->pending && !msg->timestamp) {
			memcpy(buf, msg->buf, CEC_MSG_SIZE);
			msg->pending = false;
			break;
		}
	}

	if (i < ADV7533_CEC_BUF_MAX) {
		*size = CEC_MSG_SIZE;
		ret = 0;
	} else {
		pr_err("%s: no pending cec msg\n", __func__);
		*size = 0;
	}
end:
	return ret;
}

static int adv7533_get_edid_size(void *client,
	u32 *size, u32 flags)
{
	int ret = 0;

	if (!size) {
		ret = -EINVAL;
		goto end;
	}

	*size = EDID_SEG_SIZE;

end:
	return ret;
}

static int adv7533_get_raw_edid(void *client,
	u32 size, char *buf, u32 flags)
{
	struct adv7533_platform_data *pdata =
		adv7533_get_platform_data(client);

	if (!pdata || !buf) {
		pr_err("%s: invalid data\n", __func__);
		goto end;
	}

	size = size > sizeof(pdata->edid_buf) ? sizeof(pdata->edid_buf) : size;

	memcpy(buf, pdata->edid_buf, size);
end:
	return 0;
}

static int adv7533_reg_fxn(struct msm_dba_client_info *cinfo)
{
	int ret = 0;
	struct adv7533_platform_data *pdata =
		adv7533_get_platform_data((void *)cinfo);

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		goto end;
	}

	pdata->client_cb = cinfo->cb;
	pdata->client_cb_data = cinfo->cb_data;

end:
	return ret;
}

static int adv7533_register_dba(struct adv7533_platform_data *pdata)
{
	struct msm_dba_ops *client_ops;

	if (!pdata)
		return -EINVAL;

	client_ops = &pdata->dev_info.client_ops;

	client_ops->power_on = adv7533_power_on;
	client_ops->video_on = adv7533_video_on;
	client_ops->configure_audio = adv7533_configure_audio;
	client_ops->hdcp_enable = adv7533_hdcp_enable;
	client_ops->hdmi_cec_write = adv7533_hdmi_cec_write;
	client_ops->hdmi_cec_read = adv7533_hdmi_cec_read;
	client_ops->get_edid_size = adv7533_get_edid_size;
	client_ops->get_raw_edid = adv7533_get_raw_edid;

	strlcpy(pdata->dev_info.chip_name, "adv7533",
		sizeof(pdata->dev_info.chip_name));

	pdata->dev_info.instance_id = 0;
	pdata->dev_info.reg_fxn = adv7533_reg_fxn;

	mutex_init(&pdata->dev_info.dev_mutex);

	INIT_LIST_HEAD(&pdata->dev_info.client_list);

	return msm_dba_add_probed_device(&pdata->dev_info);
}


static int adv7533_probe(struct i2c_client *client_,
	 const struct i2c_device_id *id)
{
	int ret = 0;
	client = client_;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct adv7533_platform_data), GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}

		ret = adv7533_parse_dt(&client->dev, pdata);
		if (ret) {
			pr_err("%s: Failed to parse DT\n", __func__);
			goto p_err;
		}
	}

	ret = adv7533_read_device_rev();
	if (ret != 0) {
		pr_err("%s: Failed to read revision\n", __func__);
		goto p_err;
	}

	ret = adv7533_register_dba(pdata);
	if (ret)
		pr_err("%s: Error registering with DBA %d\n",
			__func__, ret);

	ret = pinctrl_select_state(pdata->ts_pinctrl,
		pdata->pinctrl_state_active);
	if (ret < 0)
		pr_err("%s: Failed to select %s pinstate %d\n",
			__func__, PINCTRL_STATE_ACTIVE, ret);

	ret = adv7533_gpio_configure(pdata, true);
	if (ret) {
		pr_err("%s: Failed to configure GPIOs\n", __func__);
		goto p_err;
	}

	gpio_set_value(pdata->switch_gpio, 0);

	pdata->irq = gpio_to_irq(pdata->irq_gpio);

	ret = request_threaded_irq(pdata->irq, NULL, adv7533_irq,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "adv7533", pdata);
	if (ret) {
		pr_err("%s: Failed to enable ADV7533 interrupt\n",
			__func__);
		goto p_err;
	}

	if (pdata->audio) {
		ret = adv7533_write_regs(pdata, I2S_cfg, ARRAY_SIZE(I2S_cfg));
		if (ret != 0) {
			pr_err("%s: I2S configuration fail = %d!\n",
				__func__, ret);
			goto p_err;
		}
	}

	pdata->workq = create_workqueue("adv7533_workq");
	if (!pdata->workq) {
		pr_err("%s: workqueue creation failed.\n", __func__);
		return -EPERM;
	}

	INIT_DELAYED_WORK(&pdata->adv7533_intr_work_id, adv7533_intr_work);

	ret = adv7533_write_regs(pdata, adv7533_cec_en,
		ARRAY_SIZE(adv7533_cec_en));
	if (ret)
		pr_err("%s: Failed to write cec en config\n",
			__func__);

	ret = adv7533_write_regs(pdata, adv7533_cec_tg_init,
		ARRAY_SIZE(adv7533_cec_tg_init));
	if (ret)
		pr_err("%s: Failed to write cec tg init\n",
			__func__);

	ret = adv7533_write_regs(pdata, adv7533_cec_power,
		ARRAY_SIZE(adv7533_cec_power));
	if (ret)
		pr_err("%s: Failed to write cec power\n",
			__func__);

	pm_runtime_enable(&client->dev);
	pm_runtime_set_active(&client->dev);

	return 0;

p_err:
	disable_irq(pdata->irq);
	free_irq(pdata->irq, pdata);
	adv7533_gpio_configure(pdata, false);

	devm_kfree(&client->dev, pdata);
	return ret;
}

static int adv7533_remove(struct i2c_client *client)
{
	int ret = 0;

	pm_runtime_disable(&client->dev);
	disable_irq(pdata->irq);
	free_irq(pdata->irq, pdata);

	ret = adv7533_gpio_configure(pdata, false);

	devm_kfree(&client->dev, pdata);

	return ret;
}

static struct i2c_driver adv7533_driver = {
	.driver = {
		.name = "adv7533",
		.owner = THIS_MODULE,
	},
	.probe = adv7533_probe,
	.remove = adv7533_remove,
	.id_table = adv7533_id,
};

static int __init adv7533_init(void)
{
	return i2c_add_driver(&adv7533_driver);
}

static void __exit adv7533_exit(void)
{
	i2c_del_driver(&adv7533_driver);
}

module_param_string(panel, mdss_mdp_panel, MDSS_MAX_PANEL_LEN, 0);

module_init(adv7533_init);
module_exit(adv7533_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("adv7533 driver");
