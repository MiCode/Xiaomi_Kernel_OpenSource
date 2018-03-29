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
 *   mt_soc_pcm_i2s0.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio i2s0 playback
 *
 * Author:
 * -------
 * Chipeng Chang
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

static DEFINE_SPINLOCK(auddrv_I2S0_lock);
static AFE_MEM_CONTROL_T *pI2s0MemControl;

static struct device *mDev;

/*
 *    function implementation
 */

static int mtk_i2s0_probe(struct platform_device *pdev);
static int mtk_pcm_i2s0_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_i2s0_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_i2s0_probe(struct snd_soc_platform *platform);

static int mi2s0_sidegen_control;
static int mi2s0_hdoutput_control;
static const char * const i2s0_SIDEGEN[] = { "Off", "On48000", "On44100", "On32000", "On16000", "On8000" };
static const char * const i2s0_HD_output[] = { "Off", "On" };
static bool mPlaybackUseSram;

static const struct soc_enum Audio_i2s0_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(i2s0_SIDEGEN), i2s0_SIDEGEN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(i2s0_HD_output), i2s0_HD_output),
};

static int Audio_i2s0_SideGen_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_AmpR_Get = %d\n", mi2s0_sidegen_control);
	ucontrol->value.integer.value[0] = mi2s0_sidegen_control;
	return 0;
}

static int Audio_i2s0_SideGen_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	uint32 u32AudioI2S = 0;
	uint32 samplerate = 0;
	uint32 Audio_I2S_Dac;

	AudDrv_Clk_On();

	pr_warn("%s() samplerate = %d mi2s0_hdoutput_control = %d\n", __func__,
	       samplerate, mi2s0_hdoutput_control);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(i2s0_SIDEGEN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mi2s0_sidegen_control = ucontrol->value.integer.value[0];
	if (mi2s0_sidegen_control == 1) {
		samplerate = 48000;
	} else if (mi2s0_sidegen_control == 2) {
		samplerate = 44100;
	} else if (mi2s0_sidegen_control == 3) {
		samplerate = 32000;
	} else if (mi2s0_sidegen_control == 4) {
		samplerate = 16000;
		/* here start digital part */
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I14,
			      Soc_Aud_InterConnectionOutput_O00);
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I14,
			      Soc_Aud_InterConnectionOutput_O01);
	} else if (mi2s0_sidegen_control == 5) {
		samplerate = 8000;
		/* here start digital part */
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I14,
			      Soc_Aud_InterConnectionOutput_O00);
		SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I14,
			      Soc_Aud_InterConnectionOutput_O01);
	} else {
		samplerate = 48000;
		pr_err("Wrong sidegen_control input\n");
	}

	if (mi2s0_sidegen_control) {
		AudDrv_Clk_On();
		Afe_Set_Reg(AUDIO_TOP_CON1, 0x2, 0x2);	/* I2S_SOFT_Reset */
		Afe_Set_Reg(AUDIO_TOP_CON1, 0x1 << 4, 0x1 << 4);	/* I2S_SOFT_Reset */
		Audio_I2S_Dac = 0;
		SetCLkMclk(Soc_Aud_I2S0, samplerate);
		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, samplerate);

		Audio_I2S_Dac |= (Soc_Aud_LR_SWAP_NO_SWAP << 31);
		if (mi2s0_hdoutput_control == true) {
			/* Low jitter mode */
			Audio_I2S_Dac |= Soc_Aud_LOW_JITTER_CLOCK << 12;
		} else {
			/* normal mode */
			Audio_I2S_Dac |= Soc_Aud_NORMAL_CLOCK << 12;
		}
		Audio_I2S_Dac |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
		Audio_I2S_Dac |= (Soc_Aud_I2S_FORMAT_I2S << 3);
		Audio_I2S_Dac |= (Soc_Aud_I2S_WLEN_WLEN_32BITS << 1);


		u32AudioI2S = SampleRateTransform(samplerate) << 8;
		u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3;	/* us3 I2s format */
		u32AudioI2S |= Soc_Aud_I2S_WLEN_WLEN_32BITS << 1;	/* 32 BITS */
		if (mi2s0_hdoutput_control == true)
			u32AudioI2S |= Soc_Aud_LOW_JITTER_CLOCK << 12; /* Low jitter mode */
		else
			u32AudioI2S |= Soc_Aud_NORMAL_CLOCK << 12; /* Low jitter mode */

		/* start I2S DAC out */
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) == false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, true);
			Afe_Set_Reg(AFE_I2S_CON, Audio_I2S_Dac | 0x1, MASK_ALL);
			Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S | 1, AFE_MASK_ALL);
			Afe_Set_Reg(AUDIO_TOP_CON1, 0x0 << 4, 0x1 << 4);
			Afe_Set_Reg(AUDIO_TOP_CON1, 0x0, 0x2);	/* I2S_SOFT_Reset */
			EnableAfe(true);
		} else {
			pr_warn
			    ("%s(), mi2s0_sidegen_control=%d, write AFE_I2S_CON (0x%x), AFE_I2S_CON3(0x%x)\n",
			     __func__, mi2s0_sidegen_control, Audio_I2S_Dac, u32AudioI2S);
			Afe_Set_Reg(AFE_I2S_CON, 0x0, 0x1);
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, true);
			Afe_Set_Reg(AFE_I2S_CON, Audio_I2S_Dac | 0x1, MASK_ALL);
			Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S | 1, AFE_MASK_ALL);
			Afe_Set_Reg(AUDIO_TOP_CON1, 0x0 << 4, 0x1 << 4);
			Afe_Set_Reg(AUDIO_TOP_CON1, 0x0, 0x2);	/* I2S_SOFT_Reset */
			EnableAfe(true);
		}

	} else {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, false);
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) == false) {
			Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);
			Afe_Set_Reg(AFE_I2S_CON, 0x0, 0x1);
			SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I14,
				      Soc_Aud_InterConnectionOutput_O00);
			SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I14,
				      Soc_Aud_InterConnectionOutput_O01);
			EnableAfe(false);
		}
		AudDrv_Clk_Off();
	}
	AudDrv_Clk_Off();
	return 0;
}

static int Audio_i2s0_hdoutput_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_i2s0_hdoutput_Get = %d\n", mi2s0_hdoutput_control);
	ucontrol->value.integer.value[0] = mi2s0_hdoutput_control;
	return 0;
}

static int Audio_i2s0_hdoutput_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("+%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(i2s0_HD_output)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
#if 0				/* no */
	AudDrv_Clk_On();
	mi2s0_hdoutput_control = ucontrol->value.integer.value[0];
	if (mi2s0_hdoutput_control) {
		/* set APLL clock setting */
		EnableApll1(true);
		EnableApll2(true);
		EnableI2SDivPower(AUDIO_APLL1_DIV0, true);
		EnableI2SDivPower(AUDIO_APLL2_DIV0, true);
	} else {
		/* set APLL clock setting */
		EnableApll1(false);
		EnableApll2(false);
		EnableI2SDivPower(AUDIO_APLL1_DIV0, false);
		EnableI2SDivPower(AUDIO_APLL2_DIV0, false);
	}
	AudDrv_Clk_Off();
#endif
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_i2s0_controls[] = {
	SOC_ENUM_EXT("Audio_i2s0_SideGen_Switch", Audio_i2s0_Enum[0], Audio_i2s0_SideGen_Get,
		     Audio_i2s0_SideGen_Set),
	SOC_ENUM_EXT("Audio_i2s0_hd_Switch", Audio_i2s0_Enum[1], Audio_i2s0_hdoutput_Get,
		     Audio_i2s0_hdoutput_Set),
};

static struct snd_pcm_hardware mtk_i2s0_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.period_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int mtk_pcm_i2s0_stop(struct snd_pcm_substream *substream)
{
	AFE_BLOCK_T *Afe_Block = &(pI2s0MemControl->rBlock);

	pr_warn("mtk_pcm_i2s0_stop\n");
	irq_remove_user(substream, Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE);

	/* here start digital part */
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O00);
	SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O01);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

	/* stop I2S */
	Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);

	EnableAfe(false);

	/* clean audio hardware buffer */
	memset_io(Afe_Block->pucVirtBufAddr, 0, Afe_Block->u4BufferSize);
	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
	AudDrv_Clk_Off();

	return 0;
}

static snd_pcm_uframes_t mtk_pcm_i2s0_pointer(struct snd_pcm_substream
					      *substream)
{
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	kal_uint32 Frameidx = 0;
	kal_int32 Afe_consumed_bytes = 0;
	AFE_BLOCK_T *Afe_Block = &pI2s0MemControl->rBlock;
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

#ifdef AUDIO_64BYTE_ALIGN
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


static int mtk_pcm_i2s0_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	PRINTK_AUDDRV("mtk_pcm_hw_params\n");

	/* runtime->dma_bytes has to be set manually to allow mmap */
	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

	/* here to allcoate sram to hardware --------------------------- */
	AudDrv_Allocate_mem_Buffer(mDev, Soc_Aud_Digital_Block_MEM_DL1,
				   substream->runtime->dma_bytes);
	/* substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE; */
	substream->runtime->dma_area = (unsigned char *)Get_Afe_SramBase_Pointer();
	substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;

	/* ------------------------------------------------------- */
	PRINTK_AUDDRV("1 dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		      substream->runtime->dma_bytes, substream->runtime->dma_area,
		      (long)substream->runtime->dma_addr);

	return ret;
}

static int mtk_pcm_i2s0_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};

static int mPlaybackSramState;
static int mtk_pcm_i2s0_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	AfeControlSramLock();
	if (GetSramState() == SRAM_STATE_FREE) {
		mtk_i2s0_hardware.buffer_bytes_max = GetPLaybackSramFullSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKFULL;
		SetSramState(mPlaybackSramState);
		mPlaybackUseSram = true;
	}
#if 0				/* not support partial Sram case */
	else {
		mtk_i2s0_hardware.buffer_bytes_max = GetPLaybackSramPartial();
		mPlaybackSramState = SRAM_STATE_PLAYBACKPARTIAL;
		SetSramState(mPlaybackSramState);
	}
#else
	else {
		mtk_i2s0_hardware.buffer_bytes_max = GetPLaybackDramSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKDRAM;
		mPlaybackUseSram = false;
	}
#endif
	AfeControlSramUnLock();
	runtime->hw = mtk_i2s0_hardware;

	pr_warn("mtk_pcm_i2s0_open\n");

	AudDrv_Clk_On();
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_i2s0_hardware,
	       sizeof(struct snd_pcm_hardware));
	pI2s0MemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);


	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");

	/* print for hw pcm information */
	pr_warn("mtk_pcm_i2s0_open runtime rate = %d channels = %d substream->pcm->device = %d\n",
	       runtime->rate, runtime->channels, substream->pcm->device);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_warn("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_i2s0_playback_constraints\n");

	if (ret < 0) {
		pr_err("mtk_pcm_i2s0_close\n");
		mtk_pcm_i2s0_close(substream);
		return ret;
	}
	pr_warn("mtk_pcm_i2s0_open return\n");
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_pcm_i2s0_close(struct snd_pcm_substream *substream)
{
	pr_warn("%s\n", __func__);
	AfeControlSramLock();
	ClearSramState(mPlaybackSramState);
	mPlaybackSramState = GetSramState();
	AfeControlSramUnLock();
	return 0;
}

static int mtk_pcm_i2s0_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_pcm_i2s0_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	uint32 u32AudioI2S = 0;

	AudDrv_Clk_On();
	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
#if 0				/* not support 32 bit playback */
	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
	    runtime->format == SNDRV_PCM_FORMAT_S32_LE) {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
					     AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
	} else
#endif
	{
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
	}

	SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O00);
	SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O01);

	/* here start digital part */
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O00);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O01);


	u32AudioI2S = SampleRateTransform(runtime->rate) << 8;
	u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3;	/* us3 I2s format */
	u32AudioI2S |= Soc_Aud_I2S_WLEN_WLEN_16BITS << 1;	/* 16 BITS */

	if (mi2s0_hdoutput_control == true)
		u32AudioI2S |= Soc_Aud_LOW_JITTER_CLOCK << 12; /* Low jitter mode */

	pr_warn(" u32AudioI2S= 0x%x\n", u32AudioI2S);
	Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S | 1, AFE_MASK_ALL);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

	/* here to set interrupt */
	irq_add_user(substream,
		     Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE,
		     substream->runtime->rate,
		     substream->runtime->period_size);

	EnableAfe(true);

	return 0;
}

static int mtk_pcm_i2s0_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_warn("mtk_pcm_i2s0_trigger cmd = %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_i2s0_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_i2s0_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_i2s0_copy(struct snd_pcm_substream *substream,
			     int channel, snd_pcm_uframes_t pos,
			     void __user *dst, snd_pcm_uframes_t count)
{
	AFE_BLOCK_T *Afe_Block = NULL;
	int copy_size = 0, Afe_WriteIdx_tmp;
	unsigned long flags;
	char *data_w_ptr = (char *)dst;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	PRINTK_AUD_DL1("%s  pos = 0x%x count = 0x%x\n ", __func__, (unsigned int)pos,
		       (unsigned int)count);

	count = audio_frame_to_bytes(substream, count);

	/* check which memif nned to be write */
	Afe_Block = &pI2s0MemControl->rBlock;

	/* handle for buffer management */
	PRINTK_AUD_DL1(" WriteIdx=0x%x, ReadIdx=0x%x, DataRemained=0x%x\n",
		       Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);
	if (Afe_Block->u4BufferSize == 0) {
		pr_err(" u4BufferSize=0 Error");
		return 0;
	}

	AudDrv_checkDLISRStatus();

	spin_lock_irqsave(&auddrv_I2S0_lock, flags);
	copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;	/* free space of the buffer */
	spin_unlock_irqrestore(&auddrv_I2S0_lock, flags);
	if (count <=  copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

#ifdef AUDIO_64BYTE_ALIGN
	copy_size = Align64ByteSize(copy_size);
#endif
	PRINTK_AUD_DL1("copy_size=0x%x, count=0x%x\n", (unsigned int)copy_size,
		       (unsigned int)count);

	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_I2S0_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_I2S0_lock, flags);

		if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) {	/* copy once */
			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
				PRINTK_AUDDRV("0ptr invalid data_w_ptr=%p, size=%d", data_w_ptr,
					      copy_size);
				PRINTK_AUDDRV(" u4BufferSize=%d, u4DataRemained=%d",
					      Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {

				PRINTK_AUD_DL1("memcpy WriteIdx= %p data_w_ptr = %p copy_size = 0x%x\n",
					       Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, copy_size);
				if (mtk_local_audio_copy_from_user
				    (mPlaybackUseSram, (Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr,
				     copy_size)) {
					PRINTK_AUDDRV(" Fail copy from user\n");
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_I2S0_lock, flags);
			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_I2S0_lock, flags);
			data_w_ptr += copy_size;
			count -= copy_size;

			PRINTK_AUD_DL1
			    (" finish1, copy_size:%x, WriteIdx:%x, ReadIdx=%x, DataRemained:%x, count=%x \r\n",
			     copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
			     Afe_Block->u4DataRemained, (unsigned int)count);

		} else {	/* copy twice */
			kal_uint32 size_1 = 0, size_2 = 0;
#ifdef AUDIO_64BYTE_ALIGN
			size_1 = Align64ByteSize((Afe_Block->u4BufferSize - Afe_WriteIdx_tmp));
			size_2 = Align64ByteSize((copy_size - size_1));
#else
			size_1 = Afe_Block->u4BufferSize - Afe_WriteIdx_tmp;
			size_2 = copy_size - size_1;
#endif
			PRINTK_AUD_DL1("size_1=0x%x, size_2=0x%x\n", size_1, size_2);
			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
				pr_warn(" 1ptr invalid data_w_ptr=%p, size_1=%d", data_w_ptr,
				       size_1);
				pr_warn(" u4BufferSize=%d, u4DataRemained=%d",
				       Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {

				PRINTK_AUD_DL1
				    ("mcmcpy Afe_Block->pucVirtBufAddr+Afe_WriteIdx= %p data_w_ptr = %p size_1 = %x\n",
				     Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr,
				     size_1);
				if ((mtk_local_audio_copy_from_user
				     (mPlaybackUseSram, (Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr,
				      size_1))) {
					PRINTK_AUDDRV(" Fail 1 copy from user");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_I2S0_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&auddrv_I2S0_lock, flags);

			if (!access_ok(VERIFY_READ, data_w_ptr + size_1, size_2)) {
				PRINTK_AUDDRV("2ptr invalid data_w_ptr=%p, size_1=%d, size_2=%d",
					      data_w_ptr, size_1, size_2);
				PRINTK_AUDDRV("u4BufferSize=%d, u4DataRemained=%d",
					      Afe_Block->u4BufferSize, Afe_Block->u4DataRemained);
			} else {

				PRINTK_AUD_DL1("mcmcpy WriteIdx= %p data_w_ptr+size_1 = %p size_2 = %x\n",
					       Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp,
					       data_w_ptr + size_1, size_2);
				if ((mtk_local_audio_copy_from_user
				     (mPlaybackUseSram, (Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
				      (data_w_ptr + size_1), size_2))) {
					PRINTK_AUDDRV("AudDrv_write Fail 2  copy from user");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_I2S0_lock, flags);

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_I2S0_lock, flags);
			count -= copy_size;
			data_w_ptr += copy_size;

			PRINTK_AUD_DL1
			    (" finish2, copy size:%x, WriteIdx:%x,ReadIdx=%x DataRemained:%x \r\n",
			     copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
			     Afe_Block->u4DataRemained);
		}
	}
	return 0;
	PRINTK_AUD_DL1("pcm_copy return\n");

}

static int mtk_pcm_i2s0_silence(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	pr_warn("%s\n", __func__);
	return 0;		/* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_i2s0_pcm_page(struct snd_pcm_substream *substream, unsigned long offset)
{
	pr_warn("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]);	/* the same page */
}

static struct snd_pcm_ops mtk_i2s0_ops = {
	.open = mtk_pcm_i2s0_open,
	.close = mtk_pcm_i2s0_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_i2s0_hw_params,
	.hw_free = mtk_pcm_i2s0_hw_free,
	.prepare = mtk_pcm_i2s0_prepare,
	.trigger = mtk_pcm_i2s0_trigger,
	.pointer = mtk_pcm_i2s0_pointer,
	.copy = mtk_pcm_i2s0_copy,
	.silence = mtk_pcm_i2s0_silence,
	.page = mtk_i2s0_pcm_page,
};

static struct snd_soc_platform_driver mtk_i2s0_soc_platform = {
	.ops = &mtk_i2s0_ops,
	.pcm_new = mtk_asoc_pcm_i2s0_new,
	.probe = mtk_afe_i2s0_probe,
};

static int mtk_i2s0_probe(struct platform_device *pdev)
{
	pr_warn("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_I2S0_PCM);

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	mDev = &pdev->dev;

	return snd_soc_register_platform(&pdev->dev, &mtk_i2s0_soc_platform);
}

static int mtk_asoc_pcm_i2s0_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_warn("%s\n", __func__);
	return ret;
}


static int mtk_afe_i2s0_probe(struct snd_soc_platform *platform)
{
	pr_warn("mtk_afe_i2s0_probe\n");
	snd_soc_add_platform_controls(platform, Audio_snd_i2s0_controls,
				      ARRAY_SIZE(Audio_snd_i2s0_controls));
	return 0;
}

static int mtk_i2s0_remove(struct platform_device *pdev)
{
	pr_warn("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dl1_i2s0_of_ids[] = {
	{.compatible = "mediatek,mt_soc_pcm_dl1_i2s0",},
	{}
};
#endif

static struct platform_driver mtk_i2s0_driver = {
	.driver = {
		   .name = MT_SOC_I2S0_PCM,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_pcm_dl1_i2s0_of_ids,
#endif
		   },
	.probe = mtk_i2s0_probe,
	.remove = mtk_i2s0_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtki2s0_dev;
#endif

static int __init mtk_i2s0_soc_platform_init(void)
{
	int ret;

	pr_warn("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtki2s0_dev = platform_device_alloc(MT_SOC_I2S0_PCM, -1);
	if (!soc_mtki2s0_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtki2s0_dev);
	if (ret != 0) {
		platform_device_put(soc_mtki2s0_dev);
		return ret;
	}
#endif

	ret = platform_driver_register(&mtk_i2s0_driver);
	return ret;

}
module_init(mtk_i2s0_soc_platform_init);

static void __exit mtk_i2s0_soc_platform_exit(void)
{
	pr_warn("%s\n", __func__);

	platform_driver_unregister(&mtk_i2s0_driver);
}
module_exit(mtk_i2s0_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
