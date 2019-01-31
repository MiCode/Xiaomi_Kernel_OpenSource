/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_fm_i2s.c
 *
 * Project:
 * --------
 *    merge interface rx
 *
 * Description:
 * ------------
 *   Audio fm i2s playback
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
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include <sound/pcm_params.h>

/* static DEFINE_SPINLOCK(auddrv_fm_i2s_lock); */

/*
 *    function implementation
 */

static int mtk_fm_i2s_probe(struct platform_device *pdev);
static int mtk_pcm_fm_i2s_close(struct snd_pcm_substream *substream);
static int mtk_afe_fm_i2s_probe(struct snd_soc_platform *platform);

static unsigned int mfm_i2s_Volume = 0x10000;
static bool mPrepareDone;

static int Audio_fm_i2s_Volume_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_AmpR_Get = %d\n", mfm_i2s_Volume);
	ucontrol->value.integer.value[0] = mfm_i2s_Volume;
	return 0;
}

static int Audio_fm_i2s_Volume_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	mfm_i2s_Volume = ucontrol->value.integer.value[0];
	pr_debug("%s mfm_i2s_Volume = 0x%x\n", __func__, mfm_i2s_Volume);

	if (GetFmI2sInPathEnable() == true)
		SetHwDigitalGain(mfm_i2s_Volume,
				 Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1);

	return 0;
}

static const struct snd_kcontrol_new Audio_snd_fm_i2s_controls[] = {
	SOC_SINGLE_EXT("Audio FM I2S Volume", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_fm_i2s_Volume_Get, Audio_fm_i2s_Volume_Set),
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
static snd_pcm_uframes_t
mtk_pcm_fm_i2s_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t return_frames;

	return_frames = (Previous_Hw_cur >> 2);
	return return_frames;
}

static int mtk_pcm_fm_i2s_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

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

	if (ret < 0) {
		pr_err("mtk_pcm_fm_i2s_close\n");
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

	/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_0);//temp
	 * mark for early porting
	 */

	SetFmI2sInPathEnable(false);
	if (GetFmI2sInPathEnable() == false) {
		SetFmI2sAsrcEnable(false);
		SetFmI2sAsrcConfig(false, 0); /* Setting to bypass ASRC */
		SetFmI2sInEnable(false);
	}

	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
	if (GetI2SDacEnable() == false)
		SetI2SDacEnable(false);

	/* interconnection setting */
	SetFmI2sConnection(Soc_Aud_InterCon_DisConnect);

	EnableAfe(false);

	AudDrv_I2S_Clk_Off();
	AudDrv_Clk_Off();
	mPrepareDone = false;
	SetFMEnableFlag(false);

	return 0;
}

static int mtk_pcm_fm_i2s_prepare(struct snd_pcm_substream *substream)
{
	struct audio_digital_i2s mI2SInAttribute;

	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s rate = %d\n", __func__, runtime->rate);

	if (mPrepareDone == false) {
		/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_3);
		 * mark for early porting
		 */

		/* interconnection setting */
		SetFmI2sConnection(Soc_Aud_InterCon_Connection);

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
		if (GetFmI2sInPathEnable() == false) {
			/* set merge interface */
			SetFmI2sInPathEnable(true);

			/* Config I2S IN */
			memset_io((void *)&mI2SInAttribute, 0,
				  sizeof(mI2SInAttribute));

			mI2SInAttribute.mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
			mI2SInAttribute.mI2S_IN_PAD_SEL =
				false; /* I2S_IN_FROM_CONNSYS */
			mI2SInAttribute.mI2S_SLAVE = Soc_Aud_I2S_SRC_SLAVE_MODE;
			mI2SInAttribute.mI2S_SAMPLERATE = 32000;
			mI2SInAttribute.mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
			mI2SInAttribute.mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
			mI2SInAttribute.mI2S_WLEN =
				Soc_Aud_I2S_WLEN_WLEN_16BITS;
			SetFmI2sIn(&mI2SInAttribute);

			if (runtime->rate == 48000)
				SetFmI2sAsrcConfig(true,
						   48000); /* Covert from 32000
							    * Hz to 48000 Hz
							    */
			else
				SetFmI2sAsrcConfig(true,
						   44100); /* Covert from 32000
							    * Hz to 44100 Hz
							    */
			SetFmI2sAsrcEnable(true);

			SetFmI2sInEnable(true);
		} else
			SetFmI2sInPathEnable(true);

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

static int mtk_pcm_fm_i2s_copy(struct snd_pcm_substream *substream, int channel,
			       snd_pcm_uframes_t pos, void __user *dst,
			       snd_pcm_uframes_t count)
{
	count = audio_frame_to_bytes(substream, count);
	return count;
}

static int mtk_pcm_fm_i2s_silence(struct snd_pcm_substream *substream,
				  int channel, snd_pcm_uframes_t pos,
				  snd_pcm_uframes_t count)
{
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_fm_i2s_pcm_page(struct snd_pcm_substream *substream,
					unsigned long offset)
{
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
	.ops = &mtk_fm_i2s_ops, .probe = mtk_afe_fm_i2s_probe,
};

static int mtk_fm_i2s_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_FM_I2S_PCM);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev, &mtk_fm_i2s_soc_platform);
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
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_fm_i2s_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_fm_i2s",
	},
	{} };
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
	platform_driver_unregister(&mtk_fm_i2s_driver);
}
module_exit(mtk_fm_i2s_soc_platform_exit);

MODULE_DESCRIPTION("fm_i2s module platform driver");
MODULE_LICENSE("GPL");
