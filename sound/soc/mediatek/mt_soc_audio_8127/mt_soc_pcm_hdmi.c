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
#include <linux/dma-mapping.h>
#include "mt_soc_afe_common.h"
#include "mt_soc_afe_def.h"
#include "mt_soc_afe_reg.h"
#include "mt_soc_afe_clk.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"


/* information about */

static struct AFE_MEM_CONTROL_T *pMemControl;
static bool fake_buffer = 1;

struct snd_dma_buffer *HDMI_dma_buf = NULL;


static DEFINE_SPINLOCK(auddrv_hdmi_lock);

/*
 *    function implementation
 */

static int mtk_hdmi_probe(struct platform_device *pdev);
static int mtk_pcm_hdmi_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_hdmi_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_hdmi_probe(struct snd_soc_platform *platform);


#define MAX_PCM_DEVICES     4
#define MAX_PCM_SUBSTREAMS  128
#define MAX_MIDI_DEVICES
/* #define _DEBUG_TDM_KERNEL_ */
#define _NO_SRAM_USAGE_


/* defaults */
/* #define HDMI_MAX_BUFFER_SIZE     (192*1024) */
/* #define MIN_PERIOD_SIZE       64 */
/* #define MAX_PERIOD_SIZE     HDMI_MAX_BUFFER_SIZE */
#define USE_FORMATS         (SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S16_LE)
#define USE_RATE        (SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_192000)
#define USE_RATE_MIN        8000
#define USE_RATE_MAX        192000
#define USE_CHANNELS_MIN     1
#define USE_CHANNELS_MAX    8
#define USE_PERIODS_MIN     512
#define USE_PERIODS_MAX     16384

struct mt_pcm_hdmi_priv {
	bool prepared;
	unsigned int hdmi_loop_type;
	unsigned int hdmi_sinegen_switch;
	unsigned int cached_sample_rate;
	struct snd_dma_buffer *hdmi_dma_buf;
};

/* unused variable
static int32_t Previous_Hw_cur = 0;
*/

static int mHdmi_sidegen_control;
static const char * const HDMI_SIDEGEN[] = { "Off", "On" };

static const struct soc_enum Audio_Hdmi_Enum[] = {

	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(HDMI_SIDEGEN), HDMI_SIDEGEN),
};

static int Audio_hdmi_SideGen_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_hdmi_SideGen_Get = %d\n", mHdmi_sidegen_control);
	ucontrol->value.integer.value[0] = mHdmi_sidegen_control;
	return 0;
}

static int Audio_hdmi_SideGen_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(HDMI_SIDEGEN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_hdmi_controls[] = {

	SOC_ENUM_EXT("Audio_Hdmi_SideGen_Switch", Audio_Hdmi_Enum[0], Audio_hdmi_SideGen_Get,
		     Audio_hdmi_SideGen_Set)
};


static struct snd_pcm_hardware mtk_hdmi_hardware = {

	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = USE_CHANNELS_MIN,
	.channels_max = USE_CHANNELS_MAX,
	.buffer_bytes_max = HDMI_MAX_BUFFER_SIZE,
	.period_bytes_max = HDMI_MAX_PERIODBYTE_SIZE,
	.periods_min = HDMI_MIN_PERIOD_SIZE,
	.periods_max = HDMI_MAX_2CH_16BIT_PERIOD_SIZE,
	.fifo_size = 0,
};


static struct snd_soc_pcm_runtime *pruntimehdmi;


static int mtk_pcm_hdmi_stop(struct snd_pcm_substream *substream)
{
	struct AFE_BLOCK_T *Afe_Block = &(pMemControl->rBlock);
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("mtk_pcm_hdmi_stop\n");

	SetHDMIEnable(false);

	mt_afe_set_irq_state(Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE, false);

	mt_afe_disable_memory_path(Soc_Aud_Digital_Block_MEM_HDMI);

	mt_afe_enable_afe(false);

	SetHdmiPcmInterConnection(Soc_Aud_InterCon_DisConnect, runtime->channels);

	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_HDMI, substream);

	mt_afe_hdmi_clk_off();

	mt_afe_aplltuner_clk_off();

	Afe_Block->u4DMAReadIdx = 0;
	Afe_Block->u4WriteIdx = 0;
	Afe_Block->u4DataRemained = 0;

	return 0;
}


static snd_pcm_uframes_t mtk_pcm_hdmi_pointer(struct snd_pcm_substream *substream)
{
	int32_t HW_memory_index = 0;
	int32_t HW_Cur_ReadIdx = 0;
	uint32_t Frameidx = 0;
	int32_t Afe_consumed_bytes = 0;
	struct AFE_BLOCK_T *Afe_Block = &(pMemControl->rBlock);

	PRINTK_AUD_HDMI("%s Afe_Block->u4DMAReadIdx = 0x%x\n", __func__,
		Afe_Block->u4DMAReadIdx);

	if (mt_afe_get_memory_path_state(Soc_Aud_Digital_Block_MEM_HDMI) == true) {
		HW_Cur_ReadIdx = mt_afe_get_reg(AFE_HDMI_OUT_CUR);

		if (HW_Cur_ReadIdx == 0) {
			PRINTK_AUD_HDMI("[Auddrv] HW_Cur_ReadIdx == 0\n");
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}

		HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

		if (HW_memory_index >= Afe_Block->u4DMAReadIdx)
			Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
		else {
			Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index
				- Afe_Block->u4DMAReadIdx;
		}

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;

		PRINTK_AUD_HDMI
			("[Auddrv] HW_Cur_ReadIdx = 0x%x, HW_memory_index = 0x%x, Afe_consumed_bytes = 0x%x\n",
			HW_Cur_ReadIdx, HW_memory_index, Afe_consumed_bytes);

		return audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	}

	Frameidx = audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	return Frameidx;
}


static void SetHDMIBuffer(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{

	uint32_t volatile u4tmpMrg1;

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct AFE_BLOCK_T *pblock = &(pMemControl->rBlock);

	pblock->pucPhysBufAddr = runtime->dma_addr;
	pblock->pucVirtBufAddr = runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;

	PRINTK_AUD_HDMI("%s, dma_bytes = %d, dma_area = %p, dma_addr = 0x%x\n",
		__func__, pblock->u4BufferSize, pblock->pucVirtBufAddr,
		pblock->pucPhysBufAddr);

	mt_afe_set_reg(AFE_HDMI_OUT_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	mt_afe_set_reg(AFE_HDMI_OUT_END, pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1),
		0xffffffff);

	u4tmpMrg1 = mt_afe_get_reg(AFE_HDMI_OUT_BASE);

	u4tmpMrg1 &= 0x00ffffff;

	PRINTK_AUD_HDMI("SetHDMIBuffer AFE_HDMI_OUT_BASE = 0x%x\n", u4tmpMrg1);
}


static int mtk_pcm_hdmi_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	int ret = 0;

	pr_debug("mtk_pcm_hdmi_hw_params\n");

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;

	if (fake_buffer) {
		PRINTK_AUD_HDMI("[mtk_pcm_hdmi_hw_params] HDMI_dma_buf->area\n");

#ifdef _NO_SRAM_USAGE_
		runtime->dma_area = HDMI_dma_buf->area;
		runtime->dma_addr = HDMI_dma_buf->addr;
		/* runtime->dma_bytes = HDMI_dma_buf->bytes; */
		/* runtime->buffer_size = HDMI_dma_buf->bytes; */
		runtime->dma_bytes = params_buffer_bytes(hw_params);
		runtime->buffer_size = runtime->dma_bytes;
#else
		runtime->dma_area = (unsigned char *)mt_afe_get_sram_base_ptr();
		runtime->dma_addr = mt_afe_get_sram_phy_addr();
		runtime->dma_bytes = params_buffer_bytes(hw_params);
		runtime->buffer_size = runtime->dma_bytes;
#endif

	} else {
		PRINTK_AUD_HDMI("[mtk_pcm_hdmi_hw_params] snd_pcm_lib_malloc_pages\n");
		ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
	}
	PRINTK_AUD_HDMI("%s, dma_bytes = %zu, dma_area = %p, dma_addr = 0x%lx\n",
		__func__, substream->runtime->dma_bytes, substream->runtime->dma_area,
		(long)substream->runtime->dma_addr);

	SetHDMIBuffer(substream, hw_params);
	return ret;
}


static int mtk_pcm_hdmi_hw_free(struct snd_pcm_substream *substream)
{
	PRINTK_AUD_HDMI("mtk_pcm_hdmi_hw_free\n");

	if (fake_buffer)
		return 0;

	return snd_pcm_lib_free_pages(substream);
}


/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {

	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000, 192000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {

	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};


static int mtk_pcm_hdmi_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);

	mt_afe_emi_clk_on();
	mt_afe_main_clk_on();

	runtime->hw = mtk_hdmi_hardware;

	pMemControl = get_mem_control_t(Soc_Aud_Digital_Block_MEM_HDMI);

	memcpy((void *)(&(runtime->hw)), (void *)&mtk_hdmi_hardware,
	       sizeof(struct snd_pcm_hardware));


	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);

	if (ret < 0) {
		PRINTK_AUD_HDMI("snd_pcm_hw_constraint_integer failed\n");
		return ret;
	}

	/* print for hw pcm information */
	pr_debug("%s, runtime->rate = %u, channels = %u, substream->pcm->device = %d\n",
		__func__, runtime->rate, runtime->channels, substream->pcm->device);

	runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_MMAP_VALID;

	PRINTK_AUD_HDMI("mtk_pcm_hdmi_open return\n");

	return 0;
}


static int mtk_pcm_hdmi_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	struct AFE_BLOCK_T *Afe_Block = &(pMemControl->rBlock);

	if (priv->prepared) {
		mt_afe_top_apll_clk_off();
		priv->prepared = false;
	}

	mt_afe_emi_clk_off();
	mt_afe_main_clk_off();

	Afe_Block->u4DMAReadIdx = 0;
	Afe_Block->u4WriteIdx = 0;
	Afe_Block->u4DataRemained = 0;

	return 0;
}

static int mtk_pcm_hdmi_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mt_pcm_hdmi_priv *priv = snd_soc_platform_get_drvdata(rtd->platform);

	pr_debug("%s rate = %u channels = %u period_size = %lu.\n",
		__func__, runtime->rate, runtime->channels, runtime->period_size);

	if (!priv->prepared) {
		set_hdmi_clock_source(runtime->rate);
		mt_afe_top_apll_clk_on();

	} else if (priv->cached_sample_rate != runtime->rate) {
		/* Must turn on pdn_apll before setting clk mux */
		mt_afe_top_apll_clk_off();
		/* enable audio clock */
		set_hdmi_clock_source(runtime->rate);
		mt_afe_top_apll_clk_on();
	}
	priv->prepared = true;
	priv->cached_sample_rate = runtime->rate;

	return 0;
}



static int mtk_pcm_hdmi_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s period_size = %lu,runtime->rate=%u,runtime->channels=%u\n",
			__func__, runtime->period_size, runtime->rate, runtime->channels);

	set_memif_substream(Soc_Aud_Digital_Block_MEM_HDMI, substream);
	mt_afe_aplltuner_clk_on();
	mt_afe_hdmi_clk_on();

	mt_afe_set_reg(AFE_HDMI_OUT_BASE, runtime->dma_addr, 0xffffffff);
	mt_afe_set_reg(AFE_HDMI_OUT_END, runtime->dma_addr + (runtime->dma_bytes - 1), 0xffffffff);

	/* here to set interrupt */
	mt_afe_set_irq_counter(Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE, runtime->period_size);
	mt_afe_set_irq_state(Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE, true);

	/* interconnection */
	SetHdmiPcmInterConnection(Soc_Aud_InterCon_Connection, runtime->channels);

	/* HDMI Out control */
	set_hdmi_out_control(runtime->channels);
	set_hdmi_out_control_enable(true);

	/* HDMI I2S */
	set_hdmi_i2s();
	set_hdmi_bck_div(runtime->rate);
	set_hdmi_i2s_enable(true);

	mt_afe_enable_memory_path(Soc_Aud_Digital_Block_MEM_HDMI);/*	SetHdmiPathEnable(true);*/
	mt_afe_enable_afe(true);

	return 0;
}

static int mtk_pcm_hdmi_trigger(struct snd_pcm_substream *substream, int cmd)
{
	PRINTK_AUD_HDMI("mtk_pcm_hdmi_trigger cmd = %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_hdmi_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_hdmi_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_hdmi_copy(struct snd_pcm_substream *substream,
			     int channel, snd_pcm_uframes_t pos,
			     void __user *dst, snd_pcm_uframes_t count)
{
	struct AFE_BLOCK_T *Afe_Block = NULL;
	int copy_size = 0, Afe_WriteIdx_tmp;
	unsigned long flags;
	char *data_w_ptr = (char *)dst;
	struct snd_pcm_runtime *runtime = substream->runtime;

	count = frames_to_bytes(runtime, count);

	/* check which memif need to be write */
	Afe_Block = &(pMemControl->rBlock);

	/* handle for buffer management */

	PRINTK_AUD_HDMI
		("[%s] count = %d, WriteIdx = %x, ReadIdx = %x, DataRemained = %x\n",
		__func__, (uint32_t) count, Afe_Block->u4WriteIdx,
		Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);

	if (Afe_Block->u4BufferSize == 0) {
		pr_err("%s: u4BufferSize = 0, Error!!!\n", __func__);
		return 0;
	}

	spin_lock_irqsave(&auddrv_hdmi_lock, flags);
	/* free space of the buffer */
	copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;
	spin_unlock_irqrestore(&auddrv_hdmi_lock, flags);

	if (count <= copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_hdmi_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_hdmi_lock, flags);

		if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) {	/* copy once */
			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
				pr_warn("[%s] 0 ptr invalid data_w_ptr = %p, size = %d,", __func__,
					data_w_ptr, copy_size);
				pr_warn(" u4BufferSize = %d, u4DataRemained = %d\n",
					Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {
				PRINTK_AUD_HDMI2
				("[%s] memcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%x, data_w_ptr=%p, copy_size=%x\n",
				Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, copy_size);

				if (copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
					data_w_ptr, copy_size)) {
					pr_err("[%s] Fail copy from user\n", __func__);
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_hdmi_lock, flags);
			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_hdmi_lock, flags);
			data_w_ptr += copy_size;
			count -= copy_size;

			PRINTK_AUD_HDMI2
				("[%s] finish 1, copy_size:%x, WriteIdx:%x, ReadIdx:%x, DataRemained:%x\n",
				__func__, copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained);

		} else {	/* copy twice */
			uint32_t size_1 = 0, size_2 = 0;

			size_1 = Afe_Block->u4BufferSize - Afe_WriteIdx_tmp;
			size_2 = copy_size - size_1;

			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
				pr_warn("[%s] 1 ptr invalid data_w_ptr = %p, size_1 = %d,",
					__func__, data_w_ptr, size_1);
				pr_warn(" u4BufferSize = %d, u4DataRemained = %d\n",
					Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {
				PRINTK_AUD_HDMI2
				("[%s] mcmcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%x, data_w_ptr=%p, size_1=%x\n",
				__func__, Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, size_1);

				if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
					data_w_ptr, size_1))) {
					pr_err("[%s] Fail 1 copy from user\n", __func__);
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_hdmi_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&auddrv_hdmi_lock, flags);

			if (!access_ok(VERIFY_READ, data_w_ptr + size_1, size_2)) {
				pr_warn("[%s] 2 ptr invalid data_w_ptr = %p, size_1 = %d, size_2 = %d,",
					__func__, data_w_ptr, size_1, size_2);
				pr_warn(" u4BufferSize = %d, u4DataRemained = %d\n",
					Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {
				PRINTK_AUD_HDMI2
					("[%s] mcmcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx=%x,\n",
					__func__, Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp);
				PRINTK_AUD_HDMI2
					(" data_w_ptr+size_1=%p, size_2=%x\n", data_w_ptr + size_1, size_2);

				if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
					(data_w_ptr + size_1), size_2))) {
					pr_err("[%s] Fail 2 copy from user\n", __func__);
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_hdmi_lock, flags);

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_hdmi_lock, flags);
			count -= copy_size;
			data_w_ptr += copy_size;

			PRINTK_AUD_HDMI2
				("[%s] finish 2, copy size:%x, WriteIdx:%x, ReadIdx:%x, DataRemained:%x\n",
				__func__, copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
				Afe_Block->u4DataRemained);
		}
	}

	return 0;
}

static int mtk_pcm_hdmi_silence(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	PRINTK_AUD_HDMI("%s\n", __func__);
	/* do nothing */
	return 0;
}

static void *dummy_page[2];

static struct page *mtk_pcm_page(struct snd_pcm_substream *substream, unsigned long offset)
{
	PRINTK_AUD_HDMI("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]);	/* the same page */
}

static struct snd_pcm_ops mtk_hdmi_ops = {

	.open = mtk_pcm_hdmi_open,
	.close = mtk_pcm_hdmi_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_hdmi_hw_params,
	.hw_free = mtk_pcm_hdmi_hw_free,
	.prepare = mtk_pcm_hdmi_prepare,
	.trigger = mtk_pcm_hdmi_trigger,
	.pointer = mtk_pcm_hdmi_pointer,
	.copy = mtk_pcm_hdmi_copy,
	.silence = mtk_pcm_hdmi_silence,
	.page = mtk_pcm_page,
};

static struct snd_soc_platform_driver mtk_hdmi_soc_platform = {

	.ops = &mtk_hdmi_ops,
	.pcm_new = mtk_asoc_pcm_hdmi_new,
	.probe = mtk_afe_hdmi_probe,
};

static int mtk_hdmi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt_pcm_hdmi_priv *priv;

	PRINTK_AUD_HDMI("%s dev name %s\n", __func__, dev_name(dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT_SOC_HDMI_PCM);
		PRINTK_AUD_HDMI("%s set dev name %s\n", __func__, dev_name(dev));
	}

	priv = devm_kzalloc(dev, sizeof(struct mt_pcm_hdmi_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->cached_sample_rate = 44100;

	dev_set_drvdata(dev, priv);

	return snd_soc_register_platform(dev, &mtk_hdmi_soc_platform);
}

static int mtk_asoc_pcm_hdmi_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pruntimehdmi = rtd;
	PRINTK_AUD_HDMI("%s\n", __func__);
	return ret;
}

static int mtk_afe_hdmi_probe(struct snd_soc_platform *platform)
{

	HDMI_dma_buf = kmalloc(sizeof(struct snd_dma_buffer), GFP_KERNEL);
	memset((void *)HDMI_dma_buf, 0, sizeof(struct snd_dma_buffer));
	PRINTK_AUD_HDMI("mtk_afe_hdmi_probe dma_alloc_coherent HDMI_dma_buf->addr = 0x%lx\n",
			(long)HDMI_dma_buf->addr);

	HDMI_dma_buf->area = dma_alloc_coherent(platform->dev, HDMI_MAX_BUFFER_SIZE,
		&HDMI_dma_buf->addr, GFP_KERNEL);	/* virtual pointer */

	if (HDMI_dma_buf->area)
		HDMI_dma_buf->bytes = HDMI_MAX_BUFFER_SIZE;

	snd_soc_add_platform_controls(platform, Audio_snd_hdmi_controls,
				      ARRAY_SIZE(Audio_snd_hdmi_controls));
	return 0;
}

static int mtk_hdmi_remove(struct platform_device *pdev)
{
	PRINTK_AUD_HDMI("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_hdmi_of_ids[] = {

	{.compatible = "mediatek," MT_SOC_HDMI_PCM,},
	{}
};
MODULE_DEVICE_TABLE(of, mt_soc_pcm_hdmi_of_ids);

#endif

static struct platform_driver mtk_hdmi_driver = {

	.driver = {
		   .name = MT_SOC_HDMI_PCM,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_pcm_hdmi_of_ids,
#endif
		   },
	.probe = mtk_hdmi_probe,
	.remove = mtk_hdmi_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkhdmi_dev;
#endif
#ifdef CONFIG_OF
module_platform_driver(mtk_hdmi_driver);
#else
static int __init mtk_hdmi_soc_platform_init(void)
{
	int ret;

	PRINTK_AUD_HDMI("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkhdmi_dev = platform_device_alloc(MT_SOC_HDMI_PCM, -1);

	if (!soc_mtkhdmi_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkhdmi_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkhdmi_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_hdmi_driver);
	return ret;

}
module_init(mtk_hdmi_soc_platform_init);

static void __exit mtk_hdmi_soc_platform_exit(void)
{
	PRINTK_AUD_HDMI("%s\n", __func__);

	platform_driver_unregister(&mtk_hdmi_driver);
}
module_exit(mtk_hdmi_soc_platform_exit);
#endif
/* EXPORT_SYMBOL(Auddrv_Hdmi_Interrupt_Handler); */

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
