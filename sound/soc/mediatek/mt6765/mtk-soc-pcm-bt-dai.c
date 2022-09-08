// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_pcm_bt_dai.c
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio bt to dai capture
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

#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include <linux/dma-mapping.h>

/* information about */
static struct afe_mem_control_t *Bt_Dai_Control_context;
static struct snd_dma_buffer *Bt_Dai_Capture_dma_buf;

static DEFINE_SPINLOCK(auddrv_BTDaiInCtl_lock);

/*
 *    function implementation
 */
static void StartAudioBtDaiHardware(struct snd_pcm_substream *substream);
static void StopAudioBtDaiHardware(struct snd_pcm_substream *substream);
static int mtk_bt_dai_probe(struct platform_device *pdev);
static int mtk_bt_dai_pcm_close(struct snd_soc_component *component,
				struct snd_pcm_substream *substream);
static int mtk_asoc_bt_dai_component_probe(struct snd_soc_component *component);

static struct snd_pcm_hardware mtk_btdai_hardware = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED),
	.formats = SND_SOC_STD_MT_FMTS,
	.rates = SOC_NORMAL_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = BT_DAI_MAX_BUFFER_SIZE,
	.period_bytes_max = BT_DAI_MAX_BUFFER_SIZE,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static void StopAudioBtDaiHardware(struct snd_pcm_substream *substream)
{
	pr_debug("StopAudioBtDaiHardware\n");

	/* here to set interrupt */
	irq_remove_user(substream,
			irq_request_number(Soc_Aud_Digital_Block_MEM_DAI));

	/* here to turn off digital part */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_DAI_BT_IN,
			  Soc_Aud_AFE_IO_Block_MEM_DAI);

	EnableAfe(false);
}

static bool SetVoipDAIBTAttribute(int sample_rate)
{
	struct audio_digital_dai_bt daibt_attribute;

	memset_io((void *)&daibt_attribute, 0, sizeof(daibt_attribute));

#if 0 /* temp for merge only support */
	daibt_attribute.mUSE_MRGIF_INPUT = Soc_Aud_BT_DAI_INPUT_FROM_BT;
#else
	daibt_attribute.mUSE_MRGIF_INPUT = Soc_Aud_BT_DAI_INPUT_FROM_MGRIF;
#endif
	daibt_attribute.mDAI_BT_MODE = (sample_rate == 8000)
					       ? Soc_Aud_DATBT_MODE_Mode8K
					       : Soc_Aud_DATBT_MODE_Mode16K;
	daibt_attribute.mDAI_DEL =
		Soc_Aud_DAI_DEL_HighWord; /* suggest always HighWord */
	daibt_attribute.mBT_LEN = 0;
	daibt_attribute.mDATA_RDY = true;
	daibt_attribute.mBT_SYNC = Soc_Aud_BTSYNC_Short_Sync;
	daibt_attribute.mBT_ON = true;
	daibt_attribute.mDAIBT_ON = false;
	SetDaiBt(&daibt_attribute);
	return true;
}

static void StartAudioBtDaiHardware(struct snd_pcm_substream *substream)
{
	/* here to set interrupt */
	irq_add_user(substream,
		     irq_request_number(Soc_Aud_Digital_Block_MEM_DAI),
		     substream->runtime->rate, substream->runtime->period_size);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_DAI, substream->runtime->rate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI, true);

	/* here to turn off digital part */
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_DAI_BT_IN,
			  Soc_Aud_AFE_IO_Block_MEM_DAI);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false) {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
		SetVoipDAIBTAttribute(substream->runtime->rate);
		SetDaiBtEnable(true);
	} else {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
	}

	EnableAfe(true);
}

static int mtk_bt_dai_pcm_prepare(struct snd_soc_component *component,
				  struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_bt_dai_alsa_stop(struct snd_pcm_substream *substream)
{
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI, false);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, false);
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false)
		SetDaiBtEnable(false);

	StopAudioBtDaiHardware(substream);
	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DAI, substream);
	return 0;
}

static snd_pcm_uframes_t
mtk_bt_dai_pcm_pointer(struct snd_soc_component *component,
		       struct snd_pcm_substream *substream)
{
	struct afe_block_t *Dai_Block = &(Bt_Dai_Control_context->rBlock);
	kal_uint32 Frameidx = 0;

	/* get total bytes to copy */
	Frameidx = audio_bytes_to_frame(substream, Dai_Block->u4WriteIdx);
	return Frameidx;
}

static int mtk_bt_dai_pcm_hw_params(struct snd_soc_component *component,
				    struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

	if (Bt_Dai_Capture_dma_buf->area) {
		pr_debug("Bt_Dai_Capture_dma_buf->area\n");
		runtime->dma_bytes = params_buffer_bytes(hw_params);
		runtime->dma_area = Bt_Dai_Capture_dma_buf->area;
		runtime->dma_addr = Bt_Dai_Capture_dma_buf->addr;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DAI, true,
			    runtime->dma_addr);
	} else {
		pr_debug("snd_pcm_lib_malloc_pages\n");
		ret = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(hw_params));
	}
	pr_debug("dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		runtime->dma_bytes, runtime->dma_area, (long)runtime->dma_addr);

	set_mem_block(substream, hw_params, Bt_Dai_Control_context,
		      Soc_Aud_Digital_Block_MEM_DAI);

	AudDrv_Emi_Clk_On();

	return ret;
}

static int mtk_bt_dai_capture_pcm_hw_free(struct snd_soc_component *component,
					  struct snd_pcm_substream *substream)
{
	AudDrv_Emi_Clk_Off();

	if (Bt_Dai_Capture_dma_buf->area)
		return 0;
	else
		return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_hw_constraint_list bt_dai_constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_voice_supported_sample_rates),
	.list = soc_voice_supported_sample_rates,
};

static int mtk_bt_dai_pcm_open(struct snd_soc_component *component,
			       struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_debug("stream %d\n", substream->stream);

	Bt_Dai_Control_context =
		Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DAI);
	runtime->hw = mtk_btdai_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_btdai_hardware,
	       sizeof(struct snd_pcm_hardware));
	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &bt_dai_constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_debug("failed\n");

	AudDrv_Clk_On();

	/* print for hw pcm information */
	runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;

	if (ret < 0) {
		pr_err("bt_dai_pcm_close\n");
		mtk_bt_dai_pcm_close(component, substream);
		return ret;
	}
	return 0;
}

static int mtk_bt_dai_pcm_close(struct snd_soc_component *component,
				struct snd_pcm_substream *substream)
{
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_bt_dai_alsa_start(struct snd_pcm_substream *substream)
{
	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DAI, substream);
	StartAudioBtDaiHardware(substream);
	return 0;
}

static int mtk_bt_dai_pcm_trigger(struct snd_soc_component *component,
				  struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_bt_dai_alsa_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_bt_dai_alsa_stop(substream);
	}
	return -EINVAL;
}

static bool CheckNullPointer(void *pointer)
{
	if (pointer == NULL) {
		pr_err("%s(), pointer = NULL\n", __func__);
		return true;
	}
	return false;
}

static int mtk_bt_dai_pcm_copy(struct snd_soc_component *component,
			       struct snd_pcm_substream *substream, int channel,
			       unsigned long pos, void __user *buf,
			       unsigned long bytes)
{
	struct afe_mem_control_t *pDAI_MEM_ConTrol = NULL;
	struct afe_block_t *Dai_Block = NULL;
	char *Read_Data_Ptr = (char *)buf;
	ssize_t DMA_Read_Ptr = 0, read_size = 0, read_count = 0;
	unsigned long flags;
	unsigned int count = 0;

#if defined(AUD_DEBUG_LOG)
	pr_debug("%s(), pos = %lu, bytes = %lu\n", __func__, pos, bytes);
#endif
	/* get total bytes to copy */
	count = word_size_align(bytes);

	/* check which memif nned to be write */
	pDAI_MEM_ConTrol = Bt_Dai_Control_context;
	Dai_Block = &(pDAI_MEM_ConTrol->rBlock);

	if (pDAI_MEM_ConTrol == NULL) {
		pr_err("cannot find MEM control !!!!!!!\n");
		msleep(50);
		return 0;
	}

	if (Dai_Block->u4BufferSize <= 0) {
		msleep(50);
		return 0;
	}

	if (CheckNullPointer((void *)Dai_Block->pucVirtBufAddr)) {
		pr_err("CheckNullPointer  pucVirtBufAddr = %p\n",
		       Dai_Block->pucVirtBufAddr);
		return 0;
	}

	spin_lock_irqsave(&auddrv_BTDaiInCtl_lock, flags);
	if (Dai_Block->u4DataRemained > Dai_Block->u4BufferSize) {
		pr_warn("%s(), u4DataRemained 0x%x > u4BufferSize 0x%x",
			__func__, Dai_Block->u4DataRemained,
			Dai_Block->u4BufferSize);
		Dai_Block->u4DataRemained = 0;
		Dai_Block->u4DMAReadIdx = Dai_Block->u4WriteIdx;
	}

	if (count > Dai_Block->u4DataRemained)
		read_size = Dai_Block->u4DataRemained;
	else
		read_size = count;

	DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
	spin_unlock_irqrestore(&auddrv_BTDaiInCtl_lock, flags);

	if (DMA_Read_Ptr + read_size < Dai_Block->u4BufferSize) {
		if (DMA_Read_Ptr != Dai_Block->u4DMAReadIdx) {
			pr_warn("%s 1, rsize:%zu, Remained:0x%x,Read_Ptr:%zu,DIdx:%x\n",
				__func__, read_size, Dai_Block->u4DataRemained,
				DMA_Read_Ptr, Dai_Block->u4DMAReadIdx);
		}

		if (copy_to_user((void __user *)Read_Data_Ptr,
				 (Dai_Block->pucVirtBufAddr + DMA_Read_Ptr),
				 read_size)) {

			pr_err("%s Fail 1 copy to user Read_Data_Ptr:%p, pucVirtBufAddr:%p, u4DMAReadIdx:0x%x",
			       __func__, Read_Data_Ptr,
			       Dai_Block->pucVirtBufAddr,
			       Dai_Block->u4DMAReadIdx);
			pr_err("%s Fail 1 copy to user DMA_Read_Ptr:%zu,read_size:%zu",
			       __func__, DMA_Read_Ptr, read_size);
			return 0;
		}

		read_count += read_size;
		spin_lock(&auddrv_BTDaiInCtl_lock);
		Dai_Block->u4DataRemained -= read_size;
		Dai_Block->u4DMAReadIdx += read_size;
		Dai_Block->u4DMAReadIdx %= Dai_Block->u4BufferSize;
		DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
		spin_unlock(&auddrv_BTDaiInCtl_lock);

		Read_Data_Ptr += read_size;
		count -= read_size;
#if defined(AUD_DEBUG_LOG)
		pr_debug(
			"%s f 1,size:%zd,RIdx:%x,WIdx:%x,Remain%x\n",
			__func__, read_size, Dai_Block->u4DMAReadIdx,
			Dai_Block->u4WriteIdx, Dai_Block->u4DataRemained);
#endif
	}

	else {
		unsigned int size_1 = Dai_Block->u4BufferSize - DMA_Read_Ptr;
		unsigned int size_2 = read_size - size_1;

		if (DMA_Read_Ptr != Dai_Block->u4DMAReadIdx) {
			pr_warn("%s 2, read_size1:0x%x,DataRemained:0x%x, DMA_Read_Ptr:%zu, DMAReadIdx:0x%x \r\n",
				__func__, size_1, Dai_Block->u4DataRemained,
				DMA_Read_Ptr, Dai_Block->u4DMAReadIdx);
		}
		if (copy_to_user((void __user *)Read_Data_Ptr,
				 (Dai_Block->pucVirtBufAddr + DMA_Read_Ptr),
				 size_1)) {

			pr_warn("%s Fail 2 copy to user Ptr:%p,VirtAddr:%p, ReadIdx:0x%x, Read_Ptr:%zu,read_size:%zu",
				__func__, Read_Data_Ptr,
				Dai_Block->pucVirtBufAddr,
				Dai_Block->u4DMAReadIdx, DMA_Read_Ptr,
				read_size);
			return 0;
		}

		read_count += size_1;
		spin_lock(&auddrv_BTDaiInCtl_lock);
		Dai_Block->u4DataRemained -= size_1;
		Dai_Block->u4DMAReadIdx += size_1;
		Dai_Block->u4DMAReadIdx %= Dai_Block->u4BufferSize;
		DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
		spin_unlock(&auddrv_BTDaiInCtl_lock);

#if defined(AUD_DEBUG_LOG)
		pr_debug(
			"%s finish2, copy size_1:0x%x,u4DMAReadIdx:0x%x, u4WriteIdx:0x%x, Remained:0x%x \r\n",
			__func__, size_1, Dai_Block->u4DMAReadIdx,
			Dai_Block->u4WriteIdx, Dai_Block->u4DataRemained);
#endif
		if (DMA_Read_Ptr != Dai_Block->u4DMAReadIdx) {

			pr_warn("%s 3, read_size2:%x,Remained:%x, Read_Ptr:%zu, ReadIdx:%x \r\n",
				__func__, size_2, Dai_Block->u4DataRemained,
				DMA_Read_Ptr, Dai_Block->u4DMAReadIdx);
		}
		if (copy_to_user((void __user *)(Read_Data_Ptr + size_1),
				 (Dai_Block->pucVirtBufAddr + DMA_Read_Ptr),
				 size_2)) {

			pr_warn("%s Fail 3 copy to user Ptr:%p,VirtAddr:%p, ReadIdx:0x%x , Ptr:%zu,read_size:%zu",
				__func__, Read_Data_Ptr,
				Dai_Block->pucVirtBufAddr,
				Dai_Block->u4DMAReadIdx, DMA_Read_Ptr,
				read_size);
			return read_count << 2;
		}

		read_count += size_2;
		spin_lock(&auddrv_BTDaiInCtl_lock);
		Dai_Block->u4DataRemained -= size_2;
		Dai_Block->u4DMAReadIdx += size_2;
		DMA_Read_Ptr = Dai_Block->u4DMAReadIdx;
		spin_unlock(&auddrv_BTDaiInCtl_lock);

		count -= read_size;
		Read_Data_Ptr += read_size;
#if defined(AUD_DEBUG_LOG)
		pr_debug(
			"%s finish3, copy size_2:0x%x,ReadIdx:0x%x, WriteIdx:0x%x Remained:0x%x \r\n",
			__func__, size_2, Dai_Block->u4DMAReadIdx,
			Dai_Block->u4WriteIdx, Dai_Block->u4DataRemained);
#endif
	}

	return count;
}


static void *dummy_page[2];

static struct page *
mtk_bt_dai_capture_pcm_page(struct snd_soc_component *component,
			    struct snd_pcm_substream *substream,
			    unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_soc_component_driver mtk_bt_dai_soc_component = {
	.name = AFE_PCM_NAME,
	.probe = mtk_asoc_bt_dai_component_probe,
	.open = mtk_bt_dai_pcm_open,
	.close = mtk_bt_dai_pcm_close,
	.hw_params = mtk_bt_dai_pcm_hw_params,
	.hw_free = mtk_bt_dai_capture_pcm_hw_free,
	.prepare = mtk_bt_dai_pcm_prepare,
	.trigger = mtk_bt_dai_pcm_trigger,
	.pointer = mtk_bt_dai_pcm_pointer,
	.copy_user = mtk_bt_dai_pcm_copy,
	.page = mtk_bt_dai_capture_pcm_page,

};

static int mtk_bt_dai_probe(struct platform_device *pdev)
{
	pr_debug("mtk_bt_dai_probe\n");

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_VOIP_BT_IN);
	pdev->name = pdev->dev.kobj.name;

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev,
					  &mtk_bt_dai_soc_component,
					  NULL,
					  0);
}

static int mtk_asoc_bt_dai_component_probe(struct snd_soc_component *component)
{
	pr_debug("%s()\n", __func__);
	AudDrv_Allocate_mem_Buffer(component->dev, Soc_Aud_Digital_Block_MEM_DAI,
				   BT_DAI_MAX_BUFFER_SIZE);
	Bt_Dai_Capture_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DAI);
	return 0;
}

static int mtk_bt_dai_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mt_soc_pcm_bt_dai_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_bt_dai",
	},
	{} };
#endif

static struct platform_driver mtk_bt_dai_capture_driver = {
	.driver = {

			.name = MT_SOC_VOIP_BT_IN,
			.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
			.of_match_table = mt_soc_pcm_bt_dai_of_ids,
#endif
		},
	.probe = mtk_bt_dai_probe,
	.remove = mtk_bt_dai_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_bt_dai_capture_dev;
#endif

static int __init mtk_soc_bt_dai_platform_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_bt_dai_capture_dev = platform_device_alloc(MT_SOC_VOIP_BT_IN, -1);
	if (!soc_bt_dai_capture_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_bt_dai_capture_dev);
	if (ret != 0) {
		platform_device_put(soc_bt_dai_capture_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_bt_dai_capture_driver);
	return ret;
}

static void __exit mtk_soc_bt_dai_platform_exit(void)
{
	platform_driver_unregister(&mtk_bt_dai_capture_driver);
}
module_init(mtk_soc_bt_dai_platform_init);
module_exit(mtk_soc_bt_dai_platform_exit);

MODULE_DESCRIPTION("BT DAI module platform driver");
MODULE_LICENSE("GPL");
