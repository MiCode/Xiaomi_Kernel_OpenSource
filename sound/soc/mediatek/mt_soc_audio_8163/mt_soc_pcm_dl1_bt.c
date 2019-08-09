/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */



/********************************************************************
 *                     C O M P I L E R   F L A G S
 ********************************************************************/


/********************************************************************
 *                E X T E R N A L   R E F E R E N C E
 ********************************************************************/

#include <linux/dma-mapping.h>
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"

static struct AFE_MEM_CONTROL_T *pdl1btMemControl;

static DEFINE_SPINLOCK(auddrv_DL1BTCtl_lock);

static struct device *mDev;

/*
 *    function implementation
 */

static int mtk_dl1bt_probe(struct platform_device *pdev);
static int mtk_Dl1Bt_close(struct snd_pcm_substream *substream);
static int mtk_asoc_Dl1Bt_pcm_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_asoc_dl1bt_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_dl1bt_pcm_hardware = {

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

static int mtk_pcm_dl1Bt_stop(struct snd_pcm_substream *substream)
{

	pr_debug("mtk_pcm_dl1Bt_stop\n");

	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);

	/* here to turn off digital part */
	SetConnection(Soc_Aud_InterCon_DisConnect,
		 Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O02);
	SetConnection(Soc_Aud_InterCon_DisConnect,
		 Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O02);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, false);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false)
		SetDaiBtEnable(false);

	EnableAfe(false);
	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();

	return 0;
}

static snd_pcm_uframes_t
	mtk_dl1bt_pcm_pointer(struct snd_pcm_substream *substream)
{
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	kal_uint32 Frameidx = 0;
	kal_int32 Afe_consumed_bytes = 0;
	unsigned long flags;

	struct AFE_BLOCK_T *Afe_Block = &pdl1btMemControl->rBlock;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	pr_debug(" %s Afe_Block->u4DMAReadIdx = 0x%x\n", __func__,
		Afe_Block->u4DMAReadIdx);

	spin_lock_irqsave(&pdl1btMemControl->substream_lock, flags);



	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == true) {
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL1_CUR);
		if (HW_Cur_ReadIdx == 0) {
			pr_debug("[Auddrv] HW_Cur_ReadIdx == 0\n");
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}

		HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

		if (HW_memory_index >= Afe_Block->u4DMAReadIdx)
			Afe_consumed_bytes =
				HW_memory_index - Afe_Block->u4DMAReadIdx;
		else {
			Afe_consumed_bytes =
				Afe_Block->u4BufferSize + HW_memory_index
				- Afe_Block->u4DMAReadIdx;
		}

		Afe_consumed_bytes = Align64ByteSize(Afe_consumed_bytes);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;

		pr_debug
		("cur = 0x%x mem_index = 0x%x consumed = 0x%x\n",
		HW_Cur_ReadIdx, HW_memory_index, Afe_consumed_bytes);

		spin_unlock_irqrestore(&pdl1btMemControl->substream_lock,
			flags);

		return audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	}

	Frameidx = audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	spin_unlock_irqrestore(&pdl1btMemControl->substream_lock, flags);
	return Frameidx;
}


static int mtk_pcm_dl1bt_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	pr_debug("mtk_pcm_dl1bt_hw_params\n");

	/* runtime->dma_bytes has to be set manually to allow mmap */
	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

	/* here to allcoate sram to hardware --------------------------- */
	AudDrv_Allocate_mem_Buffer(mDev, Soc_Aud_Digital_Block_MEM_DL1,
				   substream->runtime->dma_bytes);
	/* substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE; */
	substream->runtime->dma_area =
	 (unsigned char *)Get_Afe_SramBase_Pointer();
	substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;

	pr_debug("dma_bytes = %zu, dma_area = %p, dma_addr = 0x%lx\n",
		substream->runtime->dma_bytes,
		substream->runtime->dma_area,
		(long)substream->runtime->dma_addr);
	return ret;
}

static int mtk_pcm_dl1bt_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_pcm_dl1bt_hw_free\n");
	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_dl1_sample_rates = {

	.count = ARRAY_SIZE(soc_voice_supported_sample_rates),
	.list = soc_voice_supported_sample_rates,
	.mask = 0,
};

static int mPlaybackSramState;
static int mtk_dl1bt_pcm_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	AfeControlSramLock();
	if (GetSramState() == SRAM_STATE_FREE) {
		mtk_dl1bt_pcm_hardware.buffer_bytes_max =
		 GetPLaybackSramFullSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKFULL;
		SetSramState(mPlaybackSramState);
	} else {
		mtk_dl1bt_pcm_hardware.buffer_bytes_max =
			GetPLaybackSramPartial();
		mPlaybackSramState = SRAM_STATE_PLAYBACKPARTIAL;
		SetSramState(mPlaybackSramState);
	}
	AfeControlSramUnLock();

	pr_debug("mtk_dl1bt_pcm_open\n");
	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();

	/* get dl1 memconptrol and record substream */
	pdl1btMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
	runtime->hw = mtk_dl1bt_pcm_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_dl1bt_pcm_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_dl1_sample_rates);
	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_list failed\n");

	ret = snd_pcm_hw_constraint_integer(runtime,
	 SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_integer failed\n");

	/* print for hw pcm information */
	pr_debug("%s rate = %d, ch = %d,device = %d\n",
	__func__, runtime->rate,
	runtime->channels, substream->pcm->device);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("SNDRV_PCM_STREAM_PLAYBACK ");
		pr_debug("mtkalsa_playback_constraints\n");

	if (ret < 0) {
		pr_debug("mtk_Dl1Bt_close\n");
		mtk_Dl1Bt_close(substream);
		return ret;
	}
	return 0;
}

static int mtk_Dl1Bt_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	AfeControlSramLock();
	ClearSramState(mPlaybackSramState);
	mPlaybackSramState = GetSramState();
	AfeControlSramUnLock();
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();
	return 0;
}

static int mtk_dl1bt_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static bool SetVoipDAIBTAttribute(int sample_rate)
{
	struct AudioDigitalDAIBT daibt_attribute;

	memset((void *)&daibt_attribute, 0, sizeof(daibt_attribute));

#if 0				/* temp for merge only support */
	daibt_attribute.mUSE_MRGIF_INPUT = Soc_Aud_BT_DAI_INPUT_FROM_BT;
#else
	daibt_attribute.mUSE_MRGIF_INPUT = Soc_Aud_BT_DAI_INPUT_FROM_MGRIF;
#endif
	daibt_attribute.mDAI_BT_MODE =
		(sample_rate == 8000) ? Soc_Aud_DATBT_MODE_Mode8K :
			Soc_Aud_DATBT_MODE_Mode16K;
	daibt_attribute.mDAI_DEL = Soc_Aud_DAI_DEL_HighWord;
	daibt_attribute.mBT_LEN = 0;
	daibt_attribute.mDATA_RDY = true;
	daibt_attribute.mBT_SYNC = Soc_Aud_BTSYNC_Short_Sync;
	daibt_attribute.mBT_ON = true;
	daibt_attribute.mDAIBT_ON = false;
	SetDaiBt(&daibt_attribute);
	return true;
}


static int mtk_pcm_dl1bt_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();
	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE
	    || runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);

		/* BT SCO only support 16 bit */
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
		 Soc_Aud_InterConnectionOutput_O02);
	} else {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
		 AFE_WLEN_16_BIT);
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2,
		 AFE_WLEN_16_BIT);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
		 Soc_Aud_InterConnectionOutput_O02);
	}

	/* here start digital part */
	SetConnection(Soc_Aud_InterCon_Connection,
	 Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O02);
	SetConnection(Soc_Aud_InterCon_Connection,
	 Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O02);
	SetConnection(Soc_Aud_InterCon_ConnectionShift,
	 Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O02);
	SetConnection(Soc_Aud_InterCon_ConnectionShift,
	 Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O02);

	/* set dl1 sample ratelimit_state */
	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

	/* here to set interrupt */
	SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE,
	 runtime->period_size >> 1);
	SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE,
	 runtime->rate);
	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, true);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT) == false) {
		/* set merge interface */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);
	} else
		SetMemoryPathEnable(Soc_Aud_Digital_Block_DAI_BT, true);

	SetVoipDAIBTAttribute(runtime->rate);
	SetDaiBtEnable(true);

	EnableAfe(true);

	return 0;
}

static int mtk_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("mtk_pcm_trigger cmd = %d\n", cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_dl1bt_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_dl1Bt_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_dl1bt_copy(struct snd_pcm_substream *substream,
			      int channel, snd_pcm_uframes_t pos,
			      void __user *dst, snd_pcm_uframes_t count)
{
	struct AFE_BLOCK_T *Afe_Block = NULL;
	unsigned long flags;
	char *data_w_ptr = (char *)dst;
	int copy_size = 0, Afe_WriteIdx_tmp;

	pr_debug("%s pos = %lu count = %lu\n",
	 __func__, pos, count);

	/* get total bytes to copy */
	count = audio_frame_to_bytes(substream, count);

	/* check which memif nned to be write */
	Afe_Block = &pdl1btMemControl->rBlock;

	pr_debug("%s wp=0x%x, rp=0x%x, remained=0x%x\n",
	__func__, Afe_Block->u4WriteIdx,
	Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);

	if (Afe_Block->u4BufferSize == 0) {
		pr_err("%s: u4BufferSize=0 Error\n", __func__);
		return 0;
	}

	spin_lock_irqsave(&auddrv_DL1BTCtl_lock, flags);
	copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;
	spin_unlock_irqrestore(&auddrv_DL1BTCtl_lock, flags);

	if (count <= copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

	copy_size = Align64ByteSize(copy_size);
	pr_debug("copy_size=0x%x, count=0x%x\n",
		 copy_size, (unsigned int)count);

	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_DL1BTCtl_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_DL1BTCtl_lock, flags);

		if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) {
			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
				pr_debug("write 0 ptr invalid\n");
				pr_debug("ptr=%p, size=%d",
					data_w_ptr, copy_size);
				pr_debug("write Size=%d, Remained=%d",
					Afe_Block->u4BufferSize,
					Afe_Block->u4DataRemained);
			} else {
				pr_debug
				("copy ptr=%p, data_w_ptr=%p, copy_size=%x\n",
				Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp,
				data_w_ptr, copy_size);

				if (copy_from_user((Afe_Block->pucVirtBufAddr +
					Afe_WriteIdx_tmp),
					data_w_ptr, copy_size)) {
					pr_debug
					("AudDrv_write Fail copy from user\n");
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_DL1BTCtl_lock, flags);
			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_DL1BTCtl_lock, flags);
			data_w_ptr += copy_size;
			count -= copy_size;

			pr_debug("%s finish1,size:%x, wp:%x, rp=%x\n",
				__func__,
				copy_size,
				Afe_Block->u4WriteIdx,
				Afe_Block->u4DMAReadIdx);
			pr_debug("remained:%x, count=%lu\n",
				Afe_Block->u4DataRemained, count);

		} else {	/* copy twice */
			kal_uint32 size_1 = 0, size_2 = 0;

			size_1 = Align64ByteSize((Afe_Block->u4BufferSize -
			 Afe_WriteIdx_tmp));
			size_2 = Align64ByteSize((copy_size - size_1));
			pr_debug("size_1=0x%x, size_2=0x%x\n",
				size_1, size_2);

			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
				pr_warn("AudDrv_write 1 ptr invalid\n");
				pr_warn("data_w_ptr=%p\n", data_w_ptr);
				pr_warn("size_1=%d\n", size_1);
				pr_warn("AudDrv_write u4BufferSize=%d\n",
					Afe_Block->u4BufferSize);
				pr_warn("u4DataRemained=%d\n",
					Afe_Block->u4DataRemained);
			} else {
				pr_debug
				("copy, ptr=%p,data_w_ptr=%p, size_1=%x\n",
				Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp,
				data_w_ptr, size_1);

				if ((copy_from_user((Afe_Block->pucVirtBufAddr +
					Afe_WriteIdx_tmp),
					data_w_ptr, size_1))) {
					pr_debug("AudDrv_write Fail 1 ");
					pr_debug("copy from user\n");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_DL1BTCtl_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&auddrv_DL1BTCtl_lock, flags);

			if (!access_ok(VERIFY_READ,
				data_w_ptr + size_1, size_2)) {
				pr_debug("write 2 invalid ptr=%p\n",
					data_w_ptr);
				pr_debug("size_1=%d\n", size_1);
				pr_debug("size_2=%d\n", size_2);
				pr_debug("AudDrv_write u4BufferSize = %dn",
					Afe_Block->u4BufferSize);
				pr_debug("u4DataRemained = %d\n",
					Afe_Block->u4DataRemained);
			} else {
				pr_debug
				("copy, wp=%p, size_1=%p, size_2=%x\n",
				Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp,
				data_w_ptr + size_1, size_2);

				if ((copy_from_user((Afe_Block->pucVirtBufAddr +
					 Afe_WriteIdx_tmp),
					(data_w_ptr + size_1), size_2))) {
					pr_debug
					("write Fail 2 copy from user\n");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_DL1BTCtl_lock, flags);

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_DL1BTCtl_lock, flags);
			count -= copy_size;
			data_w_ptr += copy_size;

			pr_debug
			("write finish 2,size:%x, wp:%x, rp:%x, remained:%x\n",
			copy_size, Afe_Block->u4WriteIdx,
			Afe_Block->u4DMAReadIdx,
			Afe_Block->u4DataRemained);
		}
	}
	pr_debug("pcm_copy return\n");
	return 0;
}

static int mtk_pcm_dl1bt_silence(struct snd_pcm_substream *substream,
	 int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	pr_debug("%s\n", __func__);
	/* do nothing */
	return 0;
}

static void *dummy_page[2];

static struct page *mtk_pcm_page(struct snd_pcm_substream *substream,
	 unsigned long offset)
{
	pr_debug("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]);
}

static struct snd_pcm_ops mtk_d1lbt_ops = {

	.open = mtk_dl1bt_pcm_open,
	.close = mtk_Dl1Bt_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_dl1bt_hw_params,
	.hw_free = mtk_pcm_dl1bt_hw_free,
	.prepare = mtk_dl1bt_pcm_prepare,
	.trigger = mtk_pcm_trigger,
	.pointer = mtk_dl1bt_pcm_pointer,
	.copy = mtk_pcm_dl1bt_copy,
	.silence = mtk_pcm_dl1bt_silence,
	.page = mtk_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_dl1bt_platform = {

	.ops = &mtk_d1lbt_ops,
	.pcm_new = mtk_asoc_Dl1Bt_pcm_new,
	.probe = mtk_asoc_dl1bt_probe,
};

static int mtk_dl1bt_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (pdev->dev.dma_mask == NULL)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_VOIP_BT_OUT);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	mDev = &pdev->dev;

	return snd_soc_register_platform(&pdev->dev, &mtk_soc_dl1bt_platform);
}

static int mtk_asoc_Dl1Bt_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	return ret;
}


static int mtk_asoc_dl1bt_probe(struct snd_soc_platform *platform)
{
	pr_debug("mtk_asoc_dl1bt_probe\n");
	return 0;
}

static int mtk_asoc_dl1bt_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dl1_bt_of_ids[] = {

	{.compatible = "mediatek,mt8163-soc-pcm-dl1-bt",},
	{}
};
#endif

static struct platform_driver mtk_dl1bt_driver = {

	.driver = {
		   .name = MT_SOC_VOIP_BT_OUT,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_pcm_dl1_bt_of_ids,
#endif
		   },
	.probe = mtk_dl1bt_probe,
	.remove = mtk_asoc_dl1bt_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_dl1bt_dev;
#endif

static int __init mtk_soc_dl1bt_platform_init(void)
{
	int ret;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtk_dl1bt_dev = platform_device_alloc(MT_SOC_VOIP_BT_OUT, -1);

	if (!soc_mtk_dl1bt_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_dl1bt_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_dl1bt_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_dl1bt_driver);
	return ret;

}
module_init(mtk_soc_dl1bt_platform_init);

static void __exit mtk_soc_dl1bt_platform_exit(void)
{
	pr_debug("%s\n", __func__);

	platform_driver_unregister(&mtk_dl1bt_driver);
}
module_exit(mtk_soc_dl1bt_platform_exit);

MODULE_DESCRIPTION("AFE dl1bt module platform driver");
MODULE_LICENSE("GPL");
