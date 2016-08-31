/*
 * tegra_vcm30t124.c - Tegra VCM30 T124 Machine driver
 *
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>

#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "../codecs/wm8731.h"
#include "../codecs/ad193x.h"

#include "tegra_asoc_utils_alt.h"

#define DRV_NAME "tegra-snd-vcm30t124"

#define GPIO_PR0 136
#define CODEC_TO_DAP 0
#define DAP_TO_CODEC 1

struct tegra_vcm30t124 {
	struct tegra_asoc_audio_clock_info audio_clock;
	int gpio_dap_direction;
	struct i2c_client *max9485_client;
};

static struct i2c_board_info max9485_info = {
	I2C_BOARD_INFO("max9485", 0x60),
};

#define MAX9485_MCLK_FREQ_163840 0x31
#define MAX9485_MCLK_FREQ_112896 0x22
#define MAX9485_MCLK_FREQ_122880 0x23
#define MAX9485_MCLK_FREQ_225792 0x32
#define MAX9485_MCLK_FREQ_245760 0x33

static void set_max9485_clk(struct i2c_client *i2s, int mclk)
{
	char clk;

	switch (mclk) {
	case 16384000:
		clk =  MAX9485_MCLK_FREQ_163840;
		break;
	case 11289600:
		clk = MAX9485_MCLK_FREQ_112896;
		break;
	case 12288000:
		clk = MAX9485_MCLK_FREQ_122880;
		break;
	case 22579200:
		clk = MAX9485_MCLK_FREQ_225792;
		break;
	case 24576000:
		clk = MAX9485_MCLK_FREQ_245760;
		break;
	default:
		return;
	}
	i2c_master_send(i2s, &clk, 1);
}

static int tegra_vcm30t124_x_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[10].dai_link->params;
	int srate, mclk, clk_out_rate;
	int err;

	srate = params_rate(params);
	switch (srate) {
	case 64000:
	case 88200:
	case 96000:
		clk_out_rate = 128 * srate;
		mclk = clk_out_rate * 2;
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	default:
		clk_out_rate = 12288000;
		/*
		 * MCLK is pll_a_out, it is a source clock of ahub.
		 * So it need to be faster than BCLK in slave mode.
		 */
		mclk = 12288000 * 2;
		break;
	case 44100:
		clk_out_rate = 11289600;
		/*
		 * MCLK is pll_a_out, it is a source clock of ahub.
		 * So it need to be faster than BCLK in slave mode.
		 */
		mclk = 11289600 * 2;
		break;
	}

	/* update link_param to update hw_param for DAPM */
	dai_params->rate_min = srate;

	err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock,
					srate, mclk, clk_out_rate);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(card->rtd[10].codec_dai,
			WM8731_SYSCLK_MCLK, clk_out_rate, SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "x codec_dai clock not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(card->rtd[10].cpu_dai, 0, srate,
					SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "x cpu_dai clock not set\n");
		return err;
	}

	return 0;
}

static int tegra_vcm30t124_y_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[12].dai_link->params;
	unsigned int fmt = card->rtd[12].dai_link->dai_fmt;
	int srate, mclk, clk_out_rate, val;
	int err;

	srate = params_rate(params);
	switch (srate) {
	case 64000:
	case 88200:
	case 96000:
		clk_out_rate = 128 * srate;
		break;
	default:
		clk_out_rate = 256 * srate;
		break;
	}

	/* update link_param to update hw_param for DAPM */
	dai_params->rate_min = srate;

	mclk = clk_out_rate * 2;

	set_max9485_clk(machine->max9485_client, mclk);

	err = snd_soc_dai_set_sysclk(card->rtd[12].codec_dai,
			0, mclk,
			SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "y codec_dai clock not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(card->rtd[12].cpu_dai, 0, srate,
					SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "y cpu_dai clock not set\n");
		return err;
	}

	/*
	 * AD193X driver enables both DAC and ADC as MASTER
	 * so both ADC and DAC drive LRCLK and BCLK and it causes
	 * noise. To solve this, we need to disable one of them.
	 */
	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM) {
		val = snd_soc_read(card->rtd[12].codec_dai->codec,
				AD193X_DAC_CTRL1);
		val &= ~AD193X_DAC_LCR_MASTER;
		val &= ~AD193X_DAC_BCLK_MASTER;
		snd_soc_write(card->rtd[12].codec_dai->codec,
				AD193X_DAC_CTRL1, val);
	}

	return 0;
}

static int tegra_vcm30t124_x_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static void tegra_vcm30t124_x_shutdown(struct snd_pcm_substream *substream)
{
	return;
}

static int tegra_vcm30t124_y_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static void tegra_vcm30t124_y_shutdown(struct snd_pcm_substream *substream)
{
	return;
}

static struct snd_soc_ops tegra_vcm30t124_x_ops = {
	.hw_params = tegra_vcm30t124_x_hw_params,
	.startup = tegra_vcm30t124_x_startup,
	.shutdown = tegra_vcm30t124_x_shutdown,
};

static struct snd_soc_ops tegra_vcm30t124_y_ops = {
	.hw_params = tegra_vcm30t124_y_hw_params,
	.startup = tegra_vcm30t124_y_startup,
	.shutdown = tegra_vcm30t124_y_shutdown,
};

static const struct snd_soc_dapm_widget tegra_vcm30t124_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone-x", NULL),
	SND_SOC_DAPM_HP("Headphone-y", NULL),
	SND_SOC_DAPM_LINE("LineIn-x", NULL),
	SND_SOC_DAPM_LINE("LineIn-y", NULL),
};

static const struct snd_soc_dapm_route tegra_vcm30t124_audio_map[] = {
	{"Headphone-y",	NULL,	"y DAC1OUT"},
	{"Headphone-y", NULL,	"y DAC2OUT"},
	{"Headphone-y",	NULL,	"y DAC3OUT"},
	{"Headphone-y", NULL,	"y DAC4OUT"},
	{"y ADC1IN",	NULL,	"LineIn-y"},
	{"y ADC2IN",	NULL,	"LineIn-y"},
	{"Headphone-x",	NULL,	"x ROUT"},
	{"Headphone-x",	NULL,	"x LOUT"},
	{"x LLINEIN",	NULL,	"LineIn-x"},
	{"x RLINEIN",	NULL,	"LineIn-x"},
};

static int tegra_vcm30t124_wm8731_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *wm8731_dai = card->rtd[10].codec_dai;
	struct snd_soc_dai *i2s_dai = card->rtd[10].cpu_dai;
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[10].dai_link->params;
	unsigned int clk_out, mclk, srate;
	int err;

	srate = 48000;
	clk_out = srate * 256;
	mclk = clk_out * 2;

	/* update link_param to update hw_param for DAPM */
	dai_params->rate_min = srate;

	tegra_alt_asoc_utils_set_parent(&machine->audio_clock, true);

	/* wm8731 needs mclk from tegra */
	err = tegra_alt_asoc_utils_set_rate(&machine->audio_clock,
					srate, mclk, clk_out);
	if (err < 0) {
		dev_err(card->dev, "Can't configure clocks\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(wm8731_dai, WM8731_SYSCLK_MCLK, clk_out,
					SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "wm8731 clock not set\n");
		return err;
	}

	err = snd_soc_dai_set_sysclk(i2s_dai, 0, srate, SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "i2s clock not set\n");
		return err;
	}

	return 0;
}

static int tegra_vcm30t124_ad1937_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *ad1937_dai = card->rtd[12].codec_dai;
	struct snd_soc_dai *i2s_dai = card->rtd[12].cpu_dai;
	struct snd_soc_pcm_stream *dai_params =
		(struct snd_soc_pcm_stream *)card->rtd[12].dai_link->params;
	unsigned int fmt = card->rtd[12].dai_link->dai_fmt;
	unsigned int mclk, srate;
	int err;

	srate = 48000;
	mclk = srate * 512;

	/* update link_param to update hw_param for DAPM */
	dai_params->rate_min = srate;

	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM) {
		/* direct MCLK mode in AD1937, mclk needs to be srate * 512 */
		set_max9485_clk(machine->max9485_client, mclk);
		err = snd_soc_dai_set_sysclk(ad1937_dai, 0, mclk,
						SND_SOC_CLOCK_IN);
		if (err < 0) {
			dev_err(card->dev, "ad1937 clock not set\n");
			return err;
		}

		snd_soc_write(ad1937_dai->codec, AD193X_PLL_CLK_CTRL1, 0x03);

		/* set SCLK, FS direction from codec to dap */
		gpio_direction_output(machine->gpio_dap_direction,
					CODEC_TO_DAP);
	} else {
		/* set PLL_SRC with LRCLK for AD1937 slave mode */
		snd_soc_write(ad1937_dai->codec, AD193X_PLL_CLK_CTRL0, 0xb9);

		/* set SCLK, FS direction from dap to codec */
		gpio_direction_output(machine->gpio_dap_direction,
					DAP_TO_CODEC);
	}

	err = snd_soc_dai_set_sysclk(i2s_dai, 0, srate, SND_SOC_CLOCK_IN);
	if (err < 0) {
		dev_err(card->dev, "i2s clock not set %d\n", __LINE__);
		return err;
	}

	if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_DSP_A)
		ad1937_dai->driver->ops->set_tdm_slot(ad1937_dai, 0, 0, 8, 0);

	return 0;
}

static int tegra_vcm30t124_amx_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct snd_soc_dai *amx_dai = card->rtd[18].cpu_dai;
	unsigned int tx_slot[32], i, j;

	for (i = 0, j = 0; i < 32; i += 8) {
		tx_slot[i] = 0;
		tx_slot[i + 1] = 0;
		tx_slot[i + 2] = (j << 16) | (1 << 8) | 0;
		tx_slot[i + 3] = (j << 16) | (1 << 8) | 1;
		tx_slot[i + 4] = 0;
		tx_slot[i + 5] = 0;
		tx_slot[i + 6] = (j << 16) | (2 << 8) | 0;
		tx_slot[i + 7] = (j << 16) | (2 << 8) | 1;
		j++;
	}

	if (amx_dai->driver->ops->set_channel_map)
		amx_dai->driver->ops->set_channel_map(amx_dai,
							32, tx_slot, 0, 0);

	return 0;
}

static int tegra_vcm30t124_adx_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	struct snd_soc_card *card = codec->card;
	struct snd_soc_dai *adx_dai = card->rtd[19].codec_dai;
	unsigned int rx_slot[32], i, j;

	for (i = 0, j = 0; i < 32; i += 8) {
		rx_slot[i] = 0;
		rx_slot[i + 1] = 0;
		rx_slot[i + 2] = (j << 16) | (1 << 8) | 0;
		rx_slot[i + 3] = (j << 16) | (1 << 8) | 1;
		rx_slot[i + 4] = 0;
		rx_slot[i + 5] = 0;
		rx_slot[i + 6] = (j << 16) | (2 << 8) | 0;
		rx_slot[i + 7] = (j << 16) | (2 << 8) | 1;
		j++;
	}

	if (adx_dai->driver->ops->set_channel_map)
		adx_dai->driver->ops->set_channel_map(adx_dai,
							0, 0, 32, rx_slot);

	return 0;
}

static int tegra_vcm30t124_remove(struct snd_soc_card *card)
{
	return 0;
}

static const struct snd_soc_pcm_stream x_link_params = {
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static const struct snd_soc_pcm_stream y_link_params = {
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min = 32000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static const struct snd_soc_pcm_stream tdm_link_params = {
	.formats = SNDRV_PCM_FMTBIT_S32_LE,
	.rate_min = 44100,
	.rate_max = 48000,
	.channels_min = 8,
	.channels_max = 8,
};

static struct snd_soc_dai_link tegra_vcm30t124_links[] = {
	{
		/* 0 */
		.name = "APBIF0 CIF",
		.stream_name = "APBIF0 CIF",
		/* .cpu_of_node = AHUB APBIF */
		.cpu_dai_name = "APBIF0",
		/* .codec_of_node = AHUB XBAR */
		.codec_dai_name = "APBIF0",
		.ops = &tegra_vcm30t124_y_ops,
		.ignore_pmdown_time = 1,
	},
	{
		/* 1 */
		.name = "APBIF1 CIF",
		.stream_name = "APBIF1 CIF",
		/* .cpu_of_node = AHUB APBIF */
		.cpu_dai_name = "APBIF1",
		/* .codec_of_node = AHUB XBAR */
		.codec_dai_name = "APBIF1",
		.ops = &tegra_vcm30t124_y_ops,
		.ignore_pmdown_time = 1,
	},
	{
		/* 2 */
		.name = "APBIF2 CIF",
		.stream_name = "APBIF2 CIF",
		/* .cpu_of_node = AHUB APBIF */
		.cpu_dai_name = "APBIF2",
		/* .codec_of_node = AHUB XBAR */
		.codec_dai_name = "APBIF2",
		.ops = &tegra_vcm30t124_y_ops,
		.ignore_pmdown_time = 1,
	},
	{
		/* 3 */
		.name = "APBIF3 CIF",
		.stream_name = "APBIF3 CIF",
		/* .cpu_of_node = AHUB APBIF */
		.cpu_dai_name = "APBIF3",
		/* .codec_of_node = AHUB XBAR */
		.codec_dai_name = "APBIF3",
		.ops = &tegra_vcm30t124_y_ops,
		.ignore_pmdown_time = 1,
	},
	{
		/* 4 */
		.name = "APBIF4 CIF",
		.stream_name = "APBIF4 CIF",
		/* .cpu_of_node = AHUB APBIF */
		.cpu_dai_name = "APBIF4",
		/* .codec_of_node = AHUB XBAR */
		.codec_dai_name = "APBIF4",
		.ops = &tegra_vcm30t124_x_ops,
		.ignore_pmdown_time = 1,
	},

	{
		/* 5 */
		.name = "APBIF5 CIF",
		.stream_name = "APBIF5 CIF",
		/* .cpu_of_node = AHUB APBIF */
		.cpu_dai_name = "APBIF5",
		/* .codec_of_node = AHUB XBAR */
		.codec_dai_name = "APBIF5",
		.ops = &tegra_vcm30t124_y_ops,
		.ignore_pmdown_time = 1,
	},
	{
		/* 6 */
		.name = "APBIF6 CIF",
		.stream_name = "APBIF6 CIF",
		/* .cpu_of_node = AHUB APBIF */
		.cpu_dai_name = "APBIF6",
		/* .codec_of_node = AHUB XBAR */
		.codec_dai_name = "APBIF6",
		.ops = &tegra_vcm30t124_y_ops,
		.ignore_pmdown_time = 1,
	},
	{
		/* 7 */
		.name = "APBIF7 CIF",
		.stream_name = "APBIF7 CIF",
		/* .cpu_of_node = AHUB APBIF */
		.cpu_dai_name = "APBIF7",
		/* .codec_of_node = AHUB XBAR */
		.codec_dai_name = "APBIF7",
		.ops = &tegra_vcm30t124_y_ops,
		.ignore_pmdown_time = 1,
	},
	{
		/* 8 */
		.name = "APBIF8 CIF",
		.stream_name = "APBIF8 CIF",
		/* .cpu_of_node = AHUB APBIF */
		.cpu_dai_name = "APBIF8",
		/* .codec_of_node = AHUB XBAR */
		.codec_dai_name = "APBIF8",
		.ops = &tegra_vcm30t124_y_ops,
		.ignore_pmdown_time = 1,
	},
	{
		/* 9 */
		.name = "APBIF9 CIF",
		.stream_name = "APBIF9 CIF",
		/* .cpu_of_node = AHUB APBIF */
		.cpu_dai_name = "APBIF9",
		/* .codec_of_node = AHUB XBAR */
		.codec_dai_name = "APBIF9",
		.ops = &tegra_vcm30t124_y_ops,
		.ignore_pmdown_time = 1,
	},
	{
		/* 10 */
		.name = "wm8731",
		.stream_name = "Playback",
		/* .cpu_of_node = I2S0 */
		.cpu_dai_name = "DAP",
		/* .codec_of_node = WM8731 */
		.codec_dai_name = "wm8731-hifi",
		.init = tegra_vcm30t124_wm8731_init,
		.params = &x_link_params,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBS_CFS,
	},
	{
		/* 11 */
		.name = "I2S0 CIF",
		.stream_name = "I2S0 CIF",
		/* .cpu_of_node = AHUB XBAR */
		.cpu_dai_name = "I2S0",
		/* .codec_of_node = I2S0 */
		.codec_dai_name = "CIF",
		.params = &x_link_params,
	},
	{
		/* 12 */
		.name = "ad1937",
		.stream_name = "Playback",
		/* .cpu_of_node = I2S4 */
		.cpu_dai_name = "DAP",
		/* .codec_of_node = AD1937 */
		.codec_dai_name = "ad193x-hifi",
		.init = tegra_vcm30t124_ad1937_init,
		.params = &tdm_link_params,
		.dai_fmt = SND_SOC_DAIFMT_DSP_A |
			   SND_SOC_DAIFMT_NB_NF |
			   SND_SOC_DAIFMT_CBM_CFM,
	},

	{
		/* 13 */
		.name = "I2S4 CIF",
		.stream_name = "I2S4 CIF",
		/* .cpu_of_node = AHUB XBAR */
		.cpu_dai_name = "I2S4",
		/* .codec_of_node = I2S4 */
		.codec_dai_name = "CIF",
		.params = &tdm_link_params,
	},
	{
		/* 14 */
		.name = "AMX0 IN0",
		.stream_name = "AMX0 IN",
		/* .cpu_of_node = AHUB XBAR */
		.cpu_dai_name = "AMX0-0",
		/* .codec_of_node = AMX0 */
		.codec_dai_name = "IN0",
		.params = &y_link_params,
	},
	{
		/* 15 */
		.name = "AMX0 IN1",
		.stream_name = "AMX0 IN",
		/* .cpu_of_node = AHUB XBAR */
		.cpu_dai_name = "AMX0-1",
		/* .codec_of_node = AMX0 */
		.codec_dai_name = "IN1",
		.params = &y_link_params,
	},
	{
		/* 16 */
		.name = "AMX0 IN2",
		.stream_name = "AMX0 IN",
		/* .cpu_of_node = AHUB XBAR */
		.cpu_dai_name = "AMX0-2",
		/* .codec_of_node = AMX0 */
		.codec_dai_name = "IN2",
		.params = &y_link_params,
	},
	{
		/* 17 */
		.name = "AMX0 IN3",
		.stream_name = "AMX0 IN",
		/* .cpu_of_node = AHUB XBAR */
		.cpu_dai_name = "AMX0-3",
		/* .codec_of_node = AMX0 */
		.codec_dai_name = "IN3",
		.params = &y_link_params,
	},
	{
		/* 18 */
		.name = "AMX0 CIF",
		.stream_name = "AMX0 CIF",
		/* .cpu_of_node = AMX0 OUT */
		.cpu_dai_name = "OUT",
		/* .codec_of_node = AHUB XBAR */
		.codec_dai_name = "AMX0",
		.init = tegra_vcm30t124_amx_dai_init,
		.params = &tdm_link_params,
	},
	{
		/* 19 */
		.name = "ADX0 CIF",
		.stream_name = "ADX0 IN",
		.cpu_dai_name = "ADX0",
		.codec_dai_name = "IN",
		.init = tegra_vcm30t124_adx_dai_init,
		.params = &tdm_link_params,
	},
	{
		/* 20 */
		.name = "ADX0 OUT0",
		.stream_name = "ADX0 OUT",
		.cpu_dai_name = "OUT0",
		.codec_dai_name = "ADX0-0",
		.params = &y_link_params,
	},
	{
		/* 21 */
		.name = "ADX0 OUT1",
		.stream_name = "ADX0 OUT",
		.cpu_dai_name = "OUT1",
		.codec_dai_name = "ADX0-1",
		.params = &y_link_params,
	},
	{
		/* 22 */
		.name = "ADX0 OUT2",
		.stream_name = "ADX0 OUT",
		.cpu_dai_name = "OUT2",
		.codec_dai_name = "ADX0-2",
		.params = &y_link_params,
	},
	{
		/* 23 */
		.name = "ADX0 OUT3",
		.stream_name = "ADX0 OUT",
		.cpu_dai_name = "OUT3",
		.codec_dai_name = "ADX0-3",
		.params = &y_link_params,
	},
};

static struct snd_soc_codec_conf ad193x_codec_conf[] = {
	{
		.dev_name = "wm8731.0-001a",
		.name_prefix = "x",
	},
	{
		.dev_name = "ad193x.0-0007",
		.name_prefix = "y",
	},
	{
		.dev_name = "tegra30-i2s.0",
		.name_prefix = "I2S0",
	},
	{
		.dev_name = "tegra30-i2s.4",
		.name_prefix = "I2S4",
	},
};

static struct snd_soc_card snd_soc_tegra_vcm30t124 = {
	.name = "tegra-vcm30t124",
	.owner = THIS_MODULE,
	.dai_link = tegra_vcm30t124_links,
	.num_links = ARRAY_SIZE(tegra_vcm30t124_links),
	.remove = tegra_vcm30t124_remove,
	.dapm_widgets = tegra_vcm30t124_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra_vcm30t124_dapm_widgets),
	.codec_conf = ad193x_codec_conf,
	.num_configs = ARRAY_SIZE(ad193x_codec_conf),
	.fully_routed = true,
};


static int tegra_vcm30t124_driver_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &snd_soc_tegra_vcm30t124;
	struct tegra_vcm30t124 *machine;
	int ret, i;

	machine = devm_kzalloc(&pdev->dev, sizeof(struct tegra_vcm30t124),
			       GFP_KERNEL);
	if (!machine) {
		dev_err(&pdev->dev, "Can't allocate tegra_vcm30t124 struct\n");
		ret = -ENOMEM;
		goto err;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, machine);

	if (np) {
		ret = snd_soc_of_parse_card_name(card, "nvidia,model");
		if (ret)
			goto err;

		ret = snd_soc_of_parse_audio_routing(card,
					"nvidia,audio-routing");
		if (ret)
			goto err;

		machine->gpio_dap_direction = of_get_named_gpio(np,
					"nvidia,dap_direction_gpios", 0);

		if (!gpio_is_valid(machine->gpio_dap_direction)) {
			ret = -EINVAL;
			goto err;
		}

		tegra_vcm30t124_links[10].codec_of_node = of_parse_phandle(np,
					"nvidia,audio-codec-x", 0);
		if (!tegra_vcm30t124_links[10].codec_of_node) {
			dev_err(&pdev->dev,
				"Property 'nvidia,audio-codec-x' missing or invalid\n");
			ret = -EINVAL;
			goto err;
		}

		tegra_vcm30t124_links[10].cpu_of_node = of_parse_phandle(np,
					"nvidia,i2s-controller-1", 0);
		if (!tegra_vcm30t124_links[10].cpu_of_node) {
			dev_err(&pdev->dev,
				"Property 'nvidia,i2s-controller-1' missing or invalid\n");
			ret = -EINVAL;
			goto err;
		}

		of_property_read_string(np, "nvidia,xbar",
					&tegra_vcm30t124_links[11].cpu_name);
		if (!tegra_vcm30t124_links[11].cpu_name) {
			dev_err(&pdev->dev,
				"Property 'nvidia,xbar' missing or invalid\n");
			ret = -EINVAL;
			goto err;
		}

		tegra_vcm30t124_links[11].codec_of_node =
					tegra_vcm30t124_links[10].cpu_of_node;

		tegra_vcm30t124_links[12].codec_of_node = of_parse_phandle(np,
					"nvidia,audio-codec-y", 0);
		if (!tegra_vcm30t124_links[12].codec_of_node) {
			dev_err(&pdev->dev,
				"Property 'nvidia,audio-codec-y' missing or invalid\n");
			ret = -EINVAL;
			goto err;
		}

		tegra_vcm30t124_links[12].cpu_of_node = of_parse_phandle(np,
					"nvidia,i2s-controller-2", 0);
		if (!tegra_vcm30t124_links[12].cpu_of_node) {
			dev_err(&pdev->dev,
				"Property 'nvidia,i2s-controller-2' missing or invalid\n");
			ret = -EINVAL;
			goto err;
		}

		tegra_vcm30t124_links[13].cpu_name =
					tegra_vcm30t124_links[11].cpu_name;

		tegra_vcm30t124_links[13].codec_of_node =
					tegra_vcm30t124_links[12].cpu_of_node;

		tegra_vcm30t124_links[18].cpu_of_node = of_parse_phandle(np,
					"nvidia,amx", 0);
		if (!tegra_vcm30t124_links[18].cpu_of_node) {
			dev_err(&pdev->dev,
				"Property 'nvidia,amx' missing or invalid\n");
			ret = -EINVAL;
			goto err;
		}

		tegra_vcm30t124_links[18].codec_name =
					tegra_vcm30t124_links[13].cpu_name;

		tegra_vcm30t124_links[19].codec_of_node = of_parse_phandle(np,
					"nvidia,adx", 0);
		if (!tegra_vcm30t124_links[19].codec_of_node) {
			dev_err(&pdev->dev,
				"Property 'nvidia,adx' missing or invalid\n");
			ret = -EINVAL;
			goto err;
		}

		tegra_vcm30t124_links[19].cpu_name =
					tegra_vcm30t124_links[13].cpu_name;

		tegra_vcm30t124_links[0].cpu_of_node = of_parse_phandle(np,
					"nvidia,apbif", 0);
		if (!tegra_vcm30t124_links[0].cpu_of_node) {
			dev_err(&pdev->dev,
				"Property 'nvidia,apbif' missing or invalid\n");
			ret = -EINVAL;
			goto err;
		}

		tegra_vcm30t124_links[0].codec_name =
				tegra_vcm30t124_links[11].cpu_name;
		tegra_vcm30t124_links[0].platform_of_node =
				tegra_vcm30t124_links[0].cpu_of_node;

		for (i = 1; i < 10; i++) {
			tegra_vcm30t124_links[i].cpu_of_node =
				tegra_vcm30t124_links[0].cpu_of_node;
			tegra_vcm30t124_links[i].codec_name =
					tegra_vcm30t124_links[11].cpu_name;
			tegra_vcm30t124_links[i].platform_of_node =
					tegra_vcm30t124_links[0].cpu_of_node;
		}

		for (i = 14; i < 18; i++) {
			tegra_vcm30t124_links[i].codec_of_node =
				tegra_vcm30t124_links[18].cpu_of_node;
			tegra_vcm30t124_links[i].cpu_name =
				tegra_vcm30t124_links[18].codec_name;
		}

		for (i = 20; i < 24; i++) {
			tegra_vcm30t124_links[i].codec_name =
				tegra_vcm30t124_links[19].cpu_name;
			tegra_vcm30t124_links[i].cpu_of_node =
				tegra_vcm30t124_links[19].codec_of_node;
		}

		machine->gpio_dap_direction = of_get_named_gpio(np,
					"nvidia,dap_direction-gpios", 0);
	} else {
		tegra_vcm30t124_links[0].cpu_name = "tegra30-ahub-apbif";
		tegra_vcm30t124_links[0].cpu_of_node = NULL;

		tegra_vcm30t124_links[10].codec_name = "wm8731.0-001a";
		tegra_vcm30t124_links[10].cpu_name = "tegra30-i2s.0";
		tegra_vcm30t124_links[10].cpu_of_node = NULL;
		tegra_vcm30t124_links[10].codec_of_node = NULL;

		tegra_vcm30t124_links[11].codec_name =
			tegra_vcm30t124_links[10].cpu_name;
		tegra_vcm30t124_links[11].cpu_name = "tegra30-ahub-xbar";
		tegra_vcm30t124_links[11].cpu_of_node = NULL;
		tegra_vcm30t124_links[11].codec_of_node = NULL;

		tegra_vcm30t124_links[12].codec_name = "ad193x.0-0007";
		tegra_vcm30t124_links[12].cpu_name = "tegra30-i2s.4";
		tegra_vcm30t124_links[12].cpu_of_node = NULL;
		tegra_vcm30t124_links[12].codec_of_node = NULL;

		tegra_vcm30t124_links[13].codec_name =
			tegra_vcm30t124_links[12].cpu_name;
		tegra_vcm30t124_links[13].cpu_name = "tegra30-ahub-xbar";
		tegra_vcm30t124_links[13].cpu_of_node = NULL;
		tegra_vcm30t124_links[13].codec_of_node = NULL;

		for (i = 14; i < 18; i++) {
			tegra_vcm30t124_links[i].codec_name =
						"tegra124-amx.0";
			tegra_vcm30t124_links[i].cpu_name =
						"tegra30-ahub-xbar";
		}

		tegra_vcm30t124_links[18].codec_name = "tegra30-ahub-xbar";
		tegra_vcm30t124_links[18].cpu_name = "tegra124-amx.0";
		tegra_vcm30t124_links[18].cpu_of_node = NULL;
		tegra_vcm30t124_links[18].codec_of_node = NULL;

		tegra_vcm30t124_links[19].codec_name = "tegra124-adx.0";
		tegra_vcm30t124_links[19].cpu_name = "tegra30-ahub-xbar";
		tegra_vcm30t124_links[19].cpu_of_node = NULL;
		tegra_vcm30t124_links[19].codec_of_node = NULL;

		for (i = 20; i < 24; i++) {
			tegra_vcm30t124_links[i].codec_name =
						"tegra30-ahub-xbar";
			tegra_vcm30t124_links[i].cpu_name = "tegra124-adx.0";
			tegra_vcm30t124_links[i].cpu_of_node = NULL;
			tegra_vcm30t124_links[i].codec_of_node = NULL;
		}

		for (i = 1; i < 10; i++)
			tegra_vcm30t124_links[i].cpu_name =
				tegra_vcm30t124_links[0].cpu_name;
		for (i = 0; i < 10; i++) {
			tegra_vcm30t124_links[i].codec_name =
				tegra_vcm30t124_links[11].cpu_name;
			tegra_vcm30t124_links[i].platform_name =
				tegra_vcm30t124_links[i].cpu_name;
		}

		machine->gpio_dap_direction = GPIO_PR0;
		card->dapm_routes = tegra_vcm30t124_audio_map;
		card->num_dapm_routes = ARRAY_SIZE(tegra_vcm30t124_audio_map);
	}

	machine->max9485_client = i2c_new_device(i2c_get_adapter(0),
						&max9485_info);
	if (IS_ERR(machine->max9485_client)) {
		dev_err(&pdev->dev, "cannot get i2c device for max9485\n");
		goto err;

	}

	ret = devm_gpio_request(&pdev->dev, machine->gpio_dap_direction,
				"dap_dir_control");
	if (ret) {
		dev_err(&pdev->dev, "cannot get dap_dir_control gpio\n");
		goto err_i2c_unregister;
	}

	ret = tegra_alt_asoc_utils_init(&machine->audio_clock,
					&pdev->dev,
					card);
	if (ret)
		goto err_gpio_free;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err_fini_utils;
	}

	return 0;

err_fini_utils:
	tegra_alt_asoc_utils_fini(&machine->audio_clock);
err_gpio_free:
	devm_gpio_free(&pdev->dev, machine->gpio_dap_direction);
err_i2c_unregister:
	i2c_unregister_device(machine->max9485_client);
err:
	return ret;
}

static int tegra_vcm30t124_driver_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct tegra_vcm30t124 *machine = snd_soc_card_get_drvdata(card);

	snd_soc_unregister_card(card);

	tegra_alt_asoc_utils_fini(&machine->audio_clock);
	devm_gpio_free(&pdev->dev, machine->gpio_dap_direction);
	i2c_unregister_device(machine->max9485_client);

	return 0;
}

static const struct of_device_id tegra_vcm30t124_of_match[] = {
	{ .compatible = "nvidia,tegra-audio-vcm30t124", },
	{},
};

static struct platform_driver tegra_vcm30t124_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tegra_vcm30t124_of_match,
	},
	.probe = tegra_vcm30t124_driver_probe,
	.remove = tegra_vcm30t124_driver_remove,
};
module_platform_driver(tegra_vcm30t124_driver);

MODULE_AUTHOR("Songhee Baek <sbaek@nvidia.com>");
MODULE_DESCRIPTION("Tegra+VCM30T124 machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra_vcm30t124_of_match);
