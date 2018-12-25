/*
 * Mediatek ALSA SoC AFE platform driver for 2701
 *
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Garlic Tseng <garlic.tseng@mediatek.com>
 *	     Ir Lian <ir.lian@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>

#include "mt2701-afe-common.h"

#include "mt2701-afe-clock-ctrl.h"
#include "../common/mtk-afe-platform-driver.h"
#include "../common/mtk-afe-fe-dai.h"

#define AFE_IRQ_STATUS_BITS	0xff

static const struct snd_pcm_hardware mt2701_afe_hardware = {
	.info = SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED
		| SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID,
	.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE
		| SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min = 16,
	.period_bytes_max = 1024 * 256,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = 1 * 1024 * 1024,
	.fifo_size = 0,
};

struct mt2701_afe_rate {
	unsigned int rate;
	unsigned int regvalue;
};

struct snd_soc_dai_driver *find_dai_driver_by_id(struct snd_soc_dai_driver *dai_list,
		int array_size, int id)
{
	int i;

	for (i = 0; i < array_size; i++) {
		if (dai_list[i].id == (unsigned int)id)
			return dai_list+i;
	}
	return 0;
}

static const struct mt2701_afe_rate mt2701_afe_i2s_rates[] = {
	{ .rate = 8000, .regvalue = 0 },
	{ .rate = 12000, .regvalue = 1 },
	{ .rate = 16000, .regvalue = 2 },
	{ .rate = 24000, .regvalue = 3 },
	{ .rate = 32000, .regvalue = 4 },
	{ .rate = 48000, .regvalue = 5 },
	{ .rate = 96000, .regvalue = 6 },
	{ .rate = 192000, .regvalue = 7 },
	{ .rate = 384000, .regvalue = 8 },
	{ .rate = 7350, .regvalue = 16 },
	{ .rate = 11025, .regvalue = 17 },
	{ .rate = 14700, .regvalue = 18 },
	{ .rate = 22050, .regvalue = 19 },
	{ .rate = 29400, .regvalue = 20 },
	{ .rate = 44100, .regvalue = 21 },
	{ .rate = 88200, .regvalue = 22 },
	{ .rate = 176400, .regvalue = 23 },
	{ .rate = 352800, .regvalue = 24 },
};

static int mt2701_dai_num_to_i2s(struct mtk_base_afe *afe, int num)
{
	int val = num - MT2701_IO_I2S;

	if (val < 0 || val >= MT2701_I2S_NUM) {
		dev_err(afe->dev, "%s, num not available, num %d, val %d\n",
			__func__, num, val);
		return -EINVAL;
	}
	return val;
}

static int mt2701_afe_i2s_fs(unsigned int sample_rate)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(mt2701_afe_i2s_rates); i++)
		if (mt2701_afe_i2s_rates[i].rate == sample_rate)
			return mt2701_afe_i2s_rates[i].regvalue;

	return -EINVAL;
}
static int mt2701_afe_i2s_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int i2s_num = mt2701_dai_num_to_i2s(afe, dai->id);

	/* enable mclk */
	if (of_device_is_compatible(afe->dev->of_node, "mediatek,mt2701-audio") != 0)
		return afe_priv->clk_ctrl->enable_mclk(afe, i2s_num);
	else if (afe_priv->i2s_path[i2s_num].i2s_mode == I2S_COCLK) {
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			afe_priv->clk_ctrl->enable_mclk(afe, i2s_num);
			return afe_priv->clk_ctrl->enable_mclk(afe, i2s_num + MCLK_I2SIN_OFFSET);
		} else
			return afe_priv->clk_ctrl->enable_mclk(afe, i2s_num);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			return afe_priv->clk_ctrl->enable_mclk(afe, i2s_num + MCLK_I2SIN_OFFSET);
		else
			return afe_priv->clk_ctrl->enable_mclk(afe, i2s_num);
	}
}

static int mt2701_afe_i2s_path_shutdown(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai,
					int dir_invert)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int i2s_num = mt2701_dai_num_to_i2s(afe, dai->id);
	struct mt2701_i2s_path *i2s_path;
	const struct mt2701_i2s_data *i2s_data;
	int stream_dir = substream->stream;

	if (i2s_num < 0)
		return i2s_num;

	i2s_path = &afe_priv->i2s_path[i2s_num];

	if (dir_invert != 0)	{
		if (stream_dir == SNDRV_PCM_STREAM_PLAYBACK)
			stream_dir = SNDRV_PCM_STREAM_CAPTURE;
		else
			stream_dir = SNDRV_PCM_STREAM_PLAYBACK;
	}
	i2s_data = i2s_path->i2s_data[stream_dir];

	i2s_path->on[stream_dir]--;
	if (i2s_path->on[stream_dir] < 0) {
		dev_warn(afe->dev, "i2s_path->on: %d, dir: %d\n",
			 i2s_path->on[stream_dir], stream_dir);
		i2s_path->on[stream_dir] = 0;
	}
	if (i2s_path->on[stream_dir] != 0)
		return 0;

	/* disable i2s */
	mt2701_regmap_update_bits(afe->regmap, i2s_data->i2s_ctrl_reg,
			   ASYS_I2S_CON_I2S_EN, 0U);
	mt2701_regmap_update_bits(afe->regmap, AUDIO_TOP_CON4,
			   1U << i2s_data->i2s_pwn_shift,
			   1U << i2s_data->i2s_pwn_shift);
	return 0;
}

static void mt2701_afe_i2s_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int i2s_num = mt2701_dai_num_to_i2s(afe, dai->id);
	struct mt2701_i2s_path *i2s_path;
	int ret;

	if (i2s_num < 0)
		return;

	i2s_path = &afe_priv->i2s_path[i2s_num];

	if (i2s_path->occupied[substream->stream] != 0)
		i2s_path->occupied[substream->stream] = 0;
	else
		goto I2S_UNSTART;

	ret = mt2701_afe_i2s_path_shutdown(substream, dai, 0);
	if (ret != 0)
		dev_info(afe->dev, "mt2701_afe_i2s_path_shutdown fail %d", ret);
	if (afe_priv->i2s_path[i2s_num].i2s_mode == I2S_COCLK) {
		/* need to disable i2s-out path when disable i2s-in */
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			ret = mt2701_afe_i2s_path_shutdown(substream, dai, 1);
			if (ret != 0)
				dev_info(afe->dev, "mt2701_afe_i2s_path_shutdown fail %d", ret);
		}
	}

I2S_UNSTART:
	/* disable mclk */
	if (of_device_is_compatible(afe->dev->of_node, "mediatek,mt2701-audio") != 0)
		return afe_priv->clk_ctrl->disable_mclk(afe, i2s_num);
	else if (afe_priv->i2s_path[i2s_num].i2s_mode == I2S_COCLK) {
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			afe_priv->clk_ctrl->disable_mclk(afe, i2s_num);
			return afe_priv->clk_ctrl->disable_mclk(afe, i2s_num + MCLK_I2SIN_OFFSET);
		} else
			return afe_priv->clk_ctrl->disable_mclk(afe, i2s_num);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			return afe_priv->clk_ctrl->disable_mclk(afe, i2s_num + MCLK_I2SIN_OFFSET);
		else
			return afe_priv->clk_ctrl->disable_mclk(afe, i2s_num);
	}
}

static int mt2701_afe_i2s_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int i2s_num = mt2701_dai_num_to_i2s(afe, dai->id);
	struct mt2701_i2s_path *i2s_path;
	const struct mt2701_i2s_data *i2s_data;
	int stream_dir = substream->stream;
	unsigned int mask = 0, val = 0;

	if (i2s_num < 0)
		return i2s_num;

	i2s_path = &afe_priv->i2s_path[i2s_num];
	i2s_data = i2s_path->i2s_data[stream_dir];

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		val = ASYS_I2S_CON_WIDE_MODE_SET(0U);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		val = ASYS_I2S_CON_WIDE_MODE_SET(1U);
		break;
	default:
		val = ASYS_I2S_CON_WIDE_MODE_SET(0U);
		break;
	}
	mask = ASYS_I2S_CON_WIDE_MODE;

	mt2701_regmap_update_bits(afe->regmap, i2s_data->i2s_ctrl_reg, mask, val);

	/* for 4-pin mode, use output clock */
	if (stream_dir == SNDRV_PCM_STREAM_CAPTURE
		&& afe_priv->i2s_path[i2s_num].i2s_mode == I2S_COCLK) {
		i2s_data = i2s_path->i2s_data[SNDRV_PCM_STREAM_PLAYBACK];
		mt2701_regmap_update_bits(afe->regmap, i2s_data->i2s_ctrl_reg, mask, val);
	}

	return 0;
}

static int mt2701_i2s_path_prepare_enable(struct snd_pcm_substream *substream,
					  struct snd_soc_dai *dai,
					  int dir_invert)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int i2s_num = mt2701_dai_num_to_i2s(afe, dai->id);
	struct mt2701_i2s_path *i2s_path;
	const struct mt2701_i2s_data *i2s_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	unsigned int reg, fs;
	int stream_dir = substream->stream;
	unsigned int mask = 0, val = 0;

	if (i2s_num < 0)
		return i2s_num;

	i2s_path = &afe_priv->i2s_path[i2s_num];

	if (dir_invert != 0) {
		if (stream_dir == SNDRV_PCM_STREAM_PLAYBACK)
			stream_dir = SNDRV_PCM_STREAM_CAPTURE;
		else
			stream_dir = SNDRV_PCM_STREAM_PLAYBACK;
	}
	i2s_data = i2s_path->i2s_data[stream_dir];

	/* no need to enable if already done */
	i2s_path->on[stream_dir]++;

	if (i2s_path->on[stream_dir] != 1)
		return 0;

	fs = (unsigned int)mt2701_afe_i2s_fs(runtime->rate);

	mask = ASYS_I2S_CON_FS |
	       ASYS_I2S_CON_I2S_COUPLE_MODE;
	val = ASYS_I2S_CON_FS_SET(fs);

	if (stream_dir == SNDRV_PCM_STREAM_CAPTURE) {
		mask |= ASYS_I2S_IN_PHASE_FIX;
		val |= ASYS_I2S_IN_PHASE_FIX;
	}

	mt2701_regmap_update_bits(afe->regmap, i2s_data->i2s_ctrl_reg, mask, val);
	mt2701_regmap_update_bits(afe->regmap, i2s_path->i2s_data[0]->i2s_ctrl_reg,
		ASYS_I2S_CON_I2S_COUPLE_MODE, 0U);

	if (stream_dir == SNDRV_PCM_STREAM_PLAYBACK)
		reg = ASMO_TIMING_CON1;
	else
		reg = ASMI_TIMING_CON1;

	mt2701_regmap_update_bits(afe->regmap, reg,
			   i2s_data->i2s_asrc_fs_mask
			   << i2s_data->i2s_asrc_fs_shift,
			   fs << i2s_data->i2s_asrc_fs_shift);

	/* enable i2s */
	mt2701_regmap_update_bits(afe->regmap, AUDIO_TOP_CON4,
			   1U << i2s_data->i2s_pwn_shift,
			   0U << i2s_data->i2s_pwn_shift);

	/* reset i2s hw status before enable */
	mt2701_regmap_update_bits(afe->regmap, i2s_data->i2s_ctrl_reg,
			   ASYS_I2S_CON_RESET, ASYS_I2S_CON_RESET);
	udelay(1U);
	mt2701_regmap_update_bits(afe->regmap, i2s_data->i2s_ctrl_reg,
			   ASYS_I2S_CON_RESET, 0U);
	udelay(1U);
	mt2701_regmap_update_bits(afe->regmap, i2s_data->i2s_ctrl_reg,
			   ASYS_I2S_CON_I2S_EN, ASYS_I2S_CON_I2S_EN);
	return 0;
}

static int mt2701_afe_i2s_prepare(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	int clk_domain;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int i2s_num = mt2701_dai_num_to_i2s(afe, dai->id);
	struct mt2701_i2s_path *i2s_path;
	struct clock_ctrl *clk_ctrl = afe_priv->clk_ctrl;
	unsigned int mclk_rate;
	int ret;

	if (i2s_num < 0)
		return i2s_num;

	i2s_path = &afe_priv->i2s_path[i2s_num];
	mclk_rate = i2s_path->mclk_rate;

	if (i2s_path->occupied[substream->stream] != 0)
		return -EBUSY;
	i2s_path->occupied[substream->stream] = 1;

	if (clk_ctrl->apll0_rate % mclk_rate == 0U) {
		clk_domain = 0;
	} else if (clk_ctrl->apll1_rate % mclk_rate == 0U) {
		clk_domain = 1;
	} else {
		dev_err(dai->dev, "%s() bad mclk rate %d\n",
			__func__, mclk_rate);
		return -EINVAL;
	}

	if (of_device_is_compatible(afe->dev->of_node, "mediatek,mt2701-audio") != 0)
		clk_ctrl->mclk_configuration(afe, i2s_num, clk_domain, mclk_rate);
	else if (afe_priv->i2s_path[i2s_num].i2s_mode == I2S_COCLK) {
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			clk_ctrl->mclk_configuration(afe, i2s_num, clk_domain, mclk_rate);
			clk_ctrl->mclk_configuration(afe, i2s_num + MCLK_I2SIN_OFFSET, clk_domain, mclk_rate);
		} else
			clk_ctrl->mclk_configuration(afe, i2s_num, clk_domain, mclk_rate);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			clk_ctrl->mclk_configuration(afe, i2s_num + MCLK_I2SIN_OFFSET, clk_domain, mclk_rate);
		else
			clk_ctrl->mclk_configuration(afe, i2s_num, clk_domain, mclk_rate);
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK
		|| afe_priv->i2s_path[i2s_num].i2s_mode == I2S_SEPCLK) {
		ret = mt2701_i2s_path_prepare_enable(substream, dai, 0);
	} else {
		/* need to enable i2s-out path when enable i2s-in */
		/* prepare for another direction "out" */
		ret = mt2701_i2s_path_prepare_enable(substream, dai, 1);
		/* prepare for "in" */
		if (ret != 0)
			goto err;
		ret = mt2701_i2s_path_prepare_enable(substream, dai, 0);
	}
err:
	return ret;
}

static int mt2701_afe_i2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int i2s_num = mt2701_dai_num_to_i2s(afe, dai->id);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	const struct mt2701_i2s_data *i2s_data_out =
		afe_priv->i2s_path[i2s_num].i2s_data[I2S_OUT];
	const struct mt2701_i2s_data *i2s_data_in =
		afe_priv->i2s_path[i2s_num].i2s_data[I2S_IN];
	unsigned int mask = 0, val = 0;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:	/* I2S */
	case SND_SOC_DAIFMT_DSP_A:
		val |= ASYS_I2S_CON_I2S_MODE_SET(1U);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		val |= ASYS_I2S_CON_RIGHT_J_SET(1U);
		val |= ASYS_I2S_CON_I2S_MODE_SET(0U);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		val |= ASYS_I2S_CON_RIGHT_J_SET(0U);
		val |= ASYS_I2S_CON_I2S_MODE_SET(0U);
		break;
	case SND_SOC_DAIFMT_DSP_B:	/* EIAJ */
		val |= ASYS_I2S_CON_I2S_MODE_SET(0U);
		break;
	default:
		val |= ASYS_I2S_CON_I2S_MODE_SET(1U);
		break;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		val |= ASYS_I2S_CON_INV_BCK_SET(0U) | ASYS_I2S_CON_INV_LRCK_SET(0U);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		val |= ASYS_I2S_CON_INV_BCK_SET(0U) | ASYS_I2S_CON_INV_LRCK_SET(1U);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		val |= ASYS_I2S_CON_INV_BCK_SET(1U) | ASYS_I2S_CON_INV_LRCK_SET(0U);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		val |= ASYS_I2S_CON_INV_BCK_SET(1U) | ASYS_I2S_CON_INV_LRCK_SET(1U);
		break;
	default:
		val |= ASYS_I2S_CON_INV_BCK_SET(0U) | ASYS_I2S_CON_INV_LRCK_SET(0U);
		break;
	}

	mask = ASYS_I2S_CON_I2S_MODE | ASYS_I2S_CON_RIGHT_J |
		   ASYS_I2S_CON_INV_BCK | ASYS_I2S_CON_INV_LRCK;
	mt2701_regmap_update_bits(afe->regmap, i2s_data_out->i2s_ctrl_reg, mask, val);
	mt2701_regmap_update_bits(afe->regmap, i2s_data_in->i2s_ctrl_reg, mask, val);

	return 0;
}

static int mt2701_afe_i2s_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				     unsigned int freq, int dir)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dai->dev);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int i2s_num = mt2701_dai_num_to_i2s(afe, dai->id);

	if (i2s_num < 0)
		return i2s_num;

	/* mclk */
	if (dir == SND_SOC_CLOCK_IN) {
		dev_warn(dai->dev,
			 "%s() warning: mt2701 doesn't support mclk input\n",
			__func__);
		return -EINVAL;
	}
	afe_priv->i2s_path[i2s_num].mclk_rate = freq;
	return 0;
}

static int mt2701_btmrg_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	mt2701_regmap_update_bits(afe->regmap, AUDIO_TOP_CON4,
			   AUDIO_TOP_CON4_PDN_MRGIF, 0);

	afe_priv->mrg_enable[substream->stream] = 1;
	return 0;
}

static int mt2701_btmrg_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int stream_fs;
	u32 val, msk;

	stream_fs = params_rate(params);

	if ((stream_fs != 8000) && (stream_fs != 16000)) {
		dev_err(afe->dev, "%s() btmgr not supprt this stream_fs %d\n",
			__func__, stream_fs);
		return -EINVAL;
	}

	mt2701_regmap_update_bits(afe->regmap, AFE_MRGIF_CON,
			   AFE_MRGIF_CON_I2S_MODE_MASK,
			   AFE_MRGIF_CON_I2S_MODE_32K);

	val = AFE_DAIBT_CON0_BT_FUNC_EN | AFE_DAIBT_CON0_BT_FUNC_RDY
	      | AFE_DAIBT_CON0_MRG_USE;
	msk = val;

	if (stream_fs == 16000)
		val |= AFE_DAIBT_CON0_BT_WIDE_MODE_EN;

	msk |= AFE_DAIBT_CON0_BT_WIDE_MODE_EN;

	mt2701_regmap_update_bits(afe->regmap, AFE_DAIBT_CON0, msk, val);

	mt2701_regmap_update_bits(afe->regmap, AFE_DAIBT_CON0,
			   AFE_DAIBT_CON0_DAIBT_EN,
			   AFE_DAIBT_CON0_DAIBT_EN);
	mt2701_regmap_update_bits(afe->regmap, AFE_MRGIF_CON,
			   AFE_MRGIF_CON_MRG_I2S_EN,
			   AFE_MRGIF_CON_MRG_I2S_EN);
	mt2701_regmap_update_bits(afe->regmap, AFE_MRGIF_CON,
			   AFE_MRGIF_CON_MRG_EN,
			   AFE_MRGIF_CON_MRG_EN);
	return 0;
}

static void mt2701_btmrg_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	/* if the other direction stream is not occupied */
	if (!afe_priv->mrg_enable[!substream->stream]) {
		mt2701_regmap_update_bits(afe->regmap, AFE_DAIBT_CON0,
				   AFE_DAIBT_CON0_DAIBT_EN, 0);
		mt2701_regmap_update_bits(afe->regmap, AFE_MRGIF_CON,
				   AFE_MRGIF_CON_MRG_EN, 0);
		mt2701_regmap_update_bits(afe->regmap, AFE_MRGIF_CON,
				   AFE_MRGIF_CON_MRG_I2S_EN, 0);
		mt2701_regmap_update_bits(afe->regmap, AUDIO_TOP_CON4,
				   AUDIO_TOP_CON4_PDN_MRGIF,
				   AUDIO_TOP_CON4_PDN_MRGIF);
	}
	afe_priv->mrg_enable[substream->stream] = 0;
}

static unsigned int pcmi_palette(unsigned int fs)
{
	unsigned int ret;

	switch (fs) {
	case 8000U:
		ret = 0x050000U;
		break;
	case 16000U:
		ret = 0x0A0000U;
		break;
	case 32000U:
		ret = 0x140000U;
		break;
	case 48000U:
		ret = 0x1E0000U;
		break;
	default:
		ret = 0U;
		break;
	}
	return ret;
}

static unsigned int pcmo_palette(unsigned int fs)
{
	unsigned int ret;

	switch (fs) {
	case 8000U:
		ret = 0x060000U;
		break;
	case 16000U:
		ret = 0x030000U;
		break;
	case 32000U:
		ret =  0x018000U;
		break;
	case 48000U:
		ret =  0x010000U;
		break;
	default:
		ret = 0U;
		break;
	}
	return ret;
}

static unsigned int auto_rst_th_lo(unsigned int fs)
{
	unsigned int ret;

	switch (fs) {
	case 8000U:
		ret = 0x05A000U;
		break;
	case 16000U:
		ret = 0x02d000U;
		break;
	case 32000U:
		ret = 0x016000U;
		break;
	case 48000U:
		ret = 0x00f000U;
		break;
	default:
		ret = 0x0U;
		break;
	}
	return ret;
}

static unsigned int auto_rst_th_hi(unsigned int fs)
{
	unsigned int ret;

	switch (fs) {
	case 8000U:
		ret = 0x066000U;
		break;
	case 16000U:
		ret = 0x033000U;
		break;
	case 32000U:
		ret = 0x01a000U;
		break;
	case 48000U:
		ret = 0x011000U;
		break;
	default:
		ret = 0x0U;
		break;
	}
	return ret;
}

static int mt2701_modpcm_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	mt2701_regmap_update_bits(afe->regmap, AUDIO_TOP_CON4,
			   AUDIO_TOP_CON4_PDN_PCM, 0U);

	afe_priv->pcm_enable[substream->stream] = 1;
	return 0;
}

static int mt2701_modpcm_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	unsigned int stream_fs, channel;
	int bit_width;
	unsigned int mask = 0, val = 0;

	stream_fs = params_rate(params);
	bit_width = params_width(params);
	channel = params_channels(params);

	mask = AFE_PCM_MODE | AFE_PCM_WLEN | AFE_PCM_EX_MODEM | AFE_PCM_24BIT;
	val = AFE_PCM_EX_MODEM_SET(1U);
	switch (stream_fs) {
	case 8000:
		val |= AFE_PCM_MODE_SET(0U);
		break;
	case 16000:
		val |= AFE_PCM_MODE_SET(1U);
		break;
	case 32000:
		val |= AFE_PCM_MODE_SET(2U);
		break;
	case 48000:
		val |= AFE_PCM_MODE_SET(3U);
		break;
	default:
		dev_err(afe->dev, "%s() btmgr not supprt this stream_fs %d\n",
					__func__, stream_fs);
		return -EINVAL;
	}

	if (bit_width == 16)
		val |= AFE_PCM_WLEN_SET(0U) | AFE_PCM_24BIT_SET(0U);
	else
		val |= AFE_PCM_WLEN_SET(1U) | AFE_PCM_24BIT_SET(1U);

	mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1, mask, val);
	mt2701_regmap_update_bits(afe->regmap, AFE_SGEN_CON0,
			   0xffffffffU, 0xf8000000U);

	if (afe_priv->pcm_slave) {
		/* set ASRC */
		mask = AFE_PCM_ASRC_O16BIT | AFE_PCM_ASRC_MONO | AFE_PCM_ASRC_OFS
		       | AFE_PCM_ASRC_IFS | AFE_PCM_ASRC_IIR;
		if (bit_width == 16)
			val = AFE_PCM_ASRC_O16BIT_SET(1U);
		else
			val = AFE_PCM_ASRC_O16BIT_SET(0U);

		if (channel == 1U)
			val |= AFE_PCM_ASRC_MONO_SET(1U);
		else
			val |= AFE_PCM_ASRC_MONO_SET(0U);

		val |= AFE_PCM_ASRC_IIR_SET(0U);

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			/* set Tx ASRC */
			val |= AFE_PCM_ASRC_OFS_SET(0U) | AFE_PCM_ASRC_IFS_SET(3U);

			mt2701_regmap_update_bits(afe->regmap, AUDIO_TOP_CON4,
					AUDIO_TOP_CON4_PDN_ASRC_PCMO, 0);
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMO_CON0, mask, val);
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMO_CON4,
					AFE_PCM_ASRC_PALETTE, AFE_PCM_ASRC_PALETTE_SET(pcmo_palette(stream_fs)));
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMO_CON1,
					AFE_PCM_ASRC_PALETTE, AFE_PCM_ASRC_PALETTE_SET(pcmo_palette(stream_fs)));
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMO_CON6, 0xffffffffU, 0x003f988fU);
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMO_CON7, 0xffffffffU, 0x00003c00U);
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMO_CON13,
					AFE_PCM_ASRC_TH, AFE_PCM_ASRC_TH_SET(auto_rst_th_hi(stream_fs)));
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMO_CON14,
					AFE_PCM_ASRC_TH, AFE_PCM_ASRC_TH_SET(auto_rst_th_lo(stream_fs)));

			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMO_CON0,
					AFE_PCM_ASRC_CLR, AFE_PCM_ASRC_CLR);
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMO_CON0,
					AFE_PCM_ASRC_CLR | AFE_PCM_ASRC_EN, AFE_PCM_ASRC_CLR | AFE_PCM_ASRC_EN_SET(1U));

		} else {
			/* set Rx ASRC */
			val |= AFE_PCM_ASRC_OFS_SET(1U) | AFE_PCM_ASRC_IFS_SET(2U);

			mt2701_regmap_update_bits(afe->regmap, AUDIO_TOP_CON4,
					AUDIO_TOP_CON4_PDN_ASRC_PCMI, 0);
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMI_CON0, mask, val);
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMI_CON3, AFE_PCM_ASRC_PALETTE,
						   AFE_PCM_ASRC_PALETTE_SET(pcmi_palette(stream_fs)));
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMI_CON2,
						   AFE_PCM_ASRC_PALETTE,
						   AFE_PCM_ASRC_PALETTE_SET(pcmi_palette(stream_fs)));
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMI_CON6, 0xffffffffU, 0x003f988fU);
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMI_CON7, 0xffffffffU, 0x00003c00U);
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMI_CON13,
						   AFE_PCM_ASRC_TH, AFE_PCM_ASRC_TH_SET(auto_rst_th_hi(stream_fs)));
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMI_CON14,
						   AFE_PCM_ASRC_TH, AFE_PCM_ASRC_TH_SET(auto_rst_th_lo(stream_fs)));

			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMI_CON0,
						   AFE_PCM_ASRC_CLR, AFE_PCM_ASRC_CLR);
			mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMI_CON0,
						   AFE_PCM_ASRC_CLR | AFE_PCM_ASRC_EN,
						   AFE_PCM_ASRC_CLR | AFE_PCM_ASRC_EN_SET(1U));

		}
	}

	mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1, AFE_PCM_EN, 1U);
	return 0;
}

static int mt2701_modpcm_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1,
		   AFE_PCM_FMT,
		   AFE_PCM_FMT_SET(0U));
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1,
		   AFE_PCM_FMT,
		   AFE_PCM_FMT_SET(1U));
		break;
	case SND_SOC_DAIFMT_DSP_A:
		mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1,
		   AFE_PCM_FMT,
		   AFE_PCM_FMT_SET(2U));
		break;
	case SND_SOC_DAIFMT_DSP_B:
		mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1,
		   AFE_PCM_FMT,
		   AFE_PCM_FMT_SET(3U));
		break;
	default:
		mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1,
		   AFE_PCM_FMT,
		   AFE_PCM_FMT_SET(0U));
		break;
	}

	/* master/slave */
	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBS_CFS) {
		mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1,
			   AFE_PCM_SLAVE,
			   AFE_PCM_SLAVE_SET(0U));
		mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1,
			   AFE_PCM_BYP_ASRC,
			   AFE_PCM_BYP_ASRC_SET(1U));
		afe_priv->pcm_slave = 0;
	} else {
		mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1,
			   AFE_PCM_SLAVE,
			   AFE_PCM_SLAVE_SET(1U));
		mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1,
			   AFE_PCM_BYP_ASRC,
			   AFE_PCM_BYP_ASRC_SET(0U));

		afe_priv->pcm_slave = 1;
	}
	return 0;
}


static void mt2701_modpcm_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	/* if the other direction stream is not occupied */
	if (!afe_priv->pcm_enable[!substream->stream]) {
		mt2701_regmap_update_bits(afe->regmap, AFE_PCM_INTF_CON1,
				   AFE_PCM_EN, 0);
		mt2701_regmap_update_bits(afe->regmap, AUDIO_TOP_CON4,
				   AUDIO_TOP_CON4_PDN_PCM,
				   AUDIO_TOP_CON4_PDN_PCM);
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMO_CON0,
			     AFE_PCM_ASRC_CLR | AFE_PCM_ASRC_EN, AFE_PCM_ASRC_CLR | AFE_PCM_ASRC_EN_SET(0U));
		mt2701_regmap_update_bits(afe->regmap, AUDIO_TOP_CON4,
				   AUDIO_TOP_CON4_PDN_ASRC_PCMO,
				   AUDIO_TOP_CON4_PDN_ASRC_PCMO);
	} else {
		mt2701_regmap_update_bits(afe->regmap, AFE_ASRC_PCMI_CON0,
			     AFE_PCM_ASRC_CLR | AFE_PCM_ASRC_EN, AFE_PCM_ASRC_CLR | AFE_PCM_ASRC_EN_SET(0U));
		mt2701_regmap_update_bits(afe->regmap, AUDIO_TOP_CON4,
				   AUDIO_TOP_CON4_PDN_ASRC_PCMI,
				   AUDIO_TOP_CON4_PDN_ASRC_PCMI);
	}

	afe_priv->pcm_enable[substream->stream] = 0;
}


static int mt2701_simple_fe_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int stream_dir = substream->stream;
	int memif_num = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif_tmp;

	if (of_device_is_compatible(afe->dev->of_node, "mediatek,mt2701-audio") != 0) {
		dev_warn(afe->dev, "%s, can't run single DL & DLM at the same time.\n", __func__);

		/* can't run single DL & DLM at the same time */
		if (stream_dir == SNDRV_PCM_STREAM_PLAYBACK) {
			memif_tmp = &afe->memif[MT2701_MEMIF_DLM];
			if (memif_tmp->substream != 0) {
				dev_warn(afe->dev, "%s memif is not available, stream_dir %d, memif_num %d\n",
					 __func__, stream_dir, memif_num);
				return -EBUSY;
			}
		}
	}

	return mtk_afe_fe_startup(substream, dai);
}

static int mt2701_simple_fe_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params,
				      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int stream_dir = substream->stream;

	/* single DL use PAIR_INTERLEAVE */
	if (stream_dir == SNDRV_PCM_STREAM_PLAYBACK) {
		mt2701_regmap_update_bits(afe->regmap,
				   AFE_MEMIF_PBUF_SIZE,
				   AFE_MEMIF_PBUF_SIZE_DLM_MASK,
				   AFE_MEMIF_PBUF_SIZE_PAIR_INTERLEAVE);
	}
	return mtk_afe_fe_hw_params(substream, params, dai);
}

static int mt2701_dlm_fe_startup(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_afe_memif *memif_tmp;
	const struct mtk_base_memif_data *memif_data;
	int i;

	if (of_device_is_compatible(afe->dev->of_node, "mediatek,mt2701-audio") != 0) {
		dev_warn(afe->dev, "%s, can't run single DL & DLM at the same time.\n", __func__);

		for (i = MT2701_MEMIF_DL1; i < MT2701_MEMIF_DL_SINGLE_NUM; ++i) {
			memif_tmp = &afe->memif[i];
			if (memif_tmp->substream != 0)
				return -EBUSY;
		}
	}

	/* enable agent for all signal DL (due to hw design) */
	for (i = MT2701_MEMIF_DL1; i < MT2701_MEMIF_DL_SINGLE_NUM; ++i) {
		memif_data = afe->memif[i].data;
		mt2701_regmap_update_bits(afe->regmap,
				   memif_data->agent_disable_reg,
				   1U << memif_data->agent_disable_shift,
				   0U << memif_data->agent_disable_shift);
	}

	return mtk_afe_fe_startup(substream, dai);
}

static void mt2701_dlm_fe_shutdown(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	const struct mtk_base_memif_data *memif_data;
	int i;

	if (of_device_is_compatible(afe->dev->of_node, "mediatek,mt2701-audio") != 0) {
		dev_warn(afe->dev, "%s, can't run single DL & DLM at the same time.\n", __func__);

		for (i = MT2701_MEMIF_DL1; i < MT2701_MEMIF_DL_SINGLE_NUM; ++i) {
			memif_data = afe->memif[i].data;
			mt2701_regmap_update_bits(afe->regmap,
					   memif_data->agent_disable_reg,
					   1U << memif_data->agent_disable_shift,
					   1U << memif_data->agent_disable_shift);
		}
	}

	return mtk_afe_fe_shutdown(substream, dai);
}

static int mt2701_dlm_fe_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int channels = params_channels(params);

	if (of_device_is_compatible(afe->dev->of_node, "mediatek,mt2701-audio") != 0) {
		mt2701_regmap_update_bits(afe->regmap,
				   AFE_MEMIF_PBUF_SIZE,
				   AFE_MEMIF_PBUF_SIZE_DLM_MASK,
				   AFE_MEMIF_PBUF_SIZE_FULL_INTERLEAVE);
	}
	mt2701_regmap_update_bits(afe->regmap,
			   AFE_MEMIF_PBUF_SIZE,
			   AFE_MEMIF_PBUF_SIZE_DLM_BYTE_MASK,
			   AFE_MEMIF_PBUF_SIZE_DLM_32BYTES);
	mt2701_regmap_update_bits(afe->regmap,
			   AFE_MEMIF_PBUF_SIZE,
			   AFE_MEMIF_PBUF_SIZE_DLM_CH_MASK,
			   AFE_MEMIF_PBUF_SIZE_DLM_CH(channels));

	return mtk_afe_fe_hw_params(substream, params, dai);
}

static int mt2701_dlm_fe_trigger(struct snd_pcm_substream *substream,
				 int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_afe_memif *memif_tmp = &afe->memif[MT2701_MEMIF_DL1];
	int ret;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		mt2701_regmap_update_bits(afe->regmap, memif_tmp->data->enable_reg,
				   1U << memif_tmp->data->enable_shift,
				   1U << memif_tmp->data->enable_shift);
		ret = mtk_afe_fe_trigger(substream, cmd, dai);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ret = mtk_afe_fe_trigger(substream, cmd, dai);
		mt2701_regmap_update_bits(afe->regmap, memif_tmp->data->enable_reg,
				   1U << memif_tmp->data->enable_shift, 0U);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int mt2701_dai_num_to_tdm(struct mtk_base_afe *afe, int num)
{
	int val;

	if (num > MT2701_MEMIF_NUM)
		val = num - MT2701_IO_TDMO1;
	else
		val = num - MT2701_MEMIF_DLTDM1;

	if (val < 0 || val >= MT2701_TDM_NUM) {
		dev_err(afe->dev, "%s, num not available, num %d, val %d\n",
						__func__, num, val);
		return -EINVAL;
	}
	return val;
}

enum mt2701_tdm_channel mt2701_tdm_map_channel(int channel)
{
	if (channel > 16)
		return TDM_16CH;
	else if (channel > 12)
		return TDM_16CH;
	else if (channel > 8)
		return TDM_12CH;
	else if (channel > 4)
		return TDM_8CH;
	else if (channel > 2)
		return TDM_4CH;
	else
		return TDM_2CH;
}

int afe_tdm_in_mask_configurate(int channel)
{
	u32 odd_mask = 0U; /* ch0,1: 7th bit; ch14,15: 1st bit */
	u32 disable_out = 0U; /* ch0,1: 7th bit; ch14,15: 1st bit */
	u32 reg_val_to_write = 0U;
	int i;
	int ch_pair_num = channel / 2;
	int ch_odd = channel % 2;

	for (i = 0; i < 8 - ch_pair_num; i++)
		disable_out |= 1U << i;

	if (ch_odd != 0)
		odd_mask = 1U << (unsigned int)(8 - ch_pair_num);
	else
		odd_mask = 0U;

	reg_val_to_write |= AFE_TDM_IN_DISABLE_OUT_SET(disable_out) & AFE_TDM_IN_DISABLE_OUT;
	reg_val_to_write |= AFE_TDM_IN_MASK_ODD_SET(odd_mask) & AFE_TDM_IN_MASK_ODD;

	return reg_val_to_write;
}

void mt2701_tdm_configuration(struct mtk_base_afe *afe, int tdm_id, struct snd_pcm_hw_params *params)
{
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	const struct mt2701_tdm_data *tdm_data = afe_priv->tdm_path[tdm_id].tdm_data;
	int channels = params_channels(params);
	int bit_width = params_width(params);
	unsigned int val = 0, mask = 0;

	mask = AFE_TDM_CON_WLEN | AFE_TDM_CON_CH | AFE_TDM_CON_LRCK_WIDTH
	       | (3U << tdm_data->tdm_lrck_cycle_shift);

	/* TDM config */
	if (bit_width == 32)
		val = AFE_TDM_CON_WLEN_SET(3U) | (2U << tdm_data->tdm_lrck_cycle_shift);
	else if (bit_width == 16)
		val = AFE_TDM_CON_WLEN_SET(1U) | (0U << tdm_data->tdm_lrck_cycle_shift);
	else if (bit_width == 24)
		val = AFE_TDM_CON_WLEN_SET(2U) | (2U << tdm_data->tdm_lrck_cycle_shift);

	val |= AFE_TDM_CON_CH_SET((unsigned int)mt2701_tdm_map_channel(channels));
	val |= AFE_TDM_CON_LRCK_WIDTH_SET((unsigned int)(bit_width - 1));

	if (tdm_id == MT2701_TDMI) {
		mask |= AFE_TDM_CON_LRCK_DELAY | AFE_TDM_CON_LRCK_WIDTH | (0x1U<<21U);
		val |= AFE_TDM_CON_LRCK_DELAY_SET(afe_priv->tdm_in_lrck.delay_half_T) | (0x1U<<21U);

		if (afe_priv->tdm_in_lrck.width != 0U)
			val |= AFE_TDM_CON_LRCK_WIDTH_SET((unsigned int)((channels * bit_width)-2));

		mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_ctrl_reg, mask, val);
		mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_ctrl_2nd_reg, 0xffffU,
			afe_tdm_in_mask_configurate(channels));
	} else {
		mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_ctrl_reg, mask, val);
		mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_ctrl_2nd_reg, 0xffffffffU, 0U);
	}

}

static int mt2701_tdm_fe_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int tdm_num = mt2701_dai_num_to_tdm(afe, dai->id);
	const struct mt2701_tdm_data *tdm_data = afe_priv->tdm_path[tdm_num].tdm_data;
	int channels = params_channels(params);
	int bit_width = params_width(params);

	if (afe->memif_fs(substream, params_rate(params)) < 0)
		return -EINVAL;

	if (tdm_num < MT2701_TDMI) {
		if (bit_width != 16)
			mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_agent_reg,
					   1U << tdm_data->tdm_agent_bit_width_shift,
					   1U << tdm_data->tdm_agent_bit_width_shift);
		else
			mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_agent_reg,
					   1U << tdm_data->tdm_agent_bit_width_shift,
					   0U << tdm_data->tdm_agent_bit_width_shift);

		mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_agent_reg,
				0x1fU << tdm_data->tdm_agent_ch_num_shift,
				(unsigned int)(channels) << tdm_data->tdm_agent_ch_num_shift);
	}

	return mtk_afe_fe_hw_params(substream, params, dai);
}

static int mt2701_tdmio_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				     unsigned int freq, int dir)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dai->dev);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int tdm_num = mt2701_dai_num_to_tdm(afe, dai->id);

	if (tdm_num < 0)
		return tdm_num;

	/* mclk */
	afe_priv->tdm_path[tdm_num].mclk_rate = freq;
	return 0;
}

static int mt2701_set_tdm_fmt(struct snd_soc_dai *dai, unsigned int fmt, int tdm_num)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	const struct mt2701_tdm_data *tdm_data = afe_priv->tdm_path[tdm_num].tdm_data;
	unsigned int mask = 0, val = 0;

	mask = AFE_TDM_CON_DELAY | AFE_TDM_CON_LEFT_ALIGN | AFE_TDM_CON_INV_BCK | AFE_TDM_CON_INV_LRCK;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_LEFT_J:
	/* falling through */
	case SND_SOC_DAIFMT_RIGHT_J:
		val = AFE_TDM_CON_DELAY_SET(0U);
		break;

	case SND_SOC_DAIFMT_I2S:
	/* falling through */
	case SND_SOC_DAIFMT_DSP_A:
	default:
		val = AFE_TDM_CON_DELAY_SET(1U);
		break;

	}

	if (tdm_num != MT2701_TDMI) {
		if ((fmt & SND_SOC_DAIFMT_FORMAT_MASK) == SND_SOC_DAIFMT_RIGHT_J)
			val |= AFE_TDM_CON_LEFT_ALIGN_SET(0U);
		else
			val |= AFE_TDM_CON_LEFT_ALIGN_SET(1U);
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		val |= AFE_TDM_CON_INV_BCK_SET(0U) | AFE_TDM_CON_INV_LRCK_SET(0U);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		val |= AFE_TDM_CON_INV_BCK_SET(0U) | AFE_TDM_CON_INV_LRCK_SET(1U);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		val |= AFE_TDM_CON_INV_BCK_SET(1U) | AFE_TDM_CON_INV_LRCK_SET(0U);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		val |= AFE_TDM_CON_INV_BCK_SET(1U) | AFE_TDM_CON_INV_LRCK_SET(1U);
		break;
	default:
		val |= AFE_TDM_CON_INV_BCK_SET(0U) | AFE_TDM_CON_INV_LRCK_SET(0U);
		break;
	}

	mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_ctrl_reg, mask, val);
	return 0;

}

static int mt2701_tdmio_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	return mt2701_set_tdm_fmt(dai, fmt, mt2701_dai_num_to_tdm(afe, dai->id));
}

static int mt2701_tdmio_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int tdm_num = mt2701_dai_num_to_tdm(afe, dai->id);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	return afe_priv->clk_ctrl->enable_mclk(afe, MCLK_TDM_OFFSET + tdm_num);
}

static int mt2701_tdmio_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int tdm_num = mt2701_dai_num_to_tdm(afe, dai->id);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	const struct mt2701_tdm_data *tdm_data = afe_priv->tdm_path[tdm_num].tdm_data;
	int ret;

	mt2701_regmap_update_bits(afe->regmap, AFE_TDM_IN_CON1, 3U<<21U, 0U);

	ret = mt2701_tdm_clk_configuration(afe, tdm_num, params, false);
	if (ret != 0)
		return ret;
	mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_bck_reg,
			1U<<tdm_data->tdm_bck_on_shift, 1U<<tdm_data->tdm_bck_on_shift);

	mt2701_tdm_configuration(afe, tdm_num, params);
	return 0;
}

int mt2701_tdmio_trigger(struct snd_pcm_substream *substream, int cmd,
		       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int tdm_num = mt2701_dai_num_to_tdm(afe, dai->id);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	const struct mt2701_tdm_data *tdm_data = afe_priv->tdm_path[tdm_num].tdm_data;
	int ret;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_ctrl_reg,
				1U<<tdm_data->tdm_on_shift, 1U<<tdm_data->tdm_on_shift);
		ret = 0;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_ctrl_reg,
				1U<<tdm_data->tdm_on_shift, 0U<<tdm_data->tdm_on_shift);
		ret = 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void mt2701_tdmio_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int tdm_num = mt2701_dai_num_to_tdm(afe, dai->id);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	const struct mt2701_tdm_data *tdm_data = afe_priv->tdm_path[tdm_num].tdm_data;

	mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_bck_reg,
			 1U<<tdm_data->tdm_bck_on_shift, 0U<<tdm_data->tdm_bck_on_shift);
	afe_priv->clk_ctrl->disable_mclk(afe, MCLK_TDM_OFFSET + tdm_num);
}

static int mt2701_tdmio_coclk_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				     unsigned int freq, int dir)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dai->dev);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int tdm_num = afe_priv->tdm_coclk_info.src;

	if (tdm_num < 0)
		return tdm_num;

	/* mclk */
	afe_priv->tdm_path[tdm_num].mclk_rate = freq;
	afe_priv->tdm_path[MT2701_TDMI].mclk_rate = freq;
	return 0;
}

static int mt2701_tdmio_coclk_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	afe_priv->tdm_coclk_info.on++;
	if (afe_priv->tdm_coclk_info.on == 1)
		return afe_priv->clk_ctrl->enable_mclk(afe, MCLK_TDM_OFFSET + MT2701_TDMI);
	else
		return 0;
}

static int mt2701_tdmio_coclk_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int tdm_num = afe_priv->tdm_coclk_info.src;
	int tdm_num_in = MT2701_TDMI;
	const struct mt2701_tdm_data *tdm_data = afe_priv->tdm_path[tdm_num].tdm_data;
	const struct mt2701_tdm_data *tdm_data_in = afe_priv->tdm_path[tdm_num_in].tdm_data;
	int ret = 0;

	mt2701_regmap_update_bits(afe->regmap, AFE_TDM_IN_CON1, 3U<<21U, 0U);

	if (afe_priv->tdm_coclk_info.hw_on == 0) {
		mt2701_regmap_update_bits(afe->regmap, AFE_TDM_IN_CON1, AFE_TDM_CON_IN_BCK,
				AFE_TDM_CON_IN_BCK_SET((unsigned int)tdm_num));
		mt2701_regmap_update_bits(afe->regmap, AFE_TDM_IN_CON1, AFE_TDM_CON_INOUT_SYNC,
				AFE_TDM_CON_INOUT_SYNC_SET(1U));

		ret = mt2701_tdm_clk_configuration(afe, tdm_num, params, true);

		mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_bck_reg, 1U<<tdm_data->tdm_bck_on_shift,
				 1U<<tdm_data->tdm_bck_on_shift);

		mt2701_tdm_configuration(afe, tdm_num, params);
		mt2701_tdm_configuration(afe, tdm_num_in, params);

		mt2701_regmap_update_bits(afe->regmap, tdm_data_in->tdm_ctrl_reg, 1U<<tdm_data_in->tdm_on_shift,
				 1U<<tdm_data_in->tdm_on_shift);
		mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_ctrl_reg, 1U<<tdm_data->tdm_on_shift,
				 1U<<tdm_data->tdm_on_shift);

		afe_priv->tdm_coclk_info.sample_rate = params_rate(params);
		afe_priv->tdm_coclk_info.channels = params_channels(params);
		afe_priv->tdm_coclk_info.bit_width = params_width(params);
		afe_priv->tdm_coclk_info.hw_on = 1;
	} else {
		if (params_rate(params) != afe_priv->tdm_coclk_info.sample_rate
			|| params_channels(params) != afe_priv->tdm_coclk_info.channels
			|| params_width(params) != afe_priv->tdm_coclk_info.bit_width)
			ret = -EINVAL;
	}

	return ret;
}

static int mt2701_tdmio_coclk_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int ret;

	ret = mt2701_set_tdm_fmt(dai, fmt, afe_priv->tdm_coclk_info.src);
	if (ret != 0)
		return ret;
	ret = mt2701_set_tdm_fmt(dai, fmt, MT2701_TDMI);
	return ret;

}

static void mt2701_tdmio_coclk_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int tdm_num = afe_priv->tdm_coclk_info.src;
	const struct mt2701_tdm_data *tdm_data = afe_priv->tdm_path[tdm_num].tdm_data;
	const struct mt2701_tdm_data *tdm_data_in = afe_priv->tdm_path[MT2701_TDMI].tdm_data;

	if (afe_priv->tdm_coclk_info.on == 1) {
		mt2701_regmap_update_bits(afe->regmap, tdm_data_in->tdm_ctrl_reg,
				1U<<tdm_data_in->tdm_on_shift, 0U<<tdm_data_in->tdm_on_shift);
		mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_ctrl_reg,
				1U<<tdm_data->tdm_on_shift, 0U<<tdm_data->tdm_on_shift);
		mt2701_regmap_update_bits(afe->regmap, tdm_data->tdm_bck_reg,
				1U<<tdm_data->tdm_bck_on_shift, 0U<<tdm_data->tdm_bck_on_shift);
		afe_priv->clk_ctrl->disable_mclk(afe, MCLK_TDM_OFFSET + MT2701_TDMI);
		afe_priv->tdm_coclk_info.hw_on = 0;
	}
	afe_priv->tdm_coclk_info.on--;
}

static int mt2701_memif_fs(struct snd_pcm_substream *substream,
			   unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int fs;

	if (rtd->cpu_dai->id != MT2701_MEMIF_ULBT)
		fs = mt2701_afe_i2s_fs(rate);
	else
		fs = (rate == 16000U ? 1 : 0);
	return fs;
}

static int mt2701_irq_fs(struct snd_pcm_substream *substream, unsigned int rate)
{
	return mt2701_afe_i2s_fs(rate);
}

/* FE DAIs */
static const struct snd_soc_dai_ops mt2701_single_memif_dai_ops = {
	.startup	= mt2701_simple_fe_startup,
	.shutdown	= mtk_afe_fe_shutdown,
	.hw_params	= mt2701_simple_fe_hw_params,
	.hw_free	= mtk_afe_fe_hw_free,
	.prepare	= mtk_afe_fe_prepare,
	.trigger	= mtk_afe_fe_trigger,

};

static const struct snd_soc_dai_ops mt2701_dlm_memif_dai_ops = {
	.startup	= mt2701_dlm_fe_startup,
	.shutdown	= mt2701_dlm_fe_shutdown,
	.hw_params	= mt2701_dlm_fe_hw_params,
	.hw_free	= mtk_afe_fe_hw_free,
	.prepare	= mtk_afe_fe_prepare,
	.trigger	= mt2701_dlm_fe_trigger,
};

/* TDM FE DAIs */
static const struct snd_soc_dai_ops mt2701_tdm_memif_ops = {
	.startup	= mtk_afe_fe_startup,
	.shutdown	= mtk_afe_fe_shutdown,
	.hw_params	= mt2701_tdm_fe_hw_params,
	.hw_free	= mtk_afe_fe_hw_free,
	.prepare	= mtk_afe_fe_prepare,
	.trigger	= mtk_afe_fe_trigger,
};

/* I2S BE DAIs */
static const struct snd_soc_dai_ops mt2701_afe_i2s_ops = {
	.startup	= mt2701_afe_i2s_startup,
	.shutdown	= mt2701_afe_i2s_shutdown,
	.prepare	= mt2701_afe_i2s_prepare,
	.set_sysclk	= mt2701_afe_i2s_set_sysclk,
	.hw_params	= mt2701_afe_i2s_hw_params,
	.set_fmt	= mt2701_afe_i2s_set_fmt,
};

/* MRG BE DAIs */
static struct snd_soc_dai_ops mt2701_btmrg_ops = {
	.startup = mt2701_btmrg_startup,
	.shutdown = mt2701_btmrg_shutdown,
	.hw_params = mt2701_btmrg_hw_params,
};


static struct snd_soc_dai_ops mt2701_modpcm_ops = {
	.startup = mt2701_modpcm_startup,
	.shutdown = mt2701_modpcm_shutdown,
	.hw_params = mt2701_modpcm_hw_params,
	.set_fmt = mt2701_modpcm_set_fmt
};


/* TDM BE DAIs */
static const struct snd_soc_dai_ops mt2701_tdmio_ops = {
	.startup	= mt2701_tdmio_startup,
	.hw_params      = mt2701_tdmio_hw_params,
	.trigger	= mt2701_tdmio_trigger,
	.set_fmt	= mt2701_tdmio_set_fmt,
	.set_sysclk	= mt2701_tdmio_set_sysclk,
	.shutdown	= mt2701_tdmio_shutdown,
};

static const struct snd_soc_dai_ops mt2701_tdmio_coclk_ops = {
	.startup	= mt2701_tdmio_coclk_startup,
	.hw_params      = mt2701_tdmio_coclk_hw_params,
	.set_fmt	= mt2701_tdmio_coclk_set_fmt,
	.set_sysclk	= mt2701_tdmio_coclk_set_sysclk,
	.shutdown	= mt2701_tdmio_coclk_shutdown,
};



static struct snd_soc_dai_driver mt2701_afe_pcm_dais[] = {
	/* FE DAIs: memory intefaces to CPU */
	{
		.name = "PCMO0",
		.id = MT2701_MEMIF_DL1,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "DL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)

		},
		.ops = &mt2701_single_memif_dai_ops,
	},
	{
		.name = "PCM0",
		.id = MT2701_MEMIF_UL1,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.capture = {
			.stream_name = "UL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
		},
		.ops = &mt2701_single_memif_dai_ops,
	},
	{
		.name = "PCMO1",
		.id = MT2701_MEMIF_DL2,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)

		},
		.ops = &mt2701_single_memif_dai_ops,
	},
	{
		.name = "PCM1",
		.id = MT2701_MEMIF_UL2,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.capture = {
			.stream_name = "UL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)

		},
		.ops = &mt2701_single_memif_dai_ops,
	},
	{
		.name = "PCMO2",
		.id = MT2701_MEMIF_DL3,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "DL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)

		},
		.ops = &mt2701_single_memif_dai_ops,
	},
	{
		.name = "PCM2",
		.id = MT2701_MEMIF_UL3,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.capture = {
			.stream_name = "UL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)

		},
		.ops = &mt2701_single_memif_dai_ops,
	},
	{
		.name = "PCMO3",
		.id = MT2701_MEMIF_DL4,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "DL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)

		},
		.ops = &mt2701_single_memif_dai_ops,
	},
	{
		.name = "PCM_multi",
		.id = MT2701_MEMIF_DLM,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "DLM",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)

		},
		.ops = &mt2701_dlm_memif_dai_ops,
	},
	{
		.name = "PCM_AWB2",
		.id = MT2701_MEMIF_AWB2,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.capture = {
			.stream_name = "ULAWB2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)

		},
		.ops = &mtk_afe_fe_ops,
	},
	{
		.name = "PCM_BT_DL",
		.id = MT2701_MEMIF_DLBT,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "DLBT",
			.channels_min = 1,
			.channels_max = 1,
			.rates = (SNDRV_PCM_RATE_8000
				| SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mt2701_single_memif_dai_ops,
	},
	{
		.name = "PCM_BT_UL",
		.id = MT2701_MEMIF_ULBT,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.capture = {
			.stream_name = "ULBT",
			.channels_min = 1,
			.channels_max = 1,
			.rates = (SNDRV_PCM_RATE_8000
				| SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE
		},
		.ops = &mt2701_single_memif_dai_ops,
	},
	{
		.name = "PCM_MOD_DL",
		.id = MT2701_MEMIF_DLMOD,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "DLMOD",
			.channels_min = 1,
			.channels_max = 2,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
		},
		.ops = &mtk_afe_fe_ops,
	},
	{
		.name = "PCM_MOD_UL",
		.id = MT2701_MEMIF_ULMOD,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.capture = {
			.stream_name = "ULMOD",
			.channels_min = 1,
			.channels_max = 2,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
		},
		.ops = &mtk_afe_fe_ops,
	},
	{
		.name = "PCM_TDMO0",
		.id = MT2701_MEMIF_DLTDM1,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "DLTDM0",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
		},
		.ops = &mt2701_tdm_memif_ops,
	},
	{
		.name = "PCM_TDMO1",
		.id = MT2701_MEMIF_DLTDM2,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "DLTDM1",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
		},
		.ops = &mt2701_tdm_memif_ops,
	},
	{
		.name = "PCM_TDMIN",
		.id = MT2701_MEMIF_ULTDM,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.capture = {
			.stream_name = "ULTDM",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
		},
		.ops = &mt2701_tdm_memif_ops,
	},
	/* BE DAIs */
	{
		.name = "I2S0",
		.id = MT2701_IO_I2S,
		.playback = {
			.stream_name = "I2S0 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)

		},
		.capture = {
			.stream_name = "I2S0 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)

		},
		.ops = &mt2701_afe_i2s_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "I2S1",
		.id = MT2701_IO_2ND_I2S,
		.playback = {
			.stream_name = "I2S1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
			},
		.capture = {
			.stream_name = "I2S1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
			},
		.ops = &mt2701_afe_i2s_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "I2S2",
		.id = MT2701_IO_3RD_I2S,
		.playback = {
			.stream_name = "I2S2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
			},
		.capture = {
			.stream_name = "I2S2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
			},
		.ops = &mt2701_afe_i2s_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "I2S3",
		.id = MT2701_IO_4TH_I2S,
		.playback = {
			.stream_name = "I2S3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
			},
		.capture = {
			.stream_name = "I2S3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
			},
		.ops = &mt2701_afe_i2s_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "AADC",
		.id = MT2701_IO_AADC,
		.capture = {
			.stream_name = "AADC Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_384000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
			},
		.symmetric_rates = 1,
	},
	{
		.name = "MRG BT",
		.id = MT2701_IO_MRG,
		.playback = {
			.stream_name = "BT Playback",
			.channels_min = 1,
			.channels_max = 1,
			.rates = (SNDRV_PCM_RATE_8000
				| SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "BT Capture",
			.channels_min = 1,
			.channels_max = 1,
			.rates = (SNDRV_PCM_RATE_8000
				| SNDRV_PCM_RATE_16000),
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mt2701_btmrg_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "MOD PCM",
		.id = MT2701_IO_MOD,
		.playback = {
			.stream_name = "MOD Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
		},
		.capture = {
			.stream_name = "MOD Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000
				| SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000),
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
		},
		.ops = &mt2701_modpcm_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "TDMO0",
		.id = MT2701_IO_TDMO1,
		.playback = {
			.stream_name = "TDM0 Playback",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
		},
		.ops = &mt2701_tdmio_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "TDMO1",
		.id = MT2701_IO_TDMO2,
		.playback = {
			.stream_name = "TDM1 Playback",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
		},
		.ops = &mt2701_tdmio_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "TDMIN",
		.id = MT2701_IO_TDMI,
		.capture = {
			.stream_name = "TDM Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = (SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE)
		},
		.ops = &mt2701_tdmio_ops,
		.symmetric_rates = 1,
	},
};

static const struct snd_kcontrol_new mt2701_afe_o00_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I00 Switch", AFE_CONN0, 0, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o01_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I01 Switch", AFE_CONN1, 1, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o02_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I02 Switch", AFE_CONN2, 2, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o03_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I03 Switch", AFE_CONN3, 3, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o04_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN4, 4, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o05_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN5, 5, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o10_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN10, 8, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o11_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I09 Switch", AFE_CONN11, 9, 1, 0),
};


static const struct snd_kcontrol_new mt2701_afe_o14_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I26 Switch", AFE_CONN14, 26, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o15_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I12 Switch", AFE_CONN15, 12, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o16_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I13 Switch", AFE_CONN16, 13, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o17_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I14 Switch", AFE_CONN17, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I16 Switch", AFE_CONN17, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN17, 18, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o18_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I15 Switch", AFE_CONN18, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I17 Switch", AFE_CONN18, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN18, 19, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o19_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I14 Switch", AFE_CONN19, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I16 Switch", AFE_CONN19, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN19, 18, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o20_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I15 Switch", AFE_CONN20, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I17 Switch", AFE_CONN20, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN20, 19, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o21_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I14 Switch", AFE_CONN21, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I16 Switch", AFE_CONN21, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I18 Switch", AFE_CONN21, 18, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o22_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I15 Switch", AFE_CONN22, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I17 Switch", AFE_CONN22, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I19 Switch", AFE_CONN22, 19, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o23_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I20 Switch", AFE_CONN23, 20, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o24_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I21 Switch", AFE_CONN24, 21, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o25_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I22 Switch", AFE_CONN25, 22, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o26_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I23 Switch", AFE_CONN26, 23, 1, 0),
};


static const struct snd_kcontrol_new mt2701_afe_o31_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I35 Switch", AFE_CONN41, 9, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o32_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I31 Switch", AFE_CONN32, 31, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_o33_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I32 Switch", AFE_CONN41, 18, 1, 0),
};


static const struct snd_kcontrol_new mt2701_afe_i02_mix[] = {
	SOC_DAPM_SINGLE("I2S0 Switch", SND_SOC_NOPM, 0, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_multi_ch_out_i2s0[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("Multich I2S0 Out Switch",
				    ASYS_I2SO1_CON, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("Multich DL1 Switch",
				    AFE_MCH_OUT_CFG, 0, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_multi_ch_out_i2s1[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("Multich I2S1 Out Switch",
				    ASYS_I2SO2_CON, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("Multich DL2 Switch",
				    AFE_MCH_OUT_CFG, 1, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_multi_ch_out_i2s2[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("Multich I2S2 Out Switch",
				    PWR2_TOP_CON, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("Multich DL3 Switch",
				    AFE_MCH_OUT_CFG, 2, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_multi_ch_out_i2s3[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("Multich I2S3 Out Switch",
				    PWR2_TOP_CON, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("Multich DL4 Switch",
				    AFE_MCH_OUT_CFG, 3, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_multi_ch_out_i2s4[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("Multich I2S4 Out Switch",
				    PWR2_TOP_CON, 19, 1, 0),
};

static const struct snd_kcontrol_new mt2701_afe_multi_ch_out_asrc0[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("Asrc0 out Switch", AUDIO_TOP_CON4, 14, 1,
				    1),
};

static const struct snd_kcontrol_new mt2701_afe_multi_ch_out_asrc1[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("Asrc1 out Switch", AUDIO_TOP_CON4, 15, 1,
				    1),
};

static const struct snd_kcontrol_new mt2701_afe_multi_ch_out_asrc2[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("Asrc2 out Switch", PWR2_TOP_CON, 6, 1,
				    1),
};

static const struct snd_kcontrol_new mt2701_afe_multi_ch_out_asrc3[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("Asrc3 out Switch", PWR2_TOP_CON, 7, 1,
				    1),
};

static const struct snd_kcontrol_new mt2701_afe_multi_ch_out_asrc4[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("Asrc4 out Switch", PWR2_TOP_CON, 8, 1,
				    1),
};

static int mt2701_tdm_loopback_sel_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->tdm_coclk_info.src;
	return 0;
}

static int mt2701_tdm_loopback_sel_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	afe_priv->tdm_coclk_info.src = (int)ucontrol->value.integer.value[0];
	if (afe_priv->tdm_coclk_info.src == 0)
		mt2701_regmap_update_bits(afe->regmap, AFE_TDM_IN_CON1,
			(1U << 15U), (1U << 15U));
	else
		mt2701_regmap_update_bits(afe->regmap, AFE_TDM_IN_CON1,
			(1U << 15U), (0U << 15U));
	return 0;
}

static int mt2701_tdm_loopback_switch_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = (int)afe_priv->tdm_coclk;
	return 0;
}

static int mt2701_tdm_loopback_switch_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dai_driver *dai_driver;

	if (afe_priv->tdm_coclk == (bool)ucontrol->value.integer.value[0])
		return 0;

	afe_priv->tdm_coclk = (bool)ucontrol->value.integer.value[0];
	if (afe_priv->tdm_coclk) {
		dai_driver = find_dai_driver_by_id(mt2701_afe_pcm_dais, ARRAY_SIZE(mt2701_afe_pcm_dais),
				afe_priv->tdm_coclk_info.src+MT2701_IO_TDMO1);
		if (dai_driver != 0)
			dai_driver->ops = &mt2701_tdmio_coclk_ops;
		dai_driver = find_dai_driver_by_id(mt2701_afe_pcm_dais, ARRAY_SIZE(mt2701_afe_pcm_dais),
				 MT2701_IO_TDMI);
		if (dai_driver != 0)
			dai_driver->ops = &mt2701_tdmio_coclk_ops;

		mt2701_regmap_update_bits(afe->regmap, AFE_TDM_IN_CON1, (1U << 20U), (1U << 20U));

	} else {
		dai_driver = find_dai_driver_by_id(mt2701_afe_pcm_dais, ARRAY_SIZE(mt2701_afe_pcm_dais),
				MT2701_IO_TDMO1);
		if (dai_driver != 0)
			dai_driver->ops = &mt2701_tdmio_ops;

		dai_driver = find_dai_driver_by_id(mt2701_afe_pcm_dais, ARRAY_SIZE(mt2701_afe_pcm_dais),
				MT2701_IO_TDMO2);
		if (dai_driver != 0)
			dai_driver->ops = &mt2701_tdmio_ops;

		dai_driver = find_dai_driver_by_id(mt2701_afe_pcm_dais, ARRAY_SIZE(mt2701_afe_pcm_dais),
				 MT2701_IO_TDMI);
		if (dai_driver != 0)
			dai_driver->ops = &mt2701_tdmio_ops;

		mt2701_regmap_update_bits(afe->regmap, AFE_TDM_IN_CON1, (1U << 20), (0U << 20U));
	}

	return 0;
}

static int mt2701_i2so2_mclk_switch_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = (int)afe_priv->i2so2_mclk;
	return 0;
}

static int mt2701_i2so2_mclk_switch_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	if (afe_priv->i2so2_mclk == (bool)ucontrol->value.integer.value[0])
		return 0;

	afe_priv->i2so2_mclk = (bool)ucontrol->value.integer.value[0];
	if (afe_priv->i2so2_mclk)
		afe_priv->clk_ctrl->enable_mclk(afe, 2);
	else
		afe_priv->clk_ctrl->disable_mclk(afe, 2);

	return 0;
}

static const struct snd_kcontrol_new mt2701_soc_controls[] = {
	SOC_SINGLE_EXT("TDM Loopback Switch", AFE_TDM_IN_CON1, 20, 1, 0,
		mt2701_tdm_loopback_switch_get, mt2701_tdm_loopback_switch_set),
	SOC_SINGLE_EXT("TDM Out Loopback Select", AFE_TDM_IN_CON1, 15, 1, 1,
		mt2701_tdm_loopback_sel_get, mt2701_tdm_loopback_sel_set),
	SOC_SINGLE("I2SO0_I2SI0 Loopback Switch", ASYS_I2SIN1_CON, 21, 1, 0),
	SOC_SINGLE("I2SO1_I2SI1 Loopback Switch", ASYS_I2SIN2_CON, 21, 1, 0),
	SOC_SINGLE("I2SO2_I2SI2 Loopback Switch", ASYS_I2SIN3_CON, 21, 1, 0),
	SOC_SINGLE("I2SO3_I2SI2 Loopback Switch", ASYS_I2SIN3_CON, 20, 1, 0),
	SOC_SINGLE("MODPCM Loopback Switch", AFE_PCM_INTF_CON2, 10, 1, 0),
	SOC_SINGLE("MRGIF Serial Loopback Switch", AFE_MRGIF_CON, 17, 1, 0),
	SOC_SINGLE_BOOL_EXT("I2SO2 MCLK Switch", 0, mt2701_i2so2_mclk_switch_get,
		mt2701_i2so2_mclk_switch_set),
};

static const struct snd_soc_dapm_widget mt2701_afe_pcm_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("I00", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I01", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I02", SND_SOC_NOPM, 0, 0, mt2701_afe_i02_mix,
			   ARRAY_SIZE(mt2701_afe_i02_mix)),
	SND_SOC_DAPM_MIXER("I03", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I04", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I05", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I08", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I09", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I12", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I13", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I14", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I15", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I16", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I17", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I18", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I19", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I22", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I23", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I26", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I31", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I32", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I35", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("O00", SND_SOC_NOPM, 0, 0, mt2701_afe_o00_mix,
			   ARRAY_SIZE(mt2701_afe_o00_mix)),
	SND_SOC_DAPM_MIXER("O01", SND_SOC_NOPM, 0, 0, mt2701_afe_o01_mix,
			   ARRAY_SIZE(mt2701_afe_o01_mix)),
	SND_SOC_DAPM_MIXER("O02", SND_SOC_NOPM, 0, 0, mt2701_afe_o02_mix,
			   ARRAY_SIZE(mt2701_afe_o02_mix)),
	SND_SOC_DAPM_MIXER("O03", SND_SOC_NOPM, 0, 0, mt2701_afe_o03_mix,
			   ARRAY_SIZE(mt2701_afe_o03_mix)),
	SND_SOC_DAPM_MIXER("O04", SND_SOC_NOPM, 0, 0, mt2701_afe_o04_mix,
			   ARRAY_SIZE(mt2701_afe_o04_mix)),
	SND_SOC_DAPM_MIXER("O05", SND_SOC_NOPM, 0, 0, mt2701_afe_o05_mix,
			   ARRAY_SIZE(mt2701_afe_o05_mix)),
	SND_SOC_DAPM_MIXER("O10", SND_SOC_NOPM, 0, 0, mt2701_afe_o10_mix,
			   ARRAY_SIZE(mt2701_afe_o10_mix)),
	SND_SOC_DAPM_MIXER("O11", SND_SOC_NOPM, 0, 0, mt2701_afe_o11_mix,
			   ARRAY_SIZE(mt2701_afe_o11_mix)),
	SND_SOC_DAPM_MIXER("O14", SND_SOC_NOPM, 0, 0, mt2701_afe_o14_mix,
			   ARRAY_SIZE(mt2701_afe_o14_mix)),
	SND_SOC_DAPM_MIXER("O15", SND_SOC_NOPM, 0, 0, mt2701_afe_o15_mix,
			   ARRAY_SIZE(mt2701_afe_o15_mix)),
	SND_SOC_DAPM_MIXER("O16", SND_SOC_NOPM, 0, 0, mt2701_afe_o16_mix,
			   ARRAY_SIZE(mt2701_afe_o16_mix)),
	SND_SOC_DAPM_MIXER("O17", SND_SOC_NOPM, 0, 0, mt2701_afe_o17_mix,
			   ARRAY_SIZE(mt2701_afe_o17_mix)),
	SND_SOC_DAPM_MIXER("O18", SND_SOC_NOPM, 0, 0, mt2701_afe_o18_mix,
			   ARRAY_SIZE(mt2701_afe_o18_mix)),
	SND_SOC_DAPM_MIXER("O19", SND_SOC_NOPM, 0, 0, mt2701_afe_o19_mix,
			   ARRAY_SIZE(mt2701_afe_o19_mix)),
	SND_SOC_DAPM_MIXER("O20", SND_SOC_NOPM, 0, 0, mt2701_afe_o20_mix,
			   ARRAY_SIZE(mt2701_afe_o20_mix)),
	SND_SOC_DAPM_MIXER("O21", SND_SOC_NOPM, 0, 0, mt2701_afe_o21_mix,
			   ARRAY_SIZE(mt2701_afe_o21_mix)),
	SND_SOC_DAPM_MIXER("O22", SND_SOC_NOPM, 0, 0, mt2701_afe_o22_mix,
			   ARRAY_SIZE(mt2701_afe_o22_mix)),
	SND_SOC_DAPM_MIXER("O25", SND_SOC_NOPM, 0, 0, mt2701_afe_o25_mix,
			   ARRAY_SIZE(mt2701_afe_o25_mix)),
	SND_SOC_DAPM_MIXER("O26", SND_SOC_NOPM, 0, 0, mt2701_afe_o26_mix,
			   ARRAY_SIZE(mt2701_afe_o26_mix)),
	SND_SOC_DAPM_MIXER("O31", SND_SOC_NOPM, 0, 0, mt2701_afe_o31_mix,
			   ARRAY_SIZE(mt2701_afe_o31_mix)),
	SND_SOC_DAPM_MIXER("O32", SND_SOC_NOPM, 0, 0, mt2701_afe_o32_mix,
			   ARRAY_SIZE(mt2701_afe_o32_mix)),
	SND_SOC_DAPM_MIXER("O33", SND_SOC_NOPM, 0, 0, mt2701_afe_o33_mix,
			   ARRAY_SIZE(mt2701_afe_o33_mix)),

	SND_SOC_DAPM_MIXER("I12I13", SND_SOC_NOPM, 0, 0,
			   mt2701_afe_multi_ch_out_i2s0,
			   ARRAY_SIZE(mt2701_afe_multi_ch_out_i2s0)),
	SND_SOC_DAPM_MIXER("I14I15", SND_SOC_NOPM, 0, 0,
			   mt2701_afe_multi_ch_out_i2s1,
			   ARRAY_SIZE(mt2701_afe_multi_ch_out_i2s1)),
	SND_SOC_DAPM_MIXER("I16I17", SND_SOC_NOPM, 0, 0,
			   mt2701_afe_multi_ch_out_i2s2,
			   ARRAY_SIZE(mt2701_afe_multi_ch_out_i2s2)),
	SND_SOC_DAPM_MIXER("I18I19", SND_SOC_NOPM, 0, 0,
			   mt2701_afe_multi_ch_out_i2s3,
			   ARRAY_SIZE(mt2701_afe_multi_ch_out_i2s3)),

	SND_SOC_DAPM_MIXER("ASRC_O0", SND_SOC_NOPM, 0, 0,
			   mt2701_afe_multi_ch_out_asrc0,
			   ARRAY_SIZE(mt2701_afe_multi_ch_out_asrc0)),
	SND_SOC_DAPM_MIXER("ASRC_O1", SND_SOC_NOPM, 0, 0,
			   mt2701_afe_multi_ch_out_asrc1,
			   ARRAY_SIZE(mt2701_afe_multi_ch_out_asrc1)),
	SND_SOC_DAPM_MIXER("ASRC_O2", SND_SOC_NOPM, 0, 0,
			   mt2701_afe_multi_ch_out_asrc2,
			   ARRAY_SIZE(mt2701_afe_multi_ch_out_asrc2)),
	SND_SOC_DAPM_MIXER("ASRC_O3", SND_SOC_NOPM, 0, 0,
			   mt2701_afe_multi_ch_out_asrc3,
			   ARRAY_SIZE(mt2701_afe_multi_ch_out_asrc3)),
};

static const struct snd_soc_dapm_route mt2701_afe_pcm_routes[] = {
	{"I12", NULL, "DL1"},
	{"I13", NULL, "DL1"},
	{"I35", NULL, "DLBT"},

	{"I2S0 Playback", NULL, "O15"},
	{"I2S0 Playback", NULL, "O16"},

	{"I2S1 Playback", NULL, "O17"},
	{"I2S1 Playback", NULL, "O18"},
	{"I2S2 Playback", NULL, "O19"},
	{"I2S2 Playback", NULL, "O20"},
	{"I2S3 Playback", NULL, "O21"},
	{"I2S3 Playback", NULL, "O22"},
	{"BT Playback", NULL, "O31"},
	{"MOD Playback", NULL, "O25"},
	{"MOD Playback", NULL, "O26"},

	{"UL1", NULL, "O00"},
	{"UL1", NULL, "O01"},
	{"UL2", NULL, "O02"},
	{"UL2", NULL, "O03"},
	{"UL3", NULL, "O04"},
	{"UL3", NULL, "O05"},
	{"ULBT", NULL, "O14"},
	{"ULMOD", NULL, "O10"},
	{"ULMOD", NULL, "O11"},
	{"ULAWB2", NULL, "O32"},
	{"ULAWB2", NULL, "O33"},

	{"I00", NULL, "I2S0 Capture"},
	{"I01", NULL, "I2S0 Capture"},

	{"I02", NULL, "I2S1 Capture"},
	{"I03", NULL, "I2S1 Capture"},
	/* I02,03 link to UL2, also need to open I2S0 */
	{"I02", "I2S0 Switch", "I2S0 Capture"},
	{"I04", NULL, "I2S2 Capture"},
	{"I05", NULL, "I2S2 Capture"},

	{"I26", NULL, "BT Capture"},

	{"I08", NULL, "MOD Capture"},
	{"I09", NULL, "MOD Capture"},
	{"I31", NULL, "AADC Capture"},
	{"I32", NULL, "AADC Capture"},

	{"ASRC_O0", "Asrc0 out Switch", "DLM"},
	{"ASRC_O1", "Asrc1 out Switch", "DLM"},
	{"ASRC_O2", "Asrc2 out Switch", "DLM"},
	{"ASRC_O3", "Asrc3 out Switch", "DLM"},

	{"I12I13", "Multich I2S0 Out Switch", "ASRC_O0"},
	{"I14I15", "Multich I2S1 Out Switch", "ASRC_O1"},
	{"I16I17", "Multich I2S2 Out Switch", "ASRC_O2"},
	{"I18I19", "Multich I2S3 Out Switch", "ASRC_O3"},

	{"I12I13", "Multich DL1 Switch", "DLM"},
	{"I14I15", "Multich DL2 Switch", "DLM"},
	{"I16I17", "Multich DL3 Switch", "DLM"},
	{"I18I19", "Multich DL4 Switch", "DLM"},

	{"I12I13", NULL, "DL1"},
	{"I14I15", NULL, "DL2"},
	{"I16I17", NULL, "DL3"},
	{"I18I19", NULL, "DL4"},
	{"I22", NULL, "DLMOD"},
	{"I23", NULL, "DLMOD"},

	{ "I12", NULL, "I12I13" },
	{ "I13", NULL, "I12I13" },
	{ "I14", NULL, "I14I15" },
	{ "I15", NULL, "I14I15" },
	{ "I16", NULL, "I16I17" },
	{ "I17", NULL, "I16I17" },
	{ "I18", NULL, "I18I19" },
	{ "I19", NULL, "I18I19" },

	{ "O00", "I00 Switch", "I00" },
	{ "O01", "I01 Switch", "I01" },
	{ "O02", "I02 Switch", "I02" },
	{ "O03", "I03 Switch", "I03" },
	{ "O04", "I04 Switch", "I04" },
	{ "O05", "I05 Switch", "I05" },
	{ "O14", "I26 Switch", "I26" },
	{ "O15", "I12 Switch", "I12" },
	{ "O16", "I13 Switch", "I13" },
	{ "O17", "I14 Switch", "I14" },
	{ "O17", "I16 Switch", "I16" },
	{ "O17", "I18 Switch", "I18" },
	{ "O18", "I15 Switch", "I15" },
	{ "O18", "I17 Switch", "I17" },
	{ "O18", "I19 Switch", "I19" },
	{ "O19", "I14 Switch", "I14" },
	{ "O19", "I16 Switch", "I16" },
	{ "O19", "I18 Switch", "I18" },
	{ "O20", "I15 Switch", "I15" },
	{ "O20", "I17 Switch", "I17" },
	{ "O20", "I19 Switch", "I19" },
	{ "O21", "I14 Switch", "I14" },
	{ "O21", "I16 Switch", "I16" },
	{ "O21", "I18 Switch", "I18" },
	{ "O22", "I15 Switch", "I15" },
	{ "O22", "I17 Switch", "I17" },
	{ "O22", "I19 Switch", "I19" },
	{ "O31", "I35 Switch", "I35" },
	{ "O25", "I22 Switch", "I22" },
	{ "O26", "I23 Switch", "I23" },
	{ "O10", "I08 Switch", "I08" },
	{ "O11", "I09 Switch", "I09" },
	{ "O32", "I31 Switch", "I31" },
	{ "O33", "I32 Switch", "I32" },

	{"TDM0 Playback", NULL, "DLTDM0"},
	{"TDM1 Playback", NULL, "DLTDM1"},
	{"ULTDM", NULL, "TDM Capture"},

};

static const struct snd_soc_component_driver mt2701_afe_pcm_dai_component = {
	.name = "mt2701-afe-pcm-dai",
	.controls = mt2701_soc_controls,
	.num_controls = ARRAY_SIZE(mt2701_soc_controls),
	.dapm_widgets = mt2701_afe_pcm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt2701_afe_pcm_widgets),
	.dapm_routes = mt2701_afe_pcm_routes,
	.num_dapm_routes = ARRAY_SIZE(mt2701_afe_pcm_routes),
};

static const struct mtk_base_memif_data memif_data[MT2701_MEMIF_NUM] = {
	[MT2701_MEMIF_DL1] = {
		.name = "DL1",
		.id = MT2701_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 0,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_DAC_CON3,
		.mono_shift = 16,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 1,
		.hd_reg = AFE_MEMIF_HD_CON0,
		.hd_shift = 0,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 6,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 16,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 16,
	},
	[MT2701_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT2701_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 5,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_DAC_CON3,
		.mono_shift = 17,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 2,
		.hd_reg = AFE_MEMIF_HD_CON0,
		.hd_shift = 2,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 7,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 17,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 17,
	},
	[MT2701_MEMIF_DL3] = {
		.name = "DL3",
		.id = MT2701_MEMIF_DL3,
		.reg_ofs_base = AFE_DL3_BASE,
		.reg_ofs_cur = AFE_DL3_CUR,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 10,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_DAC_CON3,
		.mono_shift = 18,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 3,
		.hd_reg = AFE_MEMIF_HD_CON0,
		.hd_shift = 4,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 8,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 21,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 21,
	},
	[MT2701_MEMIF_DL4] = {
		.name = "DL4",
		.id = MT2701_MEMIF_DL4,
		.reg_ofs_base = AFE_DL4_BASE,
		.reg_ofs_cur = AFE_DL4_CUR,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 15,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_DAC_CON3,
		.mono_shift = 19,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 4,
		.hd_reg = AFE_MEMIF_HD_CON0,
		.hd_shift = 6,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 9,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 22,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 22,
	},
	[MT2701_MEMIF_DL5] = {
		.name = "DL5",
		.id = MT2701_MEMIF_DL5,
		.reg_ofs_base = AFE_DL5_BASE,
		.reg_ofs_cur = AFE_DL5_CUR,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 20,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_DAC_CON3,
		.mono_shift = 20,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 5,
		.hd_reg = AFE_MEMIF_HD_CON0,
		.hd_shift = 8,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 10,
		.msb_reg = -1,
		.msb_shift = -1,
		.msb2_reg = -1,
		.msb2_shift = -1,
	},
	[MT2701_MEMIF_DLM] = {
		.name = "DLM",
		.id = MT2701_MEMIF_DLM,
		.reg_ofs_base = AFE_DLMCH_BASE,
		.reg_ofs_cur = AFE_DLMCH_CUR,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = 0,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 7,
		.hd_reg = AFE_MEMIF_PBUF_SIZE,
		.hd_shift = 28,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 12,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 25,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 25,
	},
	[MT2701_MEMIF_UL1] = {
		.name = "UL1",
		.id = MT2701_MEMIF_UL1,
		.reg_ofs_base = AFE_VUL_BASE,
		.reg_ofs_cur = AFE_VUL_CUR,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = 0,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_DAC_CON4,
		.mono_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 10,
		.hd_reg = AFE_MEMIF_HD_CON1,
		.hd_shift = 0,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 0,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 1,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 1,
	},
	[MT2701_MEMIF_UL2] = {
		.name = "UL2",
		.id = MT2701_MEMIF_UL2,
		.reg_ofs_base = AFE_UL2_BASE,
		.reg_ofs_cur = AFE_UL2_CUR,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = 5,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_DAC_CON4,
		.mono_shift = 2,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 11,
		.hd_reg = AFE_MEMIF_HD_CON1,
		.hd_shift = 2,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 1,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 4,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 4,
	},
	[MT2701_MEMIF_UL3] = {
		.name = "UL3",
		.id = MT2701_MEMIF_UL3,
		.reg_ofs_base = AFE_UL3_BASE,
		.reg_ofs_cur = AFE_UL3_CUR,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = 10,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_DAC_CON4,
		.mono_shift = 4,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 12,
		.hd_reg = AFE_MEMIF_HD_CON1,
		.hd_shift = 4,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 2,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 5,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 5,
	},
	[MT2701_MEMIF_UL4] = {
		.name = "UL4",
		.id = MT2701_MEMIF_UL4,
		.reg_ofs_base = AFE_UL4_BASE,
		.reg_ofs_cur = AFE_UL4_CUR,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = 15,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_DAC_CON4,
		.mono_shift = 6,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 13,
		.hd_reg = AFE_MEMIF_HD_CON0,
		.hd_shift = 6,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 3,
		.msb_reg = -1,
		.msb_shift = -1,
		.msb2_reg = -1,
		.msb2_shift = -1,
	},
	[MT2701_MEMIF_UL5] = {
		.name = "UL5",
		.id = MT2701_MEMIF_UL5,
		.reg_ofs_base = AFE_UL5_BASE,
		.reg_ofs_cur = AFE_UL5_CUR,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = 20,
		.mono_reg = AFE_DAC_CON4,
		.mono_shift = 8,
		.fs_maskbit = 0x1f,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 14,
		.hd_reg = AFE_MEMIF_HD_CON0,
		.hd_shift = 8,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 4,
		.msb_reg = -1,
		.msb_shift = -1,
		.msb2_reg = -1,
		.msb2_shift = -1,
	},
	[MT2701_MEMIF_AWB2] = {
		.name = "ULAWB2",
		.id = MT2701_MEMIF_AWB2,
		.reg_ofs_base = AFE_AWB2_BASE,
		.reg_ofs_cur = AFE_AWB2_CUR,
		.fs_reg = AFE_DAC_CON3,
		.fs_shift = 5,
		.mono_reg = AFE_DAC_CON4,
		.mono_shift = 18,
		.fs_maskbit = 0x1f,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 21,
		.hd_reg = AFE_MEMIF_HD_CON1,
		.hd_shift = 16,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 15,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 10,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 10,
	},
	[MT2701_MEMIF_DLBT] = {
		.name = "DLBT",
		.id = MT2701_MEMIF_DLBT,
		.reg_ofs_base = AFE_ARB1_BASE,
		.reg_ofs_cur = AFE_ARB1_CUR,
		.fs_reg = AFE_DAC_CON3,
		.fs_shift = 10,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_DAC_CON3,
		.mono_shift = 22,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 8,
		.hd_reg = AFE_MEMIF_HD_CON0,
		.hd_shift = 14,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 13,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 26,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 26,
	},
	[MT2701_MEMIF_ULBT] = {
		.name = "ULBT",
		.id = MT2701_MEMIF_ULBT,
		.reg_ofs_base = AFE_DAI_BASE,
		.reg_ofs_cur = AFE_DAI_CUR,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = 30,
		.fs_maskbit = 0x1,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 17,
		.hd_reg = AFE_MEMIF_HD_CON1,
		.hd_shift = 20,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 16,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 3,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 3,
	},
	[MT2701_MEMIF_DLMOD] = {
		.name = "DLMOD",
		.id = MT2701_MEMIF_DLMOD,
		.reg_ofs_base = AFE_PCMO_BASE,
		.reg_ofs_cur = AFE_PCMO_CUR,
		.fs_reg = -1,
		.fs_shift = -1,
		.fs_maskbit = 0x1,
		.mono_reg = AFE_DAC_CON3,
		.mono_shift = 23,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 9,
		.hd_reg = AFE_MEMIF_HD_CON0,
		.hd_shift = 16,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 20,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 20,
	},
	[MT2701_MEMIF_ULMOD] = {
		.name = "ULMOD",
		.id = MT2701_MEMIF_ULMOD,
		.reg_ofs_base = AFE_PCMI_BASE,
		.reg_ofs_cur = AFE_PCMI_CUR,
		.fs_reg = -1,
		.fs_shift = -1,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_DAC_CON4,
		.mono_shift = 6,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 13,
		.hd_reg = AFE_MEMIF_HD_CON1,
		.hd_shift = 6,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 2,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 2,
	},
	[MT2701_MEMIF_DLTDM1] = {
		.name = "DLTDM1",
		.id = MT2701_MEMIF_DLTDM1,
		.reg_ofs_base = AFE_TDM_G1_BASE,
		.reg_ofs_cur = AFE_TDM_G1_CUR,
		.fs_reg = -1,
		.fs_shift = -1,
		.fs_maskbit = 0x0,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 5,
		.hd_reg = AFE_MEMIF_HD_CON0,
		.hd_shift = 8,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 23,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 23,
	},
	[MT2701_MEMIF_DLTDM2] = {
		.name = "DLTDM2",
		.id = MT2701_MEMIF_DLTDM2,
		.reg_ofs_base = AFE_TDM_G2_BASE,
		.reg_ofs_cur = AFE_TDM_G2_CUR,
		.fs_reg = -1,
		.fs_shift = -1,
		.fs_maskbit = 0x0,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 6,
		.hd_reg = AFE_MEMIF_HD_CON0,
		.hd_shift = 10,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 24,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 24,
	},
	[MT2701_MEMIF_ULTDM] = {
		.name = "ULTDM",
		.id = MT2701_MEMIF_ULTDM,
		.reg_ofs_base = AFE_TDM_IN_BASE,
		.reg_ofs_cur = AFE_TDM_IN_CUR,
		.fs_reg = -1,
		.fs_shift = -1,
		.fs_maskbit = 0x0,
		.mono_reg = -1,
		.mono_shift = -1,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 16,
		.hd_reg = AFE_MEMIF_HD_CON1,
		.hd_shift = 12,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = AFE_MEMIF_BASE_MSB,
		.msb_shift = 9,
		.msb2_reg = AFE_MEMIF_END_MSB,
		.msb2_shift = 9,
	},
};

static const struct mtk_base_irq_data irq_data[MT2701_IRQ_ASYS_END] = {
	{
		.id = MT2701_IRQ_ASYS_IRQ1,
		.irq_cnt_reg = ASYS_IRQ1_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ1_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_en_reg = ASYS_IRQ1_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 0,
	},
	{
		.id = MT2701_IRQ_ASYS_IRQ2,
		.irq_cnt_reg = ASYS_IRQ2_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ2_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_en_reg = ASYS_IRQ2_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 1,
	},
	{
		.id = MT2701_IRQ_ASYS_IRQ3,
		.irq_cnt_reg = ASYS_IRQ3_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ3_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_en_reg = ASYS_IRQ3_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 2,
	},
	{
		.id = MT2701_IRQ_ASYS_IRQ4,
		.irq_cnt_reg = ASYS_IRQ4_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ4_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_en_reg = ASYS_IRQ4_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 3,
	},
	{
		.id = MT2701_IRQ_ASYS_IRQ5,
		.irq_cnt_reg = ASYS_IRQ5_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ5_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_en_reg = ASYS_IRQ5_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 4,
	},
	{
		.id = MT2701_IRQ_ASYS_IRQ6,
		.irq_cnt_reg = ASYS_IRQ6_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ6_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1f,
		.irq_en_reg = ASYS_IRQ6_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = ASYS_IRQ_CLR,
		.irq_clr_shift = 5,
	}
};

static const struct mt2701_i2s_data mt2701_i2s_data[MT2701_I2S_NUM][2] = {
	{
		{
			.i2s_ctrl_reg = ASYS_I2SO1_CON,
			.i2s_pwn_shift = 6,
			.i2s_asrc_fs_shift = 0,
			.i2s_asrc_fs_mask = 0x1f,

		},
		{
			.i2s_ctrl_reg = ASYS_I2SIN1_CON,
			.i2s_pwn_shift = 0,
			.i2s_asrc_fs_shift = 0,
			.i2s_asrc_fs_mask = 0x1f,

		},
	},
	{
		{
			.i2s_ctrl_reg = ASYS_I2SO2_CON,
			.i2s_pwn_shift = 7,
			.i2s_asrc_fs_shift = 5,
			.i2s_asrc_fs_mask = 0x1f,

		},
		{
			.i2s_ctrl_reg = ASYS_I2SIN2_CON,
			.i2s_pwn_shift = 1,
			.i2s_asrc_fs_shift = 5,
			.i2s_asrc_fs_mask = 0x1f,

		},
	},
	{
		{
			.i2s_ctrl_reg = ASYS_I2SO3_CON,
			.i2s_pwn_shift = 8,
			.i2s_asrc_fs_shift = 10,
			.i2s_asrc_fs_mask = 0x1f,

		},
		{
			.i2s_ctrl_reg = ASYS_I2SIN3_CON,
			.i2s_pwn_shift = 2,
			.i2s_asrc_fs_shift = 10,
			.i2s_asrc_fs_mask = 0x1f,

		},
	},
	{
		{
			.i2s_ctrl_reg = ASYS_I2SO4_CON,
			.i2s_pwn_shift = 9,
			.i2s_asrc_fs_shift = 15,
			.i2s_asrc_fs_mask = 0x1f,

		},
		{
			.i2s_ctrl_reg = ASYS_I2SIN4_CON,
			.i2s_pwn_shift = 3,
			.i2s_asrc_fs_shift = 15,
			.i2s_asrc_fs_mask = 0x1f,

		},
	},
};

static const struct mt2701_tdm_data mt2701_tdm_data[MT2701_TDM_NUM] = {
	{
		.tdm_bck_reg = AUDIO_TOP_CON2,
		.tdm_plldiv_shift = 8,
		.tdm_pll_sel_shift = 0,
		.tdm_bck_on_shift = 1,
		.tdm_ctrl_reg = AFE_TDM_G1_CON1,
		.tdm_ctrl_2nd_reg = AFE_TDM_G1_CON2,
		.tdm_conn_reg = AFE_TDM_G1_CONN_CON0,
		.tdm_conn_2nd_reg = AFE_TDM_G1_CONN_CON1,
		.tdm_lrck_cycle_shift = 10,
		.tdm_on_shift = 0,
		.tdm_agent_reg = AFE_TDM_AGENT_CFG,
		.tdm_agent_bit_width_shift = 5,
		.tdm_agent_ch_num_shift = 0,
	},
	{
		.tdm_bck_reg = AUDIO_TOP_CON2,
		.tdm_plldiv_shift = 24,
		.tdm_pll_sel_shift = 16,
		.tdm_bck_on_shift = 17,
		.tdm_ctrl_reg = AFE_TDM_G2_CON1,
		.tdm_ctrl_2nd_reg = AFE_TDM_G2_CON2,
		.tdm_conn_reg = AFE_TDM_G2_CONN_CON0,
		.tdm_conn_2nd_reg = AFE_TDM_G2_CONN_CON1,
		.tdm_lrck_cycle_shift = 10,
		.tdm_on_shift = 0,
		.tdm_agent_reg = AFE_TDM_AGENT_CFG,
		.tdm_agent_bit_width_shift = 21,
		.tdm_agent_ch_num_shift = 16,
	},
	{
		.tdm_bck_reg = AUDIO_TOP_CON1,
		.tdm_plldiv_shift = 24,
		.tdm_pll_sel_shift = 16,
		.tdm_bck_on_shift = 17,
		.tdm_ctrl_reg = AFE_TDM_IN_CON1,
		.tdm_ctrl_2nd_reg = AFE_TDM_IN_CON2,
		.tdm_conn_reg = -1,
		.tdm_conn_2nd_reg = -1,
		.tdm_lrck_cycle_shift = 16,
		.tdm_on_shift = 0,
		.tdm_agent_reg = -1,
		.tdm_agent_bit_width_shift = -1,
		.tdm_agent_ch_num_shift = -1,
	},
};

static const struct regmap_config mt2701_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = AFE_END_ADDR,
	.cache_type = REGCACHE_NONE,
};

struct clock_ctrl mt2701_clk_ctrl = {
	.init_clock = mt2701_init_clock,
	.afe_enable_clock = mt2701_afe_enable_clock,
	.afe_disable_clock = mt2701_afe_disable_clock,
	.mclk_configuration = mt2701_mclk_configuration,
	.enable_mclk = mt2701_turn_on_mclk,
	.disable_mclk = mt2701_turn_off_mclk,
	.apll0_rate = MT2701_PLL_DOMAIN_0_RATE,
	.apll1_rate = MT2701_PLL_DOMAIN_1_RATE,
};

struct clock_ctrl mt2712_clk_ctrl = {
	.init_clock = mt2712_init_clock,
	.afe_enable_clock = mt2712_afe_enable_clock,
	.afe_disable_clock = mt2712_afe_disable_clock,
	.mclk_configuration = mt2712_mclk_configuration,
	.enable_mclk = mt2712_turn_on_mclk,
	.disable_mclk = mt2712_turn_off_mclk,
	.apll0_rate = MT2712_PLL_DOMAIN_0_RATE,
	.apll1_rate = MT2712_PLL_DOMAIN_1_RATE,
};


static const struct of_device_id mt2701_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt2701-audio", .data = &mt2701_clk_ctrl},
	{ .compatible = "mediatek,mt2712-audio", .data = &mt2712_clk_ctrl},
	{},
};
MODULE_DEVICE_TABLE(of, mt2701_afe_pcm_dt_match);

static irqreturn_t mt2701_asys_isr(int irq_id, void *dev)
{
	int id;
	struct mtk_base_afe *afe = dev;
	struct mtk_base_afe_memif *memif;
	struct mtk_base_afe_irq *irq;
	u32 status;

	mt2701_regmap_read(afe->regmap, ASYS_IRQ_STATUS, &status);
	mt2701_regmap_write(afe->regmap, ASYS_IRQ_CLR, status);

	for (id = MT2701_MEMIF_DL1; id < MT2701_MEMIF_NUM; ++id) {
		memif = &afe->memif[id];
		if (memif->irq_usage < 0)
			continue;
		irq = &afe->irqs[memif->irq_usage];
		if ((status & 1U << (irq->irq_data->irq_clr_shift)) != 0)
			snd_pcm_period_elapsed(memif->substream);
	}
	return IRQ_HANDLED;
}

static int mt2701_afe_runtime_suspend(struct device *dev)
{
	return 0;
}

static int mt2701_afe_runtime_resume(struct device *dev)
{
	return 0;
}

static int mt2701_afe_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;

	afe_priv->clk_ctrl->afe_disable_clock(afe);
	return 0;
}

static int mt2701_afe_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	int ret;

	ret = afe_priv->clk_ctrl->afe_enable_clock(afe);
	return ret;
}


static int mt2701_afe_pcm_dev_probe(struct platform_device *pdev)
{
	int ret, i;
	unsigned int irq_id;
	struct mtk_base_afe *afe;
	struct mt2701_afe_private *afe_priv;
	struct resource *res;
	struct device *dev;
	const struct of_device_id *of_id;
	int tdm_mode = TDM_MODE_SEPCLK;
	int i2s_mode[3];

	ret = 0;
	afe = devm_kzalloc(&pdev->dev, sizeof(*afe), GFP_KERNEL);
	if (afe == NULL)
		return -ENOMEM;
	afe->platform_priv = devm_kzalloc(&pdev->dev, sizeof(*afe_priv),
					  GFP_KERNEL);
	if (afe->platform_priv == NULL)
		return -ENOMEM;
	afe_priv = afe->platform_priv;

	afe->dev = &pdev->dev;
	dev = afe->dev;

	irq_id = platform_get_irq(pdev, 0);
	if (irq_id == 0U) {
		dev_err(dev, "%s no irq found\n", dev->of_node->name);
		return -ENXIO;
	}
	ret = devm_request_irq(dev, irq_id, mt2701_asys_isr,
			       IRQF_TRIGGER_NONE, "asys-isr", (void *)afe);
	if (ret != 0) {
		dev_err(dev, "could not request_irq for asys-isr\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	afe->base_addr = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(afe->base_addr))
		return PTR_ERR(afe->base_addr);

	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->base_addr,
		&mt2701_afe_regmap_config);
	if (IS_ERR(afe->regmap))
		return PTR_ERR(afe->regmap);

	mutex_init(&afe->irq_alloc_lock);

	/* memif initialize */
	afe->memif_size = MT2701_MEMIF_NUM;
	afe->memif = devm_kcalloc(dev, afe->memif_size, sizeof(*afe->memif),
				  GFP_KERNEL);

	if (afe->memif == NULL)
		return -ENOMEM;

	for (i = MT2701_MEMIF_DL1; i < afe->memif_size; i++) {
		afe->memif[i].data = &memif_data[i];
		afe->memif[i].irq_usage = -1;
	}

	/* irq initialize */
	afe->irqs_size = MT2701_IRQ_ASYS_END;
	afe->irqs = devm_kcalloc(dev, afe->irqs_size, sizeof(*afe->irqs),
				 GFP_KERNEL);

	if (afe->irqs == NULL)
		return -ENOMEM;

	for (i = 0; i < afe->irqs_size; i++)
		afe->irqs[i].irq_data = &irq_data[i];

	/* I2S initialize */
	for (i = 0; i < MT2701_I2S_NUM; i++) {
		afe_priv->i2s_path[i].i2s_data[I2S_OUT]
			= &mt2701_i2s_data[i][I2S_OUT];
		afe_priv->i2s_path[i].i2s_data[I2S_IN]
			= &mt2701_i2s_data[i][I2S_IN];
		afe_priv->i2s_path[i].i2s_mode = I2S_COCLK;
	}

	if (of_device_is_compatible(afe->dev->of_node, "mediatek,mt2712-audio") != 0) {
		of_property_read_u32_array(afe->dev->of_node, "i2s-mode", (unsigned int *)i2s_mode, 3);
		for (i = 0; i < 3; i++)
			afe_priv->i2s_path[i].i2s_mode = (enum mt2701_i2s_mode)i2s_mode[i];
	}

	/* TDM initialize */
	for (i = 0; i < MT2701_TDM_NUM; i++) {
		afe_priv->tdm_path[i].tdm_data
			= &mt2701_tdm_data[i];
		afe_priv->tdm_path[i].tdm_data
			= &mt2701_tdm_data[i];
	}

	of_property_read_s32(afe->dev->of_node, "tdm-mode", &tdm_mode);

	switch (tdm_mode) {
	case TDM_MODE_COCLK_O1:
		afe_priv->tdm_coclk_info.src = MT2701_TDMO_1;
		afe_priv->tdm_coclk = true;
		break;
	case TDM_MODE_COCLK_O2:
		afe_priv->tdm_coclk_info.src = MT2701_TDMO_2;
		afe_priv->tdm_coclk = true;
		break;
	case TDM_MODE_SEPCLK:
		afe_priv->tdm_coclk = false;
		break;
	}

	if (afe_priv->tdm_coclk) {
		struct snd_soc_dai_driver *dai_driver;

		dai_driver = find_dai_driver_by_id(mt2701_afe_pcm_dais, ARRAY_SIZE(mt2701_afe_pcm_dais),
				afe_priv->tdm_coclk_info.src+MT2701_IO_TDMO1);
		if (dai_driver != 0)
			dai_driver->ops = &mt2701_tdmio_coclk_ops;
		dai_driver = find_dai_driver_by_id(mt2701_afe_pcm_dais, ARRAY_SIZE(mt2701_afe_pcm_dais),
				 MT2701_IO_TDMI);
		if (dai_driver != 0)
			dai_driver->ops = &mt2701_tdmio_coclk_ops;

	}

	afe_priv->tdm_coclk_info.on = 0;
	afe_priv->tdm_coclk_info.hw_on = 0;

	of_property_read_u32_array(afe->dev->of_node, "tdm-in-lrck-setting", (unsigned int *)&afe_priv->tdm_in_lrck, 2);

	afe->mtk_afe_hardware = &mt2701_afe_hardware;
	afe->memif_fs = mt2701_memif_fs;
	afe->irq_fs = mt2701_irq_fs;

	afe->reg_back_up_list = mt2701_afe_backup_list;
	afe->reg_back_up_list_num = ARRAY_SIZE(mt2701_afe_backup_list);
	afe->runtime_resume = mt2701_afe_runtime_resume;
	afe->runtime_suspend = mt2701_afe_runtime_suspend;

	/* initial audio related clock */
	of_id = of_match_device(mt2701_afe_pcm_dt_match, dev);
	if (of_id != 0)
		afe_priv->clk_ctrl = (struct clock_ctrl *)of_id->data;
	else{
		dev_err(dev, "init clock ctrl error\n");
		return -ENXIO;
	}

	ret = afe_priv->clk_ctrl->init_clock(afe);
	if (ret != 0) {
		dev_err(dev, "init clock error\n");
		return ret;
	}

	/* set 4GB dma mask */
	ret = dma_set_mask(dev, DMA_BIT_MASK(33U));
	if (ret != 0)
		return ret;

	platform_set_drvdata(pdev, afe);
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev))
		goto err_pm_disable;
	pm_runtime_get_sync(&pdev->dev);

	ret = snd_soc_register_platform(&pdev->dev, &mtk_afe_pcm_platform);
	if (ret != 0) {
		dev_warn(dev, "err_platform\n");
		goto err_platform;
	}

	ret = snd_soc_register_component(&pdev->dev,
					 &mt2701_afe_pcm_dai_component,
					 mt2701_afe_pcm_dais,
					 ARRAY_SIZE(mt2701_afe_pcm_dais));
	if (ret != 0) {
		dev_warn(dev, "err_dai_component\n");
		goto err_dai_component;
	}

	ret = mt2701_afe_resume(&pdev->dev);
	if (ret != 0) {
		dev_info(dev, "err_open_audio_clock\n");
		goto err_open_audio_clock;
	}

	return 0;

err_open_audio_clock:
err_dai_component:
	snd_soc_unregister_component(&pdev->dev);

err_platform:
	snd_soc_unregister_platform(&pdev->dev);

err_pm_disable:
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static int mt2701_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mtk_base_afe *afe = platform_get_drvdata(pdev);
	int ret;

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev)) {
		ret = mt2701_afe_suspend(&pdev->dev);
		if (ret != 0) {
			dev_info(&pdev->dev, "err_close_audio_clock\n");
			goto err_close_audio_clock;
		}
	}
	pm_runtime_put_sync(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	snd_soc_unregister_platform(&pdev->dev);
	/* disable afe clock */
	mt2701_afe_disable_clock(afe);
	return 0;

err_close_audio_clock:
	return ret;
}

static const struct dev_pm_ops mt2701_afe_pm_ops = {
	.suspend = mt2701_afe_suspend,
	.resume = mt2701_afe_resume,
};

static struct platform_driver mt2701_afe_pcm_driver = {
	.driver = {
		   .name = "mt2701-audio",
		   .of_match_table = mt2701_afe_pcm_dt_match,
#ifdef CONFIG_PM
		   .pm = &mt2701_afe_pm_ops,
#endif
	},
	.probe = mt2701_afe_pcm_dev_probe,
	.remove = mt2701_afe_pcm_dev_remove,
};

module_platform_driver(mt2701_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC AFE platform driver for 2701");
MODULE_AUTHOR("Garlic Tseng <garlic.tseng@mediatek.com>");
MODULE_LICENSE("GPL v2");

