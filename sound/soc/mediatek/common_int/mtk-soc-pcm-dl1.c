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
#include "mtk-auddrv-gpio.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include "mtk-auddrv-gpio.h"

#include <asm/div64.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#endif

static struct afe_mem_control_t *pMemControl;
static unsigned int mPlaybackDramState;
static struct snd_dma_buffer *Dl1_Playback_dma_buf;

static DEFINE_SPINLOCK(auddrv_DLCtl_lock);

static struct device *mDev;

/*
 *    function implementation
 */

/*void StartAudioPcmHardware(void);*/
/*void StopAudioPcmHardware(void);*/
static int mtk_soc_dl1_probe(struct platform_device *pdev);
static int mtk_soc_pcm_dl1_close(struct snd_pcm_substream *substream);
static int mtk_asoc_dl1_probe(struct snd_soc_platform *platform);

static bool mPrepareDone;

#define USE_RATE (SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000)
#define USE_RATE_MIN 8000
#define USE_RATE_MAX 192000
#define USE_CHANNELS_MIN 1
#define USE_CHANNELS_MAX 2
#define USE_PERIODS_MIN 512
#define USE_PERIODS_MAX 8192

static struct snd_pcm_hardware mtk_pcm_dl1_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
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
	pr_debug("%s\n", __func__);

	irq_remove_user(substream,
			irq_request_number(Soc_Aud_Digital_Block_MEM_DL1));
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

	/* here start digital part */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

	ClearMemBlock(Soc_Aud_Digital_Block_MEM_DL1);
	return 0;
}

static snd_pcm_uframes_t mtk_pcm_pointer(struct snd_pcm_substream *substream)
{
	return get_mem_frame_index(substream, pMemControl,
				   Soc_Aud_Digital_Block_MEM_DL1);
}

static int mtk_pcm_dl1_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *hw_params)
{
	/* struct snd_dma_buffer *dma_buf = &substream->dma_buffer; */
	int ret = 0;

	/* runtime->dma_bytes has to be set manually to allow mmap */
	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

	if (AllocateAudioSram(&substream->runtime->dma_addr,
			      &substream->runtime->dma_area,
			      substream->runtime->dma_bytes, substream,
			      params_format(hw_params), false) == 0) {
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL1, false,
			    substream->runtime->dma_addr);

	} else {
		substream->runtime->dma_area = Dl1_Playback_dma_buf->area;
		substream->runtime->dma_addr = Dl1_Playback_dma_buf->addr;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL1, true,
			    substream->runtime->dma_addr);
		mPlaybackDramState = true;
		AudDrv_Emi_Clk_On();
	}

	set_mem_block(substream, hw_params, pMemControl,
		      Soc_Aud_Digital_Block_MEM_DL1);
#if defined(DL1_DEBUG_LOG)
	pr_debug("dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		      substream->runtime->dma_bytes,
		      substream->runtime->dma_area,
		      (long)substream->runtime->dma_addr);
#endif
	return ret;
}

static int mtk_pcm_dl1_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s substream = %p\n", __func__, substream);
	if (mPlaybackDramState == true) {
		AudDrv_Emi_Clk_Off();
		mPlaybackDramState = false;
	} else
		freeAudioSram((void *)substream);
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

	mPlaybackDramState = false;

	mtk_pcm_dl1_hardware.buffer_bytes_max = GetPLaybackSramFullSize();

	pr_debug("mtk_pcm_dl1_hardware.buffer_bytes_max = %zu mPlaybackDramState = %d\n",
		mtk_pcm_dl1_hardware.buffer_bytes_max, mPlaybackDramState);
	runtime->hw = mtk_pcm_dl1_hardware;

	AudDrv_Clk_On();
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_dl1_hardware,
	       sizeof(struct snd_pcm_hardware));
	pMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);

	if (ret < 0) {
		pr_err("ret < 0 mtk_soc_pcm_dl1_close\n");
		mtk_soc_pcm_dl1_close(substream);
		return ret;
	}

	return 0;
}

static int mtk_soc_pcm_dl1_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	if (mPrepareDone == true) {
		/* stop DAC output */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);

		RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
		EnableAfe(false);
		mPrepareDone = false;
	}
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_pcm_prepare(struct snd_pcm_substream *substream)
{
	bool mI2SWLen;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (mPrepareDone == false) {
		pr_debug("%s format = %d\n", __func__, runtime->format);
		SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL1,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		}

		/* start I2S DAC out */
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) ==
		    false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
					    true);
			SetI2SDacOut(substream->runtime->rate, false, mI2SWLen);
			SetI2SDacEnable(true);
		} else {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
					    true);
		}

		EnableAfe(true);
		mPrepareDone = true;
	}
	return 0;
}

static int mtk_pcm_dl1_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);
	/* here start digital part */

	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

	/* here to set interrupt */
	irq_add_user(substream,
		     irq_request_number(Soc_Aud_Digital_Block_MEM_DL1),
		     substream->runtime->rate, substream->runtime->period_size);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

	EnableAfe(true);

	return 0;
}

static int mtk_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
#if defined(DL1_DEBUG_LOG)
	pr_debug("mtk_pcm_trigger cmd = %d\n", cmd);
#endif
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

static int mtk_pcm_copy(struct snd_pcm_substream *substream, int channel,
			snd_pcm_uframes_t pos, void __user *dst,
			snd_pcm_uframes_t count)
{
	struct afe_block_t *Afe_Block = NULL;
	int copy_size = 0, Afe_WriteIdx_tmp;
	unsigned long flags;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	char *data_w_ptr = (char *)dst;
#ifdef DL1_DEBUG_LOG
	pr_debug("mtk_pcm_copy pos = %lu count = %lu\n ", pos, count);
#endif
	/* get total bytes to copy */
	count = audio_frame_to_bytes(substream, count);

	/* check which memif nned to be write */
	Afe_Block = &pMemControl->rBlock;
#ifdef DL1_DEBUG_LOG
	pr_debug(
		"AudDrv_write WriteIdx=0x%x, ReadIdx=0x%x, DataRemained=0x%x\n",
		Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
		Afe_Block->u4DataRemained);
#endif

	if (Afe_Block->u4BufferSize == 0) {
		pr_err("AudDrv_write: u4BufferSize=0 Error");
		return 0;
	}

	AudDrv_checkDLISRStatus();

	spin_lock_irqsave(&auddrv_DLCtl_lock, flags);
	copy_size = Afe_Block->u4BufferSize -
		    Afe_Block->u4DataRemained; /* free space of the buffer */
	spin_unlock_irqrestore(&auddrv_DLCtl_lock, flags);
	if (count <= copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

	copy_size = word_size_align(copy_size);
#ifdef DL1_DEBUG_LOG
	pr_debug("copy_size=0x%x, count=0x%x\n", copy_size,
		       (unsigned int)count);
#endif
	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_DLCtl_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_DLCtl_lock, flags);

		/* copy once */
		if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) {

			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
#if defined(DL1_DEBUG_LOG)
				pr_debug(
					"AudDrv_write 0ptr invalid data_w_ptr=%p, size=%d u4BufferSize=%d, u4DataRemained=%d",
					data_w_ptr, copy_size,
					Afe_Block->u4BufferSize,
					Afe_Block->u4DataRemained);
#endif
			} else {
#ifdef DL1_DEBUG_LOG
				pr_debug(
					"memcpy VirtBufAddr+Afe_WriteIdx= %p,data_w_ptr = %p copy_size = 0x%x\n",
					Afe_Block->pucVirtBufAddr +
						Afe_WriteIdx_tmp,
					data_w_ptr, copy_size);
#endif
				if (copy_from_user((Afe_Block->pucVirtBufAddr +
						    Afe_WriteIdx_tmp),
						   data_w_ptr, copy_size)) {
#if defined(DL1_DEBUG_LOG)
					pr_debug(
						"AudDrv_write Fail copy from user\n");
#endif
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
#ifdef DL1_DEBUG_LOG
			pr_debug(
				"AudDrv_write finish1, copy:%x, WriteIdx:%x,ReadIdx=%x,Remained:%x, count=%d \r\n",
				copy_size, Afe_Block->u4WriteIdx,
				Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained, (int)count);
#endif
		} else {
			/* copy twice */
			kal_uint32 size_1 = 0, size_2 = 0;

			size_1 = word_size_align(
				(Afe_Block->u4BufferSize - Afe_WriteIdx_tmp));
			size_2 = word_size_align((copy_size - size_1));
#ifdef DL1_DEBUG_LOG
			pr_debug("size_1=0x%x, size_2=0x%x\n", size_1,
				       size_2);
#endif
			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
				pr_err("AudDrv_write 1ptr invalid data_w_ptr=%p, size_1=%d u4BufferSize=%d, u4DataRemained=%d",
				       data_w_ptr, size_1,
				       Afe_Block->u4BufferSize,
				       Afe_Block->u4DataRemained);
			} else {
#ifdef DL1_DEBUG_LOG
				pr_debug(
					"mcmcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx= %p data_w_ptr = %p size_1 = %x\n",
					Afe_Block->pucVirtBufAddr +
						Afe_WriteIdx_tmp,
					data_w_ptr, size_1);
#endif
				if ((copy_from_user((Afe_Block->pucVirtBufAddr +
						     Afe_WriteIdx_tmp),
						    data_w_ptr,
						    (unsigned int)size_1))) {
#if defined(DL1_DEBUG_LOG)
					pr_debug(
						"AudDrv_write Fail 1 copy from user");
#endif
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_DLCtl_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&auddrv_DLCtl_lock, flags);

			if (!access_ok(VERIFY_READ, data_w_ptr + size_1,
				       size_2)) {
#if defined(DL1_DEBUG_LOG)
				pr_debug(
					"AudDrv_write 2ptr invalid data_w_ptr=%p, size_1=%d, size_2=%d u4BufferSize=%d, u4DataRemained=%d",
					data_w_ptr, size_1, size_2,
					Afe_Block->u4BufferSize,
					Afe_Block->u4DataRemained);
#endif
			} else {
#ifdef DL1_DEBUG_LOG
				pr_debug(
					"mcmcpy VirtBufAddr+Afe_WriteIdx= %p,data_w_ptr+size_1 = %p size_2 = %x\n",
					Afe_Block->pucVirtBufAddr +
						Afe_WriteIdx_tmp,
					data_w_ptr + size_1,
					(unsigned int)size_2);
#endif
				if ((copy_from_user((Afe_Block->pucVirtBufAddr +
						     Afe_WriteIdx_tmp),
						    (data_w_ptr + size_1),
						    size_2))) {
#if defined(DL1_DEBUG_LOG)
					pr_debug(
						"AudDrv_write Fail 2  copy from user");
#endif
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
#ifdef DL1_DEBUG_LOG

			pr_debug(
				"AudDrv_write finish2, copy size:%x, WriteIdx:%x,ReadIdx=%x DataRemained:%x \r\n",
				copy_size, Afe_Block->u4WriteIdx,
				Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained);
#endif
		}
	}
	return 0;
}

static int mtk_pcm_silence(struct snd_pcm_substream *substream, int channel,
			   snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
#if defined(DL1_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_pcm_page(struct snd_pcm_substream *substream,
				 unsigned long offset)
{
#if defined(DL1_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
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
	.ops = &mtk_afe_ops, .probe = mtk_asoc_dl1_probe,
};

static int mtk_asoc_dl1_probe(struct snd_soc_platform *platform)
{
#if defined(DL1_DEBUG_LOG)
	pr_debug("mtk_asoc_dl1_probe\n");
#endif
	/* allocate dram */
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL1,
				   Dl1_MAX_BUFFER_SIZE);
	Dl1_Playback_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);
	return 0;
}

static int mtk_afe_remove(struct platform_device *pdev)
{
#if defined(DL1_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	AudDrv_Clk_Deinit(&pdev->dev);

	snd_soc_unregister_platform(&pdev->dev);

	return 0;
}

#ifdef CONFIG_OF
/*extern void *AFE_BASE_ADDRESS;*/
u32 afe_irq_number;

static const struct of_device_id mt_soc_pcm_dl1_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_dl1",
	},
	{} };

static int auddrv_get_irqline(void *dev)
{
	struct device *pdev = dev;

	if (!pdev->of_node) {
		pr_err("%s invalid of_node\n", __func__);
		return -ENODEV;
	}

	/*get afe irq num */
	afe_irq_number = irq_of_parse_and_map(pdev->of_node, 0);

	pr_debug("[ge_mt_soc_pcm_dl1] afe_irq_number=%d\n", afe_irq_number);

	if (!afe_irq_number) {
		pr_err("[ge_mt_soc_pcm_dl1] get afe_irq_number failed!!!\n");
		return -ENODEV;
	}

	return 0;
}

#endif

static void DL1GlobalVarInit(void)
{
	pMemControl = NULL;

	mPlaybackDramState = 0;

	Dl1_Playback_dma_buf = NULL;

	mDev = NULL;

	mPrepareDone = false;
}

static int mtk_soc_dl1_probe(struct platform_device *pdev)
{
	int ret = 0;

	mDev = &pdev->dev;
#if defined(DL1_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", MT_SOC_DL1_PCM);
	} else {
		pr_err("%s invalid of_node\n", __func__);
		return -ENODEV;
	}

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	DL1GlobalVarInit();

#ifdef CONFIG_OF
	AudDrv_Clk_probe(&pdev->dev);

#ifndef CONFIG_MTK_LEGACY
	AudDrv_GPIO_probe(&pdev->dev);
#endif
#endif

	InitAfeControl(&pdev->dev);

#ifdef CONFIG_OF
	auddrv_get_irqline(&pdev->dev);
	ret = Register_Aud_Irq(&pdev->dev, afe_irq_number);
#else
	ret = Register_Aud_Irq(&pdev->dev, MT6735_AFE_MCU_IRQ_LINE);
#endif

	/* config smartpa gpio pins, set initial state : SMARTPA_OFF */
	AudDrv_GPIO_SMARTPA_Select(0);

	return snd_soc_register_platform(&pdev->dev, &mtk_soc_platform);
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
#if defined(DL1_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
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
	platform_driver_unregister(&mtk_afe_driver);
}
module_exit(mtk_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
