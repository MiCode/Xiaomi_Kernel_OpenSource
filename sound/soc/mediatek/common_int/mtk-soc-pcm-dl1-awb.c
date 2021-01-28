// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_pcm_dl1_awb.c
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio dl1 to  awb capture
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
static struct afe_mem_control_t *Dl1_AWB_Control_context;
static struct snd_dma_buffer *Awb_Capture_dma_buf;
static int deep_buffer_mem_blk_io;

/*
 *    function implementation
 */
static void StartAudioDl1AWBHardware(struct snd_pcm_substream *substream);
static void StopAudioDl1AWBHardware(struct snd_pcm_substream *substream);
static int mtk_dl1_awb_probe(struct platform_device *pdev);
static int mtk_dl1_awb_pcm_close(struct snd_pcm_substream *substream);
static int mtk_afe_dl1_awb_component_probe(struct snd_soc_component *component);

#define MAX_PCM_DEVICES 4
#define MAX_PCM_SUBSTREAMS 128
#define MAX_MIDI_DEVICES

static struct snd_pcm_hardware mtk_dl1_awb_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_STD_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = AWB_MAX_BUFFER_SIZE,
	.period_bytes_max = AWB_MAX_BUFFER_SIZE,
	.periods_min = AWB_MIN_PERIOD_SIZE,
	.periods_max = AWB_MAX_PERIOD_SIZE,
	.fifo_size = 0,
};

static void StopAudioDl1AWBHardware(struct snd_pcm_substream *substream)
{

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB, false);

	/* here to set interrupt */
	irq_remove_user(substream,
			irq_request_number(Soc_Aud_Digital_Block_MEM_AWB));

	/* here to turn off digital part */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_MEM_AWB);

	/* for DL2 echoref*/
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL2,
			  Soc_Aud_AFE_IO_Block_MEM_AWB);

	EnableAfe(false);
}

static void StartAudioDl1AWBHardware(struct snd_pcm_substream *substream)
{

	/* here to set interrupt */
	irq_add_user(
		substream, irq_request_number(Soc_Aud_Digital_Block_MEM_AWB),
		substream->runtime->rate, substream->runtime->period_size >> 1);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_AWB, substream->runtime->rate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB, true);
	if (deep_buffer_mem_blk_io >= 0) {
		pr_debug(
			"%s(), deep_buffer_mem_blk_io %d, set intercon between awb and deep buffer output\n",
			__func__, deep_buffer_mem_blk_io);
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  deep_buffer_mem_blk_io,
				  Soc_Aud_AFE_IO_Block_MEM_AWB);
	}

	/* here to turn on digital part */
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_MEM_AWB);

	/* for DL2 echoref*/
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL2,
			  Soc_Aud_AFE_IO_Block_MEM_AWB);

	EnableAfe(true);
}

static int mtk_dl1_awb_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_dl1_awb_alsa_stop(struct snd_pcm_substream *substream)
{
	pr_debug("%s()\n", __func__);
	StopAudioDl1AWBHardware(substream);
	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_AWB, substream);
	return 0;
}

static snd_pcm_uframes_t
mtk_dl1_awb_pcm_pointer(struct snd_pcm_substream *substream)
{
	return get_mem_frame_index(substream, Dl1_AWB_Control_context,
				   Soc_Aud_Digital_Block_MEM_AWB);
}

static int mtk_dl1_awb_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;


	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

	if (Awb_Capture_dma_buf->area) {
		pr_debug("%s() Awb_Capture_dma_buf->area\n", __func__);
		runtime->dma_bytes = params_buffer_bytes(hw_params);
		runtime->dma_area = Awb_Capture_dma_buf->area;
		runtime->dma_addr = Awb_Capture_dma_buf->addr;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_AWB, true,
			    runtime->dma_addr);
	} else {
		ret = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(hw_params));
	}
	set_mem_block(substream, hw_params, Dl1_AWB_Control_context,
		      Soc_Aud_Digital_Block_MEM_AWB);

	pr_debug("%s() dma_bytes = %zu dma_area = %p dma_addr = 0x%lx hw.buffer_bytes_max = %zu\n",
		 __func__, runtime->dma_bytes, runtime->dma_area,
		 (long)runtime->dma_addr, runtime->hw.buffer_bytes_max);

	return ret;
}

static int mtk_dl1_capture_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s()\n", __func__);
	if (Awb_Capture_dma_buf->area)
		return 0;
	else
		return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_hw_constraint_list dl1_awb_constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
};

static int mtk_dl1_awb_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	Dl1_AWB_Control_context =
		Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_AWB);
	runtime->hw = mtk_dl1_awb_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_dl1_awb_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &dl1_awb_constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	/* here open audio clocks */
	AudDrv_Clk_On();

	if (ret < 0) {
		pr_debug("mtk_dl1_awb_pcm_close\n");
		mtk_dl1_awb_pcm_close(substream);
		return ret;
	}
	AudDrv_Emi_Clk_On();
	return 0;
}

static int mtk_dl1_awb_pcm_close(struct snd_pcm_substream *substream)
{
	AudDrv_Emi_Clk_Off();
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_dl1_awb_alsa_start(struct snd_pcm_substream *substream)
{
	pr_debug("%s()\n", __func__);
	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_AWB, substream);
	StartAudioDl1AWBHardware(substream);
	return 0;
}

static int mtk_dl1_awb_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_dl1_awb_alsa_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_dl1_awb_alsa_stop(substream);
	}
	return -EINVAL;
}

static int mtk_dl1_awb_pcm_copy(struct snd_pcm_substream *substream,
				int channel, unsigned long pos,
				void __user *dst, unsigned long count)
{
	return mtk_memblk_copy(substream, channel, pos, dst, count,
			       Dl1_AWB_Control_context,
			       Soc_Aud_Digital_Block_MEM_AWB);
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
mtk_dl1_capture_pcm_page(struct snd_pcm_substream *substream,
			 unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_dl1_awb_ops = {
	.open = mtk_dl1_awb_pcm_open,
	.close = mtk_dl1_awb_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_dl1_awb_pcm_hw_params,
	.hw_free = mtk_dl1_capture_pcm_hw_free,
	.prepare = mtk_dl1_awb_pcm_prepare,
	.trigger = mtk_dl1_awb_pcm_trigger,
	.pointer = mtk_dl1_awb_pcm_pointer,
	.copy_user = mtk_dl1_awb_pcm_copy,
	.fill_silence = mtk_capture_pcm_silence,
	.page = mtk_dl1_capture_pcm_page,
};

static struct snd_soc_component_driver mtk_soc_component = {
	.name = AFE_PCM_NAME,
	.ops = &mtk_dl1_awb_ops,
	.probe = mtk_afe_dl1_awb_component_probe,
};

static int mtk_dl1_awb_probe(struct platform_device *pdev)
{
	deep_buffer_mem_blk_io =
		get_usage_digital_block_io(AUDIO_USAGE_DEEPBUFFER_PLAYBACK);
	if (deep_buffer_mem_blk_io < 0) {
		pr_debug(
			"%s(), invalid mem blk io %d, no need to set intercon between awb and deep buffer output\n",
			__func__, deep_buffer_mem_blk_io);
	}
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	if (pdev->dev.dma_mask == NULL)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_DL1_AWB_PCM);
	pdev->name = pdev->dev.kobj.name;

	pr_debug("%s(): dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev,
					  &mtk_soc_component,
					  NULL,
					  0);

}

static int mtk_afe_dl1_awb_component_probe(struct snd_soc_component *component)
{
	pr_debug("%s()\n", __func__);
	AudDrv_Allocate_mem_Buffer(component->dev, Soc_Aud_Digital_Block_MEM_AWB,
				   AWB_MAX_BUFFER_SIZE);
	Awb_Capture_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_AWB);
	return 0;
}

static int mtk_dl1_awb_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dl1_awb_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_dl1_awb",
	},
	{} };
#endif

static struct platform_driver mtk_dl1_awb_capture_driver = {
	.driver = {

			.name = MT_SOC_DL1_AWB_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_dl1_awb_of_ids,
#endif
		},
	.probe = mtk_dl1_awb_probe,
	.remove = mtk_dl1_awb_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_dl1_awb_capture_dev;
#endif

static int __init mtk_soc_dl1_awb_platform_init(void)
{
	int ret = 0;

	pr_debug("%s()\n", __func__);
#ifndef CONFIG_OF
	soc_dl1_awb_capture_dev = platform_device_alloc(MT_SOC_DL1_AWB_PCM, -1);
	if (!soc_dl1_awb_capture_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_dl1_awb_capture_dev);
	if (ret != 0) {
		platform_device_put(soc_dl1_awb_capture_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_dl1_awb_capture_driver);
	return ret;
}

static void __exit mtk_soc_dl1_awb_platform_exit(void)
{
	platform_driver_unregister(&mtk_dl1_awb_capture_driver);
}
module_init(mtk_soc_dl1_awb_platform_init);
module_exit(mtk_soc_dl1_awb_platform_exit);

MODULE_DESCRIPTION("DL1 AWB module platform driver");
MODULE_LICENSE("GPL");
