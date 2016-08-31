/*
 * tegra30_apbif_alt.c - Tegra APBIF driver
 *
 * Copyright (c) 2011-2013 NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <mach/clk.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tegra30_xbar_alt.h"
#include "tegra30_apbif_alt.h"
#include "tegra_pcm_alt.h"

#define DRV_NAME "tegra30-ahub-apbif"

#define FIFOS_IN_FIRST_REG_BLOCK 4

#define LAST_REG(name) \
	(TEGRA_AHUB_##name + \
	 (TEGRA_AHUB_##name##_STRIDE * TEGRA_AHUB_##name##_COUNT) - 4)

#define REG_IN_ARRAY(reg, name) \
	((reg >= TEGRA_AHUB_##name) && \
	 (reg <= LAST_REG(name) && \
	 (!((reg - TEGRA_AHUB_##name) % TEGRA_AHUB_##name##_STRIDE))))

static bool tegra30_apbif_wr_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA_AHUB_CONFIG_LINK_CTRL:
	case TEGRA_AHUB_MISC_CTRL:
	case TEGRA_AHUB_APBDMA_LIVE_STATUS:
	case TEGRA_AHUB_I2S_LIVE_STATUS:
	case TEGRA_AHUB_SPDIF_LIVE_STATUS:
	case TEGRA_AHUB_I2S_INT_MASK:
	case TEGRA_AHUB_DAM_INT_MASK:
	case TEGRA_AHUB_SPDIF_INT_MASK:
	case TEGRA_AHUB_APBIF_INT_MASK:
	case TEGRA_AHUB_I2S_INT_STATUS:
	case TEGRA_AHUB_DAM_INT_STATUS:
	case TEGRA_AHUB_SPDIF_INT_STATUS:
	case TEGRA_AHUB_APBIF_INT_STATUS:
	case TEGRA_AHUB_I2S_INT_SOURCE:
	case TEGRA_AHUB_DAM_INT_SOURCE:
	case TEGRA_AHUB_SPDIF_INT_SOURCE:
	case TEGRA_AHUB_APBIF_INT_SOURCE:
	case TEGRA_AHUB_I2S_INT_SET:
	case TEGRA_AHUB_DAM_INT_SET:
	case TEGRA_AHUB_SPDIF_INT_SET:
	case TEGRA_AHUB_APBIF_INT_SET:
		return true;
	default:
		break;
	};

	if (REG_IN_ARRAY(reg, CHANNEL_CTRL) ||
	    REG_IN_ARRAY(reg, CHANNEL_CLEAR) ||
	    REG_IN_ARRAY(reg, CHANNEL_STATUS) ||
	    REG_IN_ARRAY(reg, CHANNEL_TXFIFO) ||
	    REG_IN_ARRAY(reg, CHANNEL_RXFIFO) ||
	    REG_IN_ARRAY(reg, CIF_TX_CTRL) ||
	    REG_IN_ARRAY(reg, CIF_RX_CTRL) ||
	    REG_IN_ARRAY(reg, DAM_LIVE_STATUS))
		return true;

	return false;
}

static bool tegra30_apbif_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA_AHUB_CONFIG_LINK_CTRL:
	case TEGRA_AHUB_MISC_CTRL:
	case TEGRA_AHUB_APBDMA_LIVE_STATUS:
	case TEGRA_AHUB_I2S_LIVE_STATUS:
	case TEGRA_AHUB_SPDIF_LIVE_STATUS:
	case TEGRA_AHUB_I2S_INT_STATUS:
	case TEGRA_AHUB_DAM_INT_STATUS:
	case TEGRA_AHUB_SPDIF_INT_STATUS:
	case TEGRA_AHUB_APBIF_INT_STATUS:
	case TEGRA_AHUB_I2S_INT_SET:
	case TEGRA_AHUB_DAM_INT_SET:
	case TEGRA_AHUB_SPDIF_INT_SET:
	case TEGRA_AHUB_APBIF_INT_SET:
		return true;
	default:
		break;
	};

	if (REG_IN_ARRAY(reg, CHANNEL_CLEAR) ||
	    REG_IN_ARRAY(reg, CHANNEL_STATUS) ||
	    REG_IN_ARRAY(reg, CHANNEL_TXFIFO) ||
	    REG_IN_ARRAY(reg, CHANNEL_RXFIFO) ||
	    REG_IN_ARRAY(reg, DAM_LIVE_STATUS))
		return true;

	return false;
}

static bool tegra30_apbif_precious_reg(struct device *dev, unsigned int reg)
{
	if (REG_IN_ARRAY(reg, CHANNEL_TXFIFO) ||
	    REG_IN_ARRAY(reg, CHANNEL_RXFIFO))
		return true;

	return false;
}

static const struct regmap_config tegra30_apbif_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = TEGRA_AHUB_APBIF_INT_SET,
	.writeable_reg = tegra30_apbif_wr_rd_reg,
	.readable_reg = tegra30_apbif_wr_rd_reg,
	.volatile_reg = tegra30_apbif_volatile_reg,
	.precious_reg = tegra30_apbif_precious_reg,
	.cache_type = REGCACHE_RBTREE,
};

static const struct regmap_config tegra30_apbif2_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = TEGRA_AHUB_CIF_RX9_CTRL,
	.cache_type = REGCACHE_RBTREE,
};

static int tegra30_apbif_runtime_suspend(struct device *dev)
{
	struct tegra30_apbif *apbif = dev_get_drvdata(dev);

	regcache_cache_only(apbif->regmap[0], true);
	if (apbif->regmap[1])
		regcache_cache_only(apbif->regmap[1], true);

	clk_disable(apbif->clk);

	return 0;
}

static int tegra30_apbif_runtime_resume(struct device *dev)
{
	struct tegra30_apbif *apbif = dev_get_drvdata(dev);
	int ret;

	ret = clk_enable(apbif->clk);
	if (ret) {
		dev_err(dev, "clk_enable failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(apbif->regmap[0], false);
	if (apbif->regmap[1])
		regcache_cache_only(apbif->regmap[1], false);

	return 0;
}

static int tegra30_apbif_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra30_apbif *apbif = snd_soc_dai_get_drvdata(dai);
	u32 reg, mask, val, base_ch;
	struct tegra30_xbar_cif_conf cif_conf;
	struct regmap *regmap;

	cif_conf.audio_channels = params_channels(params);
	cif_conf.client_channels = params_channels(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		cif_conf.audio_bits = TEGRA30_AUDIOCIF_BITS_16;
		cif_conf.client_bits = TEGRA30_AUDIOCIF_BITS_16;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		cif_conf.audio_bits = TEGRA30_AUDIOCIF_BITS_32;
		cif_conf.client_bits = TEGRA30_AUDIOCIF_BITS_32;
		break;
	default:
		dev_err(dev, "Wrong format!\n");
		return -EINVAL;
	}

	if (dai->id < FIFOS_IN_FIRST_REG_BLOCK) {
		base_ch = 0;
		regmap = apbif->regmap[0];
	} else {
		base_ch = FIFOS_IN_FIRST_REG_BLOCK;
		regmap = apbif->regmap[1];
	}

	reg = TEGRA_AHUB_CHANNEL_CTRL +
		((dai->id - base_ch) * TEGRA_AHUB_CHANNEL_CTRL_STRIDE);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mask = TEGRA_AHUB_CHANNEL_CTRL_TX_THRESHOLD_MASK |
		       TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_EN |
		       TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_MASK;
		val = (7 << TEGRA_AHUB_CHANNEL_CTRL_TX_THRESHOLD_SHIFT) |
		      TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_EN |
		      TEGRA_AHUB_CHANNEL_CTRL_TX_PACK_16;
		regmap_update_bits(regmap, reg, mask, val);
	} else {
		mask = TEGRA_AHUB_CHANNEL_CTRL_RX_THRESHOLD_MASK |
		       TEGRA_AHUB_CHANNEL_CTRL_RX_PACK_EN |
		       TEGRA_AHUB_CHANNEL_CTRL_RX_PACK_MASK;
		val = (7 << TEGRA_AHUB_CHANNEL_CTRL_RX_THRESHOLD_SHIFT) |
		      TEGRA_AHUB_CHANNEL_CTRL_RX_PACK_EN |
		      TEGRA_AHUB_CHANNEL_CTRL_RX_PACK_16;
		regmap_update_bits(regmap, reg, mask, val);
	}

	cif_conf.threshold = 0;
	cif_conf.expand = 0;
	cif_conf.stereo_conv = 0;
	cif_conf.replicate = 0;
	cif_conf.truncate = 0;
	cif_conf.mono_conv = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		cif_conf.direction = TEGRA30_AUDIOCIF_DIRECTION_TX;
		reg = TEGRA_AHUB_CIF_TX_CTRL +
		      ((dai->id - base_ch) * TEGRA_AHUB_CIF_TX_CTRL_STRIDE);

	} else {
		cif_conf.direction = TEGRA30_AUDIOCIF_DIRECTION_RX;
		reg = TEGRA_AHUB_CIF_RX_CTRL +
		      ((dai->id - base_ch) * TEGRA_AHUB_CIF_RX_CTRL_STRIDE);
	}
	apbif->soc_data->set_audio_cif(regmap, reg, &cif_conf);

	return 0;
}

static void tegra30_apbif_start_playback(struct snd_soc_dai *dai)
{
	struct tegra30_apbif *apbif = snd_soc_dai_get_drvdata(dai);
	unsigned int reg, base_ch;
	struct regmap *regmap;

	if (dai->id < FIFOS_IN_FIRST_REG_BLOCK) {
		base_ch = 0;
		regmap = apbif->regmap[0];
	} else {
		base_ch = FIFOS_IN_FIRST_REG_BLOCK;
		regmap = apbif->regmap[1];
	}

	reg = TEGRA_AHUB_CHANNEL_CTRL +
		((dai->id - base_ch) * TEGRA_AHUB_CHANNEL_CTRL_STRIDE);
	regmap_update_bits(regmap, reg, TEGRA_AHUB_CHANNEL_CTRL_TX_EN,
					TEGRA_AHUB_CHANNEL_CTRL_TX_EN);
}

static void tegra30_apbif_stop_playback(struct snd_soc_dai *dai)
{
	struct tegra30_apbif *apbif = snd_soc_dai_get_drvdata(dai);
	unsigned int reg, base_ch;
	struct regmap *regmap;

	if (dai->id < FIFOS_IN_FIRST_REG_BLOCK) {
		base_ch = 0;
		regmap = apbif->regmap[0];
	} else {
		base_ch = FIFOS_IN_FIRST_REG_BLOCK;
		regmap = apbif->regmap[1];
	}

	reg = TEGRA_AHUB_CHANNEL_CTRL +
		((dai->id - base_ch) * TEGRA_AHUB_CHANNEL_CTRL_STRIDE);
	regmap_update_bits(regmap, reg, TEGRA_AHUB_CHANNEL_CTRL_TX_EN, 0);
}

static void tegra30_apbif_start_capture(struct snd_soc_dai *dai)
{
	struct tegra30_apbif *apbif = snd_soc_dai_get_drvdata(dai);
	unsigned int reg, base_ch;
	struct regmap *regmap;

	if (dai->id < FIFOS_IN_FIRST_REG_BLOCK) {
		base_ch = 0;
		regmap = apbif->regmap[0];
	} else {
		base_ch = FIFOS_IN_FIRST_REG_BLOCK;
		regmap = apbif->regmap[1];
	}

	reg = TEGRA_AHUB_CHANNEL_CTRL +
		((dai->id - base_ch) * TEGRA_AHUB_CHANNEL_CTRL_STRIDE);
	regmap_update_bits(regmap, reg, TEGRA_AHUB_CHANNEL_CTRL_RX_EN,
				   TEGRA_AHUB_CHANNEL_CTRL_RX_EN);
}

static void tegra30_apbif_stop_capture(struct snd_soc_dai *dai)
{
	struct tegra30_apbif *apbif = snd_soc_dai_get_drvdata(dai);
	unsigned int reg, base_ch;
	struct regmap *regmap;

	if (dai->id < FIFOS_IN_FIRST_REG_BLOCK) {
		base_ch = 0;
		regmap = apbif->regmap[0];
	} else {
		base_ch = FIFOS_IN_FIRST_REG_BLOCK;
		regmap = apbif->regmap[1];
	}

	reg = TEGRA_AHUB_CHANNEL_CTRL +
		((dai->id - base_ch) * TEGRA_AHUB_CHANNEL_CTRL_STRIDE);
	regmap_update_bits(regmap, reg, TEGRA_AHUB_CHANNEL_CTRL_RX_EN, 0);
}

static int tegra30_apbif_trigger(struct snd_pcm_substream *substream, int cmd,
				 struct snd_soc_dai *dai)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			tegra30_apbif_start_playback(dai);
		else
			tegra30_apbif_start_capture(dai);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			tegra30_apbif_stop_playback(dai);
		else
			tegra30_apbif_stop_capture(dai);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct snd_soc_dai_ops tegra30_apbif_dai_ops = {
	.hw_params	= tegra30_apbif_hw_params,
	.trigger	= tegra30_apbif_trigger,
};

static int tegra30_apbif_dai_probe(struct snd_soc_dai *dai)
{
	struct tegra30_apbif *apbif = snd_soc_dai_get_drvdata(dai);

	dai->capture_dma_data = &apbif->capture_dma_data[dai->id];
	dai->playback_dma_data = &apbif->playback_dma_data[dai->id];

	return 0;
}

#define APBIF_DAI(id)							\
	{							\
		.name = "APBIF" #id,				\
		.probe = tegra30_apbif_dai_probe,		\
		.playback = {					\
			.stream_name = "Playback " #id,		\
			.channels_min = 2,			\
			.channels_max = 2,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S16_LE,	\
		},						\
		.capture = {					\
			.stream_name = "Capture " #id,		\
			.channels_min = 2,			\
			.channels_max = 2,			\
			.rates = SNDRV_PCM_RATE_8000_96000,	\
			.formats = SNDRV_PCM_FMTBIT_S16_LE,	\
		},						\
		.ops = &tegra30_apbif_dai_ops,			\
	}

static struct snd_soc_dai_driver tegra30_apbif_dais[10] = {
	APBIF_DAI(0),
	APBIF_DAI(1),
	APBIF_DAI(2),
	APBIF_DAI(3),
	APBIF_DAI(4),
	APBIF_DAI(5),
	APBIF_DAI(6),
	APBIF_DAI(7),
	APBIF_DAI(8),
	APBIF_DAI(9),
};

static const struct snd_soc_component_driver tegra30_apbif_dai_driver = {
	.name		= DRV_NAME,
};

#define CLK_LIST_MASK_TEGRA30 BIT(0)
#define CLK_LIST_MASK_TEGRA114 BIT(1)
#define CLK_LIST_MASK_TEGRA124 BIT(2)

#define CLK_LIST_MASK_TEGRA30_OR_LATER \
		(CLK_LIST_MASK_TEGRA30 | CLK_LIST_MASK_TEGRA114 |\
		CLK_LIST_MASK_TEGRA124)
#define CLK_LIST_MASK_TEGRA114_OR_LATER \
		(CLK_LIST_MASK_TEGRA114 | CLK_LIST_MASK_TEGRA124)

static const struct {
	const char *clk_name;
	unsigned int clk_list_mask;
} configlink_clocks[] = {
	{ "i2s0", CLK_LIST_MASK_TEGRA30_OR_LATER },
	{ "i2s1", CLK_LIST_MASK_TEGRA30_OR_LATER },
	{ "i2s2", CLK_LIST_MASK_TEGRA30_OR_LATER },
	{ "i2s3", CLK_LIST_MASK_TEGRA30_OR_LATER },
	{ "i2s4", CLK_LIST_MASK_TEGRA30_OR_LATER },
	{ "dam0", CLK_LIST_MASK_TEGRA30_OR_LATER },
	{ "dam1", CLK_LIST_MASK_TEGRA30_OR_LATER },
	{ "dam2", CLK_LIST_MASK_TEGRA30_OR_LATER },
	{ "spdif_in", CLK_LIST_MASK_TEGRA30_OR_LATER },
	{ "amx", CLK_LIST_MASK_TEGRA114_OR_LATER },
	{ "adx", CLK_LIST_MASK_TEGRA114_OR_LATER },
	{ "amx1", CLK_LIST_MASK_TEGRA124 },
	{ "adx1", CLK_LIST_MASK_TEGRA124 },
	{ "afc0", CLK_LIST_MASK_TEGRA124 },
	{ "afc1", CLK_LIST_MASK_TEGRA124 },
	{ "afc2", CLK_LIST_MASK_TEGRA124 },
	{ "afc3", CLK_LIST_MASK_TEGRA124 },
	{ "afc4", CLK_LIST_MASK_TEGRA124 },
	{ "afc5", CLK_LIST_MASK_TEGRA124 },
};


struct of_dev_auxdata tegra30_apbif_auxdata[] = {
	OF_DEV_AUXDATA("nvidia,tegra30-i2s", 0x70080300, "tegra30-i2s.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra30-i2s", 0x70080400, "tegra30-i2s.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra30-i2s", 0x70080500, "tegra30-i2s.2", NULL),
	OF_DEV_AUXDATA("nvidia,tegra30-i2s", 0x70080600, "tegra30-i2s.3", NULL),
	OF_DEV_AUXDATA("nvidia,tegra30-i2s", 0x70080700, "tegra30-i2s.4", NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-amx", 0x70080c00, "tegra114-amx.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra114-adx", 0x70080e00, "tegra114-adx.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-i2s", 0x70301000, "tegra30-i2s.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-i2s", 0x70301100, "tegra30-i2s.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-i2s", 0x70301200, "tegra30-i2s.2", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-i2s", 0x70301300, "tegra30-i2s.3", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-i2s", 0x70301400, "tegra30-i2s.4", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-spdif", 0x70306000, "tegra30-spdif", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-amx", 0x70303000, "tegra124-amx.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-amx", 0x70303100, "tegra124-amx.1", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-adx", 0x70303800, "tegra124-adx.0", NULL),
	OF_DEV_AUXDATA("nvidia,tegra124-adx", 0x70303900, "tegra124-adx.1", NULL),
	{}
};

static struct tegra30_apbif_soc_data soc_data_tegra30 = {
	.num_ch = FIFOS_IN_FIRST_REG_BLOCK,
	.clk_list_mask = CLK_LIST_MASK_TEGRA30,
	.set_audio_cif = tegra30_xbar_set_cif,
};

static struct tegra30_apbif_soc_data soc_data_tegra114 = {
	.num_ch = FIFOS_IN_FIRST_REG_BLOCK + 6,
	.clk_list_mask = CLK_LIST_MASK_TEGRA114,
	.set_audio_cif = tegra30_xbar_set_cif,
};

static struct tegra30_apbif_soc_data soc_data_tegra124 = {
	.num_ch = FIFOS_IN_FIRST_REG_BLOCK + 6,
	.clk_list_mask = CLK_LIST_MASK_TEGRA124,
	.set_audio_cif = tegra124_xbar_set_cif,
};

static const struct of_device_id tegra30_apbif_of_match[] = {
	{ .compatible = "nvidia,tegra30-ahub", .data = &soc_data_tegra30 },
	{ .compatible = "nvidia,tegra114-ahub", .data = &soc_data_tegra114 },
	{ .compatible = "nvidia,tegra124-ahub", .data = &soc_data_tegra124 },
	{},
};

static struct platform_device_info tegra30_xbar_device_info = {
	.name = "tegra30-ahub-xbar",
	.id = -1,
};

static int tegra30_apbif_probe(struct platform_device *pdev)
{
	int i;
	struct clk *clk;
	int ret;
	struct tegra30_apbif *apbif;
	void __iomem *regs;
	struct resource *res[2];
	u32 of_dma[10][2];
	const struct of_device_id *match;
	struct tegra30_apbif_soc_data *soc_data;

	match = of_match_device(tegra30_apbif_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}
	soc_data = (struct tegra30_apbif_soc_data *)match->data;

	/*
	 * The TEGRA_AHUB APBIF hosts a register bus: the "configlink".
	 * For this to operate correctly, all devices on this bus must
	 * be out of reset.
	 * Ensure that here.
	 */
	for (i = 0; i < ARRAY_SIZE(configlink_clocks); i++) {
		if (!(configlink_clocks[i].clk_list_mask &
					soc_data->clk_list_mask))
			continue;
		clk = devm_clk_get(&pdev->dev, configlink_clocks[i].clk_name);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Can't get clock %s\n",
				configlink_clocks[i].clk_name);
			ret = PTR_ERR(clk);
			goto err;
		}
		tegra_periph_reset_deassert(clk);
		devm_clk_put(&pdev->dev, clk);
	}

	apbif = devm_kzalloc(&pdev->dev, sizeof(*apbif), GFP_KERNEL);
	if (!apbif) {
		dev_err(&pdev->dev, "Can't allocate tegra30_apbif\n");
		ret = -ENOMEM;
		goto err;
	}

	dev_set_drvdata(&pdev->dev, apbif);

	apbif->soc_data = soc_data;

	apbif->capture_dma_data = devm_kzalloc(&pdev->dev,
			sizeof(struct tegra_alt_pcm_dma_params) *
				apbif->soc_data->num_ch,
			GFP_KERNEL);
	if (!apbif->capture_dma_data) {
		dev_err(&pdev->dev, "Can't allocate tegra_alt_pcm_dma_params\n");
		ret = -ENOMEM;
		goto err;
	}

	apbif->playback_dma_data = devm_kzalloc(&pdev->dev,
			sizeof(struct tegra_alt_pcm_dma_params) *
				apbif->soc_data->num_ch,
			GFP_KERNEL);
	if (!apbif->playback_dma_data) {
		dev_err(&pdev->dev, "Can't allocate tegra_alt_pcm_dma_params\n");
		ret = -ENOMEM;
		goto err;
	}

	apbif->clk = devm_clk_get(&pdev->dev, "apbif");
	if (IS_ERR(apbif->clk)) {
		dev_err(&pdev->dev, "Can't retrieve clock\n");
		ret = PTR_ERR(apbif->clk);
		goto err;
	}

	res[0] = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res[0]) {
		dev_err(&pdev->dev, "No memory resource for apbif\n");
		ret = -ENODEV;
		goto err_clk_put;
	}
	res[1] = NULL;

	regs = devm_request_and_ioremap(&pdev->dev, res[0]);
	if (!regs) {
		dev_err(&pdev->dev, "request/iomap region failed\n");
		ret = -ENODEV;
		goto err_clk_put;
	}

	apbif->regmap[0] = devm_regmap_init_mmio(&pdev->dev, regs,
					&tegra30_apbif_regmap_config);
	if (IS_ERR(apbif->regmap[0])) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(apbif->regmap[0]);
		goto err_clk_put;
	}
	regcache_cache_only(apbif->regmap[0], true);

	if (apbif->soc_data->num_ch > FIFOS_IN_FIRST_REG_BLOCK) {
		res[1] = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (!res[1]) {
			dev_info(&pdev->dev, "No memory resource for apbif2\n");
			ret = -ENODEV;
			goto err_clk_put;
		}

		regs = devm_request_and_ioremap(&pdev->dev, res[1]);
		if (!regs) {
			dev_err(&pdev->dev, "request/iomap region failed\n");
			ret = -ENODEV;
			goto err_clk_put;
		}

		apbif->regmap[1] = devm_regmap_init_mmio(&pdev->dev, regs,
						&tegra30_apbif2_regmap_config);
		if (IS_ERR(apbif->regmap[1])) {
			dev_err(&pdev->dev, "regmap init failed\n");
			ret = PTR_ERR(apbif->regmap[1]);
			goto err_clk_put;
		}
		regcache_cache_only(apbif->regmap[1], true);
	}

	if (of_property_read_u32_array(pdev->dev.of_node,
				"nvidia,dma-request-selector",
				&of_dma[0][0],
				apbif->soc_data->num_ch * 2) < 0) {
		dev_err(&pdev->dev,
			"Missing property nvidia,dma-request-selector\n");
		ret = -ENODEV;
		goto err_clk_put;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra30_apbif_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	/* default DAI number is 4 */
	for (i = 0; i < apbif->soc_data->num_ch; i++) {
		if (i < FIFOS_IN_FIRST_REG_BLOCK) {
			apbif->playback_dma_data[i].addr = res[0]->start +
					TEGRA_AHUB_CHANNEL_TXFIFO +
					(i * TEGRA_AHUB_CHANNEL_TXFIFO_STRIDE);

			apbif->capture_dma_data[i].addr = res[0]->start +
					TEGRA_AHUB_CHANNEL_RXFIFO +
					(i * TEGRA_AHUB_CHANNEL_RXFIFO_STRIDE);
		} else {
			apbif->playback_dma_data[i].addr = res[1]->start +
					TEGRA_AHUB_CHANNEL_TXFIFO +
					((i - FIFOS_IN_FIRST_REG_BLOCK) *
					TEGRA_AHUB_CHANNEL_TXFIFO_STRIDE);

			apbif->capture_dma_data[i].addr = res[1]->start +
					TEGRA_AHUB_CHANNEL_RXFIFO +
					((i - FIFOS_IN_FIRST_REG_BLOCK) *
					TEGRA_AHUB_CHANNEL_RXFIFO_STRIDE);
		}

		apbif->playback_dma_data[i].wrap = 4;
		apbif->playback_dma_data[i].width = 32;
		apbif->playback_dma_data[i].req_sel = of_dma[i][1];

		apbif->capture_dma_data[i].wrap = 4;
		apbif->capture_dma_data[i].width = 32;
		apbif->capture_dma_data[i].req_sel = of_dma[i][1];
	}


	ret = snd_soc_register_component(&pdev->dev,
					&tegra30_apbif_dai_driver,
					tegra30_apbif_dais,
					apbif->soc_data->num_ch);
	if (ret) {
		dev_err(&pdev->dev, "Could not register DAIs %d: %d\n",
			i, ret);
		ret = -ENOMEM;
		goto err_suspend;
	}

	ret = tegra_alt_pcm_platform_register(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Could not register PCM: %d\n", ret);
		goto err_unregister_dais;
	}

	tegra30_xbar_device_info.res = platform_get_resource(pdev,
						IORESOURCE_MEM, 1);
	if (!tegra30_xbar_device_info.res) {
		dev_err(&pdev->dev, "No memory resource for xbar\n");
		goto err_unregister_platform;
	}
	tegra30_xbar_device_info.num_res = 1;
	tegra30_xbar_device_info.parent = &pdev->dev;
	platform_device_register_full(&tegra30_xbar_device_info);

	of_platform_populate(pdev->dev.of_node, NULL, tegra30_apbif_auxdata,
			     &pdev->dev);

	return 0;

err_unregister_platform:
	tegra_alt_pcm_platform_unregister(&pdev->dev);
err_unregister_dais:
	snd_soc_unregister_component(&pdev->dev);
err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra30_apbif_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_clk_put:
	devm_clk_put(&pdev->dev, apbif->clk);
err:
	return ret;
}

static int tegra30_apbif_remove(struct platform_device *pdev)
{
	struct tegra30_apbif *apbif = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);

	tegra_alt_pcm_platform_unregister(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra30_apbif_runtime_suspend(&pdev->dev);

	devm_clk_put(&pdev->dev, apbif->clk);

	return 0;
}

static const struct dev_pm_ops tegra30_apbif_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra30_apbif_runtime_suspend,
			   tegra30_apbif_runtime_resume, NULL)
};

static struct platform_driver tegra30_apbif_driver = {
	.probe = tegra30_apbif_probe,
	.remove = tegra30_apbif_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra30_apbif_of_match,
		.pm = &tegra30_apbif_pm_ops,
	},
};
module_platform_driver(tegra30_apbif_driver);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra30 APBIF driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
