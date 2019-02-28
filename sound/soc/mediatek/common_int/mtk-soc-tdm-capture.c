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

/***************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_pcm_capture.c
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio Ul1 data1 uplink
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *-------------------------------------------------------------------------
 *
 *
 *************************************************************************
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

/* information about */
struct afe_mem_control_t *TDM_VUL_Control_context;
static struct snd_dma_buffer *Capture_dma_buf;
static struct audio_digital_i2s *mAudioDigitalI2S;
static bool mCaptureUseSram;

/*
 *    function implementation
 */
static void StartAudioCaptureHardware(struct snd_pcm_substream *substream);
static void StopAudioCaptureHardware(struct snd_pcm_substream *substream);
static int mtk_capture_probe(struct platform_device *pdev);
static int mtk_capture_pcm_close(struct snd_pcm_substream *substream);
static int mtk_afe_capture_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_capture_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_NORMAL_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = UL1_MAX_BUFFER_SIZE,
	.period_bytes_max = UL1_MAX_BUFFER_SIZE,
	.periods_min = UL1_MIN_PERIOD_SIZE,
	.periods_max = UL1_MAX_PERIOD_SIZE,
	.fifo_size = 0,
};

static void StopAudioCaptureHardware(struct snd_pcm_substream *substream)
{
	SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2, false);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2) == false)
		Set2ndI2SInEnable(false);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL, false);

	/* here to set interrupt */
	irq_remove_user(substream,
			irq_request_number(Soc_Aud_Digital_Block_MEM_VUL));

	/* here to turn off digital part */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_I2S0,
			  Soc_Aud_AFE_IO_Block_MEM_VUL);

	EnableAfe(false);
}

static void StartAudioCaptureHardware(struct snd_pcm_substream *substream)
{
	struct audio_digital_i2s m2ndI2SInAttribute;

	memset_io((void *)&m2ndI2SInAttribute, 0, sizeof(m2ndI2SInAttribute));

	m2ndI2SInAttribute.mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
	m2ndI2SInAttribute.mI2S_IN_PAD_SEL = true; /* I2S_IN_FROM_IO_MUX */
	m2ndI2SInAttribute.mI2S_SLAVE = Soc_Aud_I2S_SRC_SLAVE_MODE;
	m2ndI2SInAttribute.mI2S_SAMPLERATE = substream->runtime->rate;
	m2ndI2SInAttribute.mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
	m2ndI2SInAttribute.mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
	if (substream->runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
	    substream->runtime->format == SNDRV_PCM_FORMAT_U32_LE)
		m2ndI2SInAttribute.mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_32BITS;
	else
		m2ndI2SInAttribute.mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_16BITS;

	Set2ndI2SIn(&m2ndI2SInAttribute);

	if (substream->runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
	    substream->runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
		SetMemIfFetchFormatPerSample(
			Soc_Aud_Digital_Block_MEM_VUL,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
				    Soc_Aud_AFE_IO_Block_MEM_VUL);
	} else {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_VUL,
					     AFE_WLEN_16_BIT);
		SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
				    Soc_Aud_AFE_IO_Block_MEM_VUL);
	}

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2) == false) {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2, true);
		Set2ndI2SInEnable(true);
	} else
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_2, true);

	/* here to set interrupt */
	irq_add_user(substream,
		     irq_request_number(Soc_Aud_Digital_Block_MEM_VUL),
		     substream->runtime->rate, substream->runtime->period_size);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_VUL, substream->runtime->rate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL, true);

	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_I2S0,
			  Soc_Aud_AFE_IO_Block_MEM_VUL);

	EnableAfe(true);
}

static int mtk_capture_pcm_prepare(struct snd_pcm_substream *substream)
{
	pr_debug("capture_pcm_prepare substream->rate = %d  substream->channels = %d\n",
		substream->runtime->rate, substream->runtime->channels);
	return 0;
}

static int mtk_capture_alsa_stop(struct snd_pcm_substream *substream)
{
	struct afe_block_t *Vul_Block = &(TDM_VUL_Control_context->rBlock);

	pr_debug("capture_alsa_stop\n");
	StopAudioCaptureHardware(substream);
	Vul_Block->u4DMAReadIdx = 0;
	Vul_Block->u4WriteIdx = 0;
	Vul_Block->u4DataRemained = 0;
	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_VUL, substream);
	return 0;
}

static snd_pcm_uframes_t
mtk_capture_pcm_pointer(struct snd_pcm_substream *substream)
{
	return get_mem_frame_index(substream, TDM_VUL_Control_context,
				   Soc_Aud_Digital_Block_MEM_VUL);
}

static int mtk_capture_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

	runtime->dma_bytes = params_buffer_bytes(hw_params);

	if (AllocateAudioSram(&substream->runtime->dma_addr,
			      &substream->runtime->dma_area,
			      substream->runtime->dma_bytes, substream,
			      params_format(hw_params), false) == 0) {
#if defined(AUD_DEBUG_LOG)
		pr_debug("AllocateAudioSram success\n");
#endif
		SetHighAddr(Soc_Aud_Digital_Block_MEM_VUL, false,
			    substream->runtime->dma_addr);
	} else if (Capture_dma_buf->area) {
#if defined(AUD_DEBUG_LOG)
		pr_debug("%s = %p dma_buf->area = %p dma_buf->addr = 0x%lx\n",
		       __func__, Capture_dma_buf, Capture_dma_buf->area,
		       (long)Capture_dma_buf->addr);
#endif
		runtime->dma_area = Capture_dma_buf->area;
		runtime->dma_addr = Capture_dma_buf->addr;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_VUL, true,
			    runtime->dma_addr);
		mCaptureUseSram = true;
		AudDrv_Emi_Clk_On();
	} else {
		pr_debug("capture_pcm_hw_params snd_pcm_lib_malloc_pages\n");
		ret = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(hw_params));
	}

	set_mem_block(substream, hw_params, TDM_VUL_Control_context,
		      Soc_Aud_Digital_Block_MEM_VUL);
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
	       __func__, substream->runtime->dma_bytes,
	       substream->runtime->dma_area,
	       (long)substream->runtime->dma_addr);
#endif
	return ret;
}

static int mtk_capture_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("capture_pcm_hw_free\n");
	if (Capture_dma_buf->area) {
		if (mCaptureUseSram == true) {
			AudDrv_Emi_Clk_Off();
			mCaptureUseSram = false;
		} else
			freeAudioSram((void *)substream);
		return 0;
	} else
		return snd_pcm_lib_free_pages(substream);
}

/* Conventional and unconventional sample rate supported */
static unsigned int Vul1_supported_sample_rates[] = {
	8000,  11025, 12000, 16000, 22050, 24000,
	32000, 44100, 48000, 88200, 96000, 192000};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(Vul1_supported_sample_rates),
	.list = Vul1_supported_sample_rates,
};

static int mtk_capture_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	AudDrv_Clk_On();

	TDM_VUL_Control_context =
		Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_VUL);

	runtime->hw = mtk_capture_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_capture_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);


	pr_debug("capture_pcm_open runtime rate = %d channels = %d\n",
		 runtime->rate, runtime->channels);
	runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_MMAP_VALID;


	if (ret < 0) {
		pr_err("capture_pcm_close\n");
		mtk_capture_pcm_close(substream);
		return ret;
	}
	pr_debug("capture_pcm_open return\n");
	return 0;
}

static int mtk_capture_pcm_close(struct snd_pcm_substream *substream)
{
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_capture_alsa_start(struct snd_pcm_substream *substream)
{
	pr_debug("capture_alsa_start\n");
	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_VUL, substream);
	StartAudioCaptureHardware(substream);
	return 0;
}

static int mtk_capture_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("capture_pcm_trigger cmd = %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_capture_alsa_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_capture_alsa_stop(substream);
	}
	return -EINVAL;
}

static int mtk_capture_pcm_copy(struct snd_pcm_substream *substream,
				int channel, unsigned long pos,
				void __user *dst, unsigned long count)
{
	return mtk_memblk_copy(substream, channel, pos, dst, count,
			       TDM_VUL_Control_context,
			       Soc_Aud_Digital_Block_MEM_VUL);
}

static void *dummy_page[2];

static struct page *mtk_capture_pcm_page(struct snd_pcm_substream *substream,
					 unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_afe_capture_ops = {
	.open = mtk_capture_pcm_open,
	.close = mtk_capture_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_capture_pcm_hw_params,
	.hw_free = mtk_capture_pcm_hw_free,
	.prepare = mtk_capture_pcm_prepare,
	.trigger = mtk_capture_pcm_trigger,
	.pointer = mtk_capture_pcm_pointer,
	.copy_user = mtk_capture_pcm_copy,
	.page = mtk_capture_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_platform = {
	.ops = &mtk_afe_capture_ops, .probe = mtk_afe_capture_probe,
};

static int mtk_capture_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_TDMRX_PCM);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev, &mtk_soc_platform);
}

static int mtk_afe_capture_probe(struct snd_soc_platform *platform)
{
	pr_debug("afe_capture_probe TODO\n");
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_VUL,
				   UL1_MAX_BUFFER_SIZE);
	Capture_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_VUL);
	mAudioDigitalI2S =
		kzalloc(sizeof(struct audio_digital_i2s), GFP_KERNEL);
	return 0;
}

static int mtk_capture_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_tdm_capture_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_tdm_capture",
	},
	{} };
#endif

static struct platform_driver mtk_afe_capture_driver = {
	.driver = {

			.name = MT_SOC_TDMRX_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_tdm_capture_of_ids,
#endif
		},
	.probe = mtk_capture_probe,
	.remove = mtk_capture_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkafe_capture_dev;
#endif

static int __init mtk_soc_capture_platform_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkafe_capture_dev = platform_device_alloc(MT_SOC_TDMRX_PCM, -1);
	if (!soc_mtkafe_capture_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkafe_capture_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkafe_capture_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_afe_capture_driver);
	return ret;
}
module_init(mtk_soc_capture_platform_init);

static void __exit mtk_soc_platform_exit(void)
{

	platform_driver_unregister(&mtk_afe_capture_driver);
}
module_exit(mtk_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
