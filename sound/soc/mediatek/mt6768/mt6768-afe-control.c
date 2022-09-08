// SPDX-License-Identifier: GPL-2.0
//
// MediaTek ALSA SoC Audio Control
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Michael Hsiao <michael.hsiao@mediatek.com>

#include "mt6768-afe-common.h"
#include <linux/pm_runtime.h>

#include "../common/mtk-sram-manager.h"

#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
#include "../audio_dsp/mtk-dsp-common.h"
#endif

/* don't use this directly if not necessary */
static struct mtk_base_afe *local_afe;

int mt6768_set_local_afe(struct mtk_base_afe *afe)
{
	local_afe = afe;
	return 0;
}

enum {
	MTK_AFE_RATE_130K = 7,
	MTK_AFE_RATE_174K = 13,
};

unsigned int mt6768_general_rate_transform(struct device *dev,
					   unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_RATE_8K;
	case 11025:
		return MTK_AFE_RATE_11K;
	case 12000:
		return MTK_AFE_RATE_12K;
	case 16000:
		return MTK_AFE_RATE_16K;
	case 22050:
		return MTK_AFE_RATE_22K;
	case 24000:
		return MTK_AFE_RATE_24K;
	case 32000:
		return MTK_AFE_RATE_32K;
	case 44100:
		return MTK_AFE_RATE_44K;
	case 48000:
		return MTK_AFE_RATE_48K;
	case 88200:
		return MTK_AFE_RATE_88K;
	case 96000:
		return MTK_AFE_RATE_96K;
	case 130000:
		return MTK_AFE_RATE_130K;
	case 176400:
		return MTK_AFE_RATE_174K;
	case 192000:
		return MTK_AFE_RATE_192K;
	case 260000:
		return MTK_AFE_RATE_260K;
	default:
		dev_warn(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_RATE_48K);
		return MTK_AFE_RATE_48K;
	}
}

static unsigned int dai_memif_rate_transform(struct device *dev,
					     unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_DAI_MEMIF_RATE_8K;
	case 16000:
		return MTK_AFE_DAI_MEMIF_RATE_16K;
	case 32000:
		return MTK_AFE_DAI_MEMIF_RATE_32K;
	case 48000:
		return MTK_AFE_DAI_MEMIF_RATE_48K;
	default:
		dev_warn(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_DAI_MEMIF_RATE_16K);
		return MTK_AFE_DAI_MEMIF_RATE_16K;
	}
}

static unsigned int pcm_rate_transform(struct device *dev,
				       unsigned int rate)
{
	switch (rate) {
	case 8000:
		return MTK_AFE_PCM_RATE_8K;
	case 16000:
		return MTK_AFE_PCM_RATE_16K;
	case 32000:
		return MTK_AFE_PCM_RATE_32K;
	case 48000:
		return MTK_AFE_PCM_RATE_48K;
	default:
		dev_warn(dev, "%s(), rate %u invalid, use %d!!!\n",
			 __func__,
			 rate, MTK_AFE_PCM_RATE_32K);
		return MTK_AFE_PCM_RATE_32K;
	}
}

unsigned int mt6768_rate_transform(struct device *dev,
				   unsigned int rate, int aud_blk)
{
	switch (aud_blk) {
	case MT6768_MEMIF_MOD_DAI:
		return dai_memif_rate_transform(dev, rate);
	case MT6768_DAI_PCM_2:
		return pcm_rate_transform(dev, rate);
	default:
		return mt6768_general_rate_transform(dev, rate);
	}
}

int mt6768_enable_dc_compensation(bool enable)
{
	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_warn(local_afe->dev, "%s(), status suspended\n", __func__);


	pm_runtime_get_sync(local_afe->dev);
	regmap_update_bits(local_afe->regmap,
			   AFE_ADDA_DL_SDM_DCCOMP_CON,
			   AUD_DC_COMP_EN_MASK_SFT,
			   (enable ? 1 : 0) << AUD_DC_COMP_EN_SFT);
	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6768_enable_dc_compensation);

int mt6768_set_lch_dc_compensation(int value)
{
	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_warn(local_afe->dev, "%s(), status suspended\n", __func__);

	pm_runtime_get_sync(local_afe->dev);
	regmap_write(local_afe->regmap,
		     AFE_ADDA_DL_DC_COMP_CFG0,
		     value);
	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6768_set_lch_dc_compensation);

int mt6768_set_rch_dc_compensation(int value)
{
	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_warn(local_afe->dev, "%s(), status suspended\n", __func__);

	pm_runtime_get_sync(local_afe->dev);
	regmap_write(local_afe->regmap,
		     AFE_ADDA_DL_DC_COMP_CFG1,
		     value);
	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6768_set_rch_dc_compensation);

int mt6768_adda_dl_gain_control(bool mute)
{
	unsigned int dl_2_gain_ctl;

	if (!local_afe)
		return -EPERM;

	if (pm_runtime_status_suspended(local_afe->dev))
		dev_warn(local_afe->dev, "%s(), status suspended\n", __func__);

	pm_runtime_get_sync(local_afe->dev);

	if (mute)
		dl_2_gain_ctl = MTK_AFE_ADDA_DL_GAIN_MUTE;
	else
		dl_2_gain_ctl = MTK_AFE_ADDA_DL_GAIN_NORMAL;

	regmap_update_bits(local_afe->regmap,
			   AFE_ADDA_DL_SRC2_CON1,
			   DL_2_GAIN_CTL_PRE_MASK_SFT,
			   dl_2_gain_ctl << DL_2_GAIN_CTL_PRE_SFT);

	dev_info(local_afe->dev, "%s(), adda_dl_gain = 0x%x\n",
		 __func__, dl_2_gain_ctl);

	pm_runtime_put(local_afe->dev);
	return 0;
}
EXPORT_SYMBOL(mt6768_adda_dl_gain_control);

/* api for other modules */
static int request_sram_count;
int mtk_audio_request_sram(dma_addr_t *phys_addr,
			   unsigned char **virt_addr,
			   unsigned int length,
			   void *user)
{
	int ret;

	dev_info(local_afe->dev, "%s(), user = %p, length = %d, count = %d\n",
		 __func__, user, length, request_sram_count);

	pm_runtime_get_sync(local_afe->dev);

	ret = mtk_audio_sram_allocate(local_afe->sram, phys_addr, virt_addr,
				      length, user,
				      SNDRV_PCM_FORMAT_S16_LE, true);
	if (ret) {
		dev_warn(local_afe->dev, "%s(), allocate sram fail, ret %d\n",
			 __func__, ret);
		pm_runtime_put(local_afe->dev);
		return ret;
	}

	request_sram_count++;


	dev_info(local_afe->dev, "%s(), return 0, count = %d\n",
		 __func__, request_sram_count);
	return 0;
}
EXPORT_SYMBOL(mtk_audio_request_sram);

void mtk_audio_free_sram(void *user)
{
	dev_info(local_afe->dev, "%s(), user = %p, count = %d\n",
		 __func__, user, request_sram_count);

	mtk_audio_sram_free(local_afe->sram, user);
	pm_runtime_put(local_afe->dev);
	request_sram_count--;

	dev_info(local_afe->dev, "%s(), return, count = %d\n",
		 __func__, request_sram_count);
}
EXPORT_SYMBOL(mtk_audio_free_sram);

bool mtk_audio_condition_enter_suspend(void)
{
	struct mt6768_afe_private *afe_priv = local_afe->platform_priv;

	if (afe_priv->dai_on[MT6768_DAI_CONNSYS_I2S])
		return false;

#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
	if (is_adsp_feature_registered() || is_adsp_core_ready())
		return false;
#endif
	return true;
}
EXPORT_SYMBOL(mtk_audio_condition_enter_suspend);

bool mtk_get_speech_status(void)
{
	int speech_en = 0;

	regmap_read(local_afe->regmap,
		    PCM2_INTF_CON, &speech_en);

	return (speech_en & PCM_EN_MASK_SFT) ? true : false;
}
EXPORT_SYMBOL(mtk_get_speech_status);
