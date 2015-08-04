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
#include <linux/switch.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#define ADV7533_REG_CHIP_REVISION (0x00)
#define ADV7533_RESET_DELAY (100)

#define PINCTRL_STATE_ACTIVE    "pmx_adv7533_active"
#define PINCTRL_STATE_SUSPEND   "pmx_adv7533_suspend"

#define MDSS_MAX_PANEL_LEN      256

enum adv7533_i2c_addr {
	ADV7533_MAIN = 0x39, /* 7a main right shift 1 */
	ADV7533_CEC_DSI = 0x3C,
};

enum adv7533_video_mode {
	ADV7533_VIDEO_PATTERN,
	ADV7533_VIDEO_480P,
	ADV7533_VIDEO_720P,
	ADV7533_VIDEO_1080P,
};

enum adv7533_audio {
	ADV7533_AUDIO_OFF,
	ADV7533_AUDIO_ON,
};

struct adv7533_reg_cfg {
	u8 i2c_addr;
	u8 reg;
	u8 val;
};

struct adv7533_platform_data {
	u8 main_i2c_addr;
	u8 cec_dsi_i2c_addr;
	u8 video_mode;
	u8 audio;
	int irq;
	u32 irq_gpio;
	u32 irq_flags;
	int hpd_irq;
	u32 hpd_irq_gpio;
	u32 hpd_irq_flags;
	u32 switch_gpio;
	u32 switch_flags;
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	bool disable_gpios;
	bool adv_output;
	struct switch_dev audio_sdev;
};

static struct adv7533_reg_cfg setup_cfg[] = {
	{ADV7533_MAIN, 0x41, 0x10},		/* HDMI normal */
	{ADV7533_MAIN, 0xd6, 0x48},		/* HPD overriden */
	{ADV7533_CEC_DSI, 0x03, 0x89},		/* HDMI enabled */
	{ADV7533_MAIN, 0x16, 0x20},
	{ADV7533_MAIN, 0x9A, 0xE0},
	{ADV7533_MAIN, 0xBA, 0x70},
	{ADV7533_MAIN, 0xDE, 0x82},
	{ADV7533_MAIN, 0xE4, 0xC0},
	{ADV7533_MAIN, 0xE5, 0x80},
	{ADV7533_CEC_DSI, 0x15, 0xD0},
	{ADV7533_CEC_DSI, 0x17, 0xD0},
	{ADV7533_CEC_DSI, 0x24, 0x20},
	{ADV7533_CEC_DSI, 0x57, 0x11},
	/* hdmi or dvi mode: hdmi */
	{ADV7533_MAIN, 0xAF, 0x06},
	{ADV7533_MAIN, 0x40, 0x80},
	{ADV7533_MAIN, 0x4C, 0x04},
	{ADV7533_MAIN, 0x49, 0x02},
	{ADV7533_MAIN, 0x0D, 1 << 6},
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

static struct adv7533_reg_cfg tg_cfg_pattern_1080p[] = {
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
	/* Test Pattern Enable (0x55[7] = 1) */
	{ADV7533_CEC_DSI, 0x55, 0x80},
};

static struct adv7533_reg_cfg tg_cfg_720p[] = {
	{ADV7533_CEC_DSI, 0x1C, 0x30},
	/* hsync and vsync active low */
	{ADV7533_MAIN, 0x17, 0x02},
	/* Control for Pixel Clock Divider */
	{ADV7533_CEC_DSI, 0x16, 0x24},
	/* h_width 0x898 2200*/
	{ADV7533_CEC_DSI, 0x28, 0x67},
	{ADV7533_CEC_DSI, 0x29, 0x20},
	/* hsync_width 0x2C 44*/
	{ADV7533_CEC_DSI, 0x2A, 0x02},
	{ADV7533_CEC_DSI, 0x2B, 0x80},
	/* hfp 0x58 88 */
	{ADV7533_CEC_DSI, 0x2C, 0x06},
	{ADV7533_CEC_DSI, 0x2D, 0xE0},
	/* hbp 0x94 148 */
	{ADV7533_CEC_DSI, 0x2E, 0x0D},
	{ADV7533_CEC_DSI, 0x2F, 0xC0},
	/* v_total 0x465 1125 */
	{ADV7533_CEC_DSI, 0x30, 0x2E},
	{ADV7533_CEC_DSI, 0x31, 0xE0},
	/* vsync_width 0x05 5*/
	{ADV7533_CEC_DSI, 0x32, 0x00},
	{ADV7533_CEC_DSI, 0x33, 0x50},
	/* vfp 0x04 4  */
	{ADV7533_CEC_DSI, 0x34, 0x00},
	{ADV7533_CEC_DSI, 0x35, 0x50},
	/* vbp 0x24 36 */
	{ADV7533_CEC_DSI, 0x36, 0x01},
	{ADV7533_CEC_DSI, 0x37, 0x40},
	/* Test Pattern Disable (0x55[7] = 0) */
	{ADV7533_CEC_DSI, 0x55, 0x00},
	/* HDMI disabled */
	{ADV7533_CEC_DSI, 0x03, 0x09},
	/* HDMI enabled */
	{ADV7533_CEC_DSI, 0x03, 0x89},
};

static struct adv7533_reg_cfg tg_cfg_pattern_720p[] = {
	{ADV7533_CEC_DSI, 0x1C, 0x30},
	/* hsync and vsync active low */
	{ADV7533_MAIN, 0x17, 0x02},
	/* Control for Pixel Clock Divider */
	{ADV7533_CEC_DSI, 0x16, 0x24},
	/* h_width 0x898 2200*/
	{ADV7533_CEC_DSI, 0x28, 0x67},
	{ADV7533_CEC_DSI, 0x29, 0x20},
	/* hsync_width 0x2C 44*/
	{ADV7533_CEC_DSI, 0x2A, 0x02},
	{ADV7533_CEC_DSI, 0x2B, 0x80},
	/* hfp 0x58 88 */
	{ADV7533_CEC_DSI, 0x2C, 0x06},
	{ADV7533_CEC_DSI, 0x2D, 0xE0},
	/* hbp 0x94 148 */
	{ADV7533_CEC_DSI, 0x2E, 0x0D},
	{ADV7533_CEC_DSI, 0x2F, 0xC0},
	/* v_total 0x465 1125 */
	{ADV7533_CEC_DSI, 0x30, 0x2E},
	{ADV7533_CEC_DSI, 0x31, 0xE0},
	/* vsync_width 0x05 5*/
	{ADV7533_CEC_DSI, 0x32, 0x00},
	{ADV7533_CEC_DSI, 0x33, 0x50},
	/* vfp 0x04 4  */
	{ADV7533_CEC_DSI, 0x34, 0x00},
	{ADV7533_CEC_DSI, 0x35, 0x50},
	/* vbp 0x24 36 */
	{ADV7533_CEC_DSI, 0x36, 0x01},
	{ADV7533_CEC_DSI, 0x37, 0x40},
	/* DSI Internal Timing Generator Enable register bit (0x27[7] = 1) */
	{ADV7533_CEC_DSI, 0x27, 0x8B},
	/* Test Pattern Enable (0x55[7] = 1) */
	{ADV7533_CEC_DSI, 0x55, 0x80},
	/* DSI Internal Timing Generator Reset Enable
		register bit (0x27[6] = 0) */
	{ADV7533_CEC_DSI, 0x27, 0x8B},
	/* DSI Internal Timing Generator Reset Enable
		register bit (0x27[6] = 1) */
	{ADV7533_CEC_DSI, 0x27, 0xCB},

	{ADV7533_CEC_DSI, 0x03, 0x09},/* HDMI disabled */
	{ADV7533_CEC_DSI, 0x03, 0x89},/* HDMI enabled */
};

static struct adv7533_reg_cfg I2S_cfg[] = {
	{ADV7533_MAIN, 0x0D, 0x18},	/* Bit width = 16Bits*/
	{ADV7533_MAIN, 0x15, 0x20},	/* Sampling Frequency = 48kHz*/
	{ADV7533_MAIN, 0x02, 0x18},	/* N value 6144 --> 0x1800*/
	{ADV7533_MAIN, 0x14, 0x02},	/* Word Length = 16Bits*/
	{ADV7533_MAIN, 0x73, 0x01},	/* Channel Count = 2 channels */
};

static struct adv7533_reg_cfg irq_config[] = {
	{ADV7533_CEC_DSI, 0x38, 0xCE},
	{ADV7533_CEC_DSI, 0x38, 0xC0},
};

static struct i2c_client *client;
static struct adv7533_platform_data *pdata;
static char mdss_mdp_panel[MDSS_MAX_PANEL_LEN];

/*
 * If i2c read or write fails, wait for 100ms to try again, and try
 * max 3 times.
 */
#define MAX_WAIT_TIME (100)
#define MAX_RW_TRIES (3)

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

	ret = of_property_read_u32(np, "adv7533,main-addr", &temp_val);
	pr_debug("%s: DT property %s is %X\n", __func__, "adv7533,main-addr",
		temp_val);
	if (ret)
		goto end;
	pdata->main_i2c_addr = (u8)temp_val;

	ret = of_property_read_u32(np, "adv7533,cec-dsi-addr", &temp_val);
	pr_debug("%s: DT property %s is %X\n", __func__, "adv7533,cec-dsi-addr",
		temp_val);
	if (ret)
		goto end;
	pdata->cec_dsi_i2c_addr = (u8)temp_val;

	ret = of_property_read_u32(np, "adv7533,video-mode", &temp_val);
	pr_debug("%s: DT property %s is %X\n", __func__, "adv7533,video-mode",
		temp_val);
	if (ret)
		goto end;
	pdata->video_mode = (u8)temp_val;

	ret = of_property_read_u32(np, "adv7533,audio", &temp_val);
	pr_debug("%s: DT property %s is %X\n",
		__func__, "adv7533,audio", temp_val);
	if (ret)
		goto end;
	pdata->audio = (u8)temp_val;

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
			"adv7533,disable-gpios");

	if (!(pdata->disable_gpios)) {
		pdata->irq_gpio = of_get_named_gpio_flags(np,
				"adv7533,irq-gpio", 0, &pdata->irq_flags);

		pdata->hpd_irq_gpio = of_get_named_gpio_flags(np,
				"adv7533,hpd-irq-gpio", 0,
				&pdata->hpd_irq_flags);

		pdata->switch_gpio = of_get_named_gpio_flags(np,
				"adv7533,switch-gpio", 0, &pdata->switch_flags);
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

static void adv7533_get_cmdline_config(void)
{
	int len = 0;
	char *t;

	len = strlen(mdss_mdp_panel);

	if (len <= 0)
		return;

	t = strnstr(mdss_mdp_panel, "hdmi", MDSS_MAX_PANEL_LEN);
	if (t)
		pdata->adv_output = true;
	else
		pdata->adv_output = false;

	t = strnstr(mdss_mdp_panel, "720", MDSS_MAX_PANEL_LEN);
	if (t)
		pdata->video_mode = ADV7533_VIDEO_720P;

	t = strnstr(mdss_mdp_panel, "1080", MDSS_MAX_PANEL_LEN);
	if (t)
		pdata->video_mode = ADV7533_VIDEO_1080P;
}

static struct i2c_device_id adv7533_id[] = {
	{ "adv7533", 0},
	{}
};

static int adv7533_probe(struct i2c_client *client_,
						 const struct i2c_device_id *id)
{
	int ret = 0;

	client = client_;

	if (strnstr(mdss_mdp_panel, "no_display", MDSS_MAX_PANEL_LEN))
		 return 0;

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

	ret = pinctrl_select_state(pdata->ts_pinctrl,
		pdata->pinctrl_state_active);
	if (ret < 0)
		pr_err("%s: Failed to select %s pinstate %d\n",
			__func__, PINCTRL_STATE_ACTIVE, ret);

	pdata->adv_output = true;
	adv7533_get_cmdline_config();

	if (!(pdata->disable_gpios)) {
		ret = adv7533_gpio_configure(pdata, true);
		if (ret) {
			pr_err("%s: Failed to configure GPIOs\n", __func__);
			goto p_err;
		}

		if (pdata->adv_output) {
			gpio_set_value(pdata->switch_gpio, 0);
		} else {
			gpio_set_value(pdata->switch_gpio, 1);
			goto p_err;
		}
	}

	ret = adv7533_write_regs(pdata, setup_cfg, ARRAY_SIZE(setup_cfg));
	if (ret != 0) {
		pr_err("%s: Failed to write common config\n", __func__);
		goto p_err;
	}

	switch (pdata->video_mode) {
	case ADV7533_VIDEO_PATTERN:
		ret = adv7533_write_regs(pdata, tg_cfg_pattern_1080p,
			ARRAY_SIZE(tg_cfg_pattern_1080p));
		if (ret != 0) {
			pr_err("%s: adv7533 pattern config i2c fails [%d]\n",
				__func__, ret);
			goto p_err;
		}
		break;
	case ADV7533_VIDEO_480P:
	case ADV7533_VIDEO_720P:
		ret = adv7533_write_regs(pdata, tg_cfg_pattern_720p,
			ARRAY_SIZE(tg_cfg_pattern_720p));
		ret = adv7533_write_regs(pdata, tg_cfg_720p,
			ARRAY_SIZE(tg_cfg_720p));
		if (ret != 0) {
			pr_err("%s: adv7533 pattern config i2c fails [%d]\n",
				__func__, ret);
			goto p_err;
		}
		break;
	case ADV7533_VIDEO_1080P:
	default:
		ret = adv7533_write_regs(pdata, tg_cfg_1080p,
			ARRAY_SIZE(tg_cfg_1080p));
		if (ret != 0) {
			pr_err("%s: adv7533 1080p config i2c fails [%d]\n",
				__func__, ret);
			goto p_err;
		}
		break;
	}

	ret = adv7533_write_regs(pdata, irq_config, ARRAY_SIZE(irq_config));
	if (ret != 0) {
		pr_err("%s: adv7533 interrupt config i2c fails with ret = %d!\n",
			__func__, ret);
		goto p_err;
	}

	pdata->audio_sdev.name = "hdmi_audio";
	if (switch_dev_register(&pdata->audio_sdev) < 0) {
		pr_err("%s: hdmi_audio switch registration failed\n",
			__func__);
		ret = -ENODEV;
		goto p_err;
	}

	switch (pdata->audio) {
	case ADV7533_AUDIO_ON:
		ret = adv7533_write_regs(pdata, I2S_cfg, ARRAY_SIZE(I2S_cfg));
		if (ret != 0) {
			pr_err("%s: I2S configuration fail = %d!\n",
				__func__, ret);
			goto p_err;
		}
		switch_set_state(&pdata->audio_sdev, 1);
		break;
	case ADV7533_AUDIO_OFF:
	default:
		break;
	}

	pm_runtime_enable(&client->dev);
	pm_runtime_set_active(&client->dev);

	return 0;

p_err:
	adv7533_gpio_configure(pdata, false);
	devm_kfree(&client->dev, pdata);
	return ret;
}

static int adv7533_remove(struct i2c_client *client)
{
	int ret = 0;

	pm_runtime_disable(&client->dev);
	switch_dev_unregister(&pdata->audio_sdev);
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
