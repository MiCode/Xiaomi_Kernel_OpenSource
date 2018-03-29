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
 *   mt_soc_pcm_hp_impedance.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio dl1 impedance setting
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

#include "AudDrv_Common.h"
#include "AudDrv_Common_func.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"
#include "mt_soc_pcm_platform.h"
#include "mt_soc_codec_63xx.h"

#include <linux/time.h>
#include <linux/dma-mapping.h>

/* #define SUPPORT_GOOGLE_LINEOUT */

static AFE_MEM_CONTROL_T *pHp_impedance_MemControl;
static const int DCoffsetDefault = 1460;	/* w/o 33 ohm */
static const int DCoffsetDefault_33Ohm = 1620;	/* w/ 33 ohm */

static const int DCoffsetVariance = 200;    /* denali 0.2v */
static const unsigned short HpImpedanceAuxCable = 10000;

static const int mDcRangestep = 7;
static const int HpImpedancePhase1Step = 100;
static const int HpImpedancePhase2Step = 100;
static const int HpImpedancePhase0AdcValue = 200;
static const int HpImpedancePhase1AdcValue = 2000;
static const int HpImpedancePhase2AdcValue = 8800;
static struct snd_dma_buffer *Dl1_Hp_Playback_dma_buf;
static int EfuseCurrentCalibration;

#define AUXADC_BIT_RESOLUTION (1 << 12)
#define AUXADC_VOLTAGE_RANGE 1800

/*
 *    function implementation
 */

static int mtk_soc_hp_impedance_probe(struct platform_device *pdev);
static int mtk_soc_pcm_hp_impedance_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_hp_impedance_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_asoc_dhp_impedance_probe(struct snd_soc_platform *platform);

static struct snd_pcm_hardware mtk_pcm_hp_impedance_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
	SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_RESUME |
	SNDRV_PCM_INFO_MMAP_VALID),
	.formats =      SND_SOC_ADV_MT_FMTS,
	.rates =           SOC_HIGH_USE_RATE,
	.rate_min =     SOC_HIGH_USE_RATE_MIN,
	.rate_max =     SOC_HIGH_USE_RATE_MAX,
	.channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min =      SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max =    SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size =        0,
};

#if 0 /* not used */
static int mtk_pcm_hp_impedance_stop(struct snd_pcm_substream *substream)
{
	PRINTK_AUDDRV("mtk_pcm_hp_impedance_stop\n");
	return 0;
}

static int mtk_pcm_hp_impedance_start(struct snd_pcm_substream *substream)
{
	return 0;
}
#endif

static snd_pcm_uframes_t mtk_pcm_hp_impedance_pointer(struct snd_pcm_substream
						      *substream)
{
	return 0;
}

static void SetDL1Buffer(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	AFE_BLOCK_T *pblock = &pHp_impedance_MemControl->rBlock;

	pblock->pucPhysBufAddr =  runtime->dma_addr;
	pblock->pucVirtBufAddr =  runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;  /* 32 byte align */
	pblock->u4WriteIdx     = 0;
	pblock->u4DMAReadIdx    = 0;
	pblock->u4DataRemained  = 0;
	pblock->u4fsyncflag     = false;
	pblock->uResetFlag      = true;
	pr_aud("SetDL1Buffer u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
	       pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
	/* set dram address top hardware */
	Afe_Set_Reg(AFE_DL1_BASE , pblock->pucPhysBufAddr , 0xffffffff);
	Afe_Set_Reg(AFE_DL1_END  , pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1),
		    0xffffffff);
	memset_io((void *)pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);

}


static int mtk_pcm_hp_impedance_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	/* runtime->dma_bytes has to be set manually to allow mmap */
	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);

	substream->runtime->dma_area = Dl1_Hp_Playback_dma_buf->area;
	substream->runtime->dma_addr = Dl1_Hp_Playback_dma_buf->addr;
	SetHighAddr(Soc_Aud_Digital_Block_MEM_DL1, true, substream->runtime->dma_addr);
	SetDL1Buffer(substream, hw_params);

	pr_aud("mtk_pcm_hp_impedance_params dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
	       substream->runtime->dma_bytes, substream->runtime->dma_area,
	       (long)substream->runtime->dma_addr);
	return ret;
}

static int mtk_pcm_hp_impedance_hw_free(struct snd_pcm_substream *substream)
{
	PRINTK_AUDDRV("mtk_pcm_hp_impedance_hw_free\n");
	return 0;
}


static struct snd_pcm_hw_constraint_list constraints_hp_impedance_sample_rates
	= {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_hp_impedance_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_aud("mtk_pcm_hp_impedance_open\n");
	AudDrv_Clk_On();
	AudDrv_Emi_Clk_On();
	pHp_impedance_MemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
	runtime->hw = mtk_pcm_hp_impedance_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hp_impedance_hardware ,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_hp_impedance_sample_rates);

	if (ret < 0)
		pr_err("snd_pcm_hw_constraint_integer failed\n");

	if (ret < 0) {
		pr_err("mtk_soc_pcm_hp_impedance_close\n");
		mtk_soc_pcm_hp_impedance_close(substream);
		return ret;
	}
	return 0;
}


bool mPrepareDone = false;
static int mtk_pcm_hp_impedance_prepare(struct snd_pcm_substream *substream)
{
	bool mI2SWLen;
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_aud("mtk_pcm_hp_impedance_prepare, mPrepareDone %d\n", mPrepareDone);
	if (mPrepareDone == false) {
		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
						     AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					Soc_Aud_AFE_IO_Block_I2S1_DAC);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					Soc_Aud_AFE_IO_Block_I2S1_DAC);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
		}
		SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S,  runtime->rate);
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
			SetI2SDacOut(substream->runtime->rate, false, mI2SWLen);
			SetI2SDacEnable(true);
		} else
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);

		SetIntfConnection(Soc_Aud_InterCon_Connection,
				Soc_Aud_AFE_IO_Block_MEM_DL1, Soc_Aud_AFE_IO_Block_I2S1_DAC);

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
	pr_aud("%s\n", __func__);
	if (mPrepareDone == true) {
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);

		SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);
		EnableAfe(false);
		mPrepareDone = false;
	}
	AudDrv_Emi_Clk_Off();
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_pcm_hp_impedance_trigger(struct snd_pcm_substream *substream,
					int cmd)
{
	pr_aud("mtk_pcm_hp_impedance_trigger cmd = %d\n", cmd);
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
					int channel, snd_pcm_uframes_t pos,
					snd_pcm_uframes_t count)
{
	PRINTK_AUDDRV("%s\n", __func__);
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_pcm_hp_impedance_page(struct snd_pcm_substream
					      *substream,
					      unsigned long offset)
{
	PRINTK_AUDDRV("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}


static unsigned short mhp_impedance;
#ifndef CONFIG_FPGA_EARLY_PORTING
static unsigned short mAuxAdc_Offset;
#endif
static int Audio_HP_ImpeDance_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	const int off_counter = 20;

	pr_warn("%s\n", __func__);
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
		/*msleep(5);*/
		usleep_range(5*1000, 20*1000);
		mAuxAdc_Offset = (PMIC_IMM_GetOneChannelValue(PMIC_AUX_CH9,
							     off_counter,
							     0) *
				 AUXADC_VOLTAGE_RANGE) /
				 AUXADC_BIT_RESOLUTION;

		pr_warn("mAuxAdc_Offset= %d\n", mAuxAdc_Offset);

		memset((void *)Get_Afe_SramBase_Pointer(),
		       ucontrol->value.integer.value[0],
		       AFE_INTERNAL_SRAM_SIZE);
		msleep(5 * 1000);
		pr_warn("4 %s\n", __func__);

		EnableTrimbuffer(false);
		OpenAnalogTrimHardware(false);
	}

	if (mhp_impedance  == 1) {
		pr_warn("start open hp impedance setting\n");
		OpenHeadPhoneImpedanceSetting(true);
		setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPR);
		setOffsetTrimBufferGain(3);
		EnableTrimbuffer(true);
		setHpGainZero();
		memset((void *)Get_Afe_SramBase_Pointer(), ucontrol->value.integer.value[0],
		       AFE_INTERNAL_SRAM_SIZE);
		msleep(5 * 1000);
	} else if (mhp_impedance == 0) {
		pr_warn("stop hp impedance setting\n");
		OpenHeadPhoneImpedanceSetting(false);
		setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
		EnableTrimbuffer(false);
		SetSdmLevel(AUDIO_SDM_LEVEL_NORMAL);
	} else {
		unsigned short  value = 0;

		for (value = 0; value < 10000; value += 200) {
			unsigned short temp = value;
			static unsigned short dcoffset;
			volatile unsigned short *Sramdata;
			int i = 0;

			pr_warn("set sram to dc value = %d\n", temp);
			Sramdata = Get_Afe_SramBase_Pointer();
			for (i = 0; i < AFE_INTERNAL_SRAM_SIZE >> 1; i++) {
				*Sramdata = temp;
				Sramdata++;
			}
			Sramdata = Get_Afe_SramBase_Pointer();
			pr_debug("Sramdata = %p\n", Sramdata);
			pr_debug("Sramdata = 0x%x Sramdata+1 = 0x%x Sramdata+2 = 0x%x Sramdata+3 = 0x%x\n",
			       *Sramdata, *(Sramdata + 1), *(Sramdata + 2), *(Sramdata + 3));
			Sramdata += 4;
			pr_debug("Sramdata = %p\n", Sramdata);
			pr_debug("Sramdata = 0x%x Sramdata+1 = 0x%x Sramdata+2 = 0x%x Sramdata+3 = 0x%x\n",
			       *Sramdata, *(Sramdata + 1), *(Sramdata + 2), *(Sramdata + 3));
			/* memset((void *)Get_Afe_SramBase_Pointer(),
			ucontrol->value.integer.value[0], AFE_INTERNAL_SRAM_SIZE); */
			msleep(20);
			dcoffset = 0;
			dcoffset = (PMIC_IMM_GetOneChannelValue(PMIC_AUX_CH9,
								off_counter,
								0) *
				   AUXADC_VOLTAGE_RANGE) /
				   AUXADC_BIT_RESOLUTION;
			pr_warn("dcoffset= %d\n", dcoffset);
			msleep(3 * 1000);
		}
	}

	AudDrv_Clk_Off();
#endif
	return 0;
}

#ifndef CONFIG_FPGA_EARLY_PORTING
static void FillDatatoDlmemory(volatile unsigned int *memorypointer,
			       unsigned int fillsize, short value)
{
	int addr  = 0;
	unsigned int tempvalue = value;

	tempvalue = tempvalue << 16;
	tempvalue |= value;
	/* set memory to DC value */
	for (addr = 0; addr < (fillsize >> 2); addr++) {
		*memorypointer = tempvalue;
		memorypointer++;
	}
}

static unsigned short Calculate_HP_Impedance(unsigned short dcinit,
		unsigned short dcinput, unsigned short pcmoffset)
{
	unsigned short R_hp;
	unsigned int dcvalue;
	unsigned int R_tmp = 0;

	if (dcinput < dcinit)
		return 0;

	dcvalue = (unsigned int)(dcinput - dcinit);		/* S32.3 = S32.2 - S32.2 */

	if (pcmoffset == HpImpedancePhase0AdcValue) {
		R_tmp = (dcvalue * 2417 + 256) >> 9;		/* 200 (S32.0) */

		if (R_tmp < 650) {
			pr_debug("%s Phase%d detected resistor is %d, smaller than 650, Goto Phase%d\n",
				__func__, HpImpedancePhase0AdcValue, R_tmp, HpImpedancePhase1AdcValue);
			R_tmp = 0;
		}
	} else if (pcmoffset == HpImpedancePhase1AdcValue) {
		R_tmp = (dcvalue * 967 + 1024) >> 11;		/* 2000 (S32.0) */

		if (R_tmp < 150) {
			pr_debug("%s Phase%d detected resistor is %d, smaller than 150, Goto Phase%d\n",
				__func__, HpImpedancePhase1AdcValue, R_tmp, HpImpedancePhase2AdcValue);
			R_tmp = 0;
		}
	} else if (pcmoffset == HpImpedancePhase2AdcValue) {
		R_tmp = (dcvalue * 879 + 4096) >> 13;		/* 8800(S32.0) */
	}

	/* Efuse calibration */
	if ((EfuseCurrentCalibration != 0) && (R_tmp != 0)) {
		/* pr_debug("%s Before Calibration from EFUSE: %d, R: %d\n",
			__func__, EfuseCurrentCalibration, R_tmp); */
		R_tmp = R_tmp * 128 / (128 + EfuseCurrentCalibration);
		pr_debug("%s After Calibration from EFUSE: %d, R: %d\n",
			__func__, EfuseCurrentCalibration, R_tmp);
	}

	R_hp = (unsigned short)R_tmp;
	pr_debug("%s pcmoffset %d dcoffset %d detected resistor is %d\n",
		__func__, pcmoffset, dcvalue, R_hp);

	return R_hp;
}
#endif

static void ApplyDctoDl(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING

	unsigned int i;
	unsigned short dcoffset = 0, average = 0;
	unsigned short ibuffer_v[4];
	short value = 0;
#ifdef SUPPORT_GOOGLE_LINEOUT
	int ret_value;
#endif

	/* pr_debug("%s\n", __func__); */

	for (value = 0; value < (HpImpedancePhase2AdcValue + HpImpedancePhase1Step);
		value += HpImpedancePhase1Step) {
		volatile unsigned int *Sramdata = (unsigned int *)(Dl1_Hp_Playback_dma_buf->area);

		if (value > HpImpedancePhase2AdcValue)
			value = HpImpedancePhase2AdcValue;

		/* apply to dram */
		FillDatatoDlmemory(Sramdata , Dl1_Hp_Playback_dma_buf->bytes , value);

		/* save for DC =0 offset */
		if (value == 0) {
			usleep_range(1*1000, 2*1000);

			/* get adc value */
			dcoffset = 0;
			for (i = 0; i < 4; i++) {
				ibuffer_v[i] = PMIC_IMM_GetOneChannelValue(PMIC_AUX_CH9, 5, 0);
				dcoffset = dcoffset + ibuffer_v[i];
			}

			pr_debug("[DCinit] SUM = %d 1st = %d 2nd = %d 3rd= %d 4th = %d\n",
				dcoffset, ibuffer_v[0], ibuffer_v[1], ibuffer_v[2], ibuffer_v[3]);
		}

		/* start checking */
		if (value == HpImpedancePhase0AdcValue) {
			usleep_range(1*1000, 1*2000);

			/* get adc value */
			average = 0;
			for (i = 0; i < 4; i++) {
				ibuffer_v[i] = PMIC_IMM_GetOneChannelValue(PMIC_AUX_CH9, 5, 0);
				average = average + ibuffer_v[i];
			}

			if ((average >> 2) > 4050)
				mhp_impedance = HpImpedanceAuxCable;
			else
				mhp_impedance = Calculate_HP_Impedance(dcoffset, average, value);

			if (mhp_impedance) {
				pr_debug("[phase %d] SUM = %d 1st = %d 2nd = %d 3rd = %d 4th = %d\n",
					value, average, ibuffer_v[0], ibuffer_v[1], ibuffer_v[2], ibuffer_v[3]);
				pr_debug("[phase %d]average = %d dcinit_value = %d mhp_impedance = %d\n ",
					value, average, dcoffset, mhp_impedance);
				break;
			}
		}

		if (value == HpImpedancePhase1AdcValue || value == HpImpedancePhase2AdcValue) {
			usleep_range(1*1000, 2*1000);

			/* get adc value */
			average = 0;
			for (i = 0; i < 4; i++) {
				ibuffer_v[i] = PMIC_IMM_GetOneChannelValue(PMIC_AUX_CH9, 5, 0);
				average = average + ibuffer_v[i];
			}

			mhp_impedance = Calculate_HP_Impedance(dcoffset, average, value);

			if (mhp_impedance) {
				pr_debug("[phase %d] SUM = %d 1st = %d 2nd = %d 3rd = %d 4th = %d\n",
					value, average, ibuffer_v[0], ibuffer_v[1], ibuffer_v[2], ibuffer_v[3]);
				pr_debug("[phase %d]average = %d dcinit_value = %d mhp_impedance = %d\n ",
					value, average, dcoffset, mhp_impedance);
				break;
			}
		}

		usleep_range(1*250, 1*500);
	}

#ifdef SUPPORT_GOOGLE_LINEOUT
	/* return detected value to ACCDET */
	ret_value = accdet_read_audio_res((unsigned int)mhp_impedance);
#endif

	/* Ramp-Down */
	while (value > 0) {
		volatile unsigned int *Sramdata = (unsigned int *)(Dl1_Hp_Playback_dma_buf->area);

		value = value - HpImpedancePhase1Step;
		/* apply to dram */
		FillDatatoDlmemory(Sramdata , Dl1_Hp_Playback_dma_buf->bytes , value);

		usleep_range(1*200, 1*400);
	}
#endif
}

static int Audio_HP_ImpeDance_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("+ %s()\n", __func__);
	AudDrv_Clk_On();
	if (OpenHeadPhoneImpedanceSetting(true) == true) {
		setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPR);
		/* setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND); */

		setOffsetTrimBufferGain(3);

		EnableTrimbuffer(true);
		setHpGainZero();
		ApplyDctoDl();
		SetSdmLevel(AUDIO_SDM_LEVEL_MUTE);
		/* usleep_range(0.5*1000, 1*1000); */
		OpenHeadPhoneImpedanceSetting(false);
		setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
		EnableTrimbuffer(false);
		SetSdmLevel(AUDIO_SDM_LEVEL_NORMAL);
	} else
		pr_warn("Audio_HP_ImpeDance_Get just do nothing\n");
	AudDrv_Clk_Off();
	ucontrol->value.integer.value[0] = mhp_impedance;
	pr_aud("- %s()\n", __func__);
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_hp_impedance_controls[] = {
	SOC_SINGLE_EXT("Audio HP ImpeDance Setting", SND_SOC_NOPM,
		0, 65536, 0, Audio_HP_ImpeDance_Get, Audio_HP_ImpeDance_Set),
};


static struct snd_pcm_ops mtk_hp_impedance_ops = {
	.open =     mtk_pcm_hp_impedance_open,
	.close =    mtk_soc_pcm_hp_impedance_close,
	.ioctl =    snd_pcm_lib_ioctl,
	.hw_params =    mtk_pcm_hp_impedance_params,
	.hw_free =  mtk_pcm_hp_impedance_hw_free,
	.prepare =  mtk_pcm_hp_impedance_prepare,
	.trigger =  mtk_pcm_hp_impedance_trigger,
	.pointer =  mtk_pcm_hp_impedance_pointer,
	.copy =     mtk_pcm_hp_impedance_copy,
	.silence =  mtk_pcm_hp_impedance_silence,
	.page =     mtk_pcm_hp_impedance_page,
};

static struct snd_soc_platform_driver mtk_soc_platform = {
	.ops        = &mtk_hp_impedance_ops,
	.pcm_new    = mtk_asoc_pcm_hp_impedance_new,
	.probe      = mtk_asoc_dhp_impedance_probe,
};

static int mtk_soc_hp_impedance_probe(struct platform_device *pdev)
{
	PRINTK_AUDDRV("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_HP_IMPEDANCE_PCM);
	PRINTK_AUDDRV("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
					 &mtk_soc_platform);
}

static int mtk_asoc_pcm_hp_impedance_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	PRINTK_AUDDRV("%s\n", __func__);
	return ret;
}


static int mtk_asoc_dhp_impedance_probe(struct snd_soc_platform *platform)
{
	PRINTK_AUDDRV("mtk_asoc_dhp_impedance_probe\n");
	/* add  controls */
	snd_soc_add_platform_controls(platform, Audio_snd_hp_impedance_controls,
				      ARRAY_SIZE(Audio_snd_hp_impedance_controls));
	/* allocate dram */
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL1,
				   Dl1_MAX_BUFFER_SIZE);
	Dl1_Hp_Playback_dma_buf =  Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);

	/* Read calibration from Efuse */
	EfuseCurrentCalibration = Audio_Read_Efuse_HP_Impedance_Current_Calibration();

	return 0;
}

static int mtk_hp_impedance_remove(struct platform_device *pdev)
{
	PRINTK_AUDDRV("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id Mt_soc_pcm_hp_impedance_of_ids[] = {
	{ .compatible = "mediatek,mt_soc_pcm_hp_impedance", },
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

	PRINTK_AUDDRV("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtk_hp_impedance_dev = platform_device_alloc(MT_SOC_HP_IMPEDANCE_PCM, -1);
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
	PRINTK_AUDDRV("%s\n", __func__);

	platform_driver_unregister(&mtk_hp_impedance_driver);
}
module_exit(mtk_soc_hp_impedance_platform_exit);

MODULE_DESCRIPTION("hp impedance module platform driver");
MODULE_LICENSE("GPL");
