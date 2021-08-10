// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2016, 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <soc/soundwire.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <dsp/q6afe-v2.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <soc/internal.h>
#include <linux/regmap.h>
#include <asoc/msm-cdc-pinctrl.h>
#include "wsa881x-analog.h"
#include "wsa881x-temp-sensor.h"

#define SPK_GAIN_12DB 4
#define WIDGET_NAME_MAX_SIZE 80
#define REGMAP_REGISTER_CHECK_RETRY 30

#define MAX_NAME_LEN 30
#define WSA881X_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 |\
			SNDRV_PCM_RATE_384000)
/* Fractional Rates */
#define WSA881X_FRAC_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 |\
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800)

#define WSA881X_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S24_LE |\
			SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

/*
 * Private data Structure for wsa881x. All parameters related to
 * WSA881X codec needs to be defined here.
 */
struct wsa881x_pdata {
	struct regmap *regmap[2];
	struct i2c_client *client[2];
	struct snd_soc_component *component;

	/* track wsa881x status during probe */
	int status;
	bool boost_enable;
	bool visense_enable;
	int spk_pa_gain;
	struct i2c_msg xfer_msg[2];
	struct mutex xfer_lock;
	bool regmap_flag;
	bool wsa_active;
	int index;
	struct wsa881x_tz_priv tz_pdata;
	struct clk *wsa_mclk;
	int bg_cnt;
	int clk_cnt;
	int enable_cnt;
	int version;
	struct mutex bg_lock;
	struct mutex res_lock;
	struct delayed_work ocp_ctl_work;
	struct device_node *wsa_vi_gpio_p;
	struct device_node *wsa_clk_gpio_p;
	struct device_node *wsa_reset_gpio_p;
	char *wsa881x_name_prefix;
	struct snd_soc_dai_driver *dai_driver;
	struct snd_soc_component_driver *driver;
};

enum {
	WSA881X_STATUS_PROBING,
	WSA881X_STATUS_I2C,
};

#define WSA881X_OCP_CTL_TIMER_SEC 2
#define WSA881X_OCP_CTL_TEMP_CELSIUS 25
#define WSA881X_OCP_CTL_POLL_TIMER_SEC 60

static int wsa881x_ocp_poll_timer_sec = WSA881X_OCP_CTL_POLL_TIMER_SEC;
module_param(wsa881x_ocp_poll_timer_sec, int, 0664);
MODULE_PARM_DESC(wsa881x_ocp_poll_timer_sec, "timer for ocp ctl polling");

static int32_t wsa881x_resource_acquire(struct snd_soc_component *component,
						bool enable);

const char *wsa_tz_names[] = {"wsa881x.0e", "wsa881x.0f"};

static struct wsa881x_pdata wsa_pdata[MAX_WSA881X_DEVICE];

static bool pinctrl_init;

static int wsa881x_populate_dt_pdata(struct device *dev, int wsa881x_index);
static int wsa881x_reset(struct wsa881x_pdata *pdata, bool enable);
static int wsa881x_startup(struct wsa881x_pdata *pdata);
static int wsa881x_shutdown(struct wsa881x_pdata *pdata);

static int delay_array_msec[] = {10, 20, 30, 40, 50};

static int wsa881x_i2c_addr = -1;
static int wsa881x_probing_count;
static int wsa881x_presence_count;

static const char * const wsa881x_spk_pa_gain_text[] = {
"POS_13P5_DB", "POS_12_DB", "POS_10P5_DB", "POS_9_DB", "POS_7P5_DB",
"POS_6_DB", "POS_4P5_DB", "POS_3_DB", "POS_1P5_DB", "POS_0_DB"};

static const struct soc_enum wsa881x_spk_pa_gain_enum[] = {
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(wsa881x_spk_pa_gain_text),
				    wsa881x_spk_pa_gain_text),
};

static int wsa881x_spk_pa_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wsa881x_pdata *wsa881x =
			snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wsa881x->spk_pa_gain;

	dev_dbg(component->dev, "%s: spk_pa_gain = %ld\n", __func__,
				ucontrol->value.integer.value[0]);

	return 0;
}

static int wsa881x_spk_pa_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wsa881x_pdata *wsa881x =
			snd_soc_component_get_drvdata(component);

	if (ucontrol->value.integer.value[0] < 0 ||
		ucontrol->value.integer.value[0] > 0xC) {
		dev_err(component->dev, "%s: Unsupported gain val %ld\n",
			 __func__, ucontrol->value.integer.value[0]);
		return -EINVAL;
	}
	wsa881x->spk_pa_gain = ucontrol->value.integer.value[0];
	dev_dbg(component->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
			 __func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int get_i2c_wsa881x_device_index(u16 reg)
{
	u16 mask = 0x0f00;
	int value = 0;

	value = ((reg & mask) >> 8) & 0x000f;

	switch (value) {
	case 0:
		return 0;
	case 1:
		return 1;
	default:
		break;
	}
	return -EINVAL;
}

static int wsa881x_i2c_write_device(struct wsa881x_pdata *wsa881x,
			unsigned int reg, unsigned int val)
{
	int i = 0, rc = 0;
	int wsa881x_index;
	struct i2c_msg *msg;
	int ret = 0;
	int bytes = 1;
	u8 reg_addr = 0;
	u8 data[2];

	wsa881x_index = get_i2c_wsa881x_device_index(reg);
	if (wsa881x_index < 0) {
		pr_err_ratelimited("%s:invalid register to write\n", __func__);
		return -EINVAL;
	}
	if (wsa881x->regmap_flag) {
		rc = regmap_write(wsa881x->regmap[wsa881x_index], reg, val);
		for (i = 0; rc && i < ARRAY_SIZE(delay_array_msec); i++) {
			pr_err_ratelimited("Failed writing reg=%u-retry(%d)\n",
							reg, i);
			/* retry after delay of increasing order */
			msleep(delay_array_msec[i]);
			rc = regmap_write(wsa881x->regmap[wsa881x_index],
								reg, val);
		}
		if (rc)
			pr_err_ratelimited("Failed writing reg=%u rc=%d\n",
							reg, rc);
		else
			pr_debug("write success register = %x val = %x\n",
							reg, val);
	} else {
		reg_addr = (u8)reg;
		msg = &wsa881x->xfer_msg[0];
		msg->addr = wsa881x->client[wsa881x_index]->addr;
		msg->len = bytes + 1;
		msg->flags = 0;
		data[0] = reg;
		data[1] = (u8)val;
		msg->buf = data;
		ret = i2c_transfer(wsa881x->client[wsa881x_index]->adapter,
						wsa881x->xfer_msg, 1);
		/* Try again if the write fails */
		if (ret != 1) {
			ret = i2c_transfer(
					wsa881x->client[wsa881x_index]->adapter,
							wsa881x->xfer_msg, 1);
			if (ret != 1) {
				pr_err_ratelimited("failed to write the device\n");
				return ret;
			}
		}
		pr_debug("write success reg = %x val = %x\n", reg, data[1]);
	}
	return rc;
}

static int wsa881x_i2c_read_device(struct wsa881x_pdata *wsa881x,
				unsigned int reg)
{
	int wsa881x_index;
	int i = 0, rc = 0;
	unsigned int val;
	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	u8 dest[5] = {0};

	wsa881x_index = get_i2c_wsa881x_device_index(reg);
	if (wsa881x_index < 0) {
		pr_err_ratelimited("%s:invalid register to read\n", __func__);
		return -EINVAL;
	}
	if (wsa881x->regmap_flag) {
		rc = regmap_read(wsa881x->regmap[wsa881x_index], reg, &val);
		for (i = 0; rc && i < ARRAY_SIZE(delay_array_msec); i++) {
			pr_err_ratelimited("Failed reading reg=%u - retry(%d)\n",
								reg, i);
			/* retry after delay of increasing order */
			msleep(delay_array_msec[i]);
			rc = regmap_read(wsa881x->regmap[wsa881x_index],
						reg, &val);
		}
		if (rc) {
			pr_err_ratelimited("Failed reading reg=%u rc=%d\n",
								 reg, rc);
			return rc;
		}
		pr_debug("read success reg = %x val = %x\n",
						reg, val);
	} else {
		reg_addr = (u8)reg;
		msg = &wsa881x->xfer_msg[0];
		msg->addr = wsa881x->client[wsa881x_index]->addr;
		msg->len = 1;
		msg->flags = 0;
		msg->buf = &reg_addr;

		msg = &wsa881x->xfer_msg[1];
		msg->addr = wsa881x->client[wsa881x_index]->addr;
		msg->len = 1;
		msg->flags = I2C_M_RD;
		msg->buf = dest;
		ret = i2c_transfer(wsa881x->client[wsa881x_index]->adapter,
					wsa881x->xfer_msg, 2);

		/* Try again if read fails first time */
		if (ret != 2) {
			ret = i2c_transfer(
				wsa881x->client[wsa881x_index]->adapter,
						wsa881x->xfer_msg, 2);
			if (ret != 2) {
				pr_err_ratelimited("failed to read wsa register:%d\n",
								reg);
				return ret;
			}
		}
		val = dest[0];
	}
	return val;
}

static unsigned int wsa881x_i2c_read(struct snd_soc_component *component,
				unsigned int reg)
{
	struct wsa881x_pdata *wsa881x;
	int wsa881x_index;

	if (component == NULL) {
		pr_err_ratelimited("%s: invalid component\n", __func__);
		return -EINVAL;
	}
	wsa881x = snd_soc_component_get_drvdata(component);
	if (!wsa881x->wsa_active)
		return 0;

	wsa881x_index = get_i2c_wsa881x_device_index(reg);
	if (wsa881x_index < 0) {
		pr_err_ratelimited("%s:invalid register to read\n", __func__);
		return -EINVAL;
	}
	return wsa881x_i2c_read_device(wsa881x, reg);
}

static int wsa881x_i2c_write(struct snd_soc_component *component,
			unsigned int reg,
			unsigned int val)
{
	struct wsa881x_pdata *wsa881x;
	int wsa881x_index;

	if (component == NULL) {
		pr_err_ratelimited("%s: invalid component\n", __func__);
		return -EINVAL;
	}
	wsa881x = snd_soc_component_get_drvdata(component);
	if (!wsa881x->wsa_active)
		return 0;

	wsa881x_index = get_i2c_wsa881x_device_index(reg);
	if (wsa881x_index < 0) {
		pr_err_ratelimited("%s:invalid register to read\n", __func__);
		return -EINVAL;
	}
	return wsa881x_i2c_write_device(wsa881x, reg, val);
}

static int wsa881x_i2c_get_client_index(struct i2c_client *client,
					int *wsa881x_index)
{
	int ret = 0;

	switch (client->addr) {
	case WSA881X_I2C_SPK0_SLAVE0_ADDR:
	case WSA881X_I2C_SPK0_SLAVE1_ADDR:
		*wsa881x_index = WSA881X_I2C_SPK0_SLAVE0;
	break;
	case WSA881X_I2C_SPK1_SLAVE0_ADDR:
	case WSA881X_I2C_SPK1_SLAVE1_ADDR:
		*wsa881x_index = WSA881X_I2C_SPK1_SLAVE0;
	break;
	default:
		ret = -EINVAL;
	break;
	}
	return ret;
}

static int wsa881x_boost_ctrl(struct snd_soc_component *component, bool enable)
{
	struct wsa881x_pdata *wsa881x =
			snd_soc_component_get_drvdata(component);


	pr_debug("%s: enable:%d\n", __func__, enable);
	if (enable) {
		if (!WSA881X_IS_2_0(wsa881x->version)) {
			snd_soc_component_update_bits(component,
						WSA881X_ANA_CTL, 0x01, 0x01);
			snd_soc_component_update_bits(component,
						WSA881X_ANA_CTL, 0x04, 0x04);
			snd_soc_component_update_bits(component,
						WSA881X_BOOST_PS_CTL,
						0x40, 0x00);
			snd_soc_component_update_bits(component,
						WSA881X_BOOST_PRESET_OUT1,
						0xF0, 0xB0);
			snd_soc_component_update_bits(component,
						WSA881X_BOOST_ZX_CTL,
						0x20, 0x00);
			snd_soc_component_update_bits(component,
						WSA881X_BOOST_EN_CTL,
						0x80, 0x80);
		} else {
			snd_soc_component_update_bits(component,
						WSA881X_BOOST_LOOP_STABILITY,
						0x03, 0x03);
			snd_soc_component_update_bits(component,
						WSA881X_BOOST_MISC2_CTL,
						0xFF, 0x14);
			snd_soc_component_update_bits(component,
						WSA881X_BOOST_START_CTL,
						0x80, 0x80);
			snd_soc_component_update_bits(component,
						WSA881X_BOOST_START_CTL,
						0x03, 0x00);
			snd_soc_component_update_bits(component,
					WSA881X_BOOST_SLOPE_COMP_ISENSE_FB,
					0x0C, 0x04);
			snd_soc_component_update_bits(component,
					WSA881X_BOOST_SLOPE_COMP_ISENSE_FB,
					0x03, 0x00);
			if (snd_soc_component_read32(component, WSA881X_OTP_REG_0))
				snd_soc_component_update_bits(component,
					WSA881X_BOOST_PRESET_OUT1,
					0xF0, 0x70);
			else
				snd_soc_component_update_bits(component,
					WSA881X_BOOST_PRESET_OUT1,
					0xF0, 0xB0);
			snd_soc_component_update_bits(component,
						WSA881X_ANA_CTL, 0x03, 0x01);
			snd_soc_component_update_bits(component,
						WSA881X_SPKR_DRV_EN,
						0x08, 0x08);
			snd_soc_component_update_bits(component,
						WSA881X_ANA_CTL, 0x04, 0x04);
			snd_soc_component_update_bits(component,
						WSA881X_BOOST_CURRENT_LIMIT,
						0x0F, 0x08);
			snd_soc_component_update_bits(component,
						WSA881X_BOOST_EN_CTL,
						0x80, 0x80);
		}
		/* For WSA8810, start-up time is 1500us as per qcrg sequence */
		usleep_range(1500, 1510);
	} else {
		/* ENSURE: Class-D amp is shutdown. CLK is still on */
		snd_soc_component_update_bits(component,
					WSA881X_BOOST_EN_CTL, 0x80, 0x00);
		/* boost settle time is 1500us as per qcrg sequence */
		usleep_range(1500, 1510);
	}
	return 0;
}

static int wsa881x_visense_txfe_ctrl(struct snd_soc_component *component,
				     bool enable,
				     u8 isense1_gain, u8 isense2_gain,
				     u8 vsense_gain)
{
	u8 value = 0;
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);

	pr_debug("%s: enable:%d\n", __func__, enable);

	if (enable) {
		if (WSA881X_IS_2_0(wsa881x->version)) {
			snd_soc_component_update_bits(component,
						WSA881X_OTP_REG_28,
						0x3F, 0x3A);
			snd_soc_component_update_bits(component,
						WSA881X_BONGO_RESRV_REG1,
						0xFF, 0xB2);
			snd_soc_component_update_bits(component,
						WSA881X_BONGO_RESRV_REG2,
						0xFF, 0x05);
		}
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_PROT_FE_VSENSE_VCM,
					0x08, 0x00);
		if (WSA881X_IS_2_0(wsa881x->version)) {
			snd_soc_component_update_bits(component,
					WSA881X_SPKR_PROT_ATEST2,
					0x1C, 0x04);
		} else {
			snd_soc_component_update_bits(component,
					WSA881X_SPKR_PROT_ATEST2,
					0x08, 0x08);
			snd_soc_component_update_bits(component,
					WSA881X_SPKR_PROT_ATEST2,
					0x02, 0x02);
		}
		value = ((isense2_gain << 6) | (isense1_gain << 4) |
			(vsense_gain << 3));
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_PROT_FE_GAIN,
					0xF8, value);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_PROT_FE_GAIN,
					0x01, 0x01);
	} else {
		if (WSA881X_IS_2_0(wsa881x->version))
			snd_soc_component_update_bits(component,
				WSA881X_SPKR_PROT_FE_VSENSE_VCM, 0x10, 0x10);
		else
			snd_soc_component_update_bits(component,
				WSA881X_SPKR_PROT_FE_VSENSE_VCM, 0x08, 0x08);
		/*
		 * 200us sleep is needed after visense txfe disable as per
		 * HW requirement.
		 */
		usleep_range(200, 210);

		snd_soc_component_update_bits(component,
					WSA881X_SPKR_PROT_FE_GAIN,
					0x01, 0x00);
	}
	return 0;
}

static int wsa881x_visense_adc_ctrl(struct snd_soc_component *component,
				    bool enable)
{
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);

	pr_debug("%s: enable:%d\n", __func__, enable);
	if (enable) {
		if (!WSA881X_IS_2_0(wsa881x->version))
			snd_soc_component_update_bits(component,
					WSA881X_ADC_SEL_IBIAS,
					0x70, 0x40);
			snd_soc_component_update_bits(component,
					WSA881X_ADC_EN_SEL_IBIAS,
					0x07, 0x04);
			snd_soc_component_update_bits(component,
					WSA881X_ADC_EN_MODU_V, 0x80, 0x80);
			snd_soc_component_update_bits(component,
					WSA881X_ADC_EN_MODU_I, 0x80, 0x80);
	} else {
		/* Ensure: Speaker Protection has been stopped */
		snd_soc_component_update_bits(component,
					WSA881X_ADC_EN_MODU_V, 0x80, 0x00);
		snd_soc_component_update_bits(component,
					WSA881X_ADC_EN_MODU_I, 0x80, 0x00);
	}

	return 0;
}

static void wsa881x_bandgap_ctrl(struct snd_soc_component *component,
				 bool enable)
{
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s: enable:%d, bg_count:%d\n", __func__,
		enable, wsa881x->bg_cnt);
	mutex_lock(&wsa881x->bg_lock);
	if (enable) {
		++wsa881x->bg_cnt;
		if (wsa881x->bg_cnt == 1) {
			snd_soc_component_update_bits(component,
						WSA881X_TEMP_OP, 0x08, 0x08);
			/* 400usec sleep is needed as per HW requirement */
			usleep_range(400, 410);
			snd_soc_component_update_bits(component,
						WSA881X_TEMP_OP, 0x04, 0x04);
		}
	} else {
		--wsa881x->bg_cnt;
		if (wsa881x->bg_cnt <= 0) {
			wsa881x->bg_cnt = 0;
			snd_soc_component_update_bits(component,
						WSA881X_TEMP_OP, 0x04, 0x00);
			snd_soc_component_update_bits(component,
						WSA881X_TEMP_OP, 0x08, 0x00);
		}
	}
	mutex_unlock(&wsa881x->bg_lock);
}

static void wsa881x_clk_ctrl(struct snd_soc_component *component, bool enable)
{
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s:ss enable:%d, clk_count:%d\n", __func__,
		enable, wsa881x->clk_cnt);
	mutex_lock(&wsa881x->res_lock);
	if (enable) {
		++wsa881x->clk_cnt;
		if (wsa881x->clk_cnt == 1) {
			snd_soc_component_write(component,
					WSA881X_CDC_RST_CTL, 0x02);
			snd_soc_component_write(component,
					WSA881X_CDC_RST_CTL, 0x03);
			snd_soc_component_write(component,
					WSA881X_CLOCK_CONFIG, 0x01);

			snd_soc_component_write(component,
					WSA881X_CDC_DIG_CLK_CTL, 0x01);
			snd_soc_component_write(component,
					WSA881X_CDC_ANA_CLK_CTL, 0x01);

		}
	} else {
		--wsa881x->clk_cnt;
		if (wsa881x->clk_cnt <= 0) {
			wsa881x->clk_cnt = 0;
			snd_soc_component_write(component,
					WSA881X_CDC_ANA_CLK_CTL, 0x00);
			snd_soc_component_write(component,
					WSA881X_CDC_DIG_CLK_CTL, 0x00);
			if (WSA881X_IS_2_0(wsa881x->version))
				snd_soc_component_update_bits(component,
					WSA881X_CDC_TOP_CLK_CTL, 0x01, 0x00);
		}
	}
	mutex_unlock(&wsa881x->res_lock);
}

static int wsa881x_rdac_ctrl(struct snd_soc_component *component, bool enable)
{
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);

	pr_debug("%s: enable:%d\n", __func__, enable);
	if (enable) {
		snd_soc_component_update_bits(component,
					WSA881X_ANA_CTL, 0x08, 0x00);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_DRV_GAIN, 0x08, 0x08);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_DAC_CTL, 0x20, 0x20);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_DAC_CTL, 0x20, 0x00);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_DAC_CTL, 0x40, 0x40);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_DAC_CTL, 0x80, 0x80);
		if (WSA881X_IS_2_0(wsa881x->version)) {
			snd_soc_component_update_bits(component,
					WSA881X_SPKR_BIAS_CAL, 0x01, 0x01);
			snd_soc_component_update_bits(component,
					WSA881X_SPKR_OCP_CTL, 0x30, 0x30);
			snd_soc_component_update_bits(component,
					WSA881X_SPKR_OCP_CTL, 0x0C, 0x00);
		}
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_DRV_GAIN, 0xF0, 0x40);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_MISC_CTL1, 0x01, 0x01);
	} else {
		/* Ensure class-D amp is off */
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_DAC_CTL, 0x80, 0x00);
	}
	return 0;
}

static int wsa881x_spkr_pa_ctrl(struct snd_soc_component *component,
				bool enable)
{
	int ret = 0;
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);

	pr_debug("%s: enable:%d\n", __func__, enable);
	if (enable) {
		/*
		 * Ensure: Boost is enabled and stable, Analog input is up
		 * and outputting silence
		 */
		if (!WSA881X_IS_2_0(wsa881x->version)) {
			snd_soc_component_update_bits(component,
						WSA881X_ADC_EN_DET_TEST_I,
						0xFF, 0x01);
			snd_soc_component_update_bits(component,
						WSA881X_ADC_EN_MODU_V,
						0x02, 0x02);
			snd_soc_component_update_bits(component,
						WSA881X_ADC_EN_DET_TEST_V,
						0xFF, 0x10);
			snd_soc_component_update_bits(component,
						WSA881X_SPKR_PWRSTG_DBG,
						0xA0, 0xA0);
			snd_soc_component_update_bits(component,
						WSA881X_SPKR_DRV_EN,
						0x80, 0x80);
			usleep_range(700, 710);
			snd_soc_component_update_bits(component,
						WSA881X_SPKR_PWRSTG_DBG,
						0x00, 0x00);
			snd_soc_component_update_bits(component,
						WSA881X_ADC_EN_DET_TEST_V,
						0xFF, 0x00);
			snd_soc_component_update_bits(component,
						WSA881X_ADC_EN_MODU_V,
						0x02, 0x00);
			snd_soc_component_update_bits(component,
						WSA881X_ADC_EN_DET_TEST_I,
						0xFF, 0x00);
		} else
			snd_soc_component_update_bits(component,
					WSA881X_SPKR_DRV_EN, 0x80, 0x80);
		/* add 1000us delay as per qcrg */
		usleep_range(1000, 1010);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_DRV_EN, 0x01, 0x01);
		if (WSA881X_IS_2_0(wsa881x->version))
			snd_soc_component_update_bits(component,
						WSA881X_SPKR_BIAS_CAL,
						0x01, 0x00);
		usleep_range(1000, 1010);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_DRV_GAIN,
					0xF0, (wsa881x->spk_pa_gain << 4));
		if (wsa881x->visense_enable) {
			ret = msm_cdc_pinctrl_select_active_state(
						wsa881x->wsa_vi_gpio_p);
			if (ret) {
				pr_err("%s: gpio set cannot be activated %s\n",
					__func__, "wsa_vi");
				return ret;
			}
			wsa881x_visense_txfe_ctrl(component, true,
						0x00, 0x01, 0x00);
			wsa881x_visense_adc_ctrl(component, true);
		}
	} else {
		/*
		 * Ensure: Boost is still on, Stream from Analog input and
		 * Speaker Protection has been stopped and input is at 0V
		 */
		if (WSA881X_IS_2_0(wsa881x->version)) {
			snd_soc_component_update_bits(component,
						WSA881X_SPKR_BIAS_CAL,
						0x01, 0x01);
			usleep_range(1000, 1010);
			snd_soc_component_update_bits(component,
						WSA881X_SPKR_BIAS_CAL,
						0x01, 0x00);
			msleep(20);
			snd_soc_component_update_bits(component,
						WSA881X_ANA_CTL, 0x03, 0x00);
			usleep_range(200, 210);
		}
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_DRV_EN, 0x80, 0x00);
	}
	return 0;
}

static int wsa881x_get_boost(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_component *component =
					snd_soc_kcontrol_component(kcontrol);
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wsa881x->boost_enable;
	return 0;
}

static int wsa881x_set_boost(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
					snd_soc_kcontrol_component(kcontrol);
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);
	int value = ucontrol->value.integer.value[0];

	dev_dbg(component->dev, "%s: Boost enable current %d, new %d\n",
		 __func__, wsa881x->boost_enable, value);
	wsa881x->boost_enable = value;
	return 0;
}

static int wsa881x_get_visense(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_component *component =
					snd_soc_kcontrol_component(kcontrol);
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wsa881x->visense_enable;
	return 0;
}

static int wsa881x_set_visense(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
					snd_soc_kcontrol_component(kcontrol);
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);
	int value = ucontrol->value.integer.value[0];

	dev_dbg(component->dev, "%s: VIsense enable current %d, new %d\n",
		 __func__, wsa881x->visense_enable, value);
	wsa881x->visense_enable = value;
	return 0;
}

static const struct snd_kcontrol_new wsa881x_snd_controls[] = {
	SOC_SINGLE_EXT("BOOST Switch", SND_SOC_NOPM, 0, 1, 0,
		wsa881x_get_boost, wsa881x_set_boost),

	SOC_SINGLE_EXT("VISENSE Switch", SND_SOC_NOPM, 0, 1, 0,
		wsa881x_get_visense, wsa881x_set_visense),

	SOC_ENUM_EXT("WSA_SPK PA Gain", wsa881x_spk_pa_gain_enum[0],
		wsa881x_spk_pa_gain_get, wsa881x_spk_pa_gain_put),
};

static const char * const rdac_text[] = {
	"ZERO", "Switch",
};

static const struct soc_enum rdac_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(rdac_text), rdac_text);

static const struct snd_kcontrol_new rdac_mux[] = {
	SOC_DAPM_ENUM("RDAC", rdac_enum)
};

static int wsa881x_rdac_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
					snd_soc_dapm_to_component(w->dapm);
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_dbg(component->dev, "%s: %s %d boost %d visense %d\n",
		 __func__, w->name, event,
		wsa881x->boost_enable, wsa881x->visense_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = wsa881x_startup(wsa881x);
		if (ret) {
			pr_err("%s: wsa startup failed ret: %d", __func__, ret);
			return ret;
		}
		wsa881x_clk_ctrl(component, true);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_DAC_CTL, 0x02, 0x02);
		if (!WSA881X_IS_2_0(wsa881x->version))
			snd_soc_component_update_bits(component,
						WSA881X_BIAS_REF_CTRL,
						0x0F, 0x08);
		wsa881x_bandgap_ctrl(component, true);
		if (!WSA881X_IS_2_0(wsa881x->version))
			snd_soc_component_update_bits(component,
						WSA881X_SPKR_BBM_CTL,
						0x02, 0x02);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_MISC_CTL1, 0xC0, 0x80);
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_MISC_CTL1, 0x06, 0x06);
		if (!WSA881X_IS_2_0(wsa881x->version)) {
			snd_soc_component_update_bits(component,
					WSA881X_SPKR_MISC_CTL2,
					0x04, 0x04);
			snd_soc_component_update_bits(component,
					WSA881X_SPKR_BIAS_INT,
					0x09, 0x09);
		}
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_PA_INT, 0xF0, 0x20);
		if (WSA881X_IS_2_0(wsa881x->version))
			snd_soc_component_update_bits(component,
					WSA881X_SPKR_PA_INT,
					0x0E, 0x0E);
		if (wsa881x->boost_enable)
			wsa881x_boost_ctrl(component, true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		wsa881x_rdac_ctrl(component, true);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		wsa881x_rdac_ctrl(component, false);
		if (wsa881x->visense_enable) {
			wsa881x_visense_adc_ctrl(component, false);
			wsa881x_visense_txfe_ctrl(component, false,
						0x00, 0x01, 0x00);
			ret = msm_cdc_pinctrl_select_sleep_state(
						wsa881x->wsa_vi_gpio_p);
			if (ret) {
				pr_err("%s: gpio set cannot be suspended %s\n",
					__func__, "wsa_vi");
				return ret;
			}
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (wsa881x->boost_enable)
			wsa881x_boost_ctrl(component, false);
		wsa881x_clk_ctrl(component, false);
		wsa881x_bandgap_ctrl(component, false);
		ret = wsa881x_shutdown(wsa881x);
		if (ret < 0) {
			pr_err("%s: wsa shutdown failed ret: %d",
					__func__, ret);
			return ret;
		}
		break;
	default:
		pr_err("%s: invalid event:%d\n", __func__, event);
		return -EINVAL;
	}
	return 0;
}

static void wsa881x_ocp_ctl_work(struct work_struct *work)
{
	struct wsa881x_pdata *wsa881x;
	struct delayed_work *dwork;
	struct snd_soc_component *component;
	int temp_val;

	dwork = to_delayed_work(work);
	wsa881x = container_of(dwork, struct wsa881x_pdata, ocp_ctl_work);

	if (!wsa881x)
		return;

	component = wsa881x->component;
	wsa881x_get_temp(wsa881x->tz_pdata.tz_dev, &temp_val);
	dev_dbg(component->dev, " temp = %d\n", temp_val);

	if (temp_val <= WSA881X_OCP_CTL_TEMP_CELSIUS)
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_OCP_CTL, 0xC0, 0x00);
	else
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_OCP_CTL, 0xC0, 0xC0);

	schedule_delayed_work(&wsa881x->ocp_ctl_work,
			msecs_to_jiffies(wsa881x_ocp_poll_timer_sec * 1000));
}

static int wsa881x_spkr_pa_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
					snd_soc_dapm_to_component(w->dapm);
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);

	pr_debug("%s: %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
					WSA881X_SPKR_OCP_CTL, 0xC0, 0x80);
		break;
	case SND_SOC_DAPM_POST_PMU:
		wsa881x_spkr_pa_ctrl(component, true);
		schedule_delayed_work(&wsa881x->ocp_ctl_work,
			msecs_to_jiffies(WSA881X_OCP_CTL_TIMER_SEC * 1000));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		wsa881x_spkr_pa_ctrl(component, false);
		break;
	case SND_SOC_DAPM_POST_PMD:
		cancel_delayed_work_sync(&wsa881x->ocp_ctl_work);
		snd_soc_component_update_bits(component,
				WSA881X_SPKR_OCP_CTL, 0xC0, 0xC0);
		break;
	default:
		pr_err("%s: invalid event:%d\n", __func__, event);
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dapm_widget wsa881x_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("WSA_IN"),

	SND_SOC_DAPM_DAC_E("RDAC Analog", NULL, SND_SOC_NOPM, 0, 0,
		wsa881x_rdac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("WSA_RDAC", SND_SOC_NOPM, 0, 0,
		rdac_mux),

	SND_SOC_DAPM_PGA_S("WSA_SPKR PGA", 1, SND_SOC_NOPM, 0, 0,
			wsa881x_spkr_pa_event,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
			SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("WSA_SPKR"),
};

static const struct snd_soc_dapm_route wsa881x_audio_map[] = {
	{"WSA_RDAC", "Switch", "WSA_IN"},
	{"RDAC Analog", NULL, "WSA_RDAC"},
	{"WSA_SPKR PGA", NULL, "RDAC Analog"},
	{"WSA_SPKR", NULL, "WSA_SPKR PGA"},
};


static int wsa881x_startup(struct wsa881x_pdata *pdata)
{
	int ret = 0;

	pr_debug("%s(): wsa startup, enable_cnt:%d\n", __func__,
					pdata->enable_cnt);

	if (pdata->enable_cnt++ > 0)
		return 0;
	ret = msm_cdc_pinctrl_select_active_state(pdata->wsa_clk_gpio_p);
	if (ret) {
		pr_err("%s: gpio set cannot be activated %s\n",
			__func__, "wsa_clk");
		return ret;
	}
	ret = clk_prepare_enable(pdata->wsa_mclk);
	if (ret) {
		pr_err("%s: WSA MCLK enable failed\n",
			__func__);
		return ret;
	}
	ret = wsa881x_reset(pdata, true);
	return ret;
}

static int wsa881x_shutdown(struct wsa881x_pdata *pdata)
{
	int ret = 0;

	pr_debug("%s(): wsa shutdown, enable_cnt:%d\n", __func__,
					pdata->enable_cnt);
	if (--pdata->enable_cnt > 0)
		return 0;
	ret = wsa881x_reset(pdata, false);
	if (ret) {
		pr_err("%s: wsa reset failed suspend %d\n",
			__func__, ret);
		return ret;
	}

	if (__clk_is_enabled(pdata->wsa_mclk))
		clk_disable_unprepare(pdata->wsa_mclk);

	ret = msm_cdc_pinctrl_select_sleep_state(pdata->wsa_clk_gpio_p);
	if (ret) {
		pr_err("%s: gpio set cannot be suspended %s\n",
			__func__, "wsa_clk");
		return ret;
	}

	return 0;
}

static int32_t wsa881x_resource_acquire(struct snd_soc_component *component,
						bool enable)
{
	int ret = 0;
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);

	if (enable) {
		ret = wsa881x_startup(wsa881x);
		if (ret < 0) {
			dev_err_ratelimited(component->dev,
				"%s: failed to startup\n", __func__);
			return ret;
		}
	}
	wsa881x_clk_ctrl(component, enable);
	wsa881x_bandgap_ctrl(component, enable);
	if (!enable) {
		ret = wsa881x_shutdown(wsa881x);
		if (ret < 0)
			dev_err_ratelimited(component->dev,
				"%s: failed to shutdown\n", __func__);
	}
	return ret;
}

static int32_t wsa881x_temp_reg_read(struct snd_soc_component *component,
				     struct wsa_temp_register *wsa_temp_reg)
{
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (!wsa881x) {
		dev_err(component->dev, "%s: wsa881x is NULL\n", __func__);
		return -EINVAL;
	}
	ret = wsa881x_resource_acquire(component, true);
	if (ret) {
		dev_err_ratelimited(component->dev,
			"%s: resource acquire fail\n", __func__);
		return ret;
	}

	if (WSA881X_IS_2_0(wsa881x->version)) {
		snd_soc_component_update_bits(component,
					WSA881X_TADC_VALUE_CTL, 0x01, 0x00);
		wsa_temp_reg->dmeas_msb =
				snd_soc_component_read32(component,
							 WSA881X_TEMP_MSB);
		wsa_temp_reg->dmeas_lsb =
				snd_soc_component_read32(component,
							 WSA881X_TEMP_LSB);
		snd_soc_component_update_bits(component,
					WSA881X_TADC_VALUE_CTL, 0x01, 0x01);
	} else {
		wsa_temp_reg->dmeas_msb = snd_soc_component_read32(component,
						   WSA881X_TEMP_DOUT_MSB);
		wsa_temp_reg->dmeas_lsb = snd_soc_component_read32(component,
						   WSA881X_TEMP_DOUT_LSB);
	}
	wsa_temp_reg->d1_msb = snd_soc_component_read32(component,
							WSA881X_OTP_REG_1);
	wsa_temp_reg->d1_lsb = snd_soc_component_read32(component,
							WSA881X_OTP_REG_2);
	wsa_temp_reg->d2_msb = snd_soc_component_read32(component,
							WSA881X_OTP_REG_3);
	wsa_temp_reg->d2_lsb = snd_soc_component_read32(component,
							WSA881X_OTP_REG_4);

	ret = wsa881x_resource_acquire(component, false);
	if (ret)
		dev_err_ratelimited(component->dev,
			"%s: resource release fail\n", __func__);

	return ret;
}

static int wsa881x_probe(struct snd_soc_component *component)
{
	struct i2c_client *client;
	int ret = 0;
	int retry = REGMAP_REGISTER_CHECK_RETRY;
	int wsa881x_index = 0;
	struct snd_soc_dapm_context *dapm =
					snd_soc_component_get_dapm(component);
	char *widget_name = NULL;

	client = dev_get_drvdata(component->dev);
	ret = wsa881x_i2c_get_client_index(client, &wsa881x_index);
	if (ret != 0) {
		dev_err(&client->dev, "%s: I2C get codec I2C\n"
			"client failed\n", __func__);
		return ret;
	}
	mutex_init(&wsa_pdata[wsa881x_index].bg_lock);
	mutex_init(&wsa_pdata[wsa881x_index].res_lock);
	snprintf(wsa_pdata[wsa881x_index].tz_pdata.name, 100, "%s",
		wsa_tz_names[wsa881x_index]);
	wsa_pdata[wsa881x_index].component = component;
	wsa_pdata[wsa881x_index].spk_pa_gain = SPK_GAIN_12DB;
	wsa_pdata[wsa881x_index].component = component;
	wsa_pdata[wsa881x_index].tz_pdata.component = component;
	wsa_pdata[wsa881x_index].tz_pdata.wsa_temp_reg_read =
						wsa881x_temp_reg_read;
	snd_soc_component_set_drvdata(component, &wsa_pdata[wsa881x_index]);
	while (retry) {
		if (wsa_pdata[wsa881x_index].regmap[WSA881X_ANALOG_SLAVE]
							!= NULL)
			break;
		msleep(100);
		retry--;
	}
	if (!retry)
		dev_err(&client->dev, "%s: max retry expired and regmap of\n"
				"analog slave not initilized\n", __func__);
	wsa881x_init_thermal(&wsa_pdata[wsa881x_index].tz_pdata);
	INIT_DELAYED_WORK(&wsa_pdata[wsa881x_index].ocp_ctl_work,
				wsa881x_ocp_ctl_work);

	if (component->name_prefix) {
		widget_name = kcalloc(WIDGET_NAME_MAX_SIZE, sizeof(char),
					GFP_KERNEL);
		if (!widget_name)
			return -ENOMEM;

		snprintf(widget_name, WIDGET_NAME_MAX_SIZE,
			"%s WSA_SPKR", component->name_prefix);
		snd_soc_dapm_ignore_suspend(dapm, widget_name);
		snprintf(widget_name, WIDGET_NAME_MAX_SIZE,
			"%s WSA_IN", component->name_prefix);
		snd_soc_dapm_ignore_suspend(dapm, widget_name);
		kfree(widget_name);
	} else {
		snd_soc_dapm_ignore_suspend(dapm, "WSA_SPKR");
		snd_soc_dapm_ignore_suspend(dapm, "WSA_IN");
	}

	snd_soc_dapm_sync(dapm);

	return 0;
}

static void wsa881x_remove(struct snd_soc_component *component)
{
	struct wsa881x_pdata *wsa881x =
				snd_soc_component_get_drvdata(component);

	if (wsa881x->tz_pdata.tz_dev)
		wsa881x_deinit_thermal(wsa881x->tz_pdata.tz_dev);

	mutex_destroy(&wsa881x->bg_lock);
	mutex_destroy(&wsa881x->res_lock);
}

static const struct snd_soc_component_driver soc_codec_dev_wsa881x = {
	.name = "",
	.probe	= wsa881x_probe,
	.remove	= wsa881x_remove,

	.read = wsa881x_i2c_read,
	.write = wsa881x_i2c_write,

	.controls = wsa881x_snd_controls,
	.num_controls = ARRAY_SIZE(wsa881x_snd_controls),
	.dapm_widgets = wsa881x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(wsa881x_dapm_widgets),
	.dapm_routes = wsa881x_audio_map,
	.num_dapm_routes = ARRAY_SIZE(wsa881x_audio_map),
};

static struct snd_soc_dai_driver wsa_dai[] = {
	{
		.name = "",
		.playback = {
			.stream_name = "",
			.rates = WSA881X_RATES | WSA881X_FRAC_RATES,
			.formats = WSA881X_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
			},
	},
};

static int wsa881x_reset(struct wsa881x_pdata *pdata, bool enable)
{
	int ret = 0;

	/*
	 * shutdown the GPIOs WSA_EN, WSA_MCLK, regulators
	 * and restore defaults in soc cache when shutdown.
	 * Enable regulators, GPIOs WSA_MCLK, WSA_EN when powerup.
	 */
	if (enable) {
		if (pdata->wsa_active)
			return 0;
		ret = msm_cdc_pinctrl_select_active_state(
					pdata->wsa_reset_gpio_p);
		if (ret) {
			pr_err("%s: gpio set cannot be activated %s\n",
				__func__, "wsa_reset");
			return ret;
		}
		ret = msm_cdc_pinctrl_select_sleep_state(
					pdata->wsa_reset_gpio_p);
		if (ret) {
			pr_err("%s: gpio set cannot be suspended(powerup) %s\n",
				__func__, "wsa_reset");
			return ret;
		}
		ret = msm_cdc_pinctrl_select_active_state(
					pdata->wsa_reset_gpio_p);
		if (ret) {
			pr_err("%s: gpio set cannot be activated %s\n",
				__func__, "wsa_reset");
			return ret;
		}
		pdata->wsa_active = true;
	} else {
		if (!pdata->wsa_active)
			return 0;
		ret = msm_cdc_pinctrl_select_sleep_state(
					pdata->wsa_reset_gpio_p);
		if (ret) {
			pr_err("%s: gpio set cannot be suspended %s\n",
				__func__, "wsa_reset");
			return ret;
		}
		pdata->wsa_active = false;
	}
	return ret;
}

int wsa881x_get_client_index(void)
{
	return wsa881x_i2c_addr;
}
EXPORT_SYMBOL(wsa881x_get_client_index);

int wsa881x_get_probing_count(void)
{
	return wsa881x_probing_count;
}
EXPORT_SYMBOL(wsa881x_get_probing_count);

int wsa881x_get_presence_count(void)
{
	return wsa881x_presence_count;
}
EXPORT_SYMBOL(wsa881x_get_presence_count);

static int check_wsa881x_presence(struct i2c_client *client)
{
	int ret = 0;
	int wsa881x_index = 0;

	ret = wsa881x_i2c_get_client_index(client, &wsa881x_index);
	if (ret != 0) {
		dev_err(&client->dev, "%s: I2C get codec I2C\n"
			"client failed\n", __func__);
		return ret;
	}
	ret = wsa881x_i2c_read_device(&wsa_pdata[wsa881x_index],
					WSA881X_CDC_RST_CTL);
	if (ret < 0) {
		dev_err(&client->dev, "failed to read wsa881x with addr %x\n",
				client->addr);
		return ret;
	}
	ret = wsa881x_i2c_write_device(&wsa_pdata[wsa881x_index],
					WSA881X_CDC_RST_CTL, 0x01);
	if (ret < 0) {
		dev_err(&client->dev, "failed write addr %x reg:0x5 val:0x1\n",
					client->addr);
		return ret;
	}
	/* allow 20ms before trigger next write to verify wsa881x presence */
	msleep(20);
	ret = wsa881x_i2c_write_device(&wsa_pdata[wsa881x_index],
					WSA881X_CDC_RST_CTL, 0x00);
	if (ret < 0) {
		dev_err(&client->dev, "failed write addr %x reg:0x5 val:0x0\n",
					client->addr);
		return ret;
	}
	return ret;
}

static int wsa881x_populate_dt_pdata(struct device *dev, int wsa881x_index)
{
	int ret = 0;
	struct wsa881x_pdata *pdata = &wsa_pdata[wsa881x_index];

	/* reading the gpio configurations from dtsi file */
	pdata->wsa_vi_gpio_p = of_parse_phandle(dev->of_node,
				"qcom,wsa-analog-vi-gpio", 0);
	pdata->wsa_clk_gpio_p = of_parse_phandle(dev->of_node,
				"qcom,wsa-analog-clk-gpio", 0);
	pdata->wsa_reset_gpio_p = of_parse_phandle(dev->of_node,
				"qcom,wsa-analog-reset-gpio", 0);
	pinctrl_init = true;
	return ret;
}

static int wsa881x_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	int wsa881x_index = 0;
	struct wsa881x_pdata *pdata = NULL;
	struct clk *wsa_mclk = NULL;
	char buffer[MAX_NAME_LEN];
	const char *wsa881x_name_prefix_of = NULL;
	struct snd_soc_component *component;

	ret = wsa881x_i2c_get_client_index(client, &wsa881x_index);
	if (ret != 0) {
		dev_err(&client->dev, "%s: I2C get codec I2C\n"
			"client failed\n", __func__);
		return ret;
	}

	pdata = &wsa_pdata[wsa881x_index];
	if ((client->addr == WSA881X_I2C_SPK0_SLAVE1_ADDR ||
		client->addr == WSA881X_I2C_SPK1_SLAVE1_ADDR) &&
		(pdata->status == WSA881X_STATUS_PROBING)) {
		wsa881x_probing_count++;
		return -EPROBE_DEFER;
	}

	if (pdata->status == WSA881X_STATUS_I2C) {
		dev_info(&client->dev, "%s:probe for other slaves\n"
			"devices of codec I2C slave Addr = %x wsa_idx = %d\n",
			__func__, client->addr, wsa881x_index);
		dev_dbg(&client->dev, "%s:wsa_idx = %d SLAVE = %d\n",
				__func__, wsa881x_index, WSA881X_ANALOG_SLAVE);
		pdata->regmap[WSA881X_ANALOG_SLAVE] =
			devm_regmap_init_i2c(
				client,
			&wsa881x_ana_regmap_config[WSA881X_ANALOG_SLAVE]);
		regcache_cache_bypass(pdata->regmap[WSA881X_ANALOG_SLAVE],
					true);
		if (IS_ERR(pdata->regmap[WSA881X_ANALOG_SLAVE])) {
			ret = PTR_ERR(pdata->regmap[WSA881X_ANALOG_SLAVE]);
			dev_err(&client->dev,
				"%s: regmap_init failed %d\n",
					__func__, ret);
		}
		client->dev.platform_data = pdata;
		i2c_set_clientdata(client, pdata);
		pdata->client[WSA881X_ANALOG_SLAVE] = client;
		pdata->regmap_flag = true;
		if (pdata->version == WSA881X_2_0)
			wsa881x_update_regmap_2_0(
					pdata->regmap[WSA881X_ANALOG_SLAVE],
					WSA881X_ANALOG_SLAVE);

		wsa881x_probing_count++;
		return ret;
	} else if (pdata->status == WSA881X_STATUS_PROBING) {
		pdata->index = wsa881x_index;
		if (client->dev.of_node) {
			dev_dbg(&client->dev, "%s:Platform data\n"
				"from device tree\n", __func__);
			ret = wsa881x_populate_dt_pdata(
					&client->dev, wsa881x_index);
			if (ret < 0) {
				dev_err(&client->dev,
				"%s: Fail to obtain pdata from device tree\n",
					 __func__);
				ret = -EINVAL;
				goto err;
			}
			client->dev.platform_data = pdata;
		} else {
			dev_dbg(&client->dev, "%s:Platform data from\n"
				"board file\n", __func__);
			pdata = client->dev.platform_data;
		}
		if (!pdata) {
			dev_dbg(&client->dev, "no platform data?\n");
			ret = -EINVAL;
			goto err;
		}
		wsa_mclk = devm_clk_get(&client->dev, "wsa_mclk");
		if (IS_ERR(wsa_mclk)) {
			ret = PTR_ERR(wsa_mclk);
			dev_dbg(&client->dev, "%s: clk get %s failed %d\n",
				__func__, "wsa_mclk", ret);
			wsa_mclk = NULL;
			goto err;
		}
		pdata->wsa_mclk = wsa_mclk;
		dev_set_drvdata(&client->dev, client);

		pdata->regmap[WSA881X_DIGITAL_SLAVE] =
			devm_regmap_init_i2c(
				client,
			&wsa881x_ana_regmap_config[WSA881X_DIGITAL_SLAVE]);
		regcache_cache_bypass(pdata->regmap[WSA881X_DIGITAL_SLAVE],
					true);
		if (IS_ERR(pdata->regmap[WSA881X_DIGITAL_SLAVE])) {
			ret = PTR_ERR(pdata->regmap[WSA881X_DIGITAL_SLAVE]);
			dev_err(&client->dev, "%s: regmap_init failed %d\n",
				__func__, ret);
			goto err;
		}

		/* bus reset sequence */
		ret = wsa881x_reset(pdata, true);
		if (ret < 0) {
			wsa881x_probing_count++;
			dev_err(&client->dev, "%s: WSA enable Failed %d\n",
				__func__, ret);
			goto err;
		}
		pdata->client[WSA881X_DIGITAL_SLAVE] = client;
		ret = check_wsa881x_presence(client);
		if (ret < 0) {
			dev_err(&client->dev,
				"failed to ping wsa with addr:%x, ret = %d\n",
						client->addr, ret);
			wsa881x_probing_count++;
			goto err1;
		}
		pdata->version = wsa881x_i2c_read_device(pdata,
					WSA881X_CHIP_ID1);
		pr_debug("%s: wsa881x version: %d\n", __func__, pdata->version);
		if (pdata->version == WSA881X_2_0) {
			wsa881x_update_reg_defaults_2_0();
			wsa881x_update_regmap_2_0(
					pdata->regmap[WSA881X_DIGITAL_SLAVE],
					WSA881X_DIGITAL_SLAVE);
		}
		wsa881x_presence_count++;
		wsa881x_probing_count++;

		ret = of_property_read_string(client->dev.of_node,
				"qcom,wsa-prefix", &wsa881x_name_prefix_of);
		if (ret) {
			dev_err(&client->dev,
				"%s: Looking up %s property in node %s failed\n",
				__func__, "qcom,wsa-prefix",
				client->dev.of_node->full_name);
			goto err1;
		}

		pdata->driver = devm_kzalloc(&client->dev,
					sizeof(struct snd_soc_component_driver),
					GFP_KERNEL);
		if (!pdata->driver) {
			ret = -ENOMEM;
			goto err1;
		}

		memcpy(pdata->driver, &soc_codec_dev_wsa881x,
				sizeof(struct snd_soc_component_driver));

		pdata->dai_driver = devm_kzalloc(&client->dev,
					sizeof(struct snd_soc_dai_driver),
					GFP_KERNEL);
		if (!pdata->dai_driver) {
			ret = -ENOMEM;
			goto err_mem;
		}

		memcpy(pdata->dai_driver, wsa_dai,
			sizeof(struct snd_soc_dai_driver));

		snprintf(buffer, sizeof(buffer), "wsa-codec%d", wsa881x_index);
		pdata->driver->name = kstrndup(buffer,
					       strlen(buffer), GFP_KERNEL);

		snprintf(buffer, sizeof(buffer), "wsa_rx%d", wsa881x_index);
		pdata->dai_driver->name =
				kstrndup(buffer, strlen(buffer), GFP_KERNEL);

		snprintf(buffer, sizeof(buffer),
			 "WSA881X_AIF%d Playback", wsa881x_index);
		pdata->dai_driver->playback.stream_name =
				kstrndup(buffer, strlen(buffer), GFP_KERNEL);

		/* Number of DAI's used is 1 */
		ret = snd_soc_register_component(&client->dev,
					pdata->driver, pdata->dai_driver, 1);


		pdata->wsa881x_name_prefix = kstrndup(wsa881x_name_prefix_of,
			strlen(wsa881x_name_prefix_of), GFP_KERNEL);

		component = snd_soc_lookup_component(&client->dev, pdata->driver->name);
		if (!component) {
			dev_err(&client->dev, "%s: component is NULL \n", __func__);
			ret = -EINVAL;
			goto err_mem;
		}

		component->name_prefix = pdata->wsa881x_name_prefix;

		pdata->status = WSA881X_STATUS_I2C;
		dev_info(&client->dev, "%s:pdata status changed to I2C\n",
			__func__);
		goto err1;
	}
err_mem:
	 kfree(pdata->wsa881x_name_prefix);
	if (pdata->dai_driver) {
		kfree(pdata->dai_driver->name);
		kfree(pdata->dai_driver->playback.stream_name);
		kfree(pdata->dai_driver);
	}
	if (pdata->driver) {
		kfree(pdata->driver->name);
		kfree(pdata->driver);
	}

err1:
	wsa881x_reset(pdata, false);
err:
	return ret;
}

static int wsa881x_i2c_remove(struct i2c_client *client)
{
	struct wsa881x_pdata *wsa881x = client->dev.platform_data;

	snd_soc_unregister_component(&client->dev);
	kfree(wsa881x->wsa881x_name_prefix);
	if (wsa881x->dai_driver) {
		kfree(wsa881x->dai_driver->name);
		kfree(wsa881x->dai_driver->playback.stream_name);
		kfree(wsa881x->dai_driver);
	}
	if (wsa881x->driver) {
		kfree(wsa881x->driver->name);
		kfree(wsa881x->driver);
	}
	i2c_set_clientdata(client, NULL);
	kfree(wsa881x);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int wsa881x_i2c_suspend(struct device *dev)
{
	pr_debug("%s: system suspend\n", __func__);
	return 0;
}

static int wsa881x_i2c_resume(struct device *dev)
{
	pr_debug("%s: system resume\n", __func__);
	return 0;
}

static const struct dev_pm_ops wsa881x_i2c_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(wsa881x_i2c_suspend, wsa881x_i2c_resume)
};
#endif /* CONFIG_PM_SLEEP */

static const struct i2c_device_id wsa881x_i2c_id[] = {
	{"wsa881x-i2c-dev", WSA881X_I2C_SPK0_SLAVE0_ADDR},
	{"wsa881x-i2c-dev", WSA881X_I2C_SPK0_SLAVE1_ADDR},
	{"wsa881x-i2c-dev", WSA881X_I2C_SPK1_SLAVE0_ADDR},
	{"wsa881x-i2c-dev", WSA881X_I2C_SPK1_SLAVE1_ADDR},
	{}
};

MODULE_DEVICE_TABLE(i2c, wsa881x_i2c_id);


static const struct of_device_id msm_match_table[] = {
	{.compatible = "qcom,wsa881x-i2c-codec"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_match_table);

static struct i2c_driver wsa881x_codec_driver = {
	.driver = {
		.name = "wsa881x-i2c-codec",
		.owner = THIS_MODULE,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#ifdef CONFIG_PM_SLEEP
		.pm = &wsa881x_i2c_pm_ops,
#endif
		.of_match_table = msm_match_table,
	},
	.id_table = wsa881x_i2c_id,
	.probe = wsa881x_i2c_probe,
	.remove = wsa881x_i2c_remove,
};

static int __init wsa881x_codec_init(void)
{
	int i = 0;

	for (i = 0; i < MAX_WSA881X_DEVICE; i++)
		wsa_pdata[i].status = WSA881X_STATUS_PROBING;
	return i2c_add_driver(&wsa881x_codec_driver);
}
module_init(wsa881x_codec_init);

static void __exit wsa881x_codec_exit(void)
{
	i2c_del_driver(&wsa881x_codec_driver);
}

module_exit(wsa881x_codec_exit);

MODULE_DESCRIPTION("WSA881x Codec driver");
MODULE_LICENSE("GPL v2");
