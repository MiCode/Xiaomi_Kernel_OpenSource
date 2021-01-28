// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_pcm_fm_i2s_awb.c
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio fm_i2s awb capture
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
#include <linux/dma-mapping.h>

/* information about */
static struct afe_mem_control_t *FM_I2S_AWB_Control_context;
static struct snd_dma_buffer *Awb_Capture_dma_buf;

static int fm_capture_mem_blk;

/*
 *    function implementation
 */
static void StartAudioFMI2SAWBHardware(struct snd_pcm_substream *substream);
static void StopAudioFMI2SAWBHardware(struct snd_pcm_substream *substream);
static int mtk_fm_i2s_awb_probe(struct platform_device *pdev);
static int mtk_fm_i2s_awb_pcm_close(struct snd_pcm_substream *substream);
static int mtk_afe_fm_i2s_awb_component_probe(struct snd_soc_component *component);

static struct snd_pcm_hardware mtk_mgrrx_awb_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_STD_MT_FMTS,
	.rates = SOC_NORMAL_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = FM_I2S_MAX_BUFFER_SIZE,
	.period_bytes_max = FM_I2S_MAX_BUFFER_SIZE,
	.periods_min = FM_I2S_MIN_PERIOD_SIZE,
	.periods_max = FM_I2S_MAX_PERIOD_SIZE,
	.fifo_size = 0,
};

static void StopAudioFMI2SAWBHardware(struct snd_pcm_substream *substream)
{
	pr_debug("StopAudioFMI2SAWBHardware\n");

	SetMemoryPathEnable(fm_capture_mem_blk, false);

	/* here to set interrupt */
	irq_remove_user(substream, irq_request_number(fm_capture_mem_blk));

	/* here to turn off digital part */
	SetFmAwbConnection(Soc_Aud_InterCon_DisConnect);

	SetFmI2sInPathEnable(false);
	if (GetFmI2sInPathEnable() == false) {
		SetFmI2sAsrcEnable(false);
		SetFmI2sAsrcConfig(false, 0); /* Setting to bypass ASRC */
		SetFmI2sInEnable(false);
	}

	EnableAfe(false);
}

static void StartAudioFMI2SAWBHardware(struct snd_pcm_substream *substream)
{
	struct audio_digital_i2s mI2SInAttribute;

	pr_debug("StartAudioFMI2SAWBHardware\n");

	/* here to set interrupt */
	irq_add_user(substream, irq_request_number(fm_capture_mem_blk),
		     substream->runtime->rate,
		     substream->runtime->period_size >> 1);

	SetSampleRate(fm_capture_mem_blk, substream->runtime->rate);
	SetMemoryPathEnable(fm_capture_mem_blk, true);

	/* here to turn off digital part */
	SetFmAwbConnection(Soc_Aud_InterCon_Connection);

	if (GetFmI2sInPathEnable() == false) {
		/* set merge interface */
		SetFmI2sInPathEnable(true);

		/* Config 2nd I2S IN */
		memset_io((void *)&mI2SInAttribute, 0, sizeof(mI2SInAttribute));

		mI2SInAttribute.mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
		mI2SInAttribute.mI2S_IN_PAD_SEL =
			false; /* I2S_IN_FROM_CONNSYS */
		mI2SInAttribute.mI2S_SLAVE = Soc_Aud_I2S_SRC_SLAVE_MODE;
		mI2SInAttribute.mI2S_SAMPLERATE = 32000;
		mI2SInAttribute.mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
		mI2SInAttribute.mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
		mI2SInAttribute.mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		SetFmI2sIn(&mI2SInAttribute);

		if (substream->runtime->rate == 48000)
			SetFmI2sAsrcConfig(
				true,
				48000); /* Covert from 32000 Hz to 48000 Hz */
		else
			SetFmI2sAsrcConfig(
				true,
				44100); /* Covert from 32000 Hz to 44100 Hz */
		SetFmI2sAsrcEnable(true);

		SetFmI2sInEnable(true);
	} else
		SetFmI2sInPathEnable(true);

	EnableAfe(true);
}

static int mtk_fm_i2s_awb_pcm_prepare(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_fm_i2s_awb_pcm_prepare substream->rate = %d  substream->channels = %d\n",
		substream->runtime->rate, substream->runtime->channels);
	return 0;
}

static int mtk_fm_i2s_awb_alsa_stop(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_fm_i2s_awb_alsa_stop\n");
	StopAudioFMI2SAWBHardware(substream);
	RemoveMemifSubStream(fm_capture_mem_blk, substream);

	return 0;
}

static snd_pcm_uframes_t
mtk_awb_pcm_pointer(struct snd_pcm_substream *substream)
{
	return get_mem_frame_index(substream, FM_I2S_AWB_Control_context,
				   fm_capture_mem_blk);
}

static int mtk_mgrrx_awb_pcm_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

	if (Awb_Capture_dma_buf->area) {
		pr_debug("mtk_mgrrx_awb_pcm_hw_params Awb_Capture_dma_buf->area\n");
		runtime->dma_bytes = params_buffer_bytes(hw_params);
		runtime->dma_area = Awb_Capture_dma_buf->area;
		runtime->dma_addr = Awb_Capture_dma_buf->addr;
		SetHighAddr(fm_capture_mem_blk, true, runtime->dma_addr);
	} else {
		pr_debug("mtk_mgrrx_awb_pcm_hw_params snd_pcm_lib_malloc_pages\n");
		ret = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(hw_params));
	}
	set_mem_block(substream, hw_params, FM_I2S_AWB_Control_context,
		      fm_capture_mem_blk);

	pr_debug("dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		substream->runtime->dma_bytes, substream->runtime->dma_area,
		(long)substream->runtime->dma_addr);
	return ret;
}

static int mtk_fm_i2s_capture_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_fm_i2s_capture_pcm_hw_free\n");
	if (Awb_Capture_dma_buf->area)
		return 0;
	else
		return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_hw_constraint_list fm_i2s_awb_constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_normal_supported_sample_rates),
	.list = soc_normal_supported_sample_rates,
};

static int mtk_fm_i2s_awb_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_debug("mtk_fm_i2s_awb_pcm_open\n");
	FM_I2S_AWB_Control_context = Get_Mem_ControlT(fm_capture_mem_blk);
	runtime->hw = mtk_mgrrx_awb_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_mgrrx_awb_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &fm_i2s_awb_constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_integer failed\n");

	if (substream->stream != SNDRV_PCM_STREAM_CAPTURE)
		return -1;
	/* here open audio clocks */
	AudDrv_Clk_On();
	AudDrv_I2S_Clk_On();
	AudDrv_Emi_Clk_On();

	if (ret < 0) {
		pr_err("mtk_fm_i2s_awb_pcm_close\n");
		mtk_fm_i2s_awb_pcm_close(substream);
		return ret;
	}
	pr_debug("mtk_fm_i2s_awb_pcm_open return\n");
	return 0;
}

static int mtk_fm_i2s_awb_pcm_close(struct snd_pcm_substream *substream)
{
	AudDrv_Emi_Clk_Off();
	AudDrv_I2S_Clk_Off();
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_fm_i2s_awb_alsa_start(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_fm_i2s_awb_alsa_start\n");
	SetMemifSubStream(fm_capture_mem_blk, substream);
	StartAudioFMI2SAWBHardware(substream);
	return 0;
}

static int mtk_capture_fm_i2s_pcm_trigger(struct snd_pcm_substream *substream,
					  int cmd)
{
	pr_debug("mtk_capture_fm_i2s_pcm_trigger cmd = %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_fm_i2s_awb_alsa_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_fm_i2s_awb_alsa_stop(substream);
	}
	return -EINVAL;
}

static int mtk_fm_i2s_awb_pcm_copy(struct snd_pcm_substream *substream,
				   int channel,
				   unsigned long pos,
				   void __user *buf,
				   unsigned long bytes)
{
	snd_pcm_uframes_t frames = audio_bytes_to_frame(substream, bytes);

	return mtk_memblk_copy(substream, channel, pos, buf, frames,
			       FM_I2S_AWB_Control_context, fm_capture_mem_blk);
}

static int mtk_capture_pcm_silence(struct snd_pcm_substream *substream,
				   int channel,
				   unsigned long pos,
				   unsigned long bytes)
{
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *
mtk_fm_i2s_capture_pcm_page(struct snd_pcm_substream *substream,
			    unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_fm_i2s_awb_ops = {
	.open = mtk_fm_i2s_awb_pcm_open,
	.close = mtk_fm_i2s_awb_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_mgrrx_awb_pcm_hw_params,
	.hw_free = mtk_fm_i2s_capture_pcm_hw_free,
	.prepare = mtk_fm_i2s_awb_pcm_prepare,
	.trigger = mtk_capture_fm_i2s_pcm_trigger,
	.pointer = mtk_awb_pcm_pointer,
	.copy_user = mtk_fm_i2s_awb_pcm_copy,
	.fill_silence = mtk_capture_pcm_silence,
	.page = mtk_fm_i2s_capture_pcm_page,
};

static struct snd_soc_component_driver mtk_soc_component = {
	.name = AFE_PCM_NAME,
	.ops = &mtk_fm_i2s_awb_ops,
	.probe = mtk_afe_fm_i2s_awb_component_probe,
};

static int mtk_fm_i2s_awb_probe(struct platform_device *pdev)
{
	fm_capture_mem_blk = get_usage_digital_block(AUDIO_USAGE_FM_CAPTURE);
	if (fm_capture_mem_blk < 0) {
		pr_debug("%s(), invalid mem blk %d, use default\n", __func__,
			 fm_capture_mem_blk);
		fm_capture_mem_blk = Soc_Aud_Digital_Block_MEM_AWB;
	}

	pr_debug("%s(), mem_blk %d\n", __func__, fm_capture_mem_blk);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_FM_I2S_AWB_PCM);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev,
					  &mtk_soc_component,
					  NULL,
					  0);
}

static int mtk_afe_fm_i2s_awb_component_probe(struct snd_soc_component *component)
{
	pr_debug("%s\n", __func__);
	AudDrv_Allocate_mem_Buffer(component->dev, fm_capture_mem_blk,
				   FM_I2S_MAX_BUFFER_SIZE);
	Awb_Capture_dma_buf = Get_Mem_Buffer(fm_capture_mem_blk);
	return 0;
}

static int mtk_fm_i2s_awb_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_fm_i2s_awb_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_fm_i2s_awb",
	},
	{} };
#endif

static struct platform_driver mtk_fm_i2s_awb_capture_driver = {
	.driver = {

			.name = MT_SOC_FM_I2S_AWB_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_fm_i2s_awb_of_ids,
#endif
		},
	.probe = mtk_fm_i2s_awb_probe,
	.remove = mtk_fm_i2s_awb_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_fm_i2s_capture_dev;
#endif

static int __init mtk_soc_fm_i2s_awb_platform_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_fm_i2s_capture_dev =
		platform_device_alloc(MT_SOC_FM_I2S_AWB_PCM, -1);
	if (!soc_fm_i2s_capture_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_fm_i2s_capture_dev);
	if (ret != 0) {
		platform_device_put(soc_fm_i2s_capture_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_fm_i2s_awb_capture_driver);
	return ret;
}

static void __exit mtk_soc_fm_i2s_awb_platform_exit(void)
{
	platform_driver_unregister(&mtk_fm_i2s_awb_capture_driver);
}
module_init(mtk_soc_fm_i2s_awb_platform_init);
module_exit(mtk_soc_fm_i2s_awb_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
