/*
 * tas2552.c  --  smart PA driver for TAS2552
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Author: Nannan Wang <wangnannan@xiaomi.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "tas2552.h"


#define TAS2552_PLL_CLK_48000		24576000
#define TAS2552_PLL_CLK_44100		22579200


struct tas2552_priv {
	unsigned int sysclk;
	int enable_gpio;
};

struct tas2552_reg_preset {
	u8 reg;
	u8 value;
};

static const struct tas2552_reg_preset tas2552_preset[] = {
	{TAS2552_REG_CONFIG1,				0x02},
	{TAS2552_REG_CONFIG2,				0xE3},
	{TAS2552_REG_CONFIG3,				0x5D},
	{TAS2552_REG_OUTPUT_DATA,			0xC8},
	{TAS2552_REG_PGA_GAIN,				0x16},
	{TAS2552_REG_BOOST_AUTO_PASS_THROUGH_CTRL,	0x0F},
};

/*
 * PLL_CLK = (0.5 * PLL_CLKIN * J.D) / (1 << P)
 *
 * J = 4, 5, 6, ... 96
 * D = 0, 1, 2, ... 9999
 * P = 0, 1
 *
 * D = 0, 512kHz <= (PLL_CLKIN / (1 << P)) <= 12.288MHz
 * D != 0, 1.1MHz <= (PLL_CLKIN / (1 << P)) <= 9.2MHz
 *
 * sample rate	sys clock	pll clock	p,j,d,
 * 48kHz	12.288MHz	24.5760MHz	0,4,0
 * 44.1kHz	12.288MHz	22.5792MHz	1,7,35
 *
 */
static int tas2552_set_pll_clk(struct snd_soc_codec *codec,
				unsigned int sample_rate)
{
	struct tas2552_priv *tas2552 = snd_soc_codec_get_drvdata(codec);
	unsigned int j, d, p;
	u64 jd;
	unsigned int target_clk;
	unsigned int value = 0;

	dev_dbg(codec->dev, "%s: sysclk %d, sample rate %d\n", __func__,
		tas2552->sysclk, sample_rate);
	target_clk = (sample_rate == 48000) ?
			TAS2552_PLL_CLK_48000 : TAS2552_PLL_CLK_44100;

	if (tas2552->sysclk == target_clk) {
		/* bypass PLL */
		value = 1 << TAS2552_PLLCTRL2_BYPASS_POS;
		snd_soc_update_bits(codec, TAS2552_REG_PLLCTRL2,
			TAS2552_PLLCTRL2_BYPASS_MSK, value);
	} else {
		for (p = 0; p <= 1; p++) {
			jd = ((u64)target_clk << (p + 1)) * 10000;
			do_div(jd, tas2552->sysclk);
			d = do_div(jd, 10000);
			j = jd;

			if ((j >= 4 && j <= 96) && (d <= 9999)) {
				if (d == 0) {
					if ((tas2552->sysclk / (1 << p)) >= 512000 &&
						(tas2552->sysclk / (1 << p)) <= 12288000)
						break;
				} else {
					if ((tas2552->sysclk / (1 << p)) >= 1100000 &&
						(tas2552->sysclk / (1 << p)) <= 9200000)
						break;
				}
			}
		}

		if (p > 1) {
			dev_err(codec->dev, "%s: Failed to set PLL clock\n", __func__);
			dev_err(codec->dev, "%s: sys clock %d, sample rate %d\n", __func__,
				tas2552->sysclk, sample_rate);
			return -EINVAL;
		}

		dev_dbg(codec->dev, "%s: J=%d, P=%d, D=%d\n", __func__, j, p, d);

		/* disable PLL bypass */
		snd_soc_update_bits(codec, TAS2552_REG_PLLCTRL2,
			TAS2552_PLLCTRL2_BYPASS_MSK, 0);

		/* set J and P value */
		value = ((p << TAS2552_PLLCTRL1_P_POS) & TAS2552_PLLCTRL1_P_MSK) |\
			(j & TAS2552_PLLCTRL1_J_MSK);
		snd_soc_update_bits(codec, TAS2552_REG_PLLCTRL1,
			TAS2552_PLLCTRL1_J_MSK | TAS2552_PLLCTRL1_P_MSK, value);

		/* set D value */
		value = d & 0xFF;
		snd_soc_update_bits(codec, TAS2552_REG_PLLCTRL3,
			TAS2552_PLLCTRL3_D_BIT7_0_MSK, value);
		value = (d >> 8) & 0x3F;
		snd_soc_update_bits(codec, TAS2552_REG_PLLCTRL2,
			TAS2552_PLLCTRL2_D_BIT13_8_MSK, value);
	}

	return 0;
}

static int tas2552_probe(struct snd_soc_codec *codec)
{
	struct tas2552_priv *tas2552;
	int i, ret;

	dev_dbg(codec->dev, "%s: enter\n", __func__);
	tas2552 = kzalloc(sizeof(struct tas2552_priv), GFP_KERNEL);
	if (tas2552 == NULL) {
		dev_err(codec->dev, "%s: Failed to alloc tas2552_priv\n", __func__);
		return -ENOMEM;
	}

	tas2552->enable_gpio = of_get_named_gpio(codec->dev->of_node,
				"ti,enable-gpio", 0);
	if (tas2552->enable_gpio < 0) {
		ret = tas2552->enable_gpio;
		dev_err(codec->dev, "%s: Failed to parse gpio %d\n", __func__, ret);
		kfree(tas2552);
		return ret;
	}

	/* request enable gpio and set output to high */
	ret = gpio_request(tas2552->enable_gpio, "tas2552 enable");
	if (ret < 0) {
		dev_err(codec->dev, "%s: Failed to request enable gpio %d\n",
			__func__, ret);
		kfree(tas2552);
		return ret;
	}
	gpio_direction_output(tas2552->enable_gpio, 1);

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);
	if (ret < 0) {
		dev_err(codec->dev, "%s: Failed to set cache I/O(%d)\n", __func__, ret);
		gpio_set_value(tas2552->enable_gpio, 0);
		gpio_free(tas2552->enable_gpio);
		kfree(tas2552);
		return ret;
	}

	/* set registers preset value */
	for (i = 0; i < ARRAY_SIZE(tas2552_preset); i++)
		snd_soc_write(codec, tas2552_preset[i].reg, tas2552_preset[i].value);

	snd_soc_codec_set_drvdata(codec, tas2552);

	return ret;
}

static int tas2552_remove(struct snd_soc_codec *codec)
{
	struct tas2552_priv *tas2552 = snd_soc_codec_get_drvdata(codec);

	if (tas2552->enable_gpio > 0) {
		gpio_set_value(tas2552->enable_gpio, 0);
		gpio_free(tas2552->enable_gpio);
	}

	kfree(tas2552);

	return 0;
}

static int tas2552_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	dev_dbg(codec->dev, "%s: level %d\n", __func__, level);
	switch (level) {
	case SND_SOC_BIAS_ON:
		/* mute data */
		snd_soc_update_bits(codec, TAS2552_REG_CONFIG1,
			TAS2552_CONFIG1_MUTE_MSK, TAS2552_CONFIG1_MUTE);
		snd_soc_update_bits(codec, TAS2552_REG_CONFIG3,
			TAS2552_CONFIG3_SOURCE_SELECT_MSK,
			TAS2552_CONFIG3_SOURCE_SELECT_NONE);
		/* initialization chip to power up */
		snd_soc_write(codec, TAS2552_REG_LIMITER_LEVEL_CTRL,
			TAS2552_LIMITER_LEVEL_CTRL_INIT_EN);
		snd_soc_update_bits(codec, TAS2552_REG_LIMITER_AR_HT,
			TAS2552_LIMITER_AR_HT_INIT_MSK,
			TAS2552_LIMITER_AR_HT_INIT_EN);
		snd_soc_update_bits(codec, TAS2552_REG_CONFIG2,
			TAS2552_CONFIG2_INIT_MSK,
			TAS2552_CONFIG2_INIT_EN);
		/* enable PLL */
		snd_soc_update_bits(codec, TAS2552_REG_CONFIG2,
			TAS2552_CONFIG2_PLL_EN_MSK,
			TAS2552_CONFIG2_PLL_EN_ENABLE);
		snd_soc_update_bits(codec, TAS2552_REG_CONFIG1,
			TAS2552_CONFIG1_SWS_MSK, 0);
		break;
	default:
		/* disable PLL */
		snd_soc_update_bits(codec, TAS2552_REG_CONFIG2,
			TAS2552_CONFIG2_PLL_EN_MSK, 0);
		/* SWS */
		snd_soc_update_bits(codec, TAS2552_REG_CONFIG1,
			TAS2552_CONFIG1_SWS_MSK, TAS2552_CONFIG1_SWS);
		/* reset chip to default status */
		snd_soc_update_bits(codec, TAS2552_REG_CONFIG2,
			TAS2552_CONFIG2_INIT_MSK,
			TAS2552_CONFIG2_INIT_DEFAULT);
		snd_soc_update_bits(codec, TAS2552_REG_LIMITER_AR_HT,
			TAS2552_LIMITER_AR_HT_INIT_MSK,
			TAS2552_LIMITER_AR_HT_INIT_DEFAULT);
		snd_soc_write(codec, TAS2552_REG_LIMITER_LEVEL_CTRL,
			TAS2552_LIMITER_LEVEL_CTRL_INIT_DEFAULT);
		break;
	}

	codec->dapm.bias_level = level;
	return 0;
}

static const char * const tas2552_src_sel_text[] = {
	"None", "Left", "Right", "Mono",
};

static const SOC_ENUM_SINGLE_DECL(
	tas2552_src_sel_enum, TAS2552_REG_CONFIG3,
	TAS2552_CONFIG3_SOURCE_SELECT_POS, tas2552_src_sel_text);

static const DECLARE_TLV_DB_SCALE(
	tas2552_vol_tlv, -700, 100, 0);

static const struct snd_kcontrol_new tas2552_controls[] = {
	SOC_ENUM("TAS2552 Input Channel Mux", tas2552_src_sel_enum),
	SOC_SINGLE_TLV("TAS2552 Volume", TAS2552_REG_PGA_GAIN,
		TAS2552_PGA_GAIN_POS, TAS2552_PAG_GAIN_MAX,
		0, tas2552_vol_tlv),
	SOC_SINGLE("TAS2552 Mute", TAS2552_REG_CONFIG1,
		TAS2552_CONFIG1_MUTE_POS, TAS2552_CONFIG1_MUTE_MAX, 0),
};

static const struct snd_soc_dapm_widget tas2552_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Int Spk", NULL),
};

static const struct snd_soc_dapm_route tas2552_routes[] = {
	{ "Capture", NULL, "Playback" },
	{ "Int Spk", NULL, "Playback" },
};

static const u8 tas2552_reg[0x1A] = {
	[0x00] = 0x00,	/* TAS2552_REG_DEVICE_STATUS */
	[0x01] = 0x22,	/* TAS2552_REG_CONFIG1 */
	[0x02] = 0xFF,	/* TAS2552_REG_CONFIG2 */
	[0x03] = 0x80,	/* TAS2552_REG_CONFIG3 */
	[0x04] = 0x00,	/* TAS2552_REG_DOUT_TRISTATE_MODE */
	[0x05] = 0x00,	/* TAS2552_REG_I2SCTRL1 */
	[0x06] = 0x00,	/* TAS2552_REG_I2SCTRL2 */
	[0x07] = 0xC0,	/* TAS2552_REG_OUTPUT_DATA */
	[0x08] = 0x10,	/* TAS2552_REG_PLLCTRL1 */
	[0x09] = 0x00,	/* TAS2552_REG_PLLCTRL2 */
	[0x0A] = 0x00,	/* TAS2552_REG_PLLCTRL3 */
	[0x0B] = 0x8F,	/* TAS2552_REG_BATTERY_GUARD_INFLECTION_PT */
	[0x0C] = 0x80,	/* TAS2552_REG_BATTERY_GUARD_SLOPE_CTRL */
	[0x0D] = 0xBE,	/* TAS2552_REG_LIMITER_LEVEL_CTRL */
	[0x0E] = 0x08,	/* TAS2552_REG_LIMITER_AR_HT */
	[0x0F] = 0x05,	/* TAS2552_REG_LIMITER_RELEASE_RATE */
	[0x10] = 0x00,	/* TAS2552_REG_LIMITER_INTEGRATION_COUNT_CTRL */
	[0x11] = 0x01,	/* TAS2552_REG_PDM_CONFIG */
	[0x12] = 0x00,	/* TAS2552_REG_PGA_GAIN */
	[0x13] = 0x40,	/* TAS2552_REG_CLASS_D_EDGE_RATE_CTRL */
	[0x14] = 0x00,	/* TAS2552_REG_BOOST_AUTO_PASS_THROUGH_CTRL */
	[0x15] = 0x00,	/* TAS2552_REG_RESERVED */
	[0x16] = 0x00,	/* TAS2552_REG_VERSION_NUMBER */
	[0x17] = 0x00,	/* TAS2552_REG_INTERRUPT_MASK */
	[0x18] = 0x00,	/* TAS2552_REG_VBOOST_DATA */
	[0x19] = 0x00,	/* TAS2552_REG_VBAT_DATA */
};

static const struct snd_soc_codec_driver tas2552_drv = {
	.probe = tas2552_probe,
	.remove = tas2552_remove,
	.controls = tas2552_controls,
	.num_controls = ARRAY_SIZE(tas2552_controls),
	.dapm_widgets = tas2552_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tas2552_dapm_widgets),
	.dapm_routes = tas2552_routes,
	.num_dapm_routes = ARRAY_SIZE(tas2552_routes),
	.reg_cache_size = ARRAY_SIZE(tas2552_reg),
	.reg_word_size = sizeof(tas2552_reg[0]),
	.reg_cache_default = tas2552_reg,
	.set_bias_level = tas2552_set_bias_level,
	.idle_bias_off = 1,
};

#define TAS2552_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE |\
				SNDRV_PCM_FMTBIT_S24_LE |\
				SNDRV_PCM_FMTBIT_S32_LE)

#define TAS2552_RATES		(SNDRV_PCM_RATE_44100 |\
				SNDRV_PCM_RATE_48000)

static int tas2552_set_sysclk(struct snd_soc_dai *codec_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct tas2552_priv *tas2552 = snd_soc_codec_get_drvdata(codec);
	unsigned int value = 0;

	dev_dbg(codec->dev, "%s: clk_id %d, freq %d\n", __func__,
		clk_id, freq);
	switch (clk_id) {
	case TAS2552_SCLK_S_MCLK:
		value = TAS2552_CONFIG1_PLL_SRC_MCLK;
		break;
	case TAS2552_SCLK_S_BCLK:
		value = TAS2552_CONFIG1_PLL_SRC_BCLK;
		break;
	case TAS2552_SCLK_S_IVCLKIN:
		value = TAS2552_CONFIG1_PLL_SRC_IVCLKIN;
		break;
	case TAS2552_SCLK_S_INTERNAL_1P8:
		value = TAS2552_CONFIG1_PLL_SRC_INTERNAL_1P8;
		break;
	default:
		dev_err(codec->dev, "%s: Unknown clock source: %d\n", __func__, clk_id);
		return -EINVAL;
	}

	snd_soc_update_bits(codec, TAS2552_REG_CONFIG1,
		TAS2552_CONFIG1_PLL_SRC_MSK, value);

	tas2552->sysclk = freq;
	return 0;
}

static int tas2552_set_format(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	unsigned int value = 0;

	dev_dbg(codec->dev, "%s: fmt 0x%x\n", __func__, fmt);
	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		value |= TAS2552_I2SCTRL1_PCM_DATAFMT_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		value |= TAS2552_I2SCTRL1_PCM_DATAFMT_RJF;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		value |= TAS2552_I2SCTRL1_PCM_DATAFMT_LJF;
		break;
	default:
		dev_err(codec->dev, "Invalid interface format\n");
		return -EINVAL;
	}

	snd_soc_update_bits(codec, TAS2552_REG_I2SCTRL1,
		TAS2552_I2SCTRL1_PCM_DATAFMT_MSK, value);

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		dev_err(codec->dev, "Invalid clock inversion\n");
		return -EINVAL;
	}

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		/* set BCLK & WCLK as input */
		snd_soc_update_bits(codec, TAS2552_REG_I2SCTRL1,
			TAS2552_I2SCTRL1_PCM_BCLKDIR_MSK |
			TAS2552_I2SCTRL1_PCM_WCLKDIR_MSK, 0);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		/* set BCLK & WCLK as output */
		snd_soc_update_bits(codec, TAS2552_REG_I2SCTRL1,
			TAS2552_I2SCTRL1_PCM_BCLKDIR_MSK |
			TAS2552_I2SCTRL1_PCM_WCLKDIR_MSK,
			TAS2552_I2SCTRL1_PCM_BCLKDIR_OUTPUT |
			TAS2552_I2SCTRL1_PCM_WCLKDIR_OUTPUT);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		/* set BCLK as input, and WCLK as output */
		snd_soc_update_bits(codec, TAS2552_REG_I2SCTRL1,
			TAS2552_I2SCTRL1_PCM_BCLKDIR_MSK |
			TAS2552_I2SCTRL1_PCM_WCLKDIR_MSK,
			TAS2552_I2SCTRL1_PCM_WCLKDIR_OUTPUT);
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		/* set BCLK as output, and WCLK as input */
		snd_soc_update_bits(codec, TAS2552_REG_I2SCTRL1,
			TAS2552_I2SCTRL1_PCM_BCLKDIR_MSK |
			TAS2552_I2SCTRL1_PCM_WCLKDIR_MSK,
			TAS2552_I2SCTRL1_PCM_BCLKDIR_OUTPUT);
		break;
	default:
		dev_err(codec->dev, "Invalid master/slave setting\n");
		return -EINVAL;
	}

	return 0;
}

static int tas2552_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s: mute %d\n", __func__, mute);
	snd_soc_update_bits(codec, TAS2552_REG_CONFIG1,
			TAS2552_CONFIG1_MUTE_MSK,
			mute ? TAS2552_CONFIG1_MUTE : 0);

	if (!mute) {
		/* sleep 10ms to wait signal is clean */
		usleep(10000);
		dev_dbg(codec->dev, "unmute, set source selete to mix\n");
		snd_soc_update_bits(codec, TAS2552_REG_CONFIG3,
				TAS2552_CONFIG3_SOURCE_SELECT_MSK,
				TAS2552_CONFIG3_SOURCE_SELECT_MONO);
	}

	return 0;
}

static int tas2552_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int value = 0;

	dev_dbg(codec->dev, "%s: fmt %d, rate %d\n", __func__,
		params_format(params), params_rate(params));
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		value = TAS2552_I2SCTRL1_PCM_FMT_16;
		value |= TAS2552_I2SCTRL1_PCM_BCLK_32;
		break;
/*	case SNDRV_PCM_FORMAT_S20_3LE:
		value = TAS2552_I2SCTRL1_PCM_FMT_20;
		value |= TAS2552_I2SCTRL1_PCM_BCLK_64;
		break;*/
	case SNDRV_PCM_FORMAT_S24_LE:
		value = TAS2552_I2SCTRL1_PCM_FMT_24;
		value |= TAS2552_I2SCTRL1_PCM_BCLK_64;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		value = TAS2552_I2SCTRL1_PCM_FMT_32;
		value |= TAS2552_I2SCTRL1_PCM_BCLK_64;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid format!\n", __func__);
		return -EINVAL;
	}

	snd_soc_update_bits(codec, TAS2552_REG_I2SCTRL1,
		TAS2552_I2SCTRL1_PCM_FMT_MSK | TAS2552_I2SCTRL1_PCM_BCLK_MSK,
		value);

	switch (params_rate(params)) {
	case 48000:
	case 44100:
		value = TAS2552_CONFIG3_WCLK_44100_48000;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid sample rate!\n", __func__);
		return -EINVAL;
	}

	snd_soc_update_bits(codec, TAS2552_REG_CONFIG3,
			TAS2552_CONFIG3_WCLK_MSK, value);

	return tas2552_set_pll_clk(codec, params_rate(params));
}

static int tas2552_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	dev_dbg(codec->dev, "%s: enter\n", __func__);
	snd_soc_dapm_enable_pin(dapm, "Int Spk");
	return 0;
}

static void tas2552_shutdown(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	dev_dbg(codec->dev, "%s: enter\n", __func__);
	snd_soc_dapm_disable_pin(dapm, "Int Spk");
}


static const struct snd_soc_dai_ops tas2552_dai_ops = {
	.startup = tas2552_startup,
	.shutdown = tas2552_shutdown,
	.set_sysclk = tas2552_set_sysclk,
	.set_fmt = tas2552_set_format,
	.digital_mute = tas2552_digital_mute,
	.hw_params = tas2552_hw_params,
};


static struct snd_soc_dai_driver tas2552_dai = {
	.name = "tas2552-dai",
	.ops = &tas2552_dai_ops,
	.capture = {
		.stream_name = "Capture",
		.formats = TAS2552_FORMATS,
		.rates = TAS2552_RATES,
		.channels_min = 2,
		.channels_max = 2,
	},
	.playback = {
		.stream_name = "Playback",
		.formats = TAS2552_FORMATS,
		.rates = TAS2552_RATES,
		.channels_min = 2,
		.channels_max = 2,
	},
	.symmetric_rates = 1,
};

static int tas2552_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	dev_dbg(&client->dev, "%s: enter\n", __func__);
	return snd_soc_register_codec(&client->dev,
			&tas2552_drv, &tas2552_dai, 1);
}

static int tas2552_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct of_device_id tas2552_of_match[] = {
	{.compatible = "ti, tas2552"},
	{ },
};
MODULE_DEVICE_TABLE(of, tas2552_of_match);

static const struct i2c_device_id tas2552_i2c_id[] = {
	{"tas2552", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, tas2552_id_table);

static struct i2c_driver tas2552_i2c_driver = {
	.driver = {
		.name = "tas2552",
		.owner = THIS_MODULE,
		.of_match_table = tas2552_of_match,
	},
	.probe = tas2552_i2c_probe,
	.remove = tas2552_i2c_remove,
	.id_table = tas2552_i2c_id,
};

module_i2c_driver(tas2552_i2c_driver);

MODULE_AUTHOR("Nannan Wang <wangnannan@xiaomi.com>");
MODULE_DESCRIPTION("TI TAS2552 chip driver");
MODULE_LICENSE("GPL");
