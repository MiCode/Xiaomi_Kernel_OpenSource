/*
 * Mediatek ALSA SoC AFE platform driver
 *
 * Copyright (c) 2016 MediaTek Inc.
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
#include "mtk-afe-common.h"
#include "mtk-afe-regs.h"
#include "mtk-afe-util.h"
#include "mtk-afe-controls.h"
#include "mtk-afe-debug.h"


static const unsigned int mtk_afe_backup_list[] = {
	AUDIO_TOP_CON0,
	AFE_CONN0,
	AFE_CONN1,
	AFE_CONN2,
	AFE_CONN3,
	AFE_CONN5,
	AFE_CONN_24BIT,
	AFE_DAC_CON0,
	AFE_DAC_CON1,
	AFE_DL1_BASE,
	AFE_DL1_END,
	AFE_DL2_BASE,
	AFE_DL2_END,
	AFE_VUL_BASE,
	AFE_VUL_END,
	AFE_AWB_BASE,
	AFE_AWB_END,
	AFE_DAI_BASE,
	AFE_DAI_END,
	AFE_HDMI_OUT_BASE,
	AFE_HDMI_OUT_END,
};

static const struct snd_pcm_hardware mtk_afe_hardware = {
	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_MMAP_VALID,
	.buffer_bytes_max = 1024 * 1024,
	.period_bytes_min = 256,
	.period_bytes_max = 512 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.fifo_size = 0,
};

static snd_pcm_uframes_t mtk_afe_pcm_pointer
			 (struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_memif *memif = &afe->memif[rtd->cpu_dai->id];
	unsigned int hw_ptr;
	int ret;

	ret = regmap_read(afe->regmap, memif->data->reg_ofs_cur, &hw_ptr);
	if (ret || hw_ptr == 0) {
		dev_err(afe->dev, "%s hw_ptr err ret = %d\n", __func__, ret);
		hw_ptr = memif->phys_buf_addr;
	} else if (memif->use_sram) {
		/* enforce natural alignment to 8 bytes */
		hw_ptr &= ~7;
	}

	return bytes_to_frames(substream->runtime,
			       hw_ptr - memif->phys_buf_addr);
}


static const struct snd_pcm_ops mtk_afe_pcm_ops = {
	.ioctl = snd_pcm_lib_ioctl,
	.pointer = mtk_afe_pcm_pointer,
};

static int mtk_afe_pcm_probe(struct snd_soc_platform *platform)
{
	return mtk_afe_add_controls(platform);
}

static int mtk_afe_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	size_t size = afe->memif[rtd->cpu_dai->id].data->prealloc_size;
	struct snd_pcm_substream *substream;
	int stream;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (substream) {
			struct snd_dma_buffer *buf = &substream->dma_buffer;

			buf->dev.type = SNDRV_DMA_TYPE_DEV;
			buf->dev.dev = card->dev;
			buf->private_data = NULL;
		}
	}

	if (size > 0)
		return snd_pcm_lib_preallocate_pages_for_all(pcm,
							     SNDRV_DMA_TYPE_DEV,
							     card->dev,
							     size, size);

	return 0;
}

static void mtk_afe_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static const struct snd_soc_platform_driver mtk_afe_pcm_platform = {
	.probe = mtk_afe_pcm_probe,
	.pcm_new = mtk_afe_pcm_new,
	.pcm_free = mtk_afe_pcm_free,
	.ops = &mtk_afe_pcm_ops,
};

struct mtk_afe_rate {
	unsigned int rate;
	unsigned int regvalue;
};

static const struct mtk_afe_rate mtk_afe_i2s_rates[] = {
	{ .rate = 8000, .regvalue = 0 },
	{ .rate = 11025, .regvalue = 1 },
	{ .rate = 12000, .regvalue = 2 },
	{ .rate = 16000, .regvalue = 4 },
	{ .rate = 22050, .regvalue = 5 },
	{ .rate = 24000, .regvalue = 6 },
	{ .rate = 32000, .regvalue = 8 },
	{ .rate = 44100, .regvalue = 9 },
	{ .rate = 48000, .regvalue = 10 },
	{ .rate = 88000, .regvalue = 11 },
	{ .rate = 96000, .regvalue = 12 },
	{ .rate = 174000, .regvalue = 13 },
	{ .rate = 192000, .regvalue = 14 },
};

static int mtk_afe_i2s_fs(unsigned int sample_rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtk_afe_i2s_rates); i++)
		if (mtk_afe_i2s_rates[i].rate == sample_rate)
			return mtk_afe_i2s_rates[i].regvalue;

	return -EINVAL;
}

static int mtk_afe_set_i2s_out(struct mtk_afe *afe, unsigned int rate,
	int bit_width)
{
	unsigned int val;
	int fs = mtk_afe_i2s_fs(rate);

	if (fs < 0)
		return -EINVAL;

	val = AFE_I2S_CON1_TDMOUT_MUX |
	      AFE_I2S_CON1_LOW_JITTER_CLK |
	      AFE_I2S_CON1_RATE(fs) |
	      AFE_I2S_CON1_FORMAT_I2S;

	if (bit_width > 16)
		val |= AFE_I2S_CON1_WLEN_32BIT;

	regmap_update_bits(afe->regmap, AFE_I2S_CON1, ~AFE_I2S_CON1_EN, val);

	return 0;
}

static int mtk_afe_set_2nd_i2s_out(struct mtk_afe *afe, unsigned int rate,
	int bit_width)
{
	unsigned int val;
	int fs = mtk_afe_i2s_fs(rate);

	if (fs < 0)
		return -EINVAL;

	val = AFE_I2S_CON3_LOW_JITTER_CLK |
	      AFE_I2S_CON3_RATE(fs) |
	      AFE_I2S_CON3_FORMAT_I2S;

	if (bit_width > 16)
		val |= AFE_I2S_CON3_WLEN_32BIT;

	regmap_update_bits(afe->regmap, AFE_I2S_CON3, ~AFE_I2S_CON3_EN, val);

	return 0;
}

static int mtk_afe_set_i2s_in(struct mtk_afe *afe, unsigned int rate,
	int bit_width)
{
	unsigned int val;
	int fs = mtk_afe_i2s_fs(rate);

	if (fs < 0)
		return -EINVAL;

	val = AFE_I2S_CON2_LOW_JITTER_CLK |
	      AFE_I2S_CON2_RATE(fs) |
	      AFE_I2S_CON2_FORMAT_I2S;

	if (bit_width > 16)
		val |= AFE_I2S_CON2_WLEN_32BIT;

	regmap_update_bits(afe->regmap, AFE_I2S_CON2, ~AFE_I2S_CON2_EN, val);

	regmap_update_bits(afe->regmap, AFE_ADDA_TOP_CON0, 0x1, 0x1);

	return 0;
}

static int mtk_afe_set_2nd_i2s_in(struct mtk_afe *afe, unsigned int rate,
	int bit_width)
{
	unsigned int val;
	int fs = mtk_afe_i2s_fs(rate);

	if (fs < 0)
		return -EINVAL;

	regmap_update_bits(afe->regmap, AFE_DAC_CON1, 0xf << 8, fs << 8);

	val = AFE_I2S_CON_PHASE_SHIFT_FIX |
	      AFE_I2S_CON_FROM_IO_MUX |
	      AFE_I2S_CON_LOW_JITTER_CLK |
	      AFE_I2S_CON_FORMAT_I2S;

	if (bit_width > 16)
		val |= AFE_I2S_CON_WLEN_32BIT;

	regmap_update_bits(afe->regmap, AFE_I2S_CON, ~AFE_I2S_CON_EN, val);

	return 0;
}

static void mtk_afe_set_i2s_out_enable(struct mtk_afe *afe, bool enable)
{
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);

	if (enable) {
		afe->i2s_out_on_ref_cnt++;
		if (afe->i2s_out_on_ref_cnt == 1)
			regmap_update_bits(afe->regmap, AFE_I2S_CON1, 0x1, enable);
	} else {
		afe->i2s_out_on_ref_cnt--;
		if (afe->i2s_out_on_ref_cnt == 0)
			regmap_update_bits(afe->regmap, AFE_I2S_CON1, 0x1, enable);
		else if (afe->i2s_out_on_ref_cnt < 0)
			afe->i2s_out_on_ref_cnt = 0;
	}

	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);
}

static void mtk_afe_set_2nd_i2s_out_enable(struct mtk_afe *afe, bool enable)
{
	regmap_update_bits(afe->regmap, AFE_I2S_CON3, 0x1, enable);
}

static void mtk_afe_set_i2s_in_enable(struct mtk_afe *afe, bool enable)
{
	regmap_update_bits(afe->regmap, AFE_I2S_CON2, 0x1, enable);
}

static void mtk_afe_set_2nd_i2s_in_enable(struct mtk_afe *afe, bool enable)
{
	regmap_update_bits(afe->regmap, AFE_I2S_CON, 0x1, enable);
}

static int mtk_afe_enable_adda_on(struct mtk_afe *afe)
{
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);

	afe->adda_afe_on_ref_cnt++;
	if (afe->adda_afe_on_ref_cnt == 1)
		regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0, 0x1, 0x1);

	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	return 0;
}

static int mtk_afe_disable_adda_on(struct mtk_afe *afe)
{
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);

	afe->adda_afe_on_ref_cnt--;
	if (afe->adda_afe_on_ref_cnt == 0)
		regmap_update_bits(afe->regmap, AFE_ADDA_UL_DL_CON0, 0x1, 0x0);
	else if (afe->adda_afe_on_ref_cnt < 0)
		afe->adda_afe_on_ref_cnt = 0;

	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	return 0;
}

static int mtk_afe_set_adda_out(struct mtk_afe *afe, unsigned int rate)
{
	unsigned int val = 0;

	switch (rate) {
	case 8000:
		val |= (0 << 28) | AFE_ADDA_DL_VOICE_DATA;
		break;
	case 11025:
		val |= 1 << 28;
		break;
	case 12000:
		val |= 2 << 28;
		break;
	case 16000:
		val |= (3 << 28) | AFE_ADDA_DL_VOICE_DATA;
		break;
	case 22050:
		val |= 4 << 28;
		break;
	case 24000:
		val |= 5 << 28;
		break;
	case 32000:
		val |= 6 << 28;
		break;
	case 44100:
		val |= 7 << 28;
		break;
	case 48000:
		val |= 8 << 28;
		break;
	default:
		return -EINVAL;
	}

	val |= AFE_ADDA_DL_8X_UPSAMPLE |
	       AFE_ADDA_DL_MUTE_OFF |
	       AFE_ADDA_DL_DEGRADE_GAIN;

	regmap_update_bits(afe->regmap, AFE_ADDA_PREDIS_CON0, 0xffffffff, 0);
	regmap_update_bits(afe->regmap, AFE_ADDA_PREDIS_CON1, 0xffffffff, 0);
	regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0, 0xffffffff, val);
	regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON1, 0xffffffff, 0xf74f0000);

	return 0;
}

static int mtk_afe_set_adda_in(struct mtk_afe *afe, unsigned int rate)
{
	unsigned int val = 0;
	unsigned int val2 = 0;

	switch (rate) {
	case 8000:
		val |= (0 << 17) | (0 << 19);
		val2 |= 1 << 10;
		break;
	case 16000:
		val |= (1 << 17) | (1 << 19);
		val2 |= 1 << 10;
		break;
	case 32000:
		val |= (2 << 17) | (2 << 19);
		val2 |= 1 << 10;
		break;
	case 48000:
		val |= (3 << 17) | (3 << 19);
		val2 |= 3 << 10;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0, 0x001e0000, val);

	regmap_update_bits(afe->regmap, AFE_ADDA_NEWIF_CFG1, 0xc00, val2);

	regmap_update_bits(afe->regmap, AFE_ADDA_TOP_CON0, 0x1, 0x0);

	return 0;
}

static void mtk_afe_set_adda_out_enable(struct mtk_afe *afe, bool enable)
{
	regmap_update_bits(afe->regmap, AFE_ADDA_DL_SRC2_CON0, 0x1, enable);

	if (enable)
		mtk_afe_enable_adda_on(afe);
	else
		mtk_afe_disable_adda_on(afe);
}

static void mtk_afe_set_adda_in_enable(struct mtk_afe *afe, bool enable)
{
	regmap_update_bits(afe->regmap, AFE_ADDA_UL_SRC_CON0, 0x1, enable);

	if (enable)
		mtk_afe_enable_adda_on(afe);
	else
		mtk_afe_disable_adda_on(afe);
}

static int mtk_afe_set_mrg(struct mtk_afe *afe, unsigned int rate)
{
	unsigned int val = 0;

	switch (rate) {
	case 8000:
		val |= 0 << 9;
		break;
	case 16000:
		val |= 1 << 9;
		break;
	default:
		return -EINVAL;
	}

	val |= AFE_DAIBT_CON0_USE_MRG_INPUT |
	       AFE_DAIBT_CON0_DATA_DRY;

	regmap_update_bits(afe->regmap, AFE_MRGIF_CON, 0xf00000, 9 << 20);
	regmap_update_bits(afe->regmap, AFE_DAIBT_CON0, 0x1208, val);

	return 0;
}

static void mtk_afe_enable_mrg(struct mtk_afe *afe)
{
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);
	afe->daibt_on_ref_cnt++;
	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	if (afe->daibt_on_ref_cnt != 1)
		return;

	regmap_update_bits(afe->regmap, AFE_MRGIF_CON, 1 << 16, 1 << 16);
	regmap_update_bits(afe->regmap, AFE_MRGIF_CON, 0x1, 0x1);

	udelay(100);

	regmap_update_bits(afe->regmap, AFE_DAIBT_CON0, 0x3, 0x3);
}

static void mtk_afe_disable_mrg(struct mtk_afe *afe)
{
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);
	afe->daibt_on_ref_cnt--;
	if (afe->daibt_on_ref_cnt < 0)
		afe->daibt_on_ref_cnt = 0;
	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	if (afe->daibt_on_ref_cnt != 0)
		return;

	regmap_update_bits(afe->regmap, AFE_DAIBT_CON0, 0x3, 0x0);

	udelay(100);

	regmap_update_bits(afe->regmap, AFE_MRGIF_CON, 1 << 16, 0x0);
	regmap_update_bits(afe->regmap, AFE_MRGIF_CON, 0x1, 0x0);
}

static int mtk_afe_set_pcm0(struct mtk_afe *afe, unsigned int rate)
{
	unsigned int val = 0;

	switch (rate) {
	case 8000:
		val |= 0 << 9;
		break;
	case 16000:
		val |= 1 << 9;
		break;
	default:
		return -EINVAL;
	}

	val |= AFE_DAIBT_CON0_DATA_DRY;

	regmap_update_bits(afe->regmap, AFE_DAIBT_CON0, 0x1208, val);

	return 0;
}

static void mtk_afe_enable_pcm0(struct mtk_afe *afe)
{
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);
	afe->daibt_on_ref_cnt++;
	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	if (afe->daibt_on_ref_cnt != 1)
		return;

	regmap_update_bits(afe->regmap, AFE_DAIBT_CON0, 0x3, 0x3);
}

static void mtk_afe_disable_pcm0(struct mtk_afe *afe)
{
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);
	afe->daibt_on_ref_cnt--;
	if (afe->daibt_on_ref_cnt < 0)
		afe->daibt_on_ref_cnt = 0;
	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	if (afe->daibt_on_ref_cnt != 0)
		return;

	regmap_update_bits(afe->regmap, AFE_DAIBT_CON0, 0x3, 0x0);
}

static int mtk_afe_enable_irq(struct mtk_afe *afe, struct mtk_afe_memif *memif)
{
	int irq_mode = memif->data->irq_mode;
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);

	afe->irq_mode_ref_cnt[irq_mode]++;
	if (afe->irq_mode_ref_cnt[irq_mode] > 1) {
		spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);
		return 0;
	}

	switch (irq_mode) {
	case MTK_AFE_IRQ_1:
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CON, 1 << 0, 1 << 0);
		break;
	case MTK_AFE_IRQ_2:
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CON, 1 << 1, 1 << 1);
		break;
	case MTK_AFE_IRQ_5:
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CON2, 1 << 3, 1 << 3);
		break;
	case MTK_AFE_IRQ_7:
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CON, 1 << 14, 1 << 14);
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	return 0;
}

static int mtk_afe_disable_irq(struct mtk_afe *afe, struct mtk_afe_memif *memif)
{
	int irq_mode = memif->data->irq_mode;
	unsigned long flags;

	spin_lock_irqsave(&afe->afe_ctrl_lock, flags);

	afe->irq_mode_ref_cnt[irq_mode]--;
	if (afe->irq_mode_ref_cnt[irq_mode] > 0) {
		spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);
		return 0;
	} else if (afe->irq_mode_ref_cnt[irq_mode] < 0) {
		afe->irq_mode_ref_cnt[irq_mode] = 0;
		spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);
		return 0;
	}

	switch (irq_mode) {
	case MTK_AFE_IRQ_1:
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CON, 1 << 0, 0 << 0);
		regmap_write(afe->regmap, AFE_IRQ_CLR, 1 << 0);
		break;
	case MTK_AFE_IRQ_2:
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CON, 1 << 1, 0 << 1);
		regmap_write(afe->regmap, AFE_IRQ_CLR, 1 << 1);
		break;
	case MTK_AFE_IRQ_5:
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CON2, 1 << 3, 0 << 3);
		regmap_write(afe->regmap, AFE_IRQ_CLR, 1 << 4);
		break;
	case MTK_AFE_IRQ_7:
		regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CON, 1 << 14, 0 << 14);
		regmap_write(afe->regmap, AFE_IRQ_CLR, 1 << 6);
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&afe->afe_ctrl_lock, flags);

	return 0;
}

static int mtk_afe_dais_enable_clks(struct mtk_afe *afe,
				    struct clk *m_ck, struct clk *b_ck)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	int ret;

	if (m_ck) {
		ret = clk_prepare_enable(m_ck);
		if (ret) {
			dev_err(afe->dev, "Failed to enable m_ck\n");
			return ret;
		}
	}

	if (b_ck) {
		ret = clk_prepare_enable(b_ck);
		if (ret) {
			dev_err(afe->dev, "Failed to enable b_ck\n");
			return ret;
		}
	}
#endif
	return 0;
}

static int mtk_afe_dais_set_clks(struct mtk_afe *afe,
				 struct clk *m_ck, unsigned int mck_rate,
				 struct clk *b_ck, unsigned int bck_rate)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	int ret;

	if (m_ck) {
		ret = clk_set_rate(m_ck, mck_rate);
		if (ret) {
			dev_err(afe->dev, "Failed to set m_ck rate\n");
			return ret;
		}
	}

	if (b_ck) {
		ret = clk_set_rate(b_ck, bck_rate);
		if (ret) {
			dev_err(afe->dev, "Failed to set b_ck rate\n");
			return ret;
		}
	}
#endif
	return 0;
}

static void mtk_afe_dais_disable_clks(struct mtk_afe *afe,
				      struct clk *m_ck, struct clk *b_ck)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	if (m_ck)
		clk_disable_unprepare(m_ck);
	if (b_ck)
		clk_disable_unprepare(b_ck);
#endif
}

static int mtk_afe_i2s_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(afe->dev, "%s '%s'\n",
		__func__, snd_pcm_stream_str(substream));

	mtk_afe_enable_main_clk(afe);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mtk_afe_dais_enable_clks(afe, afe->clocks[MTK_CLK_I2S1_M], NULL);
	else
		mtk_afe_dais_enable_clks(afe, afe->clocks[MTK_CLK_I2S2_M], NULL);

	return 0;
}

static void mtk_afe_i2s_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_be_dai_data *be = &afe->be_data[dai->id - MTK_AFE_BACKEND_BASE];
	const unsigned int rate = substream->runtime->rate;
	const unsigned int stream = substream->stream;

	dev_dbg(afe->dev, "%s '%s'\n",
		__func__, snd_pcm_stream_str(substream));

	if (be->prepared[stream]) {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			mtk_afe_set_i2s_out_enable(afe, false);
		else
			mtk_afe_set_i2s_in_enable(afe, false);

		if (rate % 8000)
			mtk_afe_disable_top_cg(afe, MTK_AFE_CG_22M);
		else
			mtk_afe_disable_top_cg(afe, MTK_AFE_CG_24M);

		be->prepared[stream] = false;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		mtk_afe_dais_disable_clks(afe, afe->clocks[MTK_CLK_I2S1_M], NULL);
	else
		mtk_afe_dais_disable_clks(afe, afe->clocks[MTK_CLK_I2S2_M], NULL);

	mtk_afe_disable_main_clk(afe);
}

static int mtk_afe_i2s_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params,
			  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int width_val = params_width(params) > 16 ?
		(AFE_CONN_24BIT_O03 | AFE_CONN_24BIT_O04) : 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		regmap_update_bits(afe->regmap, AFE_CONN_24BIT,
			   AFE_CONN_24BIT_O03 | AFE_CONN_24BIT_O04, width_val);

	return 0;
}

static int mtk_afe_i2s_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_be_dai_data *be = &afe->be_data[dai->id - MTK_AFE_BACKEND_BASE];
	const unsigned int rate = substream->runtime->rate;
	const int bit_width = snd_pcm_format_width(substream->runtime->format);
	const unsigned int stream = substream->stream;
	int ret;

	if (be->prepared[stream]) {
		dev_info(afe->dev, "%s '%s' prepared already\n",
			 __func__, snd_pcm_stream_str(substream));
		return 0;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = mtk_afe_set_i2s_out(afe, rate, bit_width);
	else
		ret = mtk_afe_set_i2s_in(afe, rate, bit_width);

	if (ret)
		return ret;

	/* TODO: add apll control */

	if (rate % 8000)
		mtk_afe_enable_top_cg(afe, MTK_AFE_CG_22M);
	else
		mtk_afe_enable_top_cg(afe, MTK_AFE_CG_24M);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mtk_afe_dais_set_clks(afe, afe->clocks[MTK_CLK_I2S1_M],
				rate * 256, NULL, 0);

		mtk_afe_set_i2s_out_enable(afe, true);
	} else {
		mtk_afe_dais_set_clks(afe, afe->clocks[MTK_CLK_I2S2_M],
				rate * 256, NULL, 0);

		mtk_afe_set_i2s_in_enable(afe, true);
	}

	be->prepared[stream] = true;

	return 0;
}

static int mtk_afe_2nd_i2s_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_dbg(afe->dev, "%s '%s'\n",
		__func__, snd_pcm_stream_str(substream));

	mtk_afe_enable_main_clk(afe);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mtk_afe_dais_enable_clks(afe, afe->clocks[MTK_CLK_I2S3_M], NULL);
	else
		mtk_afe_dais_enable_clks(afe, afe->clocks[MTK_CLK_I2S0_M], NULL);

	return 0;
}

static void mtk_afe_2nd_i2s_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_be_dai_data *be = &afe->be_data[dai->id - MTK_AFE_BACKEND_BASE];
	const unsigned int rate = substream->runtime->rate;
	const unsigned int stream = substream->stream;

	dev_dbg(afe->dev, "%s '%s'\n",
		__func__, snd_pcm_stream_str(substream));

	if (be->prepared[stream]) {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			mtk_afe_set_2nd_i2s_out_enable(afe, false);
		else
			mtk_afe_set_2nd_i2s_in_enable(afe, false);

		if (rate % 8000)
			mtk_afe_disable_top_cg(afe, MTK_AFE_CG_22M);
		else
			mtk_afe_disable_top_cg(afe, MTK_AFE_CG_24M);

		be->prepared[stream] = false;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		mtk_afe_dais_disable_clks(afe, afe->clocks[MTK_CLK_I2S3_M], NULL);
	else
		mtk_afe_dais_disable_clks(afe, afe->clocks[MTK_CLK_I2S0_M], NULL);

	mtk_afe_disable_main_clk(afe);
}

static int mtk_afe_2nd_i2s_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params,
			  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int width_val = params_width(params) > 16 ?
		(AFE_CONN_24BIT_O01 | AFE_CONN_24BIT_O02) : 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		regmap_update_bits(afe->regmap, AFE_CONN_24BIT,
			   AFE_CONN_24BIT_O01 | AFE_CONN_24BIT_O02, width_val);

	return 0;
}

static int mtk_afe_2nd_i2s_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_be_dai_data *be = &afe->be_data[dai->id - MTK_AFE_BACKEND_BASE];
	const unsigned int rate = substream->runtime->rate;
	const int bit_width = snd_pcm_format_width(substream->runtime->format);
	const unsigned int stream = substream->stream;
	int ret;

	if (be->prepared[stream]) {
		dev_info(afe->dev, "%s '%s' prepared already\n",
			 __func__, snd_pcm_stream_str(substream));
		return 0;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = mtk_afe_set_2nd_i2s_out(afe, rate, bit_width);
	else
		ret = mtk_afe_set_2nd_i2s_in(afe, rate, bit_width);

	if (ret)
		return ret;

	/* TODO: add apll control */

	if (rate % 8000)
		mtk_afe_enable_top_cg(afe, MTK_AFE_CG_22M);
	else
		mtk_afe_enable_top_cg(afe, MTK_AFE_CG_24M);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mtk_afe_dais_set_clks(afe, afe->clocks[MTK_CLK_I2S3_M],
				rate * 256, NULL, 0);

		mtk_afe_set_2nd_i2s_out_enable(afe, true);
	} else {
		mtk_afe_dais_set_clks(afe, afe->clocks[MTK_CLK_I2S0_M],
				rate * 256, NULL, 0);

		mtk_afe_set_2nd_i2s_in_enable(afe, true);
	}

	be->prepared[stream] = true;

	return 0;
}

static int mtk_afe_int_adda_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	mtk_afe_enable_main_clk(afe);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mtk_afe_enable_top_cg(afe, MTK_AFE_CG_DAC);
		mtk_afe_enable_top_cg(afe, MTK_AFE_CG_DAC_PREDIS);
	} else {
		mtk_afe_enable_top_cg(afe, MTK_AFE_CG_ADC);
	}

	return 0;
}

static void mtk_afe_int_adda_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_be_dai_data *be = &afe->be_data[dai->id - MTK_AFE_BACKEND_BASE];
	const unsigned int stream = substream->stream;

	dev_dbg(afe->dev, "%s '%s'\n", __func__,
		snd_pcm_stream_str(substream));

	if (be->prepared[stream]) {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			mtk_afe_set_adda_out_enable(afe, false);
			mtk_afe_set_i2s_out_enable(afe, false);
		} else {
			mtk_afe_set_adda_in_enable(afe, false);
		}

		be->prepared[stream] = false;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mtk_afe_disable_top_cg(afe, MTK_AFE_CG_DAC);
		mtk_afe_disable_top_cg(afe, MTK_AFE_CG_DAC_PREDIS);
	} else {
		mtk_afe_disable_top_cg(afe, MTK_AFE_CG_ADC);
	}

	mtk_afe_disable_main_clk(afe);
}

static int mtk_afe_int_adda_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params,
			  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int width_val = params_width(params) > 16 ?
		(AFE_CONN_24BIT_O03 | AFE_CONN_24BIT_O04) : 0;

	dev_dbg(afe->dev, "%s '%s'\n", __func__, snd_pcm_stream_str(substream));

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		regmap_update_bits(afe->regmap, AFE_CONN_24BIT,
			   AFE_CONN_24BIT_O03 | AFE_CONN_24BIT_O04, width_val);

	return 0;
}

static int mtk_afe_int_adda_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_be_dai_data *be = &afe->be_data[dai->id - MTK_AFE_BACKEND_BASE];
	const unsigned int rate = substream->runtime->rate;
	const unsigned int stream = substream->stream;
	const int bit_width = snd_pcm_format_width(substream->runtime->format);
	int ret;

	dev_dbg(afe->dev, "%s '%s' rate = %u\n", __func__,
		snd_pcm_stream_str(substream), rate);

	if (be->prepared[stream]) {
		dev_info(afe->dev, "%s '%s' prepared already\n",
			 __func__, snd_pcm_stream_str(substream));
		return 0;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = mtk_afe_set_adda_out(afe, rate);
		if (ret)
			return ret;

		ret = mtk_afe_set_i2s_out(afe, rate, bit_width);
		if (ret)
			return ret;

		mtk_afe_set_adda_out_enable(afe, true);
		mtk_afe_set_i2s_out_enable(afe, true);
	} else {
		ret = mtk_afe_set_adda_in(afe, rate);
		if (ret)
			return ret;

		mtk_afe_set_adda_in_enable(afe, true);
	}

	be->prepared[stream] = true;

	return 0;
}

static int mtk_afe_mrg_bt_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	mtk_afe_enable_main_clk(afe);

	return 0;
}

static void mtk_afe_mrg_bt_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	mtk_afe_disable_main_clk(afe);
}

static int mtk_afe_mrg_bt_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params,
			  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int ret;

	dev_dbg(afe->dev, "%s '%s' rate = %u\n", __func__,
		snd_pcm_stream_str(substream), params_rate(params));

	ret = mtk_afe_set_mrg(afe, params_rate(params));
	if (ret)
		return ret;

	return 0;
}

static int mtk_afe_mrg_bt_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_info(afe->dev, "%s %s '%s' cmd = %d\n", __func__,
		 dai->name, snd_pcm_stream_str(substream), cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		mtk_afe_enable_mrg(afe);
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mtk_afe_disable_mrg(afe);
		return 0;
	default:
		return -EINVAL;
	}
}

static int mtk_afe_pcm0_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	mtk_afe_enable_main_clk(afe);

	return 0;
}

static void mtk_afe_pcm0_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	mtk_afe_disable_main_clk(afe);
}

static int mtk_afe_pcm0_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params,
			  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	int ret;

	dev_dbg(afe->dev, "%s '%s' rate = %u\n", __func__,
		snd_pcm_stream_str(substream), params_rate(params));

	ret = mtk_afe_set_pcm0(afe, params_rate(params));
	if (ret)
		return ret;

	return 0;
}

static int mtk_afe_pcm0_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_info(afe->dev, "%s %s '%s' cmd = %d\n", __func__,
		 dai->name, snd_pcm_stream_str(substream), cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		mtk_afe_enable_pcm0(afe);
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mtk_afe_disable_pcm0(afe);
		return 0;
	default:
		return -EINVAL;
	}
}

static int mtk_afe_hdmi_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	mtk_afe_enable_main_clk(afe);

	/* TODO: change to I2S4_M */
	mtk_afe_dais_enable_clks(afe, afe->clocks[MTK_CLK_I2S3_M],
				 afe->clocks[MTK_CLK_I2S3_B]);
	return 0;
}

static void mtk_afe_hdmi_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	/* TODO: change to I2S4_M */
	mtk_afe_dais_disable_clks(afe, afe->clocks[MTK_CLK_I2S3_M],
				  afe->clocks[MTK_CLK_I2S3_B]);

	mtk_afe_disable_main_clk(afe);
}

static int mtk_afe_hdmi_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	unsigned int val;

	/* TODO: change to I2S4_M */
	mtk_afe_dais_set_clks(afe,
			      afe->clocks[MTK_CLK_I2S3_M], runtime->rate * 128,
			      afe->clocks[MTK_CLK_I2S3_B],
			      runtime->rate * runtime->channels * 32);

	val = AFE_TDM_CON1_BCK_INV |
	      AFE_TDM_CON1_1_BCK_DELAY |
	      AFE_TDM_CON1_MSB_ALIGNED | /* I2S mode */
	      AFE_TDM_CON1_WLEN_32BIT |
	      AFE_TDM_CON1_32_BCK_CYCLES |
	      AFE_TDM_CON1_LRCK_WIDTH(32);

	regmap_update_bits(afe->regmap, AFE_TDM_CON1, ~AFE_TDM_CON1_EN, val);

	/* set tdm2 config */
	switch (runtime->channels) {
	case 1:
	case 2:
		val = AFE_TDM_CH_START_O30_O31;
		val |= (AFE_TDM_CH_ZERO << 4);
		val |= (AFE_TDM_CH_ZERO << 8);
		val |= (AFE_TDM_CH_ZERO << 12);
		break;
	case 3:
	case 4:
		val = AFE_TDM_CH_START_O30_O31;
		val |= (AFE_TDM_CH_START_O32_O33 << 4);
		val |= (AFE_TDM_CH_ZERO << 8);
		val |= (AFE_TDM_CH_ZERO << 12);
		break;
	case 5:
	case 6:
		val = AFE_TDM_CH_START_O30_O31;
		val |= (AFE_TDM_CH_START_O32_O33 << 4);
		val |= (AFE_TDM_CH_START_O34_O35 << 8);
		val |= (AFE_TDM_CH_ZERO << 12);
		break;
	case 7:
	case 8:
		val = AFE_TDM_CH_START_O30_O31;
		val |= (AFE_TDM_CH_START_O32_O33 << 4);
		val |= (AFE_TDM_CH_START_O34_O35 << 8);
		val |= (AFE_TDM_CH_START_O36_O37 << 12);
		break;
	default:
		val = 0;
	}

	regmap_update_bits(afe->regmap, AFE_TDM_CON2, 0x0000ffff, val);

	regmap_update_bits(afe->regmap, AFE_HDMI_OUT_CON0,
			   0x000000f0, runtime->channels << 4);
	return 0;
}

static int mtk_afe_hdmi_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);

	dev_info(afe->dev, "%s cmd=%d %s\n", __func__, cmd, dai->name);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		regmap_update_bits(afe->regmap, AUDIO_TOP_CON0,
				   AUD_TCON0_PDN_HDMI | AUD_TCON0_PDN_SPDF, 0);

		/* TODO: align the connection logic with HDMI Tx */
		/* set connections:  O30~O37: L/R/LS/RS/C/LFE/CH7/CH8 */
		regmap_write(afe->regmap, AFE_HDMI_CONN0,
			     AFE_HDMI_CONN0_O30_I30 | AFE_HDMI_CONN0_O31_I31 |
			     AFE_HDMI_CONN0_O32_I34 | AFE_HDMI_CONN0_O33_I35 |
			     AFE_HDMI_CONN0_O34_I32 | AFE_HDMI_CONN0_O35_I33 |
			     AFE_HDMI_CONN0_O36_I36 | AFE_HDMI_CONN0_O37_I37);

		/* enable Out control */
		regmap_update_bits(afe->regmap, AFE_HDMI_OUT_CON0, 0x1, 0x1);

		/* enable tdm */
		regmap_update_bits(afe->regmap, AFE_TDM_CON1, 0x1, 0x1);

		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		/* disable tdm */
		regmap_update_bits(afe->regmap, AFE_TDM_CON1, 0x1, 0);

		/* disable Out control */
		regmap_update_bits(afe->regmap, AFE_HDMI_OUT_CON0, 0x1, 0);

		regmap_update_bits(afe->regmap, AUDIO_TOP_CON0,
				   AUD_TCON0_PDN_HDMI | AUD_TCON0_PDN_SPDF,
				   AUD_TCON0_PDN_HDMI | AUD_TCON0_PDN_SPDF);

		return 0;
	default:
		return -EINVAL;
	}
}

static int mtk_afe_dais_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mtk_afe_memif *memif = &afe->memif[rtd->cpu_dai->id];
	int ret;

	dev_dbg(afe->dev, "%s %s\n", __func__, memif->data->name);

	snd_soc_set_runtime_hwparams(substream, &mtk_afe_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_err(afe->dev, "snd_pcm_hw_constraint_integer failed\n");
		return ret;
	}

	memif->substream = substream;

	mtk_afe_enable_main_clk(afe);

	return 0;
}

static void mtk_afe_dais_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_memif *memif = &afe->memif[rtd->cpu_dai->id];

	dev_dbg(afe->dev, "%s %s\n", __func__, memif->data->name);

	if (memif->prepared) {
		mtk_afe_disable_afe_on(afe);
		memif->prepared = false;
	}

	memif->substream = NULL;

	mtk_afe_disable_main_clk(afe);
}

static int mtk_afe_dais_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_memif *memif = &afe->memif[rtd->cpu_dai->id];
	const struct mtk_afe_memif_data *data = memif->data;
	const size_t request_size = params_buffer_bytes(params);
	int ret;

	dev_dbg(afe->dev,
		"%s %s period = %u rate = %u channels = %u size = %lu\n",
		__func__, data->name, params_period_size(params),
		params_rate(params), params_channels(params), request_size);

	if (request_size > data->max_sram_size) {
		ret = snd_pcm_lib_malloc_pages(substream, request_size);
		if (ret < 0) {
			dev_err(afe->dev,
				"%s %s malloc pages %zu bytes failed %d\n",
				__func__, data->name, request_size, ret);
			return ret;
		}

		memif->use_sram = false;

		mtk_afe_emi_clk_on(afe);
	} else {
		struct snd_dma_buffer *dma_buf = &substream->dma_buffer;

		dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
		dma_buf->dev.dev = substream->pcm->card->dev;
		dma_buf->area = ((unsigned char *)afe->sram_address) +
				 data->sram_offset;
		dma_buf->addr = afe->sram_phy_address + data->sram_offset;
		dma_buf->bytes = request_size;
		snd_pcm_set_runtime_buffer(substream, dma_buf);

		memif->use_sram = true;
	}

	memif->phys_buf_addr = substream->runtime->dma_addr;
	memif->buffer_size = substream->runtime->dma_bytes;

	/* start */
	regmap_write(afe->regmap, data->reg_ofs_base,
		     memif->phys_buf_addr);

	/* end */
	regmap_write(afe->regmap, data->reg_ofs_end,
		     memif->phys_buf_addr + memif->buffer_size - 1);

	/* set channel */
	if (data->mono_shift >= 0) {
		unsigned int mono = (params_channels(params) == 1) ? 1 : 0;

		regmap_update_bits(afe->regmap, AFE_DAC_CON1,
				   1 << data->mono_shift,
				   mono << data->mono_shift);
	}

	/* set format */
	if (data->format_shift >= 0) {
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S16_LE:
			regmap_update_bits(afe->regmap, data->format_reg,
					   3 << data->format_shift,
					   0 << data->format_shift);
			break;
		case SNDRV_PCM_FORMAT_S32_LE:
			regmap_update_bits(afe->regmap, data->format_reg,
					   3 << data->format_shift,
					   3 << data->format_shift);
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			regmap_update_bits(afe->regmap, data->format_reg,
					   3 << data->format_shift,
					   1 << data->format_shift);
			break;
		default:
			return -EINVAL;
		}
	}

	if (data->conn_format_mask > 0) {
		if (params_width(params) > 16)
			regmap_update_bits(afe->regmap, AFE_CONN_24BIT,
					   data->conn_format_mask,
					   data->conn_format_mask);
		else
			regmap_update_bits(afe->regmap, AFE_CONN_24BIT,
					   data->conn_format_mask,
					   0);
	}

	/* set rate */
	if (data->fs_shift < 0)
		return 0;

	if (data->id == MTK_AFE_MEMIF_DAI ||
	    data->id == MTK_AFE_MEMIF_MOD_DAI) {
		unsigned int val;

		switch (params_rate(params)) {
		case 8000:
			val = 0;
			break;
		case 16000:
			val = 1;
			break;
		case 32000:
			val = 2;
			break;
		default:
			dev_err(afe->dev, "%s %s rate %u not supported\n",
				__func__, data->name, params_rate(params));
			return -EINVAL;
		}

		if (data->id == MTK_AFE_MEMIF_DAI)
			regmap_update_bits(afe->regmap, AFE_DAC_CON0,
					   0x3 << data->fs_shift,
					   val << data->fs_shift);
		else
			regmap_update_bits(afe->regmap, AFE_DAC_CON1,
					   0x3 << data->fs_shift,
					   val << data->fs_shift);
	} else {
		int fs = mtk_afe_i2s_fs(params_rate(params));

		if (fs < 0)
			return -EINVAL;

		regmap_update_bits(afe->regmap, AFE_DAC_CON1,
				   0xf << data->fs_shift,
				   fs << data->fs_shift);
	}

	return 0;
}

static int mtk_afe_dais_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_memif *memif = &afe->memif[rtd->cpu_dai->id];
	int ret = 0;

	dev_dbg(afe->dev, "%s %s\n", __func__, memif->data->name);

	if (memif->use_sram) {
		snd_pcm_set_runtime_buffer(substream, NULL);
	} else {
		ret = snd_pcm_lib_free_pages(substream);

		mtk_afe_emi_clk_off(afe);
	}

	return ret;
}

static int mtk_afe_dais_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_memif *memif = &afe->memif[rtd->cpu_dai->id];

	if (!memif->prepared) {
		mtk_afe_enable_afe_on(afe);
		memif->prepared = true;
	}

	return 0;
}

static int mtk_afe_dais_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_afe_memif *memif = &afe->memif[rtd->cpu_dai->id];
	unsigned int counter = runtime->period_size;

	dev_info(afe->dev, "%s %s cmd = %d\n", __func__,
		 memif->data->name, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (memif->data->enable_shift >= 0)
			regmap_update_bits(afe->regmap, AFE_DAC_CON0,
					   1 << memif->data->enable_shift,
					   1 << memif->data->enable_shift);

		/* set irq counter */
		regmap_update_bits(afe->regmap,
				   memif->data->irq_reg_cnt,
				   0x3ffff << memif->data->irq_cnt_shift,
				   counter << memif->data->irq_cnt_shift);

		/* set irq fs */
		if (memif->data->irq_fs_shift >= 0) {
			int fs = mtk_afe_i2s_fs(runtime->rate);

			if (fs < 0)
				return -EINVAL;

			regmap_update_bits(afe->regmap,
					   memif->data->irq_fs_reg,
					   0xf << memif->data->irq_fs_shift,
					   fs << memif->data->irq_fs_shift);
		}

		mtk_afe_enable_irq(afe, memif);
		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (memif->data->enable_shift >= 0)
			regmap_update_bits(afe->regmap, AFE_DAC_CON0,
					   1 << memif->data->enable_shift, 0);

		mtk_afe_disable_irq(afe, memif);
		return 0;
	default:
		return -EINVAL;
	}
}

/* FE DAIs */
static const struct snd_soc_dai_ops mtk_afe_dai_ops = {
	.startup	= mtk_afe_dais_startup,
	.shutdown	= mtk_afe_dais_shutdown,
	.hw_params	= mtk_afe_dais_hw_params,
	.hw_free	= mtk_afe_dais_hw_free,
	.prepare	= mtk_afe_dais_prepare,
	.trigger	= mtk_afe_dais_trigger,
};

/* BE DAIs */
static const struct snd_soc_dai_ops mtk_afe_i2s_ops = {
	.startup	= mtk_afe_i2s_startup,
	.shutdown	= mtk_afe_i2s_shutdown,
	.hw_params	= mtk_afe_i2s_hw_params,
	.prepare	= mtk_afe_i2s_prepare,
};

static const struct snd_soc_dai_ops mtk_afe_2nd_i2s_ops = {
	.startup	= mtk_afe_2nd_i2s_startup,
	.shutdown	= mtk_afe_2nd_i2s_shutdown,
	.hw_params	= mtk_afe_2nd_i2s_hw_params,
	.prepare	= mtk_afe_2nd_i2s_prepare,
};

static const struct snd_soc_dai_ops mtk_afe_int_adda_ops = {
	.startup	= mtk_afe_int_adda_startup,
	.shutdown	= mtk_afe_int_adda_shutdown,
	.hw_params	= mtk_afe_int_adda_hw_params,
	.prepare	= mtk_afe_int_adda_prepare,
};

static const struct snd_soc_dai_ops mtk_afe_mrg_bt_ops = {
	.startup	= mtk_afe_mrg_bt_startup,
	.shutdown	= mtk_afe_mrg_bt_shutdown,
	.hw_params	= mtk_afe_mrg_bt_hw_params,
	.trigger	= mtk_afe_mrg_bt_trigger,
};

static const struct snd_soc_dai_ops mtk_afe_pcm0_ops = {
	.startup	= mtk_afe_pcm0_startup,
	.shutdown	= mtk_afe_pcm0_shutdown,
	.hw_params	= mtk_afe_pcm0_hw_params,
	.trigger	= mtk_afe_pcm0_trigger,
};

static const struct snd_soc_dai_ops mtk_afe_hdmi_ops = {
	.startup	= mtk_afe_hdmi_startup,
	.shutdown	= mtk_afe_hdmi_shutdown,
	.prepare	= mtk_afe_hdmi_prepare,
	.trigger	= mtk_afe_hdmi_trigger,
};

static int mtk_afe_runtime_suspend(struct device *dev);
static int mtk_afe_runtime_resume(struct device *dev);

static int mtk_afe_dai_suspend(struct snd_soc_dai *dai)
{
	struct mtk_afe *afe = snd_soc_dai_get_drvdata(dai);
	int i;

	dev_dbg(afe->dev, "%s id %d suspended %d runtime suspended %d\n",
		__func__, dai->id, afe->suspended,
		pm_runtime_status_suspended(afe->dev));

	if (pm_runtime_status_suspended(afe->dev) || afe->suspended)
		return 0;

	mtk_afe_enable_main_clk(afe);

	for (i = 0; i < ARRAY_SIZE(mtk_afe_backup_list); i++)
		regmap_read(afe->regmap, mtk_afe_backup_list[i],
			    &afe->backup_regs[i]);

	mtk_afe_disable_main_clk(afe);

	afe->suspended = true;
	mtk_afe_runtime_suspend(afe->dev);
	return 0;
}

static int mtk_afe_dai_resume(struct snd_soc_dai *dai)
{
	struct mtk_afe *afe = snd_soc_dai_get_drvdata(dai);
	int i = 0;

	dev_dbg(afe->dev, "%s id %d suspended %d runtime suspended %d\n",
		__func__, dai->id, afe->suspended,
		pm_runtime_status_suspended(afe->dev));

	if (pm_runtime_status_suspended(afe->dev) || !afe->suspended)
		return 0;

	mtk_afe_runtime_resume(afe->dev);

	mtk_afe_enable_main_clk(afe);

	for (i = 0; i < ARRAY_SIZE(mtk_afe_backup_list); i++)
		regmap_write(afe->regmap, mtk_afe_backup_list[i],
			     afe->backup_regs[i]);

	mtk_afe_disable_main_clk(afe);

	afe->suspended = false;
	return 0;
}

static struct snd_soc_dai_driver mtk_afe_pcm_dais[] = {
	/* FE DAIs: memory intefaces to CPU */
	{
		.name = "DL1", /* downlink 1 */
		.id = MTK_AFE_MEMIF_DL1,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "DL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &mtk_afe_dai_ops,
	}, {
		.name = "VUL", /* voice uplink */
		.id = MTK_AFE_MEMIF_VUL,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.capture = {
			.stream_name = "VUL",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mtk_afe_dai_ops,
	}, {
		.name = "DL2", /* downlink 2 */
		.id = MTK_AFE_MEMIF_DL2,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &mtk_afe_dai_ops,
	}, {
		.name = "AWB",
		.id = MTK_AFE_MEMIF_AWB,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.capture = {
			.stream_name = "AWB",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mtk_afe_dai_ops,
	}, {
		.name = "DAI",
		.id = MTK_AFE_MEMIF_DAI,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.capture = {
			.stream_name = "DAI",
			.channels_min = 1,
			.channels_max = 1,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mtk_afe_dai_ops,
	}, {
		.name = "HDMI",
		.id = MTK_AFE_MEMIF_HDMI,
		.suspend = mtk_afe_dai_suspend,
		.resume = mtk_afe_dai_resume,
		.playback = {
			.stream_name = "HDMI",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000 |
				 SNDRV_PCM_RATE_88200 |
				 SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_176400 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &mtk_afe_dai_ops,
	}, {
	/* BE DAIs */
		.name = "I2S",
		.id = MTK_AFE_IO_I2S,
		.playback = {
			.stream_name = "I2S Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
		},
		.capture = {
			.stream_name = "I2S Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &mtk_afe_i2s_ops,
	}, {
		.name = "2ND I2S",
		.id = MTK_AFE_IO_2ND_I2S,
		.playback = {
			.stream_name = "2ND I2S Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
		},
		.capture = {
			.stream_name = "2ND I2S Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &mtk_afe_2nd_i2s_ops,
	}, {
	/* BE DAIs */
		.name = "INT ADDA",
		.id = MTK_AFE_IO_INT_ADDA,
		.playback = {
			.stream_name = "INT ADDA Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
		},
		.capture = {
			.stream_name = "INT ADDA Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000 |
				 SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mtk_afe_int_adda_ops,
	}, {
	/* BE DAIs */
		.name = "MRG BT",
		.id = MTK_AFE_IO_MRG_BT,
		.playback = {
			.stream_name = "MRG BT Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "MRG BT Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mtk_afe_mrg_bt_ops,
		.symmetric_rates = 1,
	}, {
	/* BE DAIs */
		.name = "PCM0",
		.id = MTK_AFE_IO_PCM_BT,
		.playback = {
			.stream_name = "PCM0 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.capture = {
			.stream_name = "PCM0 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &mtk_afe_pcm0_ops,
		.symmetric_rates = 1,
	}, {
	/* BE DAIs */
		.name = "DL Input",
		.id = MTK_AFE_IO_DL_BE,
		.capture = {
			.stream_name = "DL Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	}, {
	/* BE DAIs */
		.name = "HDMIO",
		.id = MTK_AFE_IO_HDMI,
		.playback = {
			.stream_name = "HDMIO Playback",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_32000 |
				 SNDRV_PCM_RATE_44100 |
				 SNDRV_PCM_RATE_48000 |
				 SNDRV_PCM_RATE_88200 |
				 SNDRV_PCM_RATE_96000 |
				 SNDRV_PCM_RATE_176400 |
				 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				   SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &mtk_afe_hdmi_ops,
	},
};

static const struct snd_kcontrol_new mtk_afe_o00_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN0, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN0, 7, 1, 0),
};

static const struct snd_kcontrol_new mtk_afe_o01_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN0, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN0, 24, 1, 0),
};

static const struct snd_kcontrol_new mtk_afe_o02_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN1, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN1, 6, 1, 0),
};

static const struct snd_kcontrol_new mtk_afe_o03_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN1, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN1, 23, 1, 0),
};

static const struct snd_kcontrol_new mtk_afe_o04_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN2, 8, 1, 0),
};

static const struct snd_kcontrol_new mtk_afe_o05_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I00 Switch", AFE_CONN2, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I05 Switch", AFE_CONN2, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I07 Switch", AFE_CONN2, 20, 1, 0),
};

static const struct snd_kcontrol_new mtk_afe_o06_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I01 Switch", AFE_CONN2, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I06 Switch", AFE_CONN2, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I08 Switch", AFE_CONN2, 25, 1, 0),
};

static const struct snd_kcontrol_new mtk_afe_o09_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I00 Switch", AFE_CONN5, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I03 Switch", AFE_CONN3, 0, 1, 0),
};

static const struct snd_kcontrol_new mtk_afe_o10_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I01 Switch", AFE_CONN5, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I04 Switch", AFE_CONN3, 3, 1, 0),
};

static const struct snd_kcontrol_new mtk_afe_o11_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I02 Switch", AFE_CONN3, 6, 1, 0),
};

static const char * const ain_text[] = {
	"INT ADC", "EXT ADC"
};

static SOC_ENUM_SINGLE_DECL(ain_enum, AFE_ADDA_TOP_CON0, 0, ain_text);

static const struct snd_kcontrol_new ain_mux = SOC_DAPM_ENUM("AIN Source", ain_enum);

static const char * const daibt_mux_text[] = {
	"PCM", "MRG"
};

static SOC_ENUM_SINGLE_DECL(daibt_mux_enum, AFE_DAIBT_CON0, 12, daibt_mux_text);

static const struct snd_kcontrol_new daibt_mux = SOC_DAPM_ENUM("DAIBT Source", daibt_mux_enum);

static const struct snd_kcontrol_new i2s_o03_o04_enable_ctl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new int_adda_o03_o04_enable_ctl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new mrg_bt_o02_enable_ctl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_kcontrol_new pcm0_o02_enable_ctl =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

static const struct snd_soc_dapm_widget mtk_afe_pcm_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("I00", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I01", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I02", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I03", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I04", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I05", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I06", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I07", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I08", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I05L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I06L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I07L", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I08L", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("O00", SND_SOC_NOPM, 0, 0,
			   mtk_afe_o00_mix, ARRAY_SIZE(mtk_afe_o00_mix)),
	SND_SOC_DAPM_MIXER("O01", SND_SOC_NOPM, 0, 0,
			   mtk_afe_o01_mix, ARRAY_SIZE(mtk_afe_o01_mix)),
	SND_SOC_DAPM_MIXER("O02", SND_SOC_NOPM, 0, 0,
			   mtk_afe_o02_mix, ARRAY_SIZE(mtk_afe_o02_mix)),
	SND_SOC_DAPM_MIXER("O03", SND_SOC_NOPM, 0, 0,
			   mtk_afe_o03_mix, ARRAY_SIZE(mtk_afe_o03_mix)),
	SND_SOC_DAPM_MIXER("O04", SND_SOC_NOPM, 0, 0,
			   mtk_afe_o04_mix, ARRAY_SIZE(mtk_afe_o04_mix)),
	SND_SOC_DAPM_MIXER("O05", SND_SOC_NOPM, 0, 0,
			   mtk_afe_o05_mix, ARRAY_SIZE(mtk_afe_o05_mix)),
	SND_SOC_DAPM_MIXER("O06", SND_SOC_NOPM, 0, 0,
			   mtk_afe_o06_mix, ARRAY_SIZE(mtk_afe_o06_mix)),
	SND_SOC_DAPM_MIXER("O09", SND_SOC_NOPM, 0, 0,
			   mtk_afe_o09_mix, ARRAY_SIZE(mtk_afe_o09_mix)),
	SND_SOC_DAPM_MIXER("O10", SND_SOC_NOPM, 0, 0,
			   mtk_afe_o10_mix, ARRAY_SIZE(mtk_afe_o10_mix)),
	SND_SOC_DAPM_MIXER("O11", SND_SOC_NOPM, 0, 0,
			   mtk_afe_o11_mix, ARRAY_SIZE(mtk_afe_o11_mix)),

	SND_SOC_DAPM_MUX("AIN Mux", SND_SOC_NOPM, 0, 0, &ain_mux),
	SND_SOC_DAPM_MUX("DAIBT Mux", SND_SOC_NOPM, 0, 0, &daibt_mux),

	SND_SOC_DAPM_SWITCH("I2S O03_O04", SND_SOC_NOPM, 0, 0,
			    &i2s_o03_o04_enable_ctl),
	SND_SOC_DAPM_SWITCH("INT ADDA O03_O04", SND_SOC_NOPM, 0, 0,
			    &int_adda_o03_o04_enable_ctl),
	SND_SOC_DAPM_SWITCH("MRG BT O02", SND_SOC_NOPM, 0, 0,
			    &mrg_bt_o02_enable_ctl),
	SND_SOC_DAPM_SWITCH("PCM0 O02", SND_SOC_NOPM, 0, 0,
			    &pcm0_o02_enable_ctl),

	SND_SOC_DAPM_OUTPUT("MRG Out"),
	SND_SOC_DAPM_OUTPUT("PCM0 Out"),

	SND_SOC_DAPM_INPUT("DL Source"),
	SND_SOC_DAPM_INPUT("MRG In"),
	SND_SOC_DAPM_INPUT("PCM0 In"),
};

static const struct snd_soc_dapm_route mtk_afe_pcm_routes[] = {
	/* downlink */
	{"I05", NULL, "DL1"},
	{"I06", NULL, "DL1"},
	{"I07", NULL, "DL2"},
	{"I08", NULL, "DL2"},
	{"O03", "I05 Switch", "I05"},
	{"O04", "I06 Switch", "I06"},
	{"O02", "I05 Switch", "I05"},
	{"O02", "I06 Switch", "I06"},
	{"O00", "I05 Switch", "I05"},
	{"O01", "I06 Switch", "I06"},
	{"O03", "I07 Switch", "I07"},
	{"O04", "I08 Switch", "I08"},
	{"O00", "I07 Switch", "I07"},
	{"O01", "I08 Switch", "I08"},
	{"I2S O03_O04", "Switch", "O03"},
	{"I2S O03_O04", "Switch", "O04"},
	{"I2S Playback", NULL, "I2S O03_O04"},
	{"INT ADDA O03_O04", "Switch", "O03"},
	{"INT ADDA O03_O04", "Switch", "O04"},
	{"INT ADDA Playback", NULL, "INT ADDA O03_O04"},
	{"2ND I2S Playback", NULL, "O00"},
	{"2ND I2S Playback", NULL, "O01"},

	{"MRG BT O02", "Switch", "O02"},
	{"PCM0 O02", "Switch", "O02"},
	{"MRG BT Playback", NULL, "MRG BT O02"},
	{"PCM0 Playback", NULL, "PCM0 O02"},
	{"MRG Out", NULL, "MRG BT Playback"},
	{"PCM0 Out", NULL, "PCM0 Playback"},

	{"HDMIO Playback", NULL, "HDMI"},

	/* uplink */
	{"AIN Mux", "EXT ADC", "I2S Capture"},
	{"AIN Mux", "INT ADC", "INT ADDA Capture"},
	{"I03", NULL, "AIN Mux"},
	{"I04", NULL, "AIN Mux"},
	{"I00", NULL, "2ND I2S Capture"},
	{"I01", NULL, "2ND I2S Capture"},
	{"O09", "I03 Switch", "I03"},
	{"O10", "I04 Switch", "I04"},
	{"O09", "I00 Switch", "I00"},
	{"O10", "I01 Switch", "I01"},
	{"VUL", NULL, "O09"},
	{"VUL", NULL, "O10"},

	{"DL Capture", NULL, "DL Source"},
	{"I05L", NULL, "DL Capture"},
	{"I06L", NULL, "DL Capture"},
	{"I07L", NULL, "DL Capture"},
	{"I08L", NULL, "DL Capture"},
	{"O05", "I05 Switch", "I05L"},
	{"O06", "I06 Switch", "I06L"},
	{"O05", "I07 Switch", "I07L"},
	{"O06", "I08 Switch", "I08L"},
	{"O05", "I00 Switch", "I00"},
	{"O06", "I01 Switch", "I01"},
	{"AWB", NULL, "O05"},
	{"AWB", NULL, "O06"},

	{"PCM0 Capture", NULL, "PCM0 In"},
	{"MRG BT Capture", NULL, "MRG In"},
	{"DAIBT Mux", "PCM", "PCM0 Capture"},
	{"DAIBT Mux", "MRG", "MRG BT Capture"},
	{"I02", NULL, "DAIBT Mux"},
	{"O11", "I02 Switch", "I02"},
	{"DAI", NULL, "O11"},
};

static const struct snd_soc_component_driver mtk_afe_pcm_dai_component = {
	.name = "mtk-afe-pcm-dai",
	.dapm_widgets = mtk_afe_pcm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mtk_afe_pcm_widgets),
	.dapm_routes = mtk_afe_pcm_routes,
	.num_dapm_routes = ARRAY_SIZE(mtk_afe_pcm_routes),
};


#ifdef COMMON_CLOCK_FRAMEWORK_API
static const char *aud_clks[MTK_CLK_NUM] = {
	[MTK_CLK_INFRASYS_AUD] = "infra_sys_audio_clk",
	[MTK_CLK_TOP_PDN_AUD] = "top_pdn_audio",
	[MTK_CLK_TOP_PDN_AUD_BUS] = "top_pdn_aud_intbus",
	[MTK_CLK_I2S0_M] =  "i2s0_m",
	[MTK_CLK_I2S1_M] =  "i2s1_m",
	[MTK_CLK_I2S2_M] =  "i2s2_m",
	[MTK_CLK_I2S3_M] =  "i2s3_m",
	[MTK_CLK_I2S3_B] =  "i2s3_b",
	[MTK_CLK_BCK0] =  "bck0",
	[MTK_CLK_BCK1] =  "bck1",
};
#endif

static const struct mtk_afe_memif_data memif_data[MTK_AFE_MEMIF_NUM] = {
	{
		.name = "DL1",
		.id = MTK_AFE_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_end = AFE_DL1_END,
		.reg_ofs_cur = AFE_DL1_CUR,
		.fs_shift = 0,
		.mono_shift = 21,
		.enable_shift = 1,
		.irq_reg_cnt = AFE_IRQ_CNT1,
		.irq_cnt_shift = 0,
		.irq_mode = MTK_AFE_IRQ_1,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 4,
		.irq_clr_shift = 0,
		.max_sram_size = 36 * 1024,
		.sram_offset = 0,
		.format_reg = AFE_MEMIF_PBUF_SIZE,
		.format_shift = 16,
		.conn_format_mask = -1,
		.prealloc_size = 128 * 1024,
	}, {
		.name = "DL2",
		.id = MTK_AFE_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_end = AFE_DL2_END,
		.reg_ofs_cur = AFE_DL2_CUR,
		.fs_shift = 4,
		.mono_shift = 22,
		.enable_shift = 2,
		.irq_reg_cnt = AFE_IRQ_CNT7,
		.irq_cnt_shift = 0,
		.irq_mode = MTK_AFE_IRQ_7,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 24,
		.irq_clr_shift = 6,
		.max_sram_size = 0,
		.sram_offset = 0,
		.format_reg = AFE_MEMIF_PBUF_SIZE,
		.format_shift = 18,
		.conn_format_mask = -1,
		.prealloc_size = 128 * 1024,
	}, {
		.name = "VUL",
		.id = MTK_AFE_MEMIF_VUL,
		.reg_ofs_base = AFE_VUL_BASE,
		.reg_ofs_end = AFE_VUL_END,
		.reg_ofs_cur = AFE_VUL_CUR,
		.fs_shift = 16,
		.mono_shift = 27,
		.enable_shift = 3,
		.irq_reg_cnt = AFE_IRQ_CNT2,
		.irq_cnt_shift = 0,
		.irq_mode = MTK_AFE_IRQ_2,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 8,
		.irq_clr_shift = 1,
		.max_sram_size = 0,
		.sram_offset = 0,
		.format_reg = AFE_MEMIF_PBUF_SIZE,
		.format_shift = 22,
		.conn_format_mask = AFE_CONN_24BIT_O09 | AFE_CONN_24BIT_O10,
		.prealloc_size = 32 * 1024,
	}, {
		.name = "DAI",
		.id = MTK_AFE_MEMIF_DAI,
		.reg_ofs_base = AFE_DAI_BASE,
		.reg_ofs_end = AFE_DAI_END,
		.reg_ofs_cur = AFE_DAI_CUR,
		.fs_shift = 24,
		.mono_shift = -1,
		.enable_shift = 4,
		.irq_reg_cnt = AFE_IRQ_CNT2,
		.irq_cnt_shift = 0,
		.irq_mode = MTK_AFE_IRQ_2,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 8,
		.irq_clr_shift = 1,
		.max_sram_size = 0,
		.sram_offset = 0,
		.format_reg = AFE_MEMIF_PBUF_SIZE,
		.format_shift = 24,
		.conn_format_mask = -1,
		.prealloc_size = 16 * 1024,
	}, {
		.name = "AWB",
		.id = MTK_AFE_MEMIF_AWB,
		.reg_ofs_base = AFE_AWB_BASE,
		.reg_ofs_end = AFE_AWB_END,
		.reg_ofs_cur = AFE_AWB_CUR,
		.fs_shift = 12,
		.mono_shift = 24,
		.enable_shift = 6,
		.irq_reg_cnt = AFE_IRQ_CNT2,
		.irq_cnt_shift = 0,
		.irq_mode = MTK_AFE_IRQ_2,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 8,
		.irq_clr_shift = 1,
		.max_sram_size = 0,
		.sram_offset = 0,
		.format_reg = AFE_MEMIF_PBUF_SIZE,
		.format_shift = 20,
		.conn_format_mask = AFE_CONN_24BIT_O05 | AFE_CONN_24BIT_O06,
		.prealloc_size = 0,
	}, {
		.name = "MOD_DAI",
		.id = MTK_AFE_MEMIF_MOD_DAI,
		.reg_ofs_base = AFE_MOD_PCM_BASE,
		.reg_ofs_end = AFE_MOD_PCM_END,
		.reg_ofs_cur = AFE_MOD_PCM_CUR,
		.fs_shift = 30,
		.mono_shift = -1,
		.enable_shift = 7,
		.irq_reg_cnt = AFE_IRQ_CNT2,
		.irq_cnt_shift = 0,
		.irq_mode = MTK_AFE_IRQ_2,
		.irq_fs_reg = AFE_IRQ_MCU_CON,
		.irq_fs_shift = 8,
		.irq_clr_shift = 1,
		.max_sram_size = 0,
		.sram_offset = 0,
		.format_reg = AFE_MEMIF_PBUF_SIZE,
		.format_shift = 26,
		.conn_format_mask = -1,
		.prealloc_size = 0,
	}, {
		.name = "HDMI",
		.id = MTK_AFE_MEMIF_HDMI,
		.reg_ofs_base = AFE_HDMI_OUT_BASE,
		.reg_ofs_end = AFE_HDMI_OUT_END,
		.reg_ofs_cur = AFE_HDMI_OUT_CUR,
		.fs_shift = -1,
		.mono_shift = -1,
		.enable_shift = -1,
		.irq_reg_cnt = AFE_IRQ_CNT5,
		.irq_cnt_shift = 0,
		.irq_mode = MTK_AFE_IRQ_5,
		.irq_fs_reg = -1,
		.irq_fs_shift = -1,
		.irq_clr_shift = 4,
		.max_sram_size = 0,
		.sram_offset = 0,
		.format_reg = AFE_MEMIF_PBUF_SIZE,
		.format_shift = 28,
		.conn_format_mask = -1,
		.prealloc_size = 0,
	},
};

static const struct regmap_config mtk_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = ABB_AFE_SDM_TEST,
	.cache_type = REGCACHE_NONE,
};

static irqreturn_t mtk_afe_irq_handler(int irq, void *dev_id)
{
	struct mtk_afe *afe = dev_id;
	unsigned int reg_value;
	unsigned int memif_status;
	int i, ret;

	ret = regmap_read(afe->regmap, AFE_IRQ_STATUS, &reg_value);
	if (ret) {
		dev_err(afe->dev, "%s irq status err\n", __func__);
		reg_value = AFE_IRQ_STATUS_BITS;
		goto err_irq;
	}

	ret = regmap_read(afe->regmap, AFE_DAC_CON0, &memif_status);
	if (ret) {
		dev_err(afe->dev, "%s memif status err\n", __func__);
		reg_value = AFE_IRQ_STATUS_BITS;
		goto err_irq;
	}

	for (i = 0; i < MTK_AFE_MEMIF_NUM; i++) {
		struct mtk_afe_memif *memif = &afe->memif[i];
		struct snd_pcm_substream *substream = memif->substream;

		if (!substream || !(reg_value & (1 << memif->data->irq_clr_shift)))
			continue;

		if (memif->data->enable_shift >= 0 &&
		    !((1 << memif->data->enable_shift) & memif_status))
			continue;

		snd_pcm_period_elapsed(substream);
	}

err_irq:
	/* clear irq */
	regmap_write(afe->regmap, AFE_IRQ_CLR, reg_value & AFE_IRQ_STATUS_BITS);

	return IRQ_HANDLED;
}

static int mtk_afe_runtime_suspend(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);

	return 0;
}

static int mtk_afe_runtime_resume(struct device *dev)
{
	struct mtk_afe *afe = dev_get_drvdata(dev);

	dev_info(dev, "%s >>\n", __func__);

	mtk_afe_enable_main_clk(afe);

	/* unmask all IRQs */
	regmap_update_bits(afe->regmap, AFE_IRQ_MCU_EN, 0xff, 0xff);

	mtk_afe_disable_main_clk(afe);

	dev_info(dev, "%s <<\n", __func__);

	return 0;
}

static int mtk_afe_init_audio_clk(struct mtk_afe *afe)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	size_t i;

	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		afe->clocks[i] = devm_clk_get(afe->dev, aud_clks[i]);
		if (IS_ERR(afe->clocks[i])) {
			dev_err(afe->dev, "%s devm_clk_get %s fail\n",
				__func__, aud_clks[i]);
			return PTR_ERR(afe->clocks[i]);
		}
	}
	clk_set_rate(afe->clocks[MTK_CLK_BCK0], 22579200); /* 22M */
	clk_set_rate(afe->clocks[MTK_CLK_BCK1], 24576000); /* 24M */
#endif
	return 0;
}

static int mtk_afe_pcm_dev_probe(struct platform_device *pdev)
{
	int ret, i;
	unsigned int irq_id;
	struct mtk_afe *afe;
	struct resource *res;

	afe = devm_kzalloc(&pdev->dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;

	afe->backup_regs = kzalloc(ARRAY_SIZE(mtk_afe_backup_list) *
				   sizeof(unsigned int), GFP_KERNEL);
	if (!afe->backup_regs)
		return -ENOMEM;

	spin_lock_init(&afe->afe_ctrl_lock);
#ifdef IDLE_TASK_DRIVER_API
	mutex_init(&afe->emi_clk_mutex);
#endif

	afe->dev = &pdev->dev;

	irq_id = platform_get_irq(pdev, 0);
	if (!irq_id) {
		dev_err(afe->dev, "np %s no irq\n", afe->dev->of_node->name);
		return -ENXIO;
	}

	ret = devm_request_irq(afe->dev, irq_id, mtk_afe_irq_handler,
			       0, "Afe_ISR_Handle", (void *)afe);
	if (ret) {
		dev_err(afe->dev, "could not request_irq\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	afe->base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe->base_addr))
		return PTR_ERR(afe->base_addr);

	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->base_addr,
		&mtk_afe_regmap_config);
	if (IS_ERR(afe->regmap))
		return PTR_ERR(afe->regmap);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	afe->sram_address = devm_ioremap_resource(&pdev->dev, res);
	if (!IS_ERR(afe->sram_address)) {
		afe->sram_phy_address = res->start;
		afe->sram_size = resource_size(res);
	}

	/* initial audio related clock */
	ret = mtk_afe_init_audio_clk(afe);
	if (ret) {
		dev_err(afe->dev, "%s mtk_afe_init_audio_clk fail\n", __func__);
		return ret;
	}

	for (i = 0; i < MTK_AFE_MEMIF_NUM; i++)
		afe->memif[i].data = &memif_data[i];

	platform_set_drvdata(pdev, afe);

	/* TODO: check if pm_runtime_* operation is necessary */
	/* since 8167 has no individual power domain */
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		dev_warn(afe->dev, "%s power not enabled\n", __func__);
		ret = mtk_afe_runtime_resume(&pdev->dev);
		if (ret)
			goto err_pm_disable;
	}

	ret = snd_soc_register_platform(&pdev->dev, &mtk_afe_pcm_platform);
	if (ret)
		goto err_pm_disable;

	ret = snd_soc_register_component(&pdev->dev,
					 &mtk_afe_pcm_dai_component,
					 mtk_afe_pcm_dais,
					 ARRAY_SIZE(mtk_afe_pcm_dais));
	if (ret)
		goto err_platform;

	mtk_afe_init_debugfs(afe);

	dev_info(&pdev->dev, "MTK AFE driver initialized.\n");
	return 0;

err_platform:
	snd_soc_unregister_platform(&pdev->dev);
err_pm_disable:
	pm_runtime_disable(&pdev->dev);
	return ret;
}

static int mtk_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mtk_afe *afe = platform_get_drvdata(pdev);

	mtk_afe_cleanup_debugfs(afe);

	if (afe && afe->backup_regs)
		kfree(afe->backup_regs);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		mtk_afe_runtime_suspend(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mtk_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt8167-afe-pcm", },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_afe_pcm_dt_match);

static const struct dev_pm_ops mtk_afe_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_afe_runtime_suspend, mtk_afe_runtime_resume,
			   NULL)
};

static struct platform_driver mtk_afe_pcm_driver = {
	.driver = {
		   .name = "mtk-afe-pcm",
		   .of_match_table = mtk_afe_pcm_dt_match,
		   .pm = &mtk_afe_pm_ops,
	},
	.probe = mtk_afe_pcm_dev_probe,
	.remove = mtk_afe_pcm_dev_remove,
};

module_platform_driver(mtk_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC AFE platform driver");
MODULE_LICENSE("GPL v2");
