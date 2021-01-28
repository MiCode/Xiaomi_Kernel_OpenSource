// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt-soc-pcm-deep-buffer-dl.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio playback for deep buffer
 *
 * Author:
 * -------
 *   Shane Chien
 *
 *------------------------------------------------------------------------------
 *
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
#include <linux/dma-mapping.h>
#include <sound/pcm_params.h>

#include "mtk_mcdi_api.h"


#ifdef DEBUG_DEEP_BUFFER_DL
#define DEBUG_DEEP_BUFFER_DL(format, args...) pr_debug(format, ##args)
#else
#define DEBUG_DEEP_BUFFER_DL(format, args...)
#endif

#define CLEAR_BUFFER_US 600
static int CLEAR_BUFFER_SIZE;

static struct afe_mem_control_t *pMemControl;
static struct snd_dma_buffer deep_buffer_dl_dma_buf;
static unsigned int mPlaybackDramState;

static bool vcore_dvfs_enable;

/*
 *    function implementation
 */
static int deep_buffer_dl_hdoutput;
static bool mPrepareDone;
static const void *irq_user_id;
static unsigned int irq_cnt;
static struct device *mDev;

static int deep_buffer_mem_blk;
static int deep_buffer_mem_blk_io;

const char *const deep_buffer_dl_HD_output[] = {"Off", "On"};

static const struct soc_enum deep_buffer_dl_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(deep_buffer_dl_HD_output),
			    deep_buffer_dl_HD_output),
};

static int deep_buffer_dl_hdoutput_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() = %d\n", __func__, deep_buffer_dl_hdoutput);
	ucontrol->value.integer.value[0] = deep_buffer_dl_hdoutput;
	return 0;
}

static int deep_buffer_dl_hdoutput_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	/* pr_debug("%s()\n", __func__); */
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(deep_buffer_dl_HD_output)) {
		pr_warn("%s(), return -EINVAL\n", __func__);
		return -EINVAL;
	}

	deep_buffer_dl_hdoutput = ucontrol->value.integer.value[0];

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI) == true) {
		pr_debug("return HDMI enabled\n");
		return 0;
	}

	return 0;
}

static int Audio_Irqcnt_Get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	const struct Aud_RegBitsInfo *irqCntReg;

	AudDrv_Clk_On();
	irqCntReg =
		&GetIRQCtrlReg(irq_request_number(deep_buffer_mem_blk))->cnt;
	ucontrol->value.integer.value[0] = Afe_Get_Reg(irqCntReg->reg);
	AudDrv_Clk_Off();

	return 0;
}

static int Audio_Irqcnt_Set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), irq_user_id = %p, irq_cnt = %d, value = %ld\n",
		 __func__, irq_user_id, irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	irq_cnt = ucontrol->value.integer.value[0];

	AudDrv_Clk_On();
	if (irq_user_id && irq_cnt)
		irq_update_user(irq_user_id,
				irq_request_number(deep_buffer_mem_blk), 0,
				irq_cnt);
	else
		pr_debug(
			"cannot update irq counter, user_id = %p, irq_cnt = %d\n",
			irq_user_id, irq_cnt);

	AudDrv_Clk_Off();
	return 0;
}

static const struct snd_kcontrol_new deep_buffer_dl_controls[] = {
	SOC_ENUM_EXT("deep_buffer_dl_hd_Switch", deep_buffer_dl_Enum[0],
		     deep_buffer_dl_hdoutput_get, deep_buffer_dl_hdoutput_set),
	SOC_SINGLE_EXT("deep_buffer_irq_cnt", SND_SOC_NOPM, 0, IRQ_MAX_RATE, 0,
		       Audio_Irqcnt_Get, Audio_Irqcnt_Set),
};

static struct snd_pcm_hardware mtk_deep_buffer_dl_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = SOC_HIFI_DEEP_BUFFER_SIZE,
	.period_bytes_max = SOC_HIFI_DEEP_BUFFER_SIZE,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int mtk_deep_buffer_dl_stop(struct snd_pcm_substream *substream)
{
	/* struct afe_block_t *Afe_Block = &(pMemControl->rBlock); */

	pr_debug("%s\n", __func__);

	irq_user_id = NULL;
	irq_remove_user(substream, irq_request_number(deep_buffer_mem_blk));

	SetMemoryPathEnable(deep_buffer_mem_blk, false);

	ClearMemBlock(deep_buffer_mem_blk);

	return 0;
}

static snd_pcm_uframes_t
mtk_deep_buffer_dl_pointer(struct snd_pcm_substream *substream)
{
	unsigned int ptr_bytes = 0, hw_ptr, base, cur;

	base = GetBufferCtrlReg(deep_buffer_mem_blk_io, aud_buffer_ctrl_base);
	cur = GetBufferCtrlReg(deep_buffer_mem_blk_io, aud_buffer_ctrl_cur);

	if (!base || !cur) {
		pr_warn("%s(), GetBufferCtrlReg return invalid value\n",
			__func__);
	} else {
		hw_ptr = Afe_Get_Reg(cur);
		if (hw_ptr == 0)
			pr_warn("%s(), hw_ptr == 0\n", __func__);
		else
			ptr_bytes = hw_ptr - Afe_Get_Reg(base);

		ptr_bytes = word_size_align(ptr_bytes);
	}

	return bytes_to_frames(substream->runtime, ptr_bytes);
}

static int mtk_deep_buffer_dl_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
	if (substream->runtime->dma_bytes <= GetPLaybackSramFullSize() &&
	    AllocateAudioSram(&substream->runtime->dma_addr,
			      &substream->runtime->dma_area,
			      substream->runtime->dma_bytes, substream,
			      params_format(hw_params), false) == 0) {
		SetHighAddr(deep_buffer_mem_blk, false,
			    substream->runtime->dma_addr);
	} else {
		substream->runtime->dma_area = deep_buffer_dl_dma_buf.area;
		substream->runtime->dma_addr = deep_buffer_dl_dma_buf.addr;
		SetHighAddr(deep_buffer_mem_blk, true,
			    substream->runtime->dma_addr);
		mPlaybackDramState = true;
		AudDrv_Emi_Clk_On();
	}

	set_mem_block(substream, hw_params, pMemControl, deep_buffer_mem_blk);

	pr_debug("dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		 substream->runtime->dma_bytes, substream->runtime->dma_area,
		 (long)substream->runtime->dma_addr);

	return ret;
}

static int mtk_deep_buffer_dl_hw_free(struct snd_pcm_substream *substream)
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

static int mtk_deep_buffer_dl_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	if (is_irq_from_ext_module()) {
		ext_sync_signal_lock();
		ext_sync_signal_unlock();
	}

	if (mPrepareDone == true) {
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  deep_buffer_mem_blk_io,
				  Soc_Aud_AFE_IO_Block_I2S3);
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  deep_buffer_mem_blk_io,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC);
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  deep_buffer_mem_blk_io,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

		/* stop DAC output */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);

		RemoveMemifSubStream(deep_buffer_mem_blk, substream);

		if (deep_buffer_dl_hdoutput == true) {
			pr_debug("%s deep_buffer_dl_hdoutput == %d\n", __func__,
				 deep_buffer_dl_hdoutput);

			/* here to close APLL */
			if (!mtk_soc_always_hd) {
				DisableAPLLTunerbySampleRate(
					substream->runtime->rate);
				DisableALLbySampleRate(
					substream->runtime->rate);
			}

			EnableI2SCLKDiv(Soc_Aud_I2S1_MCKDIV, false);
		}
		EnableAfe(false);
		mPrepareDone = false;
	}

	irq_cnt = 0; /* reset irq_cnt */

	AudDrv_Clk_Off();

	vcore_dvfs(&vcore_dvfs_enable, true);

	system_idle_hint_request(SYSTEM_IDLE_HINT_USER_AUDIO, 0);

	return 0;
}

static int mtk_deep_buffer_dl_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_debug(
		"%s: hardware.buffer_bytes_max = %zu mPlaybackDramState = %d\n",
		__func__, mtk_deep_buffer_dl_hardware.buffer_bytes_max,
		mPlaybackDramState);

	mPlaybackDramState = false;
	runtime->hw = mtk_deep_buffer_dl_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_deep_buffer_dl_hardware,
	       sizeof(struct snd_pcm_hardware));

	AudDrv_Clk_On();

	system_idle_hint_request(SYSTEM_IDLE_HINT_USER_AUDIO, 1);

	pMemControl = Get_Mem_ControlT(deep_buffer_mem_blk);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);

	if (ret < 0) {
		pr_err("snd_pcm_hw_constraint_integer failed, close pcm\n");
		mtk_deep_buffer_dl_close(substream);
		return ret;
	}

	return 0;
}

static int mtk_deep_buffer_dl_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	bool mI2SWLen;

	pr_debug(
		"%s: mPrepareDone = %d, format = %d, SNDRV_PCM_FORMAT_S32_LE = %d, SNDRV_PCM_FORMAT_U32_LE = %d, sample rate = %d\n",
		__func__, mPrepareDone, runtime->format,
		SNDRV_PCM_FORMAT_S32_LE, SNDRV_PCM_FORMAT_U32_LE,
		substream->runtime->rate);

	if (mPrepareDone == false) {
		SetMemifSubStream(deep_buffer_mem_blk, substream);

		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
			SetMemIfFetchFormatPerSample(
				deep_buffer_mem_blk,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			SetMemIfFetchFormatPerSample(deep_buffer_mem_blk,
						     AFE_WLEN_16_BIT);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		}

		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  deep_buffer_mem_blk_io,
				  Soc_Aud_AFE_IO_Block_I2S3);
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  deep_buffer_mem_blk_io,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC);
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  deep_buffer_mem_blk_io,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

		/* I2S out Setting */
		if (deep_buffer_dl_hdoutput == true) {
			pr_debug("%s deep_buffer_dl_hdoutput == %d\n", __func__,
				 deep_buffer_dl_hdoutput);
			/* here to open APLL */
			if (!mtk_soc_always_hd) {
				EnableALLbySampleRate(runtime->rate);
				EnableAPLLTunerbySampleRate(runtime->rate);
			}

			SetCLkMclk(Soc_Aud_I2S1,
				   runtime->rate); /* select I2S */
			EnableI2SCLKDiv(Soc_Aud_I2S1_MCKDIV, true);
		}

		/* start I2S DAC out */
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) ==
		    false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
					    true);
			SetI2SDacOut(substream->runtime->rate,
				     deep_buffer_dl_hdoutput, mI2SWLen);
			SetI2SDacEnable(true);
		} else {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
					    true);
		}

		EnableAfe(true);
		mPrepareDone = true;

		CLEAR_BUFFER_SIZE = substream->runtime->rate * CLEAR_BUFFER_US *
				    audio_frame_to_bytes(substream, 1) /
				    1000000;
	}
	return 0;
}

static int mtk_deep_buffer_dl_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);

	/* here to set interrupt */
	irq_add_user(substream, irq_request_number(deep_buffer_mem_blk),
		     substream->runtime->rate,
		     irq_cnt ? irq_cnt : substream->runtime->period_size);
	irq_user_id = substream;

	SetSampleRate(deep_buffer_mem_blk, runtime->rate);
	SetChannels(deep_buffer_mem_blk, runtime->channels);
	SetMemoryPathEnable(deep_buffer_mem_blk, true);

	EnableAfe(true);

	return 0;
}

static int mtk_deep_buffer_dl_trigger(struct snd_pcm_substream *substream,
				      int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_deep_buffer_dl_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_deep_buffer_dl_stop(substream);
	}
	return -EINVAL;
}

static void *dummy_page[2];
static struct page *mtk_deep_buffer_dl_page(struct snd_pcm_substream *substream,
					    unsigned long offset)
{
	pr_debug("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static int mtk_deep_buffer_dl_ack(struct snd_pcm_substream *substream)
{
	int size_per_frame = audio_frame_to_bytes(substream, 1);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int copy_size = word_size_align(snd_pcm_playback_avail(runtime) *
					size_per_frame);
	if (pMemControl == NULL || mPrepareDone == false)
		return 0;

	if (copy_size > CLEAR_BUFFER_SIZE)
		copy_size = CLEAR_BUFFER_SIZE;

	if (copy_size > 0) {
		struct afe_block_t *afe_block_t = &pMemControl->rBlock;
		snd_pcm_uframes_t appl_ofs =
			runtime->control->appl_ptr % runtime->buffer_size;
		int32_t u4WriteIdx = appl_ofs * size_per_frame;

		if (u4WriteIdx + copy_size < afe_block_t->u4BufferSize) {
			memset_io(afe_block_t->pucVirtBufAddr + u4WriteIdx, 0,
				  copy_size);
			/*
			 * pr_debug("%s A, offset %d, clear buffer %d, copy_size
			 * %d\n",
			 *          __func__, u4WriteIdx, copy_size, copy_size);
			 */
		} else {
			int32_t size_1 = 0, size_2 = 0;

			size_1 = word_size_align(
				(afe_block_t->u4BufferSize - u4WriteIdx));
			size_2 = word_size_align((copy_size - size_1));

			if (size_1 < 0 || size_2 < 0) {
				pr_debug("%s, copy size error!!\n", __func__);
				pr_debug("u4BufferSize %d, u4WriteIdx %d\n",
					 afe_block_t->u4BufferSize, u4WriteIdx);
				return 0;
			}
			memset_io(afe_block_t->pucVirtBufAddr + u4WriteIdx, 0,
				  size_1);
			memset_io(afe_block_t->pucVirtBufAddr, 0, size_2);
			/*
			 * pr_debug("%s B-1, offset %d, clear buffer %d,
			 * copy_size %d\n",
			 *          __func__, u4WriteIdx, size_1, copy_size);
			 * pr_debug("%s B-2, offset %d, clear buffer %d,
			 * copy_size %d\n",
			 *          __func__, 0, size_2, copy_size);
			 */
		}
	}

	return 0;
}

static struct snd_pcm_ops mtk_deep_buffer_dl_ops = {
	.open = mtk_deep_buffer_dl_open,
	.close = mtk_deep_buffer_dl_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_deep_buffer_dl_hw_params,
	.hw_free = mtk_deep_buffer_dl_hw_free,
	.prepare = mtk_deep_buffer_dl_prepare,
	.trigger = mtk_deep_buffer_dl_trigger,
	.pointer = mtk_deep_buffer_dl_pointer,
	.page = mtk_deep_buffer_dl_page,
	.ack = mtk_deep_buffer_dl_ack,
};

static int mtk_deep_buffer_dl_platform_probe(struct snd_soc_component *component)
{
	deep_buffer_mem_blk =
		get_usage_digital_block(AUDIO_USAGE_DEEPBUFFER_PLAYBACK);
	deep_buffer_mem_blk_io =
		get_usage_digital_block_io(AUDIO_USAGE_DEEPBUFFER_PLAYBACK);

	if (deep_buffer_mem_blk < 0 || deep_buffer_mem_blk_io < 0) {
		pr_debug("%s(), invalid mem blk %d, io %d, use default\n",
			 __func__, deep_buffer_mem_blk, deep_buffer_mem_blk_io);
		deep_buffer_mem_blk = Soc_Aud_Digital_Block_MEM_DL1_DATA2;
		deep_buffer_mem_blk_io = Soc_Aud_AFE_IO_Block_MEM_DL1_DATA2;
	}

	pr_debug("%s(), deep_buffer_mem_blk %d, deep_buffer_mem_blk_io %d\n",
		 __func__, deep_buffer_mem_blk, deep_buffer_mem_blk_io);

	snd_soc_add_component_controls(component, deep_buffer_dl_controls,
				      ARRAY_SIZE(deep_buffer_dl_controls));
	/* allocate dram */
	deep_buffer_dl_dma_buf.area = dma_alloc_coherent(
		component->dev, SOC_HIFI_DEEP_BUFFER_SIZE,
		&deep_buffer_dl_dma_buf.addr, GFP_KERNEL | GFP_DMA);
	if (!deep_buffer_dl_dma_buf.area)
		return -ENOMEM;

	deep_buffer_dl_dma_buf.bytes = SOC_HIFI_DEEP_BUFFER_SIZE;
	pr_debug("area = %p\n", deep_buffer_dl_dma_buf.area);

	return 0;
}

static const struct snd_soc_component_driver mtk_deep_buffer_dl_soc_component = {
	.name = AFE_PCM_NAME,
	.ops = &mtk_deep_buffer_dl_ops,
	.probe = mtk_deep_buffer_dl_platform_probe,
};

static int mtk_deep_buffer_dl_probe(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_DEEP_BUFFER_DL_PCM);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	mDev = &pdev->dev;

	return snd_soc_register_component(&pdev->dev,
					  &mtk_deep_buffer_dl_soc_component,
					  NULL,
					  0);
}

static int mtk_deep_buffer_dl_remove(struct platform_device *pdev)
{
	pr_debug("%s()\n", __func__);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_deep_buffer_dl_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_deep_buffer_dl",
	},
	{} };
#endif

static struct platform_driver mtk_deep_buffer_dl_driver = {
	.driver = {

			.name = MT_SOC_DEEP_BUFFER_DL_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_deep_buffer_dl_of_ids,
#endif
		},
	.probe = mtk_deep_buffer_dl_probe,
	.remove = mtk_deep_buffer_dl_remove,
};

module_platform_driver(mtk_deep_buffer_dl_driver);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
