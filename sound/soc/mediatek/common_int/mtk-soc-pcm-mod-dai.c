// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_mod_dai.c
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio MOD DAI path
 *
 *
 *------------------------------------------------------------------------------
 *
 *
 ****************************************************************************
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

struct afe_mem_control_t *MOD_DAI_Control_context;
static struct snd_dma_buffer *Capture_dma_buf;
static bool mModDaiUseSram;

/*
 *    function implementation
 */

static int mtk_mod_dai_probe(struct platform_device *pdev);
static int mtk_mod_dai_pcm_close(struct snd_pcm_substream *substream);
static int mtk_afe_mod_dai_component_probe(struct snd_soc_component *component);

static struct snd_pcm_hardware mtk_mod_dai_hardware = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_NORMAL_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = MOD_DAI_MAX_BUFFER_SIZE,
	.period_bytes_max = MOD_DAI_MAX_BUFFER_SIZE,
	.periods_min = MOD_DAI_MIN_PERIOD_SIZE,
	.periods_max = MOD_DAI_MAX_PERIOD_SIZE,
	.fifo_size = 0,
};

static void StopAudioModDaiCaptureHardware(struct snd_pcm_substream *substream)
{
	pr_debug("StopAudioModDaiCaptureHardware\n");

	/*
	 * legacy usagebk97
	 * SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);
	 */

	irq_remove_user(substream,
			irq_request_number(Soc_Aud_Digital_Block_MEM_MOD_DAI));

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_MOD_DAI, false);

	/*
	 * connect phone call DL data to dai and disconnect DL to speaker path
	 */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
			  Soc_Aud_AFE_IO_Block_MEM_MOD_DAI);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
			  Soc_Aud_AFE_IO_Block_MEM_MOD_DAI);

	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH2,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH2,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

	EnableAfe(false);
}

static void StartAudioModDaiCaptureHardware(struct snd_pcm_substream *substream)
{
	pr_debug("StartAudioModDaiCaptureHardware\n");

	if (substream->runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
	    substream->runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
		SetMemIfFetchFormatPerSample(
			Soc_Aud_Digital_Block_MEM_MOD_DAI,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		SetConnectionFormat(AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA,
				    Soc_Aud_AFE_IO_Block_MEM_MOD_DAI);
	} else {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_MOD_DAI,
					     AFE_WLEN_16_BIT);
		SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
				    Soc_Aud_AFE_IO_Block_MEM_MOD_DAI);
	}

	/* here to set interrupt */
	/* legacy
	 * SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE,
	 * substream->runtime->period_size);
	 * SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE,
	 * substream->runtime->rate);
	 * SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, true);
	 */

	SetSampleRate(Soc_Aud_Digital_Block_MEM_MOD_DAI,
		      substream->runtime->rate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_MOD_DAI, true);

	irq_add_user(substream,
		     irq_request_number(Soc_Aud_Digital_Block_MEM_MOD_DAI),
		     substream->runtime->rate, substream->runtime->period_size);

	/*
	 * connect phone call DL data to dai and disconnect DL to speaker path
	 */
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
			  Soc_Aud_AFE_IO_Block_MEM_MOD_DAI);
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
			  Soc_Aud_AFE_IO_Block_MEM_MOD_DAI);

	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL1_CH1,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O_CH4);

	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH2,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH2,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

	EnableAfe(true);
}

static int mtk_mod_dai_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_mod_dai_alsa_stop(struct snd_pcm_substream *substream)
{
	struct afe_block_t *pModDai_Block = &(MOD_DAI_Control_context->rBlock);

	pr_debug("mtk_mod_dai_alsa_stop\n");
	StopAudioModDaiCaptureHardware(substream);
	pModDai_Block->u4DMAReadIdx = 0;
	pModDai_Block->u4WriteIdx = 0;
	pModDai_Block->u4DataRemained = 0;
	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_MOD_DAI, substream);
	return 0;
}

static snd_pcm_uframes_t
mtk_mod_dai_pcm_pointer(struct snd_pcm_substream *substream)
{
	return get_mem_frame_index(substream, MOD_DAI_Control_context,
				   Soc_Aud_Digital_Block_MEM_DAI);
}

static int mtk_mod_dai_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

	if (AllocateAudioSram(&substream->runtime->dma_addr,
			      &substream->runtime->dma_area,
			      substream->runtime->dma_bytes, substream,
			      params_format(hw_params), false) == 0) {
		mModDaiUseSram = false;

	} else {
		substream->runtime->dma_area = Capture_dma_buf->area;
		substream->runtime->dma_addr = Capture_dma_buf->addr;
		mModDaiUseSram = true;
		AudDrv_Emi_Clk_On();
	}
	pr_debug(
		"mtk_mod_dai_pcm_hw_params dma_bytes = %zu dma_area = %p dma_addr = 0x%x\n",
		runtime->dma_bytes, runtime->dma_area,
		(unsigned int)runtime->dma_addr);
	set_mem_block(substream, hw_params, MOD_DAI_Control_context,
		      Soc_Aud_Digital_Block_MEM_MOD_DAI);

	return ret;
}

static int mtk_mod_dai_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_i2s2_adc2_capture_pcm_hw_free\n");
	if (mModDaiUseSram == true) {
		AudDrv_Emi_Clk_Off();
		mModDaiUseSram = false;
	} else
		freeAudioSram((void *)substream);
	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_normal_supported_sample_rates),
	.list = soc_normal_supported_sample_rates,
};

static int mtk_mod_dai_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	AudDrv_Clk_On();
	MOD_DAI_Control_context =
		Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_MOD_DAI);
	runtime->hw = mtk_mod_dai_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_mod_dai_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	pr_debug("mtk_mod_dai_pcm_open runtime rate = %d channels = %d\n",
		 runtime->rate, runtime->channels);

	if (ret < 0) {
		pr_debug("mtk_mod_dai_pcm_close\n");
		mtk_mod_dai_pcm_close(substream);
		return ret;
	}

	pr_debug("mtk_mod_dai_pcm_open return\n");
	return 0;
}

static int mtk_mod_dai_pcm_close(struct snd_pcm_substream *substream)
{
	mModDaiUseSram = false;
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_mod_dai_alsa_start(struct snd_pcm_substream *substream)
{
	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_MOD_DAI, substream);
	StartAudioModDaiCaptureHardware(substream);
	return 0;
}

static int mtk_mod_dai_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("mtk_mod_dai_pcm_trigger cmd = %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_mod_dai_alsa_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_mod_dai_alsa_stop(substream);
	}
	return -EINVAL;
}

static int mtk_mod_dai_pcm_copy(struct snd_pcm_substream *substream,
				int channel,
				unsigned long pos,
				void __user *buf,
				unsigned long bytes)
{
	return mtk_memblk_copy(substream, channel, pos, buf, bytes,
			       MOD_DAI_Control_context,
			       Soc_Aud_Digital_Block_MEM_DAI);
}

static int mtk_mod_dai_pcm_silence(struct snd_pcm_substream *substream,
				   int channel,
				   unsigned long pos,
				   unsigned long bytes)
{
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_mod_dai_pcm_page(struct snd_pcm_substream *substream,
					 unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_afe_mod_dai_ops = {
	.open = mtk_mod_dai_pcm_open,
	.close = mtk_mod_dai_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_mod_dai_pcm_hw_params,
	.hw_free = mtk_mod_dai_pcm_hw_free,
	.prepare = mtk_mod_dai_pcm_prepare,
	.trigger = mtk_mod_dai_pcm_trigger,
	.pointer = mtk_mod_dai_pcm_pointer,
	.copy_user = mtk_mod_dai_pcm_copy,
	.fill_silence = mtk_mod_dai_pcm_silence,
	.page = mtk_mod_dai_pcm_page,
};

static struct snd_soc_component_driver mtk_soc_component = {
	.name = AFE_PCM_NAME,
	.ops = &mtk_afe_mod_dai_ops,
	.probe = mtk_afe_mod_dai_component_probe,
};

static int mtk_mod_dai_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_MOD_DAI_PCM);
	pdev->name = pdev->dev.kobj.name;

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev,
					  &mtk_soc_component,
					  NULL,
					  0);
}

static int mtk_afe_mod_dai_component_probe(struct snd_soc_component *component)
{
	pr_debug("%s\n", __func__);
	AudDrv_Allocate_mem_Buffer(component->dev,
				   Soc_Aud_Digital_Block_MEM_MOD_DAI,
				   MOD_DAI_MAX_BUFFER_SIZE);
	Capture_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_MOD_DAI);
	return 0;
}

static int mtk_mod_dai_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_afe_mod_dai_driver = {
	.driver = {

			.name = MT_SOC_MOD_DAI_PCM, .owner = THIS_MODULE,
		},
	.probe = mtk_mod_dai_probe,
	.remove = mtk_mod_dai_remove,
};

static struct platform_device *soc_mtkafe_mod_dai_dev;

static int __init mtk_soc_mod_dai_platform_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	soc_mtkafe_mod_dai_dev = platform_device_alloc(MT_SOC_MOD_DAI_PCM, -1);
	if (!soc_mtkafe_mod_dai_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkafe_mod_dai_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkafe_mod_dai_dev);
		return ret;
	}
	ret = platform_driver_register(&mtk_afe_mod_dai_driver);
	return ret;
}
module_init(mtk_soc_mod_dai_platform_init);

static void __exit mtk_soc_mod_dai_platform_exit(void)
{
	platform_driver_unregister(&mtk_afe_mod_dai_driver);
}
module_exit(mtk_soc_mod_dai_platform_exit);

MODULE_DESCRIPTION("AFE mod dai module platform driver");
MODULE_LICENSE("GPL");
