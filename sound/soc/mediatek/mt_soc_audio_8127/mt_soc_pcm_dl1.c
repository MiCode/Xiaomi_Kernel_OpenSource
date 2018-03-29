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

#include "mt_soc_afe_common.h"
#include "mt_soc_afe_def.h"
#include "mt_soc_afe_reg.h"
#include "mt_soc_afe_clk.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"
#include "mt_soc_afe_gpio.h"

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
/* #include <linux/xlog.h> */
/* #include <mach/irqs.h> */
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
/* #include <mach/mt_reg_base.h> */
#include <asm/div64.h>
/* #include <linux/aee.h> */
/* #include <mach/upmu_common.h> */
#include <mt-plat/upmu_common.h>
/*#include <mach/upmu_hw.h>*/
/* #include <mach/mt_gpio.h> */
#include <mt-plat/mt_gpio.h>
/* #include <mach/mt_typedefs.h> */
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
static unsigned int pin_extspkamp, pin_extspkamp_2, pin_vowclk, pin_audclk, pin_audmiso,
	pin_audmosi, pin_i2s1clk, pin_i2s1dat, pin_i2s1mclk, pin_i2s1ws, pin_rcvspkswitch;

static unsigned int pin_mode_audclk, pin_mode_audmosi, pin_mode_audmiso, pin_mode_vowclk,
	pin_mode_extspkamp, pin_mode_extspkamp_2, pin_mode_i2s1clk, pin_mode_i2s1dat, pin_mode_i2s1mclk,
	pin_mode_i2s1ws, pin_mode_rcvspkswitch;

static unsigned int if_config1, if_config2, if_config3, if_config4, if_config5, if_config6,
	if_config7, if_config8, if_config9, if_config10, if_config11;
#endif
#endif

static struct AFE_MEM_CONTROL_T *pMemControl;
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

struct mt_pcm_dl1_priv {
	bool prepared;
	bool enable_mtk_interface;
	bool enable_i2s0;
	unsigned int playback_mux;
	unsigned int i2s0_clock_mode;
};
static int mtk_soc_pcm_dl1_prestart(struct snd_pcm_substream *substream);
/*static int mtk_soc_pcm_dl1_post_stop(struct snd_pcm_substream *substream);*/


static struct snd_pcm_hardware mtk_pcm_dl1_hardware = {

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

static int mtk_pcm_dl1_stop(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	mt_afe_set_irq_state(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);
	mt_afe_disable_memory_path(Soc_Aud_Digital_Block_MEM_DL1);

	/* here start digital part */
	mt_afe_set_connection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O03);
	mt_afe_set_connection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O04);

	ClearMemBlock(Soc_Aud_Digital_Block_MEM_DL1);
	return 0;
}

static snd_pcm_uframes_t mtk_pcm_pointer(struct snd_pcm_substream *substream)
{
	int32_t HW_memory_index = 0;
	int32_t HW_Cur_ReadIdx = 0;
	uint32_t Frameidx = 0;
	int32_t Afe_consumed_bytes = 0;
	struct AFE_BLOCK_T *Afe_Block = &pMemControl->rBlock;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	PRINTK_AUD_DL1("%s Afe_Block->u4DMAReadIdx = 0x%x\n", __func__, Afe_Block->u4DMAReadIdx);

	afe_dl1_spinlock_lock();

	/* get total bytes to copy */
	/* Frameidx = audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx); */
	/* return Frameidx; */

	if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_DL1) == true) {
		HW_Cur_ReadIdx = mt_afe_get_reg(AFE_DL1_CUR);
		if (HW_Cur_ReadIdx == 0) {
			PRINTK_AUDDRV("[%s] HW_Cur_ReadIdx == 0\n", __func__);
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}

		HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

		if (HW_memory_index >= Afe_Block->u4DMAReadIdx)
			Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
		else {
			Afe_consumed_bytes =
			    Afe_Block->u4BufferSize + HW_memory_index - Afe_Block->u4DMAReadIdx;
		}

		Afe_consumed_bytes = align64bytesize(Afe_consumed_bytes);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;

		PRINTK_AUD_DL1
			("[%s] HW_Cur_ReadIdx = 0x%x HW_memory_index = 0x%x Afe_consumed_byte = 0x%x\n",
			__func__, HW_Cur_ReadIdx, HW_memory_index, Afe_consumed_bytes);

		afe_dl1_spinlock_unlock();

		return audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	}

	Frameidx = audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	afe_dl1_spinlock_unlock();
	return Frameidx;
}

static void set_dl1_buffer(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)/*SetDL1Buffer*/
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct AFE_BLOCK_T *pblock = &pMemControl->rBlock;

	pblock->pucPhysBufAddr = runtime->dma_addr;
	pblock->pucVirtBufAddr = runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;
	pr_debug("set_dl1_buffer u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
	       pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
	/* set dram address top hardware */
	mt_afe_set_reg(AFE_DL1_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	mt_afe_set_reg(AFE_DL1_END, pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1), 0xffffffff);
	memset((void *)pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);

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
		substream->runtime->dma_area = (unsigned char *)mt_afe_get_sram_base_ptr();
		substream->runtime->dma_addr = mt_afe_get_sram_phy_addr();
		afe_allocate_dl1_buffer(mDev, substream->runtime->dma_bytes);
	} else {
		substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
		substream->runtime->dma_area = Dl1_Playback_dma_buf->area;
		substream->runtime->dma_addr = Dl1_Playback_dma_buf->addr;
		set_dl1_buffer(substream, hw_params);
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

	.count = ARRAY_SIZE(soc_normal_supported_sample_rates),
	.list = soc_normal_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_dl1_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	PRINTK_AUDDRV("mtk_pcm_dl1_open\n");

	afe_control_sram_lock();
	if (get_sramstate() == SRAM_STATE_FREE) {
		mtk_pcm_dl1_hardware.buffer_bytes_max = get_playback_sram_fullsize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKFULL;
		set_sramstate(mPlaybackSramState);
	} else {
		mtk_pcm_dl1_hardware.buffer_bytes_max = GetPLaybackDramSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKDRAM;
	}
	afe_control_sram_unlock();

	if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
		mt_afe_emi_clk_on();

	pr_debug("mtk_pcm_dl1_hardware.buffer_bytes_max = %zu mPlaybackSramState = %d\n",
	       mtk_pcm_dl1_hardware.buffer_bytes_max, mPlaybackSramState);
	runtime->hw = mtk_pcm_dl1_hardware;

	mt_afe_main_clk_on();
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_dl1_hardware,
	       sizeof(struct snd_pcm_hardware));
	pMemControl = get_mem_control_t(Soc_Aud_Digital_Block_MEM_DL1);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);

	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_dl1playback_constraints\n");
	else
		pr_debug("SNDRV_PCM_STREAM_CAPTURE mtkalsa_dl1playback_constraints\n");

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
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_dl1_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s\n", __func__);

	if (priv->prepared == true) {
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

		priv->prepared = false;
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

static int mtk_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_dl1_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s rate= %u channels= %u period_size= %lu\n",
		 __func__, runtime->rate, runtime->channels, runtime->period_size);
	/* HW sequence: */
	/* mtk_pcm_dl1_prestart->codec->mtk_pcm_dl1_start */
	if (likely(!priv->prepared)) {
		set_memif_substream(Soc_Aud_Digital_Block_MEM_DL1, substream);
		mtk_soc_pcm_dl1_prestart(substream);
		priv->prepared = true;
	}
	return 0;
}


static int mtk_pcm_dl1_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);
	/* here start digital part */

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

#if 0
static int mtk_soc_pcm_dl1_post_stop(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_dl1_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s\n", __func__);

	if (priv->enable_mtk_interface) {
		mt_afe_disable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_DAC);
		if (!mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_I2S_OUT_DAC))
			mt_afe_disable_i2s_dac();

	mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		  Soc_Aud_InterConnectionOutput_O03);
	mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		  Soc_Aud_InterConnectionOutput_O04);

		priv->enable_mtk_interface = false;
	}

	if (priv->enable_i2s0) {
		mt_afe_disable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_2);
		if (!mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_I2S_OUT_2))
			mt_afe_disable_2nd_i2s_out();

		mt_afe_disable_memory_path(Soc_Aud_Digital_Block_I2S_IN_2);
		if (!mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_I2S_IN_2))
			mt_afe_disable_2nd_i2s_in();

	mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		  Soc_Aud_InterConnectionOutput_O00);
	mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		  Soc_Aud_InterConnectionOutput_O01);


		priv->enable_i2s0 = false;

	}

	mt_afe_enable_afe(false);
	return 0;
}
#endif
static int mtk_soc_pcm_dl1_prestart(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_dl1_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	/* here start digital part */
	mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O03);
	mt_afe_set_connection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O04);

	/* start I2S DAC out */
	if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false) {
		mt_afe_set_i2s_dac_out(runtime->rate);
		mt_afe_enable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_DAC);
		mt_afe_enable_i2s_dac();
	 /*		SetI2SDacEnable(true);*/
	} else{
		mt_afe_enable_memory_path(Soc_Aud_Digital_Block_I2S_OUT_DAC);
	}
	priv->enable_mtk_interface = true;

	mt_afe_enable_afe(true);
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
	struct AFE_BLOCK_T *Afe_Block = NULL;
	int copy_size = 0, Afe_WriteIdx_tmp;
	unsigned long flags;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	char *data_w_ptr = (char *)dst;

	PRINTK_AUD_DL1("mtk_pcm_copy pos = %lu count = %lu\n", pos, count);
	/* get total bytes to copy */
	count = audio_frame_to_bytes(substream, count);

	/* check which memif nned to be write */
	Afe_Block = &pMemControl->rBlock;

	PRINTK_AUD_DL1("%s, WriteIdx=0x%x, ReadIdx=0x%x, DataRemained=0x%x\n", __func__,
		       Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);

	if (Afe_Block->u4BufferSize == 0) {
		pr_err("AudDrv_write: u4BufferSize=0 Error\n");
		return 0;
	}

	spin_lock_irqsave(&auddrv_DLCtl_lock, flags);
	copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;	/* free space of the buffer */
	spin_unlock_irqrestore(&auddrv_DLCtl_lock, flags);

	if (count <= copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

	copy_size = align64bytesize(copy_size);
	PRINTK_AUD_DL1("copy_size=0x%x, count=0x%x\n", copy_size, (unsigned int)count);

	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_DLCtl_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_DLCtl_lock, flags);

		if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) {	/* copy once */
			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
				PRINTK_AUDDRV("AudDrv_write 0 ptr invalid data_w_ptr=%p, size=%d\n",
					      data_w_ptr, copy_size);
				PRINTK_AUDDRV("AudDrv_write u4BufferSize=%d, u4DataRemained=%d\n",
					      Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {
				PRINTK_AUD_DL1
					("cp Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%p, data_w_ptr=%p, copy_size=%x\n",
					Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, copy_size);
				if (copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
					data_w_ptr, copy_size)) {
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
				("%s finish 1, copy_size:%x, WriteIdx:%x, ReadIdx=%x, DataRemained:%x, count=%d\n",
				__func__, copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained, (int)count);

		} else {	/* copy twice */
			uint32_t size_1 = 0, size_2 = 0;

			size_1 = align64bytesize((Afe_Block->u4BufferSize - Afe_WriteIdx_tmp));
			size_2 = align64bytesize((copy_size - size_1));
			PRINTK_AUD_DL1("size_1=0x%x, size_2=0x%x\n", size_1, size_2);

			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
				pr_debug("AudDrv_write 1 ptr invalid data_w_ptr=%p, size_1=%d\n",
				       data_w_ptr, size_1);
				pr_debug("AudDrv_write u4BufferSize=%d, u4DataRemained=%d\n",
				       Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {
				PRINTK_AUD_DL1
					("cp Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%p, data_w_ptr=%p, size_1=%x\n",
					Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, size_1);

				if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
					data_w_ptr, (unsigned int)size_1))) {
					PRINTK_AUDDRV("AudDrv_write Fail 1 copy from user\n");
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
				PRINTK_AUDDRV("AudDrv_write 2 ptr invalid data_w_ptr=%p, size_1=%d, size_2=%d\n",
				     data_w_ptr, size_1, size_2);
				PRINTK_AUDDRV("AudDrv_write u4BufferSize=%d, u4DataRemained=%d\n",
					      Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {
				PRINTK_AUD_DL1
					("Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%p, data_w_ptr+size_1=%p, size_2=%x\n",
					Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr + size_1,
					(unsigned int)size_2);

				if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
					(data_w_ptr + size_1), size_2))) {
					PRINTK_AUDDRV("AudDrv_write Fail 2 copy from user\n");
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
				("%s finish 2, copy size:%x, WriteIdx:%x, ReadIdx=%x, DataRemained:%x\n",
				__func__, copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained);
		}
	}
	return 0;
}

static int mtk_pcm_silence(struct snd_pcm_substream *substream,
			   int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	PRINTK_AUDDRV("%s\n", __func__);
	/* do nothing */
	return 0;
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

static int mtk_soc_dl1_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt_pcm_dl1_priv *priv;

#ifndef CONFIG_OF
	int ret = 0;
#endif

	PRINTK_AUDDRV("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_DL1_PCM);

	priv = devm_kzalloc(dev, sizeof(struct mt_pcm_dl1_priv), GFP_KERNEL);
	if (unlikely(!priv)) {
		pr_err("%s failed to allocate private data\n", __func__);
		return -ENOMEM;
	}
	dev_set_drvdata(dev, priv);

	PRINTK_AUDDRV("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

#ifndef CONFIG_MTK_LEGACY
		AudDrv_GPIO_probe(&pdev->dev);
#endif

	/*InitAfeControl();*/

#ifndef CONFIG_OF
	ret = Register_Aud_Irq(&pdev->dev, MT8163_AFE_MCU_IRQ_LINE);
#endif

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
	afe_allocate_mem_buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL1,
				   Dl1_MAX_BUFFER_SIZE);
	Dl1_Playback_dma_buf = afe_get_mem_buffer(Soc_Aud_Digital_Block_MEM_DL1);
	return 0;
}

static int mtk_afe_remove(struct platform_device *pdev)
{
	PRINTK_AUDDRV("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
/* extern void *AFE_BASE_ADDRESS; */
static const struct of_device_id mt_soc_pcm_dl1_of_ids[] = {

	{.compatible = "mediatek," MT_SOC_DL1_PCM,},
	{}
};
MODULE_DEVICE_TABLE(of, mt_soc_pcm_dl1_of_ids);

#if 0

u32 afe_irq_number = 0;
int AFE_BASE_PHY;



static int Auddrv_Reg_map_new(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt_soc_pcm_dl1");

	if (node) {
		/* Setup IO addresses */
		AFE_BASE_ADDRESS = of_iomap(node, 0);
		pr_debug("[mt_soc_pcm_dl1] AFE_BASE_ADDRESS=0x%p\n", AFE_BASE_ADDRESS);
	} else
		pr_warn("[mt_soc_pcm_dl1] node NULL, can't iomap AFE_BASE!!!\n");

	of_property_read_u32(node, "reg", &AFE_BASE_PHY);
	pr_debug("[mt_soc_pcm_dl1] AFE_BASE_PHY=0x%x\n", AFE_BASE_PHY);

	/*get afe irq num */
	afe_irq_number = irq_of_parse_and_map(node, 0);
	pr_debug("[mt_soc_pcm_dl1] afe_irq_number=0x%x\n", afe_irq_number);
	if (!afe_irq_number) {
		pr_err("[mt_soc_pcm_dl1] get afe_irq_number failed!!!\n");
		return -1;
	}
	return 0;
}
#endif
#ifdef CONFIG_MTK_LEGACY

static int Auddrv_OF_ParseGPIO(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt_soc_pcm_dl1");
	if (node) {
		if_config1 = 1;
		if_config2 = 1;
		if_config3 = 1;
		if_config4 = 1;
		if_config5 = 1;
		if_config6 = 1;
		if_config7 = 1;
		if_config8 = 1;
		if_config9 = 1;
		if_config10 = 1;
		if_config11 = 1;

		if (of_property_read_u32_index(node, "audclk-gpio", 0, &pin_audclk)) {
			if_config1 = 0;
			pr_warn("audclk-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(node, "audclk-gpio", 1, &pin_mode_audclk)) {
			if_config1 = 0;
			pr_warn("audclk-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(node, "audmiso-gpio", 0, &pin_audmiso)) {
			if_config2 = 0;
			pr_warn("audmiso-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(node, "audmiso-gpio", 1, &pin_mode_audmiso)) {
			if_config2 = 0;
			pr_warn("audmiso-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(node, "audmosi-gpio", 0, &pin_audmosi)) {
			if_config3 = 0;
			pr_warn("audmosi-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(node, "audmosi-gpio", 1, &pin_mode_audmosi)) {
			if_config3 = 0;
			pr_warn("audmosi-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(node, "vowclk-gpio", 0, &pin_vowclk)) {
			if_config4 = 0;
			pr_warn("vowclk-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(node, "vowclk-gpio", 1, &pin_mode_vowclk)) {
			if_config4 = 0;
			pr_warn("vowclk-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(node, "extspkamp-gpio", 0, &pin_extspkamp)) {
			if_config5 = 0;
			pr_warn("extspkamp-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(node, "extspkamp-gpio", 1, &pin_mode_extspkamp)) {
			if_config5 = 0;
			pr_warn("extspkamp-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(node, "i2s1clk-gpio", 0, &pin_i2s1clk)) {
			if_config6 = 0;
			pr_warn("i2s1clk-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(node, "i2s1clk-gpio", 1, &pin_mode_i2s1clk)) {
			if_config6 = 0;
			pr_warn("i2s1clk-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(node, "i2s1dat-gpio", 0, &pin_i2s1dat)) {
			if_config7 = 0;
			pr_warn("i2s1dat-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(node, "i2s1dat-gpio", 1, &pin_mode_i2s1dat)) {
			if_config7 = 0;
			pr_warn("i2s1dat-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(node, "i2s1mclk-gpio", 0, &pin_i2s1mclk)) {
			if_config8 = 0;
			pr_warn("i2s1mclk-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(node, "i2s1mclk-gpio", 1, &pin_mode_i2s1mclk)) {
			if_config8 = 0;
			pr_warn("i2s1mclk-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(node, "i2s1ws-gpio", 0, &pin_i2s1ws)) {
			if_config9 = 0;
			pr_warn("i2s1ws-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(node, "i2s1ws-gpio", 1, &pin_mode_i2s1ws)) {
			if_config9 = 0;
			pr_warn("i2s1ws-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(node, "extspkamp_2-gpio", 0, &pin_extspkamp_2)) {
			if_config10 = 0;
			pr_warn("extspkamp_2-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index(node, "extspkamp_2-gpio", 1, &pin_mode_extspkamp_2)) {
			if_config10 = 0;
			pr_warn("extspkamp_2-gpio get pin_mode fail!!!\n");
		}

		if (of_property_read_u32_index(node, "rcvspkswitch-gpio", 0, &pin_rcvspkswitch)) {
			if_config11 = 0;
			pr_warn("rcvspkswitch-gpio get pin fail!!!\n");
		}
		if (of_property_read_u32_index
		    (node, "rcvspkswitch-gpio", 1, &pin_mode_rcvspkswitch)) {
			if_config11 = 0;
			pr_warn("rcvspkswitch-gpio get pin_mode fail!!!\n");
		}

		pr_debug("Auddrv_OF_ParseGPIO pin_audclk=%d, pin_audmiso=%d, pin_audmosi=%d\n",
		       pin_audclk, pin_audmiso, pin_audmosi);
		pr_debug("Auddrv_OF_ParseGPIO pin_vowclk=%d, pin_extspkamp=%d\n", pin_vowclk,
		       pin_extspkamp);
		pr_debug("Auddrv_OF_ParseGPIO pin_i2s1clk=%d, pin_i2s1dat=%d, pin_i2s1mclk=%d, pin_i2s1ws=%d\n",
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
			pr_warn("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 2:		/* pin_audmiso */
		if (if_config2 == 1) {
			*pin = pin_audmiso | 0x80000000;
			*pinmode = pin_mode_audmiso;
		} else {
			pr_warn("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 3:		/* pin_audmosi */
		if (if_config3 == 1) {
			*pin = pin_audmosi | 0x80000000;
			*pinmode = pin_mode_audmosi;
		} else {
			pr_warn("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 4:		/* pin_vowclk */
		if (if_config4 == 1) {
			*pin = pin_vowclk | 0x80000000;
			*pinmode = pin_mode_vowclk;
		} else {
			pr_warn("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 5:		/* pin_extspkamp */
		if (if_config5 == 1) {
			*pin = pin_extspkamp | 0x80000000;
			*pinmode = pin_mode_extspkamp;
		} else {
			pr_warn("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 6:		/* pin_i2s1clk */
		if (if_config6 == 1) {
			*pin = pin_i2s1clk | 0x80000000;
			*pinmode = pin_mode_i2s1clk;
		} else {
			pr_warn("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 7:		/* pin_i2s1dat */
		if (if_config7 == 1) {
			*pin = pin_i2s1dat | 0x80000000;
			*pinmode = pin_mode_i2s1dat;
		} else {
			pr_warn("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 8:		/* pin_i2s1mclk */
		if (if_config8 == 1) {
			*pin = pin_i2s1mclk | 0x80000000;
			*pinmode = pin_mode_i2s1mclk;
		} else {
			pr_warn("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 9:		/* pin_i2s1ws */
		if (if_config9 == 1) {
			*pin = pin_i2s1ws | 0x80000000;
			*pinmode = pin_mode_i2s1ws;
		} else {
			pr_warn("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 10:		/* pin_extspkamp_2 */
		if (if_config10 == 1) {
			*pin = pin_extspkamp_2 | 0x80000000;
			*pinmode = pin_mode_extspkamp_2;
		} else {
			pr_warn("GetGPIO_Info type %d fail!!!\n", type);
			*pin = -1;
			*pinmode = -1;
		}
		break;

	case 11:		/* pin_rcvspkswitch */
		if (if_config11 == 1) {
			*pin = pin_rcvspkswitch | 0x80000000;
			*pinmode = pin_mode_rcvspkswitch;
		} else {
			pr_warn("GetGPIO_Info type %d fail!!!\n", type);
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
#ifdef CONFIG_OF
module_platform_driver(mtk_afe_driver);
#else
static int __init mtk_soc_platform_init(void)
{
	int ret;

	PRINTK_AUDDRV("%s\n", __func__);
#ifdef CONFIG_OF
#if 0
	/*Auddrv_Reg_map_new();*/
	ret = Register_Aud_Irq(NULL, afe_irq_number);
#endif
#ifdef CONFIG_MTK_LEGACY
	Auddrv_OF_ParseGPIO();
#endif

#else
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
#endif
MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
