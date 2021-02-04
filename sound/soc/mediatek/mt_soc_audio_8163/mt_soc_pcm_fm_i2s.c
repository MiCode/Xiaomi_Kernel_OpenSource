/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */



/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"

#if 0				/* temp mark for early porting */
#include <mach/mtk_wcn_cmb_stub.h>
#endif
/* static DEFINE_SPINLOCK(auddrv_fm_i2s_lock); */

/*
 *    function implementation
 */

static int mtk_fm_i2s_probe(struct platform_device *pdev);
static int mtk_pcm_fm_i2s_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_fm_i2s_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_fm_i2s_probe(struct snd_soc_platform *platform);

static uint32 mfm_i2s_Volume = 0x10000;
static bool mPrepareDone;

static int Audio_fm_i2s_Volume_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_fm_i2s_Volume_Get = %d\n", mfm_i2s_Volume);
	ucontrol->value.integer.value[0] = mfm_i2s_Volume;
	return 0;

}

static int Audio_fm_i2s_Volume_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	mfm_i2s_Volume = ucontrol->value.integer.value[0];
	pr_debug("%s mfm_i2s_Volume = 0x%x\n", __func__, mfm_i2s_Volume);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2) == true)
		SetHwDigitalGain(mfm_i2s_Volume,
			Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1);

	return 0;
}

static const char * const wcn_stub_audio_ctr[] = {
	"CMB_STUB_AIF_0",
	"CMB_STUB_AIF_1",
	"CMB_STUB_AIF_2",
	"CMB_STUB_AIF_3"
};

static const struct soc_enum wcn_stub_audio_ctr_Enum[] = {

	/* speaker class setting */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(wcn_stub_audio_ctr), wcn_stub_audio_ctr),
};

/* static int mAudio_Wcn_Cmb = CMB_STUB_AIF_3;//temp mark for early porting */
static int Audio_Wcn_Cmb_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if 0				/* temp mark for early porting */
	pr_debug("Audio_Wcn_Cmb_Get = %d\n", mAudio_Wcn_Cmb);
	ucontrol->value.integer.value[0] = mAudio_Wcn_Cmb;
#endif
	return 0;
}

static int Audio_Wcn_Cmb_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if 0
	/* temp mark for early porting */
	mAudio_Wcn_Cmb = ucontrol->value.integer.value[0];
	pr_debug("%s mAudio_Wcn_Cmb = 0x%x\n", __func__, mAudio_Wcn_Cmb);
	//temp mark for early porting
	/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)mAudio_Wcn_Cmb); */
#endif
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_fm_i2s_controls[] = {

	SOC_SINGLE_EXT("Audio FM I2S Volume",
		SND_SOC_NOPM, 0, 0x80000, 0, Audio_fm_i2s_Volume_Get,
		Audio_fm_i2s_Volume_Set),
	SOC_ENUM_EXT("cmb stub Audio Control",
		wcn_stub_audio_ctr_Enum[0],
		Audio_Wcn_Cmb_Get,
		Audio_Wcn_Cmb_Set),
};

static struct snd_pcm_hardware mtk_fm_i2s_hardware = {

	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_STD_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = FM_I2S_MAX_BUFFER_SIZE,
	.period_bytes_max = FM_I2S_MAX_PERIOD_SIZE,
	.periods_min = FM_I2S_MIN_PERIOD_SIZE,
	.periods_max = FM_I2S_MAX_PERIOD_SIZE,
	.fifo_size = 0,
};

static int mtk_pcm_fm_i2s_stop(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_pcm_fm_i2s_stop\n");
	return 0;
}

static kal_int32 Previous_Hw_cur;
static snd_pcm_uframes_t mtk_pcm_fm_i2s_pointer(
	struct snd_pcm_substream *substream)
{
	return (Previous_Hw_cur >> 2);
}


static int mtk_pcm_fm_i2s_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	pr_debug("mtk_pcm_fm_i2s_hw_params\n");
	return ret;
}

static int mtk_pcm_fm_i2s_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_pcm_fm_i2s_hw_free\n");
	return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_hw_constraint_list fm_i2s_constraints_sample_rates = {

	.count = ARRAY_SIZE(soc_fm_supported_sample_rates),
	.list = soc_fm_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_fm_i2s_open(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();
	AudDrv_I2S_Clk_On();

	pr_debug("mtk_pcm_fm_i2s_open\n");
	runtime->hw = mtk_fm_i2s_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_fm_i2s_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
		&fm_i2s_constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
		SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_integer failed\n");

	pr_debug("%s, runtime->rate = %d, channels = %d, substream->pcm->device = %d\n",
		__func__,
		runtime->rate,
		runtime->channels,
		substream->pcm->device);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_fm_i2s_playback_constraints\n");

	if (ret < 0) {
		pr_debug("mtk_pcm_fm_i2s_close\n");
		mtk_pcm_fm_i2s_close(substream);
		return ret;
	}

	SetFMEnableFlag(true);
	pr_debug("mtk_pcm_fm_i2s_open return\n");
	return 0;
}

static int mtk_pcm_fm_i2s_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s rate = %d\n", __func__, runtime->rate);
	//temp mark for early porting
	/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_0);*/

	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2, false);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2) == false) {
		SetI2SASRCEnable(false);
		SetI2SASRCConfig(false, 0);	/* Setting to bypass ASRC */
		Set2ndI2SInEnable(false);
	}

	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);

	if (GetI2SDacEnable() == false)
		SetI2SDacEnable(false);

	/* interconnection setting */
	SetConnection(Soc_Aud_InterCon_DisConnect,
		Soc_Aud_InterConnectionInput_I00,
		Soc_Aud_InterConnectionOutput_O13);
	SetConnection(Soc_Aud_InterCon_DisConnect,
		Soc_Aud_InterConnectionInput_I01,
		Soc_Aud_InterConnectionOutput_O14);
	SetConnection(Soc_Aud_InterCon_DisConnect,
		Soc_Aud_InterConnectionInput_I10,
		Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_DisConnect,
		Soc_Aud_InterConnectionInput_I11,
		Soc_Aud_InterConnectionOutput_O04);


	EnableAfe(false);

	AudDrv_I2S_Clk_Off();
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();
	mPrepareDone = false;
	SetFMEnableFlag(false);

	return 0;
}

static int mtk_pcm_fm_i2s_prepare(struct snd_pcm_substream *substream)
{
	struct AudioDigtalI2S m2ndI2SInAttribute;

	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s rate = %d\n", __func__, runtime->rate);

	if (mPrepareDone == false) {
		/* temp mark for early porting */
		/* mtk_wcn_cmb_stub_audio_ctrl(
		 *	(CMB_STUB_AIF_X)CMB_STUB_AIF_3);
		 */
		/* interconnection setting */
		SetConnection(Soc_Aud_InterCon_Connection,
			Soc_Aud_InterConnectionInput_I00,
			Soc_Aud_InterConnectionOutput_O13);
		SetConnection(Soc_Aud_InterCon_Connection,
			Soc_Aud_InterConnectionInput_I01,
			Soc_Aud_InterConnectionOutput_O14);
		SetConnection(Soc_Aud_InterCon_Connection,
			Soc_Aud_InterConnectionInput_I10,
			Soc_Aud_InterConnectionOutput_O03);
		SetConnection(Soc_Aud_InterCon_Connection,
			Soc_Aud_InterConnectionInput_I11,
			Soc_Aud_InterConnectionOutput_O04);

		/* Set HW_GAIN */
		SetHwDigitalGainMode(Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1,
			runtime->rate, 0x40);
		SetHwDigitalGainEnable(Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1,
			true);
		SetHwDigitalGain(mfm_i2s_Volume,
			Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1);

		/* start I2S DAC out */
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) ==
			false) {
			SetI2SDacOut(runtime->rate, false,
				Soc_Aud_I2S_WLEN_WLEN_16BITS);
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
				true);
			SetI2SDacEnable(true);
		} else
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
				true);

		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2) ==
			false) {
			/* set merge interface */
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2,
				true);

			/* Config 2nd I2S IN */
			memset((void *)&m2ndI2SInAttribute,
				0,
				sizeof(m2ndI2SInAttribute));

			m2ndI2SInAttribute.mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
			/* I2S_IN_FROM_CONNSYS */
			m2ndI2SInAttribute.mI2S_IN_PAD_SEL = false;
			m2ndI2SInAttribute.mI2S_SLAVE =
				Soc_Aud_I2S_SRC_SLAVE_MODE;
			m2ndI2SInAttribute.mI2S_SAMPLERATE = 32000;
			m2ndI2SInAttribute.mINV_LRCK =
				Soc_Aud_INV_LRCK_NO_INVERSE;
			m2ndI2SInAttribute.mI2S_FMT =
				Soc_Aud_I2S_FORMAT_I2S;
			m2ndI2SInAttribute.mI2S_WLEN =
				Soc_Aud_I2S_WLEN_WLEN_16BITS;
			Set2ndI2SIn(&m2ndI2SInAttribute);
			/* Covert from 32000 Hz to 44100 Hz */
			SetI2SASRCConfig(true, 44100);
			SetI2SASRCEnable(true);

			Set2ndI2SInEnable(true);
		} else {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2,
				true);
		}

		EnableAfe(true);
		mPrepareDone = true;
	}
	return 0;
}

static int mtk_pcm_fm_i2s_start(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mtk_pcm_fm_i2s_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("mtk_pcm_fm_i2s_trigger cmd = %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_fm_i2s_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_fm_i2s_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_fm_i2s_copy(struct snd_pcm_substream *substream,
			       int channel, snd_pcm_uframes_t pos,
			       void __user *dst, snd_pcm_uframes_t count)
{
	count = audio_frame_to_bytes(substream, count);
	return count;
}

static int mtk_pcm_fm_i2s_silence(struct snd_pcm_substream *substream,
	int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	pr_debug("%s\n", __func__);
	/* do nothing */
	return 0;
}

static void *dummy_page[2];

static struct page *mtk_fm_i2s_pcm_page(struct snd_pcm_substream *substream,
	unsigned long offset)
{
	pr_debug("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_fm_i2s_ops = {

	.open = mtk_pcm_fm_i2s_open,
	.close = mtk_pcm_fm_i2s_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_fm_i2s_hw_params,
	.hw_free = mtk_pcm_fm_i2s_hw_free,
	.prepare = mtk_pcm_fm_i2s_prepare,
	.trigger = mtk_pcm_fm_i2s_trigger,
	.pointer = mtk_pcm_fm_i2s_pointer,
	.copy = mtk_pcm_fm_i2s_copy,
	.silence = mtk_pcm_fm_i2s_silence,
	.page = mtk_fm_i2s_pcm_page,
};

static struct snd_soc_platform_driver mtk_fm_i2s_soc_platform = {

	.ops = &mtk_fm_i2s_ops,
	.pcm_new = mtk_asoc_pcm_fm_i2s_new,
	.probe = mtk_afe_fm_i2s_probe,
};

static int mtk_fm_i2s_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_FM_I2S_PCM);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev, &mtk_fm_i2s_soc_platform);
}

static int mtk_asoc_pcm_fm_i2s_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	return ret;
}


static int mtk_afe_fm_i2s_probe(struct snd_soc_platform *platform)
{
	pr_debug("mtk_afe_fm_i2s_probe\n");
	snd_soc_add_platform_controls(platform, Audio_snd_fm_i2s_controls,
				      ARRAY_SIZE(Audio_snd_fm_i2s_controls));
	return 0;
}

static int mtk_fm_i2s_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_fm_i2s_of_ids[] = {

	{.compatible = "mediatek,mt8163-soc-pcm-fm-i2s",},
	{}
};
#endif

static struct platform_driver mtk_fm_i2s_driver = {

	.driver = {
		   .name = MT_SOC_FM_I2S_PCM,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_pcm_fm_i2s_of_ids,
#endif
		   },
	.probe = mtk_fm_i2s_probe,
	.remove = mtk_fm_i2s_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkfm_i2s_dev;
#endif

static int __init mtk_fm_i2s_soc_platform_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkfm_i2s_dev = platform_device_alloc(MT_SOC_FM_I2S_PCM, -1);

	if (!soc_mtkfm_i2s_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkfm_i2s_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkfm_i2s_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_fm_i2s_driver);
	return ret;

}
module_init(mtk_fm_i2s_soc_platform_init);

static void __exit mtk_fm_i2s_soc_platform_exit(void)
{
	pr_debug("%s\n", __func__);

	platform_driver_unregister(&mtk_fm_i2s_driver);
}
module_exit(mtk_fm_i2s_soc_platform_exit);

MODULE_DESCRIPTION("fm_i2s module platform driver");
MODULE_LICENSE("GPL");
