/*
 * tegra30_spdif_alt.c - Tegra30 SPDIF driver
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
#include "tegra30_spdif_alt.h"

#define DRV_NAME "tegra30-spdif"

static int tegra30_spdif_runtime_suspend(struct device *dev)
{
	struct tegra30_spdif *spdif = dev_get_drvdata(dev);

	regcache_cache_only(spdif->regmap, true);

	clk_disable_unprepare(spdif->clk_spdif_out);
	clk_disable_unprepare(spdif->clk_spdif_in);

	return 0;
}

static int tegra30_spdif_runtime_resume(struct device *dev)
{
	struct tegra30_spdif *spdif = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(spdif->clk_spdif_out);
	if (ret) {
		dev_err(dev, "spdif_out_clk_enable failed: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(spdif->clk_spdif_in);
	if (ret) {
		dev_err(dev, "spdif_in_clk_enable failed: %d\n", ret);
		return ret;
	}

	regcache_cache_only(spdif->regmap, false);

	return 0;
}

static int tegra30_spdif_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct device *dev = dai->dev;
	struct tegra30_spdif *spdif = snd_soc_dai_get_drvdata(dai);
	int spdif_out_clock_rate, spdif_in_clock_rate;
	int ret;

	switch (freq) {
	case 32000:
		spdif_out_clock_rate = 4096000;
		spdif_in_clock_rate = 48000000;
		break;
	case 44100:
		spdif_out_clock_rate = 5644800;
		spdif_in_clock_rate = 48000000;
		break;
	case 48000:
		spdif_out_clock_rate = 6144000;
		spdif_in_clock_rate = 48000000;
		break;
	case 88200:
		spdif_out_clock_rate = 11289600;
		spdif_in_clock_rate = 72000000;
		break;
	case 96000:
		spdif_out_clock_rate = 12288000;
		spdif_in_clock_rate = 72000000;
		break;
	case 176400:
		spdif_out_clock_rate = 22579200;
		spdif_in_clock_rate = 108000000;
		break;
	case 192000:
		spdif_out_clock_rate = 24576000;
		spdif_in_clock_rate = 108000000;
		break;
	default:
		return -EINVAL;
	}

	if (dir == SND_SOC_CLOCK_OUT) {
		ret = clk_set_rate(spdif->clk_spdif_out, spdif_out_clock_rate);
		if (ret) {
			dev_err(dev, "Can't set SPDIF Out clock rate: %d\n",
				ret);
			return ret;
		}
	} else {
		ret = clk_set_rate(spdif->clk_spdif_in, spdif_in_clock_rate);
		if (ret) {
			dev_err(dev, "Can't set SPDIF In clock rate: %d\n",
				ret);
			return ret;
		}
	}

	return 0;
}


static int tegra30_spdif_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra30_spdif *spdif = snd_soc_dai_get_drvdata(dai);
	int channels, audio_bits, bit_mode;
	struct tegra30_xbar_cif_conf cif_conf;

	channels = params_channels(params);

	if (channels < 2) {
		dev_err(dev, "Doesn't support %d channels\n", channels);
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		audio_bits = TEGRA30_AUDIOCIF_BITS_16;
		bit_mode = TEGRA30_SPDIF_BIT_MODE16;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		audio_bits = TEGRA30_AUDIOCIF_BITS_32;
		bit_mode = TEGRA30_SPDIF_BIT_MODERAW;
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

	regmap_update_bits(spdif->regmap, TEGRA30_SPDIF_CTRL,
				TEGRA30_SPDIF_CTRL_BIT_MODE_MASK,
				bit_mode);

	/* As a CODEC DAI, CAPTURE is transmit */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		cif_conf.direction = TEGRA30_AUDIOCIF_DIRECTION_RX;
		spdif->soc_data->set_audio_cif(spdif->regmap,
					TEGRA30_SPDIF_CIF_TXD_CTRL,
					&cif_conf);
	} else {
		cif_conf.direction = TEGRA30_AUDIOCIF_DIRECTION_TX;
		spdif->soc_data->set_audio_cif(spdif->regmap,
					TEGRA30_SPDIF_CIF_RXD_CTRL,
					&cif_conf);
	}

	return 0;
}

static int tegra30_spdif_codec_probe(struct snd_soc_codec *codec)
{
	struct tegra30_spdif *spdif = snd_soc_codec_get_drvdata(codec);
	int ret;

	codec->control_data = spdif->regmap;
	ret = snd_soc_codec_set_cache_io(codec, 32, 32, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	return 0;
}

static struct snd_soc_dai_ops tegra30_spdif_dai_ops = {
	.hw_params	= tegra30_spdif_hw_params,
	.set_sysclk	= tegra30_spdif_set_dai_sysclk,
};

static struct snd_soc_dai_driver tegra30_spdif_dais[] = {
	{
		.name = "CIF",
		.playback = {
			.stream_name = "CIF Receive",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "CIF Transmit",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "DAP",
		.playback = {
			.stream_name = "DAP Receive",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "DAP Transmit",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &tegra30_spdif_dai_ops,
	}
};

static const struct snd_soc_dapm_widget tegra30_spdif_widgets[] = {
	SND_SOC_DAPM_AIF_IN("CIF RX", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("CIF TX", NULL, 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("DAP RX", NULL, 0, TEGRA30_SPDIF_CTRL, 29, 0),
	SND_SOC_DAPM_AIF_OUT("DAP TX", NULL, 0, TEGRA30_SPDIF_CTRL, 28, 0),
};

static const struct snd_soc_dapm_route tegra30_spdif_routes[] = {
	{ "CIF RX",       NULL, "CIF Receive"},
	{ "DAP TX",       NULL, "CIF RX"},
	{ "DAP Transmit", NULL, "DAP TX"},

	{ "DAP RX",       NULL, "DAP Receive"},
	{ "CIF TX",       NULL, "DAP RX"},
	{ "CIF Transmit", NULL, "CIF TX"},
};

static struct snd_soc_codec_driver tegra30_spdif_codec = {
	.probe = tegra30_spdif_codec_probe,
	.dapm_widgets = tegra30_spdif_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tegra30_spdif_widgets),
	.dapm_routes = tegra30_spdif_routes,
	.num_dapm_routes = ARRAY_SIZE(tegra30_spdif_routes),
};

static bool tegra30_spdif_wr_rd_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TEGRA30_SPDIF_CTRL:
	case TEGRA30_SPDIF_STROBE_CTRL:
	case TEGRA30_SPDIF_CIF_TXD_CTRL:
	case TEGRA30_SPDIF_CIF_RXD_CTRL:
	case TEGRA30_SPDIF_CIF_TXU_CTRL:
	case TEGRA30_SPDIF_CIF_RXU_CTRL:
	case TEGRA30_SPDIF_CH_STA_RX_A:
	case TEGRA30_SPDIF_CH_STA_RX_B:
	case TEGRA30_SPDIF_CH_STA_RX_C:
	case TEGRA30_SPDIF_CH_STA_RX_D:
	case TEGRA30_SPDIF_CH_STA_RX_E:
	case TEGRA30_SPDIF_CH_STA_RX_F:
	case TEGRA30_SPDIF_CH_STA_TX_A:
	case TEGRA30_SPDIF_CH_STA_TX_B:
	case TEGRA30_SPDIF_CH_STA_TX_C:
	case TEGRA30_SPDIF_CH_STA_TX_D:
	case TEGRA30_SPDIF_CH_STA_TX_E:
	case TEGRA30_SPDIF_CH_STA_TX_F:
	case TEGRA30_SPDIF_FLOWCTL_CTRL:
	case TEGRA30_SPDIF_TX_STEP:
	case TEGRA30_SPDIF_FLOW_STATUS:
	case TEGRA30_SPDIF_FLOW_TOTAL:
	case TEGRA30_SPDIF_FLOW_OVER:
	case TEGRA30_SPDIF_FLOW_UNDER:
	case TEGRA30_SPDIF_LCOEF_1_4_0:
	case TEGRA30_SPDIF_LCOEF_1_4_1:
	case TEGRA30_SPDIF_LCOEF_1_4_2:
	case TEGRA30_SPDIF_LCOEF_1_4_3:
	case TEGRA30_SPDIF_LCOEF_1_4_4:
	case TEGRA30_SPDIF_LCOEF_1_4_5:
	case TEGRA30_SPDIF_LCOEF_2_4_0:
	case TEGRA30_SPDIF_LCOEF_2_4_1:
	case TEGRA30_SPDIF_LCOEF_2_4_2:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config tegra30_spdif_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA30_SPDIF_LCOEF_2_4_2,
	.writeable_reg = tegra30_spdif_wr_rd_reg,
	.readable_reg = tegra30_spdif_wr_rd_reg,
	.cache_type = REGCACHE_RBTREE,
};

static const struct tegra30_spdif_soc_data soc_data_tegra30 = {
	.set_audio_cif = tegra30_xbar_set_cif,
};

static const struct tegra30_spdif_soc_data soc_data_tegra124 = {
	.set_audio_cif = tegra124_xbar_set_cif,
};

static const struct of_device_id tegra30_spdif_of_match[] = {
	{ .compatible = "nvidia,tegra30-spdif", .data = &soc_data_tegra30 },
	{ .compatible = "nvidia,tegra124-spdif", .data = &soc_data_tegra124 },
	{},
};

static int tegra30_spdif_platform_probe(struct platform_device *pdev)
{
	struct tegra30_spdif *spdif;
	struct resource *mem, *memregion;
	void __iomem *regs;
	const struct of_device_id *match;
	struct tegra30_spdif_soc_data *soc_data;
	int ret;

	match = of_match_device(tegra30_spdif_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		ret = -ENODEV;
		goto err;
	}
	soc_data = (struct tegra30_spdif_soc_data *)match->data;

	spdif = devm_kzalloc(&pdev->dev, sizeof(struct tegra30_spdif),
				GFP_KERNEL);
	if (!spdif) {
		dev_err(&pdev->dev, "Can't allocate tegra30_spdif\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, spdif);

	spdif->soc_data = soc_data;

	spdif->clk_spdif_out = devm_clk_get(&pdev->dev, "spdif_out");
	if (IS_ERR(spdif->clk_spdif_out)) {
		dev_err(&pdev->dev, "Can't retrieve spdif clock\n");
		ret = PTR_ERR(spdif->clk_spdif_out);
		goto err;
	}

	spdif->clk_spdif_in = devm_clk_get(&pdev->dev, "spdif_in");
	if (IS_ERR(spdif->clk_spdif_in)) {
		dev_err(&pdev->dev, "Can't retrieve spdif clock\n");
		ret = PTR_ERR(spdif->clk_spdif_in);
		goto err_spdif_out_clk_put;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err_spdif_in_clk_put;
	}

	memregion = devm_request_mem_region(&pdev->dev, mem->start,
					    resource_size(mem), DRV_NAME);
	if (!memregion) {
		dev_err(&pdev->dev, "Memory region already claimed\n");
		ret = -EBUSY;
		goto err_spdif_in_clk_put;
	}

	regs = devm_ioremap(&pdev->dev, mem->start, resource_size(mem));
	if (!regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_spdif_in_clk_put;
	}

	spdif->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					    &tegra30_spdif_regmap_config);
	if (IS_ERR(spdif->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		ret = PTR_ERR(spdif->regmap);
		goto err_spdif_in_clk_put;
	}
	regcache_cache_only(spdif->regmap, true);

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra30_spdif_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_codec(&pdev->dev, &tegra30_spdif_codec,
				     tegra30_spdif_dais,
				     ARRAY_SIZE(tegra30_spdif_dais));
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto err_suspend;
	}

	return 0;

err_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra30_spdif_runtime_suspend(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
err_spdif_in_clk_put:
	devm_clk_put(&pdev->dev, spdif->clk_spdif_in);
err_spdif_out_clk_put:
	devm_clk_put(&pdev->dev, spdif->clk_spdif_out);
err:
	return ret;
}

static int tegra30_spdif_platform_remove(struct platform_device *pdev)
{
	struct tegra30_spdif *spdif = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra30_spdif_runtime_suspend(&pdev->dev);

	devm_clk_put(&pdev->dev, spdif->clk_spdif_out);
	devm_clk_put(&pdev->dev, spdif->clk_spdif_in);

	return 0;
}

static const struct dev_pm_ops tegra30_spdif_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra30_spdif_runtime_suspend,
			   tegra30_spdif_runtime_resume, NULL)
};

static struct platform_driver tegra30_spdif_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra30_spdif_of_match,
		.pm = &tegra30_spdif_pm_ops,
	},
	.probe = tegra30_spdif_platform_probe,
	.remove = tegra30_spdif_platform_remove,
};
module_platform_driver(tegra30_spdif_driver);

MODULE_AUTHOR("Songhee Baek <sbaek@nvidia.com>");
MODULE_AUTHOR("Arun Shamanna Lakshmi <aruns@nvidia.com>");
MODULE_DESCRIPTION("Tegra30 SPDIF ASoC driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra30_spdif_of_match);
