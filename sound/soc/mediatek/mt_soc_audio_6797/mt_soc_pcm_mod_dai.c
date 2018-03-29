/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
 * If not, see <http://www.gnu.org/licenses/>.
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
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_pcm_common.h"


AFE_MEM_CONTROL_T  *MOD_DAI_Control_context;
static struct snd_dma_buffer *Capture_dma_buf;
static AudioDigtalI2S *mAudioDigitalI2S;
static bool mModDaiUseSram;
static DEFINE_SPINLOCK(auddrv_ModDaiInCtl_lock);

/*
  *    function implementation
  */

static int mtk_mod_dai_probe(struct platform_device *pdev);
static int mtk_mod_dai_pcm_close(struct snd_pcm_substream *substream);
static int mtk_asoc_mod_dai_pcm_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_mod_dai_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_mod_dai_hardware = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_RESUME |
	SNDRV_PCM_INFO_MMAP_VALID),
	.formats =      SND_SOC_ADV_MT_FMTS,
	.rates =        SOC_NORMAL_USE_RATE,
	.rate_min =     SOC_NORMAL_USE_RATE_MIN,
	.rate_max =     SOC_NORMAL_USE_RATE_MAX,
	.channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = MOD_DAI_MAX_BUFFER_SIZE,
	.period_bytes_max = MOD_DAI_MAX_BUFFER_SIZE,
	.periods_min =      MOD_DAI_MIN_PERIOD_SIZE,
	.periods_max =      MOD_DAI_MAX_PERIOD_SIZE,
	.fifo_size =        0,
};

static void StopAudioModDaiCaptureHardware(struct snd_pcm_substream *substream)
{
	pr_debug("StopAudioModDaiCaptureHardware\n");

	/* legacy usagebk97
	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, false);
	*/

	irq_remove_user(substream, Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_MOD_DAI, false);

	/*
	  * connect phone call DL data to dai and disconnect DL to speaker path
	  */
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I14, Soc_Aud_InterConnectionOutput_O12);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I09, Soc_Aud_InterConnectionOutput_O12);

	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I14, Soc_Aud_InterConnectionOutput_O28);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I14, Soc_Aud_InterConnectionOutput_O29);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I21, Soc_Aud_InterConnectionOutput_O28);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I21, Soc_Aud_InterConnectionOutput_O29);

	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I09, Soc_Aud_InterConnectionOutput_O28);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I09, Soc_Aud_InterConnectionOutput_O29);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I22, Soc_Aud_InterConnectionOutput_O28);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I22, Soc_Aud_InterConnectionOutput_O29);

	EnableAfe(false);
}

static void StartAudioModDaiCaptureHardware(struct snd_pcm_substream *substream)
{
	pr_debug("StartAudioModDaiCaptureHardware\n");

	if (substream->runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		substream->runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_MOD_DAI,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		SetoutputConnectionFormat(AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA,
			Soc_Aud_InterConnectionOutput_O12);
	} else {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_MOD_DAI, AFE_WLEN_16_BIT);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O12);
	}

	/* here to set interrupt */
	/* legacy
	SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, substream->runtime->period_size);
	SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, substream->runtime->rate);
	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE, true);
	*/

	SetSampleRate(Soc_Aud_Digital_Block_MEM_MOD_DAI, substream->runtime->rate);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_MOD_DAI, true);

	irq_add_user(substream,
		Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE,
		substream->runtime->rate,
		substream->runtime->period_size);

	/*
	  * connect phone call DL data to dai and disconnect DL to speaker path
	  */
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I14, Soc_Aud_InterConnectionOutput_O12);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I09, Soc_Aud_InterConnectionOutput_O12);

	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O24);

	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I14, Soc_Aud_InterConnectionOutput_O28);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I14, Soc_Aud_InterConnectionOutput_O29);

	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I21, Soc_Aud_InterConnectionOutput_O28);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I21, Soc_Aud_InterConnectionOutput_O29);

	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I09, Soc_Aud_InterConnectionOutput_O28);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I09, Soc_Aud_InterConnectionOutput_O29);

	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I22, Soc_Aud_InterConnectionOutput_O28);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I22, Soc_Aud_InterConnectionOutput_O29);

	EnableAfe(true);
}

static int mtk_mod_dai_pcm_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_mod_dai_alsa_stop(struct snd_pcm_substream *substream)
{
	AFE_BLOCK_T *pModDai_Block = &(MOD_DAI_Control_context->rBlock);

	pr_err("mtk_mod_dai_alsa_stop\n");
	StopAudioModDaiCaptureHardware(substream);
	pModDai_Block->u4DMAReadIdx  = 0;
	pModDai_Block->u4WriteIdx  = 0;
	pModDai_Block->u4DataRemained = 0;
	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_MOD_DAI, substream);
	return 0;
}

static kal_int32 Previous_Hw_cur;
static snd_pcm_uframes_t mtk_mod_dai_pcm_pointer(struct snd_pcm_substream *substream)
{
	kal_uint32 Frameidx = 0;
	AFE_BLOCK_T *pModDai_Block = &(MOD_DAI_Control_context->rBlock);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_MOD_DAI) == true) {
		Frameidx = audio_bytes_to_frame(substream , pModDai_Block->u4WriteIdx);
		return Frameidx;
	}
	return audio_bytes_to_frame(substream, Previous_Hw_cur);
}

static void SetMODDAIBuffer(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	AFE_BLOCK_T *pblock = &MOD_DAI_Control_context->rBlock;

	pr_debug("SetMODDAIBuffer\n");
	pblock->pucPhysBufAddr =  runtime->dma_addr;
	pblock->pucVirtBufAddr =  runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;
	pr_debug("%s u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
		__func__, pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);

	Afe_Set_Reg(AFE_MOD_DAI_BASE , pblock->pucPhysBufAddr , 0xffffffff);
	Afe_Set_Reg(AFE_MOD_DAI_END  , pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);

}

static int mtk_mod_dai_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

	pr_debug("mod_dai_dma_buf = %p mod_dai_dma_buf->area = %p\n", Capture_dma_buf, Capture_dma_buf->area);

	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
	if (AllocateAudioSram(&substream->runtime->dma_addr,	&substream->runtime->dma_area,
		substream->runtime->dma_bytes, substream) == 0) {
		mModDaiUseSram = false;
		/* pr_warn("mtk_pcm_hw_params dma_bytes = %d\n",substream->runtime->dma_bytes); */
	} else {
		substream->runtime->dma_area = Capture_dma_buf->area;
		substream->runtime->dma_addr = Capture_dma_buf->addr;
		mModDaiUseSram = true;
		AudDrv_Emi_Clk_On();
	}

	pr_debug("mtk_mod_dai_pcm_hw_params dma_bytes = %zu dma_area = %p dma_addr = 0x%x\n",
	       runtime->dma_bytes, runtime->dma_area, (unsigned int)runtime->dma_addr);

	SetMODDAIBuffer(substream, hw_params);
	return ret;
}

static int mtk_mod_dai_pcm_hw_free(struct snd_pcm_substream *substream)
{
	pr_warn("mtk_i2s2_adc2_capture_pcm_hw_free\n");
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
	MOD_DAI_Control_context = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_MOD_DAI);
	runtime->hw = mtk_mod_dai_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_mod_dai_hardware , sizeof(struct snd_pcm_hardware));
	pr_debug("runtime->hw->rates = 0x%x\n ", runtime->hw.rates);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_integer failed\n");

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
	pr_debug("mtk_mod_dai_alsa_start\n");
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

static bool CheckNullPointer(void *pointer)
{
	if (pointer == NULL) {
		pr_debug("CheckNullPointer pointer = NULL");
		return true;
	}
	return false;
}

static int mtk_mod_dai_pcm_copy(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				void __user *dst, snd_pcm_uframes_t count)
{

	AFE_MEM_CONTROL_T *pMOD_DAI_MEM_ConTrol = NULL;
	AFE_BLOCK_T  *pModDai_Block = NULL;
	char *Read_Data_Ptr = (char *)dst;
	ssize_t DMA_Read_Ptr = 0 , read_size = 0, read_count = 0;
	unsigned long flags;

	pr_debug("mtk_mod_dai_pcm_copy pos = %lu, count = %lu\n ", pos, count);

	count = audio_frame_to_bytes(substream , count);

	pMOD_DAI_MEM_ConTrol = MOD_DAI_Control_context;
	pModDai_Block = &(pMOD_DAI_MEM_ConTrol->rBlock);

	if (pMOD_DAI_MEM_ConTrol == NULL) {
		pr_err("cannot find MEM control\n");
		msleep(50);
		return 0;
	}

	if (pModDai_Block->u4BufferSize <= 0) {
		msleep(50);
		pr_debug("pModDai_Block->u4BufferSize <= 0  =%d\n", pModDai_Block->u4BufferSize);
		return 0;
	}

	if (CheckNullPointer((void *)pModDai_Block->pucVirtBufAddr)) {
		pr_debug("CheckNullPointer  pucVirtBufAddr = %p\n", pModDai_Block->pucVirtBufAddr);
		return 0;
	}

	spin_lock_irqsave(&auddrv_ModDaiInCtl_lock, flags);
	if (pModDai_Block->u4DataRemained >  pModDai_Block->u4BufferSize) {
		pr_debug("mtk_mod_dai_pcm_copy u4DataRemained=%x > u4BufferSize=%x\n" ,
			pModDai_Block->u4DataRemained, pModDai_Block->u4BufferSize);
		pModDai_Block->u4DataRemained = 0;
		pModDai_Block->u4DMAReadIdx  = pModDai_Block->u4WriteIdx;
	}
	if (count >  pModDai_Block->u4DataRemained)
		read_size = pModDai_Block->u4DataRemained;
	else
		read_size = count;


	DMA_Read_Ptr = pModDai_Block->u4DMAReadIdx;
	spin_unlock_irqrestore(&auddrv_ModDaiInCtl_lock, flags);

	/*
	pr_debug("mod_dai_opy 0, read_count:%x, read_size:%lx, Remained:%x, ReadIdx:0x%x, u4WriteIdx:%x\n",
			read_count, read_size, pModDai_Block->u4DataRemained,
			pModDai_Block->u4DMAReadIdx, pModDai_Block->u4WriteIdx);*/

	if (DMA_Read_Ptr + read_size < pModDai_Block->u4BufferSize) {
		if (copy_to_user((void __user *)Read_Data_Ptr,
			(pModDai_Block->pucVirtBufAddr + DMA_Read_Ptr), read_size)) {
			pr_err("%s Fail 1 copy to user Read_Ptr:%p, VirtBufAddr:%p, ReadIdx:0x%x, Read_Ptr:0x%lx,read_size:%lx\n",
				__func__, Read_Data_Ptr, pModDai_Block->pucVirtBufAddr,
				pModDai_Block->u4DMAReadIdx, DMA_Read_Ptr, read_size);
			return 0;
		}

		read_count += read_size;
		spin_lock(&auddrv_ModDaiInCtl_lock);
		pModDai_Block->u4DataRemained -= read_size;
		pModDai_Block->u4DMAReadIdx += read_size;
		pModDai_Block->u4DMAReadIdx %= pModDai_Block->u4BufferSize;
		DMA_Read_Ptr = pModDai_Block->u4DMAReadIdx;
		spin_unlock(&auddrv_ModDaiInCtl_lock);

		Read_Data_Ptr += read_size;
		count -= read_size;
		/*
		pr_debug("mod_dai_copy 1, copy size:%x, ReadIdx:0x%x, WriteIdx:%x, Remained:%x\n",
				  read_size, pModDai_Block->u4DMAReadIdx,
				  pModDai_Block->u4WriteIdx, pModDai_Block->u4DataRemained);
				  */
	}

	else {
		uint32 size_1 = pModDai_Block->u4BufferSize - DMA_Read_Ptr;
		uint32 size_2 = read_size - size_1;

		if (copy_to_user((void __user *)Read_Data_Ptr,
			(pModDai_Block->pucVirtBufAddr + DMA_Read_Ptr), size_1)) {

			pr_debug(" %s 2, read_size1:%x, DataRemained:%x, DMA_Read_Ptr:0x%lx, DMAReadIdx:%x\n",
			       __func__, size_1, pModDai_Block->u4DataRemained,
			       DMA_Read_Ptr, pModDai_Block->u4DMAReadIdx);

			return 0;
		}

		read_count += size_1;
		spin_lock(&auddrv_ModDaiInCtl_lock);
		pModDai_Block->u4DataRemained -= size_1;
		pModDai_Block->u4DMAReadIdx += size_1;
		pModDai_Block->u4DMAReadIdx %= pModDai_Block->u4BufferSize;
		DMA_Read_Ptr = pModDai_Block->u4DMAReadIdx;
		spin_unlock(&auddrv_ModDaiInCtl_lock);

		pr_debug("%s finish2, copy size_1:%x, u4DMAReadIdx:0x%x, u4WriteIdx:0x%x, u4DataRemained:%x\n",
				  __func__, size_1, pModDai_Block->u4DMAReadIdx,
				  pModDai_Block->u4WriteIdx, pModDai_Block->u4DataRemained);

		if (copy_to_user((void __user *)(Read_Data_Ptr + size_1),
			(pModDai_Block->pucVirtBufAddr + DMA_Read_Ptr), size_2)) {
			pr_debug("%s 2 read_size1:%x, DataRemained:%x, DMA_Read_Ptr:0x%lx, DMAReadIdx:%x\n",
			       __func__, size_1, pModDai_Block->u4DataRemained,
			       DMA_Read_Ptr, pModDai_Block->u4DMAReadIdx);
			return read_count << 2;
		}

		read_count += size_2;
		spin_lock(&auddrv_ModDaiInCtl_lock);
		pModDai_Block->u4DataRemained -= size_2;
		pModDai_Block->u4DMAReadIdx += size_2;
		DMA_Read_Ptr = pModDai_Block->u4DMAReadIdx;
		spin_unlock(&auddrv_ModDaiInCtl_lock);

		count -= read_size;
		Read_Data_Ptr += read_size;
		/*
		pr_debug(" 3, read_size1:%x, DataRemained:%x, DMA_Read_Ptr:0x%lx, DMAReadIdx:%x\n",
			   __func__,size_1, pModDai_Block->u4DataRemained, DMA_Read_Ptr, pModDai_Block->u4DMAReadIdx);
			   */
	}

	return read_count >> 1;
}

static int mtk_mod_dai_pcm_silence(struct snd_pcm_substream *substream,
				   int channel, snd_pcm_uframes_t pos,
				   snd_pcm_uframes_t count)
{
	pr_debug("dummy_pcm_silence\n");
	return 0; /* do nothing */
}


static void *dummy_page[2];

static struct page *mtk_mod_dai_pcm_page(struct snd_pcm_substream *substream,
					 unsigned long offset)
{
	pr_debug("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}


static struct snd_pcm_ops mtk_afe_mod_dai_ops = {
	.open =     mtk_mod_dai_pcm_open,
	.close =    mtk_mod_dai_pcm_close,
	.ioctl =    snd_pcm_lib_ioctl,
	.hw_params =    mtk_mod_dai_pcm_hw_params,
	.hw_free =  mtk_mod_dai_pcm_hw_free,
	.prepare =  mtk_mod_dai_pcm_prepare,
	.trigger =  mtk_mod_dai_pcm_trigger,
	.pointer =  mtk_mod_dai_pcm_pointer,
	.copy =     mtk_mod_dai_pcm_copy,
	.silence =  mtk_mod_dai_pcm_silence,
	.page =     mtk_mod_dai_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_platform = {
	.ops        = &mtk_afe_mod_dai_ops,
	.pcm_new    = mtk_asoc_mod_dai_pcm_new,
	.probe      = mtk_afe_mod_dai_probe,
};

static int mtk_mod_dai_probe(struct platform_device *pdev)
{
	pr_debug("mtk_mod_dai_probe\n");

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_MOD_DAI_PCM);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
					 &mtk_soc_platform);
}

static int mtk_asoc_mod_dai_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	pr_debug("mtk_asoc_mod_dai_pcm_new\n");
	return 0;
}


static int mtk_afe_mod_dai_probe(struct snd_soc_platform *platform)
{
	pr_debug("mtk_afe_mod_dai_probe\n");
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_MOD_DAI, MOD_DAI_MAX_BUFFER_SIZE);
	Capture_dma_buf =  Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_MOD_DAI);
	mAudioDigitalI2S =  kzalloc(sizeof(AudioDigtalI2S), GFP_KERNEL);
	return 0;
}


static int mtk_mod_dai_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_afe_mod_dai_driver = {
	.driver = {
		.name = MT_SOC_MOD_DAI_PCM,
		.owner = THIS_MODULE,
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

	pr_debug("%s\n", __func__);
	platform_driver_unregister(&mtk_afe_mod_dai_driver);
}

module_exit(mtk_soc_mod_dai_platform_exit);

MODULE_DESCRIPTION("AFE mod dai module platform driver");
MODULE_LICENSE("GPL");


