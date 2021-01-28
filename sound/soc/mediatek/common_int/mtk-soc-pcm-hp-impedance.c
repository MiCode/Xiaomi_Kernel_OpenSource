// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_hp_impedance.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio dl1 impedance setting
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
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"

#include "mtk-soc-codec-63xx.h"

#include <linux/dma-mapping.h>
#include <linux/time.h>

/* #define SUPPORT_GOOGLE_LINEOUT */

static struct afe_mem_control_t *pHp_impedance_MemControl;

static struct snd_dma_buffer *Dl1_Hp_Playback_dma_buf;

/*
 *    function implementation
 */

static int mtk_soc_hp_impedance_probe(struct platform_device *pdev);
static int mtk_soc_pcm_hp_impedance_close(struct snd_pcm_substream *substream);
static int mtk_asoc_dhp_impedance_component_probe(struct snd_soc_component *component);

static struct snd_pcm_hardware mtk_pcm_hp_impedance_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static snd_pcm_uframes_t
mtk_pcm_hp_impedance_pointer(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_pcm_hp_impedance_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	/* runtime->dma_bytes has to be set manually to allow mmap */
	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
	substream->runtime->dma_area = Dl1_Hp_Playback_dma_buf->area;
	substream->runtime->dma_addr = Dl1_Hp_Playback_dma_buf->addr;
	SetHighAddr(Soc_Aud_Digital_Block_MEM_DL1, true,
		    substream->runtime->dma_addr);
	set_mem_block(substream, hw_params, pHp_impedance_MemControl,
		      Soc_Aud_Digital_Block_MEM_DL1);
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
	       __func__, substream->runtime->dma_bytes,
	       substream->runtime->dma_area,
	       (long)substream->runtime->dma_addr);
#endif
	return ret;
}

static int mtk_pcm_hp_impedance_hw_free(struct snd_pcm_substream *substream)
{
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s()\n", __func__);
#endif
	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_hp_sample_rates = {
		.count = ARRAY_SIZE(soc_high_supported_sample_rates),
		.list = soc_high_supported_sample_rates,
		.mask = 0,
};

static int mtk_pcm_hp_impedance_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s()\n", __func__);
#endif
	AudDrv_Clk_On();
	AudDrv_Emi_Clk_On();
	pHp_impedance_MemControl =
		Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
	runtime->hw = mtk_pcm_hp_impedance_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hp_impedance_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(
		runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
		&constraints_hp_sample_rates);

	if (ret < 0) {
		pr_err("mtk_soc_pcm_hp_impedance_close\n");
		mtk_soc_pcm_hp_impedance_close(substream);
		return ret;
	}
	return 0;
}

bool mPrepareDone;
static int mtk_pcm_hp_impedance_prepare(struct snd_pcm_substream *substream)
{
	bool mI2SWLen;
	struct snd_pcm_runtime *runtime = substream->runtime;
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s(), mPrepareDone = %d\n", __func__, mPrepareDone);
#endif
	if (mPrepareDone == false) {
		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL1,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		}
		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, runtime->rate);
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) ==
		    false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
					    true);
			SetI2SDacOut(substream->runtime->rate, false, mI2SWLen);
			SetI2SDacEnable(true);
		} else
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
					    true);

		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_MEM_DL1,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC);

		SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
		SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

		EnableAfe(true);
		mPrepareDone = true;
	}
	return 0;
}

static int mtk_soc_pcm_hp_impedance_close(struct snd_pcm_substream *substream)
{
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	if (mPrepareDone == true) {
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MEM_DL1,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC);

		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);

		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);
		EnableAfe(false);
		mPrepareDone = false;
	}
	AudDrv_Emi_Clk_Off();
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_pcm_hp_impedance_trigger(struct snd_pcm_substream *substream,
					int cmd)
{
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s(), cmd = %d\n", __func__, cmd);
#endif
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}
	return -EINVAL;
}

static int mtk_pcm_hp_impedance_copy(struct snd_pcm_substream *substream,
				     int channel,
				     unsigned long pos,
				     void __user *buf,
				     unsigned long bytes)
{
	return 0;
}

static int mtk_pcm_hp_impedance_silence(struct snd_pcm_substream *substream,
					int channel,
					unsigned long pos,
					unsigned long bytes)
{
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *
mtk_pcm_hp_impedance_page(struct snd_pcm_substream *substream,
			  unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_hp_impedance_ops = {
	.open = mtk_pcm_hp_impedance_open,
	.close = mtk_soc_pcm_hp_impedance_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_hp_impedance_params,
	.hw_free = mtk_pcm_hp_impedance_hw_free,
	.prepare = mtk_pcm_hp_impedance_prepare,
	.trigger = mtk_pcm_hp_impedance_trigger,
	.pointer = mtk_pcm_hp_impedance_pointer,
	.copy_user = mtk_pcm_hp_impedance_copy,
	.fill_silence = mtk_pcm_hp_impedance_silence,
	.page = mtk_pcm_hp_impedance_page,
};

static const struct snd_soc_component_driver mtk_soc_component = {
	.name = AFE_PCM_NAME,
	.ops = &mtk_hp_impedance_ops,
	.probe = mtk_asoc_dhp_impedance_component_probe,
};

static int mtk_soc_hp_impedance_probe(struct platform_device *pdev)
{
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_HP_IMPEDANCE_PCM);
	pdev->name = pdev->dev.kobj.name;
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
#endif
	return snd_soc_register_component(&pdev->dev,
					  &mtk_soc_component,
					  NULL,
					  0);
}

static int mtk_asoc_dhp_impedance_component_probe(struct snd_soc_component *component)
{
#if defined(AUD_DEBUG_LOG)
	pr_debug("mtk_asoc_dhp_impedance_probe\n");
#endif
	/* allocate dram */
	AudDrv_Allocate_mem_Buffer(component->dev,
				   Soc_Aud_Digital_Block_MEM_DL1,
				   Dl1_MAX_BUFFER_SIZE);

	Dl1_Hp_Playback_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);

	return 0;
}

static int mtk_hp_impedance_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id Mt_soc_pcm_hp_impedance_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_hp_impedance",
	},
	{} };
#endif

static struct platform_driver mtk_hp_impedance_driver = {
	.driver = {

			.name = MT_SOC_HP_IMPEDANCE_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = Mt_soc_pcm_hp_impedance_of_ids,
#endif
		},
	.probe = mtk_soc_hp_impedance_probe,
	.remove = mtk_hp_impedance_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_hp_impedance_dev;
#endif

static int __init mtk_soc_hp_impedance_platform_init(void)
{
	int ret;

#if defined(AUD_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
#ifndef CONFIG_OF
	soc_mtk_hp_impedance_dev =
		platform_device_alloc(MT_SOC_HP_IMPEDANCE_PCM, -1);
	if (!soc_mtk_hp_impedance_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_hp_impedance_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_hp_impedance_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_hp_impedance_driver);
	return ret;
}
module_init(mtk_soc_hp_impedance_platform_init);

static void __exit mtk_soc_hp_impedance_platform_exit(void)
{
	platform_driver_unregister(&mtk_hp_impedance_driver);
}
module_exit(mtk_soc_hp_impedance_platform_exit);

MODULE_DESCRIPTION("hp impedance module platform driver");
MODULE_LICENSE("GPL");
