// SPDX-License-Identifier: GPL-2.0
//
// Mediatek ALSA SoC AFE platform driver for 6768
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Michael Hsiao <michael.hsiao@mediatek.com>

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>

#if IS_ENABLED(CONFIG_MTK_ACAO_SUPPORT)
#include "mtk_mcdi_governor_hint.h"
#endif

#include "../common/mtk-afe-debug.h"
#include "../common/mtk-afe-platform-driver.h"
#include "../common/mtk-afe-fe-dai.h"
#include "../common/mtk-sp-pcm-ops.h"
#include "../common/mtk-sram-manager.h"
#include "../common/mtk-mmap-ion.h"

#include "mt6768-afe-common.h"
#include "mt6768-afe-clk.h"
#include "mt6768-afe-gpio.h"
#include "mt6768-interconnection.h"

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
#include "../audio_dsp/mtk-dsp-common.h"
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
#include "../scp_spk/mtk-scp-spk-common.h"
#endif

static const struct snd_pcm_hardware mt6768_afe_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE |
		    SNDRV_PCM_FMTBIT_S24_LE |
		    SNDRV_PCM_FMTBIT_S32_LE),
	.period_bytes_min = 256,
	.period_bytes_max = 4 * 48 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.buffer_bytes_max = 8 * 48 * 1024,
	.fifo_size = 0,
};

static int mt6768_fe_startup(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int memif_num = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	const struct snd_pcm_hardware *mtk_afe_hardware = afe->mtk_afe_hardware;
	int ret;

	memif->substream = substream;

	snd_pcm_hw_constraint_step(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 16);

	snd_soc_set_runtime_hwparams(substream, mtk_afe_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		dev_err(afe->dev, "snd_pcm_hw_constraint_integer failed\n");

	/* dynamic allocate irq to memif */
	if (memif->irq_usage < 0) {
		int irq_id = mtk_dynamic_irq_acquire(afe);

		if (irq_id != afe->irqs_size) {
			/* link */
			memif->irq_usage = irq_id;
		} else {
			dev_err(afe->dev, "%s() error: no more asys irq\n",
				__func__);
			ret = -EBUSY;
		}
	}

	return ret;
}

void mt6768_fe_shutdown(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int memif_num = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;

	memif->substream = NULL;
	afe_priv->irq_cnt[memif_num] = 0;
	afe_priv->xrun_assert[memif_num] = 0;

	if (!memif->const_irq) {
		mtk_dynamic_irq_release(afe, irq_id);
		memif->irq_usage = -1;
		memif->substream = NULL;
	}
}

int mt6768_fe_trigger(struct snd_pcm_substream *substream, int cmd,
		      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int irq_id = memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int counter = runtime->period_size;
	unsigned int rate = runtime->rate;
	int fs;
	int ret = 0;

	dev_info(afe->dev, "%s(), %s cmd %d, irq_id %d\n",
		 __func__, memif->data->name, cmd, irq_id);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) ||\
	IS_ENABLED(CONFIG_MTK_VOW_BARGE_IN_SUPPORT)
		/* with dsp enable, not to set when stop_threshold = ~(0U) */
		if (runtime->stop_threshold == ~(0U))
			ret = 0;
		else
			ret = mtk_memif_set_enable(afe, id);
#else
		ret = mtk_memif_set_enable(afe, id);
#endif

		/*
		 * for small latency record
		 * ul memif need read some data before irq enable
		 */
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if ((runtime->period_size * 1000) / rate <= 10)
				udelay(300);
		}

		if (ret) {
			dev_err(afe->dev, "%s(), error, id %d, memif enable, ret %d\n",
				__func__, id, ret);
			return ret;
		}

		/* set irq counter */
		if (afe_priv->irq_cnt[id] > 0)
			counter = afe_priv->irq_cnt[id];

		mtk_regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				       irq_data->irq_cnt_maskbit,
				       counter, irq_data->irq_cnt_shift);

		/* set irq fs */
		fs = afe->irq_fs(substream, runtime->rate);

		if (fs < 0)
			return -EINVAL;

		mtk_regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
				       irq_data->irq_fs_maskbit,
				       fs, irq_data->irq_fs_shift);
		/* enable interrupt */
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) ||\
	IS_ENABLED(CONFIG_MTK_VOW_BARGE_IN_SUPPORT)
		if (runtime->stop_threshold != ~(0U))
#endif
			mtk_regmap_update_bits(afe->regmap,
				irq_data->irq_en_reg, 1,
				1, irq_data->irq_en_shift);

		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		if (afe_priv->xrun_assert[id] > 0) {
			if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
				int avail = snd_pcm_capture_avail(runtime);

				if (avail >= runtime->buffer_size) {
					dev_warn(afe->dev, "%s(), id %d, xrun assert\n",
						 __func__, id);
				}
			}
		}
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) ||\
	IS_ENABLED(CONFIG_MTK_VOW_BARGE_IN_SUPPORT)
		if (runtime->stop_threshold == ~(0U))
			ret = 0;
		else
			ret = mtk_memif_set_disable(afe, id);
#else
		ret = mtk_memif_set_disable(afe, id);
#endif
		if (ret) {
			dev_err(afe->dev, "%s(), error, id %d, memif enable, ret %d\n",
				__func__, id, ret);
		}

		/* disable interrupt */
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) ||\
	IS_ENABLED(CONFIG_MTK_VOW_BARGE_IN_SUPPORT)
		if (runtime->stop_threshold != ~(0U))
#endif
			mtk_regmap_update_bits(afe->regmap,
				irq_data->irq_en_reg, 1,
				0, irq_data->irq_en_shift);
		/* and clear pending IRQ */
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) ||\
	IS_ENABLED(CONFIG_MTK_VOW_BARGE_IN_SUPPORT)
		if (runtime->stop_threshold != ~(0U))
#endif
			regmap_write(afe->regmap, irq_data->irq_clr_reg,
					1 << irq_data->irq_clr_shift);
		return ret;
	default:
		return -EINVAL;
	}
}

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
int mt6768_fe_prepare(struct snd_pcm_substream *substream,
		      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int irq_id = memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int counter = runtime->period_size;
	int fs;
	int ret;

	ret = mtk_afe_fe_prepare(substream, dai);
	if (ret)
		goto exit;

	/* set irq counter */
	mtk_regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
			       irq_data->irq_cnt_maskbit,
			       counter, irq_data->irq_cnt_shift);

	/* set irq fs */
	fs = afe->irq_fs(substream, runtime->rate);

	if (fs < 0)
		return -EINVAL;

	mtk_regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
			       irq_data->irq_fs_maskbit,
			       fs, irq_data->irq_fs_shift);
exit:
	return ret;
}
#endif

static int mt6768_memif_fs(struct snd_pcm_substream *substream,
			   unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;

	return mt6768_rate_transform(afe->dev, rate, id);
}

static int mt6768_get_dai_fs(struct mtk_base_afe *afe,
			     int dai_id, unsigned int rate)
{
	return mt6768_rate_transform(afe->dev, rate, dai_id);
}

static int mt6768_irq_fs(struct snd_pcm_substream *substream, unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);

	return mt6768_general_rate_transform(afe->dev, rate);
}

int mt6768_get_memif_pbuf_size(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	if ((runtime->period_size * 1000) / runtime->rate > 10)
		return MT6768_MEMIF_PBUF_SIZE_256_BYTES;
	else
		return MT6768_MEMIF_PBUF_SIZE_32_BYTES;
}

/* FE DAIs */
static const struct snd_soc_dai_ops mt6768_memif_dai_ops = {
	.startup	= mt6768_fe_startup,
	.shutdown	= mt6768_fe_shutdown,
	.hw_params	= mtk_afe_fe_hw_params,
	.hw_free	= mtk_afe_fe_hw_free,
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	.prepare	= mt6768_fe_prepare,
#else
	.prepare	= mtk_afe_fe_prepare,
#endif
	.trigger	= mt6768_fe_trigger,
};

#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_PCM_DAI_RATES (SNDRV_PCM_RATE_8000 |\
			   SNDRV_PCM_RATE_16000 |\
			   SNDRV_PCM_RATE_32000 |\
			   SNDRV_PCM_RATE_48000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mt6768_memif_dai_driver[] = {
	/* FE DAIs: memory intefaces to CPU */
	{
		.name = "DL1",
		.id = MT6768_MEMIF_DL1,
		.playback = {
			.stream_name = "DL1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6768_memif_dai_ops,
	},
	{
		.name = "DL12",
		.id = MT6768_MEMIF_DL12,
		.playback = {
			.stream_name = "DL12",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6768_memif_dai_ops,
	},
	{
		.name = "DL2",
		.id = MT6768_MEMIF_DL2,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6768_memif_dai_ops,
	},
	{
		.name = "DL3",
		.id = MT6768_MEMIF_DL3,
		.playback = {
			.stream_name = "DL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6768_memif_dai_ops,
	},
	{
		.name = "UL1",
		.id = MT6768_MEMIF_VUL12,
		.capture = {
			.stream_name = "UL1",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6768_memif_dai_ops,
	},
	{
		.name = "UL2",
		.id = MT6768_MEMIF_AWB,
		.capture = {
			.stream_name = "UL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6768_memif_dai_ops,
	},
	{
		.name = "UL3",
		.id = MT6768_MEMIF_VUL2,
		.capture = {
			.stream_name = "UL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6768_memif_dai_ops,
	},
	{
		.name = "UL4",
		.id = MT6768_MEMIF_AWB2,
		.capture = {
			.stream_name = "UL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6768_memif_dai_ops,
	},
	{
		.name = "UL7",
		.id = MT6768_MEMIF_VUL,
		.capture = {
			.stream_name = "UL7",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6768_memif_dai_ops,
	},
	{
		.name = "UL_MONO_1",
		.id = MT6768_MEMIF_MOD_DAI,
		.capture = {
			.stream_name = "UL_MONO_1",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_DAI_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt6768_memif_dai_ops,
	},
};

/* kcontrol */
static int mt6768_irq_cnt1_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] =
			afe_priv->irq_cnt[MT6768_PRIMARY_MEMIF];
	return 0;
}

static int mt6768_irq_cnt1_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6768_PRIMARY_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	unsigned int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6768_irq_cnt2_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] =
		afe_priv->irq_cnt[MT6768_RECORD_MEMIF];
	return 0;
}

static int mt6768_irq_cnt2_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6768_RECORD_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	unsigned int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6768_deep_irq_cnt_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->irq_cnt[MT6768_DEEP_MEMIF];
	return 0;
}

static int mt6768_deep_irq_cnt_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6768_DEEP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	unsigned int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6768_voip_rx_irq_cnt_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->irq_cnt[MT6768_VOIP_MEMIF];
	return 0;
}

static int mt6768_voip_rx_irq_cnt_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6768_VOIP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;
	unsigned int irq_cnt = afe_priv->irq_cnt[memif_num];

	dev_info(afe->dev, "%s(), irq_id %d, irq_cnt = %d, value = %ld\n",
		 __func__,
		 irq_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];
	afe_priv->irq_cnt[memif_num] = irq_cnt;

	if (pm_runtime_status_suspended(afe->dev) || irq_id < 0) {
		dev_info(afe->dev, "%s(), suspended || irq_id %d, not set\n",
			 __func__, irq_id);
	} else {
		struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
		const struct mtk_base_irq_data *irq_data = irqs->irq_data;

		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit
				   << irq_data->irq_cnt_shift,
				   irq_cnt << irq_data->irq_cnt_shift);
	}

	return 0;
}

static int mt6768_deep_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->deep_playback_state;
	return 0;
}

static int mt6768_deep_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6768_DEEP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->deep_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->deep_playback_state == 1) {
		memif->ack_enable = true;
#if IS_ENABLED(CONFIG_MTK_ACAO_SUPPORT)
		system_idle_hint_request(SYSTEM_IDLE_HINT_USER_AUDIO, 1);
#endif
	} else {
		memif->ack_enable = false;
#if IS_ENABLED(CONFIG_MTK_ACAO_SUPPORT)
		system_idle_hint_request(SYSTEM_IDLE_HINT_USER_AUDIO, 0);
#endif
	}

	return 0;
}

static int mt6768_fast_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->fast_playback_state;
	return 0;
}

static int mt6768_fast_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6768_FAST_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->fast_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->fast_playback_state == 1)
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt6768_primary_scene_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->primary_playback_state;
	return 0;
}

static int mt6768_primary_scene_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6768_PRIMARY_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->primary_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->primary_playback_state == 1)
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt6768_voip_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->voip_rx_state;
	return 0;
}

static int mt6768_voip_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6768_VOIP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->voip_rx_state = ucontrol->value.integer.value[0];

	if (afe_priv->voip_rx_state == 1)
		memif->use_dram_only = 1;
	else
		memif->use_dram_only = 0;

	return 0;
}

static int mt6768_record_xrun_assert_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = afe_priv->xrun_assert[MT6768_RECORD_MEMIF];

	ucontrol->value.integer.value[0] = xrun_assert;
	return 0;
}

static int mt6768_record_xrun_assert_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), xrun_assert %d\n", __func__, xrun_assert);
	afe_priv->xrun_assert[MT6768_RECORD_MEMIF] = xrun_assert;
	return 0;
}

static int mt6768_echo_ref_xrun_assert_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = afe_priv->xrun_assert[MT6768_ECHO_REF_MEMIF];

	ucontrol->value.integer.value[0] = xrun_assert;
	return 0;
}

static int mt6768_echo_ref_xrun_assert_set(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int xrun_assert = ucontrol->value.integer.value[0];

	dev_info(afe->dev, "%s(), xrun_assert %d\n", __func__, xrun_assert);
	afe_priv->xrun_assert[MT6768_ECHO_REF_MEMIF] = xrun_assert;
	return 0;
}

static int mt6768_sram_size_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_audio_sram *sram = afe->sram;

	ucontrol->value.integer.value[0] =
		mtk_audio_sram_get_size(sram, sram->prefer_mode);

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_VOW_BARGE_IN_SUPPORT)
static int mt6768_vow_barge_in_irq_id_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_BARGEIN_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	int irq_id = memif->irq_usage;

	ucontrol->value.integer.value[0] = irq_id;
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
static int mt6768_adsp_primary_mem_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_DSP_PRIMARY_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = memif->use_adsp_share_mem;
	return 0;

}

static int mt6768_adsp_primary_mem_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_DSP_PRIMARY_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	memif->use_adsp_share_mem = ucontrol->value.integer.value[0];
	return 0;
}


static int mt6768_adsp_deepbuffer_mem_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_DSP_DEEPBUFFER_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = memif->use_adsp_share_mem;
	return 0;

}

static int mt6768_adsp_deepbuffer_mem_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_DSP_DEEPBUFFER_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	memif->use_adsp_share_mem = ucontrol->value.integer.value[0];
	return 0;
}

static int mt6768_adsp_voip_mem_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_DSP_VOIP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = memif->use_adsp_share_mem;
	return 0;

}

static int mt6768_adsp_voip_mem_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_DSP_VOIP_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	memif->use_adsp_share_mem = ucontrol->value.integer.value[0];
	return 0;
}


static int mt6768_adsp_playback_mem_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_DSP_PLAYBACKDL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = memif->use_adsp_share_mem;
	return 0;

}

static int mt6768_adsp_playback_mem_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_numdl = MT6768_DSP_PLAYBACKDL_MEMIF;
	int memif_numul = MT6768_DSP_PLAYBACKUL_MEMIF;
	struct mtk_base_afe_memif *memifdl = &afe->memif[memif_numdl];
	struct mtk_base_afe_memif *memiful = &afe->memif[memif_numul];

	memifdl->use_adsp_share_mem = ucontrol->value.integer.value[0];
	memiful->use_adsp_share_mem = ucontrol->value.integer.value[0];

	return 0;
}

static int mt6768_adsp_offload_mem_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_DSP_OFFLOAD_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = memif->use_adsp_share_mem;
	return 0;

}

static int mt6768_adsp_offload_mem_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int mem_num = MT6768_DSP_OFFLOAD_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[mem_num];

	memif->use_adsp_share_mem = ucontrol->value.integer.value[0];
	return 0;
}

static int mt6768_adsp_capture_mem_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_DSP_CAPTURE_UL1_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = memif->use_adsp_share_mem;
	return 0;

}

static int mt6768_adsp_capture_mem_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int mem_num = MT6768_DSP_CAPTURE_UL1_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[mem_num];

	memif->use_adsp_share_mem = ucontrol->value.integer.value[0];
	return 0;
}

static int mt6768_adsp_ref_mem_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_DSP_REF_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = memif->use_adsp_share_mem;
	return 0;

}

static int mt6768_adsp_ref_mem_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int mem_num = MT6768_DSP_REF_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[mem_num];

	memif->use_adsp_share_mem = ucontrol->value.integer.value[0];
	return 0;
}

#endif

static int mt6768_mmap_dl_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->mmap_playback_state;
	return 0;
}

static int mt6768_mmap_dl_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6768_MMAP_DL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->mmap_playback_state = ucontrol->value.integer.value[0];

	if (afe_priv->mmap_playback_state == 1) {
		unsigned long phy_addr = 0;
		void *vir_addr = NULL;

		mtk_get_mmap_dl_buffer(&phy_addr, &vir_addr);

		if (phy_addr != 0x0 && vir_addr != NULL)
			memif->use_mmap_share_mem = 1;
	} else
		memif->use_mmap_share_mem = 0;

	dev_info(afe->dev, "%s(), state %d, mem %d\n", __func__,
		afe_priv->mmap_playback_state, memif->use_mmap_share_mem);
	return 0;
}

static int mt6768_mmap_ul_scene_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	ucontrol->value.integer.value[0] = afe_priv->mmap_record_state;
	return 0;
}

static int mt6768_mmap_ul_scene_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int memif_num = MT6768_MMAP_UL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	afe_priv->mmap_record_state = ucontrol->value.integer.value[0];

	if (afe_priv->mmap_record_state == 1) {
		unsigned long phy_addr = 0;
		void *vir_addr = NULL;

		mtk_get_mmap_ul_buffer(&phy_addr, &vir_addr);

		if (phy_addr != 0x0 && vir_addr != NULL)
			memif->use_mmap_share_mem = 2;
	} else
		memif->use_mmap_share_mem = 0;

	dev_info(afe->dev, "%s(), state %d, mem %d\n", __func__,
		afe_priv->mmap_playback_state, memif->use_mmap_share_mem);
	return 0;
}

static int mt6768_mmap_ion_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int mt6768_mmap_ion_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	//mtk_get_ion_buffer();
	return 0;
}

static int mt6768_dl_mmap_fd_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_MMAP_DL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = (memif->use_mmap_share_mem == 1) ?
						mtk_get_mmap_dl_fd() : 0;
	//dev_info(afe->dev, "%s, fd %d\n", __func__,
	//ucontrol->value.integer.value[0]);
	return 0;
}

static int mt6768_dl_mmap_fd_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt6768_ul_mmap_fd_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(cmpnt);
	int memif_num = MT6768_MMAP_UL_MEMIF;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];

	ucontrol->value.integer.value[0] = (memif->use_mmap_share_mem == 2) ?
						mtk_get_mmap_ul_fd() : 0;
	//dev_info(afe->dev, "%s, fd %d\n", __func__,
	//ucontrol->value.integer.value[0]);
	return 0;
}

static int mt6768_ul_mmap_fd_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static const struct snd_kcontrol_new mt6768_pcm_kcontrols[] = {
	SOC_SINGLE_EXT("Audio IRQ1 CNT", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6768_irq_cnt1_get, mt6768_irq_cnt1_set),
	SOC_SINGLE_EXT("Audio IRQ2 CNT", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6768_irq_cnt2_get, mt6768_irq_cnt2_set),
	SOC_SINGLE_EXT("deep_buffer_irq_cnt", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6768_deep_irq_cnt_get, mt6768_deep_irq_cnt_set),
	SOC_SINGLE_EXT("voip_rx_irq_cnt", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6768_voip_rx_irq_cnt_get, mt6768_voip_rx_irq_cnt_set),
	SOC_SINGLE_EXT("deep_buffer_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_deep_scene_get, mt6768_deep_scene_set),
	SOC_SINGLE_EXT("record_xrun_assert", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_record_xrun_assert_get,
		       mt6768_record_xrun_assert_set),
	SOC_SINGLE_EXT("echo_ref_xrun_assert", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_echo_ref_xrun_assert_get,
		       mt6768_echo_ref_xrun_assert_set),
	SOC_SINGLE_EXT("fast_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_fast_scene_get, mt6768_fast_scene_set),
	SOC_SINGLE_EXT("primary_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_primary_scene_get, mt6768_primary_scene_set),
	SOC_SINGLE_EXT("voip_rx_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_voip_scene_get, mt6768_voip_scene_set),
	SOC_SINGLE_EXT("sram_size", SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6768_sram_size_get, NULL),
#if IS_ENABLED(CONFIG_MTK_VOW_BARGE_IN_SUPPORT)
	SOC_SINGLE_EXT("vow_barge_in_irq_id", SND_SOC_NOPM, 0, 0x3ffff, 0,
		       mt6768_vow_barge_in_irq_id_get, NULL),
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	SOC_SINGLE_EXT("adsp_primary_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_adsp_primary_mem_get,
		       mt6768_adsp_primary_mem_set),
	SOC_SINGLE_EXT("adsp_deepbuffer_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_adsp_deepbuffer_mem_get,
		       mt6768_adsp_deepbuffer_mem_set),
	SOC_SINGLE_EXT("adsp_voip_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_adsp_voip_mem_get,
		       mt6768_adsp_voip_mem_set),
	SOC_SINGLE_EXT("adsp_playback_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_adsp_playback_mem_get,
		       mt6768_adsp_playback_mem_set),
	SOC_SINGLE_EXT("adsp_offload_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_adsp_offload_mem_get,
		       mt6768_adsp_offload_mem_set),
	SOC_SINGLE_EXT("adsp_capture_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_adsp_capture_mem_get,
		       mt6768_adsp_capture_mem_set),
	SOC_SINGLE_EXT("adsp_ref_sharemem_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_adsp_ref_mem_get,
		       mt6768_adsp_ref_mem_set),
#endif
	SOC_SINGLE_EXT("mmap_play_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_mmap_dl_scene_get, mt6768_mmap_dl_scene_set),
	SOC_SINGLE_EXT("mmap_record_scenario", SND_SOC_NOPM, 0, 0x1, 0,
		       mt6768_mmap_ul_scene_get, mt6768_mmap_ul_scene_set),
	SOC_SINGLE_EXT("aaudio_ion",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6768_mmap_ion_get,
		       mt6768_mmap_ion_set),
	SOC_SINGLE_EXT("aaudio_dl_mmap_fd",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6768_dl_mmap_fd_get,
		       mt6768_dl_mmap_fd_set),
	SOC_SINGLE_EXT("aaudio_ul_mmap_fd",
		       SND_SOC_NOPM, 0, 0xffffffff, 0,
		       mt6768_ul_mmap_fd_get,
		       mt6768_ul_mmap_fd_set),
};

/* dma widget & routes*/
static const struct snd_kcontrol_new memif_ul1_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN21,
				    I_ADDA_UL_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN22,
				    I_ADDA_UL_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch3_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN9,
				    I_ADDA_UL_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul1_ch4_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN10,
				    I_ADDA_UL_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul7_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN9,
				    I_ADDA_UL_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul7_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN10,
				    I_ADDA_UL_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1", AFE_CONN5,
				    I_I2S0_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN5,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN5,
				    I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN5,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN5,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_2_CAP_CH1", AFE_CONN5,
				    I_PCM_2_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH1", AFE_CONN5,
				    I_I2S2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN5_1,
				    I_CONNSYS_I2S_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul2_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2", AFE_CONN6,
				    I_I2S0_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN6,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN6,
				    I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN6,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN6,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_2_CAP_CH1", AFE_CONN6,
				    I_PCM_2_CAP_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S2_CH2", AFE_CONN6,
				    I_I2S2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN6_1,
				    I_CONNSYS_I2S_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH1", AFE_CONN32_1,
				    I_CONNSYS_I2S_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN32,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN32,
				    I_DL2_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul3_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("CONNSYS_I2S_CH2", AFE_CONN33_1,
				    I_CONNSYS_I2S_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH1", AFE_CONN38,
				    I_ADDA_UL_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH1", AFE_CONN38,
				    I_DL1_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH1", AFE_CONN38,
				    I_DL12_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH1", AFE_CONN38,
				    I_DL2_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH1", AFE_CONN38,
				    I_DL3_CH1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH1", AFE_CONN38,
				    I_I2S0_CH1, 1, 0),
};

static const struct snd_kcontrol_new memif_ul4_ch2_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("ADDA_UL_CH2", AFE_CONN39,
				    I_ADDA_UL_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL1_CH2", AFE_CONN39,
				    I_DL1_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL12_CH2", AFE_CONN39,
				    I_DL12_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL2_CH2", AFE_CONN39,
				    I_DL2_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DL3_CH2", AFE_CONN39,
				    I_DL3_CH2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I2S0_CH2", AFE_CONN39,
				    I_I2S0_CH2, 1, 0),
};

static const struct snd_kcontrol_new memif_ul_mono_1_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("PCM_2_CAP_CH1", AFE_CONN12,
				    I_PCM_2_CAP_CH1, 1, 0),
};

static const struct snd_kcontrol_new mtk_dsp_dl_playback_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL1", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL2", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL12", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL3", SND_SOC_NOPM, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("DSP_DL4", SND_SOC_NOPM, 0, 1, 0),
};

static const struct snd_soc_dapm_widget mt6768_memif_widgets[] = {
	/* inter-connections */
	SND_SOC_DAPM_MIXER("UL1_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch1_mix, ARRAY_SIZE(memif_ul1_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL1_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch2_mix, ARRAY_SIZE(memif_ul1_ch2_mix)),
	SND_SOC_DAPM_MIXER("UL1_CH3", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch3_mix, ARRAY_SIZE(memif_ul1_ch3_mix)),
	SND_SOC_DAPM_MIXER("UL1_CH4", SND_SOC_NOPM, 0, 0,
			   memif_ul1_ch4_mix, ARRAY_SIZE(memif_ul1_ch4_mix)),

	SND_SOC_DAPM_MIXER("UL2_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch1_mix, ARRAY_SIZE(memif_ul2_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL2_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul2_ch2_mix, ARRAY_SIZE(memif_ul2_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL3_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch1_mix, ARRAY_SIZE(memif_ul3_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL3_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul3_ch2_mix, ARRAY_SIZE(memif_ul3_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL4_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch1_mix, ARRAY_SIZE(memif_ul4_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL4_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul4_ch2_mix, ARRAY_SIZE(memif_ul4_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL7_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul7_ch1_mix, ARRAY_SIZE(memif_ul7_ch1_mix)),
	SND_SOC_DAPM_MIXER("UL7_CH2", SND_SOC_NOPM, 0, 0,
			   memif_ul7_ch2_mix, ARRAY_SIZE(memif_ul7_ch2_mix)),

	SND_SOC_DAPM_MIXER("UL_MONO_1_CH1", SND_SOC_NOPM, 0, 0,
			   memif_ul_mono_1_mix,
			   ARRAY_SIZE(memif_ul_mono_1_mix)),

	SND_SOC_DAPM_MIXER("DSP_DL", SND_SOC_NOPM, 0, 0,
			   mtk_dsp_dl_playback_mix,
			   ARRAY_SIZE(mtk_dsp_dl_playback_mix)),

	SND_SOC_DAPM_INPUT("UL1_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL2_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL3_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL4_VIRTUAL_INPUT"),
	SND_SOC_DAPM_INPUT("UL7_VIRTUAL_INPUT"),

	SND_SOC_DAPM_OUTPUT("DL_TO_DSP"),
};

static const struct snd_soc_dapm_route mt6768_memif_routes[] = {
	{"UL1", NULL, "UL1_CH1"},
	{"UL1", NULL, "UL1_CH2"},
	{"UL1", NULL, "UL1_CH3"},
	{"UL1", NULL, "UL1_CH4"},
	{"UL1_CH1", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL1_CH2", "ADDA_UL_CH2", "ADDA Capture"},
	{"UL1_CH3", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL1_CH4", "ADDA_UL_CH2", "ADDA Capture"},

	{"UL2", NULL, "UL2_CH1"},
	{"UL2", NULL, "UL2_CH2"},

	/* cannot connect FE to FE directly */
	{"UL2_CH1", "DL1_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL1_CH2", "Hostless_UL2 UL"},
	{"UL2_CH1", "DL12_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL12_CH2", "Hostless_UL2 UL"},
	{"UL2_CH1", "DL2_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL2_CH2", "Hostless_UL2 UL"},
	{"UL2_CH1", "DL3_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL3_CH2", "Hostless_UL2 UL"},
	{"UL2_CH1", "DL4_CH1", "Hostless_UL2 UL"},
	{"UL2_CH2", "DL4_CH2", "Hostless_UL2 UL"},

	{"Hostless_UL2 UL", NULL, "UL2_VIRTUAL_INPUT"},

	{"UL2_CH1", "I2S0_CH1", "I2S0"},
	{"UL2_CH2", "I2S0_CH2", "I2S0"},
	{"UL2_CH1", "I2S2_CH1", "I2S2"},
	{"UL2_CH2", "I2S2_CH2", "I2S2"},

	{"UL2_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL2_CH2", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL2_CH1", "PCM_2_CAP_CH1", "PCM 2 Capture"},
	{"UL2_CH2", "PCM_2_CAP_CH1", "PCM 2 Capture"},

	{"UL_MONO_1", NULL, "UL_MONO_1_CH1"},
	{"UL_MONO_1_CH1", "PCM_1_CAP_CH1", "PCM 1 Capture"},
	{"UL_MONO_1_CH1", "PCM_2_CAP_CH1", "PCM 2 Capture"},

	{"UL2_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL2_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},

	{"UL3", NULL, "UL3_CH1"},
	{"UL3", NULL, "UL3_CH2"},
	{"UL3_CH1", "CONNSYS_I2S_CH1", "Connsys I2S"},
	{"UL3_CH2", "CONNSYS_I2S_CH2", "Connsys I2S"},

	{"Hostless_UL4 UL", NULL, "UL4_VIRTUAL_INPUT"},

	{"UL4_CH1", "DL1_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL1_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL12_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL12_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL2_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL2_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL3_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL3_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "DL4_CH1", "Hostless_UL4 UL"},
	{"UL4_CH2", "DL4_CH2", "Hostless_UL4 UL"},
	{"UL4_CH1", "I2S0_CH1", "I2S0"},
	{"UL4_CH2", "I2S0_CH2", "I2S0"},

	{"UL4", NULL, "UL4_CH1"},
	{"UL4", NULL, "UL4_CH2"},
	{"UL4_CH1", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL4_CH2", "ADDA_UL_CH2", "ADDA Capture"},

	{"UL7", NULL, "UL7_CH1"},
	{"UL7", NULL, "UL7_CH2"},
	{"UL7_CH1", "ADDA_UL_CH1", "ADDA Capture"},
	{"UL7_CH2", "ADDA_UL_CH2", "ADDA Capture"},

	{"DL_TO_DSP", NULL, "Hostless_DSP_DL DL"},
	{"Hostless_DSP_DL DL", NULL, "DSP_DL"},

	{"DSP_DL", "DSP_DL1", "DL1"},
	{"DSP_DL", "DSP_DL2", "DL2"},
	{"DSP_DL", "DSP_DL12", "DL12"},
	{"DSP_DL", "DSP_DL3", "DL3"},
	{"DSP_DL", "DSP_DL4", "DL4"},
};

static const struct mtk_base_memif_data memif_data[MT6768_MEMIF_NUM] = {
	[MT6768_MEMIF_DL1] = {
		.name = "DL1",
		.id = MT6768_MEMIF_DL1,
		.reg_ofs_base = AFE_DL1_BASE,
		.reg_ofs_cur = AFE_DL1_CUR,
		.reg_ofs_end = AFE_DL1_END,
		.reg_ofs_base_msb = AFE_DL1_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL1_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL1_END_MSB,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = DL1_MODE_SFT,
		.fs_maskbit = DL1_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = DL1_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL1_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = DL1_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_MEMIF_PBUF_SIZE,
		.pbuf_mask = DL1_PBUF_SIZE_MASK,
		.pbuf_shift = DL1_PBUF_SIZE_SFT,
		.minlen_reg = AFE_MEMIF_MINLEN,
		.minlen_mask = DL1_MINLEN_MASK,
		.minlen_shift = DL1_MINLEN_SFT,
	},
	[MT6768_MEMIF_DL12] = {
		.name = "DL12",
		.id = MT6768_MEMIF_DL12,
		.reg_ofs_base = AFE_DL1_D2_BASE,
		.reg_ofs_cur = AFE_DL1_D2_CUR,
		.reg_ofs_end = AFE_DL1_D2_END,
		.reg_ofs_base_msb = AFE_DL1_D2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL1_D2_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL1_D2_END_MSB,
		.fs_reg = AFE_DAC_CON0,
		.fs_shift = DL1_DATA2_MODE_SFT,
		.fs_maskbit = DL1_DATA2_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = DL1_DATA2_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL12_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = DL12_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_MEMIF_PBUF_SIZE,
		.pbuf_mask = DL1_DATA2_PBUF_SIZE_MASK,
		.pbuf_shift = DL1_DATA2_PBUF_SIZE_SFT,
		.minlen_reg = AFE_MEMIF_MINLEN,
		.minlen_mask = DL1_DATA2_MINLEN_MASK,
		.minlen_shift = DL1_DATA2_MINLEN_SFT,
	},
	[MT6768_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT6768_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.reg_ofs_end = AFE_DL2_END,
		.reg_ofs_base_msb = AFE_DL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL2_END_MSB,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = DL2_MODE_SFT,
		.fs_maskbit = DL2_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = DL2_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL2_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = DL2_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_MEMIF_PBUF_SIZE,
		.pbuf_mask = DL2_PBUF_SIZE_MASK,
		.pbuf_shift = DL2_PBUF_SIZE_SFT,
		.minlen_reg = AFE_MEMIF_MINLEN,
		.minlen_mask = DL2_MINLEN_MASK,
		.minlen_shift = DL2_MINLEN_SFT,
	},
	[MT6768_MEMIF_DL3] = {
		.name = "DL3",
		.id = MT6768_MEMIF_DL3,
		.reg_ofs_base = AFE_DL3_BASE,
		.reg_ofs_cur = AFE_DL3_CUR,
		.reg_ofs_end = AFE_DL3_END,
		.reg_ofs_base_msb = AFE_DL3_BASE_MSB,
		.reg_ofs_cur_msb = AFE_DL3_CUR_MSB,
		.reg_ofs_end_msb = AFE_DL3_END_MSB,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = DL3_MODE_SFT,
		.fs_maskbit = DL3_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = DL3_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = DL3_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = DL3_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
		.pbuf_reg = AFE_MEMIF_PBUF_SIZE,
		.pbuf_mask = DL3_PBUF_SIZE_MASK,
		.pbuf_shift = DL3_PBUF_SIZE_SFT,
		.minlen_reg = AFE_MEMIF_MINLEN,
		.minlen_mask = DL3_MINLEN_MASK,
		.minlen_shift = DL3_MINLEN_SFT,
	},
	[MT6768_MEMIF_MOD_DAI] = {
		.name = "MOD_DAI",
		.id = MT6768_MEMIF_MOD_DAI,
		.reg_ofs_base = AFE_MOD_DAI_BASE,
		.reg_ofs_cur = AFE_MOD_DAI_CUR,
		.reg_ofs_end = AFE_MOD_DAI_END,
		.reg_ofs_base_msb = AFE_MOD_DAI_BASE_MSB,
		.reg_ofs_cur_msb = AFE_MOD_DAI_CUR_MSB,
		.reg_ofs_end_msb = AFE_MOD_DAI_END_MSB,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = MOD_DAI_MODE_SFT,
		.fs_maskbit = MOD_DAI_MODE_MASK,
		.mono_reg = AFE_DAC_CON0,
		.mono_shift = MOD_DAI_DUP_WR_SFT,
		.mono_invert = 1,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = MOD_DAI_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = MOD_DAI_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6768_MEMIF_VUL] = {
		.name = "VUL",
		.id = MT6768_MEMIF_VUL,
		.reg_ofs_base = AFE_VUL_BASE,
		.reg_ofs_cur = AFE_VUL_CUR,
		.reg_ofs_end = AFE_VUL_END,
		.reg_ofs_base_msb = AFE_VUL_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_END_MSB,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = VUL_MODE_SFT,
		.fs_maskbit = VUL_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = VUL_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = VUL_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6768_MEMIF_VUL12] = {
		.name = "VUL12",
		.id = MT6768_MEMIF_VUL12,
		.reg_ofs_base = AFE_VUL_D2_BASE,
		.reg_ofs_cur = AFE_VUL_D2_CUR,
		.reg_ofs_end = AFE_VUL_D2_END,
		.reg_ofs_base_msb = AFE_VUL_D2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL_D2_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL_D2_END_MSB,
		.fs_reg = AFE_DAC_CON0,
		.fs_shift = VUL_DATA2_MODE_SFT,
		.fs_maskbit = VUL_DATA2_MODE_MASK,
		.mono_reg = AFE_DAC_CON0,
		.mono_shift = VUL_DATA2_DATA_SFT,
		.quad_ch_reg = AFE_MEMIF_PBUF_SIZE,
		.quad_ch_mask = VUL12_4CH_EN_MASK,
		.quad_ch_shift = VUL12_4CH_EN_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL_DATA2_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = VUL12_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6768_MEMIF_VUL2] = {
		.name = "VUL2",
		.id = MT6768_MEMIF_VUL2,
		.reg_ofs_base = AFE_VUL2_BASE,
		.reg_ofs_cur = AFE_VUL2_CUR,
		.reg_ofs_end = AFE_VUL2_END,
		.reg_ofs_base_msb = AFE_VUL2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_VUL2_CUR_MSB,
		.reg_ofs_end_msb = AFE_VUL2_END_MSB,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = VUL2_MODE_SFT,
		.fs_maskbit = VUL2_MODE_MASK,
		.mono_reg = AFE_DAC_CON2,
		.mono_shift = VUL2_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = VUL2_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = VUL2_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6768_MEMIF_AWB] = {
		.name = "AWB",
		.id = MT6768_MEMIF_AWB,
		.reg_ofs_base = AFE_AWB_BASE,
		.reg_ofs_cur = AFE_AWB_CUR,
		.reg_ofs_end = AFE_AWB_END,
		.reg_ofs_base_msb = AFE_AWB_BASE_MSB,
		.reg_ofs_cur_msb = AFE_AWB_CUR_MSB,
		.reg_ofs_end_msb = AFE_AWB_END_MSB,
		.fs_reg = AFE_DAC_CON1,
		.fs_shift = AWB_MODE_SFT,
		.fs_maskbit = AWB_MODE_MASK,
		.mono_reg = AFE_DAC_CON1,
		.mono_shift = AWB_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = AWB_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = AWB_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
	[MT6768_MEMIF_AWB2] = {
		.name = "AWB2",
		.id = MT6768_MEMIF_AWB2,
		.reg_ofs_base = AFE_AWB2_BASE,
		.reg_ofs_cur = AFE_AWB2_CUR,
		.reg_ofs_end = AFE_AWB2_END,
		.reg_ofs_base_msb = AFE_AWB2_BASE_MSB,
		.reg_ofs_cur_msb = AFE_AWB2_CUR_MSB,
		.reg_ofs_end_msb = AFE_AWB2_END_MSB,
		.fs_reg = AFE_DAC_CON2,
		.fs_shift = AWB2_MODE_SFT,
		.fs_maskbit = AWB2_MODE_MASK,
		.mono_reg = AFE_DAC_CON2,
		.mono_shift = AWB2_DATA_SFT,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = AWB2_ON_SFT,
		.hd_reg = AFE_MEMIF_HD_MODE,
		.hd_shift = AWB2_HD_SFT,
		.agent_disable_reg = -1,
		.agent_disable_shift = -1,
		.msb_reg = -1,
		.msb_shift = -1,
	},
};

static const struct mtk_base_irq_data irq_data[MT6768_IRQ_NUM] = {
	[MT6768_IRQ_0] = {
		.id = MT6768_IRQ_0,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT0,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0xf,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ0_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ0_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ0_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_EN1,
		.irq_scp_en_shift = IRQ0_MCU_SCP_EN_SFT,
	},
	[MT6768_IRQ_1] = {
		.id = MT6768_IRQ_1,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT1,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = 4,
		.irq_fs_maskbit = 0xf,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ1_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ1_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ1_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_EN1,
		.irq_scp_en_shift = IRQ1_MCU_SCP_EN_SFT,
	},
	[MT6768_IRQ_2] = {
		.id = MT6768_IRQ_2,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT2,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = 8,
		.irq_fs_maskbit = 0xf,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ2_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ2_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ2_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_EN1,
		.irq_scp_en_shift = IRQ2_MCU_SCP_EN_SFT,
	},
	[MT6768_IRQ_3] = {
		.id = MT6768_IRQ_3,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT3,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = 12,
		.irq_fs_maskbit = 0xf,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ3_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ3_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ3_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_EN1,
		.irq_scp_en_shift = IRQ3_MCU_SCP_EN_SFT,
	},
	[MT6768_IRQ_4] = {
		.id = MT6768_IRQ_4,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT4,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = 16,
		.irq_fs_maskbit = 0xf,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ4_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ4_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ4_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_EN1,
		.irq_scp_en_shift = IRQ4_MCU_SCP_EN_SFT,
	},
	[MT6768_IRQ_5] = {
		.id = MT6768_IRQ_5,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT5,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = 20,
		.irq_fs_maskbit = 0xf,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ5_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ5_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ5_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_EN1,
		.irq_scp_en_shift = IRQ5_MCU_SCP_EN_SFT,
	},
	[MT6768_IRQ_6] = {
		.id = MT6768_IRQ_6,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT6,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0xf,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ6_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ6_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ6_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_EN1,
		.irq_scp_en_shift = IRQ6_MCU_SCP_EN_SFT,
	},
	[MT6768_IRQ_7] = {
		.id = MT6768_IRQ_7,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT7,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_fs_reg = AFE_IRQ_MCU_CON1,
		.irq_fs_shift = 28,
		.irq_fs_maskbit = 0xf,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ7_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ7_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ7_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_EN1,
		.irq_scp_en_shift = IRQ7_MCU_SCP_EN_SFT,
	},
	[MT6768_IRQ_11] = {
		.id = MT6768_IRQ_11,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT11,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_fs_reg = AFE_IRQ_MCU_CON2,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0xf,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ11_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ11_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ11_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_EN1,
		.irq_scp_en_shift = IRQ11_MCU_SCP_EN_SFT,
	},
	[MT6768_IRQ_12] = {
		.id = MT6768_IRQ_12,
		.irq_cnt_reg = AFE_IRQ_MCU_CNT12,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0x3ffff,
		.irq_fs_reg = AFE_IRQ_MCU_CON2,
		.irq_fs_shift = 4,
		.irq_fs_maskbit = 0xf,
		.irq_en_reg = AFE_IRQ_MCU_CON0,
		.irq_en_shift = IRQ12_MCU_ON_SFT,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = IRQ12_MCU_CLR_SFT,
		.irq_ap_en_reg = AFE_IRQ_MCU_EN,
		.irq_ap_en_shift = IRQ12_MCU_EN_SFT,
		.irq_scp_en_reg = AFE_IRQ_MCU_EN1,
		.irq_scp_en_shift = IRQ12_MCU_SCP_EN_SFT,
	},
};

static bool mt6768_is_volatile_reg(struct device *dev, unsigned int reg)
{
	/* these auto-gen reg has read-only bit, so put it as volatile */
	/* volatile reg cannot be cached, so cannot be set when power off */
	switch (reg) {
	case AUDIO_TOP_CON0:	/* reg bit controlled by CCF */
	case AUDIO_TOP_CON1:	/* reg bit controlled by CCF */
	case AUDIO_TOP_CON3:
	case AFE_DAC_CON0:
	case AFE_IRQ_MCU_CON0:
	case AFE_IRQ_MCU_EN1:
	case AFE_IRQ_MCU_EN:
	case AFE_DL1_CUR_MSB:
	case AFE_DL1_CUR:
	case AFE_DL1_END:
	case AFE_DL2_CUR_MSB:
	case AFE_DL2_CUR:
	case AFE_DL2_END:
	case AFE_DL3_CUR_MSB:
	case AFE_DL3_CUR:
	case AFE_DL3_END:
	case AFE_DL1_D2_CUR_MSB:
	case AFE_DL1_D2_CUR:
	case AFE_DL1_D2_END:
	case AFE_ADDA_SRC_DEBUG_MON0:
	case AFE_ADDA_SRC_DEBUG_MON1:
	case AFE_ADDA_UL_SRC_MON0:
	case AFE_ADDA_UL_SRC_MON1:
	case AFE_VUL_CUR_MSB:
	case AFE_VUL_CUR:
	case AFE_VUL_END:
	case AFE_SIDETONE_MON:
	case AFE_SIDETONE_CON0:
	case AFE_SIDETONE_COEFF:
	case AFE_VUL2_CUR_MSB:
	case AFE_VUL2_CUR:
	case AFE_VUL2_END:
	case AFE_BUS_MON0:
	case AFE_MRGIF_MON0:
	case AFE_MRGIF_MON1:
	case AFE_MRGIF_MON2:
	case AFE_DAC_MON:
	case AFE_IRQ0_MCU_CNT_MON:
	case AFE_IRQ6_MCU_CNT_MON:
	case AFE_VUL_D2_CUR_MSB:
	case AFE_VUL_D2_CUR:
	case AFE_VUL_D2_END:
	case AFE_IRQ3_MCU_CNT_MON:
	case AFE_IRQ4_MCU_CNT_MON:
	case AFE_IRQ_MCU_STATUS:
	case AFE_IRQ_MCU_CLR:
	case AFE_IRQ_MCU_MON2:
	case AFE_IRQ1_MCU_CNT_MON:
	case AFE_IRQ2_MCU_CNT_MON:
	case AFE_IRQ1_MCU_EN_CNT_MON:
	case AFE_IRQ5_MCU_CNT_MON:
	case AFE_IRQ7_MCU_CNT_MON:
	case AFE_GAIN1_CUR:
	case AFE_GAIN2_CUR:
	case AFE_SRAM_DELSEL_CON0:
	case AFE_SRAM_DELSEL_CON2:
	case AFE_SRAM_DELSEL_CON3:
	case AFE_ASRC_2CH_CON12:
	case AFE_ASRC_2CH_CON13:
	case FPGA_CFG0:
	case FPGA_CFG1:
	case FPGA_CFG2:
	case FPGA_CFG3:
	case AUDIO_TOP_DBG_MON0:
	case AUDIO_TOP_DBG_MON1:
	case AFE_IRQ8_MCU_CNT_MON:
	case AFE_IRQ11_MCU_CNT_MON:
	case AFE_IRQ12_MCU_CNT_MON:
	case AFE_CBIP_MON0:
	case AFE_CBIP_SLV_MUX_MON0:
	case AFE_CBIP_SLV_DECODER_MON0:
	case AFE_AWB_CUR_MSB:
	case AFE_AWB_CUR:
	case AFE_AWB_END:
	case AFE_AWB2_CUR_MSB:
	case AFE_AWB2_CUR:
	case AFE_AWB2_END:
	case AFE_MOD_DAI_CUR_MSB:
	case AFE_MOD_DAI_CUR:
	case AFE_MOD_DAI_END:
	case AFE_ADDA_DL_SDM_FIFO_MON:
	case AFE_ADDA_DL_SRC_LCH_MON:
	case AFE_ADDA_DL_SRC_RCH_MON:
	case AFE_ADDA_DL_SDM_OUT_MON:
	case AFE_CONNSYS_I2S_MON:
	case AFE_ASRC_2CH_CON0:
	case AFE_ASRC_2CH_CON2:
	case AFE_ASRC_2CH_CON3:
	case AFE_ASRC_2CH_CON4:
	case AFE_ASRC_2CH_CON5:
	case AFE_ASRC_2CH_CON7:
	case AFE_ASRC_2CH_CON8:
	case AFE_ADDA_MTKAIF_MON0:
	case AFE_ADDA_MTKAIF_MON1:
	case AFE_AUD_PAD_TOP:
	case AFE_APLL_TUNER_CFG:	/* [20:31] is monitor */
		return true;
	default:
		return false;
	};
}

static const struct regmap_config mt6768_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,

	.volatile_reg = mt6768_is_volatile_reg,

	.max_register = AFE_MAX_REGISTER,
	.num_reg_defaults_raw = AFE_MAX_REGISTER,

	.cache_type = REGCACHE_FLAT,
};

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static irqreturn_t mt6768_afe_irq_handler(int irq_id, void *dev)
{
	struct mtk_base_afe *afe = dev;
	struct mtk_base_afe_irq *irq = NULL;
	unsigned int status = 0;
	unsigned int status_mcu = 0;
	unsigned int mcu_en = 0;
	int ret = 0;
	int i = 0;

	/* get irq that is sent to MCU */
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &mcu_en);

	ret = regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &status);
	/* only care IRQ which is sent to MCU */
	status_mcu = status & mcu_en & AFE_IRQ_STATUS_BITS;

	if (ret || status_mcu == 0) {
		dev_err(afe->dev, "%s(), irq status err, ret %d, status 0x%x, mcu_en 0x%x\n",
			__func__, ret, status, mcu_en);

		goto err_irq;
	}

	for (i = 0; i < MT6768_MEMIF_NUM; i++) {
		struct mtk_base_afe_memif *memif = &afe->memif[i];

		if (!memif->substream)
			continue;

		if (memif->irq_usage < 0)
			continue;

		irq = &afe->irqs[memif->irq_usage];

		if (status_mcu & (1 << irq->irq_data->irq_en_shift))
			snd_pcm_period_elapsed(memif->substream);
	}

err_irq:
	/* clear irq */
	regmap_write(afe->regmap,
		     AFE_IRQ_MCU_CLR,
		     status_mcu);

	return IRQ_HANDLED;
}
#endif

static int mt6768_afe_runtime_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	unsigned int value = 0;
	int ret;

	dev_info(afe->dev, "%s()\n", __func__);

	if (!afe->regmap)
		goto skip_regmap;

	/* disable AFE */
	regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x0);

	ret = regmap_read_poll_timeout(afe->regmap,
				       AFE_DAC_MON,
				       value,
				       (value & AFE_ON_RETM_MASK_SFT) == 0,
				       20,
				       1 * 1000 * 1000);
	if (ret)
		dev_warn(afe->dev, "%s(), ret %d\n", __func__, ret);

	/* make sure all irq status are cleared */
	regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CLR, 0xffff, 0xffff);
	regmap_update_bits(afe->regmap, AFE_IRQ_MCU_CLR, 0xffff, 0xffff);

	/* reset sgen */
	regmap_write(afe->regmap, AFE_SINEGEN_CON0, 0x0);
	regmap_update_bits(afe->regmap, AFE_SINEGEN_CON2,
			   INNER_LOOP_BACK_MODE_MASK_SFT,
			   0x3f << INNER_LOOP_BACK_MODE_SFT);

	/* cache only */
	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

skip_regmap:
	mt6768_afe_disable_clock(afe);
	return 0;
}

static int mt6768_afe_runtime_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	int ret;

	dev_info(afe->dev, "%s()\n", __func__);

	ret = mt6768_afe_enable_clock(afe);
	if (ret)
		return ret;

	if (!afe->regmap)
		goto skip_regmap;

	regcache_cache_only(afe->regmap, false);
	regcache_sync(afe->regmap);

	/* enable audio sys DCM for power saving */
	regmap_update_bits(afe->regmap, AUDIO_TOP_CON0, 0x1 << 29, 0x1 << 29);

	/* force cpu use 8_24 format when writing 32bit data */
	regmap_update_bits(afe->regmap, AFE_MEMIF_MSB,
			   1 << 28, 0 << 28);

	/* set all output port to 24bit */
	regmap_write(afe->regmap, AFE_CONN_24BIT, 0xffffffff);
	regmap_write(afe->regmap, AFE_CONN_24BIT_1, 0xffffffff);

	/* enable AFE */
	regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x1);

skip_regmap:
	return 0;
}

static int mt6768_set_sram_mode(struct device *dev,
				enum mtk_audio_sram_mode sram_mode)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);

	if (sram_mode == MTK_AUDIO_SRAM_COMPACT_MODE) {
		/* all memif use compact mode */
		regmap_update_bits(afe->regmap, AFE_MEMIF_HDALIGN,
				   0x7fff << 16, 0x0);
		/* cpu use compact mode when access sram data */
		regmap_update_bits(afe->regmap, AFE_MEMIF_MSB,
				   1 << 29, 1 << 29);
	} else {
		/* all memif use normal mode */
		regmap_update_bits(afe->regmap, AFE_MEMIF_HDALIGN,
				   0x7fff << 16, 0x7fff << 16);
		/* cpu use normal mode when access sram data */
		regmap_update_bits(afe->regmap, AFE_MEMIF_MSB,
				   1 << 29, 0x0);
	}
	return 0;
}

static const struct mtk_audio_sram_ops mt6768_sram_ops = {
	.set_sram_mode = mt6768_set_sram_mode,
};

static int mt6768_afe_pcm_platform_probe(struct snd_soc_component *platform)
{
	mtk_afe_add_sub_dai_control(platform);
	mt6768_add_misc_control(platform);
	return 0;
}

const struct snd_soc_component_driver mt6768_afe_component = {
	.name           = AFE_PCM_NAME,
	.open           = mtk_afe_pcm_open,
	.pcm_construct  = mtk_afe_pcm_new,
	.pcm_destruct   = mtk_afe_pcm_free,
	.pointer        = mtk_afe_pcm_pointer,
	.copy_user      = mtk_afe_pcm_copy_user,
	.probe          = mt6768_afe_pcm_platform_probe,
};

#if IS_ENABLED(CONFIG_DEBUG_FS)
static ssize_t mt6768_debugfs_read(struct file *file, char __user *buf,
				    size_t count, loff_t *pos)
{
	struct mtk_base_afe *afe = file->private_data;
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	const int size = 12288;
	char *buffer = NULL; /* for reduce kernel stack */
	int n = 0;
	int ret = 0;
	unsigned int value;
	int i;

	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	n += scnprintf(buffer + n, size - n,
		       "mtkaif_calibration_ok %d\n",
		       afe_priv->mtkaif_calibration_ok);

	n += scnprintf(buffer + n, size - n,
		       "mtkaif calibration phase %d, %d, %d, %d\n",
		       afe_priv->mtkaif_chosen_phase[0],
		       afe_priv->mtkaif_chosen_phase[1],
		       afe_priv->mtkaif_chosen_phase[2],
		       afe_priv->mtkaif_chosen_phase[3]);

	n += scnprintf(buffer + n, size - n,
		       "mtkaif calibration cycle %d, %d, %d, %d\n",
		       afe_priv->mtkaif_phase_cycle[0],
		       afe_priv->mtkaif_phase_cycle[1],
		       afe_priv->mtkaif_phase_cycle[2],
		       afe_priv->mtkaif_phase_cycle[3]);

	for (i = 0; i < afe->memif_size; i++) {
		n += scnprintf(buffer + n, size - n,
			       "memif[%d], irq_usage %d\n",
			       i, afe->memif[i].irq_usage);
	}

	regmap_read(afe_priv->topckgen, CLK_CFG_4, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_CFG_4 = 0x%x\n",
		       value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_0, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_0 = 0x%x\n",
		       value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_1, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_1 = 0x%x\n",
		       value);
	regmap_read(afe_priv->topckgen, CLK_AUDDIV_2, &value);
	n += scnprintf(buffer + n, size - n,
		       "CLK_AUDDIV_2 = 0x%x\n",
		       value);

	regmap_read(afe_priv->apmixed, AP_PLL_CON3, &value);
	n += scnprintf(buffer + n, size - n,
		       "AP_PLL_CON3 = 0x%x\n",
		       value);
	regmap_read(afe_priv->apmixed, APLL1_TUNER_CON0, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_TUNER_CON0 = 0x%x\n",
		       value);
	regmap_read(afe_priv->apmixed, APLL1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_CON1 = 0x%x\n",
		       value);
	regmap_read(afe_priv->apmixed, APLL1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
		       "APLL1_CON2 = 0x%x\n",
		       value);

	regmap_read(afe->regmap, AUDIO_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AUDIO_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AUDIO_TOP_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			"AUDIO_TOP_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DAC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DAC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DAC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DAC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_I2S_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN3, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN4, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_I2S_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_I2S_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_BASE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_END, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_I2S_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_I2S_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_END, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN5, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN_24BIT = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_BASE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_END, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_BASE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_END, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN6, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON3, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON4, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON5, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON6, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON7, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON8, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON9, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_DL_SRC2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_DL_SRC2_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_UL_SRC_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_UL_SRC_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_DL_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_UL_DL_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_SRC_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_SRC_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_SRC_DEBUG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_SRC_DEBUG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_SRC_DEBUG_MON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_SRC_DEBUG_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_UL_SRC_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_UL_SRC_MON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_UL_SRC_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_DEBUG, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_SIDETONE_DEBUG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_SIDETONE_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_SINEGEN_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_SIDETONE_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_COEFF, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_SIDETONE_COEFF = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_SIDETONE_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SIDETONE_GAIN, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_SIDETONE_GAIN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SINEGEN_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_SINEGEN_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_TOP_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_TOP_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_BUS_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_BUS_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_BUS_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_BUS_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_PREDIS_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_PREDIS_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MRGIF_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MRGIF_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MRGIF_MON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MRGIF_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MRGIF_MON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MRGIF_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_02_01, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_IIR_COEF_02_01 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_04_03, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_IIR_COEF_04_03 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_06_05, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_IIR_COEF_06_05 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_08_07, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_IIR_COEF_08_07 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_IIR_COEF_10_09, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_IIR_COEF_10_09 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DAC_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DAC_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DAC_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DAC_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_END, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CNT0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT6, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CNT6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_EN1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ0_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ0_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ6_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ6_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_BASE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MOD_DAI_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_END, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MOD_DAI_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MOD_DAI_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_D2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_D2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_END, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_D2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_D2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_END, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_D2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_D2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_BASE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL3_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL3_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_END, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL3_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ3_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ3_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ4_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ4_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_STATUS, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_STATUS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CLR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CLR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CNT1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CNT2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_EN, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_EN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_MON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_MON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT5, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CNT5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ1_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ1_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ2_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ2_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ1_MCU_EN_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ1_MCU_EN_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ5_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ5_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MINLEN, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MINLEN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MAXLEN, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MAXLEN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_PBUF_SIZE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_PBUF_SIZE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT7, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CNT7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ7_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ7_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT3, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CNT3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT4, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CNT4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT11, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CNT11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_APLL_TUNER_CFG, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_APLL_TUNER_CFG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_HD_MODE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_HD_MODE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_HDALIGN, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_HDALIGN = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN33, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN33 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ_MCU_CNT12, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ_MCU_CNT12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GAIN1_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GAIN1_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GAIN1_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GAIN1_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN1_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GAIN1_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GAIN2_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GAIN2_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GAIN2_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GAIN2_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GAIN2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GAIN2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN9, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN10, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN12, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN13, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN14, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN15, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN16, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN16 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN17, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN18, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN18 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN21, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN22, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN22 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN23, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN24, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN24 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN_RS = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN_DI = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN25, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN25 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN28, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN28 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN29, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN29 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN32, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN32 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SRAM_DELSEL_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_SRAM_DELSEL_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SRAM_DELSEL_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_SRAM_DELSEL_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_SRAM_DELSEL_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_SRAM_DELSEL_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON12, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON13, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON13 = 0x%x\n", value);
	regmap_read(afe->regmap, PCM2_INTF_CON, &value);
	n += scnprintf(buffer + n, size - n,
			"PCM2_INTF_CON = 0x%x\n", value);
	regmap_read(afe->regmap, FPGA_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			"FPGA_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, FPGA_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			"FPGA_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, FPGA_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
			"FPGA_CFG2 = 0x%x\n", value);
	regmap_read(afe->regmap, FPGA_CFG3, &value);
	n += scnprintf(buffer + n, size - n,
			"FPGA_CFG3 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_DBG_CON, &value);
	n += scnprintf(buffer + n, size - n,
			"AUDIO_TOP_DBG_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_DBG_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AUDIO_TOP_DBG_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AUDIO_TOP_DBG_MON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AUDIO_TOP_DBG_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ8_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ8_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ11_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ11_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_IRQ12_MCU_CNT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_IRQ12_MCU_CNT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG3, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG4, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG5, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG6, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG7, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG8, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG9, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG10, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG11, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG11 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG12, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG13, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG14, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_GENERAL_REG15, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_GENERAL_REG15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CBIP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CBIP_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_MUX_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CBIP_SLV_MUX_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CBIP_SLV_DECODER_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CBIP_SLV_DECODER_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN0_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN0_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN1_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN1_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN3_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN3_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN4_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN4_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN5_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN5_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN6_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN6_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN9_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN9_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN10_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN10_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN12_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN12_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN13_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN13_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN14_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN14_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN15_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN15_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN16_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN16_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN17_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN17_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN18_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN18_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN21_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN21_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN22_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN22_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN23_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN23_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN24_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN24_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN25_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN25_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN28_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN28_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN29_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN29_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN32_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN32_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN33_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN33_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_RS_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN_RS_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_DI_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN_DI_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_24BIT_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN_24BIT_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN_REG, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN_REG = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN38, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN38 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN38_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN38_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN39, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN39 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONN39_1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONN39_1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MOD_DAI_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MOD_DAI_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MOD_DAI_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MOD_DAI_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_D2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_D2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL1_D2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL1_D2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_D2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_D2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_VUL_D2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_VUL_D2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL3_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL3_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_DL3_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_DL3_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_BASE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB2_BASE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_END, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB2_END = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_CUR, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB2_CUR = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_BASE_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB2_BASE_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_END_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB2_END_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AWB2_CUR_MSB, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AWB2_CUR_MSB = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_DCCOMP_CON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_DL_SDM_DCCOMP_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_TEST, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_DL_SDM_TEST = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_DC_COMP_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_DL_DC_COMP_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_DC_COMP_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_DL_DC_COMP_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_FIFO_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_DL_SDM_FIFO_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_LCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_DL_SRC_LCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SRC_RCH_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_DL_SRC_RCH_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_DL_SDM_OUT_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_DL_SDM_OUT_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_CON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONNSYS_I2S_CON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_CONNSYS_I2S_MON, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_CONNSYS_I2S_MON = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON4, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON4 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON5, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON5 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON6, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON6 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON7, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON7 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON8, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON8 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON9, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON9 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ASRC_2CH_CON10, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ASRC_2CH_CON10 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_PREDIS_CON2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_PREDIS_CON3, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_PREDIS_CON3 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON12, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON12 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON13, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON13 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON14, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON14 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON15, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON15 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON16, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON16 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON17, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON17 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON18, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON18 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON19, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON19 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON21, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON21 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON23, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON23 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_MEMIF_MON24, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_MEMIF_MON24 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_HD_ENGEN_ENABLE, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_HD_ENGEN_ENABLE = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_MTKAIF_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_TX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_MTKAIF_TX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_RX_CFG0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_MTKAIF_RX_CFG0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_RX_CFG1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_MTKAIF_RX_CFG1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_RX_CFG2, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_MTKAIF_RX_CFG2 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_MON0, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_MTKAIF_MON0 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_ADDA_MTKAIF_MON1, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_ADDA_MTKAIF_MON1 = 0x%x\n", value);
	regmap_read(afe->regmap, AFE_AUD_PAD_TOP, &value);
	n += scnprintf(buffer + n, size - n,
			"AFE_AUD_PAD_TOP = 0x%x\n", value);

	ret = simple_read_from_buffer(buf, count, pos, buffer, n);
	kfree(buffer);
	return ret;
}

static const struct mtk_afe_debug_cmd mt6768_debug_cmds[] = {
	MTK_AFE_DBG_CMD("write_reg", mtk_afe_debug_write_reg),
	{}
};

static const struct file_operations mt6768_debugfs_ops = {
	.open = mtk_afe_debugfs_open,
	.write = mtk_afe_debugfs_write,
	.read = mt6768_debugfs_read,
};
#endif

static const struct snd_soc_component_driver mt6768_afe_pcm_component = {
	.name = "mt6768-afe-pcm-dai",
};

static int mt6768_dai_memif_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mt6768_memif_dai_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mt6768_memif_dai_driver);

	dai->controls = mt6768_pcm_kcontrols;
	dai->num_controls = ARRAY_SIZE(mt6768_pcm_kcontrols);
	dai->dapm_widgets = mt6768_memif_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mt6768_memif_widgets);
	dai->dapm_routes = mt6768_memif_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mt6768_memif_routes);
	return 0;
}

typedef int (*dai_register_cb)(struct mtk_base_afe *);
static const dai_register_cb dai_register_cbs[] = {
	mt6768_dai_adda_register,
	mt6768_dai_i2s_register,
	mt6768_dai_hw_gain_register,
	mt6768_dai_pcm_register,
	mt6768_dai_hostless_register,
	mt6768_dai_memif_register,
};

static int mt6768_afe_pcm_dev_probe(struct platform_device *pdev)
{
	int ret, i;
#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	int irq_id = 0;
#endif
	struct mtk_base_afe *afe = NULL;
	struct mt6768_afe_private *afe_priv = NULL;
	struct resource *res = NULL;
	struct device *dev = NULL;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));
	if (ret)
		return ret;

	afe = devm_kzalloc(&pdev->dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;
	platform_set_drvdata(pdev, afe);
	mt6768_set_local_afe(afe);

	afe->platform_priv = devm_kzalloc(&pdev->dev, sizeof(*afe_priv),
					  GFP_KERNEL);
	if (!afe->platform_priv)
		return -ENOMEM;
	afe_priv = afe->platform_priv;

	afe->dev = &pdev->dev;
	dev = afe->dev;

	/* init audio related clock */
	ret = mt6768_init_clock(afe);
	if (ret) {
		dev_err(dev, "init clock error\n");
		return ret;
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev))
		goto err_pm_disable;

	/* regmap init */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	afe->base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(afe->base_addr))
		return PTR_ERR(afe->base_addr);

	/* enable clock for regcache get default value from hw */
	pm_runtime_get_sync(&pdev->dev);

	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->base_addr,
					    &mt6768_afe_regmap_config);
	if (IS_ERR(afe->regmap))
		return PTR_ERR(afe->regmap);

	pm_runtime_put_sync(&pdev->dev);

	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

	/* init gpio */
	ret = mt6768_afe_gpio_init(afe);
	if (ret)
		dev_err(dev, "init gpio error\n");

	/* init sram */
	afe->sram = devm_kzalloc(&pdev->dev, sizeof(struct mtk_audio_sram),
				GFP_KERNEL);
	if (!afe->sram)
		return -ENOMEM;

	ret = mtk_audio_sram_init(dev, afe->sram, &mt6768_sram_ops);
	if (ret) {
		dev_err(dev, "%s(), mtk_audio_sram_init failed\n",
				__func__);
		return ret;
	}

	/* init memif */
	afe->memif_32bit_supported = 0;
	afe->memif_size = MT6768_MEMIF_NUM;
	afe->memif = devm_kcalloc(dev, afe->memif_size, sizeof(*afe->memif),
				  GFP_KERNEL);

	if (!afe->memif)
		return -ENOMEM;

	for (i = 0; i < afe->memif_size; i++) {
		afe->memif[i].data = &memif_data[i];
		afe->memif[i].irq_usage = -1;
	}
	afe->memif[MT6768_DEEP_MEMIF].ack = mtk_sp_clean_written_buffer_ack;

	mutex_init(&afe->irq_alloc_lock);

	/* init irq */
	afe->irqs_size = MT6768_IRQ_NUM;
	afe->irqs = devm_kcalloc(dev, afe->irqs_size, sizeof(*afe->irqs),
				 GFP_KERNEL);

	if (!afe->irqs)
		return -ENOMEM;

	for (i = 0; i < afe->irqs_size; i++)
		afe->irqs[i].irq_data = &irq_data[i];

#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	/* request irq */
	irq_id = platform_get_irq(pdev, 0);
	if (irq_id <= 0) {
		dev_err(dev, "%pOFn no irq found\n", dev->of_node);
		return irq_id < 0 ? irq_id : -ENXIO;
	}
	ret = devm_request_irq(dev, irq_id, mt6768_afe_irq_handler,
			       IRQF_TRIGGER_NONE, "Afe_ISR_Handle", (void *)afe);
	if (ret) {
		dev_err(dev, "could not request_irq for Afe_ISR_Handle\n");
		return ret;
	}
#endif

	/* init sub_dais */
	INIT_LIST_HEAD(&afe->sub_dais);

	for (i = 0; i < ARRAY_SIZE(dai_register_cbs); i++) {
		ret = dai_register_cbs[i](afe);
		if (ret) {
			dev_warn(afe->dev, "dai register i %d fail, ret %d\n",
				 i, ret);
			goto err_pm_disable;
		}
	}

	/* init dai_driver and component_driver */
	ret = mtk_afe_combine_sub_dai(afe);
	if (ret) {
		dev_warn(afe->dev, "mtk_afe_combine_sub_dai fail, ret %d\n",
			 ret);
		goto err_pm_disable;
	}

	/* others */
	afe->mtk_afe_hardware = &mt6768_afe_hardware;
	afe->memif_fs = mt6768_memif_fs;
	afe->irq_fs = mt6768_irq_fs;
	afe->get_dai_fs = mt6768_get_dai_fs;
	afe->get_memif_pbuf_size = mt6768_get_memif_pbuf_size;

	afe->runtime_resume = mt6768_afe_runtime_resume;
	afe->runtime_suspend = mt6768_afe_runtime_suspend;

	afe->request_dram_resource = mt6768_afe_dram_request;
	afe->release_dram_resource = mt6768_afe_dram_release;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	/* debugfs */
	afe->debug_cmds = mt6768_debug_cmds;
	afe->debugfs = debugfs_create_file("mtksocaudio",
					   S_IFREG | 0444, NULL,
					   afe, &mt6768_debugfs_ops);
#endif
	/* register platform */
	ret = devm_snd_soc_register_component(&pdev->dev,
					     &mt6768_afe_component, NULL, 0);
	if (ret) {
		dev_warn(dev, "err_platform\n");
		goto err_pm_disable;
	}

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &mt6768_afe_pcm_component,
					      afe->dai_drivers,
					      afe->num_dai_drivers);
	if (ret) {
		dev_warn(dev, "err_dai_component\n");
		goto err_dai_component;
	}

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP) ||\
	IS_ENABLED(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
	audio_set_dsp_afe(afe);
#endif

	dev_info(dev, "%s(), return 0\n", __func__);
	return 0;

err_dai_component:
	snd_soc_unregister_component(&pdev->dev);


err_pm_disable:
	pm_runtime_disable(&pdev->dev);

	dev_info(dev, "%s(), return %d\n", __func__, ret);
	return ret;
}

static int mt6768_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mtk_base_afe *afe = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		mt6768_afe_runtime_suspend(&pdev->dev);

	/* disable afe clock */
	mt6768_afe_disable_clock(afe);
	return 0;
}

static const struct of_device_id mt6768_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt6768-sound", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6768_afe_pcm_dt_match);

static int mt6768_afe_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);

	dev_info(afe->dev, "%s()\n", __func__);

	mt6768_afe_suspend_clock(afe);

	return 0;
}

static int mt6768_afe_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);

	dev_info(afe->dev, "%s()\n", __func__);

	mt6768_afe_resume_clock(afe);

	return 0;
}

static const struct dev_pm_ops mt6768_afe_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mt6768_afe_suspend,
				mt6768_afe_resume)
	SET_RUNTIME_PM_OPS(mt6768_afe_runtime_suspend,
			   mt6768_afe_runtime_resume, NULL)
};

static struct platform_driver mt6768_afe_pcm_driver = {
	.driver = {
		   .name = "mt6768-audio",
		   .of_match_table = mt6768_afe_pcm_dt_match,
#if IS_ENABLED(CONFIG_PM)
		   .pm = &mt6768_afe_pm_ops,
#endif
	},
	.probe = mt6768_afe_pcm_dev_probe,
	.remove = mt6768_afe_pcm_dev_remove,
};

module_platform_driver(mt6768_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC AFE platform driver for 6768");
MODULE_AUTHOR("Michael Hsiao <michael.hsiao@mediatek.com>");
MODULE_LICENSE("GPL v2");
