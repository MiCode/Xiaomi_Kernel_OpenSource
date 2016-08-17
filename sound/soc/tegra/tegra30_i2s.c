/*
 * tegra30_i2s.c - Tegra 30 I2S driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * Based on code copyright/by:
 *
 * Copyright (c) 2009-2013, NVIDIA Corporation.
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved.
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
#include <mach/tegra_asoc_pdata.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "tegra30_ahub.h"
#include "tegra30_dam.h"
#include "tegra30_i2s.h"

#define DRV_NAME "tegra30-i2s"

static struct tegra30_i2s  i2scont[TEGRA30_NR_I2S_IFC];

static inline void tegra30_i2s_write(struct tegra30_i2s *i2s, u32 reg, u32 val)
{
#ifdef CONFIG_PM
	i2s->reg_cache[reg >> 2] = val;
#endif
	__raw_writel(val, i2s->regs + reg);
}

static inline u32 tegra30_i2s_read(struct tegra30_i2s *i2s, u32 reg)
{
	return __raw_readl(i2s->regs + reg);
}

static void tegra30_i2s_enable_clocks(struct tegra30_i2s *i2s)
{
	tegra30_ahub_enable_clocks();
	clk_enable(i2s->clk_i2s);
}

static void tegra30_i2s_disable_clocks(struct tegra30_i2s *i2s)
{
	clk_disable(i2s->clk_i2s);
	tegra30_ahub_disable_clocks();
}

#ifdef CONFIG_DEBUG_FS
static int tegra30_i2s_show(struct seq_file *s, void *unused)
{
#define REG(r) { r, #r }
	static const struct {
		int offset;
		const char *name;
	} regs[] = {
		REG(TEGRA30_I2S_CTRL),
		REG(TEGRA30_I2S_TIMING),
		REG(TEGRA30_I2S_OFFSET),
		REG(TEGRA30_I2S_CH_CTRL),
		REG(TEGRA30_I2S_SLOT_CTRL),
		REG(TEGRA30_I2S_CIF_TX_CTRL),
		REG(TEGRA30_I2S_CIF_RX_CTRL),
		REG(TEGRA30_I2S_FLOWCTL),
		REG(TEGRA30_I2S_TX_STEP),
		REG(TEGRA30_I2S_FLOW_STATUS),
		REG(TEGRA30_I2S_FLOW_TOTAL),
		REG(TEGRA30_I2S_FLOW_OVER),
		REG(TEGRA30_I2S_FLOW_UNDER),
		REG(TEGRA30_I2S_LCOEF_1_4_0),
		REG(TEGRA30_I2S_LCOEF_1_4_1),
		REG(TEGRA30_I2S_LCOEF_1_4_2),
		REG(TEGRA30_I2S_LCOEF_1_4_3),
		REG(TEGRA30_I2S_LCOEF_1_4_4),
		REG(TEGRA30_I2S_LCOEF_1_4_5),
		REG(TEGRA30_I2S_LCOEF_2_4_0),
		REG(TEGRA30_I2S_LCOEF_2_4_1),
		REG(TEGRA30_I2S_LCOEF_2_4_2),
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
		REG(TEGRA30_I2S_SLOT_CTRL2),
#endif
	};
#undef REG

	struct tegra30_i2s *i2s = s->private;
	int i;

	tegra30_i2s_enable_clocks(i2s);

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		u32 val = tegra30_i2s_read(i2s, regs[i].offset);
		seq_printf(s, "%s = %08x\n", regs[i].name, val);
	}

	tegra30_i2s_disable_clocks(i2s);

	return 0;
}

static int tegra30_i2s_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra30_i2s_show, inode->i_private);
}

static const struct file_operations tegra30_i2s_debug_fops = {
	.open    = tegra30_i2s_debug_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static void tegra30_i2s_debug_add(struct tegra30_i2s *i2s, int id)
{
	char name[] = DRV_NAME ".0";

	snprintf(name, sizeof(name), DRV_NAME".%1d", id);
	i2s->debug = debugfs_create_file(name, S_IRUGO, snd_soc_debugfs_root,
						i2s, &tegra30_i2s_debug_fops);
}

static void tegra30_i2s_debug_remove(struct tegra30_i2s *i2s)
{
	if (i2s->debug)
		debugfs_remove(i2s->debug);
}
#else
static inline void tegra30_i2s_debug_add(struct tegra30_i2s *i2s, int id)
{
}

static inline void tegra30_i2s_debug_remove(struct tegra30_i2s *i2s)
{
}
#endif

int tegra30_i2s_startup(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	int ret;

	tegra30_i2s_enable_clocks(i2s);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* increment the playback ref count */
		i2s->playback_ref_count++;

		ret = tegra30_ahub_allocate_tx_fifo(&i2s->txcif,
					&i2s->playback_dma_data.addr,
					&i2s->playback_dma_data.req_sel);
		i2s->playback_dma_data.wrap = 4;
		i2s->playback_dma_data.width = 32;

		if (!i2s->is_dam_used)
			tegra30_ahub_set_rx_cif_source(
				TEGRA30_AHUB_RXCIF_I2S0_RX0 + i2s->id,
				i2s->txcif);
	} else {
		i2s->capture_ref_count++;
		ret = tegra30_ahub_allocate_rx_fifo(&i2s->rxcif,
					&i2s->capture_dma_data.addr,
					&i2s->capture_dma_data.req_sel);
		i2s->capture_dma_data.wrap = 4;
		i2s->capture_dma_data.width = 32;
		tegra30_ahub_set_rx_cif_source(i2s->rxcif,
					TEGRA30_AHUB_TXCIF_I2S0_TX0 + i2s->id);
	}

	tegra30_i2s_disable_clocks(i2s);

	return ret;
}

void tegra30_i2s_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	tegra30_i2s_enable_clocks(i2s);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (i2s->playback_ref_count == 1)
			tegra30_ahub_unset_rx_cif_source(
				TEGRA30_AHUB_RXCIF_I2S0_RX0 + i2s->id);

		/* free the apbif dma channel*/
		tegra30_ahub_free_tx_fifo(i2s->txcif);

		/* decrement the playback ref count */
		i2s->playback_ref_count--;
	} else {
		if (i2s->capture_ref_count == 1)
			tegra30_ahub_unset_rx_cif_source(i2s->rxcif);
		tegra30_ahub_free_rx_fifo(i2s->rxcif);
		i2s->capture_ref_count--;
	}

	tegra30_i2s_disable_clocks(i2s);
}

static int tegra30_i2s_set_fmt(struct snd_soc_dai *dai,
				unsigned int fmt)
{
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	default:
		return -EINVAL;
	}

	i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_MASTER_ENABLE;
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_MASTER_ENABLE;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		break;
	default:
		return -EINVAL;
	}

	i2s->reg_ctrl &= ~(TEGRA30_I2S_CTRL_FRAME_FORMAT_MASK |
				TEGRA30_I2S_CTRL_LRCK_MASK);
	i2s->reg_ch_ctrl &= ~TEGRA30_I2S_CH_CTRL_EGDE_CTRL_MASK;
	i2s->daifmt = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
	switch (i2s->daifmt) {
	case SND_SOC_DAIFMT_DSP_A:
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_FRAME_FORMAT_FSYNC;
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_LRCK_R_LOW;
		i2s->reg_ch_ctrl |= TEGRA30_I2S_CH_CTRL_EGDE_CTRL_NEG_EDGE;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_FRAME_FORMAT_FSYNC;
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_LRCK_R_LOW;
		i2s->reg_ch_ctrl |= TEGRA30_I2S_CH_CTRL_EGDE_CTRL_POS_EDGE;
		break;
	case SND_SOC_DAIFMT_I2S:
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_FRAME_FORMAT_LRCK;
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_LRCK_L_LOW;
		i2s->reg_ch_ctrl |= TEGRA30_I2S_CH_CTRL_EGDE_CTRL_POS_EDGE;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_FRAME_FORMAT_LRCK;
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_LRCK_R_LOW;
		i2s->reg_ch_ctrl |= TEGRA30_I2S_CH_CTRL_EGDE_CTRL_POS_EDGE;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_FRAME_FORMAT_LRCK;
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_LRCK_R_LOW;
		i2s->reg_ch_ctrl |= TEGRA30_I2S_CH_CTRL_EGDE_CTRL_POS_EDGE;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void tegra30_i2s_set_channel_bit_count(struct tegra30_i2s *i2s,
				int i2sclock, int srate)
{
	int sym_bitclk, bitcnt;
	u32 val;

	bitcnt = (i2sclock / (2 * srate)) - 1;
	sym_bitclk = !(i2sclock % (2 * srate));

	val = bitcnt << TEGRA30_I2S_TIMING_CHANNEL_BIT_COUNT_SHIFT;

	if (!sym_bitclk)
		val |= TEGRA30_I2S_TIMING_NON_SYM_ENABLE;

	tegra30_i2s_write(i2s, TEGRA30_I2S_TIMING, val);
}

static void tegra30_i2s_set_data_offset(struct tegra30_i2s *i2s)
{
	u32 val;
	int rx_data_offset = i2s->dsp_config.rx_data_offset;
	int tx_data_offset = i2s->dsp_config.tx_data_offset;

	val = (rx_data_offset <<
				TEGRA30_I2S_OFFSET_RX_DATA_OFFSET_SHIFT) |
			(tx_data_offset <<
				TEGRA30_I2S_OFFSET_TX_DATA_OFFSET_SHIFT);

	tegra30_i2s_write(i2s, TEGRA30_I2S_OFFSET, val);
}

static void tegra30_i2s_set_slot_control(struct tegra30_i2s *i2s, int stream)
{
	u32 val;
	int tx_mask = i2s->dsp_config.tx_mask;
	int rx_mask = i2s->dsp_config.rx_mask;

	val = tegra30_i2s_read(i2s, TEGRA30_I2S_SLOT_CTRL);
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		val &= ~TEGRA30_I2S_SLOT_CTRL_TX_SLOT_ENABLES_MASK;
		val |= (tx_mask << TEGRA30_I2S_SLOT_CTRL_TX_SLOT_ENABLES_SHIFT);
	} else {
		val &= ~TEGRA30_I2S_SLOT_CTRL_RX_SLOT_ENABLES_MASK;
		val |= (rx_mask << TEGRA30_I2S_SLOT_CTRL_RX_SLOT_ENABLES_SHIFT);
	}

	val &= ~TEGRA30_I2S_SLOT_CTRL_TOTAL_SLOTS_MASK;
	val |= (i2s->dsp_config.num_slots - 1)
			<< TEGRA30_I2S_SLOT_CTRL_TOTAL_SLOTS_SHIFT;

	tegra30_i2s_write(i2s, TEGRA30_I2S_SLOT_CTRL, val);
}

static int tegra30_i2s_tdm_setup_clocks(struct device *dev,
				struct tegra30_i2s *i2s, int *i2sclock)
{
	int ret;

	if (i2s->reg_ctrl & TEGRA30_I2S_CTRL_MASTER_ENABLE) {

		ret = clk_set_parent(i2s->clk_i2s, i2s->clk_pll_a_out0);
		if (ret) {
			dev_err(dev, "Can't set parent of I2S clock\n");
			return ret;
		}

		ret = clk_set_rate(i2s->clk_i2s, *i2sclock);
		if (ret) {
			dev_err(dev, "Can't set I2S clock rate: %d\n", ret);
			return ret;
		}
	} else {

		ret = clk_set_rate(i2s->clk_i2s_sync, *i2sclock);
		if (ret) {
			dev_err(dev, "Can't set I2S sync clock rate\n");
			return ret;
		}

		ret = clk_set_parent(clk_get_parent(i2s->clk_audio_2x),
						i2s->clk_i2s_sync);
		if (ret) {
			dev_err(dev, "Can't set parent of audio2x clock\n");
			return ret;
		}

		ret = clk_set_rate(i2s->clk_audio_2x, *i2sclock);
		if (ret) {
			dev_err(dev, "Can't set audio2x clock rate\n");
			return ret;
		}

		ret = clk_set_parent(i2s->clk_i2s, i2s->clk_audio_2x);
		if (ret) {
			dev_err(dev, "Can't set parent of i2s clock\n");
			return ret;
		}
	}
	return ret;
}


static int tegra30_i2s_tdm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct device *dev = substream->pcm->card->dev;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	u32 val;
	int i2s_client_ch, i2s_audio_ch;
	int i2s_audio_bits = 0, i2s_client_bits = 0;
	int i2sclock, srate;
	int ret;

	srate = params_rate(params);

	i2sclock = srate *
				i2s->dsp_config.num_slots *
				i2s->dsp_config.slot_width;

	ret = tegra30_i2s_tdm_setup_clocks(dev, i2s, &i2sclock);
	if (ret)
		return -EINVAL;

	/* Run ahub clock greater than i2sclock */
	tegra30_ahub_clock_set_rate(i2sclock*2);

	tegra30_i2s_enable_clocks(i2s);

	tegra30_i2s_set_channel_bit_count(i2s, i2sclock*2, srate);

	i2s_client_ch = i2s->dsp_config.num_slots;
	i2s_audio_ch = i2s->dsp_config.num_slots;

	i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_BIT_SIZE_MASK;
	switch (i2s->dsp_config.slot_width) {
	case 16:
		i2s_audio_bits = TEGRA30_AUDIOCIF_BITS_16;
		i2s_client_bits = TEGRA30_AUDIOCIF_BITS_16;
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_BIT_SIZE_16;
		break;
	case 32:
		i2s_audio_bits = TEGRA30_AUDIOCIF_BITS_32;
		i2s_client_bits = TEGRA30_AUDIOCIF_BITS_32;
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_BIT_SIZE_32;
		break;
	default:
		dev_err(dev, "unknown slot_width %d\n",
				i2s->dsp_config.slot_width);
		return -EINVAL;
	}

	val = (0 << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
			((i2s_audio_ch - 1) <<
			 TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
			((i2s_client_ch - 1) <<
			 TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
			(i2s_audio_bits <<
			 TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT) |
			(i2s_client_bits <<
			 TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		val |= TEGRA30_AUDIOCIF_CTRL_DIRECTION_RX;
		tegra30_i2s_write(i2s, TEGRA30_I2S_CIF_RX_CTRL, val);

		tegra30_ahub_set_tx_cif_channels(i2s->txcif,
						i2s_audio_ch,
						i2s_client_ch);
		tegra30_ahub_set_tx_cif_bits(i2s->txcif,
						i2s_audio_bits,
						i2s_client_bits);
		tegra30_ahub_set_tx_fifo_pack_mode(i2s->txcif, 0);

	} else {
		val |= TEGRA30_AUDIOCIF_CTRL_DIRECTION_TX;
		tegra30_i2s_write(i2s, TEGRA30_I2S_CIF_TX_CTRL, val);

		tegra30_ahub_set_rx_cif_channels(i2s->rxcif,
						i2s_audio_ch,
						i2s_client_ch);
		tegra30_ahub_set_rx_cif_bits(i2s->rxcif,
						i2s_audio_bits,
						i2s_client_bits);
		tegra30_ahub_set_rx_fifo_pack_mode(i2s->rxcif, 0);
	}

	tegra30_i2s_set_slot_control(i2s, substream->stream);

	tegra30_i2s_set_data_offset(i2s);

	i2s->reg_ch_ctrl &= ~TEGRA30_I2S_CH_CTRL_FSYNC_WIDTH_MASK;
	i2s->reg_ch_ctrl |= (i2s->dsp_config.slot_width - 1) <<
			TEGRA30_I2S_CH_CTRL_FSYNC_WIDTH_SHIFT;
	tegra30_i2s_write(i2s, TEGRA30_I2S_CH_CTRL, i2s->reg_ch_ctrl);

	tegra30_i2s_disable_clocks(i2s);

	return 0;
}

static int tegra30_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct device *dev = substream->pcm->card->dev;
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(dai);
	u32 val, i;
	int ret, sample_size, srate, i2sclock, bitcnt, sym_bitclk;
	int i2s_client_ch;

	i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_BIT_SIZE_MASK;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_BIT_SIZE_8;
		sample_size = 8;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_BIT_SIZE_16;
		sample_size = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_BIT_SIZE_24;
		sample_size = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_BIT_SIZE_32;
		sample_size = 32;
		break;
	default:
		return -EINVAL;
	}

	bitcnt = sample_size;
	i = 0;

	/* TDM mode */
	if ((i2s->reg_ctrl & TEGRA30_I2S_CTRL_FRAME_FORMAT_FSYNC) &&
		(i2s->dsp_config.slot_width > 2))
		return tegra30_i2s_tdm_hw_params(substream, params, dai);

	srate = params_rate(params);

	if (i2s->reg_ctrl & TEGRA30_I2S_CTRL_MASTER_ENABLE) {
		i2sclock = srate * params_channels(params) * sample_size;

		/* Additional "* 4" is needed for FSYNC mode */
		if (i2s->reg_ctrl & TEGRA30_I2S_CTRL_FRAME_FORMAT_FSYNC)
			i2sclock *= 4;

#ifndef CONFIG_ARCH_TEGRA_3x_SOC
		/* If the I2S is used for voice also, it is not
		*  necessary to set its clock if it had been set
		*  like during voice call.*/
		if (!(i2s->playback_ref_count - 1)) {
			ret = clk_set_parent(i2s->clk_i2s,
				i2s->clk_pll_a_out0);
			if (ret) {
				dev_err(dev,
				"Can't set parent of I2S clock\n");
				return ret;
			}

			ret = clk_set_rate(i2s->clk_i2s, i2sclock);
			if (ret) {
				dev_err(dev,
				"Can't set I2S clock rate: %d\n", ret);
				return ret;
			}
		}
#else
		ret = clk_set_parent(i2s->clk_i2s, i2s->clk_pll_a_out0);
		if (ret) {
			dev_err(dev, "Can't set parent of I2S clock\n");
			return ret;
		}

		ret = clk_set_rate(i2s->clk_i2s, i2sclock);
		if (ret) {
			dev_err(dev, "Can't set I2S clock rate: %d\n", ret);
			return ret;
		}
#endif

		tegra30_i2s_enable_clocks(i2s);

		if (i2s->reg_ctrl & TEGRA30_I2S_CTRL_FRAME_FORMAT_FSYNC) {
			bitcnt = (i2sclock / srate) - 1;
			sym_bitclk = !(i2sclock % srate);
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
			val = tegra30_i2s_read(i2s, TEGRA30_I2S_SLOT_CTRL2);

			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				val &=
				  ~TEGRA30_I2S_SLOT_CTRL2_TX_SLOT_ENABLES_MASK;
				val |= ((1 << params_channels(params)) - 1) <<
				  TEGRA30_I2S_SLOT_CTRL2_TX_SLOT_ENABLES_SHIFT;
			} else {
				val &=
				  ~TEGRA30_I2S_SLOT_CTRL2_RX_SLOT_ENABLES_MASK;
				val |= ((1 << params_channels(params)) - 1) <<
				  TEGRA30_I2S_SLOT_CTRL2_RX_SLOT_ENABLES_SHIFT;
			}
			tegra30_i2s_write(i2s, TEGRA30_I2S_SLOT_CTRL2, val);
#endif
		} else {
			bitcnt = (i2sclock / (2 * srate)) - 1;
			sym_bitclk = !(i2sclock % (2 * srate));
		}
		val = bitcnt << TEGRA30_I2S_TIMING_CHANNEL_BIT_COUNT_SHIFT;

		if (!sym_bitclk)
			val |= TEGRA30_I2S_TIMING_NON_SYM_ENABLE;

		tegra30_i2s_write(i2s, TEGRA30_I2S_TIMING, val);
	} else {
		i2sclock = srate * params_channels(params) * sample_size;

		/* Additional "* 2" is needed for FSYNC mode */
		if (i2s->reg_ctrl & TEGRA30_I2S_CTRL_FRAME_FORMAT_FSYNC)
			i2sclock *= 2;

		ret = clk_set_rate(i2s->clk_i2s_sync, i2sclock);
		if (ret) {
			dev_err(dev, "Can't set I2S sync clock rate\n");
			return ret;
		}

		ret = clk_set_parent(clk_get_parent(i2s->clk_audio_2x),
						i2s->clk_i2s_sync);
		if (ret) {
			dev_err(dev, "Can't set parent of audio2x clock\n");
			return ret;
		}

		ret = clk_set_rate(i2s->clk_audio_2x, i2sclock);
		if (ret) {
			dev_err(dev, "Can't set I2S sync clock rate\n");
			return ret;
		}

		ret = clk_set_parent(i2s->clk_i2s, i2s->clk_audio_2x);
		if (ret) {
			dev_err(dev, "Can't set parent of audio2x clock\n");
			return ret;
		}

		tegra30_i2s_enable_clocks(i2s);
	}

	i2s_client_ch = (i2s->reg_ctrl & TEGRA30_I2S_CTRL_FRAME_FORMAT_FSYNC) ?
			params_channels(params) : 2;

	switch (sample_size) {
	case 8:
		val = (0 << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
	      ((params_channels(params) - 1) <<
			TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
	      ((i2s_client_ch - 1) <<
			TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
	      TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_8 |
	      TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_8;
		break;

	case 16:
		val = (0 << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
	      ((params_channels(params) - 1) <<
			TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
	      ((i2s_client_ch - 1) <<
			TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
	      TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_16 |
	      TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_16;
		break;

	case 24:
		val = (0 << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
	      ((params_channels(params) - 1) <<
			TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
	      ((i2s_client_ch - 1) <<
			TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
	      TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_24 |
	      TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_24;
		break;

	case 32:
		val = (0 << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
	      ((params_channels(params) - 1) <<
			TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
	      ((i2s_client_ch - 1) <<
			TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
	      TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_32 |
	      TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_32;
		break;

	default:
		pr_err("Error in sample size\n");
		val = 0;
		break;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		val |= TEGRA30_AUDIOCIF_CTRL_DIRECTION_RX;
		tegra30_i2s_write(i2s, TEGRA30_I2S_CIF_RX_CTRL, val);

		tegra30_ahub_set_tx_cif_channels(i2s->txcif,
						 params_channels(params),
						 params_channels(params));

		switch (sample_size) {
		case 8:
			tegra30_ahub_set_tx_cif_bits(i2s->txcif,
			  TEGRA30_AUDIOCIF_BITS_8, TEGRA30_AUDIOCIF_BITS_8);
			tegra30_ahub_set_tx_fifo_pack_mode(i2s->txcif,
			  TEGRA30_AHUB_CHANNEL_CTRL_TX_PACK_8_4);
			break;

		case 16:
			tegra30_ahub_set_tx_cif_bits(i2s->txcif,
			  TEGRA30_AUDIOCIF_BITS_16, TEGRA30_AUDIOCIF_BITS_16);
			tegra30_ahub_set_tx_fifo_pack_mode(i2s->txcif,
			  TEGRA30_AHUB_CHANNEL_CTRL_TX_PACK_16);
			break;

		case 24:
			tegra30_ahub_set_tx_cif_bits(i2s->txcif,
			  TEGRA30_AUDIOCIF_BITS_24, TEGRA30_AUDIOCIF_BITS_24);
			tegra30_ahub_set_tx_fifo_pack_mode(i2s->txcif, 0);
			break;

		case 32:
			tegra30_ahub_set_tx_cif_bits(i2s->txcif,
			  TEGRA30_AUDIOCIF_BITS_32, TEGRA30_AUDIOCIF_BITS_32);
			tegra30_ahub_set_tx_fifo_pack_mode(i2s->txcif, 0);
			break;

		default:
			pr_err("Error in sample_size\n");
			break;
		}
	} else {
		val |= TEGRA30_AUDIOCIF_CTRL_DIRECTION_TX;
		tegra30_i2s_write(i2s, TEGRA30_I2S_CIF_TX_CTRL, val);

		tegra30_ahub_set_rx_cif_channels(i2s->rxcif,
						 params_channels(params),
						 params_channels(params));

		switch (sample_size) {
		case 8:
			tegra30_ahub_set_rx_cif_bits(i2s->rxcif,
			  TEGRA30_AUDIOCIF_BITS_8, TEGRA30_AUDIOCIF_BITS_8);
			tegra30_ahub_set_rx_fifo_pack_mode(i2s->rxcif,
			  TEGRA30_AHUB_CHANNEL_CTRL_RX_PACK_8_4);
			break;

		case 16:
			tegra30_ahub_set_rx_cif_bits(i2s->rxcif,
			  TEGRA30_AUDIOCIF_BITS_16, TEGRA30_AUDIOCIF_BITS_16);
			tegra30_ahub_set_rx_fifo_pack_mode(i2s->rxcif,
			  TEGRA30_AHUB_CHANNEL_CTRL_RX_PACK_16);
			break;

		case 24:
			tegra30_ahub_set_rx_cif_bits(i2s->rxcif,
			  TEGRA30_AUDIOCIF_BITS_24, TEGRA30_AUDIOCIF_BITS_24);
			tegra30_ahub_set_rx_fifo_pack_mode(i2s->rxcif, 0);
			break;

		case 32:
			tegra30_ahub_set_rx_cif_bits(i2s->rxcif,
			  TEGRA30_AUDIOCIF_BITS_32, TEGRA30_AUDIOCIF_BITS_32);
			tegra30_ahub_set_rx_fifo_pack_mode(i2s->rxcif, 0);
			break;

		default:
			pr_err("Error in sample_size\n");
			break;
		}
	}
	switch (i2s->daifmt) {
	case SND_SOC_DAIFMT_RIGHT_J:
		val = 0;
		if ((bitcnt - sample_size) > 0)
			val = bitcnt - sample_size;
		break;
	case SND_SOC_DAIFMT_DSP_B:
	case SND_SOC_DAIFMT_LEFT_J:
		val = 0;
		break;
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_DSP_A: /* fall through */
		val = 1;
		break;
	default:
		return -EINVAL;
	}
	val = (val << TEGRA30_I2S_OFFSET_RX_DATA_OFFSET_SHIFT) |
		  (val << TEGRA30_I2S_OFFSET_TX_DATA_OFFSET_SHIFT);
	tegra30_i2s_write(i2s, TEGRA30_I2S_OFFSET, val);

	tegra30_i2s_write(i2s, TEGRA30_I2S_CH_CTRL, i2s->reg_ch_ctrl);

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	val = tegra30_i2s_read(i2s, TEGRA30_I2S_SLOT_CTRL);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		val &= ~TEGRA30_I2S_SLOT_CTRL_TX_SLOT_ENABLES_MASK;
		val |= (1 << TEGRA30_I2S_SLOT_CTRL_TX_SLOT_ENABLES_SHIFT);
	} else {
		val &= ~TEGRA30_I2S_SLOT_CTRL_RX_SLOT_ENABLES_MASK;
		val |= (1 << TEGRA30_I2S_SLOT_CTRL_RX_SLOT_ENABLES_SHIFT);
	}
#else
	val = 0;
	if (i2s->reg_ctrl & TEGRA30_I2S_CTRL_FRAME_FORMAT_FSYNC)
		val = params_channels(params) - 1;
#endif

	tegra30_i2s_write(i2s, TEGRA30_I2S_SLOT_CTRL, val);

	tegra30_i2s_disable_clocks(i2s);
	return 0;
}

static int tegra30_i2s_soft_reset(struct tegra30_i2s *i2s)
{
	int dcnt = 10;

	i2s->reg_ctrl |= TEGRA30_I2S_CTRL_SOFT_RESET;
	tegra30_i2s_write(i2s, TEGRA30_I2S_CTRL, i2s->reg_ctrl);

	while ((tegra30_i2s_read(i2s, TEGRA30_I2S_CTRL) &
		       TEGRA30_I2S_CTRL_SOFT_RESET) && dcnt--)
		udelay(100);

	/* Restore reg_ctrl to ensure if a concurrent playback/capture
	   session was active it continues after SOFT_RESET */
	i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_SOFT_RESET;
	tegra30_i2s_write(i2s, TEGRA30_I2S_CTRL, i2s->reg_ctrl);

	return (dcnt < 0) ? -ETIMEDOUT : 0;
}

static void tegra30_i2s_start_playback(struct tegra30_i2s *i2s)
{
	tegra30_ahub_enable_tx_fifo(i2s->txcif);
	/* if this is the only user of i2s tx then enable it*/
	if (i2s->playback_ref_count == 1) {
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_XFER_EN_TX;
		tegra30_i2s_write(i2s, TEGRA30_I2S_CTRL, i2s->reg_ctrl);
	}
}

static void tegra30_i2s_stop_playback(struct tegra30_i2s *i2s)
{
	int dcnt = 10;
	/* if this is the only user of i2s tx then disable it*/
	tegra30_ahub_disable_tx_fifo(i2s->txcif);
	if (i2s->playback_ref_count == 1) {
		i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_XFER_EN_TX;
		tegra30_i2s_write(i2s, TEGRA30_I2S_CTRL, i2s->reg_ctrl);
		while (tegra30_ahub_tx_fifo_is_enabled(i2s->id) && dcnt--)
			udelay(100);

		dcnt = 10;
		while (!tegra30_ahub_tx_fifo_is_empty(i2s->id) && dcnt--)
			udelay(100);

		/* In case I2S FIFO does not get empty do a soft reset of the
		   I2S channel to prevent channel reversal in next session */
		if (dcnt < 0) {
			tegra30_i2s_soft_reset(i2s);

			dcnt = 10;
			while (!tegra30_ahub_tx_fifo_is_empty(i2s->id) &&
			       dcnt--)
				udelay(100);
		}
	}
}

static void tegra30_i2s_start_capture(struct tegra30_i2s *i2s)
{
	tegra30_ahub_enable_rx_fifo(i2s->rxcif);
	if (!i2s->is_call_mode_rec && (i2s->capture_ref_count == 1)) {
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_XFER_EN_RX;
		tegra30_i2s_write(i2s, TEGRA30_I2S_CTRL, i2s->reg_ctrl);
	}
}

static void tegra30_i2s_stop_capture(struct tegra30_i2s *i2s)
{
	int dcnt = 10;
	if (!i2s->is_call_mode_rec && (i2s->capture_ref_count == 1)) {
		i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_XFER_EN_RX;
		tegra30_i2s_write(i2s, TEGRA30_I2S_CTRL, i2s->reg_ctrl);
		while (tegra30_ahub_rx_fifo_is_enabled(i2s->id) && dcnt--)
			udelay(100);

		dcnt = 10;
		while (!tegra30_ahub_rx_fifo_is_empty(i2s->id) && dcnt--)
			udelay(100);

		/* In case I2S FIFO does not get empty do a soft reset of
		   the I2S channel to prevent channel reversal in next capture
		   session */
		if (dcnt < 0) {
			tegra30_i2s_soft_reset(i2s);

			dcnt = 10;
			while (!tegra30_ahub_rx_fifo_is_empty(i2s->id) &&
			       dcnt--)
				udelay(100);
		}
		tegra30_ahub_disable_rx_fifo(i2s->rxcif);
	}
}

static int tegra30_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		tegra30_i2s_enable_clocks(i2s);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			tegra30_i2s_start_playback(i2s);
		else
			tegra30_i2s_start_capture(i2s);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			tegra30_i2s_stop_playback(i2s);
		else
			tegra30_i2s_stop_capture(i2s);
		tegra30_i2s_disable_clocks(i2s);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tegra30_i2s_probe(struct snd_soc_dai *dai)
{
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(dai);
#ifdef CONFIG_PM
	int i;
#endif

	dai->capture_dma_data = &i2s->capture_dma_data;
	dai->playback_dma_data = &i2s->playback_dma_data;

#ifdef CONFIG_PM
	tegra30_i2s_enable_clocks(i2s);

	/*cache the POR values of i2s regs*/
	for (i = 0; i < ((TEGRA30_I2S_CIF_TX_CTRL>>2) + 1); i++)
		i2s->reg_cache[i] = tegra30_i2s_read(i2s, i<<2);

	tegra30_i2s_disable_clocks(i2s);
#endif

	/* Default values for DSP mode */
	i2s->dsp_config.num_slots = 1;
	i2s->dsp_config.slot_width = 2;
	i2s->dsp_config.tx_mask = 1;
	i2s->dsp_config.rx_mask = 1;
	i2s->dsp_config.rx_data_offset = 1;
	i2s->dsp_config.tx_data_offset = 1;


	return 0;
}

int tegra30_i2s_set_tdm_slot(struct snd_soc_dai *cpu_dai,
							unsigned int tx_mask,
							unsigned int rx_mask,
							int slots,
							int slot_width)
{
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);

	i2s->dsp_config.num_slots = slots;
	i2s->dsp_config.slot_width = slot_width;
	i2s->dsp_config.tx_mask = tx_mask;
	i2s->dsp_config.rx_mask = rx_mask;
	i2s->dsp_config.rx_data_offset = 0;
	i2s->dsp_config.tx_data_offset = 0;

	return 0;
}

#ifdef CONFIG_PM
int tegra30_i2s_resume(struct snd_soc_dai *cpu_dai)
{
	struct tegra30_i2s *i2s = snd_soc_dai_get_drvdata(cpu_dai);
	int i, ret = 0;

	tegra30_i2s_enable_clocks(i2s);

	/*restore the i2s regs*/
	for (i = 0; i < ((TEGRA30_I2S_CIF_TX_CTRL>>2) + 1); i++)
		tegra30_i2s_write(i2s, i<<2, i2s->reg_cache[i]);

	tegra30_ahub_apbif_resume();

	tegra30_i2s_disable_clocks(i2s);

	if (i2s->dam_ch_refcount)
		ret = tegra30_dam_resume(i2s->dam_ifc);

	return ret;
}
#else
#define tegra30_i2s_resume NULL
#endif

static struct snd_soc_dai_ops tegra30_i2s_dai_ops = {
	.startup	= tegra30_i2s_startup,
	.shutdown	= tegra30_i2s_shutdown,
	.set_fmt	= tegra30_i2s_set_fmt,
	.hw_params	= tegra30_i2s_hw_params,
	.trigger	= tegra30_i2s_trigger,
	.set_tdm_slot = tegra30_i2s_set_tdm_slot,
};

#define TEGRA30_I2S_DAI(id) \
	{ \
		.name = DRV_NAME "." #id, \
		.probe = tegra30_i2s_probe, \
		.resume = tegra30_i2s_resume, \
		.playback = { \
			.channels_min = 1, \
			.channels_max = 16, \
			.rates = SNDRV_PCM_RATE_8000_96000, \
			.formats = SNDRV_PCM_FMTBIT_S8 | \
				   SNDRV_PCM_FMTBIT_S16_LE | \
				   SNDRV_PCM_FMTBIT_S24_LE | \
				   SNDRV_PCM_FMTBIT_S32_LE, \
		}, \
		.capture = { \
			.channels_min = 1, \
			.channels_max = 16, \
			.rates = SNDRV_PCM_RATE_8000_96000, \
			.formats = SNDRV_PCM_FMTBIT_S8 | \
				   SNDRV_PCM_FMTBIT_S16_LE | \
				   SNDRV_PCM_FMTBIT_S24_LE | \
				   SNDRV_PCM_FMTBIT_S32_LE, \
		}, \
		.ops = &tegra30_i2s_dai_ops, \
		.symmetric_rates = 1, \
	}

struct snd_soc_dai_driver tegra30_i2s_dai[] = {
	TEGRA30_I2S_DAI(0),
	TEGRA30_I2S_DAI(1),
	TEGRA30_I2S_DAI(2),
	TEGRA30_I2S_DAI(3),
	TEGRA30_I2S_DAI(4),
};

static int configure_voice_call_clocks(struct codec_config *codec_info,
	int codec_i2sclock, struct codec_config *bb_info, int bb_i2sclock)
{
	struct tegra30_i2s  *codec_i2s;
	struct tegra30_i2s  *bb_i2s;
	int ret;

	codec_i2s = &i2scont[codec_info->i2s_id];
	bb_i2s = &i2scont[bb_info->i2s_id];

	if (bb_info->is_i2smaster && codec_info->is_i2smaster) {
		/* set modem clock */
		ret = clk_set_parent(bb_i2s->clk_i2s, bb_i2s->clk_pll_a_out0);
		if (ret) {
			pr_err("Can't set parent of I2S clock\n");
			return ret;
		}

		ret = clk_set_rate(bb_i2s->clk_i2s, bb_i2sclock);
		if (ret) {
			pr_err("Can't set I2S clock rate: %d\n", ret);
			return ret;
		}
		/* set codec clock */
		ret = clk_set_parent(codec_i2s->clk_i2s,
			codec_i2s->clk_pll_a_out0);
		if (ret) {
			pr_err("Can't set parent of I2S clock\n");
			return ret;
		}

		ret = clk_set_rate(codec_i2s->clk_i2s, codec_i2sclock);
		if (ret) {
			pr_err("Can't set I2S clock rate: %d\n", ret);
			return ret;
		}

	} else if (!bb_info->is_i2smaster && codec_info->is_i2smaster) {

		/* set modem clock */
		ret = clk_set_rate(bb_i2s->clk_i2s_sync, bb_i2sclock);
		if (ret) {
			pr_err("Can't set I2S sync clock rate\n");
			return ret;
		}

		ret = clk_set_parent(clk_get_parent(bb_i2s->clk_audio_2x),
						bb_i2s->clk_i2s_sync);
		if (ret) {
			pr_err("Can't set parent of audiox clock\n");
			return ret;
		}

		ret = clk_set_rate(bb_i2s->clk_audio_2x, bb_i2sclock);
		if (ret) {
			pr_err("Can't set audio2x clock rate\n");
			return ret;
		}

		ret = clk_set_parent(bb_i2s->clk_i2s, bb_i2s->clk_audio_2x);
		if (ret) {
			pr_err("Can't set parent of clk_i2s clock\n");
			return ret;
		}

		/* Modify or ensure the frequency division*/
		ret = clk_set_rate(bb_i2s->clk_i2s, bb_i2sclock);
		if (ret) {
			pr_err("Can't set I2S clock rate: %d\n", ret);
			return ret;
		}

#ifndef CONFIG_ARCH_TEGRA_3x_SOC
		/* set codec clock */
		/* use modem clock to drive codec
		* to avoid sound to being discontinuous */
		ret = clk_set_parent(clk_get_parent(codec_i2s->clk_audio_2x),
						bb_i2s->clk_i2s_sync);
		if (ret) {
			pr_err("Can't set parent of audiox clock\n");
			return ret;
		}

		ret = clk_set_rate(codec_i2s->clk_audio_2x, bb_i2sclock);
		if (ret) {
			pr_err("Can't set audio2x clock rate\n");
			return ret;
		}

		ret = clk_set_parent(codec_i2s->clk_i2s,
			codec_i2s->clk_audio_2x);
		if (ret) {
			pr_err("Can't set parent of clk_i2s clock\n");
			return ret;
		}

		/* Modify or ensure the frequency division*/
		ret = clk_set_rate(codec_i2s->clk_i2s, codec_i2sclock);
		if (ret) {
			pr_err("Can't set I2S clock rate: %d\n", ret);
			return ret;
		}
#else
		/* set codec clock */
		ret = clk_set_parent(codec_i2s->clk_i2s,
			codec_i2s->clk_pll_a_out0);
		if (ret) {
			pr_err("Can't set parent of I2S clock\n");
			return ret;
		}

		ret = clk_set_rate(codec_i2s->clk_i2s, codec_i2sclock);
		if (ret) {
			pr_err("Can't set I2S clock rate: %d\n", ret);
			return ret;
		}
#endif

	} else if (bb_info->is_i2smaster && !codec_info->is_i2smaster) {
		/* Just because by now there is no use case about using Codec's
		 * Clock for Modem when Code is Master and Modem is slave,
		 * I do not add modification about it.
		 * If necessarily, it can be added.*/
		/* set modem clock */
		ret = clk_set_parent(bb_i2s->clk_i2s, bb_i2s->clk_pll_a_out0);
		if (ret) {
			pr_err("Can't set parent of I2S clock\n");
			return ret;
		}

		ret = clk_set_rate(bb_i2s->clk_i2s, bb_i2sclock);
		if (ret) {
			pr_err("Can't set I2S clock rate: %d\n", ret);
			return ret;
		}

		/* set codec clock */
		ret = clk_set_rate(codec_i2s->clk_i2s_sync, codec_i2sclock);
		if (ret) {
			pr_err("Can't set I2S sync clock rate\n");
			return ret;
		}

		ret = clk_set_parent(clk_get_parent(codec_i2s->clk_audio_2x),
						codec_i2s->clk_i2s_sync);
		if (ret) {
			pr_err("Can't set parent of audiox clock\n");
			return ret;
		}

		ret = clk_set_rate(codec_i2s->clk_audio_2x, codec_i2sclock);
		if (ret) {
			pr_err("Can't set audio2x clock rate\n");
			return ret;
		}

		ret = clk_set_parent(codec_i2s->clk_i2s,
			codec_i2s->clk_audio_2x);
		if (ret) {
			pr_err("Can't set parent of clk_i2s clock\n");
			return ret;
		}

		/* Modify or ensure the frequency division*/
		ret = clk_set_rate(codec_i2s->clk_i2s, codec_i2sclock);
		if (ret) {
			pr_err("Can't set I2S clock rate: %d\n", ret);
			return ret;
		}

	} else if (!bb_info->is_i2smaster && !codec_info->is_i2smaster) {
		/* set modem clock */
		ret = clk_set_rate(bb_i2s->clk_i2s_sync, bb_i2sclock);
		if (ret) {
			pr_err("Can't set I2S sync clock rate\n");
			return ret;
		}

		ret = clk_set_parent(clk_get_parent(bb_i2s->clk_audio_2x),
						bb_i2s->clk_i2s_sync);
		if (ret) {
			pr_err("Can't set parent of audiox clock\n");
			return ret;
		}

		ret = clk_set_rate(bb_i2s->clk_audio_2x, bb_i2sclock);
		if (ret) {
			pr_err("Can't set audio2x clock rate\n");
			return ret;
		}

		ret = clk_set_parent(bb_i2s->clk_i2s, bb_i2s->clk_audio_2x);
		if (ret) {
			pr_err("Can't set parent of clk_i2s clock\n");
			return ret;
		}

		/* Modify or ensure the frequency division*/
		ret = clk_set_rate(codec_i2s->clk_i2s, codec_i2sclock);
		if (ret) {
			pr_err("Can't set I2S clock rate: %d\n", ret);
			return ret;
		}

		/* set codec clock */
		ret = clk_set_rate(codec_i2s->clk_i2s_sync, codec_i2sclock);
		if (ret) {
			pr_err("Can't set I2S sync clock rate\n");
			return ret;
		}

		ret = clk_set_parent(clk_get_parent(codec_i2s->clk_audio_2x),
						codec_i2s->clk_i2s_sync);
		if (ret) {
			pr_err("Can't set parent of audiox clock\n");
			return ret;
		}

		ret = clk_set_rate(codec_i2s->clk_audio_2x, codec_i2sclock);
		if (ret) {
			pr_err("Can't set audio2x clock rate\n");
			return ret;
		}

		ret = clk_set_parent(codec_i2s->clk_i2s,
			codec_i2s->clk_audio_2x);
		if (ret) {
			pr_err("Can't set parent of clk_i2s clock\n");
			return ret;
		}

		/* Modify or ensure the frequency division*/
		ret = clk_set_rate(codec_i2s->clk_i2s, codec_i2sclock);
		if (ret) {
			pr_err("Can't set I2S clock rate: %d\n", ret);
			return ret;
		}

	}
	return 0;
}

static int configure_baseband_i2s(struct tegra30_i2s *i2s,
	struct codec_config *i2s_info,
	int i2sclock, int is_formatdsp)
{
	u32 val;
	int bitcnt;
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
	u32  i;
#endif

	tegra30_i2s_enable_clocks(i2s);

	i2s->reg_ctrl &= ~(TEGRA30_I2S_CTRL_FRAME_FORMAT_MASK |
					TEGRA30_I2S_CTRL_LRCK_MASK |
					TEGRA30_I2S_CTRL_MASTER_ENABLE);
	i2s->reg_ch_ctrl &= ~TEGRA30_I2S_CH_CTRL_EGDE_CTRL_MASK;

	i2s->reg_ctrl |= TEGRA30_I2S_CTRL_BIT_SIZE_16;

	if (i2s_info->is_i2smaster)
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_MASTER_ENABLE;

	if (i2s_info->i2s_mode == TEGRA_DAIFMT_DSP_A) {
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_FRAME_FORMAT_FSYNC;
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_LRCK_R_LOW;
		i2s->reg_ch_ctrl |= TEGRA30_I2S_CH_CTRL_EGDE_CTRL_NEG_EDGE;
	} else if (i2s_info->i2s_mode == TEGRA_DAIFMT_DSP_B) {
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_FRAME_FORMAT_FSYNC;
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_LRCK_R_LOW;
		i2s->reg_ch_ctrl |= TEGRA30_I2S_CH_CTRL_EGDE_CTRL_POS_EDGE;
	} else {
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_FRAME_FORMAT_LRCK;
		i2s->reg_ctrl |= TEGRA30_I2S_CTRL_LRCK_L_LOW;
		i2s->reg_ch_ctrl |= TEGRA30_I2S_CH_CTRL_EGDE_CTRL_POS_EDGE;
	}

	tegra30_i2s_write(i2s, TEGRA30_I2S_CH_CTRL, i2s->reg_ch_ctrl);

#ifndef CONFIG_ARCH_TEGRA_3x_SOC
	val = 0;
	for (i = 0; i < i2s_info->channels; i++)
		val |= (1 << i);

	val |= val <<
	  TEGRA30_I2S_SLOT_CTRL2_TX_SLOT_ENABLES_SHIFT;
	val |= val <<
	  TEGRA30_I2S_SLOT_CTRL2_RX_SLOT_ENABLES_SHIFT;
	tegra30_i2s_write(i2s, TEGRA30_I2S_SLOT_CTRL2, val);

	val = 0;
	if (i2s->reg_ctrl & TEGRA30_I2S_CTRL_FRAME_FORMAT_FSYNC)
		val = i2s_info->channels - 1;

	tegra30_i2s_write(i2s, TEGRA30_I2S_SLOT_CTRL, val);
#else
	val = tegra30_i2s_read(i2s, TEGRA30_I2S_SLOT_CTRL);
	val &= ~(TEGRA30_I2S_SLOT_CTRL_TX_SLOT_ENABLES_MASK |
		TEGRA30_I2S_SLOT_CTRL_RX_SLOT_ENABLES_MASK);
	val |= (1 << TEGRA30_I2S_SLOT_CTRL_TX_SLOT_ENABLES_SHIFT |
		1 << TEGRA30_I2S_SLOT_CTRL_RX_SLOT_ENABLES_SHIFT);
	tegra30_i2s_write(i2s, TEGRA30_I2S_SLOT_CTRL, val);
#endif

	val = (1 << TEGRA30_I2S_OFFSET_RX_DATA_OFFSET_SHIFT) |
	      (1 << TEGRA30_I2S_OFFSET_TX_DATA_OFFSET_SHIFT);
	tegra30_i2s_write(i2s, TEGRA30_I2S_OFFSET, val);

	if (is_formatdsp) {
		bitcnt = (i2sclock/i2s_info->rate) - 1;
		val = bitcnt << TEGRA30_I2S_TIMING_CHANNEL_BIT_COUNT_SHIFT;
		if (i2sclock % (i2s_info->rate))
			val |= TEGRA30_I2S_TIMING_NON_SYM_ENABLE;
	} else {
		bitcnt = (i2sclock/(2*i2s_info->rate)) - 1;
		val = bitcnt << TEGRA30_I2S_TIMING_CHANNEL_BIT_COUNT_SHIFT;
		if (i2sclock % (2*i2s_info->rate))
			val |= TEGRA30_I2S_TIMING_NON_SYM_ENABLE;
	}

	tegra30_i2s_write(i2s, TEGRA30_I2S_TIMING, val);

	/* configure the i2s cif*/
	val = (0 << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
		((i2s_info->channels - 1) <<
		TEGRA30_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
		((i2s_info->channels - 1) <<
		TEGRA30_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
	      TEGRA30_AUDIOCIF_CTRL_AUDIO_BITS_16 |
	      TEGRA30_AUDIOCIF_CTRL_CLIENT_BITS_16;
	val |= TEGRA30_AUDIOCIF_CTRL_DIRECTION_RX;
	tegra30_i2s_write(i2s, TEGRA30_I2S_CIF_RX_CTRL, val);

	val &= ~TEGRA30_AUDIOCIF_CTRL_DIRECTION_MASK;
	val |= TEGRA30_AUDIOCIF_CTRL_DIRECTION_TX;
	tegra30_i2s_write(i2s, TEGRA30_I2S_CIF_TX_CTRL, val);

	return 0;
}

static int configure_dam(struct tegra30_i2s  *i2s, int out_channel,
		int out_rate, int out_bitsize, int in_channels,
		int in_rate, int in_bitsize)
{

	if (!i2s->dam_ch_refcount)
		i2s->dam_ifc = tegra30_dam_allocate_controller();

	if (i2s->dam_ifc < 0) {
		pr_err("Error : Failed to allocate DAM controller\n");
		return -ENOENT;
	}
	tegra30_dam_allocate_channel(i2s->dam_ifc, TEGRA30_DAM_CHIN0_SRC);
	i2s->dam_ch_refcount++;
	tegra30_dam_enable_clock(i2s->dam_ifc);
	tegra30_dam_set_samplerate(i2s->dam_ifc, TEGRA30_DAM_CHOUT, out_rate);
	tegra30_dam_set_samplerate(i2s->dam_ifc, TEGRA30_DAM_CHIN0_SRC,
				in_rate);
	tegra30_dam_set_gain(i2s->dam_ifc, TEGRA30_DAM_CHIN0_SRC, 0x1000);
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
	tegra30_dam_set_acif(i2s->dam_ifc, TEGRA30_DAM_CHIN0_SRC,
			in_channels, in_bitsize, 1, 32);
	tegra30_dam_set_acif(i2s->dam_ifc, TEGRA30_DAM_CHOUT,
			out_channel, out_bitsize, out_channel, 32);
#else
	tegra30_dam_set_acif(i2s->dam_ifc, TEGRA30_DAM_CHIN0_SRC,
			in_channels, in_bitsize, 1, 16);
	tegra30_dam_set_acif(i2s->dam_ifc, TEGRA30_DAM_CHOUT,
			out_channel, out_bitsize, out_channel, out_bitsize);
#endif

#ifndef CONFIG_ARCH_TEGRA_3x_SOC
	if (in_rate != out_rate) {
		tegra30_dam_write_coeff_ram(i2s->dam_ifc, in_rate, out_rate);
		tegra30_dam_set_farrow_param(i2s->dam_ifc, in_rate, out_rate);
		tegra30_dam_set_biquad_fixed_coef(i2s->dam_ifc);
		tegra30_dam_enable_coeff_ram(i2s->dam_ifc);
		tegra30_dam_set_filter_stages(i2s->dam_ifc, in_rate, out_rate);
	} else {
		tegra30_dam_enable_stereo_mixing(i2s->dam_ifc);
	}
#endif

	return 0;
}


int tegra30_make_voice_call_connections(struct codec_config *codec_info,
				struct codec_config *bb_info,
				int uses_voice_codec)
{
	struct tegra30_i2s  *codec_i2s;
	struct tegra30_i2s  *bb_i2s;
	int reg, ret;
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
	int val;
#endif
	int bb_i2sclock, bb_is_formatdsp, codec_i2sclock, codec_is_formatdsp;

	codec_i2s = &i2scont[codec_info->i2s_id];
	bb_i2s = &i2scont[bb_info->i2s_id];

	/* increment the codec i2s playback ref count */
	codec_i2s->playback_ref_count++;
	bb_i2s->playback_ref_count++;
	codec_i2s->capture_ref_count++;
	bb_i2s->capture_ref_count++;

	/* Make sure i2s is disabled during the configiration */
	/* Soft reset to make sure DL and UL be not lost*/
	tegra30_i2s_enable_clocks(codec_i2s);
	reg = codec_i2s->reg_ctrl;
	reg &= ~TEGRA30_I2S_CTRL_TX_FLOWCTL_EN;
	reg &= ~TEGRA30_I2S_CTRL_XFER_EN_TX;
	reg &= ~TEGRA30_I2S_CTRL_XFER_EN_RX;
	tegra30_i2s_write(codec_i2s, TEGRA30_I2S_CTRL,
		reg | TEGRA30_I2S_CTRL_SOFT_RESET);
	tegra30_i2s_disable_clocks(codec_i2s);

	tegra30_i2s_enable_clocks(bb_i2s);
	reg = bb_i2s->reg_ctrl;
	reg &= ~TEGRA30_I2S_CTRL_TX_FLOWCTL_EN;
	reg &= ~TEGRA30_I2S_CTRL_XFER_EN_TX;
	reg &= ~TEGRA30_I2S_CTRL_XFER_EN_RX;
	tegra30_i2s_write(bb_i2s, TEGRA30_I2S_CTRL,
		reg | TEGRA30_I2S_CTRL_SOFT_RESET);
	tegra30_i2s_disable_clocks(bb_i2s);

	msleep(20);

	/* get bitclock of modem */
	codec_is_formatdsp = (codec_info->i2s_mode == TEGRA_DAIFMT_DSP_A) ||
			(codec_info->i2s_mode == TEGRA_DAIFMT_DSP_B);

	if (codec_info->bit_clk) {
		codec_i2sclock = codec_info->bit_clk;
	} else {
		codec_i2sclock = codec_info->rate * codec_info->channels *
			codec_info->bitsize * 2;
		/* additional 8 for baseband */
		if (codec_is_formatdsp)
			codec_i2sclock *= 8;
	}

	/* get bitclock of codec */
	bb_is_formatdsp = (bb_info->i2s_mode == TEGRA_DAIFMT_DSP_A) ||
			(bb_info->i2s_mode == TEGRA_DAIFMT_DSP_B);

	if (bb_info->bit_clk) {
		bb_i2sclock = bb_info->bit_clk;
	} else {
		bb_i2sclock = bb_info->rate * bb_info->channels *
			bb_info->bitsize * 2;
		/* additional 8 for baseband */
		if (bb_is_formatdsp)
			bb_i2sclock *= 8;
	}
	/* If we have two modems and one is master device and the other
	* is slave.Audio will be inaduible with the slave modem after
	* using the master modem*/
	configure_voice_call_clocks(codec_info, codec_i2sclock,
		bb_info, bb_i2sclock);

	/* Configure codec i2s */
	configure_baseband_i2s(codec_i2s, codec_info,
		codec_i2sclock,	codec_is_formatdsp);

	/* Configure bb i2s */
	configure_baseband_i2s(bb_i2s, bb_info,
		bb_i2sclock, bb_is_formatdsp);

	if (uses_voice_codec) {
		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_APBIF_RX0 +
			codec_info->i2s_id);
		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_APBIF_RX0 +
			bb_info->i2s_id);

		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_I2S0_RX0 +
			    bb_info->i2s_id, TEGRA30_AHUB_TXCIF_I2S0_TX0 +
			    codec_info->i2s_id);
		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_I2S0_RX0 +
			    codec_info->i2s_id, TEGRA30_AHUB_TXCIF_I2S0_TX0 +
			    bb_info->i2s_id);
	} else {

		/*configure codec dam*/
		ret = configure_dam(codec_i2s,
				    codec_info->channels,
				    codec_info->rate,
				    codec_info->bitsize,
				    bb_info->channels,
				    bb_info->rate,
				    bb_info->bitsize);
		if (ret != 0) {
			pr_err("Error: Failed configure_dam\n");
			return ret;
		}

		/*configure bb dam*/
		ret = configure_dam(bb_i2s,
				    bb_info->channels,
				    bb_info->rate,
				    bb_info->bitsize,
				    codec_info->channels,
				    codec_info->rate,
				    codec_info->bitsize);
		if (ret != 0) {
			pr_err("Error: Failed configure_dam\n");
			return ret;
		}

		/*make ahub connections*/

		/*if this is the only user of i2s tx, make i2s rx connection*/
		if (codec_i2s->playback_ref_count == 1) {
			tegra30_ahub_set_rx_cif_source(
			  TEGRA30_AHUB_RXCIF_I2S0_RX0 + codec_info->i2s_id,
			  TEGRA30_AHUB_TXCIF_DAM0_TX0 + codec_i2s->dam_ifc);
		}

		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_I2S0_RX0 +
				bb_info->i2s_id, TEGRA30_AHUB_TXCIF_DAM0_TX0 +
				bb_i2s->dam_ifc);
		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0 +
			(codec_i2s->dam_ifc*2), TEGRA30_AHUB_TXCIF_I2S0_TX0 +
			bb_info->i2s_id);
		tegra30_ahub_set_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0 +
			(bb_i2s->dam_ifc*2), TEGRA30_AHUB_TXCIF_I2S0_TX0 +
			codec_info->i2s_id);

		/*enable dam and i2s*/
		tegra30_dam_enable(codec_i2s->dam_ifc, TEGRA30_DAM_ENABLE,
			TEGRA30_DAM_CHIN0_SRC);
		tegra30_dam_enable(bb_i2s->dam_ifc, TEGRA30_DAM_ENABLE,
			TEGRA30_DAM_CHIN0_SRC);
	}
#ifndef CONFIG_ARCH_TEGRA_3x_SOC
	tegra30_i2s_write(codec_i2s, TEGRA30_I2S_FLOWCTL, 0);
	tegra30_i2s_write(bb_i2s, TEGRA30_I2S_FLOWCTL, 0);
	tegra30_i2s_write(codec_i2s, TEGRA30_I2S_TX_STEP, 0);
	tegra30_i2s_write(bb_i2s, TEGRA30_I2S_TX_STEP, 0);
	bb_i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_TX_FLOWCTL_EN;
	codec_i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_TX_FLOWCTL_EN;

	if (!bb_info->is_i2smaster && codec_info->is_i2smaster) {
		tegra30_i2s_write(codec_i2s, TEGRA30_I2S_FLOWCTL,
			TEGRA30_I2S_FLOWCTL_FILTER_QUAD |
			4 << TEGRA30_I2S_FLOWCTL_START_SHIFT |
			4 << TEGRA30_I2S_FLOWCTL_HIGH_SHIFT |
			4 << TEGRA30_I2S_FLOWCTL_LOW_SHIFT);
		tegra30_i2s_write(bb_i2s, TEGRA30_I2S_FLOWCTL,
			TEGRA30_I2S_FLOWCTL_FILTER_QUAD |
			4 << TEGRA30_I2S_FLOWCTL_START_SHIFT |
			4 << TEGRA30_I2S_FLOWCTL_HIGH_SHIFT |
			4 << TEGRA30_I2S_FLOWCTL_LOW_SHIFT);
		tegra30_i2s_write(codec_i2s, TEGRA30_I2S_TX_STEP, 4);
		tegra30_i2s_write(bb_i2s, TEGRA30_I2S_TX_STEP, 4);
		codec_i2s->reg_ctrl |= TEGRA30_I2S_CTRL_TX_FLOWCTL_EN;
		bb_i2s->reg_ctrl |= TEGRA30_I2S_CTRL_TX_FLOWCTL_EN;

		val = tegra30_i2s_read(codec_i2s, TEGRA30_I2S_CIF_RX_CTRL);
		val &= ~(0xf << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT);
		val |= (4 << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT);
		tegra30_i2s_write(codec_i2s, TEGRA30_I2S_CIF_RX_CTRL, val);
		val = tegra30_i2s_read(bb_i2s, TEGRA30_I2S_CIF_RX_CTRL);
		val &= ~(0xf << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT);
		val |= (4 << TEGRA30_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT);
		tegra30_i2s_write(bb_i2s, TEGRA30_I2S_CIF_RX_CTRL, val);
	}
#endif

	msleep(20);

	codec_i2s->reg_ctrl |= TEGRA30_I2S_CTRL_XFER_EN_TX;
	codec_i2s->reg_ctrl |= TEGRA30_I2S_CTRL_XFER_EN_RX;
	tegra30_i2s_write(codec_i2s, TEGRA30_I2S_CTRL,
		codec_i2s->reg_ctrl);

	msleep(20);

	bb_i2s->reg_ctrl |= TEGRA30_I2S_CTRL_XFER_EN_TX;
	bb_i2s->reg_ctrl |= TEGRA30_I2S_CTRL_XFER_EN_RX;
	tegra30_i2s_write(bb_i2s, TEGRA30_I2S_CTRL,
		bb_i2s->reg_ctrl);

	return 0;
}

int tegra30_break_voice_call_connections(struct codec_config *codec_info,
				struct codec_config *bb_info,
				int uses_voice_codec)
{
	struct tegra30_i2s  *codec_i2s;
	struct tegra30_i2s  *bb_i2s;
	int dcnt = 10;

	codec_i2s = &i2scont[codec_info->i2s_id];
	bb_i2s = &i2scont[bb_info->i2s_id];

	/*Disable Codec I2S RX (TX to ahub)*/
	if (codec_i2s->capture_ref_count == 1)
		codec_i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_XFER_EN_RX;

	tegra30_i2s_write(codec_i2s, TEGRA30_I2S_CTRL, codec_i2s->reg_ctrl);

	while (!tegra30_ahub_rx_fifo_is_empty(codec_i2s->id) && dcnt--)
		udelay(100);

	dcnt = 10;

	/*Disable baseband I2S TX (RX from ahub)*/
	bb_i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_XFER_EN_TX;
	tegra30_i2s_write(bb_i2s, TEGRA30_I2S_CTRL, bb_i2s->reg_ctrl);

	while (!tegra30_ahub_tx_fifo_is_empty(bb_i2s->id) && dcnt--)
		udelay(100);

	dcnt = 10;

	/*Disable baseband I2S RX (TX to ahub)*/
	bb_i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_XFER_EN_RX;
	tegra30_i2s_write(bb_i2s, TEGRA30_I2S_CTRL, bb_i2s->reg_ctrl);

	while (!tegra30_ahub_rx_fifo_is_empty(bb_i2s->id) && dcnt--)
		udelay(100);

	dcnt = 10;

	/*Disable Codec I2S TX (RX from ahub)*/
	if (codec_i2s->playback_ref_count == 1)
			codec_i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_XFER_EN_TX;

	tegra30_i2s_write(codec_i2s, TEGRA30_I2S_CTRL, codec_i2s->reg_ctrl);

	while (!tegra30_ahub_tx_fifo_is_empty(codec_i2s->id) && dcnt--)
		udelay(100);

	dcnt = 10;

	if (uses_voice_codec) {
		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_I2S0_RX0 +
			    bb_info->i2s_id);
		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_I2S0_RX0 +
			    codec_info->i2s_id);
	} else {

		/*Disable baseband DAM*/
		tegra30_dam_enable(bb_i2s->dam_ifc, TEGRA30_DAM_DISABLE,
				TEGRA30_DAM_CHIN0_SRC);
		tegra30_dam_free_channel(bb_i2s->dam_ifc,
				TEGRA30_DAM_CHIN0_SRC);
		bb_i2s->dam_ch_refcount--;
		if (!bb_i2s->dam_ch_refcount)
			tegra30_dam_free_controller(bb_i2s->dam_ifc);

		/*Disable Codec DAM*/
		tegra30_dam_enable(codec_i2s->dam_ifc,
			TEGRA30_DAM_DISABLE, TEGRA30_DAM_CHIN0_SRC);
		tegra30_dam_free_channel(codec_i2s->dam_ifc,
			TEGRA30_DAM_CHIN0_SRC);
		codec_i2s->dam_ch_refcount--;
		if (!codec_i2s->dam_ch_refcount)
			tegra30_dam_free_controller(codec_i2s->dam_ifc);

		/* Disconnect the ahub connections */
		/* If this is the only user of i2s tx then break ahub
		i2s rx connection */
		if (codec_i2s->playback_ref_count == 1)
			tegra30_ahub_unset_rx_cif_source(
			   TEGRA30_AHUB_RXCIF_I2S0_RX0 + codec_info->i2s_id);

		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_I2S0_RX0
					+ bb_info->i2s_id);
		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0
					+ (codec_i2s->dam_ifc*2));
		tegra30_ahub_unset_rx_cif_source(TEGRA30_AHUB_RXCIF_DAM0_RX0
					+ (bb_i2s->dam_ifc*2));

		tegra30_dam_disable_clock(codec_i2s->dam_ifc);
		tegra30_dam_disable_clock(bb_i2s->dam_ifc);
	}

	/* Decrement the codec and bb i2s playback ref count */
	codec_i2s->playback_ref_count--;
	bb_i2s->playback_ref_count--;
	codec_i2s->capture_ref_count--;
	bb_i2s->capture_ref_count--;

	/* Soft reset */
	tegra30_i2s_write(codec_i2s, TEGRA30_I2S_CTRL,
		codec_i2s->reg_ctrl | TEGRA30_I2S_CTRL_SOFT_RESET);
	tegra30_i2s_write(bb_i2s, TEGRA30_I2S_CTRL,
		bb_i2s->reg_ctrl | TEGRA30_I2S_CTRL_SOFT_RESET);

	codec_i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_TX_FLOWCTL_EN;
	bb_i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_TX_FLOWCTL_EN;
	codec_i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_SOFT_RESET;
	bb_i2s->reg_ctrl &= ~TEGRA30_I2S_CTRL_SOFT_RESET;

	while ((tegra30_i2s_read(codec_i2s, TEGRA30_I2S_CTRL) &
			TEGRA30_I2S_CTRL_SOFT_RESET)  && dcnt--)
		udelay(100);
	dcnt = 10;
	while ((tegra30_i2s_read(bb_i2s, TEGRA30_I2S_CTRL) &
			TEGRA30_I2S_CTRL_SOFT_RESET)  && dcnt--)
		udelay(100);

	/* Disable the clocks */
	tegra30_i2s_disable_clocks(codec_i2s);
	tegra30_i2s_disable_clocks(bb_i2s);

	return 0;
}

static __devinit int tegra30_i2s_platform_probe(struct platform_device *pdev)
{
	struct tegra30_i2s *i2s;
	struct resource *mem, *memregion;
	int ret;

	if ((pdev->id < 0) ||
		(pdev->id >= ARRAY_SIZE(tegra30_i2s_dai))) {
		dev_err(&pdev->dev, "ID %d out of range\n", pdev->id);
		return -EINVAL;
	}

	i2s = &i2scont[pdev->id];
	dev_set_drvdata(&pdev->dev, i2s);
	i2s->id = pdev->id;

	i2s->clk_i2s = clk_get(&pdev->dev, "i2s");
	if (IS_ERR(i2s->clk_i2s)) {
		dev_err(&pdev->dev, "Can't retrieve i2s clock\n");
		ret = PTR_ERR(i2s->clk_i2s);
		goto exit;
	}
	i2s->clk_i2s_sync = clk_get(&pdev->dev, "ext_audio_sync");
	if (IS_ERR(i2s->clk_i2s_sync)) {
		dev_err(&pdev->dev, "Can't retrieve i2s_sync clock\n");
		ret = PTR_ERR(i2s->clk_i2s_sync);
		goto err_i2s_clk_put;
	}

	i2s->clk_audio_2x = clk_get(&pdev->dev, "audio_sync_2x");
	if (IS_ERR(i2s->clk_audio_2x)) {
		dev_err(&pdev->dev, "Can't retrieve audio 2x clock\n");
		ret = PTR_ERR(i2s->clk_audio_2x);
		goto err_i2s_sync_clk_put;
	}

	i2s->clk_pll_a_out0 = clk_get_sys(NULL, "pll_a_out0");
	if (IS_ERR(i2s->clk_pll_a_out0)) {
		dev_err(&pdev->dev, "Can't retrieve pll_a_out0 clock\n");
		ret = PTR_ERR(i2s->clk_pll_a_out0);
		goto err_audio_2x_clk_put;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "No memory resource\n");
		ret = -ENODEV;
		goto err_pll_a_out0_clk_put;
	}

	memregion = request_mem_region(mem->start, resource_size(mem),
					DRV_NAME);
	if (!memregion) {
		dev_err(&pdev->dev, "Memory region already claimed\n");
		ret = -EBUSY;
		goto err_pll_a_out0_clk_put;
	}

	i2s->regs = ioremap(mem->start, resource_size(mem));
	if (!i2s->regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_release;
	}

	ret = snd_soc_register_dai(&pdev->dev, &tegra30_i2s_dai[pdev->id]);
	if (ret) {
		dev_err(&pdev->dev, "Could not register DAI: %d\n", ret);
		ret = -ENOMEM;
		goto err_unmap;
	}

	tegra30_i2s_debug_add(i2s, pdev->id);

	return 0;

err_unmap:
	iounmap(i2s->regs);
err_release:
	release_mem_region(mem->start, resource_size(mem));
err_pll_a_out0_clk_put:
	clk_put(i2s->clk_pll_a_out0);
err_audio_2x_clk_put:
	clk_put(i2s->clk_audio_2x);
err_i2s_sync_clk_put:
	clk_put(i2s->clk_i2s_sync);
err_i2s_clk_put:
	clk_put(i2s->clk_i2s);
exit:
	return ret;
}

static int __devexit tegra30_i2s_platform_remove(struct platform_device *pdev)
{
	struct tegra30_i2s *i2s = dev_get_drvdata(&pdev->dev);
	struct resource *res;

	snd_soc_unregister_dai(&pdev->dev);

	tegra30_i2s_debug_remove(i2s);

	iounmap(i2s->regs);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, resource_size(res));

	clk_put(i2s->clk_pll_a_out0);
	clk_put(i2s->clk_audio_2x);
	clk_put(i2s->clk_i2s_sync);
	clk_put(i2s->clk_i2s);

	kfree(i2s);

	return 0;
}

static struct platform_driver tegra30_i2s_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = tegra30_i2s_platform_probe,
	.remove = __devexit_p(tegra30_i2s_platform_remove),
};

static int __init snd_tegra30_i2s_init(void)
{
	return platform_driver_register(&tegra30_i2s_driver);
}
module_init(snd_tegra30_i2s_init);

static void __exit snd_tegra30_i2s_exit(void)
{
	platform_driver_unregister(&tegra30_i2s_driver);
}
module_exit(snd_tegra30_i2s_exit);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra 30 I2S ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
