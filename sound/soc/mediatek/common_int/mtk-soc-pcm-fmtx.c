// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_fmtx.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio fmtx data1 playback
 *
 * Author:George
 *
 * -------
 *
 *
 *------------------------------------------------------------------------------
 **
 *
 ******************************************************************************
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/dma-mapping.h>
#include <sound/pcm_params.h>

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"

static struct afe_mem_control_t *pMemControl;
static struct snd_dma_buffer *FMTX_Playback_dma_buf;
static unsigned int mPlaybackDramState;
static struct device *mDev;

/*
 *    function implementation
 */

static int mtk_fmtx_probe(struct platform_device *pdev);
static int mtk_pcm_fmtx_close(struct snd_pcm_substream *substream);
static int mtk_afe_fmtx_component_probe(struct snd_soc_component *component);

static int fmtx_hdoutput_control = true;

const char *const fmtx_HD_output[] = {"Off", "On"};

static const struct soc_enum Audio_fmtx_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fmtx_HD_output), fmtx_HD_output),
};

static int Audio_fmtx_hdoutput_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_AmpR_Get = %d\n", fmtx_hdoutput_control);
	ucontrol->value.integer.value[0] = fmtx_hdoutput_control;
	return 0;
}

static int Audio_fmtx_hdoutput_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(fmtx_HD_output)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	fmtx_hdoutput_control = ucontrol->value.integer.value[0];
	pr_debug("%s()\n", __func__);

#if 0
	if (fmtx_hdoutput_control) {
		/* set APLL clock setting */
		EnableApll1(true);
		EnableApll2(true);
#if 0
		EnableI2SDivPower(AUDIO_APLL1_DIV0, true);
		EnableI2SDivPower(AUDIO_APLL2_DIV0, true);
#else
		EnableI2SCLKDiv(Soc_Aud_APLL1_DIV, true);
		EnableI2SCLKDiv(Soc_Aud_APLL2_DIV, true);
#endif
		AudDrv_APLL1Tuner_Clk_On();
		AudDrv_APLL2Tuner_Clk_On();
	} else {
		/* set APLL clock setting */
		EnableApll1(false);
		EnableApll2(false);
#if 0
		EnableI2SDivPower(AUDIO_APLL1_DIV0, false);
		EnableI2SDivPower(AUDIO_APLL2_DIV0, false);
#else
		EnableI2SCLKDiv(Soc_Aud_APLL1_DIV, false);
		EnableI2SCLKDiv(Soc_Aud_APLL2_DIV, false);
#endif
		AudDrv_APLL1Tuner_Clk_Off();
		AudDrv_APLL2Tuner_Clk_Off();
	}
#endif
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_fmtx_controls[] = {
	SOC_ENUM_EXT("Audio_FMTX_hd_Switch", Audio_fmtx_Enum[0],
		     Audio_fmtx_hdoutput_Get, Audio_fmtx_hdoutput_Set),
};

static struct snd_pcm_hardware mtk_fmtx_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = MIN_PERIOD_SIZE,
	.periods_max = MAX_PERIOD_SIZE,
	.fifo_size = 0,
};

static int mtk_pcm_fmtx_stop(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	/* struct afe_block_t *Afe_Block = &(pMemControl->rBlock); */
#if defined(FMTX_DEBUG_LOG)
	pr_debug("mtk_pcm_fmtx_stop\n");
#endif
	irq_remove_user(substream,
			irq_request_number(Soc_Aud_Digital_Block_MEM_DL1));

	/* here to turn off digital part */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_MRG_I2S_OUT);

	/* if (GetMrgI2SEnable() == false) */
	/* { */
	SetMrgI2SEnable(false, runtime->rate);
	/* } */

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT, false);

	Set2ndI2SOutEnable(false);

	EnableAfe(false);

	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

	return 0;
}

static snd_pcm_uframes_t
mtk_pcm_fmtx_pointer(struct snd_pcm_substream *substream)
{
	return get_mem_frame_index(substream, pMemControl,
				   Soc_Aud_Digital_Block_MEM_DL1);
}

static int mtk_pcm_fmtx_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
	if (AllocateAudioSram(&substream->runtime->dma_addr,
			      &substream->runtime->dma_area,
			      substream->runtime->dma_bytes, substream,
			      params_format(hw_params), false) == 0) {
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL1, false,
			    substream->runtime->dma_addr);

	} else {
		substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
		substream->runtime->dma_area = FMTX_Playback_dma_buf->area;
		substream->runtime->dma_addr = FMTX_Playback_dma_buf->addr;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL1, true,
			    substream->runtime->dma_addr);
		mPlaybackDramState = true;
		AudDrv_Emi_Clk_On();
	}

	set_mem_block(substream, hw_params, pMemControl,
		      Soc_Aud_Digital_Block_MEM_DL1);

	/* ------------------------------------------------------- */
	pr_debug("1 dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		substream->runtime->dma_bytes, substream->runtime->dma_area,
		(long)substream->runtime->dma_addr);

	return ret;
}

static int mtk_pcm_fmtx_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s substream = %p\n", __func__, substream);
	if (mPlaybackDramState == true) {
		AudDrv_Emi_Clk_Off();
		mPlaybackDramState = false;
	} else
		freeAudioSram((void *)substream);
	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_fmtx_sample_rates = {
	.count = ARRAY_SIZE(soc_fm_supported_sample_rates),
	.list = soc_fm_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_fmtx_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	mPlaybackDramState = false;
	mtk_fmtx_hardware.buffer_bytes_max = GetPLaybackSramFullSize();

	pr_debug("mtk_I2S0dl1_hardware.buffer_bytes_max = %zu mPlaybackDramState = %d\n",
		mtk_fmtx_hardware.buffer_bytes_max, mPlaybackDramState);
	runtime->hw = mtk_fmtx_hardware;

	AudDrv_Clk_On();
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_fmtx_hardware,
	       sizeof(struct snd_pcm_hardware));
	pMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_fmtx_sample_rates);

	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_integer failed\n");

	if (ret < 0) {
		pr_err("ret < 0 mtkalsa_fmtx_playback close\n");
		mtk_pcm_fmtx_close(substream);
		return ret;
	}
	return 0;
}

static int mtk_pcm_fmtx_close(struct snd_pcm_substream *substream)
{
#if defined(FMTX_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_0); */

	AudDrv_Clk_Off();
	return 0;
}

static int mtk_pcm_fmtx_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_pcm_fmtx_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_2); */

	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
	    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
		SetMemIfFetchFormatPerSample(
			Soc_Aud_Digital_Block_MEM_DL1,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		SetConnectionFormat(
			OUTPUT_DATA_FORMAT_16BIT,
			Soc_Aud_AFE_IO_Block_MRG_I2S_OUT);
			/* FM Tx only support 16 bit */
	} else {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
					     AFE_WLEN_16_BIT);
		SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
				    Soc_Aud_AFE_IO_Block_MRG_I2S_OUT);
	}

	/* here start digital part */
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_MRG_I2S_OUT);

	/* set dl1 sample ratelimit_state */
	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);

	/* start MRG I2S Out */
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT, true);
	SetMrgI2SEnable(true, runtime->rate);

	/* start 2nd I2S Out */

	Set2ndI2SOutAttribute(runtime->rate);
	Set2ndI2SOutEnable(true);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

	/* here to set interrupt */
	irq_add_user(substream,
		     irq_request_number(Soc_Aud_Digital_Block_MEM_DL1),
		     runtime->rate, runtime->period_size * 2 / 3);

	EnableAfe(true);

	return 0;
}

static int mtk_pcm_fmtx_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("mtk_pcm_fmtx_trigger cmd = %d\n", cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_fmtx_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_fmtx_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_fmtx_copy(struct snd_pcm_substream *substream,
			     int channel,
			     unsigned long pos,
			     void __user *buf,
			     unsigned long bytes)
{
	return mtk_memblk_copy(substream,
			       channel,
			       pos,
			       buf,
			       bytes,
			       pMemControl,
			       Soc_Aud_Digital_Block_MEM_DL1);
}

static int mtk_pcm_fmtx_silence(struct snd_pcm_substream *substream,
				int channel,
				unsigned long pos,
				unsigned long bytes)
{
#if defined(FMTX_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_pcm_fmtx_page(struct snd_pcm_substream *substream,
				      unsigned long offset)
{
#if defined(FMTX_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_fmtx_ops = {
	.open = mtk_pcm_fmtx_open,
	.close = mtk_pcm_fmtx_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_fmtx_hw_params,
	.hw_free = mtk_pcm_fmtx_hw_free,
	.prepare = mtk_pcm_fmtx_prepare,
	.trigger = mtk_pcm_fmtx_trigger,
	.pointer = mtk_pcm_fmtx_pointer,
	.copy_user = mtk_pcm_fmtx_copy,
	.fill_silence = mtk_pcm_fmtx_silence,
	.page = mtk_pcm_fmtx_page,
};

static struct snd_soc_component_driver mtk_fmtx_soc_component = {
	.name = AFE_PCM_NAME,
	.ops = &mtk_fmtx_ops,
	.probe = mtk_afe_fmtx_component_probe,
};

static int mtk_fmtx_probe(struct platform_device *pdev)
{
	/* int ret = 0; */
#if defined(FMTX_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_FM_MRGTX_PCM);
#if defined(FMTX_DEBUG_LOG)
	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
#endif
	mDev = &pdev->dev;

	return snd_soc_register_component(&pdev->dev,
					  &mtk_fmtx_soc_component,
					  NULL,
					  0);
}

static int mtk_afe_fmtx_component_probe(struct snd_soc_component *component)
{
#if defined(FMTX_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	snd_soc_add_component_controls(component, Audio_snd_fmtx_controls,
				      ARRAY_SIZE(Audio_snd_fmtx_controls));
	AudDrv_Allocate_mem_Buffer(component->dev, Soc_Aud_Digital_Block_MEM_DL1,
				   Dl1_MAX_BUFFER_SIZE);
	FMTX_Playback_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);

	return 0;
}

static int mtk_fmtx_remove(struct platform_device *pdev)
{
#if defined(FMTX_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_fmtx_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_fmtx",
	},
	{} };
#endif

static struct platform_driver mtk_fmtx_driver = {
	.driver = {

			.name = MT_SOC_FM_MRGTX_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_fmtx_of_ids,
#endif
		},
	.probe = mtk_fmtx_probe,
	.remove = mtk_fmtx_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkfmtx_dev;
#endif

static int __init mtk_soc_platform_init(void)
{
	int ret;
#if defined(FMTX_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
#ifndef CONFIG_OF
	soc_mtkfmtx_dev = platform_device_alloc(MT_SOC_FM_MRGTX_PCM, -1);
	if (!soc_mtkfmtx_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkfmtx_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkfmtx_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_fmtx_driver);
	return ret;
}
module_init(mtk_soc_platform_init);

static void __exit mtk_soc_platform_exit(void)
{
	platform_driver_unregister(&mtk_fmtx_driver);
}
module_exit(mtk_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
