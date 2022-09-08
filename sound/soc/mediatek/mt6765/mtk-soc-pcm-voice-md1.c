// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_voice_md1.c
 *
 * Project:
 * --------
 *   voice call platform driver
 *
 * Description:
 * ------------
 *
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-analog-type.h"
#include "mtk-soc-digital-type.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include <linux/dma-mapping.h>

/*
 *    declaration
 */

struct mtk_voice_property {
	/* speech mixctrl instead property usage */
	int speech_a2m_msg_id;
	int speech_md_status;
	int speech_md_ext_status;
	int speech_mic_mute;
	int speech_dl_mute;
	int speech_ul_mute;
	int speech_phone1_md_idx;
	int speech_phone2_md_idx;
	int speech_phone_id;
	int speech_md_epof;
	int speech_bt_sco_wb;
	int speech_md_active;
};

/*
 *    function implementation
 */

static int mtk_voice_probe(struct platform_device *pdev);
static int mtk_voice_close(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream);
static int mtk_voice_component_probe(struct snd_soc_component *component);

static bool Voice_Status;

bool get_voice_status(void)
{
	return Voice_Status;
}
EXPORT_SYMBOL(get_voice_status);

static struct audio_digital_pcm Voice1Pcm = {
	.mTxLchRepeatSel = Soc_Aud_TX_LCH_RPT_TX_LCH_NO_REPEAT,
	.mVbt16kModeSel = Soc_Aud_VBT_16K_MODE_VBT_16K_MODE_DISABLE,
	.mExtModemSel = Soc_Aud_EXT_MODEM_MODEM_2_USE_INTERNAL_MODEM,
	.mExtendBckSyncLength = 0,
	.mExtendBckSyncTypeSel = Soc_Aud_PCM_SYNC_TYPE_BCK_CYCLE_SYNC,
	.mSingelMicSel = Soc_Aud_BT_MODE_DUAL_MIC_ON_TX,
	/*	.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASRC, */
	.mAsyncFifoSel =
		Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO,
	/* TODO: KC/TINA: For BringUp Setting */
	.mSlaveModeSel = Soc_Aud_PCM_CLOCK_SOURCE_SALVE_MODE,
	.mPcmWordLength = Soc_Aud_PCM_WLEN_LEN_PCM_16BIT,
	.mPcmModeWidebandSel = false,
	.mPcmFormat = Soc_Aud_PCM_FMT_PCM_MODE_B,
	.mModemPcmOn = false,
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_voice_supported_sample_rates),
	.list = soc_voice_supported_sample_rates,
	.mask = 0,
};

static struct snd_pcm_hardware mtk_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_STD_MT_FMTS,
	.rates = SOC_NORMAL_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_HIGH_USE_CHANNELS_MIN,
	.channels_max = SOC_HIGH_USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = 1,
	.periods_max = 4096,
	.fifo_size = 0,
};

static struct mtk_voice_property voice_property = {
	.speech_a2m_msg_id = 0,
	.speech_md_status = 0,
	.speech_md_ext_status = 0,
	.speech_mic_mute = 0,
	.speech_dl_mute = 0,
	.speech_ul_mute = 0,
	.speech_phone1_md_idx = 0,
	.speech_phone2_md_idx = 0,
	.speech_phone_id = 0,
	.speech_md_epof = 0,
	.speech_bt_sco_wb = 0,
	.speech_md_active = 0,
};

/* speech mixctrl instead property usage */
static void *get_sph_property_by_name(struct mtk_voice_property *voice_priv,
				      const char *name)
{
	if (strcmp(name, "Speech_A2M_Msg_ID") == 0)
		return &(voice_priv->speech_a2m_msg_id);
	else if (strcmp(name, "Speech_MD_Status") == 0)
		return &(voice_priv->speech_md_status);
	else if (strcmp(name, "Speech_MD_Ext_Status") == 0)
		return &(voice_priv->speech_md_ext_status);
	else if (strcmp(name, "Speech_Mic_Mute") == 0)
		return &(voice_priv->speech_mic_mute);
	else if (strcmp(name, "Speech_DL_Mute") == 0)
		return &(voice_priv->speech_dl_mute);
	else if (strcmp(name, "Speech_UL_Mute") == 0)
		return &(voice_priv->speech_ul_mute);
	else if (strcmp(name, "Speech_Phone1_MD_Idx") == 0)
		return &(voice_priv->speech_phone1_md_idx);
	else if (strcmp(name, "Speech_Phone2_MD_Idx") == 0)
		return &(voice_priv->speech_phone2_md_idx);
	else if (strcmp(name, "Speech_Phone_ID") == 0)
		return &(voice_priv->speech_phone_id);
	else if (strcmp(name, "Speech_MD_EPOF") == 0)
		return &(voice_priv->speech_md_epof);
	else if (strcmp(name, "Speech_BT_SCO_WB") == 0)
		return &(voice_priv->speech_bt_sco_wb);
	else if (strcmp(name, "Speech_MD_Active") == 0)
		return &(voice_priv->speech_md_active);
	else
		return NULL;
}

static int speech_property_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int *sph_property;

	sph_property = (int *)get_sph_property_by_name(&voice_property,
						       kcontrol->id.name);
	if (!sph_property) {
		pr_info("%s(), sph_property == NULL\n", __func__);
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] = *sph_property;

	return 0;
}

static int speech_property_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int *sph_property;

	sph_property = (int *)get_sph_property_by_name(&voice_property,
						       kcontrol->id.name);
	if (!sph_property) {
		pr_info("%s(), sph_property == NULL\n", __func__);
		return -EINVAL;
	}
	*sph_property = ucontrol->value.integer.value[0];

	return 0;
}

static const struct snd_kcontrol_new mtk_voice_speech_controls[] = {
	SOC_SINGLE_EXT("Speech_A2M_Msg_ID",
		       SND_SOC_NOPM, 0, 0xFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_Status",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_Ext_Status",
		       SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Mic_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_DL_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_UL_Mute",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone1_MD_Idx",
		       SND_SOC_NOPM, 0, 0x2, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone2_MD_Idx",
		       SND_SOC_NOPM, 0, 0x2, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_Phone_ID",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_EPOF",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_BT_SCO_WB",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
	SOC_SINGLE_EXT("Speech_MD_Active",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       speech_property_get, speech_property_set),
};

static int mtk_voice_pcm_open(struct snd_soc_component *component,
			      struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	AudDrv_Clk_On();

	pr_info("%s(), stream(%d)\n", __func__, substream->stream);

	runtime->hw = mtk_pcm_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0) {
		pr_err("%s(), stream(%d) snd_pcm_hw_constraint_integer failed, ret(%d)\n",
		       __func__, substream->stream, ret);
	}

	runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;

	if (ret < 0) {
		mtk_voice_close(component, substream);
		return ret;
	}
	return 0;
}

static int mtk_voice_close(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream)
{
	pr_info("%s(), stream(%d)\n", __func__, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/* 3-mic setting */
		if (substream->runtime->channels > 2) {
			SetIntfConnection(
				Soc_Aud_InterCon_DisConnect,
				Soc_Aud_AFE_IO_Block_ADDA_UL2,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O_CH3);

			SetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL2,
					    false);
			if (GetMemoryPathEnable(
				    Soc_Aud_Digital_Block_ADDA_UL2) == false)
				set_adc2_enable(false);
		}
		AudDrv_Clk_Off();
		return 0;
	}

	/* todo : enable sidetone */
	/* here start digital part */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_ADDA_UL,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

	SetI2SDacEnable(false);
	SetModemPcmEnable(MODEM_1, false);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL, false);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL) == false)
		set_adc_enable(false);

	EnableAfe(false);
	AudDrv_Clk_Off();

	Voice_Status = false;

	return 0;
}

static int mtk_voice_trigger(struct snd_soc_component *component,
			     struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}
	return 0;
}

static int mtk_voice_pcm_copy(struct snd_soc_component *component,
			      struct snd_pcm_substream *substream,
			      int channel,
			      unsigned long pos,
			      void __user *buf,
			      unsigned long bytes)
{
	return 0;
}


static void *dummy_page[2];
static struct page *mtk_pcm_page(struct snd_soc_component *component,
				 struct snd_pcm_substream *substream,
				 unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static int mtk_voice1_prepare(struct snd_soc_component *component,
			      struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtimeStream = substream->runtime;

	pr_info("%s(), stream(%d), rate = %d  channels = %d period_size = %lu\n",
		__func__, substream->stream, runtimeStream->rate,
		runtimeStream->channels, runtimeStream->period_size);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/* 3-mic setting */
		if (substream->runtime->channels > 2) {
			SetIntfConnection(
				Soc_Aud_InterCon_Connection,
				Soc_Aud_AFE_IO_Block_ADDA_UL2,
				Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O_CH3);

			if (GetMemoryPathEnable(
				    Soc_Aud_Digital_Block_ADDA_UL2) == false) {
				SetMemoryPathEnable(
					Soc_Aud_Digital_Block_ADDA_UL2, true);
				set_adc2_in(substream->runtime->rate);
				set_adc2_enable(true);
			} else {
				SetMemoryPathEnable(
					Soc_Aud_Digital_Block_ADDA_UL2, true);
			}
		}
		return 0;
	}
	/* here start digital part */
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_ADDA_UL,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O);
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

	/* start I2S DAC out */
	SetI2SDacOut(substream->runtime->rate, false,
		     Soc_Aud_I2S_WLEN_WLEN_16BITS);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
	SetI2SDacEnable(true);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL) == false) {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL, true);
		set_adc_in(substream->runtime->rate);
		set_adc_enable(true);
	} else {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL, true);
	}

	EnableAfe(true);

	Voice1Pcm.mPcmModeWidebandSel = SampleRateTransform(
		runtimeStream->rate, Soc_Aud_Digital_Block_MODEM_PCM_2_O);

	Voice1Pcm.mAsyncFifoSel = Soc_Aud_BYPASS_SRC_SLAVE_USE_ASYNC_FIFO;
	SetModemPcmConfig(MODEM_1, Voice1Pcm);
	SetModemPcmEnable(MODEM_1, true);

	Voice_Status = true;

	return 0;
}

static int mtk_pcm_hw_params(struct snd_soc_component *component,
			     struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
	return ret;
}

static int mtk_voice_hw_free(struct snd_soc_component *component,
			     struct snd_pcm_substream *substream)
{
	pr_debug("%s()\n", __func__);
	return snd_pcm_lib_free_pages(substream);
}

static struct snd_soc_component_driver mtk_soc_voice_component = {
	.name = AFE_PCM_NAME,
	.probe = mtk_voice_component_probe,
	.open = mtk_voice_pcm_open,
	.close = mtk_voice_close,
	.hw_params = mtk_pcm_hw_params,
	.hw_free = mtk_voice_hw_free,
	.prepare = mtk_voice1_prepare,
	.trigger = mtk_voice_trigger,
	.copy_user = mtk_voice_pcm_copy,
	.page = mtk_pcm_page,

};

static int mtk_voice_probe(struct platform_device *pdev)
{
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_VOICE_MD1);
	pdev->name = pdev->dev.kobj.name;

	pr_info("%s(), dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev,
					  &mtk_soc_voice_component,
					  NULL,
					  0);
}

static int mtk_voice_component_probe(struct snd_soc_component *component)
{
	pr_info("%s()\n", __func__);

	snd_soc_add_component_controls(component, mtk_voice_speech_controls,
				      ARRAY_SIZE(mtk_voice_speech_controls));

	return 0;
}

static int mtk_voice_remove(struct platform_device *pdev)
{
	pr_info("%s()\n", __func__);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

/* supend and resume function */
static int mtk_voice_pm_ops_suspend(struct device *device)
{
	/* if now in phone call state, not suspend!! */
	bool b_modem1_speech_on;
	bool b_modem2_speech_on;

	/* don't switch to 26M, 96k/192k sram is not fast enough */
	/*
	 *TODO: KC: check if we need this
	 *if (get_voice_ultra_status())
	 *	return 0;
	 */

	b_modem1_speech_on =
		GetMemoryPathEnable(Soc_Aud_Digital_Block_MODEM_PCM_1_O);
	b_modem2_speech_on =
		GetMemoryPathEnable(Soc_Aud_Digital_Block_MODEM_PCM_2_O);

	pr_debug("%s, b_modem1_speech_on=%d, b_modem2_speech_on=%d, speech_md_active=%d\n",
		 __func__, b_modem1_speech_on, b_modem2_speech_on,
		 voice_property.speech_md_active);

	if (b_modem1_speech_on == true ||
	    b_modem2_speech_on == true ||
	    voice_property.speech_md_active == true ||
	    GetOffloadEnableFlag() == true)  /* check dsp mp3 running status*/
		AudDrv_AUDINTBUS_Sel(0); /* select clk26M
					  * power down sysplll when suspend
					  */

	return 0;
}

static int mtk_voice_pm_ops_resume(struct device *device)
{
	bool b_modem1_speech_on;
	bool b_modem2_speech_on;

	/* don't switch to 26M, 96k/192k sram is not fast enough */
	/* TODO: KC: check if we need this
	 *  if (get_voice_ultra_status())
	 *	return 0;
	 */

	b_modem1_speech_on =
		GetMemoryPathEnable(Soc_Aud_Digital_Block_MODEM_PCM_1_O);
	b_modem2_speech_on =
		GetMemoryPathEnable(Soc_Aud_Digital_Block_MODEM_PCM_2_O);

	if (b_modem1_speech_on == true ||
	    b_modem2_speech_on == true ||
	    voice_property.speech_md_active == true ||
	    GetOffloadEnableFlag() == true)
		AudDrv_AUDINTBUS_Sel(1); /* syspll1_d4 */

	return 0;
}

const struct dev_pm_ops mtk_voice_pm_ops = {
	.suspend = mtk_voice_pm_ops_suspend,
	.resume = mtk_voice_pm_ops_resume,
	.freeze = NULL,
	.thaw = NULL,
	.poweroff = NULL,
	.restore = NULL,
	.restore_noirq = NULL,
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mt_soc_pcm_voice_md1_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_voice_md1",
	},
	{} };
#endif

static struct platform_driver mtk_voice_driver = {
	.driver = {

			.name = MT_SOC_VOICE_MD1,
			.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
			.of_match_table = mt_soc_pcm_voice_md1_of_ids,
#endif
#if IS_ENABLED(CONFIG_PM)
			.pm = &mtk_voice_pm_ops,
#endif
		},
	.probe = mtk_voice_probe,
	.remove = mtk_voice_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_voice_dev;
#endif

int mtk_soc_voice_platform_init(void)
{
	int ret = 0;

	pr_info("%s()\n", __func__);
#ifndef CONFIG_OF
	soc_mtk_voice_dev = platform_device_alloc(MT_SOC_VOICE_MD1, -1);

	if (!soc_mtk_voice_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_voice_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_voice_dev);
		return ret;
	}
#endif

	ret = platform_driver_register(&mtk_voice_driver);

	return ret;
}

void mtk_soc_voice_platform_exit(void)
{
	pr_info("%s()\n", __func__);
	platform_driver_unregister(&mtk_voice_driver);
}

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
