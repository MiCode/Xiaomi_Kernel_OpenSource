// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) 2019 Cirrus Logic Inc.
 *
 */


/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mtk-cirrus-machine-ops.c
 *
 * Project:
 * --------
 *   Audio soc machine vendor ops
 *
 * Description:
 * ------------
 *   Audio machine driver
 *
 * Author:
 * -------
 * Vlad Karpovich
 *
 *------------------------------------------------------------------------------
 *
 *******************************************************************************/

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/atomic.h>

static atomic_t cs35l41_mclk_rsc_ref;


int cs35l41_snd_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
		//struct snd_soc_codec *spk_cdc = rtd->codec_dais[0]->codec;
		//struct snd_soc_dapm_context *spk_dapm = snd_soc_codec_get_dapm(spk_cdc);
		//struct snd_soc_codec *rcv_cdc = rtd->codec_dais[1]->codec;
		//struct snd_soc_dapm_context *rcv_dapm = snd_soc_codec_get_dapm(rcv_cdc);

	//struct snd_soc_codec *spk1_cdc = rtd->codec_dais[0]->codec;
	//struct snd_soc_dapm_context *spk1_dapm = snd_soc_codec_get_dapm(spk1_cdc);
	//struct snd_soc_codec *spk2_cdc = rtd->codec_dais[1]->codec;
	//struct snd_soc_dapm_context *spk2_dapm = snd_soc_codec_get_dapm(spk2_cdc);

	//struct snd_soc_codec *spk3_cdc = rtd->codec_dais[2]->codec;
	//struct snd_soc_dapm_context *spk3_dapm = snd_soc_codec_get_dapm(spk3_cdc);
	//struct snd_soc_codec *spk4_cdc = rtd->codec_dais[3]->codec;
	//struct snd_soc_dapm_context *spk4_dapm = snd_soc_codec_get_dapm(spk4_cdc);


	//dev_info(card->dev, "%s: found codec[%s]\n", __func__, dev_name(spk1_cdc->dev));
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK1 AMP Playback");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK1 SPK");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK1 VMON ADC");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK1 AMP Capture");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK1 AMP Enable Switch");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK1 ASPRX1");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK1 ASPRX2");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK1 ASPRX3");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK1 ASPRX4");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK1 PCM Source");
	//snd_soc_dapm_sync(spk1_dapm);

	//dev_info(card->dev, "%s: found codec[%s]\n", __func__, dev_name(spk2_cdc->dev));
	//snd_soc_dapm_ignore_suspend(spk2_dapm, "SPK2 AMP Playback");
	//snd_soc_dapm_ignore_suspend(spk2_dapm, "SPK2 SPK");
	//snd_soc_dapm_ignore_suspend(spk2_dapm, "SPK2 VMON ADC");
	//snd_soc_dapm_ignore_suspend(spk2_dapm, "SPK2 AMP Capture");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK2 AMP Enable Switch");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK2 ASPRX1");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK2 ASPRX2");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK2 ASPRX3");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK2 ASPRX4");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK2 PCM Source");
	//snd_soc_dapm_sync(spk2_dapm);

	//dev_info(card->dev, "%s: found codec[%s]\n", __func__, dev_name(spk3_cdc->dev));
	//snd_soc_dapm_ignore_suspend(spk3_dapm, "SPK3 AMP Playback");
	//snd_soc_dapm_ignore_suspend(spk3_dapm, "SPK3 SPK");
	//snd_soc_dapm_ignore_suspend(spk3_dapm, "SPK3 VMON ADC");
	//snd_soc_dapm_ignore_suspend(spk3_dapm, "SPK3 AMP Capture");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK3 AMP Enable Switch");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK3 ASPRX1");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK3 ASPRX2");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK3 ASPRX3");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK3 ASPRX4");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK3 PCM Source");
	//snd_soc_dapm_sync(spk3_dapm);

	//dev_info(card->dev, "%s: found codec[%s]\n", __func__, dev_name(spk4_cdc->dev));
	//snd_soc_dapm_ignore_suspend(spk4_dapm, "SPK4 AMP Playback");
	//snd_soc_dapm_ignore_suspend(spk4_dapm, "SPK4 SPK");
	//snd_soc_dapm_ignore_suspend(spk4_dapm, "SPK4 VMON ADC");
	//snd_soc_dapm_ignore_suspend(spk4_dapm, "SPK4 AMP Capture");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK4 AMP Enable Switch");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK4 ASPRX1");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK4 ASPRX2");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK4 ASPRX3");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK4 ASPRX4");
	//snd_soc_dapm_ignore_suspend(spk1_dapm, "SPK4 PCM Source");
	//snd_soc_dapm_sync(spk4_dapm);

	dev_info(card->dev, "%s: set cs35l43_mclk_rsc_ref to 0\n", __func__);
	atomic_set(&cs35l41_mclk_rsc_ref, 0);

	return 0;
}
static int cirrus_amp_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static int cirrus_amp_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai **codec_dais = rtd->codec_dais;
	int ret=0, i;
	unsigned int clk_freq,rate;
	//unsigned int slot_mask;
	int slot_width = 32;
	int slots = 4;
	u8 asp_width;

	asp_width = params_physical_width(params);
	pr_info("%s\n cs35l43 width=%d", __func__, asp_width);
	if (asp_width == 16)
		slot_width = asp_width;

	dev_info(card->dev, "+%s,cs35l43 mclk refcount = %d\n", __func__,
		atomic_read(&cs35l41_mclk_rsc_ref));

	if (atomic_inc_return(&cs35l41_mclk_rsc_ref) == 1) {

		rate = params_rate(params);
		clk_freq = rate * slot_width * slots;

		for (i = 0; i < rtd->num_codecs; i++) {
		//2 slot config - bits 0 and 1 set for the first two slots
		//	slot_mask = 0x0000FFFF >> (16 - slots);

		//	ret = snd_soc_dai_set_tdm_slot(codec_dais[i],
		//				0, slot_mask,
		//				slots, slot_width);
		//	if (ret < 0) {
		//		dev_info(card->dev,"%s: cs35l43 failed to set tdm rx slot,
		//		err:%d\n", __func__, ret);
		//		return ret;
		//	}

			ret = snd_soc_dai_set_fmt(codec_dais[i],
						SND_SOC_DAIFMT_DSP_A |
						SND_SOC_DAIFMT_CBS_CFS |
						SND_SOC_DAIFMT_NB_NF);
			if (ret != 0) {
				dev_info(card->dev, "%s: cs35l43 Failed to set %s's fmt: ret = %d\n",
					codec_dais[i]->name, ret);
				return ret;
			}

			ret = snd_soc_component_set_sysclk(codec_dais[i]->component,
				0, 0,clk_freq,SND_SOC_CLOCK_IN);
			if (ret < 0){
				dev_info(card->dev," %s: cs35l43 set sysclk failed, err:%d\n",
					codec_dais[i]->name, ret);
				return ret ;
			}
		}
	}

	return ret;
}

void cirrus_amp_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;

	dev_info(card->dev, "+%s, mclk refcount = %d\n", __func__,
		atomic_read(&cs35l41_mclk_rsc_ref));
	if (atomic_read(&cs35l41_mclk_rsc_ref) > 0) {
		if (atomic_dec_return(&cs35l41_mclk_rsc_ref) == 0)
			dev_info(card->dev, "-%s shutdown amp\n", __func__);
	}
}

const struct snd_soc_ops cirrus_amp_ops = {
	.startup = cirrus_amp_startup,
	.hw_params = cirrus_amp_hw_params,
	.shutdown = cirrus_amp_shutdown,
};
EXPORT_SYMBOL_GPL(cirrus_amp_ops);


