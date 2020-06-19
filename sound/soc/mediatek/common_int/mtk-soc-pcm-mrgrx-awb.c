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
 *   mtk_pcm_mrgrx_awb.c
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio mrgrx awb capture
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
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
static struct afe_mem_control_t *Mrgrx_AWB_Control_context;
static struct snd_dma_buffer *Awb_Capture_dma_buf;
static struct snd_dma_buffer *Mrgrx_Awb_Capture_dma_buf;

/*
 *    function implementation
 */
static void StartAudioMrgrxAWBHardware(struct snd_pcm_substream *substream);
static void StopAudioAWBHardware(struct snd_pcm_substream *substream);
static int mtk_mrgrx_awb_probe(struct platform_device *pdev);
static int mtk_mrgrx_awb_pcm_close(struct snd_pcm_substream *substream);
static int mtk_afe_mrgrx_awb_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_mgrrx_awb_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_STD_MT_FMTS,
	.rates = SOC_NORMAL_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = MRGRX_MAX_BUFFER_SIZE,
	.period_bytes_max = MRGRX_MAX_BUFFER_SIZE,
	.periods_min = MRGRX_MIN_PERIOD_SIZE,
	.periods_max = MRGRX_MAX_PERIOD_SIZE,
	.fifo_size = 0,
};

static void StopAudioAWBHardware(struct snd_pcm_substream *substream)
{
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB, false);

	/* here to set interrupt */
	irq_remove_user(substream,
			irq_request_number(Soc_Aud_Digital_Block_MEM_AWB));

	/* here to turn off digital part */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MRG_I2S_IN,
			  Soc_Aud_AFE_IO_Block_MEM_AWB);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT, false);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT) == false)
		SetMrgI2SEnable(false, substream->runtime->rate);

	EnableAfe(false);
}

static void StartAudioMrgrxAWBHardware(struct snd_pcm_substream *substream)
{
	/* here to set interrupt */
	irq_add_user(
		substream, irq_request_number(Soc_Aud_Digital_Block_MEM_AWB),
		substream->runtime->rate, substream->runtime->period_size >> 1);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_AWB, substream->runtime->rate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB, true);

	/* here to turn off digital part */
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MRG_I2S_IN,
			  Soc_Aud_AFE_IO_Block_MEM_AWB);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT) == false) {
		/* set merge interface */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT, true);
		SetMrgI2SEnable(true, substream->runtime->rate);
	} else
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT, true);

	EnableAfe(true);
}

static int mtk_mrgrx_awb_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_mrgrx_awb_alsa_stop(struct snd_pcm_substream *substream)
{
	pr_debug("mrgrx_awb_alsa_stop\n");
	StopAudioAWBHardware(substream);
	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_AWB, substream);

	return 0;
}

static snd_pcm_uframes_t
mtk_awb_pcm_pointer(struct snd_pcm_substream *substream)
{
	return get_mem_frame_index(substream, Mrgrx_AWB_Control_context,
				   Soc_Aud_Digital_Block_MEM_AWB);
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
		pr_debug("mgrrx_awb_pcm_hw_params Awb_Capture_dma_buf->area\n");
		runtime->dma_bytes = params_buffer_bytes(hw_params);
		runtime->dma_area = Awb_Capture_dma_buf->area;
		runtime->dma_addr = Awb_Capture_dma_buf->addr;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_AWB, true,
			    runtime->dma_addr);
	} else {
		pr_debug("mgrrx_awb_pcm_hw_params snd_pcm_lib_malloc_pages\n");
		ret = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(hw_params));
	}
	set_mem_block(substream, hw_params, Mrgrx_AWB_Control_context,
		      Soc_Aud_Digital_Block_MEM_AWB);

	pr_debug("dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		substream->runtime->dma_bytes, substream->runtime->dma_area,
		(long)substream->runtime->dma_addr);
	return ret;
}

static int mtk_mrgrx_capture_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("mrgrx_capture_pcm_hw_free\n");
	if (Awb_Capture_dma_buf->area)
		return 0;
	else
		return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_hw_constraint_list mrgrx_awb_constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_normal_supported_sample_rates),
	.list = soc_normal_supported_sample_rates,
};

static int mtk_mrgrx_awb_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_debug("mrgrx_awb_pcm_open\n");
	Mrgrx_AWB_Control_context =
		Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_AWB);
	runtime->hw = mtk_mgrrx_awb_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_mgrrx_awb_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &mrgrx_awb_constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		pr_debug("SNDRV_PCM_STREAM_CAPTURE\n");
	else
		return -1;
	/* here open audio clocks */
	AudDrv_Clk_On();
	AudDrv_Emi_Clk_On();

	if (ret < 0) {
		pr_err("mrgrx_awb_pcm_close\n");
		mtk_mrgrx_awb_pcm_close(substream);
		return ret;
	}
	pr_debug("mrgrx_awb_pcm_open return\n");
	return 0;
}

static int mtk_mrgrx_awb_pcm_close(struct snd_pcm_substream *substream)
{
	AudDrv_Emi_Clk_Off();
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_mrgrx_awb_alsa_start(struct snd_pcm_substream *substream)
{
	pr_debug("mrgrx_awb_alsa_start\n");
	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_AWB, substream);
	StartAudioMrgrxAWBHardware(substream);
	return 0;
}

static int mtk_capture_mrgrx_pcm_trigger(struct snd_pcm_substream *substream,
					 int cmd)
{
	pr_debug("capture_mrgrx_pcm_trigger cmd = %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_mrgrx_awb_alsa_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_mrgrx_awb_alsa_stop(substream);
	}
	return -EINVAL;
}

static int mtk_mrgrx_awb_pcm_copy(struct snd_pcm_substream *substream,
				  int channel, unsigned long pos,
				  void __user *dst, unsigned long count)
{
	return mtk_memblk_copy(substream, channel, pos, dst, count,
			       Mrgrx_AWB_Control_context,
			       Soc_Aud_Digital_Block_MEM_AWB);
}

static void *dummy_page[2];

static struct page *
mtk_mrgrx_capture_pcm_page(struct snd_pcm_substream *substream,
			   unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_mrgrx_awb_ops = {
	.open = mtk_mrgrx_awb_pcm_open,
	.close = mtk_mrgrx_awb_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_mgrrx_awb_pcm_hw_params,
	.hw_free = mtk_mrgrx_capture_pcm_hw_free,
	.prepare = mtk_mrgrx_awb_pcm_prepare,
	.trigger = mtk_capture_mrgrx_pcm_trigger,
	.pointer = mtk_awb_pcm_pointer,
	.copy_user = mtk_mrgrx_awb_pcm_copy,
	.page = mtk_mrgrx_capture_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_platform = {
	.ops = &mtk_mrgrx_awb_ops, .probe = mtk_afe_mrgrx_awb_probe,
};

static int mtk_mrgrx_awb_probe(struct platform_device *pdev)
{
	pr_debug("mrgrx_awb_probe\n");

	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", MT_SOC_MRGRX_AWB_PCM);
		pdev->name = pdev->dev.kobj.name;
	} else {
		pr_debug("%s(), pdev->dev.of_node = NULL!!!\n", __func__);
	}

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev, &mtk_soc_platform);
}

static int mtk_afe_mrgrx_awb_probe(struct snd_soc_platform *platform)
{
	pr_debug("afe_mrgrx_awb_probe\n");
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_AWB,
				   MRGRX_MAX_BUFFER_SIZE);
	Awb_Capture_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_AWB);
	if (Mrgrx_Awb_Capture_dma_buf == NULL) {
		Mrgrx_Awb_Capture_dma_buf =
			kzalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
		if (Mrgrx_Awb_Capture_dma_buf != NULL) {
			Mrgrx_Awb_Capture_dma_buf->area = dma_alloc_coherent(
				platform->dev, MRGRX_MAX_BUFFER_SIZE,
				&Mrgrx_Awb_Capture_dma_buf->addr, GFP_KERNEL);
			if (Mrgrx_Awb_Capture_dma_buf->area)
				Mrgrx_Awb_Capture_dma_buf->bytes =
					MRGRX_MAX_BUFFER_SIZE;
		}
	}

	return 0;
}

static int mtk_mrgrx_awb_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_mrgrx_awb_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_mrgrx_awb",
	},
	{} };
#endif

static struct platform_driver mtk_mrgrx_awb_capture_driver = {
	.driver = {

			.name = MT_SOC_MRGRX_AWB_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_mrgrx_awb_of_ids,
#endif
		},
	.probe = mtk_mrgrx_awb_probe,
	.remove = mtk_mrgrx_awb_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mrgrx_capture_dev;
#endif

static int __init mtk_soc_mrgrx_awb_platform_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mrgrx_capture_dev = platform_device_alloc(MT_SOC_MRGRX_AWB_PCM, -1);
	if (!soc_mrgrx_capture_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mrgrx_capture_dev);
	if (ret != 0) {
		platform_device_put(soc_mrgrx_capture_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_mrgrx_awb_capture_driver);
	return ret;
}

static void __exit mtk_soc_mrgrx_awb_platform_exit(void)
{
	platform_driver_unregister(&mtk_mrgrx_awb_capture_driver);
}
module_init(mtk_soc_mrgrx_awb_platform_init);
module_exit(mtk_soc_mrgrx_awb_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
