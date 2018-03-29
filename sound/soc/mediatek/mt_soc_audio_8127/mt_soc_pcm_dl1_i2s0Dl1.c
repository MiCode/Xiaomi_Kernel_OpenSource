/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/dma-mapping.h>
#include "mt_soc_afe_common.h"
#include "mt_soc_afe_def.h"
#include "mt_soc_afe_reg.h"
#include "mt_soc_afe_clk.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"


#define MAGIC_NUMBER 0xFFFFFFC0
static DEFINE_SPINLOCK(auddrv_I2S0dl1_lock);
static struct AFE_MEM_CONTROL_T *pI2S0dl1MemControl;
static struct snd_dma_buffer *Dl1_Playback_dma_buf;
static unsigned int mPlaybackSramState = SRAM_STATE_FREE;

/*
 *    function implementation
 */

static int mtk_I2S0dl1_probe(struct platform_device *pdev);
static int mtk_pcm_I2S0dl1_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_I2S0dl1_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_I2S0dl1_probe(struct snd_soc_platform *platform);

static bool mPrepareDone;

static struct device *mDev;

static const char * const I2S0dl1_HD_output[] = { "Off", "On" };

static const struct soc_enum Audio_I2S0dl1_Enum[] = {

	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(I2S0dl1_HD_output), I2S0dl1_HD_output),
};


static int Audio_I2S0dl1_hdoutput_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	PRINTK_AUDDRV("%s not suppprt\n", __func__);
	return 0;
}

static int Audio_I2S0dl1_hdoutput_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	PRINTK_AUDDRV("%s not suppprt\n", __func__);

	return 0;
}


static const struct snd_kcontrol_new Audio_snd_I2S0dl1_controls[] = {

	SOC_ENUM_EXT("Audio_I2S0dl1_hd_Switch", Audio_I2S0dl1_Enum[0], Audio_I2S0dl1_hdoutput_Get,
		     Audio_I2S0dl1_hdoutput_Set),
};

static struct snd_pcm_hardware mtk_I2S0dl1_hardware = {

	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_STD_MT_FMTS,
	.rates = SOC_NORMAL_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.period_bytes_max = Dl1_MAX_PERIOD_SIZE,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};


static int mtk_pcm_I2S0dl1_stop(struct snd_pcm_substream *substream)
{
	/* AFE_BLOCK_T *Afe_Block = &(pI2S0dl1MemControl->rBlock); */

	pr_debug("%s\n", __func__);

	mt_afe_set_irq_state(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);
	mt_afe_disable_memory_path(Soc_Aud_Digital_Block_MEM_DL1);

	/* here start digital part */
	mt_afe_set_connection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O00);
	mt_afe_set_connection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O01);
	mt_afe_set_connection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O03);
	mt_afe_set_connection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O04);

	ClearMemBlock(Soc_Aud_Digital_Block_MEM_DL1);

	return 0;
}

static snd_pcm_uframes_t mtk_pcm_I2S0dl1_pointer(struct snd_pcm_substream *substream)
{
	int32_t HW_memory_index = 0;
	int32_t HW_Cur_ReadIdx = 0;
	uint32_t Frameidx = 0;
	int32_t Afe_consumed_bytes = 0;
	struct AFE_BLOCK_T *Afe_Block = &pI2S0dl1MemControl->rBlock;
	unsigned long flags;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */

	spin_lock_irqsave(&pI2S0dl1MemControl->substream_lock, flags);
	PRINTK_AUD_DL1("%s Afe_Block->u4DMAReadIdx = 0x%x\n", __func__,
		Afe_Block->u4DMAReadIdx);

	/* get total bytes to copy */
	/* Frameidx = audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx); */
	/* return Frameidx; */

	if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_DL1) == true) {
		HW_Cur_ReadIdx = mt_afe_get_reg(AFE_DL1_CUR);
		if (HW_Cur_ReadIdx == 0) {
			PRINTK_AUDDRV("[Auddrv] HW_Cur_ReadIdx == 0\n");
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}

		HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

		if (HW_memory_index >= Afe_Block->u4DMAReadIdx)
			Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
		else {
			Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index
				- Afe_Block->u4DMAReadIdx;
		}

		Afe_consumed_bytes = align64bytesize(Afe_consumed_bytes);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;

		PRINTK_AUD_DL1
			("[Auddrv] HW_Cur_ReadIdx = 0x%x, HW_memory_index = 0x%x, Afe_consumed_bytes = 0x%x\n",
			HW_Cur_ReadIdx, HW_memory_index, Afe_consumed_bytes);

		spin_unlock_irqrestore(&pI2S0dl1MemControl->substream_lock, flags);

		return audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	}

	Frameidx = audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	spin_unlock_irqrestore(&pI2S0dl1MemControl->substream_lock, flags);
	return Frameidx;
}


static void set_dl1_buffer(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct AFE_BLOCK_T *pblock = &pI2S0dl1MemControl->rBlock;

	pblock->pucPhysBufAddr = runtime->dma_addr;
	pblock->pucVirtBufAddr = runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;
	pr_debug("set_dl1_buffer u4BufferSize = %d, pucVirtBufAddr = %p, pucPhysBufAddr = 0x%x\n",
	       pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
	/* set dram address top hardware */
	mt_afe_set_reg(AFE_DL1_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	mt_afe_set_reg(AFE_DL1_END, pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);
	memset((void *)pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);

}

static int mtk_pcm_I2S0dl1_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
	/* PRINTK_AUDDRV("mtk_pcm_hw_params dma_bytes = %d\n",substream->runtime->dma_bytes); */
#if 1
	if (mPlaybackSramState == SRAM_STATE_PLAYBACKFULL) {
		/* substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE; */
		substream->runtime->dma_area = (unsigned char *)mt_afe_get_sram_base_ptr();
		substream->runtime->dma_addr = mt_afe_get_sram_phy_addr();
		afe_allocate_dl1_buffer(mDev, substream->runtime->dma_bytes);
	} else {
		substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
		substream->runtime->dma_area = Dl1_Playback_dma_buf->area;
		substream->runtime->dma_addr = Dl1_Playback_dma_buf->addr;
		set_dl1_buffer(substream, hw_params);
	}
#else				/* old */
	/* here to allcoate sram to hardware --------------------- */
	afe_allocate_mem_buffer(Soc_Aud_Digital_Block_MEM_DL1, substream->runtime->dma_bytes);
#ifdef AUDIO_MEMORY_SRAM
	/* substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE; */
	substream->runtime->dma_area = (unsigned char *)mt_afe_get_sram_base_ptr();
	substream->runtime->dma_addr = mt_afe_get_sram_phy_addr();
#else
	pI2S0dl1MemControl = get_mem_control_t(Soc_Aud_Digital_Block_MEM_DL1);
	Afe_Block = &pI2S0dl1MemControl->rBlock;

	substream->runtime->dma_area = (unsigned char *)Afe_Block->pucVirtBufAddr;
	substream->runtime->dma_addr = Afe_Block->pucPhysBufAddr;
#endif
	/* ------------------------------------------------------- */
#endif
	pr_debug("dma_bytes = %zu, dma_area = %p, dma_addr = 0x%lx\n",
	       substream->runtime->dma_bytes, substream->runtime->dma_area,
	       (long)substream->runtime->dma_addr);

	return ret;
}

static int mtk_pcm_I2S0dl1_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {

	.count = ARRAY_SIZE(soc_normal_supported_sample_rates),
	.list = soc_normal_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_I2S0dl1_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	afe_control_sram_lock();
	if (get_sramstate() == SRAM_STATE_FREE) {
		mtk_I2S0dl1_hardware.buffer_bytes_max = get_playback_sram_fullsize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKFULL;
		set_sramstate(mPlaybackSramState);
	} else {
		mtk_I2S0dl1_hardware.buffer_bytes_max = GetPLaybackDramSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKDRAM;
	}
	afe_control_sram_unlock();

	if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
		mt_afe_emi_clk_on();

	pr_debug("mtk_I2S0dl1_hardware.buffer_bytes_max = %zu, mPlaybackSramState = %d\n",
	       mtk_I2S0dl1_hardware.buffer_bytes_max, mPlaybackSramState);
	runtime->hw = mtk_I2S0dl1_hardware;

	mt_afe_ana_clk_on();
	mt_afe_main_clk_on();
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_I2S0dl1_hardware,
	       sizeof(struct snd_pcm_hardware));
	pI2S0dl1MemControl = get_mem_control_t(Soc_Aud_Digital_Block_MEM_DL1);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);

	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		PRINTK_AUDDRV("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_I2S0dl1_playback_constraints\n");
	else
		PRINTK_AUDDRV("SNDRV_PCM_STREAM_CAPTURE mtkalsa_I2S0dl1_playback_constraints\n");

	if (ret < 0) {
		pr_warn("ret < 0 mtk_pcm_I2S0dl1_close\n");
		mtk_pcm_I2S0dl1_close(substream);
		return ret;
	}

	/* PRINTK_AUDDRV("mtk_pcm_I2S0dl1_open return\n"); */
	return 0;
}

static int mtk_pcm_I2S0dl1_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	if (mPrepareDone == true) {
		/* stop DAC output */
		mt_afe_disable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_DAC);

		if (!mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_I2S_OUT_DAC))
			mt_afe_disable_i2s_dac();

		/* stop I2S output */
		mt_afe_disable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_2);

		if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_I2S_OUT_2) == false)
			mt_afe_set_reg(AFE_I2S_CON3, 0x0, 0x1);

		RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

		mt_afe_enable_afe(false);

		mPrepareDone = false;
	}

	if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
		mt_afe_emi_clk_off();

	afe_control_sram_lock();
	clear_sramstate(mPlaybackSramState);
	mPlaybackSramState = get_sramstate();
	afe_control_sram_unlock();
	mt_afe_main_clk_off();
	mt_afe_ana_clk_off();
	return 0;
}

static int mtk_pcm_I2S0dl1_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	uint32_t u32AudioI2S = 0;
	bool mI2SWLen;

	if (mPrepareDone == false) {
		pr_debug("%s format = %d rate= %u channels= %u period_size= %lu\n",
			__func__, runtime->format, runtime->rate, runtime->channels, runtime->period_size);
		set_memif_substream(Soc_Aud_Digital_Block_MEM_DL1, substream);

		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE
		    || runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
						  Soc_Aud_InterConnectionOutput_O03);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
						  Soc_Aud_InterConnectionOutput_O04);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
						  Soc_Aud_InterConnectionOutput_O00);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
						  Soc_Aud_InterConnectionOutput_O01);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
						  Soc_Aud_InterConnectionOutput_O03);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
						  Soc_Aud_InterConnectionOutput_O04);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
						  Soc_Aud_InterConnectionOutput_O00);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
						  Soc_Aud_InterConnectionOutput_O01);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		}

		mt_afe_set_sample_rate(Soc_Aud_Digital_Block_MEM_I2S, runtime->rate);

		/* I2S out Setting */
		u32AudioI2S = mt_afe_rate_to_idx(runtime->rate) << 8;
		u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3;	/* us3 I2s format */
		u32AudioI2S |= Soc_Aud_I2S_WLEN_WLEN_16BITS << 1;	/* 32bit */

		if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_I2S_OUT_2) == false) {
			mt_afe_enable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_2);
			mt_afe_set_reg(AFE_I2S_CON3, u32AudioI2S | 1, AFE_MASK_ALL);
		} else
			mt_afe_enable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_2);

		/* start I2S DAC out */
		if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false) {
			mt_afe_enable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_DAC);
			mt_afe_set_i2s_dac_out(substream->runtime->rate);
			mt_afe_enable_i2s_dac();
		} else
			mt_afe_enable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_DAC);

		/* here to set interrupt_distributor */
		mt_afe_set_irq_counter(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->period_size);
		mt_afe_set_irq_rate(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->rate);

		mt_afe_enable_afe(true);
		mPrepareDone = true;
	}
	return 0;
}

static int mtk_pcm_I2S0dl1_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);
	/* here start digital part */

	mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O00);
	mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O01);
	mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O03);
	mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O04);

	mt_afe_set_irq_state(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, true);

	mt_afe_set_sample_rate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	mt_afe_set_channels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
	mt_afe_enable_memory_path(Soc_Aud_Digital_Block_MEM_DL1);

	mt_afe_enable_afe(true);

	return 0;
}

static int mtk_pcm_I2S0dl1_trigger(struct snd_pcm_substream *substream, int cmd)
{
	/* printk("mtk_pcm_I2S0dl1_trigger cmd = %d\n", cmd); */

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_I2S0dl1_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_I2S0dl1_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_I2S0dl1_copy(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				void __user *dst, snd_pcm_uframes_t count)
{
	struct AFE_BLOCK_T *Afe_Block = NULL;
	int copy_size = 0, Afe_WriteIdx_tmp;
	unsigned long flags;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	char *data_w_ptr = (char *)dst;

	PRINTK_AUD_DL1("mtk_pcm_copy pos = %lu count = %lu\n", pos, count);


	/* get total bytes to copy */
	count = audio_frame_to_bytes(substream, count);

	/* check which memif nned to be write */
	Afe_Block = &pI2S0dl1MemControl->rBlock;

	/* handle for buffer management */

	PRINTK_AUD_DL1("WriteIdx = 0x%x, ReadIdx = 0x%x, DataRemained = 0x%x\n",
		       Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);

	if (Afe_Block->u4BufferSize == 0) {
		pr_err("u4BufferSize = 0, Error!!!\n");
		return 0;
	}

	/* free space of the buffer */
	spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
	copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;
	spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);

	if (count <= copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

	copy_size = align64bytesize(copy_size);
	PRINTK_AUD_DL1("copy_size = 0x%x, count = 0x%x\n", copy_size, (unsigned int)count);

	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);

		if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) {	/* copy once */
			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
				pr_warn("0 ptr invalid data_w_ptr = %p, size = %d", data_w_ptr,
					      copy_size);
				pr_warn(" u4BufferSize = %d, u4DataRemained = %d\n",
					      Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {
				PRINTK_AUD_DL1
				("memcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%p, data_w_ptr=%p, copy_size=%x\n",
				Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, copy_size);

				if (copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
					data_w_ptr, copy_size)) {
					PRINTK_AUDDRV("Fail copy from user\n");
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);
			data_w_ptr += copy_size;
			count -= copy_size;

			PRINTK_AUD_DL1
				("finish 1, copy_size:%x, WriteIdx:%x, ReadIdx:%x, DataRemained:%x, count:%x\n",
				copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained, (unsigned int)count);

		} else {	/* copy twice */
			uint32_t size_1 = 0, size_2 = 0;

			size_1 = align64bytesize((Afe_Block->u4BufferSize - Afe_WriteIdx_tmp));
			size_2 = align64bytesize((copy_size - size_1));
			PRINTK_AUD_DL1("size_1 = 0x%x, size_2 = 0x%x\n", size_1, size_2);
			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
				pr_warn("1 ptr invalid, data_w_ptr = %p, size_1 = %d,", data_w_ptr,
				       size_1);
				pr_warn(" u4BufferSize = %d, u4DataRemained = %d\n",
				       Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {
				PRINTK_AUD_DL1
				("mcmcpy, Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%p, data_w_ptr=%p, size_1=%x\n",
				Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, size_1);

				if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
					data_w_ptr, size_1))) {
					PRINTK_AUDDRV("Fail 1 copy from user\n");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);

			if (!access_ok(VERIFY_READ, data_w_ptr + size_1, size_2)) {
				PRINTK_AUDDRV("2 ptr invalid, data_w_ptr = %p, size_1 = %d, size_2 = %d,",
					      data_w_ptr, size_1, size_2);
				PRINTK_AUDDRV(" u4BufferSize = %d, u4DataRemained = %d\n",
					      Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {
				PRINTK_AUD_DL1
				("mcmcpy, Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%p, data_w_ptr+size_1=%p, size_2=%x\n",
				Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr + size_1, size_2);

				if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
				      (data_w_ptr + size_1), size_2))) {
					PRINTK_AUDDRV("AudDrv_write Fail 2 copy from user\n");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);
			count -= copy_size;
			data_w_ptr += copy_size;

			PRINTK_AUD_DL1
				("finish 2, copy size:%x, WriteIdx:%x, ReadIdx:%x, DataRemained:%x\n",
				copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained);
		}
	}
	PRINTK_AUD_DL1("pcm_copy return\n");
	return 0;
}

static int mtk_pcm_I2S0dl1_silence(struct snd_pcm_substream *substream,
				   int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	PRINTK_AUDDRV("%s\n", __func__);
	/* do nothing */
	return 0;
}

static void *dummy_page[2];

static struct page *mtk_I2S0dl1_pcm_page(struct snd_pcm_substream *substream, unsigned long offset)
{
	PRINTK_AUDDRV("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]);	/* the same page */
}

static struct snd_pcm_ops mtk_I2S0dl1_ops = {

	.open = mtk_pcm_I2S0dl1_open,
	.close = mtk_pcm_I2S0dl1_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_I2S0dl1_hw_params,
	.hw_free = mtk_pcm_I2S0dl1_hw_free,
	.prepare = mtk_pcm_I2S0dl1_prepare,
	.trigger = mtk_pcm_I2S0dl1_trigger,
	.pointer = mtk_pcm_I2S0dl1_pointer,
	.copy = mtk_pcm_I2S0dl1_copy,
	.silence = mtk_pcm_I2S0dl1_silence,
	.page = mtk_I2S0dl1_pcm_page,
};

static struct snd_soc_platform_driver mtk_I2S0dl1_soc_platform = {

	.ops = &mtk_I2S0dl1_ops,
	.pcm_new = mtk_asoc_pcm_I2S0dl1_new,
	.probe = mtk_afe_I2S0dl1_probe,
};

static int mtk_I2S0dl1_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_I2S0DL1_PCM);

	PRINTK_AUDDRV("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	mDev = &pdev->dev;

	return snd_soc_register_platform(&pdev->dev, &mtk_I2S0dl1_soc_platform);
}

static int mtk_asoc_pcm_I2S0dl1_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	PRINTK_AUDDRV("%s\n", __func__);
	return ret;
}


static int mtk_afe_I2S0dl1_probe(struct snd_soc_platform *platform)
{
	pr_debug("mtk_afe_I2S0dl1_probe\n");
	snd_soc_add_platform_controls(platform, Audio_snd_I2S0dl1_controls,
				      ARRAY_SIZE(Audio_snd_I2S0dl1_controls));
	/* allocate dram */
	afe_allocate_mem_buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL1,
				   Dl1_MAX_BUFFER_SIZE);
	Dl1_Playback_dma_buf = afe_get_mem_buffer(Soc_Aud_Digital_Block_MEM_DL1);
	return 0;
}

static int mtk_I2S0dl1_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dl1_i2s0Dl1_of_ids[] = {

	{.compatible = "mediatek," MT_SOC_I2S0DL1_PCM,},
	{}
};
MODULE_DEVICE_TABLE(of, mt_soc_pcm_dl1_i2s0Dl1_of_ids);

#endif

static struct platform_driver mtk_I2S0dl1_driver = {

	.driver = {
		   .name = MT_SOC_I2S0DL1_PCM,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_pcm_dl1_i2s0Dl1_of_ids,
#endif
		   },
	.probe = mtk_I2S0dl1_probe,
	.remove = mtk_I2S0dl1_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkI2S0dl1_dev;
#endif
#ifdef CONFIG_OF
module_platform_driver(mtk_I2S0dl1_driver);
#else
static int __init mtk_I2S0dl1_soc_platform_init(void)
{
	int ret;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkI2S0dl1_dev = platform_device_alloc(MT_SOC_I2S0DL1_PCM, -1);

	if (!soc_mtkI2S0dl1_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkI2S0dl1_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkI2S0dl1_dev);
		return ret;
	}
#endif

	ret = platform_driver_register(&mtk_I2S0dl1_driver);
	return ret;

}
module_init(mtk_I2S0dl1_soc_platform_init);

static void __exit mtk_I2S0dl1_soc_platform_exit(void)
{
	PRINTK_AUDDRV("%s\n", __func__);

	platform_driver_unregister(&mtk_I2S0dl1_driver);
}
module_exit(mtk_I2S0dl1_soc_platform_exit);
#endif
MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
