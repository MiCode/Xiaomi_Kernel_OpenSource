// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_mrgrx.c
 *
 * Project:
 * --------
 *    merge interface rx
 *
 * Description:
 * ------------
 *   Audio mrgrx playback
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *----------------------------------------------------------------------------
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
#include <linux/dma-mapping.h>

#ifndef CONFIG_SND_SOC_MTK_BTCVSD
#include <mtk_wcn_cmb_stub.h>
#endif

/*
 *    function implementation
 */

static int mtk_mrgrx_probe(struct platform_device *pdev);
static int mtk_pcm_mrgrx_close(struct snd_pcm_substream *substream);
static int mtk_afe_mrgrx_component_probe(struct snd_soc_component *component);

static unsigned int mmrgrx_Volume = 0x10000;
static bool mPrepareDone;

static int Audio_mrgrx_Volume_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_AmpR_Get = %d\n", mmrgrx_Volume);
	ucontrol->value.integer.value[0] = mmrgrx_Volume;
	return 0;
}

static int Audio_mrgrx_Volume_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	mmrgrx_Volume = ucontrol->value.integer.value[0];
	pr_debug("%s mmrgrx_Volume = 0x%x\n", __func__, mmrgrx_Volume);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT) == true)
		SetHwDigitalGain(Soc_Aud_Digital_Block_HW_GAIN1, mmrgrx_Volume);
	return 0;
}

static const char *const wcn_stub_audio_ctr[] = {
	"CMB_STUB_AIF_0", "CMB_STUB_AIF_1", "CMB_STUB_AIF_2", "CMB_STUB_AIF_3"};

static const struct soc_enum wcn_stub_audio_ctr_Enum[] = {
	/* speaker class setting */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(wcn_stub_audio_ctr), wcn_stub_audio_ctr),
};

#ifndef CONFIG_SND_SOC_MTK_BTCVSD
static int mAudio_Wcn_Cmb = CMB_STUB_AIF_3;
#else
static int mAudio_Wcn_Cmb;
#endif
static int Audio_Wcn_Cmb_Get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_Wcn_Cmb_Get = %d\n", mAudio_Wcn_Cmb);
	ucontrol->value.integer.value[0] = mAudio_Wcn_Cmb;
	return 0;
}

static int Audio_Wcn_Cmb_Set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	mAudio_Wcn_Cmb = ucontrol->value.integer.value[0];
#ifndef CONFIG_SND_SOC_MTK_BTCVSD
	pr_debug("%s mAudio_Wcn_Cmb = 0x%x\n", __func__, mAudio_Wcn_Cmb);
#ifndef CONFIG_SND_SOC_MTK_BTCVSD
	mtk_wcn_cmb_stub_audio_ctrl((enum CMB_STUB_AIF_X)mAudio_Wcn_Cmb);
#endif
#endif
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_mrgrx_controls[] = {
	SOC_SINGLE_EXT("Audio Mrgrx Volume", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_mrgrx_Volume_Get, Audio_mrgrx_Volume_Set),
	SOC_ENUM_EXT("cmb stub Audio Control", wcn_stub_audio_ctr_Enum[0],
		     Audio_Wcn_Cmb_Get, Audio_Wcn_Cmb_Set),
};

static struct snd_pcm_hardware mtk_mrgrx_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_STD_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = MRGRX_MAX_BUFFER_SIZE,
	.period_bytes_max = MRGRX_MAX_PERIOD_SIZE,
	.periods_min = MRGRX_MIN_PERIOD_SIZE,
	.periods_max = MRGRX_MAX_PERIOD_SIZE,
	.fifo_size = 0,
};

static int mtk_pcm_mrgrx_stop(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_pcm_mrgrx_stop\n");
	return 0;
}

static kal_int32 Previous_Hw_cur;
static snd_pcm_uframes_t
mtk_pcm_mrgrx_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t return_frames;

	return_frames = (Previous_Hw_cur >> 2);
	return return_frames;
}

static int mtk_pcm_mrgrx_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	pr_debug("mtk_pcm_mrgrx_hw_params\n");
	return ret;
}

static int mtk_pcm_mrgrx_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_pcm_mrgrx_hw_free\n");
	return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_hw_constraint_list mrgrx_constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_fm_supported_sample_rates),
	.list = soc_fm_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_mrgrx_open(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	AudDrv_Clk_On();
	pr_debug("mtk_pcm_mrgrx_open\n");
	runtime->hw = mtk_mrgrx_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_mrgrx_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &mrgrx_constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	pr_debug("mtk_pcm_mrgrx_open runtime rate = %d channels = %d substream->pcm->device = %d\n",
		runtime->rate, runtime->channels, substream->pcm->device);

	if (ret < 0) {
		pr_debug("mtk_pcm_mrgrx_close\n");
		mtk_pcm_mrgrx_close(substream);
		return ret;
	}
	SetFMEnableFlag(true);

	pr_debug("mtk_pcm_mrgrx_open return\n");
	return 0;
}

static int mtk_pcm_mrgrx_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);

#ifndef CONFIG_SND_SOC_MTK_BTCVSD
	mtk_wcn_cmb_stub_audio_ctrl((enum CMB_STUB_AIF_X)CMB_STUB_AIF_0);
#endif

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT, false);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT) == false)
		SetMrgI2SEnable(false, runtime->rate);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
	if (GetI2SDacEnable() == false)
		SetI2SDacEnable(false);

	/* interconnection setting */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_HW_GAIN1_OUT);

	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_HW_GAIN1_IN,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_HW_GAIN1_IN,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_HW_GAIN1_IN,
			  Soc_Aud_AFE_IO_Block_I2S3);

	EnableAfe(false);

	AudDrv_Clk_Off();
	mPrepareDone = false;
	SetFMEnableFlag(false);

	return 0;
}

static int mtk_pcm_mrgrx_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s rate = %d\n", __func__, runtime->rate);

	if (mPrepareDone == false) {

#ifndef CONFIG_SND_SOC_MTK_BTCVSD
		mtk_wcn_cmb_stub_audio_ctrl(
			(enum CMB_STUB_AIF_X)CMB_STUB_AIF_3);
#endif

		/* interconnection setting */
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_MRG_I2S_IN,
				  Soc_Aud_AFE_IO_Block_HW_GAIN1_OUT);

		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_HW_GAIN1_IN,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC);
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_HW_GAIN1_IN,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_HW_GAIN1_IN,
				  Soc_Aud_AFE_IO_Block_I2S3);

		/* Set HW_GAIN */
		SetHwDigitalGainMode(Soc_Aud_Digital_Block_HW_GAIN1,
				     runtime->rate, 0x40);
		SetHwDigitalGainEnable(Soc_Aud_Digital_Block_HW_GAIN1, true);
		SetHwDigitalGain(Soc_Aud_Digital_Block_HW_GAIN1,
				 mmrgrx_Volume);

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

		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT) ==
		    false) {
			/* set merge interface */
			SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT,
					    true);
			SetMrgI2SEnable(true, runtime->rate);
		} else
			SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT,
					    true);

		EnableAfe(true);
		mPrepareDone = true;
	}
	return 0;
}

static int mtk_pcm_mrgrx_start(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_pcm_mrgrx_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("mtk_pcm_mrgrx_trigger cmd = %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_mrgrx_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_mrgrx_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_mrgrx_copy(struct snd_pcm_substream *substream,
			      int channel,
			      unsigned long pos,
			      void __user *buf,
			      unsigned long bytes)
{
	return bytes;
}

static int mtk_pcm_mrgrx_silence(struct snd_pcm_substream *substream,
				 int channel,
				 unsigned long pos,
				 unsigned long bytes)
{
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_mrgrx_pcm_page(struct snd_pcm_substream *substream,
				       unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_mrgrx_ops = {
	.open = mtk_pcm_mrgrx_open,
	.close = mtk_pcm_mrgrx_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_mrgrx_hw_params,
	.hw_free = mtk_pcm_mrgrx_hw_free,
	.prepare = mtk_pcm_mrgrx_prepare,
	.trigger = mtk_pcm_mrgrx_trigger,
	.pointer = mtk_pcm_mrgrx_pointer,
	.copy_user = mtk_pcm_mrgrx_copy,
	.fill_silence = mtk_pcm_mrgrx_silence,
	.page = mtk_mrgrx_pcm_page,
};

static struct snd_soc_component_driver mtk_mrgrx_soc_component = {
	.name = AFE_PCM_NAME,
	.ops = &mtk_mrgrx_ops,
	.probe = mtk_afe_mrgrx_component_probe,
};

static int mtk_mrgrx_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_MRGRX_PCM);
	pdev->name = pdev->dev.kobj.name;

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev,
					  &mtk_mrgrx_soc_component,
					  NULL,
					  0);
}

static int mtk_afe_mrgrx_component_probe(struct snd_soc_component *component)
{
	pr_debug("%s\n", __func__);
	snd_soc_add_component_controls(component, Audio_snd_mrgrx_controls,
				      ARRAY_SIZE(Audio_snd_mrgrx_controls));
	return 0;
}

static int mtk_mrgrx_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_mrgrx_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_mrgrx",
	},
	{} };
#endif

static struct platform_driver mtk_mrgrx_driver = {
	.driver = {

			.name = MT_SOC_MRGRX_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_mrgrx_of_ids,
#endif
		},
	.probe = mtk_mrgrx_probe,
	.remove = mtk_mrgrx_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkmrgrx_dev;
#endif

static int __init mtk_mrgrx_soc_platform_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkmrgrx_dev = platform_device_alloc(MT_SOC_MRGRX_PCM, -1);
	if (!soc_mtkmrgrx_dev)
		return -ENOMEM;
	ret = platform_device_add(soc_mtkmrgrx_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkmrgrx_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_mrgrx_driver);
	return ret;
}
module_init(mtk_mrgrx_soc_platform_init);

static void __exit mtk_mrgrx_soc_platform_exit(void)
{
	platform_driver_unregister(&mtk_mrgrx_driver);
}
module_exit(mtk_mrgrx_soc_platform_exit);

MODULE_DESCRIPTION("mrgrx module platform driver");
MODULE_LICENSE("GPL");
