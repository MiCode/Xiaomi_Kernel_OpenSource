// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_dl2.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio dl2 data1 playback
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
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include <linux/ftrace.h>

static int fast_dl_hdoutput;
static struct afe_mem_control_t *pMemControl;
static struct snd_dma_buffer *Dl2_Playback_dma_buf;

static unsigned int UnderflowTime;

static bool StartCheckTime;
static unsigned long PrevTime;
static unsigned long NowTime;

#ifdef AUDIO_DL2_ISR_COPY_SUPPORT
static const int ISRCopyMaxSize = 256 * 2 * 4; /* 256 frames, stereo, 32bit */
static struct afe_dl_isr_copy_t ISRCopyBuffer = {0};
#endif

const char * const fast_dl_hd_output[] = {"Off", "On"};

static const struct soc_enum fast_dl_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fast_dl_hd_output), fast_dl_hd_output),
};

static int fast_dl_hdoutput_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() = %d\n", __func__, fast_dl_hdoutput);
	ucontrol->value.integer.value[0] = fast_dl_hdoutput;
	return 0;
}

static int fast_dl_hdoutput_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	/* pr_debug("%s()\n", __func__); */
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(fast_dl_hd_output)) {
		pr_warn("%s(), return -EINVAL\n", __func__);
		return -EINVAL;
	}

	fast_dl_hdoutput = ucontrol->value.integer.value[0];

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI) == true) {
		pr_debug("return HDMI enabled\n");
		return 0;
	}

	return 0;
}

static int dataTransfer(void *dest, const void *src, uint32_t size);

enum DEBUG_DL2 {
	DEBUG_DL2_LOG = 1,
	DEBUG_DL2_LOG_DETECT_DTAT = 2,
	DEBUG_DL2_AEE_UNDERFLOW = 4,
	DEBUG_DL2_AEE_OTHERS = 8
};

#define PRINT_DEBUG_LOG(format, args...)                                      \
	{                                                                      \
		if (unlikely(get_LowLatencyDebug() & DEBUG_DL2_LOG))           \
			pr_debug(format, ##args);                              \
	}

/*
 *    function implementation
 */

/* void StartAudioPcmHardware(void); */
/* void StopAudioPcmHardware(void); */
static int mtk_soc_dl2_probe(struct platform_device *pdev);
static int mtk_soc_pcm_dl2_close(struct snd_pcm_substream *substream);
static int mtk_asoc_dl2_component_probe(struct snd_soc_component *component);

static bool mPrepareDone;

#define USE_RATE (SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000)
#define USE_RATE_MIN 8000
#define USE_RATE_MAX 192000
#define USE_CHANNELS_MIN 1
#define USE_CHANNELS_MAX 2
#define USE_PERIODS_MIN 512
#define USE_PERIODS_MAX 8192

static const struct snd_kcontrol_new fast_dl_controls[] = {
	SOC_ENUM_EXT("fast_dl_hd_Switch", fast_dl_enum[0],
		    fast_dl_hdoutput_get, fast_dl_hdoutput_set),
};

static struct snd_pcm_hardware mtk_pcm_dl2_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
		 SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl2_MAX_BUFFER_SIZE,
	.period_bytes_max = Dl2_MAX_PERIOD_SIZE,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int mtk_pcm_dl2_stop(struct snd_pcm_substream *substream)
{
	PRINT_DEBUG_LOG("%s\n", __func__);

	StartCheckTime = false;
	if (unlikely(get_LowLatencyDebug())) {
		struct afe_block_t *Afe_Block = &pMemControl->rBlock;

		if (Afe_Block->u4DataRemained < 0) {
			pr_warn("%s, dl2 underflow\n", __func__);
			if (get_LowLatencyDebug() & DEBUG_DL2_AEE_UNDERFLOW)
				AUDIO_AEE("mtk_pcm_dl2_stop - dl2 underflow");
		}
	}

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2, false);

	irq_remove_substream_user(
		substream, irq_request_number(Soc_Aud_Digital_Block_MEM_DL2));

	/* here start digital part */
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL2,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL2,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL2,
			  Soc_Aud_AFE_IO_Block_I2S3);

	ClearMemBlock(Soc_Aud_Digital_Block_MEM_DL2);
	return 0;
}

static snd_pcm_uframes_t
mtk_pcm_dl2_pointer(struct snd_pcm_substream *substream)
{
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	kal_uint32 Frameidx = 0;
	kal_int32 Afe_consumed_bytes = 0;
	struct afe_block_t *Afe_Block = &pMemControl->rBlock;
	unsigned long flags;

	/* struct snd_pcm_runtime *runtime = substream->runtime; */
#ifdef DL2_DEBUG_LOG
	pr_debug(" %s Afe_Block->u4DMAReadIdx = 0x%x\n", __func__,
		       Afe_Block->u4DMAReadIdx);
#endif
	spin_lock_irqsave(&pMemControl->substream_lock, flags);

	/* get total bytes to copy */
	/* Frameidx = audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx);
	 */
	/* return Frameidx; */

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2) == true) {
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL2_CUR);
		if (HW_Cur_ReadIdx == 0) {
			pr_warn("[Auddrv] HW_Cur_ReadIdx ==0\n");
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}

		HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);
		if (HW_memory_index >= Afe_Block->u4DMAReadIdx) {
			Afe_consumed_bytes =
				HW_memory_index - Afe_Block->u4DMAReadIdx;
		} else {
			Afe_consumed_bytes = Afe_Block->u4BufferSize +
					     HW_memory_index -
					     Afe_Block->u4DMAReadIdx;
		}

#ifdef AUDIO_64BYTE_ALIGN /* no need to do 64byte align */
		Afe_consumed_bytes = word_size_align(Afe_consumed_bytes);
#endif

		PRINT_DEBUG_LOG(
			"+%s DataRemained:%d, consumed_bytes:%d, HW_memory_index = %d, ReadIdx:%d, WriteIdx:%d\n",
			__func__, Afe_Block->u4DataRemained, Afe_consumed_bytes,
			HW_memory_index, Afe_Block->u4DMAReadIdx,
			Afe_Block->u4WriteIdx);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;

		PRINT_DEBUG_LOG(
			"-%s DataRemained:%d, consumed_bytes:%d, HW_memory_index = %d, ReadIdx:%d, WriteIdx:%d\n",
			__func__, Afe_Block->u4DataRemained, Afe_consumed_bytes,
			HW_memory_index, Afe_Block->u4DMAReadIdx,
			Afe_Block->u4WriteIdx);

		if (Afe_Block->u4DataRemained < 0)
			PRINT_DEBUG_LOG("[AudioWarn] u4DataRemained=0x%x\n",
					 Afe_Block->u4DataRemained);
		Frameidx = audio_bytes_to_frame(substream,
						Afe_Block->u4DMAReadIdx);
	} else {
		Frameidx = audio_bytes_to_frame(substream,
						Afe_Block->u4DMAReadIdx);
	}
	spin_unlock_irqrestore(&pMemControl->substream_lock, flags);
	return Frameidx;
}

static int mtk_pcm_dl2_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *hw_params)
{
	/* struct snd_dma_buffer *dma_buf = &substream->dma_buffer; */
	int ret = 0;

	/* runtime->dma_bytes has to be set manually to allow mmap */
	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
	substream->runtime->dma_area = Dl2_Playback_dma_buf->area;
	substream->runtime->dma_addr = Dl2_Playback_dma_buf->addr;
	SetHighAddr(Soc_Aud_Digital_Block_MEM_DL2, true,
		    substream->runtime->dma_addr);
	set_mem_block(substream, hw_params, pMemControl,
		      Soc_Aud_Digital_Block_MEM_DL2);

#if defined(DL2_DEBUG_LOG)
	pr_debug("dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		       substream->runtime->dma_bytes,
		       substream->runtime->dma_area,
		       (long)substream->runtime->dma_addr);
#endif
	return ret;
}

static int mtk_pcm_dl2_hw_free(struct snd_pcm_substream *substream)
{
#if defined(DL2_DEBUG_LOG)
	pr_debug("%s()\n", __func__);
#endif
	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_dl2_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;


	mtk_pcm_dl2_hardware.buffer_bytes_max = GetPLaybackDramSize();
	AudDrv_Emi_Clk_On();
#if defined(DL2_DEBUG_LOG)
	pr_debug("mtk_pcm_dl2_hardware.buffer_bytes_max = %zu\n",
		       mtk_pcm_dl2_hardware.buffer_bytes_max);
#endif
	runtime->hw = mtk_pcm_dl2_hardware;

	AudDrv_Clk_On();
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_dl2_hardware,
	       sizeof(struct snd_pcm_hardware));
	pMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL2);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);

	if (ret < 0)
		pr_err("snd_pcm_hw_constraint_integer failed\n");

	if (ret < 0) {
		pr_err("ret < 0 mtk_soc_pcm_dl2_close\n");
		mtk_soc_pcm_dl2_close(substream);
		return ret;
	}

#ifdef AUDIO_DL2_ISR_COPY_SUPPORT
	if (!ISRCopyBuffer.pBufferBase) {
		ISRCopyBuffer.pBufferBase = kmalloc(ISRCopyMaxSize, GFP_KERNEL);
		if (!ISRCopyBuffer.pBufferBase)
			pr_err("%s alloc ISRCopyBuffer fail\n", __func__);
		else
			ISRCopyBuffer.u4BufferSizeMax = ISRCopyMaxSize;
	}
#endif


	return 0;
}

static int mtk_soc_pcm_dl2_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s, mPrepareDone = %d, fast_dl_hdoutput = %d\n",
		 __func__, mPrepareDone, fast_dl_hdoutput);

	if (mPrepareDone == true) {
		/* stop DAC output */
		set_memif_pbuf_size(Soc_Aud_Digital_Block_MEM_DL2,
				    MEMIF_PBUF_SIZE_256_BYTES);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, false);
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) ==
		    false)
			Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);

		RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL2, substream);

		if (fast_dl_hdoutput == true) {
			/* here to close APLL */
			if (!mtk_soc_always_hd) {
				DisableAPLLTunerbySampleRate(
						substream->runtime->rate);
				DisableALLbySampleRate(
						substream->runtime->rate);
			}
		}

		EnableAfe(false);
		mPrepareDone = false;
	}

	AudDrv_Emi_Clk_Off();
	AudDrv_Clk_Off();

#ifdef AUDIO_DL2_ISR_COPY_SUPPORT
	kfree(ISRCopyBuffer.pBufferBase);
	memset(&ISRCopyBuffer, 0, sizeof(ISRCopyBuffer));
#endif

	return 0;
}

static int mtk_pcm_dl2_prepare(struct snd_pcm_substream *substream)
{
	bool mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int u32AudioI2S = 0;

	pr_debug("%s\n", __func__);

	if (mPrepareDone == false) {
		pr_info(
			"%s format = %d SNDRV_PCM_FORMAT_S32_LE = %d SNDRV_PCM_FORMAT_U32_LE = %d, fast_dl_hdoutput = %d\n",
			__func__, runtime->format, SNDRV_PCM_FORMAT_S32_LE,
			SNDRV_PCM_FORMAT_U32_LE, fast_dl_hdoutput);
		SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL2, substream);
		set_memif_pbuf_size(Soc_Aud_Digital_Block_MEM_DL2,
				    MEMIF_PBUF_SIZE_32_BYTES);

		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
			/* not support 24bit +++ */
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL2,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S3);

			/* not support 24bit --- */
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			/* not support 24bit +++ */
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_16_BIT);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S3);

			/* not support 24bit --- */
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		}

		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, runtime->rate);

		u32AudioI2S =
			SampleRateTransform(runtime->rate,
					    Soc_Aud_Digital_Block_I2S_OUT_2)
			<< 8;
		u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3; /* use I2s format */
		u32AudioI2S |= mI2SWLen << 1;

		if (fast_dl_hdoutput == true) {
			/* here to open APLL */
			if (!mtk_soc_always_hd) {
				EnableALLbySampleRate(runtime->rate);
				EnableAPLLTunerbySampleRate(runtime->rate);
			}
			/* Low jitter mode */
			u32AudioI2S |= Soc_Aud_LOW_JITTER_CLOCK << 12;
		} else {
			u32AudioI2S &= ~(Soc_Aud_LOW_JITTER_CLOCK << 12);
		}

		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) ==
		    false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2,
					    true);
			Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S | 1,
				    AFE_MASK_ALL);
		} else {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2,
					    true);
		}

		/* start I2S DAC out */
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) ==
		    false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
					    true);
			SetI2SDacOut(substream->runtime->rate, fast_dl_hdoutput,
				     mI2SWLen);
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

static int mtk_pcm_dl2_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);
	/* here start digital part */

	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL2,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL2,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL2,
			  Soc_Aud_AFE_IO_Block_I2S3);

#ifdef CONFIG_MTK_FPGA
	/* set loopback test interconnection */
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL2,
			  Soc_Aud_AFE_IO_Block_MEM_VUL);
#endif

	/* here to set interrupt */
	irq_add_substream_user(
		substream, irq_request_number(Soc_Aud_Digital_Block_MEM_DL2),
		runtime->rate, runtime->period_size);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL2, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL2, runtime->channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2, true);

	EnableAfe(true);

	StartCheckTime = true;
	PrevTime = NowTime = 0;

	UnderflowTime = runtime->period_size * runtime->periods * 1000000000 /
			runtime->rate;

	return 0;
}

static int mtk_pcm_dl2_trigger(struct snd_pcm_substream *substream, int cmd)
{
#if defined(DL2_DEBUG_LOG)
	pr_debug("mtk_pcm_trigger cmd = %d\n", cmd);
#endif
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_dl2_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_dl2_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_dl2_copy_(void __user *dst, snd_pcm_uframes_t *size,
			     struct afe_block_t *Afe_Block, bool bCopy)
{
	int copy_size = 0, Afe_WriteIdx_tmp;
	unsigned long flags;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	char *data_w_ptr = (char *)dst;

	snd_pcm_uframes_t count = *size;
#ifdef DL2_DEBUG_LOG
	pr_debug(
		"AudDrv_write WriteIdx=0x%x, ReadIdx=0x%x, DataRemained=0x%x\n",
		Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
		Afe_Block->u4DataRemained);
#endif

	if (Afe_Block->u4BufferSize == 0) {
		pr_err("AudDrv_write: u4BufferSize=0 Error");
		return 0;
	}

	spin_lock_irqsave(&pMemControl->substream_lock, flags);
	copy_size = Afe_Block->u4BufferSize -
		    Afe_Block->u4DataRemained; /* free space of the buffer */
	spin_unlock_irqrestore(&pMemControl->substream_lock, flags);
	if (count <= copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	} else {
		if (unlikely(get_LowLatencyDebug() & DEBUG_DL2_AEE_OTHERS)) {
			pr_debug("%s, Insufficient data !\n", __func__);
			AUDIO_AEE("ISRCopy has remaining data !!");
		}
	}

#ifdef AUDIO_64BYTE_ALIGN /* no need to do 64byte align */
	copy_size = word_size_align(copy_size);
#endif
	*size = copy_size;
	PRINT_DEBUG_LOG(
		"%s, copy_size=%d, count=%d, bCopy %d, %pf %pf %pf %pf\n",
		__func__, copy_size, (unsigned int)count, bCopy,
		(void *)CALLER_ADDR0, (void *)CALLER_ADDR1,
		(void *)CALLER_ADDR2, (void *)CALLER_ADDR3);

	PRINT_DEBUG_LOG(
		"AudDrv_write DataRemained:%d, ReadIdx=%d, WriteIdx:%d\r\n",
		Afe_Block->u4DataRemained, Afe_Block->u4DMAReadIdx,
		Afe_Block->u4WriteIdx);

	if (copy_size != 0) {
		spin_lock_irqsave(&pMemControl->substream_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&pMemControl->substream_lock, flags);
#ifdef DL2_DEBUG_LOG
		pr_debug(
			"Afe_WriteIdx_tmp %d, copy_size %d, u4BufferSize %d\n",
			Afe_WriteIdx_tmp, copy_size, Afe_Block->u4BufferSize);
#endif
		if (Afe_WriteIdx_tmp + copy_size <
		    Afe_Block->u4BufferSize) { /* copy once */
			if (bCopy) {
				if (dataTransfer((Afe_Block->pucVirtBufAddr +
						  Afe_WriteIdx_tmp),
						 data_w_ptr, copy_size) == -1)
					return -1;
			}

			spin_lock_irqsave(&pMemControl->substream_lock, flags);
			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&pMemControl->substream_lock,
					       flags);
			data_w_ptr += copy_size;
			count -= copy_size;
#ifdef DL2_DEBUG_LOG
			PRINT_DEBUG_LOG(
				"AudDrv_write finish1, DataRemained:%d, ReadIdx=%d, WriteIdx:%d\r\n",
				Afe_Block->u4DataRemained,
				Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx);
#endif

		} else { /* copy twice */
			kal_uint32 size_1 = 0, size_2 = 0;

#ifdef AUDIO_64BYTE_ALIGN /* no need to do 64byte align */
			size_1 = word_size_align(
				(Afe_Block->u4BufferSize - Afe_WriteIdx_tmp));
			size_2 = word_size_align((copy_size - size_1));
#else
			size_1 = Afe_Block->u4BufferSize - Afe_WriteIdx_tmp;
			size_2 = copy_size - size_1;
#endif
#ifdef DL2_DEBUG_LOG
			pr_debug("size_1=0x%x, size_2=0x%x\n", size_1,
				       size_2);
#endif
			if (bCopy) {
				if (dataTransfer((Afe_Block->pucVirtBufAddr +
						  Afe_WriteIdx_tmp),
						 data_w_ptr, size_1) == -1)
					return -1;
			}
			spin_lock_irqsave(&pMemControl->substream_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&pMemControl->substream_lock,
					       flags);

			if (bCopy) {
				if (dataTransfer((Afe_Block->pucVirtBufAddr +
						  Afe_WriteIdx_tmp),
						 (data_w_ptr + size_1),
						 size_2) == -1)
					return -1;
			}
			spin_lock_irqsave(&pMemControl->substream_lock, flags);

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&pMemControl->substream_lock,
					       flags);
			count -= copy_size;
			data_w_ptr += copy_size;

			PRINT_DEBUG_LOG(
				"AudDrv_write finish2, DataRemained:%d, ReadIdx=%d, WriteIdx:%d\r\n",
				Afe_Block->u4DataRemained,
				Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx);
		}
	}
	return 0;
}

#ifdef AUDIO_DL2_ISR_COPY_SUPPORT

static void detectData(char *ptr, unsigned long count)
{
	if (get_LowLatencyDebug() & DEBUG_DL2_LOG_DETECT_DTAT) {
		int i;

		for (i = 0; i < count; i++) {
			if (ptr[i]) {
				pr_debug(
					"mtk_pcm_dl2_copy has data, i %d /%ld\n",
					i, count);
				break;
			}
		}
		if (i == count)
			pr_debug("mtk_pcm_dl2_copy no data\n");
	}
}

static int dataTransfer(void *dest, const void *src, uint32_t size)
{
	if (unlikely(!dest || !src)) {
		pr_err("%s, fail. dest %p, src %p\n", __func__, dest, src);
		return 0;
	}

	memcpy(dest, src, size);
	return 0;
}

void mtk_dl2_copy2buffer(const void *addr, uint32_t size)
{
	bool again = false;

	PRINT_DEBUG_LOG("%s, addr 0x%p 0x%p, size %d %d\n", __func__, addr,
			 ISRCopyBuffer.pBufferBase, size,
			 ISRCopyBuffer.u4BufferSize);

#ifdef AUDIO_64BYTE_ALIGN /* no need to do 64byte align */
	size = word_size_align(size);
#endif

	Auddrv_Dl2_Spinlock_lock();
retry:

	if (unlikely(ISRCopyBuffer.u4BufferSize)) {
		pr_info("%s, remaining data %d\n", __func__,
			ISRCopyBuffer.u4BufferSize);
		if (unlikely(get_LowLatencyDebug() & DEBUG_DL2_AEE_OTHERS))
			AUDIO_AEE("ISRCopy has remaining data !!");
	}

	if (unlikely(!ISRCopyBuffer.pBufferBase ||
		     size > ISRCopyBuffer.u4BufferSizeMax)) {
		if (!again) {
			/* realloc memory */
			kfree(ISRCopyBuffer.pBufferBase);
			ISRCopyBuffer.pBufferBase = kmalloc(size, GFP_ATOMIC);
			if (!ISRCopyBuffer.pBufferBase)
				pr_err("%s, alloc ISRCopyBuffer fail, size %d\n",
				       __func__, size);
			else
				ISRCopyBuffer.u4BufferSizeMax = size;

			again = true;
			goto retry;
		}
		pr_err("%s, alloc ISRCopyBuffer fail, again %d!!\n", __func__,
		       again);
		goto exit;
	}

	if (unlikely(copy_from_user(ISRCopyBuffer.pBufferBase, (char *)addr,
				    size))) {
		pr_warn("%s Fail copy from user !!\n", __func__);
		goto exit;
	}
	ISRCopyBuffer.pBufferIndx = ISRCopyBuffer.pBufferBase;
	ISRCopyBuffer.u4BufferSize = size;
	ISRCopyBuffer.u4IsrConsumeSize = 0; /* Restart */
exit:
	Auddrv_Dl2_Spinlock_unlock();
}

void mtk_dl2_copy_l(void)
{
	struct afe_block_t Afe_Block = pMemControl->rBlock;
	snd_pcm_uframes_t count = ISRCopyBuffer.u4BufferSize;

	if (unlikely(!ISRCopyBuffer.u4BufferSize || !ISRCopyBuffer.pBufferIndx))
		return;

	/* for debug */
	detectData((char *)ISRCopyBuffer.pBufferIndx, count);

	mtk_pcm_dl2_copy_((void *)ISRCopyBuffer.pBufferIndx, &count, &Afe_Block,
			  true);

	ISRCopyBuffer.pBufferIndx += count;
	ISRCopyBuffer.u4BufferSize -= count;
	ISRCopyBuffer.u4IsrConsumeSize += count;
}

static int mtk_pcm_dl2_copy(struct snd_pcm_substream *substream, int channel,
			    snd_pcm_uframes_t pos, void __user *dst,
			    snd_pcm_uframes_t count)
{
	struct afe_block_t *Afe_Block = &pMemControl->rBlock;
	int remainCount = 0;
	int ret = 0;
	int retryCount = 0;
#if defined(DL2_DEBUG_LOG)
	pr_debug(
		"%s pos = %lu count = %lu, BufferSize %d, ConsumeSize %d\n",
		__func__, pos, count, ISRCopyBuffer.u4BufferSize,
		ISRCopyBuffer.u4IsrConsumeSize);
#endif
	/* get total bytes to copy */
	count = audio_frame_to_bytes(substream, count);

	Auddrv_Dl2_Spinlock_lock();

	if (unlikely(!ISRCopyBuffer.pBufferIndx))
		goto exit;

	if (unlikely(get_LowLatencyDebug())) {
		/* check underflow */
		if (StartCheckTime) {
			if (PrevTime == 0)
				PrevTime = sched_clock();
			NowTime = sched_clock();

			if ((NowTime - PrevTime) > UnderflowTime) {
				pr_warn("%s, dl2 underflow, UnderflowTime %d, start %ld, end %ld\n",
					__func__, UnderflowTime, PrevTime,
					NowTime);
				if (get_LowLatencyDebug() &
				    DEBUG_DL2_AEE_UNDERFLOW)
					AUDIO_AEE(
						"mtk_pcm_dl2_copy - dl2 underflow");
			}
			PrevTime = NowTime;
		}
	}

retry:
	if (ISRCopyBuffer.u4IsrConsumeSize <= 0) {
		if (!ISRCopyBuffer.u4BufferSize)
			goto exit;

		if (unlikely(ISRCopyBuffer.u4BufferSize < count))
			count = ISRCopyBuffer.u4BufferSize;

		/* for debug */
		detectData((char *)ISRCopyBuffer.pBufferIndx, count);

		ret = mtk_pcm_dl2_copy_((void *)ISRCopyBuffer.pBufferIndx,
					&count, Afe_Block, true);

		ISRCopyBuffer.pBufferIndx += count;
		ISRCopyBuffer.u4BufferSize -= count;
	} else {
		if (unlikely(ISRCopyBuffer.u4IsrConsumeSize < count)) {
			remainCount = count - ISRCopyBuffer.u4IsrConsumeSize;
			count = ISRCopyBuffer.u4IsrConsumeSize;
		}

		ret = mtk_pcm_dl2_copy_((void *)ISRCopyBuffer.pBufferIndx,
					&count, Afe_Block, false);
		ISRCopyBuffer.u4IsrConsumeSize -= count;
		if (ISRCopyBuffer.u4IsrConsumeSize < 0)
			ISRCopyBuffer.u4IsrConsumeSize = 0;

		if (unlikely(remainCount)) {
			if ((++retryCount) > 1) {
				pr_debug(
					"%s, retryCount %d, remainCount %d, count %d\n",
					__func__, retryCount, remainCount,
					(int)count);
				if (retryCount > 5)
					goto exit;
			}
			count = remainCount;
			goto retry;
		}
	}

exit:

	Auddrv_Dl2_Spinlock_unlock();

	return ret;
}
#else

static int dataTransfer(void *dest, const void *src, uint32_t size)
{
	int ret = 0;

	if (unlikely(!access_ok(VERIFY_READ, src, size))) {
#if defined(DL2_DEBUG_LOG)
		pr_debug(
			"AudDrv_write 0ptr invalid data_w_ptr=%p, size=%d\n",
			src, size);
#endif
	} else {
#if defined(DL2_DEBUG_LOG)
		pr_debug(
			"memcpy VirtBufAddr+Afe_WriteIdx= %p,data_w_ptr = %p copy_size = 0x%x\n",
			dest, src, size);
#endif
		if (unlikely(copy_from_user(dest, src, size))) {
#if defined(DL2_DEBUG_LOG)
			pr_debug("AudDrv_write Fail copy from user\n");
#endif
			ret = -1;
		}
	}
	return ret;
}

static int mtk_pcm_dl2_copy(struct snd_pcm_substream *substream, int channel,
			    snd_pcm_uframes_t pos, void __user *dst,
			    snd_pcm_uframes_t count)
{
	struct afe_block_t *Afe_Block = &pMemControl->rBlock;

	/* get total bytes to copy */
	count = audio_frame_to_bytes(substream, count);
#if defined(DL2_DEBUG_LOG)
	pr_debug("+%s(), pos = %lu, count = %lu\n", __func__, pos, count);
#endif
	return mtk_pcm_dl2_copy_(dst, &count, Afe_Block, true);
}

#endif

static int mtk_pcm_dl2_silence(struct snd_pcm_substream *substream, int channel,
			       snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_pcm_dl2_page(struct snd_pcm_substream *substream,
				     unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_dl2_ops = {
	.open = mtk_pcm_dl2_open,
	.close = mtk_soc_pcm_dl2_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_dl2_params,
	.hw_free = mtk_pcm_dl2_hw_free,
	.prepare = mtk_pcm_dl2_prepare,
	.trigger = mtk_pcm_dl2_trigger,
	.pointer = mtk_pcm_dl2_pointer,
	.copy = mtk_pcm_dl2_copy,
	.silence = mtk_pcm_dl2_silence,
	.page = mtk_pcm_dl2_page,
	.mmap = mtk_pcm_mmap,
};

static const struct snd_soc_component_driver mtk_soc_component = {
	.name = AFE_PCM_NAME,
	.ops = &mtk_dl2_ops,
	.probe = mtk_asoc_dl2_component_probe,
};

#ifdef CONFIG_OF

static const struct of_device_id mt_soc_pcm_dl2_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_dl2",
	},
	{} };

#endif

static int mtk_soc_dl2_probe(struct platform_device *pdev)
{
#if defined(DL2_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", MT_SOC_DL2_PCM);
	} else {
		pr_err("%s invalid of_node\n", __func__);
		return -ENODEV;
	}

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	return snd_soc_register_component(&pdev->dev, &mtk_soc_component);
}

static int mtk_asoc_dl2_component_probe(struct snd_soc_component *component)
{
#if defined(DL2_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
	snd_soc_add_component_controls(component, fast_dl_controls,
				      ARRAY_SIZE(fast_dl_controls));

	/* allocate dram */
	AudDrv_Allocate_mem_Buffer(component->dev,
				   Soc_Aud_Digital_Block_MEM_DL2,
				   Dl2_MAX_BUFFER_SIZE);

	Dl2_Playback_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL2);
	return 0;
}

static int mtk_soc_dl2_remove(struct platform_device *pdev)
{
	AudDrv_Clk_Deinit(&pdev->dev);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static struct platform_driver mtk_dl2_driver = {
	.driver = {

			.name = MT_SOC_DL2_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_dl2_of_ids,
#endif
		},
	.probe = mtk_soc_dl2_probe,
	.remove = mtk_soc_dl2_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkdl2_dev;
#endif

static int __init mtk_dl2_soc_platform_init(void)
{
	int ret;
#if defined(DL2_DEBUG_LOG)
	pr_debug("%s\n", __func__);
#endif
#ifndef CONFIG_OF
	soc_mtkdl2_dev = platform_device_alloc(MT_SOC_DL2_PCM, -1);
	if (!soc_mtkdl2_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkdl2_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkdl2_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_dl2_driver);
	return ret;
}
module_init(mtk_dl2_soc_platform_init);

static void __exit mtk_dl2_soc_platform_exit(void)
{
	platform_driver_unregister(&mtk_dl2_driver);
}
module_exit(mtk_dl2_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
