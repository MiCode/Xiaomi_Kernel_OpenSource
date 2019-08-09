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



/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/time.h>
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
#include "mt_soc_codec_63xx.h"


static struct AFE_MEM_CONTROL_T *pHp_impedance_MemControl;
static const int DCoffsetDefault = 1500;
static const int DCoffsetVariance = 2;

static const int mDcRangestep = 7;
static const int HpImpedancePhase1Step = 150;
static const int HpImpedancePhase2Step = 400;
static const int HpImpedancePhase1AdcValue = 1200;
static const int HpImpedancePhase2AdcValue = 7200;
static struct snd_dma_buffer *Dl1_Playback_dma_buf;

/*
 *    function implementation
 */

/* void StartAudioPcmHardware(void); */
/* void StopAudioPcmHardware(void); */
static int mtk_soc_hp_impedance_probe(struct platform_device *pdev);
static int mtk_soc_pcm_hp_impedance_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_hp_impedance_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_asoc_dhp_impedance_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_pcm_hp_impedance_hardware = {

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

#if 0				/* not used */
static int mtk_pcm_hp_impedance_stop(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_pcm_hp_impedance_stop\n");
	return 0;
}

static int mtk_pcm_hp_impedance_start(struct snd_pcm_substream *substream)
{
	return 0;
}
#endif

static snd_pcm_uframes_t mtk_pcm_hp_impedance_pointer(
	struct snd_pcm_substream *substream)
{
	return 0;
}

static void SetDL1Buffer(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct AFE_BLOCK_T *pblock = &pHp_impedance_MemControl->rBlock;

	pblock->pucPhysBufAddr = runtime->dma_addr;
	pblock->pucVirtBufAddr = runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;

	pr_debug("%s, Size = %d, pucVirtBufAddr = %p, pucPhysBufAddr = 0x%x\n",
		__func__,
		pblock->u4BufferSize,
		pblock->pucVirtBufAddr,
		pblock->pucPhysBufAddr);

	/* set dram address top hardware */
	Afe_Set_Reg(AFE_DL1_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_DL1_END,
		pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1),
		0xffffffff);
	memset((void *)pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
}


static int mtk_pcm_hp_impedance_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	pr_debug("mtk_pcm_hp_impedance_params\n");

	/* runtime->dma_bytes has to be set manually to allow mmap */
	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

	substream->runtime->dma_area = Dl1_Playback_dma_buf->area;
	substream->runtime->dma_addr = Dl1_Playback_dma_buf->addr;
	SetDL1Buffer(substream, hw_params);

	pr_debug("dma_bytes = %zu, dma_area = %p, dma_addr = 0x%lx\n",
		substream->runtime->dma_bytes, substream->runtime->dma_area,
		(long)substream->runtime->dma_addr);
	return ret;
}

static int mtk_pcm_hp_impedance_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_pcm_hp_impedance_hw_free\n");
	return 0;
}


static struct snd_pcm_hw_constraint_list
	constraints_hp_impedance_sample_rates = {

	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_hp_impedance_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("mtk_pcm_hp_impedance_open\n");
	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();
	AudDrv_Emi_Clk_On();
	pHp_impedance_MemControl =
		Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
	runtime->hw = mtk_pcm_hp_impedance_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hp_impedance_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
		&constraints_hp_impedance_sample_rates);

	if (ret < 0)
		pr_debug("snd_pcm_hw_constraint_integer failed\n");

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("mtkalsa_playback_constraints\n");

	if (ret < 0) {
		pr_debug("mtk_soc_pcm_hp_impedance_close\n");
		mtk_soc_pcm_hp_impedance_close(substream);
		return ret;
	}

	return 0;
}


bool mPrepareDone;
static int mtk_pcm_hp_impedance_prepare(struct snd_pcm_substream *substream)
{
	bool mI2SWLen;
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("mtk_pcm_hp_impedance_prepare\n");
	if (mPrepareDone == false) {
		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE
		    || runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL1,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL2,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
				Soc_Aud_InterConnectionOutput_O03);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
				Soc_Aud_InterConnectionOutput_O04);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL2,
				AFE_WLEN_16_BIT);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
				Soc_Aud_InterConnectionOutput_O03);
			SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
				Soc_Aud_InterConnectionOutput_O04);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		}

		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, runtime->rate);

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
		SetConnection(Soc_Aud_InterCon_Connection,
			Soc_Aud_InterConnectionInput_I05,
			Soc_Aud_InterConnectionOutput_O03);
		SetConnection(Soc_Aud_InterCon_Connection,
			Soc_Aud_InterConnectionInput_I06,
			Soc_Aud_InterConnectionOutput_O04);

		SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
		SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

		EnableAfe(true);
		mPrepareDone = true;
	}
	return 0;
}


static int mtk_soc_pcm_hp_impedance_close(struct snd_pcm_substream *substream)
{
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	pr_debug("%s\n", __func__);

	if (mPrepareDone == true) {
		mPrepareDone = false;
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);
		EnableAfe(false);
	}

	AudDrv_Emi_Clk_Off();
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();
	return 0;
}

static int mtk_pcm_hp_impedance_trigger(struct snd_pcm_substream *substream,
	int cmd)
{
	pr_debug("mtk_pcm_hp_impedance_trigger cmd = %d\n", cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}
	return -EINVAL;
}

static int mtk_pcm_hp_impedance_copy(struct snd_pcm_substream *substream,
	int channel, snd_pcm_uframes_t pos,
	void __user *dst, snd_pcm_uframes_t count)
{
	return 0;
}

static int mtk_pcm_hp_impedance_silence(struct snd_pcm_substream *substream,
	int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	pr_debug("%s\n", __func__);
	return 0;/* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_pcm_hp_impedance_page(
	struct snd_pcm_substream *substream,
	unsigned long offset)
{
	pr_debug("%s\n", __func__);
	/* the same page */
	return virt_to_page(dummy_page[substream->stream]);
}


static unsigned short mhp_impedance;
static unsigned short mAuxAdc_Offset;
static int Audio_HP_ImpeDance_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
#ifndef DENALI_FPGA_EARLYPORTING

#ifdef CONFIG_MTK_FPGA
	const int off_counter = 20;
#endif
	pr_debug("%s\n", __func__);

	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();

	/* set dc value to hardware */
	mhp_impedance = ucontrol->value.integer.value[0];
	msleep(1 * 1000);

	/* start get adc value */
	if (mAuxAdc_Offset == 0) {
		OpenAnalogTrimHardware(true);
		setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
		setOffsetTrimBufferGain(3);
		EnableTrimbuffer(true);
		/* msleep(5); */
		usleep_range(5*1000, 20*1000);
#ifdef CONFIG_MTK_FPGA
		mAuxAdc_Offset = PMIC_IMM_GetOneChannelValue(MT6328_AUX_CH9,
			off_counter, 0);
#else
		mAuxAdc_Offset = 0;
#endif
		pr_debug("mAuxAdc_Offset = %d\n", mAuxAdc_Offset);

#ifdef AUDIO_MEM_IOREMAP
		memset((void *)Get_Afe_SramBase_Pointer(),
			ucontrol->value.integer.value[0],
			AFE_INTERNAL_SRAM_SIZE);
#else
		memset((void *)Get_Afe_SramBase_Pointer(),
		ucontrol->value.integer.value[0],
		AFE_INTERNAL_SRAM_SIZE);
#endif
		msleep(5 * 1000);
		pr_debug("4 %s\n", __func__);

		EnableTrimbuffer(false);
		OpenAnalogTrimHardware(false);
	}

	if (mhp_impedance == 1) {
		pr_debug("start open hp impedance setting\n");
		OpenHeadPhoneImpedanceSetting(true);
		setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPR);
		setOffsetTrimBufferGain(3);
		EnableTrimbuffer(true);
		setHpGainZero();
#ifdef AUDIO_MEM_IOREMAP
		memset((void *)Get_Afe_SramBase_Pointer(),
			ucontrol->value.integer.value[0],
			AFE_INTERNAL_SRAM_SIZE);
#else
		memset((void *)Get_Afe_SramBase_Pointer(),
			ucontrol->value.integer.value[0],
			AFE_INTERNAL_SRAM_SIZE);
#endif
		msleep(5 * 1000);
		pr_debug("5 %s\n", __func__);
	} else if (mhp_impedance == 0) {
		pr_debug("stop hp impedance setting\n");
		OpenHeadPhoneImpedanceSetting(false);
		setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
		EnableTrimbuffer(false);
		SetSdmLevel(AUDIO_SDM_LEVEL_NORMAL);
	} else {
		unsigned short value = 0;

		for (value = 0; value < 10000; value += 200) {
			unsigned short temp = value;
			static unsigned short dcoffset;

			unsigned short *Sramdata;
			int i = 0;

			pr_debug("set sram to dc value = %d\n", temp);
			Sramdata = Get_Afe_SramBase_Pointer();

			for (i = 0; i < AFE_INTERNAL_SRAM_SIZE >> 1; i++) {
				*Sramdata = temp;
				Sramdata++;
			}

			Sramdata = Get_Afe_SramBase_Pointer();
			pr_debug("Sramdata = %p\n", Sramdata);
			pr_debug("Sramdata = 0x%x, Sramdata + 1 = 0x%x,",
				*Sramdata, *(Sramdata + 1));
			pr_debug(" Sramdata + 2 = 0x%x, Sramdata + 3 = 0x%x\n",
				*(Sramdata + 2),
				*(Sramdata + 3));
			Sramdata += 4;

			pr_debug("Sramdata = %p\n", Sramdata);
			pr_debug("Sramdata = 0x%x, Sramdata + 1 = 0x%x,",
				*Sramdata, *(Sramdata + 1));
			pr_debug(" Sramdata + 2 = 0x%x, Sramdata + 3 = 0x%x\n",
				*(Sramdata + 2),
				*(Sramdata + 3));

			msleep(20);
			dcoffset = 0;
#ifdef CONFIG_MTK_FPGA
			dcoffset = PMIC_IMM_GetOneChannelValue(MT6328_AUX_CH9,
				off_counter, 0);
#else
			dcoffset = 0;
#endif
			pr_debug("dcoffset = %d\n", dcoffset);
			msleep(3 * 1000);
		}
	}

	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();
#endif
	return 0;
}

static int phase1table[] = { 7, 13 };

static unsigned short Phase1Check(unsigned short adcvalue,
	unsigned int adcoffset)
{
	unsigned int AdcDiff = adcvalue - adcoffset;

	if (adcvalue < adcoffset)
		return 0;

	if (AdcDiff > 300)
		return AUDIO_HP_IMPEDANCE32;
	else if (AdcDiff >= phase1table[1])
		return AUDIO_HP_IMPEDANCE256;
	else if ((AdcDiff >= phase1table[0]) && (AdcDiff <= phase1table[1]))
		return AUDIO_HP_IMPEDANCE128;
	else
		return 0;
}

static int phase2table[] = { 10, 24 };

static unsigned short Phase2Check(unsigned short adcvalue,
	unsigned int adcoffset)
{
	unsigned int AdcDiff = adcvalue - adcoffset;

	if (adcvalue < adcoffset)
		return AUDIO_HP_IMPEDANCE16;

	if (AdcDiff < phase2table[0])
		return AUDIO_HP_IMPEDANCE16;
	else if (AdcDiff >= phase2table[1])
		return AUDIO_HP_IMPEDANCE64;
	else
		return AUDIO_HP_IMPEDANCE32;
}

static void FillDatatoDlmemory(unsigned int *memorypointer,
	unsigned int fillsize,
	unsigned short value)
{
	int addr = 0;
	unsigned int tempvalue = value;

	tempvalue = tempvalue << 16;
	tempvalue |= value;

	/* set memory to DC value */
	for (addr = 0; addr < (fillsize >> 2); addr++) {
		*memorypointer = tempvalue;
		memorypointer++;
	}
}

static unsigned short dcinit_value;
static void CheckDcinitValue(void)
{
	if (dcinit_value > (DCoffsetDefault + DCoffsetVariance)) {
		pr_debug("%s dcinit_value = %d\n", __func__, dcinit_value);
		dcinit_value = DCoffsetDefault;
	} else if (dcinit_value < (DCoffsetDefault - DCoffsetVariance)) {
		pr_debug("%s dcinit_value = %d\n", __func__, dcinit_value);
		dcinit_value = DCoffsetDefault;
	}
}

static void ApplyDctoDl(void)
{
#ifndef DENALI_FPGA_EARLYPORTING

	unsigned short value = 0, average = 0;
	unsigned short dcoffset, dcoffset2, dcoffset3;

	pr_debug("%s\n", __func__);

	dcinit_value = DCoffsetDefault;
	for (value = 0;
		value <= (HpImpedancePhase2AdcValue + HpImpedancePhase2Step);
		value += HpImpedancePhase1Step) {
		unsigned int *Sramdata =
			(unsigned int *)(Dl1_Playback_dma_buf->area);

		FillDatatoDlmemory(Sramdata,
			Dl1_Playback_dma_buf->bytes,
			value);
		/* apply to dram */

		/* add dcvalue for phase boost */
		if (value > HpImpedancePhase1AdcValue)
			value += HpImpedancePhase1Step;

		/* save for DC =0 offset */
		if (value == 0) {
			/* get adc value */
			/* msleep(1); */
			usleep_range(1*1000, 20*1000);
#ifdef CONFIG_MTK_FPGA
			dcoffset = PMIC_IMM_GetOneChannelValue(MT6328_AUX_CH9,
				5, 0);
			dcoffset2 = PMIC_IMM_GetOneChannelValue(MT6328_AUX_CH9,
				5, 0);
			dcoffset3 = PMIC_IMM_GetOneChannelValue(MT6328_AUX_CH9,
				5, 0);
#else
			dcoffset = 0;
			dcoffset2 = 0;
			dcoffset3 = 0;
#endif
			average = (dcoffset + dcoffset2 + dcoffset3) / 3;
			dcinit_value = average;
			CheckDcinitValue();
			pr_debug("dcinit_value = %d average = %d value = %d\n",
				dcinit_value, average, value);
		}

		/* start checking */
		if (value == HpImpedancePhase1AdcValue) {
			/* get adc value */
			/* msleep(1); */
			usleep_range(1*1000, 20*1000);
#ifdef CONFIG_MTK_FPGA
			dcoffset = PMIC_IMM_GetOneChannelValue(MT6328_AUX_CH9,
				5, 0);
			dcoffset2 = PMIC_IMM_GetOneChannelValue(MT6328_AUX_CH9,
				5, 0);
			dcoffset3 = PMIC_IMM_GetOneChannelValue(MT6328_AUX_CH9,
				5, 0);
#else
			dcoffset = 0;
			dcoffset2 = 0;
			dcoffset3 = 0;
#endif
			average = (dcoffset + dcoffset2 + dcoffset3) / 3;
			mhp_impedance = Phase1Check(average, dcinit_value);
			pr_debug("val = %d average = %d value = %d imp = %d\n",
			       value, average, dcinit_value, mhp_impedance);

			if (mhp_impedance)
				break;

		} else if (value >= HpImpedancePhase2AdcValue) {
			/* get adc value */
			/* msleep(1); */
			usleep_range(1*1000, 20*1000);
#ifdef CONFIG_MTK_FPGA
			dcoffset = PMIC_IMM_GetOneChannelValue(MT6328_AUX_CH9,
				5, 0);
			dcoffset2 = PMIC_IMM_GetOneChannelValue(MT6328_AUX_CH9,
				5, 0);
			dcoffset3 = PMIC_IMM_GetOneChannelValue(MT6328_AUX_CH9,
				5, 0);
#else
			dcoffset = 0;
			dcoffset2 = 0;
			dcoffset3 = 0;
#endif
			average = (dcoffset + dcoffset2 + dcoffset3) / 3;
			mhp_impedance = Phase2Check(average, dcinit_value);
			pr_debug("value = %d average = %d dcinit_value = %d\n",
				value,
				average,
				dcinit_value);
			break;
		}
	}
#endif
}

static int Audio_HP_ImpeDance_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("+%s()\n", __func__);

	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();

	if (OpenHeadPhoneImpedanceSetting(true) == true) {
		setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPR);
		setOffsetTrimBufferGain(2);
		EnableTrimbuffer(true);
		setHpGainZero();
		ApplyDctoDl();
		SetSdmLevel(AUDIO_SDM_LEVEL_MUTE);
		/* msleep(1); */
		usleep_range(1*1000, 20*1000);
		OpenHeadPhoneImpedanceSetting(false);
		setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
		EnableTrimbuffer(false);
		SetSdmLevel(AUDIO_SDM_LEVEL_NORMAL);
	} else {
		pr_debug("Audio_HP_ImpeDance_Get just do nothing\n");
	}
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();
	ucontrol->value.integer.value[0] = mhp_impedance;
	pr_debug("-%s()\n", __func__);
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_hp_impedance_controls[] = {

	SOC_SINGLE_EXT("Audio HP ImpeDance Setting",
		SND_SOC_NOPM, 0, 65536, 0,
		Audio_HP_ImpeDance_Get, Audio_HP_ImpeDance_Set),
};


static struct snd_pcm_ops mtk_hp_impedance_ops = {

	.open = mtk_pcm_hp_impedance_open,
	.close = mtk_soc_pcm_hp_impedance_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_hp_impedance_params,
	.hw_free = mtk_pcm_hp_impedance_hw_free,
	.prepare = mtk_pcm_hp_impedance_prepare,
	.trigger = mtk_pcm_hp_impedance_trigger,
	.pointer = mtk_pcm_hp_impedance_pointer,
	.copy = mtk_pcm_hp_impedance_copy,
	.silence = mtk_pcm_hp_impedance_silence,
	.page = mtk_pcm_hp_impedance_page,
};

static struct snd_soc_platform_driver mtk_soc_platform = {

	.ops = &mtk_hp_impedance_ops,
	.pcm_new = mtk_asoc_pcm_hp_impedance_new,
	.probe = mtk_asoc_dhp_impedance_probe,
};

static int mtk_soc_hp_impedance_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_HP_IMPEDANCE_PCM);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev, &mtk_soc_platform);
}

static int mtk_asoc_pcm_hp_impedance_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	return ret;
}


static int mtk_asoc_dhp_impedance_probe(struct snd_soc_platform *platform)
{
	pr_debug("mtk_asoc_dhp_impedance_probe\n");
	/* add  controls */
	snd_soc_add_platform_controls(platform, Audio_snd_hp_impedance_controls,
		ARRAY_SIZE(Audio_snd_hp_impedance_controls));
	/* allocate dram */
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL1,
		Dl1_MAX_BUFFER_SIZE);
	Dl1_Playback_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);
	return 0;
}

static int mtk_hp_impedance_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id Mt_soc_pcm_hp_impedance_of_ids[] = {

	{.compatible = "mediatek,mt8163-soc-pcm-hp-impedance",},
	{}
};
#endif

static struct platform_driver mtk_hp_impedance_driver = {

	.driver = {
		   .name = MT_SOC_HP_IMPEDANCE_PCM,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = Mt_soc_pcm_hp_impedance_of_ids,
#endif
		   },
	.probe = mtk_soc_hp_impedance_probe,
	.remove = mtk_hp_impedance_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_hp_impedance_dev;
#endif

static int __init mtk_soc_hp_impedance_platform_init(void)
{
	int ret;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtk_hp_impedance_dev =
		platform_device_alloc(MT_SOC_HP_IMPEDANCE_PCM, -1);

	if (!soc_mtk_hp_impedance_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_hp_impedance_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_hp_impedance_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_hp_impedance_driver);
	return ret;

}
module_init(mtk_soc_hp_impedance_platform_init);

static void __exit mtk_soc_hp_impedance_platform_exit(void)
{
	pr_debug("%s\n", __func__);

	platform_driver_unregister(&mtk_hp_impedance_driver);
}
module_exit(mtk_soc_hp_impedance_platform_exit);

MODULE_DESCRIPTION("hp impedance module platform driver");
MODULE_LICENSE("GPL");
