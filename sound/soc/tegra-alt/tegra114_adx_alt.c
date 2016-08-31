/*
 * tegra114_adx_alt.c - Tegra114 ADX driver
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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_device.h>

#include "tegra30_xbar_alt.h"
#include "tegra114_adx_alt.h"

#define DRV_NAME "tegra114-adx"

/**
 * tegra114_adx_enable_outstream - enable output stream
 * @adx: struct of tegra114_adx
 * @stream_id: adx output stream id for enabling
 */
static void tegra114_adx_enable_outstream(struct tegra114_adx *adx,
					unsigned int stream_id)
{
	int reg;

	reg = TEGRA_ADX_OUT_CH_CTRL;

	regmap_update_bits(adx->regmap, reg,
			TEGRA_ADX_OUT_CH_ENABLE << stream_id,
			TEGRA_ADX_OUT_CH_ENABLE << stream_id);
}

/**
 * tegra114_adx_disable_outstream - disable output stream
 * @adx: struct of tegra114_adx
 * @stream_id: adx output stream id for disabling
 */
static void tegra114_adx_disable_outstream(struct tegra114_adx *adx,
					unsigned int stream_id)
{
	int reg;

	reg = TEGRA_ADX_OUT_CH_CTRL;

	regmap_update_bits(adx->regmap, reg,
			TEGRA_ADX_OUT_CH_ENABLE << stream_id,
			TEGRA_ADX_OUT_CH_DISABLE << stream_id);
}

/**
 * tegra114_adx_set_in_byte_mask - set byte mask for input frame
 * @adx: struct of tegra114_adx
 * @mask1: enable for bytes 31 ~ 0 of input frame
 * @mask2: enable for bytes 63 ~ 32 of input frame
 */
static void tegra114_adx_set_in_byte_mask(struct tegra114_adx *adx,
					unsigned int mask1,
					unsigned int mask2)
{
	regmap_write(adx->regmap, TEGRA_ADX_IN_BYTE_EN0, mask1);
	regmap_write(adx->regmap, TEGRA_ADX_IN_BYTE_EN1, mask2);
}

/**
 * tegra114_adx_set_map_table - set map table not RAM
 * @adx: struct of tegra114_adx
 * @out_byte_addr: byte address in one frame
 * @stream_id: input stream id
 * @nth_word: n-th word in the input stream
 * @nth_byte: n-th byte in the word
 */
static void tegra114_adx_set_map_table(struct tegra114_adx *adx,
			unsigned int out_byte_addr,
			unsigned int stream_id,
			unsigned int nth_word,
			unsigned int nth_byte)
{
	unsigned char *bytes_map = (unsigned char *)&adx->map;

	bytes_map[out_byte_addr] = (stream_id <<
				TEGRA_ADX_MAP_STREAM_NUMBER_SHIFT) |
				(nth_word <<
				TEGRA_ADX_MAP_WORD_NUMBER_SHIFT) |
				(nth_byte <<
				TEGRA_ADX_MAP_BYTE_NUMBER_SHIFT);
}

/**
 * tegra114_adx_write_map_ram - write map information in RAM
 * @adx: struct of tegra114_adx
 * @addr: n-th word of input stream
 * @val : bytes mapping information of the word
 */
static void tegra114_adx_write_map_ram(struct tegra114_adx *adx,
				unsigned int addr,
				unsigned int val)
{
	unsigned int reg;

	regmap_write(adx->regmap, TEGRA_ADX_AUDIORAMCTL_ADX_CTRL,
			 (addr <<
			TEGRA_ADX_AUDIORAMCTL_ADX_CTRL_RAM_ADR_SHIFT));

	regmap_write(adx->regmap, TEGRA_ADX_AUDIORAMCTL_ADX_DATA, val);

	regmap_read(adx->regmap, TEGRA_ADX_AUDIORAMCTL_ADX_CTRL, &reg);
	reg |= TEGRA_ADX_AUDIORAMCTL_ADX_CTRL_HW_ADR_EN_ENABLE;

	regmap_write(adx->regmap, TEGRA_ADX_AUDIORAMCTL_ADX_CTRL, reg);

	regmap_read(adx->regmap, TEGRA_ADX_AUDIORAMCTL_ADX_CTRL, &reg);
	reg |= TEGRA_ADX_AUDIORAMCTL_ADX_CTRL_RW_WRITE;

	regmap_write(adx->regmap, TEGRA_ADX_AUDIORAMCTL_ADX_CTRL, reg);
}

static void tegra114_adx_update_map_ram(struct tegra114_adx *adx)
{
	int i;

	for (i = 0; i < TEGRA_ADX_RAM_DEPTH; i++)
		tegra114_adx_write_map_ram(adx, i, adx->map[i]);
}

static int tegra114_adx_runtime_suspend(struct device *dev)
{
	struct tegra114_adx *adx = dev_get_drvdata(dev);

	regcache_cache_only(adx->regmap, true);

	clk_disable_unprepare(adx->clk_adx);

	return 0;
}

static int tegra114_adx_runtime_resume(struct device *dev)
{
	struct tegra114_adx *adx = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(adx->clk_adx);
	if (ret) {
		dev_err(dev, "clk_enable failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(adx->regmap, false);

	return 0;
}

static int tegra114_adx_set_audio_cif(struct tegra114_adx *adx,
				struct snd_pcm_hw_params *params,
				unsigned int reg)
{
	int channels, audio_bits;
	struct tegra30_xbar_cif_conf cif_conf;

	channels = params_channels(params);
	if (channels < 2)
		return -EINVAL;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		audio_bits = TEGRA30_AUDIOCIF_BITS_16;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		audio_bits = TEGRA30_AUDIOCIF_BITS_32;
		break;
	default:
		return -EINVAL;
	}

	cif_conf.threshold = 0;
	cif_conf.audio_channels = channels;
	cif_conf.client_channels = channels;
	cif_conf.audio_bits = audio_bits;
	cif_conf.client_bits = audio_bits;
	cif_conf.expand = 0;
	cif_conf.stereo_conv = 0;
	cif_conf.replicate = 0;
	cif_conf.direction = 0;
	cif_conf.truncate = 0;
	cif_conf.mono_conv = 0;

	adx->soc_data->set_audio_cif(adx->regmap, reg, &cif_conf);

	return 0;
}

static int tegra114_adx_out_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct tegra114_adx *adx = snd_soc_dai_get_drvdata(dai);
	int ret;

	ret = tegra114_adx_set_audio_cif(adx, params,
			TEGRA_ADX_AUDIOCIF_CH0_CTRL +
			(dai->id * TEGRA_ADX_AUDIOCIF_CH_STRIDE));

	return ret;
}

static int tegra114_adx_out_trigger(struct snd_pcm_substream *substream,
				 int cmd,
				 struct snd_soc_dai *dai)
{
	struct tegra114_adx *adx = snd_soc_dai_get_drvdata(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		tegra114_adx_enable_outstream(adx, dai->id);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		tegra114_adx_disable_outstream(adx, dai->id);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tegra114_adx_in_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct tegra114_adx *adx = snd_soc_dai_get_drvdata(dai);
	int ret;

	ret = tegra114_adx_set_audio_cif(adx, params,
					TEGRA_ADX_AUDIOCIF_IN_CTRL);

	return ret;
}

int tegra114_adx_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
{
	struct device *dev = dai->dev;
	struct tegra114_adx *adx = snd_soc_dai_get_drvdata(dai);
	unsigned int byte_mask1 = 0, byte_mask2 = 0;
	unsigned int out_stream_idx, out_ch_idx, out_byte_idx;
	int i;

	if ((rx_num < 1) || (rx_num > 64)) {
		dev_err(dev, "Doesn't support %d rx_num, need to be 1 to 64\n",
			rx_num);
		return -EINVAL;
	}

	if (!rx_slot) {
		dev_err(dev, "rx_slot is NULL\n");
		return -EINVAL;
	}

	for (i = 0; i < rx_num; i++) {
		if (rx_slot[i] != 0) {
			/* getting mapping information */
			/* n-th output stream : 0 to 3 */
			out_stream_idx = (rx_slot[i] >> 16) & 0x3;
			/* n-th audio channel of output stream : 1 to 16 */
			out_ch_idx = (rx_slot[i] >> 8) & 0x1f;
			/* n-th byte of audio channel : 0 to 3 */
			out_byte_idx = rx_slot[i] & 0x3;
			tegra114_adx_set_map_table(adx, i, out_stream_idx,
					out_ch_idx - 1,
					out_byte_idx);

			/* making byte_mask */
			if (i > 32)
				byte_mask2 |= 1 << (32 - i);
			else
				byte_mask1 |= 1 << i;
		}
	}

	tegra114_adx_update_map_ram(adx);

	tegra114_adx_set_in_byte_mask(adx, byte_mask1, byte_mask2);

	return 0;
}

static int tegra114_adx_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra114_adx *adx = snd_soc_codec_get_drvdata(codec);
	int ret;

	codec->control_data = adx->regmap;
	ret = snd_soc_codec_set_cache_io(codec, 32, 32, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	return 0;
}

static struct snd_soc_dai_ops tegra114_adx_in_dai_ops = {
	.hw_params	= tegra114_adx_in_hw_params,
	.set_channel_map = tegra114_adx_set_channel_map,
};

static struct snd_soc_dai_ops tegra114_adx_out_dai_ops = {
	.hw_params	= tegra114_adx_out_hw_params,
	.trigger	= tegra114_adx_out_trigger,
};

#define OUT_DAI(id)						\
	{							\
		.name = "OUT" #id,				\
		.capture = {					\
			.stream_name = "OUT" #id " Transmit",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S16_LE,	\
		},						\
		.ops = &tegra114_adx_out_dai_ops,		\
	}

#define IN_DAI(sname, dai_ops)					\
	{							\
		.name = #sname,					\
		.playback = {					\
			.stream_name = #sname " Receive",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S16_LE,	\
		},						\
		.ops = dai_ops,					\
	}

static struct snd_soc_dai_driver tegra114_adx_dais[] = {
	OUT_DAI(0),
	OUT_DAI(1),
	OUT_DAI(2),
	OUT_DAI(3),
	IN_DAI(IN, &tegra114_adx_in_dai_ops),
};

static const struct snd_soc_dapm_widget tegra114_adx_widgets[] = {
	SND_SOC_DAPM_AIF_IN("IN", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("OUT0", NULL, 0, TEGRA_ADX_OUT_CH_CTRL, 0, 0),
	SND_SOC_DAPM_AIF_OUT("OUT1", NULL, 0, TEGRA_ADX_OUT_CH_CTRL, 1, 0),
	SND_SOC_DAPM_AIF_OUT("OUT2", NULL, 0, TEGRA_ADX_OUT_CH_CTRL, 2, 0),
	SND_SOC_DAPM_AIF_OUT("OUT3", NULL, 0, TEGRA_ADX_OUT_CH_CTRL, 3, 0),
};

static const struct snd_soc_dapm_route tegra114_adx_routes[] = {
	{ "IN", NULL, "IN Receive" },
	{ "OUT0", NULL, "IN" },
	{ "OUT1", NULL, "IN" },
	{ "OUT2", NULL, "IN" },
	{ "OUT3", NULL, "IN" },
	{ "OUT0 Transmit", NULL, "OUT0" },
	{ "OUT1 Transmit", NULL, "OUT1" },
	{ "OUT2 Transmit", NULL, "OUT2" },
	{ "OUT3 Transmit", NULL, "OUT3" },
};

static struct snd_soc_codec_driver tegra114_adx_codec = {
	.probe = tegra114_adx_codec_probe,
	.dapm_widgets = tegra114_adx_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra114_adx_widgets),
	.dapm_routes = tegra114_adx_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra114_adx_routes),
};

static bool tegra114_adx_wr_rd_reg(struct device *dev,
				unsigned int reg)
{
	switch (reg) {
	case TEGRA_ADX_CTRL:
	case TEGRA_ADX_OUT_CH_CTRL:
	case TEGRA_ADX_IN_BYTE_EN0:
	case TEGRA_ADX_IN_BYTE_EN1:
	case TEGRA_ADX_AUDIORAMCTL_ADX_CTRL:
	case TEGRA_ADX_AUDIORAMCTL_ADX_DATA:
	case TEGRA_ADX_AUDIOCIF_IN_CTRL:
	case TEGRA_ADX_AUDIOCIF_CH0_CTRL:
	case TEGRA_ADX_AUDIOCIF_CH1_CTRL:
	case TEGRA_ADX_AUDIOCIF_CH2_CTRL:
	case TEGRA_ADX_AUDIOCIF_CH3_CTRL:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra114_adx_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA_ADX_AUDIOCIF_CH3_CTRL,
	.writeable_reg = tegra114_adx_wr_rd_reg,
	.readable_reg = tegra114_adx_wr_rd_reg,
	.cache_type = REGCACHE_RBTREE,
};

static const struct tegra114_adx_soc_data soc_data_tegra114 = {
	.set_audio_cif = tegra30_xbar_set_cif
};

static const struct tegra114_adx_soc_data soc_data_tegra124 = {
	.set_audio_cif = tegra124_xbar_set_cif
};

static const struct of_device_id tegra114_adx_of_match[] = {
	{ .compatible = "nvidia,tegra114-adx", .data = &soc_data_tegra114 },
	{ .compatible = "nvidia,tegra124-adx", .data = &soc_data_tegra124 },
	{},
};

static int tegra114_adx_platform_probe(struct platform_device *pdev)
{
	struct tegra114_adx *adx;
	struct resource *mem, *memregion;
	void __iomem *regs;
	int ret = 0;
	const struct of_device_id *match;
	struct tegra114_adx_soc_data *soc_data;

	match = of_match_device(tegra114_adx_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra114_adx_soc_data *)match->data;

	adx = devm_kzalloc(&pdev->dev, sizeof(struct tegra114_adx), GFP_KERNEL);
	if (!adx) {
		dev_err(&pdev->dev, "Can't allocate tegra114_adx\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, adx);

	adx->soc_data = soc_data;

	adx->clk_adx = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(adx->clk_adx)) {
		dev_err(&pdev->dev, "Can't retrieve adx clock\n");
		ret = PTR_ERR(adx->clk_adx);
		goto err;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err_clk_put;
	}

	memregion = devm_request_mem_region(&pdev->dev, mem->start,
					    resource_size(mem), DRV_NAME);
	if (!memregion) {
		dev_err(&pdev->dev, "Memory region already claimed\n");
		ret = -EBUSY;
		goto err_clk_put;
	}

	regs = devm_ioremap(&pdev->dev, mem->start, resource_size(mem));
	if (!regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_clk_put;
	}

	adx->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra114_adx_regmap_config);
	if (IS_ERR(adx->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(adx->regmap);
		goto err_clk_put;
	}
	regcache_cache_only(adx->regmap, true);

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra114_adx_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_codec(&pdev->dev, &tegra114_adx_codec,
				     tegra114_adx_dais,
				     ARRAY_SIZE(tegra114_adx_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra114_adx_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_clk_put:
	devm_clk_put(&pdev->dev, adx->clk_adx);
err:
	return ret;
}

static int tegra114_adx_platform_remove(struct platform_device *pdev)
{
	struct tegra114_adx *adx = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra114_adx_runtime_suspend(&pdev->dev);

	devm_clk_put(&pdev->dev, adx->clk_adx);

	return 0;
}

static const struct dev_pm_ops tegra114_adx_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra114_adx_runtime_suspend,
			   tegra114_adx_runtime_resume, NULL)
};

static struct platform_driver tegra114_adx_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra114_adx_of_match,
		.pm = &tegra114_adx_pm_ops,
	},
	.probe = tegra114_adx_platform_probe,
	.remove = tegra114_adx_platform_remove,
};
module_platform_driver(tegra114_adx_driver);

MODULE_AUTHOR("Arun Shamanna Lakshmi <aruns@nvidia.com>");
MODULE_DESCRIPTION("Tegra114 ADX ASoC driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra114_adx_of_match);
