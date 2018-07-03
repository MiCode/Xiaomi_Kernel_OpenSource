/*
 * es9018.c  --  codec driver for ES9018
 *
 * Copyright (C) 2014 Xiaomi Corporation
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * Author: Xiang Xiao <xiaoxiang@xiaomi.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <asm/bootinfo.h>
#include <sound/asound.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "../msm/msm8994.h"
#include "es9018.h"

struct es9018_priv {
	unsigned int reg_addr;
	struct snd_soc_codec *codec;
	unsigned long rate_mclk;
	struct pinctrl *pinctrl;
	struct regulator *reg_dvcc;
	struct regulator *reg_dvdd;
	struct regulator *reg_vcca;
	struct regulator *reg_avccl;
	struct regulator *reg_avccr;
	enum of_gpio_flags resetb_flags;
	int resetb_gpio;
	enum of_gpio_flags mute_flags;
	int mute_gpio;
	enum of_gpio_flags switch_flags;
	int switch_gpio;
	enum of_gpio_flags opa_flags;
	int opa_gpio;
	enum of_gpio_flags clock_flags;
	int clock_gpio;
	enum of_gpio_flags clock_45m_flags;
	int clock_45m_gpio;
	enum of_gpio_flags clock_49m_flags;
	int clock_49m_gpio;
	bool custom_filter_enabled;
	int hw_major;
	int hw_minor;
};

struct imped_setting_t {
	u32 imped_val;
	u8 thd_compensation;
	u8 thd2_compensation;
	u8 thd3_compensation;
};

#define ESS_OSC_CLK_45M		45158400
#define ESS_OSC_CLK_49M		49152000

static const unsigned long osc_clk_src[] = {
	ESS_OSC_CLK_45M, ESS_OSC_CLK_49M,
};
static struct es9018_priv *es9018_data;

static const struct imped_setting_t imped_settings[] = {
	{12000, 0x0, 0x00, 0x09, },
	{26000, 0x0, 0x00, 0x01, },
	{50000, 0x0, 0x00, 0x00, },
	{150000, 0x0, 0x00, 0x00, },
	{600000, 0x0, 0x00, 0x01, },
};

#define ES9018_COEFFS_STAGE1_COUNT 128
#define ES9018_COEFFS_STAGE2_COUNT 16

static int coeffs_stage1[ES9018_COEFFS_STAGE1_COUNT];
static int coeffs_stage2[ES9018_COEFFS_STAGE2_COUNT];

#define IMPEDANCE_LOW_BOUNDARY 6000
#define IMPEDANCE_HIGH_BOUNDARY 600000

static int get_impedance_index(u32 imped)
{
	int i = 0;

	if (imped < IMPEDANCE_LOW_BOUNDARY) {
		pr_err("%s, detected impedance is less than %d mOhm\n",
			__func__, IMPEDANCE_LOW_BOUNDARY);
	}
	for (i = 0; i < ARRAY_SIZE(imped_settings); i++) {
		if (imped <= imped_settings[i].imped_val)
			break;
	}
	if (i == ARRAY_SIZE(imped_settings)) {
		pr_err("%s, detected impedance is more than %d mOhm\n",
			__func__, IMPEDANCE_HIGH_BOUNDARY);
		i--;
	}
	return i;
}

static void es9018_power(struct es9018_priv *es9018, bool on)
{
	struct snd_soc_codec *codec = es9018->codec;
	int ret;

	if (on) {
		if (es9018->reg_dvdd) {
			ret = regulator_enable(es9018->reg_dvdd);
			if (ret < 0)
				dev_err(codec->dev, "Failed to enable dvdd(%d)\n", ret);
		}

		if (es9018->reg_avccl) {
			ret = regulator_enable(es9018->reg_avccl);
			if (ret < 0)
				dev_err(codec->dev, "Failed to enable avccl(%d)\n", ret);
		}

		if (es9018->reg_avccr) {
			ret = regulator_enable(es9018->reg_avccr);
			if (ret < 0)
				dev_err(codec->dev, "Failed to enable avccr(%d)\n", ret);
		}

		if (es9018->reg_vcca) {
			ret = regulator_enable(es9018->reg_vcca);
			if (ret < 0)
				dev_err(codec->dev, "Failed to enable vcca(%d)\n", ret);
		}

		if (es9018->reg_dvcc) {
			ret = regulator_enable(es9018->reg_dvcc);
			if (ret < 0)
				dev_err(codec->dev, "Failed to enable dvcc(%d)\n", ret);
		}
	} else {
		if (es9018->reg_dvcc) {
			ret = regulator_disable(es9018->reg_dvcc);
			if (ret < 0)
				dev_err(codec->dev, "Failed to disable dvcc(%d)\n", ret);
		}

		if (es9018->reg_vcca) {
			ret = regulator_disable(es9018->reg_vcca);
			if (ret < 0)
				dev_err(codec->dev, "Failed to disable vcca(%d)\n", ret);
		}

		if (es9018->reg_avccr) {
			ret = regulator_disable(es9018->reg_avccr);
			if (ret < 0)
				dev_err(codec->dev, "Failed to disable avccr(%d)\n", ret);
		}

		if (es9018->reg_avccl) {
			ret = regulator_disable(es9018->reg_avccl);
			if (ret < 0)
				dev_err(codec->dev, "Failed to disable avccl(%d)\n", ret);
		}

		if (es9018->reg_dvdd) {
			ret = regulator_disable(es9018->reg_dvdd);
			if (ret < 0)
				dev_err(codec->dev, "Failed to disable dvdd(%d)\n", ret);
		}
	}
}

static int es9018_set_coefficient(struct es9018_priv *es9018)
{
	struct snd_soc_codec *codec = es9018->codec;
	int i = 0, ret = 0;

	for (i = 0; i < ES9018_COEFFS_STAGE1_COUNT; i++) {
		ret = snd_soc_write(codec,
			ES9018_FILTER_ADDRESS,
			i);
		if (ret < 0)
			break;

		ret = snd_soc_write(codec,
			ES9018_FILTER_COEFFICIENT,
			coeffs_stage1[i] & 0xFF);
		if (ret < 0)
			break;
		ret = snd_soc_write(codec,
			ES9018_FILTER_COEFFICIENT + 1,
			(coeffs_stage1[i] >> 8) & 0xFF);
		if (ret < 0)
			break;
		ret = snd_soc_write(codec,
			ES9018_FILTER_COEFFICIENT + 2,
			(coeffs_stage1[i] >> 16) & 0xFF);
		if (ret < 0)
			break;

		ret = snd_soc_update_bits(codec,
			ES9018_FILTER_CONTROL,
			ES9018_PROG_COEFF_WRITING_MSK,
			ES9018_PROG_COEFF_WRITING);
		if (ret < 0)
			break;
	}

	snd_soc_update_bits(codec,
		ES9018_FILTER_CONTROL,
		ES9018_PROG_COEFF_WRITING_MSK,
		0x00);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set coefficient stage1.\n");
		return ret;
	}

	for (i = 0; i < ES9018_COEFFS_STAGE2_COUNT; i++) {
		ret = snd_soc_write(codec,
			ES9018_FILTER_ADDRESS,
			i | ES9018_PROG_COEFF_STAGE2);
		if (ret < 0)
			break;

		ret = snd_soc_write(codec,
			ES9018_FILTER_COEFFICIENT,
			coeffs_stage2[i] & 0xFF);
		if (ret < 0)
			break;
		ret = snd_soc_write(codec,
			ES9018_FILTER_COEFFICIENT + 1,
			(coeffs_stage2[i] >> 8) & 0xFF);
		if (ret < 0)
			break;
		ret = snd_soc_write(codec,
			ES9018_FILTER_COEFFICIENT + 2,
			(coeffs_stage2[i] >> 16) & 0xFF);
		if (ret < 0)
			break;

		ret = snd_soc_update_bits(codec,
			ES9018_FILTER_CONTROL,
			ES9018_PROG_COEFF_WRITING_MSK,
			ES9018_PROG_COEFF_WRITING);
		if (ret < 0)
			break;
	}

	snd_soc_update_bits(codec,
		ES9018_FILTER_CONTROL,
		ES9018_PROG_COEFF_WRITING_MSK,
		0x00);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set coefficient stage2.\n");
		return ret;
	}

	return 0;
}

static void es9018_reset(struct es9018_priv *es9018, bool reset)
{
	if (reset) {
		if (es9018->resetb_flags == OF_GPIO_ACTIVE_LOW)
			gpio_set_value_cansleep(es9018->resetb_gpio, 0);
		else
			gpio_set_value_cansleep(es9018->resetb_gpio, 1);
	} else {
		if (es9018->resetb_flags == OF_GPIO_ACTIVE_LOW)
			gpio_set_value_cansleep(es9018->resetb_gpio, 1);
		else
			gpio_set_value_cansleep(es9018->resetb_gpio, 0);
	}
}

static void es9018_mute(struct es9018_priv *es9018, bool mute)
{
	if (gpio_is_valid(es9018->mute_gpio)) {
		if (mute) {
			if (es9018->mute_flags == OF_GPIO_ACTIVE_LOW)
				gpio_set_value_cansleep(es9018->mute_gpio, 0);
			else
				gpio_set_value_cansleep(es9018->mute_gpio, 1);
		} else {
			if (es9018->mute_flags == OF_GPIO_ACTIVE_LOW)
				gpio_set_value_cansleep(es9018->mute_gpio, 1);
			else
				gpio_set_value_cansleep(es9018->mute_gpio, 0);
		}
	}
}

static void es9018_switch(struct es9018_priv *es9018, bool enable)
{
	if (gpio_is_valid(es9018->switch_gpio)) {
		if (enable) {
			if (es9018->switch_flags == OF_GPIO_ACTIVE_LOW)
				gpio_set_value_cansleep(es9018->switch_gpio, 0);
			else
				gpio_set_value_cansleep(es9018->switch_gpio, 1);
		} else {
			if (es9018->switch_flags == OF_GPIO_ACTIVE_LOW)
				gpio_set_value_cansleep(es9018->switch_gpio, 1);
			else
				gpio_set_value_cansleep(es9018->switch_gpio, 0);
		}
	}
}

static void es9018_opa(struct es9018_priv *es9018, bool enable)
{
	if (gpio_is_valid(es9018->opa_gpio)) {
		if (enable) {
			if (es9018->opa_flags == OF_GPIO_ACTIVE_LOW)
				gpio_set_value_cansleep(es9018->opa_gpio, 0);
			else
				gpio_set_value_cansleep(es9018->opa_gpio, 1);
		} else {
			if (es9018->opa_flags == OF_GPIO_ACTIVE_LOW)
				gpio_set_value_cansleep(es9018->opa_gpio, 1);
			else
				gpio_set_value_cansleep(es9018->opa_gpio, 0);
		}
	}
}

static void es9018_clock(struct es9018_priv *es9018, bool enable)
{
	if (enable) {
		if (es9018->clock_flags == OF_GPIO_ACTIVE_LOW)
			gpio_set_value_cansleep(es9018->clock_gpio, 0);
		else
			gpio_set_value_cansleep(es9018->clock_gpio, 1);
	} else {
		if (es9018->clock_flags == OF_GPIO_ACTIVE_LOW)
			gpio_set_value_cansleep(es9018->clock_gpio, 1);
		else
			gpio_set_value_cansleep(es9018->clock_gpio, 0);
	}
}

static int es9018_set_mclk_rate(struct es9018_priv *es9018, unsigned long rate_bclk)
{
	int i, m;

	es9018->clock_gpio = -1;
	for (m = 4; m <= 16; m <<= 1) {
		unsigned long rate_mclk = m * rate_bclk;

		for (i = 0; i < ARRAY_SIZE(osc_clk_src); i++) {
			if (rate_mclk == osc_clk_src[i])
				break;
		}

		if (i < ARRAY_SIZE(osc_clk_src)) {
			switch (osc_clk_src[i]) {
			case ESS_OSC_CLK_45M:
				es9018->clock_gpio = es9018->clock_45m_gpio;
				es9018->clock_flags = es9018->clock_45m_flags;
				break;
			case ESS_OSC_CLK_49M:
				es9018->clock_gpio = es9018->clock_49m_gpio;
				es9018->clock_flags = es9018->clock_49m_flags;
				break;
			default:
				break;
			}
		}
	}

	return es9018->clock_gpio;
}

static unsigned long es9018_get_mclk_rate(struct es9018_priv *es9018)
{
	unsigned long rate_mclk = 0;

	if (es9018->clock_gpio == es9018->clock_45m_gpio)
		rate_mclk = ESS_OSC_CLK_45M;
	else if (es9018->clock_gpio == es9018->clock_49m_gpio)
		rate_mclk = ESS_OSC_CLK_49M;
	else
		rate_mclk = 0;

	return rate_mclk;
}

static int es9018_probe(struct snd_soc_codec *codec)
{
	struct es9018_priv *es9018;
	struct pinctrl_state *pin_int;
	int ret;

	dev_dbg(codec->dev, "%s: enter\n", __func__);
	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O(%d)\n", ret);
		return ret;
	}
	codec->cache_only = true; /* don't touch hardware until the bias on */

	es9018 = kzalloc(sizeof(struct es9018_priv), GFP_KERNEL);
	if (es9018 == NULL) {
		dev_err(codec->dev, "Failed to alloc es9018_priv\n");
		return -ENOMEM;
	}

	es9018->hw_major = get_hw_version_major();
	es9018->hw_minor = get_hw_version_minor();

	es9018->pinctrl = devm_pinctrl_get(codec->dev);
	if (IS_ERR_OR_NULL(es9018->pinctrl)) {
		dev_err(codec->dev, "%s: Unable to get pinctrl handle\n", __func__);
		goto es9018_free;
	}

	pin_int = pinctrl_lookup_state(es9018->pinctrl, "ess-int");
	if (IS_ERR(pin_int)) {
		dev_err(codec->dev, "%s: could not get disable pinstate\n", __func__);
		goto pinctrl_free;
	}

	ret = pinctrl_select_state(es9018->pinctrl, pin_int);
	if (ret != 0) {
		dev_err(codec->dev, "%s: Set pins to int status failed with %d\n",
			__func__, ret);
		goto pinctrl_free;
	}

	es9018->custom_filter_enabled = false;
	es9018->resetb_gpio = of_get_named_gpio_flags(codec->dev->of_node,
		"ess,resetb-gpio", 0, &es9018->resetb_flags);
	dev_dbg(codec->dev, "reset gpio %d\n", es9018->resetb_gpio);
	if (es9018->resetb_gpio < 0) {
		ret = es9018->resetb_gpio;
		dev_err(codec->dev, "Failed to parse resetb-gpio(%d)\n", ret);
		goto pinctrl_free;
	}

	if (es9018->resetb_flags == OF_GPIO_ACTIVE_LOW) {
		ret = gpio_request_one(es9018->resetb_gpio,
			GPIOF_OUT_INIT_LOW, "ess,resetb-gpio");
	} else {
		ret = gpio_request_one(es9018->resetb_gpio,
			GPIOF_OUT_INIT_HIGH, "ess,resetb-gpio");
	}
	if (ret < 0) {
		dev_err(codec->dev, "Failed to request resetb-gpio(%d)\n", ret);
		goto pinctrl_free;
	}

	gpio_export(es9018->resetb_gpio, false); /* for factory test */
	gpio_export_link(codec->dev, "resetb", es9018->resetb_gpio);

	es9018->mute_gpio = of_get_named_gpio_flags(codec->dev->of_node,
		"ess,mute-gpio", 0, &es9018->mute_flags);
	dev_dbg(codec->dev, "mute gpio %d\n", es9018->mute_gpio);
	if (gpio_is_valid(es9018->mute_gpio)) {
		if (es9018->mute_flags == OF_GPIO_ACTIVE_LOW) {
			ret = gpio_request_one(es9018->mute_gpio,
				GPIOF_OUT_INIT_HIGH, "ess,mute-gpio");
		} else {
			ret = gpio_request_one(es9018->mute_gpio,
				GPIOF_OUT_INIT_LOW, "ess,mute-gpio");
		}
		if (ret < 0) {
			dev_err(codec->dev, "Failed to request mute-gpio(%d)\n", ret);
			goto resetb_free;
		}
	} else {
		dev_info(codec->dev, "Failed to parse mute-gpio(%d)\n", ret);
	}

	es9018->switch_gpio = of_get_named_gpio_flags(codec->dev->of_node,
		"ess,switch-gpio", 0, &es9018->switch_flags);
	dev_dbg(codec->dev, "switch gpio %d\n", es9018->switch_gpio);
	if (gpio_is_valid(es9018->switch_gpio)) {
		if (es9018->switch_flags == OF_GPIO_ACTIVE_LOW) {
			ret = gpio_request_one(es9018->switch_gpio,
				GPIOF_OUT_INIT_HIGH, "ess,switch-gpio");
		} else {
			ret = gpio_request_one(es9018->switch_gpio,
				GPIOF_OUT_INIT_LOW, "ess,switch-gpio");
		}
		if (ret < 0) {
			dev_err(codec->dev, "Failed to request switch-gpio(%d)\n", ret);
			goto mute_free;
		}
	} else {
		dev_info(codec->dev, "Failed to parse switch-gpio(%d)\n", ret);
	}

	es9018->opa_gpio = -1;
	if ((es9018->hw_major == 2) && (es9018->hw_minor == 2)) {
		es9018->opa_gpio = of_get_named_gpio_flags(codec->dev->of_node,
			"ess,opa-gpio", 0, &es9018->opa_flags);
		dev_dbg(codec->dev, "opa gpio %d\n", es9018->opa_gpio);
		if (gpio_is_valid(es9018->opa_gpio)) {
			if (es9018->opa_flags == OF_GPIO_ACTIVE_LOW) {
				ret = gpio_request_one(es9018->opa_gpio,
					GPIOF_OUT_INIT_HIGH, "ess,opa-gpio");
			} else {
				ret = gpio_request_one(es9018->opa_gpio,
					GPIOF_OUT_INIT_LOW, "ess,opa-gpio");
			}
			if (ret < 0) {
				dev_err(codec->dev, "Failed to request opa-gpio(%d)\n", ret);
				goto switch_free;
			}
		} else {
			dev_info(codec->dev, "Failed to parse opa-gpio(%d)\n", ret);
		}
	}

	es9018->clock_gpio = -1;
	es9018->clock_flags = 0;
	es9018->clock_45m_gpio = of_get_named_gpio_flags(codec->dev->of_node,
		"ess,clock-45m-gpio", 0, &es9018->clock_45m_flags);
	dev_dbg(codec->dev, "clock 45M gpio %d\n", es9018->clock_45m_gpio);
	if (es9018->clock_45m_gpio < 0) {
		ret = es9018->clock_45m_gpio;
		dev_err(codec->dev, "Failed to parse clock-45m-gpio(%d)\n", ret);
		goto opa_free;
	}
	if (es9018->clock_45m_flags == OF_GPIO_ACTIVE_LOW) {
		ret = gpio_request_one(es9018->clock_45m_gpio,
			GPIOF_OUT_INIT_HIGH, "ess,clock-45m-gpio");
	} else {
		ret = gpio_request_one(es9018->clock_45m_gpio,
			GPIOF_OUT_INIT_LOW, "ess,clock-45m-gpio");
	}
	if (ret < 0) {
		dev_err(codec->dev, "Failed to request clock-45m-gpio(%d)\n", ret);
		goto opa_free;
	}

	es9018->clock_49m_gpio = of_get_named_gpio_flags(codec->dev->of_node,
		"ess,clock-49m-gpio", 0, &es9018->clock_49m_flags);
	dev_dbg(codec->dev, "clock 49M gpio %d\n", es9018->clock_49m_gpio);
	if (es9018->clock_49m_gpio < 0) {
		ret = es9018->clock_49m_gpio;
		dev_err(codec->dev, "Failed to parse clock-49m-gpio(%d)\n", ret);
		goto clock_45m_free;
	}
	if (es9018->clock_49m_flags == OF_GPIO_ACTIVE_LOW) {
		ret = gpio_request_one(es9018->clock_49m_gpio,
			GPIOF_OUT_INIT_HIGH, "ess,clock-49m-gpio");
	} else {
		ret = gpio_request_one(es9018->clock_49m_gpio,
			GPIOF_OUT_INIT_LOW, "ess,clock-49m-gpio");
	}
	if (ret < 0) {
		dev_err(codec->dev, "Failed to request clock-49m-gpio(%d)\n", ret);
		goto clock_45m_free;
	}

	es9018->reg_dvcc = regulator_get(codec->dev, "dvcc");
	if (IS_ERR(es9018->reg_dvcc)) {
		dev_info(codec->dev, "dvcc can't be found\n");
		es9018->reg_dvcc = NULL;
	}

	es9018->reg_dvdd = regulator_get(codec->dev, "dvdd");
	if (IS_ERR(es9018->reg_dvdd)) {
		dev_info(codec->dev, "dvdd can't be found\n");
		es9018->reg_dvdd = NULL;
	}

	es9018->reg_vcca = regulator_get(codec->dev, "vcca");
	if (IS_ERR(es9018->reg_vcca)) {
		dev_info(codec->dev, "vcca can't be found\n");
		es9018->reg_vcca = NULL;
	}

	es9018->reg_avccl = regulator_get(codec->dev, "avccl");
	if (IS_ERR(es9018->reg_avccl)) {
		dev_info(codec->dev, "avccl can't be found\n");
		es9018->reg_avccl = NULL;
	}

	es9018->reg_avccr = regulator_get(codec->dev, "avccr");
	if (IS_ERR(es9018->reg_avccr)) {
		dev_info(codec->dev, "avccr can't be found\n");
		es9018->reg_avccr = NULL;
	}

	es9018->codec = codec;
	es9018_data = es9018;
	snd_soc_codec_set_drvdata(codec, es9018);

	if ((es9018->hw_major == 2) && (es9018->hw_minor == 2)) {
		es9018_power(es9018, true);
		mdelay(1);
		es9018_reset(es9018, false);
		usleep(1000);
		/* send the cached change to hardware */
		codec->cache_only = false;
		/*disable soft start per datasheet */
		ret = snd_soc_update_bits(codec,
			ES9018_SOFT_START_SETTINGS,
			ES9018_SOFT_START_MSK, 0);
		/* assert the reset pin */
		es9018_reset(es9018, true);
		codec->cache_sync = true;
		codec->cache_only = true; /* block to touch hardware */
	}

	return 0;

clock_45m_free:
	gpio_free(es9018->clock_45m_gpio);
opa_free:
	if (gpio_is_valid(es9018->opa_gpio))
		gpio_free(es9018->opa_gpio);
switch_free:
	if (gpio_is_valid(es9018->switch_gpio))
		gpio_free(es9018->switch_gpio);
mute_free:
	if (gpio_is_valid(es9018->mute_gpio))
		gpio_free(es9018->mute_gpio);
resetb_free:
	gpio_free(es9018->resetb_gpio);
pinctrl_free:
	devm_pinctrl_put(es9018->pinctrl);
	es9018->pinctrl = NULL;
es9018_free:
	kfree(es9018);
	es9018_data = NULL;
	return ret;
}

static int es9018_remove(struct snd_soc_codec *codec)
{
	struct es9018_priv *es9018 = snd_soc_codec_get_drvdata(codec);

	if (es9018->reg_avccr)
		regulator_put(es9018->reg_avccr);
	if (es9018->reg_avccl)
		regulator_put(es9018->reg_avccl);
	if (es9018->reg_vcca)
		regulator_put(es9018->reg_vcca);
	if (es9018->reg_dvdd)
		regulator_put(es9018->reg_dvdd);
	if (es9018->reg_dvcc)
		regulator_put(es9018->reg_dvcc);

	if (gpio_is_valid(es9018->switch_gpio))
		gpio_free(es9018->switch_gpio);
	if (gpio_is_valid(es9018->mute_gpio))
		gpio_free(es9018->mute_gpio);
	gpio_free(es9018->resetb_gpio);

	if (es9018->pinctrl)
		devm_pinctrl_put(es9018->pinctrl);

	kfree(es9018);
	es9018_data = NULL;
	return 0;
}

static int es9018_set_bias_on(struct snd_soc_codec *codec)
{
	struct es9018_priv *es9018 = snd_soc_codec_get_drvdata(codec);
	struct msm8994_asoc_mach_data *mach_data = snd_soc_card_get_drvdata(codec->card);
	int ret;
	int index = 0;

	dev_dbg(codec->dev, "%s: enter\n", __func__);
	/* mute before transition */
	es9018_mute(es9018, true);

	if (!((es9018->hw_major == 2) && (es9018->hw_minor == 2)))
		es9018_power(es9018, true);

	/* enable the clock */
	es9018_clock(es9018, true);
	/* release the reset pin */
	mdelay(1); /* delay 1ms per datasheet */
	es9018_reset(es9018, false);
	usleep(1000); /* delay 1ms to ensure chip is stable */

	/* send the cached change to hardware */
	codec->cache_only = false;
	if (mach_data)
		index = get_impedance_index(mach_data->curr_hs_impedance);
	ret = snd_soc_cache_write(codec, ES9018_THD_COMPENSATION,
		imped_settings[index].thd_compensation);
	if (ret < 0)
		dev_err(codec->dev, "Failed to write cache(%d)\n", ret);
	ret = snd_soc_cache_write(codec, ES9018_THD2_COMPENSATION,
		imped_settings[index].thd2_compensation);
	if (ret < 0)
		dev_err(codec->dev, "Failed to write cache(%d)\n", ret);
	ret = snd_soc_cache_write(codec, ES9018_THD3_COMPENSATION,
		imped_settings[index].thd3_compensation);
	if (ret < 0)
		dev_err(codec->dev, "Failed to write cache(%d)\n", ret);
	ret = snd_soc_cache_sync(codec);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to sync cache(%d)\n", ret);
		goto bus_access_err;
	}

	if (es9018->custom_filter_enabled) {
		ret = es9018_set_coefficient(es9018);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set coefficient, use the build-in filter\n");
			snd_soc_write(codec, ES9018_FILTER_CONTROL, 0x00);
		} else {
			/* enable the custom filter */
			ret = snd_soc_update_bits(codec,
				ES9018_FILTER_CONTROL,
				ES9018_PROG_COEFF_ENABLE_MSK,
				ES9018_PROG_COEFF_ENABLE);
			if (ret < 0)
				dev_err(codec->dev, "Failed to enable custom filter(%d)\n", ret);
		}
	} else {
		dev_dbg(codec->dev, "Use the build-in filter\n");
		snd_soc_write(codec, ES9018_FILTER_CONTROL, 0x00);
	}

	/* enable soft start */
	ret = snd_soc_update_bits(codec,
		ES9018_SOFT_START_SETTINGS,
		ES9018_SOFT_START_MSK,
		ES9018_SOFT_START);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set soft start bit(%d)\n", ret);
		goto bus_access_err;
	}
	msleep(150); /* startup time required by hardware */

	/* enable opa */
	es9018_opa(es9018, true);
	/* done, switch on and unmute */
	es9018_switch(es9018, true);
	/* for p2c, need note unmute */
	if (!((es9018->hw_major == 2) && (es9018->hw_minor == 2)))
		es9018_mute(es9018, false);

	return 0;

bus_access_err:
	codec->cache_only = true;
	es9018_reset(es9018, true);
	es9018_mute(es9018, false);
	if (!((es9018->hw_major == 2) && (es9018->hw_minor == 2)))
		es9018_power(es9018, false);
	return ret;
}

static void es9018_set_bias_off(struct snd_soc_codec *codec)
{
	struct es9018_priv *es9018 = snd_soc_codec_get_drvdata(codec);
	int ret;

	dev_dbg(codec->dev, "%s: enter\n", __func__);
	/* mute and switch off */
	es9018_mute(es9018, true);
	es9018_opa(es9018, false);
	es9018_switch(es9018, false);

	/*disable soft start per datasheet */
	ret = snd_soc_update_bits(codec,
		ES9018_SOFT_START_SETTINGS,
		ES9018_SOFT_START_MSK, 0);
	if (ret < 0)
		dev_err(codec->dev, "Failed to clear soft start bit(%d)\n", ret);

	es9018_reset(es9018, true);
	codec->cache_sync = true;
	codec->cache_only = true; /* block to touch hardware */
	es9018_clock(es9018, false);
	if (!((es9018->hw_major == 2) && (es9018->hw_minor == 2)))
		es9018_power(es9018, false);
	es9018_mute(es9018, false);
}

static int es9018_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	int ret = 0;

	if (level == SND_SOC_BIAS_ON) {
		if (codec->dapm.bias_level != SND_SOC_BIAS_ON) {
			ret = es9018_set_bias_on(codec);
			if (ret < 0)
				return ret;
		}
	} else {
		if (codec->dapm.bias_level == SND_SOC_BIAS_ON)
			es9018_set_bias_off(codec);
	}

	codec->dapm.bias_level = level;
	return ret;
}

static int es9018_get_custom_filter(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018_priv *es9018 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = (es9018->custom_filter_enabled == true ? 1 : 0);
	return 0;
}

static int es9018_put_custom_filter(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018_priv *es9018 = snd_soc_codec_get_drvdata(codec);

	es9018->custom_filter_enabled = (!ucontrol->value.integer.value[0] ? false : true);
	return 0;
}

static int es9018_get_coefficient(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int stage = ((struct soc_multi_mixer_control *)
				kcontrol->private_value)->shift;
	int i = 0;

	if (stage == 1) {
		for (i = 0; i < ES9018_COEFFS_STAGE1_COUNT; i++)
			ucontrol->value.integer.value[i] = coeffs_stage1[i];
	} else {
		for (i = 0; i < ES9018_COEFFS_STAGE2_COUNT; i++)
			ucontrol->value.integer.value[i] = coeffs_stage2[i];
	}

	return 0;
}

static int es9018_put_coefficient(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int stage = ((struct soc_multi_mixer_control *)
				kcontrol->private_value)->shift;
	int i = 0;

	if (stage == 1) {
		for (i = 0; i < ES9018_COEFFS_STAGE1_COUNT; i++)
			coeffs_stage1[i] = ucontrol->value.integer.value[i];
	} else {
		for (i = 0; i < ES9018_COEFFS_STAGE2_COUNT; i++)
			coeffs_stage2[i] = ucontrol->value.integer.value[i];
	}

	return 0;
}

static int es9018_get_hph_impedance(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct msm8994_asoc_mach_data *mach_data = snd_soc_card_get_drvdata(codec->card);

	ucontrol->value.integer.value[0] = mach_data->curr_hs_impedance;
	return 0;
}

static int es9018_reg_addr_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018_priv *es9018 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = es9018->reg_addr;
	return 0;
}

static int es9018_reg_addr_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018_priv *es9018 = snd_soc_codec_get_drvdata(codec);

	es9018->reg_addr = ucontrol->value.integer.value[0];
	return 0;
}

static int es9018_reg_value_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018_priv *es9018 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = snd_soc_read(codec, es9018->reg_addr);
	return 0;
}

static int es9018_reg_value_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct es9018_priv *es9018 = snd_soc_codec_get_drvdata(codec);

	return snd_soc_write(codec, es9018->reg_addr, ucontrol->value.integer.value[0]);
}

static const char * const es9018_filter_shape_text[] = {
	"Fast Rolloff", "Slow Rolloff", "Minimum Phase",
};
static const SOC_ENUM_SINGLE_DECL(
	es9018_filter_shape_enum, ES9018_GENERAL_SETTINGS,
	ES9018_FILTER_SHAPE_POS, es9018_filter_shape_text);

static const char * const es9018_ch_sel_text[] = {
	"Left", "Right",
};
static const SOC_ENUM_DOUBLE_DECL(
	es9018_ch_sel_enum, ES9018_CHANNEL_MAPPING,
	ES9018_CH1_SEL_POS, ES9018_CH2_SEL_POS,
	es9018_ch_sel_text);

static const DECLARE_TLV_DB_SCALE(
	es9018_vol_tlv, -12750, 50, 0);

static const char * const es9018_custom_filter_text[] = {
	"OFF", "ON",
};

static const struct soc_enum es9018_custom_filter_enum =
	SOC_ENUM_SINGLE_EXT(2, es9018_custom_filter_text);

static const struct snd_kcontrol_new es9018_controls[] = {
	SOC_SINGLE_EXT("Reg Addr", SND_SOC_NOPM, 0, ES9018_REGISTER_NUMBER, 0,
		es9018_reg_addr_get, es9018_reg_addr_put),
	SOC_SINGLE_EXT("Reg Value", SND_SOC_NOPM, 0, 0xFF, 0,
		es9018_reg_value_get, es9018_reg_value_put),
	SOC_SINGLE("Automute Time", ES9018_SOFT_VOLUME_CONTROL1,
		ES9018_AUTOMUTE_TIME_POS, ES9018_AUTOMUTE_TIME_MAX, 0),
	SOC_SINGLE("Automute Level", ES9018_SOFT_VOLUME_CONTROL2,
		ES9018_AUTOMUTE_LEVEL_POS, ES9018_AUTOMUTE_LEVEL_MAX, 0),
	SOC_SINGLE("Automute Loopback", ES9018_SOFT_VOLUME_CONTROL2,
		ES9018_AUTOMUTE_LOOPBACK_POS, ES9018_AUTOMUTE_LOOPBACK_MAX, 0),
	SOC_SINGLE("Volume Ramp Rate", ES9018_SOFT_VOLUME_CONTROL3,
		ES9018_VOL_RATE_POS, ES9018_VOL_RATE_MAX, 0),
	SOC_DOUBLE("Mute", ES9018_GENERAL_SETTINGS,
		ES9018_CH1_MUTE_POS, ES9018_CH2_MUTE_POS,
		ES9018_CH1_MUTE_MAX, 0),
	SOC_ENUM("Filter Shape", es9018_filter_shape_enum),
	SOC_ENUM("Channel Select", es9018_ch_sel_enum),
	SOC_DOUBLE("Channel Analog Swap", ES9018_CHANNEL_MAPPING,
		ES9018_CH1_ANALOG_SWAP_POS, ES9018_CH2_ANALOG_SWAP_POS,
		ES9018_CH1_ANALOG_SWAP_MAX, 0),
	SOC_SINGLE("DPLL DSD Bandwidth", ES9018_DPLL_ASRC_SETTINGS,
		ES9018_DPLL_BW_DSD_POS, ES9018_DPLL_BW_DSD_MAX, 0),
	SOC_SINGLE("DPLL I2S Bandwidth", ES9018_DPLL_ASRC_SETTINGS,
		ES9018_DPLL_BW_I2S_POS, ES9018_DPLL_BW_I2S_MAX, 0),
	SOC_SINGLE("THD Compensation", ES9018_THD_COMPENSATION,
		ES9018_THD_COMPENSATION_POS, ES9018_THD_COMPENSATION_MAX, 0),
	SOC_DOUBLE_R_TLV("Volume", ES9018_VOLUME1, ES9018_VOLUME2,
		ES9018_VOLUME1_POS, ES9018_VOLUME1_MAX, 1, es9018_vol_tlv),
	SOC_SINGLE("THD2 Compensation", ES9018_THD2_COMPENSATION,
		ES9018_THD2_COMPENSATION_POS, ES9018_THD2_COMPENSATION_MAX, 0),
	SOC_SINGLE("THD3 Compensation", ES9018_THD3_COMPENSATION,
		ES9018_THD3_COMPENSATION_POS, ES9018_THD3_COMPENSATION_MAX, 0),
	SOC_ENUM_EXT("Custom Filter", es9018_custom_filter_enum,
		es9018_get_custom_filter, es9018_put_custom_filter),
	SOC_SINGLE_MULTI_EXT("Coefficient Stage1", 0, 1, INT_MAX, 0, ES9018_COEFFS_STAGE1_COUNT,
		es9018_get_coefficient, es9018_put_coefficient),
	SOC_SINGLE_MULTI_EXT("Coefficient Stage2", 0, 2, INT_MAX, 0, ES9018_COEFFS_STAGE2_COUNT,
		es9018_get_coefficient, es9018_put_coefficient),
	SOC_SINGLE_EXT("HPH Impedance", 0, 0, UINT_MAX, 0,
		es9018_get_hph_impedance, NULL),
};

static const struct snd_soc_dapm_widget es9018_dapm_widgets[] = {
	SND_SOC_DAPM_HP("HiFi Headphone", NULL),
};

static const struct snd_soc_dapm_route es9018_routes[] = {
	{ "HiFi Headphone", NULL, "Playback" },
};

static const u8 es9018_default[ES9018_REGISTER_NUMBER] = {
	[ES9018_SYSTEM_SETTINGS] =        0x00,
	[ES9018_INPUT_CONFIGURATION] =    0x8c,
	[ES9018_SOFT_VOLUME_CONTROL1] =   0x00,
	[ES9018_SOFT_VOLUME_CONTROL2] =   0x68,
	[ES9018_SOFT_VOLUME_CONTROL3] =   0x4a,
	[ES9018_GENERAL_SETTINGS] =       0x80,
	[ES9018_GPIO_CONFIGURATION] =     0x10,
	[ES9018_MASTER_MODE_CONTROL] =    0x00,
	[ES9018_MASTER_MODE_CONTROL2] =   0x05,
	[ES9018_CHANNEL_MAPPING] =        0x02,
	[ES9018_DPLL_ASRC_SETTINGS] =     0x5a,
	[ES9018_THD_COMPENSATION] =       0x40,
	[ES9018_SOFT_START_SETTINGS] =    0x8a,
	[ES9018_VOLUME1] =                0xcc,
	[ES9018_VOLUME2] =                0xcc,
	[ES9018_MASTER_TRIM + 0] =        0xff,
	[ES9018_MASTER_TRIM + 1] =        0xff,
	[ES9018_MASTER_TRIM + 2] =        0xff,
	[ES9018_MASTER_TRIM + 3] =        0x7f,
	[ES9018_INPUT_SELECT_CONTROL] =   0x00,
	[ES9018_THD2_COMPENSATION] =      0x00,
	[ES9018_THD3_COMPENSATION] =      0x00,
	[ES9018_FILTER_ADDRESS] =         0x00,
	[ES9018_FILTER_COEFFICIENT + 0] = 0x00,
	[ES9018_FILTER_COEFFICIENT + 1] = 0x00,
	[ES9018_FILTER_COEFFICIENT + 2] = 0x00,
	[ES9018_FILTER_CONTROL] =         0x00,
};

static const struct snd_soc_reg_access es9018_access[] = {
	{ ES9018_SYSTEM_SETTINGS,           1, 1, 0 },
	{ ES9018_INPUT_CONFIGURATION,       1, 1, 0 },
	{ ES9018_SOFT_VOLUME_CONTROL1,      1, 1, 0 },
	{ ES9018_SOFT_VOLUME_CONTROL2,      1, 1, 0 },
	{ ES9018_SOFT_VOLUME_CONTROL3,      1, 1, 0 },
	{ ES9018_GENERAL_SETTINGS,          1, 1, 0 },
	{ ES9018_MASTER_MODE_CONTROL,       1, 1, 0 },
	{ ES9018_MASTER_MODE_CONTROL2,      1, 1, 0 },
	{ ES9018_CHANNEL_MAPPING,           1, 1, 0 },
	{ ES9018_DPLL_ASRC_SETTINGS,        1, 1, 0 },
	{ ES9018_THD_COMPENSATION,          1, 1, 0 },
	{ ES9018_SOFT_START_SETTINGS,       1, 1, 0 },
	{ ES9018_VOLUME1,                   1, 1, 0 },
	{ ES9018_VOLUME2,                   1, 1, 0 },
	{ ES9018_MASTER_TRIM + 0,           1, 1, 0 },
	{ ES9018_MASTER_TRIM + 1,           1, 1, 0 },
	{ ES9018_MASTER_TRIM + 2,           1, 1, 0 },
	{ ES9018_MASTER_TRIM + 3,           1, 1, 0 },
	{ ES9018_INPUT_SELECT_CONTROL,      1, 1, 0 },
	{ ES9018_THD2_COMPENSATION,         1, 1, 0 },
	{ ES9018_THD3_COMPENSATION,         1, 1, 0 },
	{ ES9018_FILTER_ADDRESS,            1, 1, 0 },
	{ ES9018_FILTER_COEFFICIENT + 0,    1, 1, 1 },
	{ ES9018_FILTER_COEFFICIENT + 1,    1, 1, 1 },
	{ ES9018_FILTER_COEFFICIENT + 2,    1, 1, 1 },
	{ ES9018_FILTER_CONTROL,            1, 1, 1 },
	{ ES9018_SYSTEM_STATUS,             1, 0, 1 },
	{ ES9018_GPIO_STATUS,               1, 0, 1 },
	{ ES9018_DPLL_NUMBER + 0,           1, 0, 1 },
	{ ES9018_DPLL_NUMBER + 1,           1, 0, 1 },
	{ ES9018_DPLL_NUMBER + 2,           1, 0, 1 },
	{ ES9018_DPLL_NUMBER + 3,           1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 0,  1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 1,  1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 2,  1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 3,  1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 4,  1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 5,  1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 6,  1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 7,  1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 8,  1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 9,  1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 10, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 11, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 12, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 13, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 14, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 15, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 16, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 17, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 18, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 19, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 20, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 21, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 22, 1, 0, 1 },
	{ ES9018_SPDIF_CHANNEL_STATUS + 23, 1, 0, 1 },
};

static const struct snd_soc_codec_driver es9018_drv = {
	.probe = es9018_probe,
	.remove = es9018_remove,
	.controls = es9018_controls,
	.num_controls = ARRAY_SIZE(es9018_controls),
	.dapm_widgets = es9018_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(es9018_dapm_widgets),
	.dapm_routes = es9018_routes,
	.num_dapm_routes = ARRAY_SIZE(es9018_routes),
	.reg_cache_size = ARRAY_SIZE(es9018_default),
	.reg_word_size = sizeof(es9018_default[0]),
	.reg_cache_default = es9018_default,
	.reg_access_size = ARRAY_SIZE(es9018_access),
	.reg_access_default = es9018_access,
	.set_bias_level = es9018_set_bias_level,
	.idle_bias_off = 1,
};

#define ES9018_FORMATS			(SNDRV_PCM_FMTBIT_S16_LE | \
					 SNDRV_PCM_FMTBIT_S24_LE | \
					 SNDRV_PCM_FMTBIT_S24_3LE | \
					 SNDRV_PCM_FMTBIT_S32_LE)

#define ES9018_RATES			SNDRV_PCM_RATE_8000_192000

static int es9018_set_sysclk(struct snd_soc_dai *codec_dai,
			     int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct es9018_priv *es9018 = snd_soc_codec_get_drvdata(codec);

	es9018->rate_mclk = freq;
	return 0;
}

static int es9018_set_format(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	int ret;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		ret = snd_soc_update_bits(codec, ES9018_INPUT_CONFIGURATION,
			ES9018_I2S_MODE_MSK | ES9018_AUTO_INPUT_MSK | ES9018_INPUT_MSK,
			ES9018_I2S_MODE_I2S | ES9018_AUTO_INPUT_INPUT | ES9018_INPUT_I2S);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set i2s(%d)\n", ret);
			return ret;
		}
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		ret = snd_soc_update_bits(codec, ES9018_INPUT_CONFIGURATION,
			ES9018_I2S_MODE_MSK | ES9018_AUTO_INPUT_MSK | ES9018_INPUT_MSK,
			ES9018_I2S_MODE_LJ | ES9018_AUTO_INPUT_INPUT | ES9018_INPUT_I2S);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set left justify(%d)\n", ret);
			return ret;
		}
		break;
	default:
		dev_err(codec->dev, "Invalid interface format\n");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		dev_err(codec->dev, "Invalid clock inversion\n");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		ret = snd_soc_update_bits(codec,
			ES9018_MASTER_MODE_CONTROL,
			ES9018_MASTER_CLOCK_MSK,
			ES9018_MASTER_CLOCK);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set master(%d)\n", ret);
			return ret;
		}
		ret = snd_soc_update_bits(codec,
			ES9018_MASTER_MODE_CONTROL2,
			ES9018_MASTER_CLOCK_MSK,
			ES9018_MASTER_CLOCK);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set master(%d)\n", ret);
			return ret;
		}
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		ret = snd_soc_update_bits(codec,
			ES9018_MASTER_MODE_CONTROL,
			ES9018_MASTER_CLOCK_MSK,
			0);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set slave(%d)\n", ret);
			return ret;
		}
		ret = snd_soc_update_bits(codec,
			ES9018_MASTER_MODE_CONTROL2,
			ES9018_MASTER_CLOCK_MSK,
			0);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set slave(%d)\n", ret);
			return ret;
		}
		break;
	default:
		dev_err(codec->dev, "Invalid master/slave setting\n");
		return -EINVAL;
	}

	return 0;
}

static int es9018_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *codec_dai)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct es9018_priv *es9018 = snd_soc_codec_get_drvdata(codec);
	unsigned long rate_mclk, rate_bclk;
	int ret;

	dev_dbg(codec->dev, "%s: format=%d, sample rate=%d\n", __func__,
		params_format(params), params_rate(params));
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		ret = snd_soc_update_bits(codec,
			ES9018_INPUT_CONFIGURATION,
			ES9018_I2S_LENGTH_MSK,
			ES9018_I2S_LENGTH_16BIT);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set 16bit pcm(%d)\n", ret);
			return ret;
		}
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		ret = snd_soc_update_bits(codec,
			ES9018_INPUT_CONFIGURATION,
			ES9018_I2S_LENGTH_MSK,
			ES9018_I2S_LENGTH_24BIT);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set 24bit pcm(%d)\n", ret);
			return ret;
		}
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		ret = snd_soc_update_bits(codec,
			ES9018_INPUT_CONFIGURATION,
			ES9018_I2S_LENGTH_MSK,
			ES9018_I2S_LENGTH_32BIT);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set 32bit pcm(%d)\n", ret);
			return ret;
		}
		break;
	default:
		dev_err(codec->dev, "Invalid pcm format\n");
		return -EINVAL;
	}

	rate_bclk = 64 * params_rate(params);
	ret = es9018_set_mclk_rate(es9018, rate_bclk);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set mclk(%d)\n", ret);
		return ret;
	}
	rate_mclk = es9018_get_mclk_rate(es9018);

	if (rate_mclk % rate_bclk) {
		dev_err(codec->dev,
			"Invalid ratio of mclk(%lu) and bclk(%lu)\n",
			rate_mclk, rate_bclk);
		return -EINVAL;
	}

	switch (rate_mclk / rate_bclk) {
	case 4:
		ret = snd_soc_update_bits(codec,
			ES9018_MASTER_MODE_CONTROL,
			ES9018_CLOCK_DIVIDER_MSK,
			ES9018_CLOCK_DIVIDER_4);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set divider 4(%d)\n", ret);
			return ret;
		}
		ret = snd_soc_update_bits(codec,
			ES9018_MASTER_MODE_CONTROL2,
			ES9018_CLOCK_DIVIDER_MSK,
			ES9018_CLOCK_DIVIDER_4);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set divider 4(%d)\n", ret);
			return ret;
		}
		break;
	case 8:
		ret = snd_soc_update_bits(codec,
			ES9018_MASTER_MODE_CONTROL,
			ES9018_CLOCK_DIVIDER_MSK,
			ES9018_CLOCK_DIVIDER_8);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set divider 8(%d)\n", ret);
			return ret;
		}
		ret = snd_soc_update_bits(codec,
			ES9018_MASTER_MODE_CONTROL2,
			ES9018_CLOCK_DIVIDER_MSK,
			ES9018_CLOCK_DIVIDER_8);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set divider 8(%d)\n", ret);
			return ret;
		}
		break;
	case 16:
		ret = snd_soc_update_bits(codec,
			ES9018_MASTER_MODE_CONTROL,
			ES9018_CLOCK_DIVIDER_MSK,
			ES9018_CLOCK_DIVIDER_16);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set divider 16(%d)\n", ret);
			return ret;
		}
		ret = snd_soc_update_bits(codec,
			ES9018_MASTER_MODE_CONTROL2,
			ES9018_CLOCK_DIVIDER_MSK,
			ES9018_CLOCK_DIVIDER_16);
		if (ret < 0) {
			dev_err(codec->dev, "Failed to set divider 16(%d)\n", ret);
			return ret;
		}
		break;
	default:
		dev_err(codec->dev,
			"Invalid ratio of mclk(%lu) and bclk(%lu)\n",
			rate_mclk, rate_bclk);
		return -EINVAL;
	}

	return ret;
}

static int es9018_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	dev_dbg(codec->dev, "%s: enter\n", __func__);
	snd_soc_dapm_enable_pin(dapm, "HiFi Headphone");
	return 0;
}

static void es9018_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	dev_dbg(codec->dev, "%s: enter\n", __func__);
	snd_soc_dapm_disable_pin(dapm, "HiFi Headphone");
}

void es9018_set_headphone(bool insert)
{
	if (es9018_data != NULL) {
		if (insert &&
			(es9018_data->codec->dapm.bias_level == SND_SOC_BIAS_ON))
			es9018_switch(es9018_data, true);
		else
			es9018_switch(es9018_data, false);
	}
}
EXPORT_SYMBOL(es9018_set_headphone);

static const struct snd_soc_dai_ops es9018_dai_ops = {
	.startup = es9018_startup,
	.shutdown = es9018_shutdown,
	.set_sysclk = es9018_set_sysclk,
	.set_fmt = es9018_set_format,
	.hw_params = es9018_hw_params,
};

static struct snd_soc_dai_driver es9018_dai = {
	.name = "es9018-dai",
	.ops = &es9018_dai_ops,
	.playback = {
		.stream_name = "Playback",
		.formats = ES9018_FORMATS,
		.rates = ES9018_RATES,
		.channels_min = 2,
		.channels_max = 2,
	},
};

static int es9018_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	pr_debug("%s: enter\n", __func__);
	return snd_soc_register_codec(&client->dev,
			&es9018_drv, &es9018_dai, 1);
}

static int es9018_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct of_device_id es9018_of_match[] = {
	{ .compatible = "ess,es9018", },
	{ }
};
MODULE_DEVICE_TABLE(of, es9018_of_match);

static const struct i2c_device_id es9018_i2c_id[] = {
	{ "es9018", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, es9018_i2c_id);

static struct i2c_driver es9018_i2c_driver = {
	.driver = {
		.name = "es9018",
		.owner = THIS_MODULE,
		.of_match_table = es9018_of_match,
	},
	.probe = es9018_i2c_probe,
	.remove = es9018_i2c_remove,
	.id_table = es9018_i2c_id,
};
module_i2c_driver(es9018_i2c_driver);

MODULE_AUTHOR("Xiang Xiao <xiaoxiang@xiaomi.com>");
MODULE_DESCRIPTION("ASoC ES9018 codec driver");
MODULE_LICENSE("GPL");
