/*
 * tegra30_spdif.c - Tegra30 SPDIF driver
 *
 * Author: Sumit Bhattacharya <sumitb@nvidia.com>
 *
 * Based on code copyright/by:
 *
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All Rights Reserved.
 * Scott Peterson <speterson@nvidia.com>
 *
 * Copyright (C) 2010 Google, Inc.
 * Iliyan Malchev <malchev@google.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <mach/iomap.h>
#include <mach/hdmi-audio.h>
#include <mach/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tegra30_spdif.h"

#define DRV_NAME "tegra30-spdif"

static inline void tegra30_spdif_write(struct tegra30_spdif *spdif,
						u32 reg, u32 val)
{
	__raw_writel(val, spdif->regs + reg);
}

static inline u32 tegra30_spdif_read(struct tegra30_spdif *spdif, u32 reg)
{
	return __raw_readl(spdif->regs + reg);
}

static void tegra30_spdif_enable_clocks(struct tegra30_spdif *spdif)
{
	clk_enable(spdif->clk_spdif_out);
	tegra30_ahub_enable_clocks();
}

static void tegra30_spdif_disable_clocks(struct tegra30_spdif *spdif)
{
	tegra30_ahub_disable_clocks();
	clk_disable(spdif->clk_spdif_out);
}

#ifdef CONFIG_DEBUG_FS
static int tegra30_spdif_show(struct seq_file *s, void *unused)
{
#define REG(r) { r, #r }
	static const struct {
		int offset;
		const char *name;
	} regs[] = {
		REG(TEGRA30_SPDIF_CTRL),
		REG(TEGRA30_SPDIF_STROBE_CTRL),
		REG(TEGRA30_SPDIF_CIF_TXD_CTRL),
		REG(TEGRA30_SPDIF_CIF_RXD_CTRL),
		REG(TEGRA30_SPDIF_CIF_TXU_CTRL),
		REG(TEGRA30_SPDIF_CIF_RXU_CTRL),
		REG(TEGRA30_SPDIF_CH_STA_RX_A),
		REG(TEGRA30_SPDIF_CH_STA_RX_B),
		REG(TEGRA30_SPDIF_CH_STA_RX_C),
		REG(TEGRA30_SPDIF_CH_STA_RX_D),
		REG(TEGRA30_SPDIF_CH_STA_RX_E),
		REG(TEGRA30_SPDIF_CH_STA_RX_F),
		REG(TEGRA30_SPDIF_CH_STA_TX_A),
		REG(TEGRA30_SPDIF_CH_STA_TX_B),
		REG(TEGRA30_SPDIF_CH_STA_TX_C),
		REG(TEGRA30_SPDIF_CH_STA_TX_D),
		REG(TEGRA30_SPDIF_CH_STA_TX_E),
		REG(TEGRA30_SPDIF_CH_STA_TX_F),
		REG(TEGRA30_SPDIF_FLOWCTL_CTRL),
		REG(TEGRA30_SPDIF_TX_STEP),
		REG(TEGRA30_SPDIF_FLOW_STATUS),
		REG(TEGRA30_SPDIF_FLOW_TOTAL),
		REG(TEGRA30_SPDIF_FLOW_OVER),
		REG(TEGRA30_SPDIF_FLOW_UNDER),
		REG(TEGRA30_SPDIF_LCOEF_1_4_0),
		REG(TEGRA30_SPDIF_LCOEF_1_4_1),
		REG(TEGRA30_SPDIF_LCOEF_1_4_2),
		REG(TEGRA30_SPDIF_LCOEF_1_4_3),
		REG(TEGRA30_SPDIF_LCOEF_1_4_4),
		REG(TEGRA30_SPDIF_LCOEF_1_4_5),
		REG(TEGRA30_SPDIF_LCOEF_2_4_0),
		REG(TEGRA30_SPDIF_LCOEF_2_4_1),
		REG(TEGRA30_SPDIF_LCOEF_2_4_2),
	};
#undef REG

	struct tegra30_spdif *spdif = s->private;
	int i;

	tegra30_spdif_enable_clocks(spdif);

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		u32 val = tegra30_spdif_read(spdif, regs[i].offset);
		seq_printf(s, "%s = %08x\n", regs[i].name, val);
	}

	tegra30_spdif_disable_clocks(spdif);

	return 0;
}

static int tegra30_spdif_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra30_spdif_show, inode->i_private);
}

static const struct file_operations tegra30_spdif_debug_fops = {
	.open    = tegra30_spdif_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static void tegra30_spdif_debug_add(struct tegra30_spdif *spdif)
{
	char name[] = DRV_NAME;

	spdif->debug = debugfs_create_file(name, S_IRUGO, snd_soc_debugfs_root,
					spdif, &tegra30_spdif_debug_fops);
}

static void tegra30_spdif_debug_remove(struct tegra30_spdif *spdif)
{
	if (spdif->debug)
		debugfs_remove(spdif->debug);
}
#else
static inline void tegra30_spdif_debug_add(struct tegra30_spdif *spdif)
{
}

static inline void tegra30_spdif_debug_remove(struct tegra30_spdif *spdif)
{
}
#endif

int tegra30_spdif_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct tegra30_spdif *spdif = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	tegra30_spdif_enable_clocks(spdif);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = tegra30_ahub_allocate_tx_fifo(&spdif->txcif,
					&spdif->playback_dma_data.addr,
					&spdif->playback_dma_data.req_sel);
		spdif->playback_dma_data.wrap = 4;
		spdif->playback_dma_data.width = 32;
		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_SPDIF_RX0,
					       spdif->txcif);
	}

	tegra30_spdif_disable_clocks(spdif);

	return ret;
}

void tegra30_spdif_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct tegra30_spdif *spdif = snd_soc_dai_get_drvdata(dai);

	tegra30_spdif_enable_clocks(spdif);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_SPDIF_RX0);
		tegra30_ahub_free_tx_fifo(spdif->txcif);
	}

	tegra30_spdif_disable_clocks(spdif);
}

static int tegra30_spdif_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct device *dev = substream->pcm->card->dev;
	struct tegra30_spdif *spdif = snd_soc_dai_get_drvdata(dai);
	int ret, srate, spdifclock;

	if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK) {
		dev_err(dev, "spdif capture is not supported\n");
		return -EINVAL;
	}

	spdif->reg_ctrl &= ~TEGRA30_SPDIF_CTRL_BIT_MODE_MASK;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		spdif->reg_ctrl |= TEGRA30_SPDIF_CTRL_PACK_ENABLE;
		spdif->reg_ctrl |= TEGRA30_SPDIF_CTRL_BIT_MODE_16BIT;
		break;
	default:
		return -EINVAL;
	}

	srate = params_rate(params);
	spdif->reg_ch_sta_a &= ~TEGRA30_SPDIF_CH_STA_TX_A_SAMP_FREQ_MASK;
	spdif->reg_ch_sta_b &= ~TEGRA30_SPDIF_CH_STA_TX_B_ORIG_SAMP_FREQ_MASK;
	switch (srate) {
	case 32000:
		spdifclock = 4096000;
		spdif->reg_ch_sta_a |=
			TEGRA30_SPDIF_CH_STA_TX_A_SAMP_FREQ_32000;
		spdif->reg_ch_sta_b |=
			TEGRA30_SPDIF_CH_STA_TX_B_ORIG_SAMP_FREQ_32000;
		break;
	case 44100:
		spdifclock = 5644800;
		spdif->reg_ch_sta_a |=
			TEGRA30_SPDIF_CH_STA_TX_A_SAMP_FREQ_44100;
		spdif->reg_ch_sta_b |=
			TEGRA30_SPDIF_CH_STA_TX_B_ORIG_SAMP_FREQ_44100;
		break;
	case 48000:
		spdifclock = 6144000;
		spdif->reg_ch_sta_a |=
			TEGRA30_SPDIF_CH_STA_TX_A_SAMP_FREQ_48000;
		spdif->reg_ch_sta_b |=
			TEGRA30_SPDIF_CH_STA_TX_B_ORIG_SAMP_FREQ_48000;
		break;
	case 88200:
		spdifclock = 11289600;
		spdif->reg_ch_sta_a |=
			TEGRA30_SPDIF_CH_STA_TX_A_SAMP_FREQ_88200;
		spdif->reg_ch_sta_b |=
			TEGRA30_SPDIF_CH_STA_TX_B_ORIG_SAMP_FREQ_88200;
		break;
	case 96000:
		spdifclock = 12288000;
		spdif->reg_ch_sta_a |=
			TEGRA30_SPDIF_CH_STA_TX_A_SAMP_FREQ_96000;
		spdif->reg_ch_sta_b |=
			TEGRA30_SPDIF_CH_STA_TX_B_ORIG_SAMP_FREQ_96000;
		break;
	case 176400:
		spdifclock = 22579200;
		spdif->reg_ch_sta_a |=
			TEGRA30_SPDIF_CH_STA_TX_A_SAMP_FREQ_176400;
		spdif->reg_ch_sta_b |=
			TEGRA30_SPDIF_CH_STA_TX_B_ORIG_SAMP_FREQ_176400;
		break;
	case 192000:
		spdifclock = 24576000;
		spdif->reg_ch_sta_a |=
			TEGRA30_SPDIF_CH_STA_TX_A_SAMP_FREQ_192000;
		spdif->reg_ch_sta_b |=
			TEGRA30_SPDIF_CH_STA_TX_B_ORIG_SAMP_FREQ_192000;
		break;
	default:
		return -EINVAL;
	}

	ret = clk_set_rate(spdif->clk_spdif_out, spdifclock);
	if (ret) {
		dev_err(dev, "Can't set SPDIF clock rate: %d\n", ret);
		return ret;
	}

	tegra30_spdif_enable_clocks(spdif);

	tegra30_spdif_write(spdif, TEGRA30_SPDIF_CH_STA_TX_A,
						spdif->reg_ch_sta_a);
	tegra30_spdif_write(spdif, TEGRA30_SPDIF_CH_STA_TX_B,
						spdif->reg_ch_sta_b);

	tegra30_spdif_disable_clocks(spdif);

	ret = tegra_hdmi_setup_audio_freq_source(srate, SPDIF);
	if (ret) {
		dev_err(dev, "Can't set HDMI audio freq source: %d\n", ret);
		return ret;
	}

	return 0;
}

static void tegra30_spdif_reset(struct tegra30_spdif *spdif)
{
#ifndef CONFIG_ARCH_TEGRA_11x_SOC
	u32 val;
	int dcnt = 10;

	val = tegra30_spdif_read(spdif, TEGRA30_SPDIF_CTRL);
	val |= TEGRA30_SPDIF_CTRL_SOFT_RESET_ENABLE;
	tegra30_spdif_write(spdif, TEGRA30_SPDIF_CTRL, val);

	while ((tegra30_spdif_read(spdif, TEGRA30_SPDIF_CTRL) &
		    TEGRA30_SPDIF_CTRL_SOFT_RESET_ENABLE) && dcnt--)
		udelay(100);
#else
	tegra_periph_reset_assert(spdif->clk_spdif_out);
	tegra_periph_reset_deassert(spdif->clk_spdif_out);
#endif
}

static void tegra30_spdif_start_playback(struct tegra30_spdif *spdif)
{
	tegra30_ahub_enable_tx_fifo(spdif->txcif);
	spdif->reg_ctrl |= TEGRA30_SPDIF_CTRL_TX_EN_ENABLE |
				TEGRA30_SPDIF_CTRL_TC_EN_ENABLE;
	tegra30_spdif_write(spdif, TEGRA30_SPDIF_CTRL, spdif->reg_ctrl);
}

static void tegra30_spdif_stop_playback(struct tegra30_spdif *spdif)
{
	tegra30_ahub_disable_tx_fifo(spdif->txcif);
	spdif->reg_ctrl &= ~(TEGRA30_SPDIF_CTRL_TX_EN_ENABLE |
				TEGRA30_SPDIF_CTRL_TC_EN_ENABLE);
	tegra30_spdif_write(spdif, TEGRA30_SPDIF_CTRL, spdif->reg_ctrl);
	tegra30_spdif_reset(spdif);
}

static int tegra30_spdif_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct tegra30_spdif *spdif = snd_soc_dai_get_drvdata(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		tegra30_spdif_enable_clocks(spdif);
		tegra30_spdif_start_playback(spdif);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		tegra30_spdif_stop_playback(spdif);
		tegra30_spdif_disable_clocks(spdif);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tegra30_spdif_probe(struct snd_soc_dai *dai)
{
	struct tegra30_spdif *spdif = snd_soc_dai_get_drvdata(dai);

	dai->playback_dma_data = &spdif->playback_dma_data;
	dai->capture_dma_data = NULL;

	return 0;
}

static struct snd_soc_dai_ops tegra30_spdif_dai_ops = {
	.startup	= tegra30_spdif_startup,
	.shutdown	= tegra30_spdif_shutdown,
	.hw_params	= tegra30_spdif_hw_params,
	.trigger	= tegra30_spdif_trigger,
};

struct snd_soc_dai_driver tegra30_spdif_dai = {
	.name = DRV_NAME,
	.probe = tegra30_spdif_probe,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.ops = &tegra30_spdif_dai_ops,
};

static __devinit int tegra30_spdif_platform_probe(struct platform_device *pdev)
{
	struct tegra30_spdif *spdif;
	struct resource *mem, *memregion;
	int ret;
	u32 reg_val;

	spdif = kzalloc(sizeof(struct tegra30_spdif), GFP_KERNEL);
	if (!spdif) {
		dev_err(&pdev->dev, "Can't allocate tegra30_spdif\n");
		ret = -ENOMEM;
		goto exit;
	}
	dev_set_drvdata(&pdev->dev, spdif);

	spdif->clk_spdif_out = clk_get(&pdev->dev, "spdif_out");
	if (IS_ERR(spdif->clk_spdif_out)) {
		dev_err(&pdev->dev, "Can't retrieve spdif clock\n");
		ret = PTR_ERR(spdif->clk_spdif_out);
		goto err_free;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err_clk_put_spdif;
	}

	memregion = request_mem_region(mem->start, resource_size(mem),
					DRV_NAME);
	if (!memregion) {
		dev_err(&pdev->dev, "Memory region already claimed\n");
		ret = -EBUSY;
		goto err_clk_put_spdif;
	}

	spdif->regs = ioremap(mem->start, resource_size(mem));
	if (!spdif->regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_release;
	}

	tegra30_spdif_enable_clocks(spdif);

	reg_val = TEGRA30_SPDIF_CIF_TXD_CTRL_DIRECTION_RXCIF |
		TEGRA30_SPDIF_CIF_TXD_CTRL_AUDIO_BIT16 |
		TEGRA30_SPDIF_CIF_TXD_CTRL_CLIENT_BIT16 |
		TEGRA30_SPDIF_CIF_TXD_CTRL_AUDIO_CH2 |
		TEGRA30_SPDIF_CIF_TXD_CTRL_CLIENT_CH2 |
		(3 << TEGRA30_SPDIF_CIF_TXD_CTRL_FIFO_TH_SHIFT);

	tegra30_spdif_write(spdif, TEGRA30_SPDIF_CIF_TXD_CTRL, reg_val);

	tegra30_spdif_disable_clocks(spdif);

	ret = snd_soc_register_dai(&pdev->dev, &tegra30_spdif_dai);
	if (ret) {
		dev_err(&pdev->dev, "Could not register DAI: %d\n", ret);
		ret = -ENOMEM;
		goto err_unmap;
	}

	tegra30_spdif_debug_add(spdif);

	return 0;

err_unmap:
	iounmap(spdif->regs);
err_release:
	release_mem_region(mem->start, resource_size(mem));
err_clk_put_spdif:
	clk_put(spdif->clk_spdif_out);
err_free:
	kfree(spdif);
exit:
	return ret;
}

static int __devexit tegra30_spdif_platform_remove(struct platform_device *pdev)
{
	struct tegra30_spdif *spdif = dev_get_drvdata(&pdev->dev);
	struct resource *res;

	snd_soc_unregister_dai(&pdev->dev);

	tegra30_spdif_debug_remove(spdif);

	iounmap(spdif->regs);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, resource_size(res));

	clk_put(spdif->clk_spdif_out);

	kfree(spdif);

	return 0;
}

static struct platform_driver tegra30_spdif_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = tegra30_spdif_platform_probe,
	.remove = __devexit_p(tegra30_spdif_platform_remove),
};

static int __init snd_tegra30_spdif_init(void)
{
	return platform_driver_register(&tegra30_spdif_driver);
}
module_init(snd_tegra30_spdif_init);

static void __exit snd_tegra30_spdif_exit(void)
{
	platform_driver_unregister(&tegra30_spdif_driver);
}
module_exit(snd_tegra30_spdif_exit);

MODULE_AUTHOR("Sumit Bhattacharya <sumitb@nvidia.com>");
MODULE_DESCRIPTION("Tegra30 SPDIF ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
