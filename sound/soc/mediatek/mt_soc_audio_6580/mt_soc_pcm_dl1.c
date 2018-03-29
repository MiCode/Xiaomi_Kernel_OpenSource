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
 *   mt_soc_pcm_afe.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio dl1 data1 playback
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
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
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"
#include "AudDrv_Gpio.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
/*#include <mach/irqs.h>*/
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
/*#include <mach/mt_reg_base.h>*/
#include <asm/div64.h>
/*
#include <linux/aee.h>
#include <mach/upmu_common.h>
#include <mach/upmu_hw.h>
#include <mach/mt_gpio.h>*/
/*#include <mach/mt_typedefs.h>*/
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
/* #include <asm/mach-types.h> */

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#ifdef CONFIG_MTK_LEGACY
static unsigned int pin_extspkamp, pin_vowclk, pin_audclk, pin_audmiso,
	pin_audmosi, pin_i2s1clk, pin_i2s1dat, pin_i2s1mclk, pin_i2s1ws;
static unsigned int pin_mode_audclk, pin_mode_audmosi, pin_mode_audmiso,
	pin_mode_vowclk, pin_mode_extspkamp, pin_mode_i2s1clk, pin_mode_i2s1dat,
	pin_mode_i2s1mclk, pin_mode_i2s1ws;
static unsigned int if_config1, if_config2, if_config3, if_config4, if_config5,
	if_config6, if_config7, if_config8, if_config9;
#endif
#endif

static AFE_MEM_CONTROL_T *pMemControl;
static int mPlaybackSramState;
static struct snd_dma_buffer *Dl1_Playback_dma_buf;

static DEFINE_SPINLOCK(auddrv_DLCtl_lock);

static struct device *mDev;

/*
 *    function implementation
 */

/* void StartAudioPcmHardware(void); */
/* void StopAudioPcmHardware(void); */
static int mtk_soc_dl1_probe(struct platform_device *pdev);
static int mtk_soc_pcm_dl1_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_dl1_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_asoc_dl1_probe(struct snd_soc_platform *platform);

static bool mPrepareDone;
static bool mPlaybackUseSram;

#define USE_RATE        (SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000)
#define USE_RATE_MIN        8000
#define USE_RATE_MAX        192000
#define USE_CHANNELS_MIN     1
#define USE_CHANNELS_MAX    2
#define USE_PERIODS_MIN     512
#define USE_PERIODS_MAX     8192

static struct snd_pcm_hardware mtk_pcm_dl1_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.period_bytes_max = Dl1_MAX_PERIOD_SIZE,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int mtk_pcm_dl1_stop(struct snd_pcm_substream *substream)
{
	pr_warn("%s\n", __func__);

	irq_remove_user(substream, Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

	/* here start digital part */
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O04);

	ClearMemBlock(Soc_Aud_Digital_Block_MEM_DL1);
	return 0;
}

static snd_pcm_uframes_t mtk_pcm_pointer(struct snd_pcm_substream *substream)
{
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	kal_uint32 Frameidx = 0;
	kal_int32 Afe_consumed_bytes = 0;
	AFE_BLOCK_T *Afe_Block = &pMemControl->rBlock;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	PRINTK_AUD_DL1(" %s Afe_Block->u4DMAReadIdx = 0x%x\n", __func__, Afe_Block->u4DMAReadIdx);

	Auddrv_Dl1_Spinlock_lock();

	/* get total bytes to copy */
	/* Frameidx = audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx); */
	/* return Frameidx; */

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == true) {
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL1_CUR);
		if (HW_Cur_ReadIdx == 0) {
			PRINTK_AUDDRV("[Auddrv] HW_Cur_ReadIdx ==0\n");
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}

		HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);
		if (HW_memory_index >= Afe_Block->u4DMAReadIdx) {
			Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
		} else {
			Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index -
			    Afe_Block->u4DMAReadIdx;
		}

#ifdef AUDIO_64BYTE_ALIGN	/* no need to do 64byte align */
		Afe_consumed_bytes = Align64ByteSize(Afe_consumed_bytes);
#endif

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
		PRINTK_AUD_DL1
		    ("[Auddrv] HW_Cur_ReadIdx =0x%x HW_memory_index = 0x%x Afe_consumed_bytes  = 0x%x\n",
		     HW_Cur_ReadIdx, HW_memory_index, Afe_consumed_bytes);
		Auddrv_Dl1_Spinlock_unlock();

		return audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	}

	Frameidx = audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	Auddrv_Dl1_Spinlock_unlock();
	return Frameidx;
}

static void SetDL1Buffer(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	AFE_BLOCK_T *pblock = &pMemControl->rBlock;

	pblock->pucPhysBufAddr = runtime->dma_addr;
	pblock->pucVirtBufAddr = runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;
	pr_warn("SetDL1Buffer u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
	       pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
	/* set dram address top hardware */
	Afe_Set_Reg(AFE_DL1_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_DL1_END, pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);
	memset_io((void *)pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);

}

static int mtk_pcm_dl1_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *hw_params)
{
	/* struct snd_dma_buffer *dma_buf = &substream->dma_buffer; */
	int ret = 0;

	PRINTK_AUDDRV("mtk_pcm_dl1_params\n");

	/* runtime->dma_bytes has to be set manually to allow mmap */
	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

	if (mPlaybackSramState == SRAM_STATE_PLAYBACKFULL) {
		/* substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE; */
		substream->runtime->dma_area = (unsigned char *)Get_Afe_SramBase_Pointer();
		substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;
		AudDrv_Allocate_DL1_Buffer(mDev, substream->runtime->dma_bytes);
	} else {
		substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
		substream->runtime->dma_area = Dl1_Playback_dma_buf->area;
		substream->runtime->dma_addr = Dl1_Playback_dma_buf->addr;
		SetDL1Buffer(substream, hw_params);
	}

	PRINTK_AUDDRV("dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		      substream->runtime->dma_bytes, substream->runtime->dma_area,
		      (long)substream->runtime->dma_addr);
	return ret;
}

static int mtk_pcm_dl1_hw_free(struct snd_pcm_substream *substream)
{
	PRINTK_AUDDRV("mtk_pcm_dl1_hw_free\n");
	return 0;
}


static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_dl1_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	PRINTK_AUDDRV("mtk_pcm_dl1_open\n");

	AfeControlSramLock();
	if (GetSramState() == SRAM_STATE_FREE) {
		mtk_pcm_dl1_hardware.buffer_bytes_max = GetPLaybackSramFullSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKFULL;
		SetSramState(mPlaybackSramState);
		mPlaybackUseSram = true;
	} else {
		mtk_pcm_dl1_hardware.buffer_bytes_max = GetPLaybackDramSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKDRAM;
		mPlaybackUseSram = false;
	}
	AfeControlSramUnLock();
	if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
		AudDrv_Emi_Clk_On();

	pr_warn("mtk_pcm_dl1_hardware.buffer_bytes_max = %zu mPlaybackSramState = %d\n",
	       mtk_pcm_dl1_hardware.buffer_bytes_max, mPlaybackSramState);
	runtime->hw = mtk_pcm_dl1_hardware;

	AudDrv_Clk_On();
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_dl1_hardware,
	       sizeof(struct snd_pcm_hardware));
	pMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);

	if (ret < 0)
		pr_err("snd_pcm_hw_constraint_integer failed\n");

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_warn("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_dl1playback_constraints\n");
	else
		pr_warn("SNDRV_PCM_STREAM_CAPTURE mtkalsa_dl1playback_constraints\n");

	if (ret < 0) {
		pr_err("ret < 0 mtk_soc_pcm_dl1_close\n");
		mtk_soc_pcm_dl1_close(substream);
		return ret;
	}
	/* PRINTK_AUDDRV("mtk_pcm_dl1_open return\n"); */
	return 0;
}

static int mtk_soc_pcm_dl1_close(struct snd_pcm_substream *substream)
{
	pr_warn("%s\n", __func__);

	if (mPrepareDone == true) {
		/* stop DAC output */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);

		RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

		EnableAfe(false);
		mPrepareDone = false;
	}

	if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
		AudDrv_Emi_Clk_Off();

	AfeControlSramLock();
	ClearSramState(mPlaybackSramState);
	mPlaybackSramState = GetSramState();
	AfeControlSramUnLock();
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_pcm_prepare(struct snd_pcm_substream *substream)
{
	bool mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (mPrepareDone == false) {
		pr_warn
		    ("%s format = %d SNDRV_PCM_FORMAT_S32_LE = %d SNDRV_PCM_FORMAT_U32_LE = %d\n",
		     __func__, runtime->format, SNDRV_PCM_FORMAT_S32_LE, SNDRV_PCM_FORMAT_U32_LE);
		SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
			/* not support 24bit +++ */
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
						     AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);

			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
						  Soc_Aud_InterConnectionOutput_O03);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
						  Soc_Aud_InterConnectionOutput_O04);
			/* not support 24bit --- */
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			/* not support 24bit +++ */
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
						     AFE_WLEN_16_BIT);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
						  Soc_Aud_InterConnectionOutput_O03);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
						  Soc_Aud_InterConnectionOutput_O04);
			/* not support 24bit --- */
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		}

		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, runtime->rate);

		/* start I2S DAC out */
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
			SetI2SDacOut(substream->runtime->rate, false, mI2SWLen);
			SetI2SDacEnable(true);
		} else {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
		}

		EnableAfe(true);
		mPrepareDone = true;
	}
	return 0;
}


static int mtk_pcm_dl1_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_warn("%s\n", __func__);
	/* here start digital part */

	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O04);

#ifdef CONFIG_FPGA_EARLY_PORTING
	/* set loopback test interconnection */
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O09);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O10);
#endif

	/* here to set interrupt */
	irq_add_user(substream,
		     Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE,
		     substream->runtime->rate,
		     substream->runtime->period_size);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

	EnableAfe(true);
	return 0;
}

static int mtk_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	PRINTK_AUDDRV("mtk_pcm_trigger cmd = %d\n", cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_dl1_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_dl1_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_copy(struct snd_pcm_substream *substream,
			int channel, snd_pcm_uframes_t pos,
			void __user *dst, snd_pcm_uframes_t count)
{
	AFE_BLOCK_T *Afe_Block = NULL;
	int copy_size = 0, Afe_WriteIdx_tmp;
	unsigned long flags;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	char *data_w_ptr = (char *)dst;

	PRINTK_AUD_DL1("mtk_pcm_copy pos = %lu count = %lu\n ", pos, count);
	/* get total bytes to copy */
	count = audio_frame_to_bytes(substream, count);

	/* check which memif nned to be write */
	Afe_Block = &pMemControl->rBlock;

	PRINTK_AUD_DL1("AudDrv_write WriteIdx=0x%x, ReadIdx=0x%x, DataRemained=0x%x\n",
		       Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);

	if (Afe_Block->u4BufferSize == 0) {
		pr_err("AudDrv_write: u4BufferSize=0 Error");
		return 0;
	}

	AudDrv_checkDLISRStatus();

	spin_lock_irqsave(&auddrv_DLCtl_lock, flags);
	copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;	/* free space of the buffer */
	spin_unlock_irqrestore(&auddrv_DLCtl_lock, flags);
	if (count <= copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

#ifdef AUDIO_64BYTE_ALIGN	/* no need to do 64byte align */
	copy_size = Align64ByteSize(copy_size);
#endif
	PRINTK_AUD_DL1("copy_size=0x%x, count=0x%x\n", copy_size, (unsigned int)count);

	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_DLCtl_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_DLCtl_lock, flags);

		if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) {	/* copy once */
			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
				PRINTK_AUDDRV("AudDrv_write 0ptr invalid data_w_ptr=%p, size=%d",
					      data_w_ptr, copy_size);
				PRINTK_AUDDRV("AudDrv_write u4BufferSize=%d, u4DataRemained=%d",
					      Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {
				PRINTK_AUD_DL1
				    ("memcpy VirtBufAddr+Afe_WriteIdx= %p,data_w_ptr = %p copy_size = 0x%x\n",
				     Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr,
				     copy_size);
				if (mtk_local_audio_copy_from_user
				    (mPlaybackUseSram, (Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr,
				     copy_size)) {
					PRINTK_AUDDRV("AudDrv_write Fail copy from user\n");
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_DLCtl_lock, flags);
			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_DLCtl_lock, flags);
			data_w_ptr += copy_size;
			count -= copy_size;

			PRINTK_AUD_DL1
			    ("AudDrv_write finish1, copy:%x, WriteIdx:%x,ReadIdx=%x,Remained:%x, count=%d \r\n",
			     copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
			     Afe_Block->u4DataRemained, (int)count);

		} else {	/* copy twice */
			kal_uint32 size_1 = 0, size_2 = 0;
#ifdef AUDIO_64BYTE_ALIGN	/* no need to do 64byte align */
			size_1 = Align64ByteSize((Afe_Block->u4BufferSize - Afe_WriteIdx_tmp));
			size_2 = Align64ByteSize((copy_size - size_1));
#else
			size_1 = Afe_Block->u4BufferSize - Afe_WriteIdx_tmp;
			size_2 = copy_size - size_1;
#endif
			PRINTK_AUD_DL1("size_1=0x%x, size_2=0x%x\n", size_1, size_2);
			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
				pr_err("AudDrv_write 1ptr invalid data_w_ptr=%p, size_1=%d",
				       data_w_ptr, size_1);
				pr_err("AudDrv_write u4BufferSize=%d, u4DataRemained=%d",
				       Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {

				PRINTK_AUD_DL1
				    ("mcmcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx= %p data_w_ptr = %p size_1 = %x\n",
				     Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr,
				     size_1);
				if ((mtk_local_audio_copy_from_user
				     (mPlaybackUseSram, (Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr,
				      (unsigned int)size_1))) {
					PRINTK_AUDDRV("AudDrv_write Fail 1 copy from user");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_DLCtl_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&auddrv_DLCtl_lock, flags);

			if (!access_ok(VERIFY_READ, data_w_ptr + size_1, size_2)) {
				PRINTK_AUDDRV
				    ("AudDrv_write 2ptr invalid data_w_ptr=%p, size_1=%d, size_2=%d",
				     data_w_ptr, size_1, size_2);
				PRINTK_AUDDRV("AudDrv_write u4BufferSize=%d, u4DataRemained=%d",
					      Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {

				PRINTK_AUD_DL1
				    ("mcmcpy VirtBufAddr+Afe_WriteIdx= %p,data_w_ptr+size_1 = %p size_2 = %x\n",
				     Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp,
				     data_w_ptr + size_1, (unsigned int)size_2);
				if ((mtk_local_audio_copy_from_user
				     (mPlaybackUseSram, (Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
				      (data_w_ptr + size_1), size_2))) {
					PRINTK_AUDDRV("AudDrv_write Fail 2  copy from user");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_DLCtl_lock, flags);

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_DLCtl_lock, flags);
			count -= copy_size;
			data_w_ptr += copy_size;

			PRINTK_AUD_DL1
			    ("AudDrv_write finish2, copy size:%x, WriteIdx:%x,ReadIdx=%x DataRemained:%x \r\n",
			     copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
			     Afe_Block->u4DataRemained);
		}
	}
	return 0;
}

static int mtk_pcm_silence(struct snd_pcm_substream *substream,
			   int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	PRINTK_AUDDRV("%s\n", __func__);
	return 0;		/* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_pcm_page(struct snd_pcm_substream *substream, unsigned long offset)
{
	PRINTK_AUDDRV("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]);	/* the same page */
}

static struct snd_pcm_ops mtk_afe_ops = {
	.open = mtk_pcm_dl1_open,
	.close = mtk_soc_pcm_dl1_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_dl1_params,
	.hw_free = mtk_pcm_dl1_hw_free,
	.prepare = mtk_pcm_prepare,
	.trigger = mtk_pcm_trigger,
	.pointer = mtk_pcm_pointer,
	.copy = mtk_pcm_copy,
	.silence = mtk_pcm_silence,
	.page = mtk_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_platform = {
	.ops = &mtk_afe_ops,
	.pcm_new = mtk_asoc_pcm_dl1_new,
	.probe = mtk_asoc_dl1_probe,
};

#ifdef CONFIG_OF
/* extern void *AFE_BASE_ADDRESS; */
u32 afe_irq_number = 0;
int AFE_BASE_PHY;

static const struct of_device_id mt_soc_pcm_dl1_of_ids[] = {
	{.compatible = "mediatek,mt_soc_pcm_dl1",},
	{}
};

static int Auddrv_Reg_map_new(void *dev)
{
	struct device *pdev = dev;

	if (!pdev->of_node) {
		pr_err("%s invalid of_node\n", __func__);
		return -ENODEV;
	}

	/*get afe irq num */
	afe_irq_number = irq_of_parse_and_map(pdev->of_node, 0);

	pr_warn("[ge_mt_soc_pcm_dl1] afe_irq_number=%d\n", afe_irq_number);

	if (!afe_irq_number) {
		pr_err("[ge_mt_soc_pcm_dl1] get afe_irq_number failed!!!\n");
		return -ENODEV;
	}

	if (pdev->of_node) {
		/* Setup IO addresses */
		AFE_BASE_ADDRESS = of_iomap(pdev->of_node, 0);
		pr_warn("[ge_mt_soc_pcm_dl1] AFE_BASE_ADDRESS=0x%p\n", AFE_BASE_ADDRESS);
	} else {
		pr_err("[mt_soc_pcm_dl1] node NULL, can't iomap AFE_BASE!!!\n");
		BUG();
		return -ENODEV;
	}

	if (pdev->of_node) {
		/* Setup IO addresses */
		of_property_read_u32(pdev->of_node, "reg", &AFE_BASE_PHY);
		pr_warn("[ge_mt_soc_pcm_dl1] AFE_BASE_PHY=0x%x\n", AFE_BASE_PHY);
	} else {
		pr_err("[mt_soc_pcm_dl1] node NULL, can't iomap AFE_BASE_PHY!!!\n");
		BUG();
		return -ENODEV;
	}
	return 0;
}

#ifdef CONFIG_MTK_LEGACY
static int Auddrv_OF_ParseGPIO(void *dev)
{
	/* struct device_node *node = NULL; */
	struct device *pdev = dev;

	if (!pdev->of_node) {
		pr_err("%s invalid of_node\n", __func__);
		return -ENODEV;
	}

	/* node = of_find_compatible_node(NULL, NULL, "mediatek,mt-soc-dl1-pcm"); */
	if (pdev->of_node) {
		if_config1 = 1;
		if_config2 = 1;
		if_config3 = 1;
		if_config4 = 1;
		if_config5 = 1;
		if_config6 = 1;
		if_config7 = 1;
		if_config8 = 1;
		if_config9 = 1;

		if (of_property_read_u32_index(pdev->of_node, "audclk-gpio", 0, &pin_audclk)) {
			if_config1 = 0;
			pr_err("audclk-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(pdev->of_node, "audclk-gpio", 1, &pin_mode_audclk)) {
			if_config1 = 0;
			pr_err("audclk-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(pdev->of_node, "audmiso-gpio", 0, &pin_audmiso)) {
			if_config2 = 0;
			pr_err("audmiso-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(pdev->of_node, "audmiso-gpio", 1, &pin_mode_audmiso)) {
			if_config2 = 0;
			pr_err("audmiso-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(pdev->of_node, "audmosi-gpio", 0, &pin_audmosi)) {
			if_config3 = 0;
			pr_err("audmosi-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(pdev->of_node, "audmosi-gpio", 1, &pin_mode_audmosi)) {
			if_config3 = 0;
			pr_err("audmosi-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(pdev->of_node, "vowclk-gpio", 0, &pin_vowclk)) {
			if_config4 = 0;
			pr_err("vowclk-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(pdev->of_node, "vowclk-gpio", 1, &pin_mode_vowclk)) {
			if_config4 = 0;
			pr_err("vowclk-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(pdev->of_node, "extspkamp-gpio", 0, &pin_extspkamp)) {
			if_config5 = 0;
			pr_err("extspkamp-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(pdev->of_node, "extspkamp-gpio", 1, &pin_mode_extspkamp)) {
			if_config5 = 0;
			pr_err("extspkamp-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(pdev->of_node, "i2s1clk-gpio", 0, &pin_i2s1clk)) {
			if_config6 = 0;
			pr_err("i2s1clk-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(pdev->of_node, "i2s1clk-gpio", 1, &pin_mode_i2s1clk)) {
			if_config6 = 0;
			pr_err("i2s1clk-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(pdev->of_node, "i2s1dat-gpio", 0, &pin_i2s1dat)) {
			if_config7 = 0;
			pr_err("i2s1dat-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(pdev->of_node, "i2s1dat-gpio", 1, &pin_mode_i2s1dat)) {
			if_config7 = 0;
			pr_err("i2s1dat-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(pdev->of_node, "i2s1mclk-gpio", 0, &pin_i2s1mclk)) {
			if_config8 = 0;
			pr_err("i2s1mclk-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(pdev->of_node, "i2s1mclk-gpio", 1, &pin_mode_i2s1mclk)) {
			if_config8 = 0;
			pr_err("i2s1mclk-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(pdev->of_node, "i2s1ws-gpio", 0, &pin_i2s1ws)) {
			if_config9 = 0;
			pr_err("i2s1ws-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(pdev->of_node, "i2s1ws-gpio", 1, &pin_mode_i2s1ws)) {
			if_config9 = 0;
			pr_err("i2s1ws-gpio get pin_mode fail!!!\n");
		}

		pr_warn("Auddrv_OF_ParseGPIO pin_audclk=%d, pin_audmiso=%d, pin_audmosi=%d\n",
		       pin_audclk, pin_audmiso, pin_audmosi);
		pr_warn("Auddrv_OF_ParseGPIO pin_vowclk=%d, pin_extspkamp=%d\n", pin_vowclk,
		       pin_extspkamp);
		pr_warn
		    ("Auddrv_OF_ParseGPIO pin_i2s1clk=%d, pin_i2s1dat=%d, pin_i2s1mclk=%d, pin_i2s1ws=%d\n",
		     pin_i2s1clk, pin_i2s1dat, pin_i2s1mclk, pin_i2s1ws);
	} else {
		pr_err("Auddrv_OF_ParseGPIO node NULL!!!\n");
		return -1;
	}
	return 0;
}

int GetGPIO_Info(int type, int *pin, int *pinmode)
{
	switch (type) {
	case 1:		/* pin_audclk */
		if (if_config1 == 1) {
			*pin = pin_audclk | 0x80000000;
			*pinmode = pin_mode_audclk;
		} else {
			pr_err("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 2:		/* pin_audmiso */
		if (if_config2 == 1) {
			*pin = pin_audmiso | 0x80000000;
			*pinmode = pin_mode_audmiso;
		} else {
			pr_err("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 3:		/* pin_audmosi */
		if (if_config3 == 1) {
			*pin = pin_audmosi | 0x80000000;
			*pinmode = pin_mode_audmosi;
		} else {
			pr_err("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 4:		/* pin_vowclk */
		if (if_config4 == 1) {
			*pin = pin_vowclk | 0x80000000;
			*pinmode = pin_mode_vowclk;
		} else {
			pr_err("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 5:		/* pin_extspkamp */
		if (if_config5 == 1) {
			*pin = pin_extspkamp | 0x80000000;
			*pinmode = pin_mode_extspkamp;
		} else {
			pr_err("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 6:		/* pin_i2s1clk */
		if (if_config6 == 1) {
			*pin = pin_i2s1clk | 0x80000000;
			*pinmode = pin_mode_i2s1clk;
		} else {
			pr_err("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 7:		/* pin_i2s1dat */
		if (if_config7 == 1) {
			*pin = pin_i2s1dat | 0x80000000;
			*pinmode = pin_mode_i2s1dat;
		} else {
			pr_err("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 8:		/* pin_i2s1mclk */
		if (if_config8 == 1) {
			*pin = pin_i2s1mclk | 0x80000000;
			*pinmode = pin_mode_i2s1mclk;
		} else {
			pr_err("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 9:		/* pin_i2s1ws */
		if (if_config9 == 1) {
			*pin = pin_i2s1ws | 0x80000000;
			*pinmode = pin_mode_i2s1ws;
		} else {
			pr_err("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	default:
		*pin = -1;
		*pinmode = -1;
		pr_err("Auddrv_OF_ParseGPIO invalid type=%d!!!\n", type);
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(GetGPIO_Info);
#endif
#endif

static void DL1GlobalVarInit(void)
{
	pMemControl = NULL;
	mPlaybackSramState = 0;
	Dl1_Playback_dma_buf = NULL;
	mDev = NULL;
	mPrepareDone = false;
}

static int mtk_soc_dl1_probe(struct platform_device *pdev)
{
	int ret = 0;

	PRINTK_AUDDRV("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", MT_SOC_DL1_PCM);
	} else {
		pr_err("%s invalid of_node\n", __func__);
		return -ENODEV;
	}

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	DL1GlobalVarInit();

#ifdef CONFIG_OF
	ret = Auddrv_Reg_map_new(&pdev->dev);
	if (ret) {
		BUG();
		return -ENODEV;
	}
	ret = Register_Aud_Irq(NULL, afe_irq_number);
	if (ret) {
		BUG();
		return -ENODEV;
	}

#ifndef CONFIG_MTK_LEGACY
	AudDrv_GPIO_probe(&pdev->dev);
#else
	ret = Auddrv_OF_ParseGPIO(&pdev->dev);
	if (ret) {
		BUG();
		return -ENODEV;
	}
#endif

#else
	ret = Register_Aud_Irq(&pdev->dev, MT6580_AFE_MCU_IRQ_LINE);
#endif

	ret = InitAfeControl();
	mDev = &pdev->dev;

	return snd_soc_register_platform(&pdev->dev, &mtk_soc_platform);
}

static int mtk_asoc_pcm_dl1_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	PRINTK_AUDDRV("%s\n", __func__);
	return ret;
}


static int mtk_asoc_dl1_probe(struct snd_soc_platform *platform)
{
	PRINTK_AUDDRV("mtk_asoc_dl1_probe\n");
	/* allocate dram */
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL1,
				   Dl1_MAX_BUFFER_SIZE);
	Dl1_Playback_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);
	return 0;
}

static int mtk_afe_remove(struct platform_device *pdev)
{
	PRINTK_AUDDRV("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_afe_driver = {
	.driver = {
		   .name = MT_SOC_DL1_PCM,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_pcm_dl1_of_ids,
#endif
		   },
	.probe = mtk_soc_dl1_probe,
	.remove = mtk_afe_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkafe_dev;
#endif

static int __init mtk_soc_platform_init(void)
{
	int ret;

	PRINTK_AUDDRV("%s\n", __func__);

#ifndef CONFIG_OF
	soc_mtkafe_dev = platform_device_alloc(MT_SOC_DL1_PCM, -1);
	if (!soc_mtkafe_dev)
		return -ENOMEM;


	ret = platform_device_add(soc_mtkafe_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkafe_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_afe_driver);
	return ret;

}
module_init(mtk_soc_platform_init);

static void __exit mtk_soc_platform_exit(void)
{
	PRINTK_AUDDRV("%s\n", __func__);

	platform_driver_unregister(&mtk_afe_driver);
}
module_exit(mtk_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
