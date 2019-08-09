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

#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#ifdef CONFIG_MTK_LEGACY
#include <mt-plat/mt_gpio.h>
#endif

#if 0				/* no this */
#include <mach/mt_clkbuf_ctl.h>
#include <mach/mt_chip.h>
#endif

#include <sound/mt_soc_audio.h>
#include "mt_soc_afe_control.h"
#include <mach/upmu_hw.h>

#include "mt_soc_pcm_common.h"
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "mt_soc_analog_type.h"
#include "AudDrv_Gpio.h"

#ifdef CONFIG_MTK_SPEAKER
#include "mt_soc_codec_speaker_63xx.h"
#endif

#define AW8736_MODE_CTRL	/* AW8736 PA output power mode control */

/* static function declaration */
static void Apply_Speaker_Gain(void);
static void TurnOffDacPower(void);
static void TurnOnDacPower(void);
#if 0				/* not used */
static void SetDcCompenSation(void);
#endif
static void Voice_Amp_Change(bool enable);
static void Speaker_Amp_Change(bool enable);

static struct mt6323_Codec_Data_Priv *mCodec_data;
static uint32 mBlockSampleRate[AUDIO_ANALOG_DEVICE_INOUT_MAX] = {
	48000, 48000, 48000 };

static DEFINE_MUTEX(Ana_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_buf_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_Clk_Mutex);
static DEFINE_MUTEX(Ana_Power_Mutex);
static DEFINE_MUTEX(AudAna_lock);

static int mAudio_Analog_Mic1_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic2_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic3_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic4_mode = AUDIO_ANALOGUL_MODE_ACC;

static const int DC1unit_in_uv = 19184;	/* in uv with 0DB */
static const int DC1devider = 8;	/* in uv */
/* Headphone DC calibration */
static int mHpLeftDcCalibration;
static int mHpRightDcCalibration;

#if 0 /* currently not used */
static uint32 RG_AUDHPLTRIM_VAUDP15, RG_AUDHPRTRIM_VAUDP15,
	RG_AUDHPLFINETRIM_VAUDP15, RG_AUDHPRFINETRIM_VAUDP15,
	RG_AUDHPLTRIM_VAUDP15_SPKHP, RG_AUDHPRTRIM_VAUDP15_SPKHP,
	RG_AUDHPLFINETRIM_VAUDP15_SPKHP, RG_AUDHPRFINETRIM_VAUDP15_SPKHP;
#endif

#ifdef CONFIG_MTK_SPEAKER
static int Speaker_mode = AUDIO_SPEAKER_MODE_D;	/* default use type D */
static unsigned int Speaker_pga_gain = 1;	/* default 0Db. */
static bool mSpeaker_Ocflag;
#endif

static const int mDcOffsetTrimChannel = 9;
static bool mInitCodec;

static void Audio_Amp_Change(int channels, bool enable);
static void SavePowerState(void)
{
	int i = 0;

	for (i = 0; i < AUDIO_ANALOG_DEVICE_MAX; i++)
		mCodec_data->mAudio_BackUpAna_DevicePower[i] =
		    mCodec_data->mAudio_Ana_DevicePower[i];
}

static void RestorePowerState(void)
{
	int i = 0;

	for (i = 0; i < AUDIO_ANALOG_DEVICE_MAX; i++)
		mCodec_data->mAudio_Ana_DevicePower[i] =
		    mCodec_data->mAudio_BackUpAna_DevicePower[i];
}

static bool GetDLStatus(void)
{
	int i = 0;

	for (i = 0; i < AUDIO_ANALOG_DEVICE_2IN1_SPK; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}

static bool GetULStatus(void)
{
	int i = 0;

	for (i = AUDIO_ANALOG_DEVICE_IN_ADC1;
		i <= AUDIO_ANALOG_DEVICE_IN_DIGITAL_MIC; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}


static bool mAnaSuspend;
void SetAnalogSuspend(bool bEnable)
{
	pr_debug("%s bEnable ==%d mAnaSuspend = %d\n",
		__func__, bEnable, mAnaSuspend);
	if ((bEnable == true) && (mAnaSuspend == false)) {
		Ana_Log_Print();
		SavePowerState();
		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
			== true) {
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = false;
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
		== true) {
		mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = false;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_EARPIECEL]
			== true) {
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] = false;
			Voice_Amp_Change(false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] == true) {
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] = false;
			Speaker_Amp_Change(false);
		}
		Ana_Log_Print();
		mAnaSuspend = true;
	} else if ((bEnable == false) && (mAnaSuspend == true)) {
		Ana_Log_Print();
		if (mCodec_data->mAudio_BackUpAna_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
			== true) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = true;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
			== true) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = false;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_EARPIECEL]
			== true) {
			Voice_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] = false;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_SPEAKERL]
			== true) {
			Speaker_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] = false;
		}
		RestorePowerState();
		Ana_Log_Print();
		mAnaSuspend = false;
	}
}

static int audck_buf_Count;
void audckbufEnable(bool enable)
{
	pr_debug("audckbufEnable audck_buf_Count = %d enable = %d, no need\n",
		 audck_buf_Count, enable);
#if 0
	mutex_lock(&Ana_buf_Ctrl_Mutex);
	if (enable) {
		if (audck_buf_Count == 0) {
			pr_debug("+clk_buf_ctrl(CLK_BUF_AUDIO,true)\n");
			clk_buf_ctrl(CLK_BUF_AUDIO, true);
			pr_debug("-clk_buf_ctrl(CLK_BUF_AUDIO,true)\n");
		}
		audck_buf_Count++;
	} else {
		audck_buf_Count--;
		if (audck_buf_Count == 0) {
			pr_debug("+clk_buf_ctrl(CLK_BUF_AUDIO,false)\n");
			clk_buf_ctrl(CLK_BUF_AUDIO, false);
			pr_debug("-clk_buf_ctrl(CLK_BUF_AUDIO,false)\n");
		}
		if (audck_buf_Count < 0) {
			pr_debug("audck_buf_Count count < 0\n");
			audck_buf_Count = 0;
		}
	}
	mutex_unlock(&Ana_buf_Ctrl_Mutex);
#endif
}

/* static int ClsqCount; */
static void ClsqEnable(bool enable)
{
	pr_debug("ClsqEnable not support\n");
}

static int TopCkCount;
static void Topck_Enable(bool enable)
{
	pr_debug("Topck_Enable enable = %d TopCkCount = %d\n",
		enable, TopCkCount);
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (TopCkCount == 0) {
			/* AUD 26M clock power down release */
			Ana_Set_Reg(TOP_CKPDN1_CLR, 0x0100, 0x0100);
		}
		TopCkCount++;
	} else {
		TopCkCount--;
		if (TopCkCount == 0) {
			/* AUD 26M clock power down */
			Ana_Set_Reg(TOP_CKPDN1_SET, 0x0100, 0x0100);
		}
		if (TopCkCount <= 0) {
			pr_debug("TopCkCount <0 =%d\n ", TopCkCount);
			TopCkCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

/* static int NvRegCount; */
static void NvregEnable(bool enable)
{
	pr_debug("NvregEnable not support\n");
}

#if 0				/* not used */
static int VOW12MCKCount;
static void VOW12MCK_Enable(bool enable)
{
	pr_debug("VOW12MCK_Enable VOW12MCKCount == %d enable = %d\n",
	VOW12MCKCount, enable);
	pr_debug("VOW12MCK_Enable not support this\n");
}
#endif

static void TopCtlChangeTrigger(void)
{
	uint32 top_ctrl_status_now = Ana_Get_Reg(ABB_AFE_CON11);

	Ana_Set_Reg(ABB_AFE_CON11,
		((top_ctrl_status_now & 0x0001) ? 0 : 1) << 8, 0x0100);
}

static void DCChangeTrigger(void)
{
	uint32_t dc_status_now = Ana_Get_Reg(0x4016);

	Ana_Set_Reg(0x4016, ((dc_status_now & 0x0002) ? 0 : 1) << 9, 0x0200);
}

/* extern kal_uint32 upmu_get_reg_value(kal_uint32 reg); */

void Auddrv_Read_Efuse_HPOffset(void)
{
	pr_debug("Auddrv_Read_Efuse_HPOffset not support\n");
}
EXPORT_SYMBOL(Auddrv_Read_Efuse_HPOffset);

#ifdef CONFIG_MTK_SPEAKER
static void Apply_Speaker_Gain(void)
{
	int index = Speaker_pga_gain;

	pr_debug("%s Speaker_pga_gain= %d\n", __func__, Speaker_pga_gain);

	if (index > 11)
		index = 11;

	if (index < 1)
		index = 1;	/* min to 0dB */

	pr_debug("%s(), index = %d\n", __func__, index);
	Ana_Set_Reg(SPK_CON9, index << 8, 0x00000f00);
}
#else
static void Apply_Speaker_Gain(void)
{
}
#endif

void setHpDcCalibration(unsigned int type, int dc_cali_value)
{
	pr_debug("%s ,type=%d, value = %d\n", __func__, type, dc_cali_value);
	if (type == AUDIO_ANALOG_DEVICE_OUT_HEADSETR)
		mHpRightDcCalibration = dc_cali_value;
	else if (type == AUDIO_ANALOG_DEVICE_OUT_HEADSETL)
		mHpLeftDcCalibration = dc_cali_value;
	else
		pr_debug("%s, wrong type, invalid operation\n", __func__);
}

void setOffsetTrimMux(unsigned int Mux)
{
#if 0				/* Todo */
	pr_debug("%s Mux = %d\n", __func__, Mux);
	/* Audio offset trimming buffer mux selection */
	Ana_Set_Reg(AUDDEC_ANA_CON3, Mux << 1, 0xf << 1);
#endif
}

void setOffsetTrimBufferGain(unsigned int gain)
{
#if 0				/* Todo */
	/* Audio offset trimming buffer gain selection */
	Ana_Set_Reg(AUDDEC_ANA_CON3, gain << 5, 0x3 << 5);
#endif
}

/* unused variable */
/* static int mHplTrimOffset = 2048; */
/* static int mHprTrimOffset = 2048; */


void SetHplTrimOffset(int Offset)
{
	pr_debug("%s Offset = %d\n", __func__, Offset);
	setHpDcCalibration(AUDIO_ANALOG_DEVICE_OUT_HEADSETL,
		(Offset * 18) / 10);
}

void SetHprTrimOffset(int Offset)
{
	pr_debug("%s Offset = %d\n", __func__, Offset);
	setHpDcCalibration(AUDIO_ANALOG_DEVICE_OUT_HEADSETR,
		(Offset * 18) / 10);
}

void EnableTrimbuffer(bool benable)
{
	pr_debug("%s , no this\n", __func__);
}


void OpenTrimBufferHardware(bool enable)
{
	pr_debug("%s , no this\n", __func__);
}

void OpenAnalogTrimHardware(bool enable)
{
	pr_debug("%s , no this\n", __func__);
}

void setHpDcCalibrationGain(unsigned int type, int gain_value)
{
	/* this will base on hw spec. */
	uint32 index = 7;

	pr_debug("%s ,type=%d, gain_value = %d\n",
		__func__, type, gain_value);
	/* const int HWgain[] = {-5, -3, -1, 1, 3, 5, 7, 9}; */
	gain_value = gain_value / 2;
	if (gain_value > index)
		gain_value = index;
	index -= gain_value;
	if (type == AUDIO_ANALOG_DEVICE_OUT_HEADSETR)
		Ana_Set_Reg(AUDTOP_CON5, index << 8, 0x000000700);
	else if (type == AUDIO_ANALOG_DEVICE_OUT_HEADSETL)
		Ana_Set_Reg(AUDTOP_CON5, index << 12, 0x00007000);
	else
		pr_debug("%s, wrong type, invalid operation\n", __func__);
}


void OpenAnalogHeadphone(bool bEnable)
{
	pr_debug("OpenAnalogHeadphone bEnable = %d\n", bEnable);
	if (bEnable) {
		/* SetHplTrimOffset(2048); */
		/* SetHprTrimOffset(2048); */
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = 44100;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
		= true;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
		= true;
	} else {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
		= false;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
		= false;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
	}
}

bool OpenHeadPhoneImpedanceSetting(bool bEnable)
{
	pr_debug("%s not support, bEnable = %d\n", __func__, bEnable);
	return true;
}

void setHpGainZero(void)
{
	pr_debug("%s not support\n", __func__);
}

void SetSdmLevel(unsigned int level)
{
#if 0				/* Todo */
	Ana_Set_Reg(AFE_DL_SDM_CON1, level, 0xffffffff);
#endif
}

#if 0				/* currently not used */
static void SetHprOffset(int OffsetTrimming)
{
	short Dccompsentation = 0;
	int DCoffsetValue = 0;
	unsigned short RegValue = 0;

	pr_debug("%s OffsetTrimming = %d\n", __func__, OffsetTrimming);
	DCoffsetValue = OffsetTrimming * 1000000;
	DCoffsetValue = (DCoffsetValue / DC1devider);	/* in uv */
	pr_debug("%s DCoffsetValue = %d\n", __func__, DCoffsetValue);
	DCoffsetValue = (DCoffsetValue / DC1unit_in_uv);
	pr_debug("%s DCoffsetValue = %d\n", __func__, DCoffsetValue);
	Dccompsentation = DCoffsetValue;
	RegValue = Dccompsentation;
	pr_debug("%s RegValue = 0x%x\n", __func__, RegValue);
#if 0				/* Todo Sam */
	Ana_Set_Reg(AFE_DL_DC_COMP_CFG1, RegValue, 0xffff);
#endif
}

static void SetHplOffset(int OffsetTrimming)
{
	short Dccompsentation = 0;
	int DCoffsetValue = 0;
	unsigned short RegValue = 0;

	pr_debug("%s OffsetTrimming = %d\n", __func__, OffsetTrimming);
	DCoffsetValue = OffsetTrimming * 1000000;
	DCoffsetValue = (DCoffsetValue / DC1devider);	/* in uv */
	pr_debug("%s DCoffsetValue = %d\n", __func__, DCoffsetValue);
	DCoffsetValue = (DCoffsetValue / DC1unit_in_uv);
	pr_debug("%s DCoffsetValue = %d\n", __func__, DCoffsetValue);
	Dccompsentation = DCoffsetValue;
	RegValue = Dccompsentation;
	pr_debug("%s RegValue = 0x%x\n", __func__, RegValue);
#if 0				/* Todo Sam */
	Ana_Set_Reg(AFE_DL_DC_COMP_CFG0, RegValue, 0xffff);
#endif
}

static void SetHprOffsetTrim(void)
{
	int OffsetTrimming = mHprTrimOffset - TrimOffset;

	pr_debug("%s mHprTrimOffset = %d TrimOffset = %d\n",
		__func__, mHprTrimOffset, TrimOffset);
	SetHprOffset(OffsetTrimming);
}

static void SetHpLOffsetTrim(void)
{
	int OffsetTrimming = mHplTrimOffset - TrimOffset;

	pr_debug("%s mHprTrimOffset = %d TrimOffset = %d\n",
		__func__, mHplTrimOffset, TrimOffset);
	SetHplOffset(OffsetTrimming);
}
#endif

uint32 GetULFrequency(uint32 frequency)
{
	uint32 Reg_value = 0;

	pr_debug("%s frequency =%d\n", __func__, frequency);
	switch (frequency) {
	case 8000:
	case 16000:
	case 32000:
		Reg_value = 0x0;
		break;
	case 48000:
		Reg_value = 0x1;
	default:
		break;
	}
	return Reg_value;
}

uint32 GetDLFrequency(uint32 frequency)
{
	uint32 Reg_value = 0;

	pr_debug("%s frequency = %d\n", __func__, frequency);
	switch (frequency) {
	case 8000:
		Reg_value = 0;
		break;
	case 11025:
		Reg_value = 1;
		break;
	case 12000:
		Reg_value = 2;
		break;
	case 16000:
		Reg_value = 4;
		break;
	case 22050:
		Reg_value = 5;
		break;
	case 24000:
		Reg_value = 6;
		break;
	case 32000:
		Reg_value = 8;
		break;
	case 44100:
		Reg_value = 9;
		break;
	case 48000:
		Reg_value = 10;
	default:
		pr_debug("GetDLFrequency with frequency = %d", frequency);
	}
	return Reg_value;
}


uint32 ULSampleRateTransform(uint32 SampleRate)
{
	switch (SampleRate) {
	case 8000:
		return 0;
	case 16000:
		return 1;
	case 32000:
		return 2;
	case 48000:
		return 3;
	default:
		break;
	}
	return 0;
}


static int mt63xx_codec_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *Daiport)
{
	pr_debug("+%s name = %s number = %d\n",
		__func__, substream->name,
		substream->number);
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
		substream->runtime->rate) {
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] =
			substream->runtime->rate;
	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK &&
			substream->runtime->rate) {
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] =
			substream->runtime->rate;
	}
	/* pr_debug("-%s name = %s number = %d\n", */
	/* __func__, substream->name, substream->number); */
	return 0;
}

static int mt63xx_codec_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *Daiport)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_debug("mt63xx_codec_prepare set up SNDRV_PCM_STREAM_CAPTURE rate = %d\n",
			 substream->runtime->rate);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] =
			substream->runtime->rate;
	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("mt63xx_codec_prepare set up playback rate = %d\n",
			 substream->runtime->rate);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] =
			substream->runtime->rate;
	}
	return 0;
}

static int mt6323_codec_trigger(struct snd_pcm_substream *substream,
				int command, struct snd_soc_dai *Daiport)
{
	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}
	return 0;
}

static const struct snd_soc_dai_ops mt6323_aif1_dai_ops = {

	.startup = mt63xx_codec_startup,
	.prepare = mt63xx_codec_prepare,
	.trigger = mt6323_codec_trigger,
};

static struct snd_soc_dai_driver mtk_6323_dai_codecs[] = {

	{
	 .name = MT_SOC_CODEC_TXDAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_DL1_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
	{
	 .name = MT_SOC_CODEC_RXDAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .capture = {
		     .stream_name = MT_SOC_UL1_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_192000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_TDMRX_DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .capture = {
		     .stream_name = MT_SOC_TDM_CAPTURE_STREAM_NAME,
		     .channels_min = 2,
		     .channels_max = 8,
		     .rates = SNDRV_PCM_RATE_8000_192000,
		     .formats = (SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S8 |
		     SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S16_LE |
		     SNDRV_PCM_FMTBIT_U16_BE | SNDRV_PCM_FMTBIT_S16_BE |
		     SNDRV_PCM_FMTBIT_U24_LE | SNDRV_PCM_FMTBIT_S24_LE |
		     SNDRV_PCM_FMTBIT_U24_BE | SNDRV_PCM_FMTBIT_S24_BE |
		     SNDRV_PCM_FMTBIT_U24_3LE | SNDRV_PCM_FMTBIT_S24_3LE |
		     SNDRV_PCM_FMTBIT_U24_3BE | SNDRV_PCM_FMTBIT_S24_3BE |
		     SNDRV_PCM_FMTBIT_U32_LE | SNDRV_PCM_FMTBIT_S32_LE |
		     SNDRV_PCM_FMTBIT_U32_BE | SNDRV_PCM_FMTBIT_S32_BE),
		     },
	 },
	{
	 .name = MT_SOC_CODEC_I2S0TXDAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_I2SDL1_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rate_min = 8000,
		      .rate_max = 192000,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
	{
	 .name = MT_SOC_CODEC_VOICE_MD1DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_VOICE_MD2DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_FMI2S2RXDAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_FM_I2S2_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_FM_I2S2_RECORD_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_FMMRGTXDAI_DUMMY_DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_FM_MRGTX_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
	{
	 .name = MT_SOC_CODEC_ULDLLOOPBACK_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_48000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_ULDLLOOPBACK_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_48000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_STUB_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_ROUTING_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
	{
	 .name = MT_SOC_CODEC_RXDAI2_NAME,
	 .capture = {
		     .stream_name = MT_SOC_UL1DATA2_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_192000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_MRGRX_DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_MRGRX_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 8,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_MRGRX_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 8,
		     .rates = SNDRV_PCM_RATE_8000_192000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_HP_IMPEDANCE_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_HP_IMPEDANCE_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
	{
	 .name = MT_SOC_CODEC_FM_I2S_DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 8,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 8,
		     .rates = SNDRV_PCM_RATE_8000_192000,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
};


uint32 GetDLNewIFFrequency(unsigned int frequency)
{
	uint32 Reg_value = 0;
	/* pr_debug("%s frequency = %d\n", __func__, frequency); */
	switch (frequency) {
	case 8000:
		Reg_value = 0;
		break;
	case 11025:
		Reg_value = 1;
		break;
	case 12000:
		Reg_value = 2;
		break;
	case 16000:
		Reg_value = 3;
		break;
	case 22050:
		Reg_value = 4;
		break;
	case 24000:
		Reg_value = 5;
		break;
	case 32000:
		Reg_value = 6;
		break;
	case 44100:
		Reg_value = 7;
		break;
	case 48000:
		Reg_value = 8;
	default:
		pr_debug("ApplyDLNewIFFrequency with frequency = %d\n",
			frequency);
	}
	return Reg_value;
}

uint32 GetULNewIFFrequency(unsigned int frequency)
{
	uint32 Reg_value = 0;

	switch (frequency) {
	case 8000:
	case 16000:
	case 32000:
		Reg_value = 1;
		break;
	case 48000:
		Reg_value = 3;
	default:
		pr_debug("GetULNewIFFrequency with frequency = %d\n",
			frequency);
	}
	pr_debug("GetULNewIFFrequency Reg_value = %d\n", Reg_value);
	return Reg_value;
}

static void TurnOnDacPower(void)
{
	uint32 dlFreq;

	pr_debug("TurnOnDacPower\n");
	audckbufEnable(true);
	ClsqEnable(true);
	Topck_Enable(true);
	udelay(250);
	NvregEnable(true);

	/* set digital part */
	Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0,
		GetDLNewIFFrequency(
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) << 12 |
		0x330, 0xffff);
	dlFreq = GetDLFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]);
	Ana_Set_Reg(ABB_AFE_CON1, dlFreq, 0x000f);	/* DL sampling rate */
	TopCtlChangeTrigger();
	Ana_Set_Reg(ABB_AFE_CON0, 0x0001, 0x0001);	/* DL turn on enable */
}

static void TurnOffDacPower(void)
{
	pr_debug("TurnOffDacPower\n");

	Ana_Set_Reg(ABB_AFE_CON0, 0x0000, 0x0001);	/* turn off DL */
	TopCtlChangeTrigger();

	NvregEnable(false);
	ClsqEnable(false);
	Topck_Enable(false);
	audckbufEnable(false);
}

static void HeadsetVoloumeRestore(void)
{
#if 0				/* todo */
	int index = 0, oldindex = 0, offset = 0, count = 1;

	pr_debug("%s\n", __func__);
	index = 8;
	oldindex = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	if (index > oldindex) {
		pr_debug("index = %d oldindex = %d\n", index, oldindex);
		offset = index - oldindex;
		while (offset > 0) {
			Ana_Set_Reg(ZCD_CON2,
				((oldindex + count) << 7) | (oldindex + count),
				0xf9f);
			offset--;
			count++;
			udelay(100);
		}
	} else {
		pr_debug("index = %d oldindex = %d\n", index, oldindex);
		offset = oldindex - index;
		while (offset > 0) {
			Ana_Set_Reg(ZCD_CON2,
				((oldindex - count) << 7) | (oldindex - count),
				0xf9f);
			offset--;
			count++;
			udelay(100);
		}
	}
	Ana_Set_Reg(ZCD_CON2, 0x0489, 0xf9f);
#else
	pr_debug("%s no this\n", __func__);
#endif
}

static void HeadsetVolumeSet(void)
{
	/* Left channel */
	int index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];
	int index2 = 7;

	pr_debug("%s(), Lindex = %d\n", __func__, index);
	/* gain[] = {-5, -3, -1, 1, 3, 5, 7, 9}; */
	if (index > index2)
		index = index2;
	index2 -= index;
	pr_debug("%s(), Lindex = %d, Lindex2 = %d\n", __func__, index, index2);
	Ana_Set_Reg(AUDTOP_CON5, index2 << 12, 0x00007000);
	/* right channel */
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	index2 = 7;
	/* gain[] = {-5, -3, -1, 1, 3, 5, 7, 9}; */
	pr_debug("%s(), Rindex = %d\n", __func__, index);
	if (index > index2)
		index = index2;
	index2 -= index;
	pr_debug("%s(), Rindex = %d, Rindex2 = %d\n", __func__, index, index2);
	Ana_Set_Reg(AUDTOP_CON5, index2 << 8, 0x000000700);
}

static void Audio_Amp_Change(int channels, bool enable)
{
	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower();

		/* here pmic analog control */
		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false
			&&
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == false) {
			pr_debug("%s on\n", __func__);
			/* may need to modify sequence */
			/* DC compensation setting */
			pr_debug("%s, r dc cal [%d] l dc cal [%d]\n",
				 __func__, mHpRightDcCalibration,
				 mHpLeftDcCalibration);
			Ana_Set_Reg(ABB_AFE_CON3,
				mHpLeftDcCalibration, 0xffff);
			Ana_Set_Reg(ABB_AFE_CON4,
				mHpRightDcCalibration, 0xffff);
			/* enable DC cpmpensation */
			Ana_Set_Reg(ABB_AFE_CON10, 0x0001, 0x0001);
			/* Trigger DC compensation */
			DCChangeTrigger();

			/* Enable 2.4V. */
			/* Enable HalfV buffer for HP VCM generation. */
			/* Enable audio clock */
			Ana_Set_Reg(AUDTOP_CON6, 0xF7F2, 0xffff);

			/* enable clean 1.35VCM buffer in audioUL */
			Ana_Set_Reg(AUDTOP_CON0, 0x7000, 0xf000);

			/* set RCH/LCH buffer gain to smallest -5dB */
			Ana_Set_Reg(AUDTOP_CON5, 0x0014, 0xffff);

			/* enable audio bias. enable audio DAC, HP buffers */
			Ana_Set_Reg(AUDTOP_CON4, 0x007C, 0xffff);
			mdelay(10);

			/* HP pre-charge function release, */
			/* disable depop mux of HP drivers. */
			/* Disable depop VCM gen. */
			Ana_Set_Reg(AUDTOP_CON6, 0xF5BA, 0xffff);

			/* set RCH/LCH buffer gain to -1dB */
			Ana_Set_Reg(AUDTOP_CON5, 0x2214, 0xffff);

			HeadsetVolumeSet();
		}
	} else {
		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false
			&&
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == false) {
			pr_debug("Audio_Amp_Change off\n");
			/* Set HPR/HPL gain as -1dB, step by step */
			HeadsetVoloumeRestore();

			/* Set RCH/LCH buffer to smallest gain -5dB */
			Ana_Set_Reg(AUDTOP_CON5, 0x0014, 0xffff);

			/* Reset pre-charge function, */
			/* Enable depop mux of HP drivers */
			/* Enable depop VCM gen */
			Ana_Set_Reg(AUDTOP_CON6, 0xF7F2, 0xffff);

			/* Disable audio bias, audio DAC, HP buffers */
			Ana_Set_Reg(AUDTOP_CON4, 0x0000, 0xffff);

			if (GetULStatus() == false) {
				/* Disable clean 1.35V VCM buffer */
				Ana_Set_Reg(AUDTOP_CON0, 0x0000, 0x1000);
			}

			/* Disable input short of HP drivers */
			/* for voice signal leakage prevent */
			/* disable 2.4V reference buffer , audio DAC clock. */
			Ana_Set_Reg(AUDTOP_CON6, 0x37E2, 0xffff);

		}

#if 0
		/* do not reset DC calibration value when */
		/* turn off to fix pop noise issue when turn off */
		/* LCH cancel DC */
		Ana_Set_Reg(ABB_AFE_CON3, 0, 0xffff);
		/* RCH cancel DC */
		Ana_Set_Reg(ABB_AFE_CON4, 0, 0xffff);
		/* enable DC cpmpensation */
		Ana_Set_Reg(ABB_AFE_CON10, 0x0000, 0x0001);
		/* Trigger DC compensation */
		DCChangeTrigger();
#endif

		if (GetDLStatus() == false)
			TurnOffDacPower();
	}
}

static int Audio_AmpL_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_AmpL_Get = %d\n",
		 mCodec_data->
		 mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->
	    mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL];
	return 0;
}

static int Audio_AmpL_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	pr_debug("%s() gain = %ld\n ",
		__func__, ucontrol->value.integer.value[0]);
	if ((ucontrol->value.integer.value[0] == true)
		&& (mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
		== false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
		ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		(mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
		== true)) {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
		ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int Audio_AmpR_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_AmpR_Get = %d\n",
		 mCodec_data->
		 mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->
	    mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR];
	return 0;
}

static int Audio_AmpR_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	pr_debug("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true) &&
		(mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
		== false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
		mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
		ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		(mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
		== true)) {
		mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
		ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

#if 0				/* not used */
static void SetVoiceAmpVolume(void)
{
	int index;

	pr_debug("%s\n", __func__);
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL];
	Ana_Set_Reg(ZCD_CON3, index, 0x001f);
}
#endif

static void Voice_Amp_Change(bool enable)
{
	uint16 i = 3;

	if (enable) {
		pr_debug("%s\n", __func__);
		if (GetDLStatus() == false) {
			TurnOnDacPower();
			pr_debug("Voice_Amp_Change on\n");
			/* set analog part (voice HS playback) */
			/* Set voice buffer to smallest -22dB. */
			Ana_Set_Reg(AUDTOP_CON7, 0x2430, 0xffff);

			/* enable input short of HP to */
			/* prevent voice signal leakage . Enable 2.4V. */
			Ana_Set_Reg(AUDTOP_CON6, 0xB7F6, 0xffff);

			/* Depop. Enable audio clock */
			/* enable clean 1.35VCM buffer in audioUL */
			Ana_Set_Reg(AUDTOP_CON0, 0x7000, 0xf000);

			/* enable audio bias. enable LCH DAC */
			Ana_Set_Reg(AUDTOP_CON4, 0x0014, 0xffff);
			for (i = 3; i < 11; i++) {
				uint16 rReg;

				mdelay(5);
				rReg = 0x2500 + (i << 4);
				/* enable voice buffer and -1dB gain. */
				/* ramp up volume from -21dB to -1dB here */
				Ana_Set_Reg(AUDTOP_CON7, rReg, 0xffff);
			}
		}
	} else {
		uint16 i;

		pr_debug("Voice_Amp_Change off\n");
		i = (Ana_Get_Reg(AUDTOP_CON7) & 0xf0) >> 4;
		i = (i < 4) ? 4 : i;
		i = (i > 16) ? 16 : i;
		for (i = i - 1; i >= 3; i--) {
			uint16 rReg = 0x2500 + (i << 4);

			mdelay(5);
			/* disable voice buffer and -21dB gain. */
			/* ramp down volume from current to -21dB here */
			Ana_Set_Reg(AUDTOP_CON7, rReg, 0xffff);
		}
		/* set voice buffer gain as -22dB */
		Ana_Set_Reg(AUDTOP_CON7, 0x2500, 0xffff);
		/* Disable voice buffer */
		Ana_Set_Reg(AUDTOP_CON7, 0x2400, 0xffff);
		/* Disable audio bias and L-DAC */
		Ana_Set_Reg(AUDTOP_CON4, 0x0000, 0xffff);
		/* Disable 1.4v common mdoe */
		if (GetULStatus() == false)
			Ana_Set_Reg(AUDTOP_CON0, 0x0000, 0x1000);

		/* Disable input short of HP drivers */
		/* for voice signal leakage prevent */
		/* disable 2.4V reference buffer , audio DAC clock */
		Ana_Set_Reg(AUDTOP_CON6, 0x37E2, 0xffff);
		TurnOffDacPower();
	}
}

static int Voice_Amp_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Voice_Amp_Get = %d\n",
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->
	    mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL];
	return 0;
}

static int Voice_Amp_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	pr_debug("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true) &&
		(mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL]
		== false)) {
		Voice_Amp_Change(true);
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		(mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL]
		== true)) {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL]
		= ucontrol->value.integer.value[0];
		Voice_Amp_Change(false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static void Speaker_Amp_Change(bool enable)
{
#ifdef CONFIG_MTK_SPEAKER
	uint16 i = 0;
#endif

	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower();
		pr_debug("%s on\n", __func__);
		/* Set voice buffer to smallest -22dB. */
		Ana_Set_Reg(AUDTOP_CON7, 0x2400, 0xffff);

		/* enable input short of HP to */
		/* prevent voice signal leakage . Enable 2.4V. */
		Ana_Set_Reg(AUDTOP_CON6, 0xB7F6, 0xffff);

		/* Depop. Enable audio clock */
		/* enable clean 1.35VCM buffer in audioUL */
		Ana_Set_Reg(AUDTOP_CON0, 0x7000, 0xf000);

		/* enable audio bias. enable LCH DAC */
		Ana_Set_Reg(AUDTOP_CON4, 0x0014, 0xffff);
		mdelay(10);
#ifdef CONFIG_MTK_SPEAKER
		if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER) {
			/* enable voice buffer and +1dB gain. */
			/* Inter-connect voice buffer to SPK AMP */
			Ana_Set_Reg(AUDTOP_CON7, 0x35B0, 0xffff);
		} else {
			/* enable voice buffer and -11dB gain. */
			/* Inter-connect voice buffer to SPK AMP */
			Ana_Set_Reg(AUDTOP_CON7, 0x3550, 0xffff);
		}
#else
		Ana_Set_Reg(AUDTOP_CON7, 0x35B0, 0xffff);
#endif
		/* Speaker clock */
		Ana_Set_Reg(TOP_CKPDN1_CLR, 0x000E, 0x000E);
#ifdef CONFIG_MTK_SPEAKER
		if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
			Speaker_ClassD_Open();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
			Speaker_ClassAB_Open();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
			Speaker_ReveiverMode_Open();
#endif
		/* spk output stage enable and enable */
		Ana_Set_Reg(SPK_CON12, 0x0A00, 0xffff);
#ifdef CONFIG_MTK_SPEAKER
		/* 2in1 speaker do not need this */
		if (Speaker_mode != AUDIO_SPEAKER_MODE_RECEIVER) {
			for (i = 6; i <= 11; i++) {
				udelay(1 * 1000);
				/* enable voice buffer and +1dB gain. */
				/* Inter-connect voice buffer to SPK AMP */
				Ana_Set_Reg(AUDTOP_CON7,
					(0x3500 | (i << 4)), 0xffff);
			}
		}
#endif
		Apply_Speaker_Gain();
	} else {
		pr_debug("Speaker_Amp_Change off\n");
#ifdef CONFIG_MTK_SPEAKER
		/* 2in1 speaker do not need this */
		if (Speaker_mode != AUDIO_SPEAKER_MODE_RECEIVER) {
			for (i = 10; i >= 5; i--) {
				/* ramp to -11dB. */
				/* Inter-connect voice buffer to SPK AMP */
				Ana_Set_Reg(AUDTOP_CON7,
				    (0x3500 | (i << 4)), 0xffff);
				udelay(1 * 1000);
			}
		}
#endif
#ifdef CONFIG_MTK_SPEAKER
		if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
			Speaker_ClassD_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
			Speaker_ClassAB_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
			Speaker_ReveiverMode_close();
#endif
		/* Disable SPK output stage, disable spk amp. */
		Ana_Set_Reg(SPK_CON12, 0x0000, 0xffff);
		/* Disable Speaker clock */
		Ana_Set_Reg(TOP_CKPDN1_SET, 0x000E, 0x000E);
		/* set voice buffer gain as -22dB */
		Ana_Set_Reg(AUDTOP_CON7, 0x2500, 0xffff);
		/* Disable voice buffer */
		Ana_Set_Reg(AUDTOP_CON7, 0x2400, 0xffff);
		/* Disable audio bias and L-DAC */
		Ana_Set_Reg(AUDTOP_CON4, 0x0000, 0xffff);

		/* Disable 1.4v common mdoe */
		if (GetULStatus() == false)
			Ana_Set_Reg(AUDTOP_CON0, 0x0000, 0x1000);

		/* Disable input short of HP drivers */
		/* for voice signal leakage prevent */
		/* disable 2.4V reference buffer , audio DAC clock */
		Ana_Set_Reg(AUDTOP_CON6, 0x37E2, 0xffff);
		if (GetDLStatus() == false)
			TurnOffDacPower();
	}
}

static int Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL];
	return 0;
}

static int Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() value = %ld\n",
		__func__, ucontrol->value.integer.value[0]);
	if ((ucontrol->value.integer.value[0] == true) &&
		(mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL]
		== false)) {
		Speaker_Amp_Change(true);
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL]
		= ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		(mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL]
		== true)) {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL]
		= ucontrol->value.integer.value[0];
		Speaker_Amp_Change(false);
	}
	return 0;
}

#ifdef CONFIG_OF

#if defined(CONFIG_MTK_LEGACY)
static unsigned int pin_extspkamp, pin_extspkamp2,
	pin_hpspkswitch, pin_extampgain0, pin_extampgain1;
static unsigned int pin_mode_extspkamp, pin_mode_extspkamp2,
	pin_mode_hpspkswitch, pin_mode_extampgain0, pin_mode_extampgain1;
#endif

#if 0				/* not used */
static unsigned int pin_vowclk, pin_audmiso;
static unsigned int pin_mode_vowclk, pin_mode_audmiso;
#endif

#define GAP (2)			/* unit: us */

#if defined(CONFIG_MTK_LEGACY)
#define AW8736_MODE3 /*0.8w*/ \
do { \
	mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ONE); \
	udelay(GAP); \
	mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ZERO); \
	udelay(GAP); \
	mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ONE); \
	udelay(GAP); \
	mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ZERO); \
	udelay(GAP); \
	mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ONE); \
} while (0)
#endif

#ifdef CONFIG_KARNAK_PROJECT
#define GAP5 (5)			/* unit: us */
#define AW8737_MODE4 /*0.8w*/ \
do { \
	AudDrv_GPIO_EXTAMP_Select(true); \
	udelay(GAP5); \
	AudDrv_GPIO_EXTAMP_Select(false); \
	udelay(GAP5); \
	AudDrv_GPIO_EXTAMP_Select(true); \
	udelay(GAP5); \
	AudDrv_GPIO_EXTAMP_Select(false); \
	udelay(GAP5); \
	AudDrv_GPIO_EXTAMP_Select(true); \
	udelay(GAP5); \
	AudDrv_GPIO_EXTAMP_Select(false); \
	udelay(GAP5); \
	AudDrv_GPIO_EXTAMP_Select(true); \
} while (0)
#endif

static void Ext_Speaker_Amp_Change(bool enable)
{
#define SPK_WARM_UP_TIME        (25)	/* unit is ms */

#if defined(CONFIG_MTK_LEGACY)
	int ret;

	ret = GetGPIO_Info(5, &pin_extspkamp, &pin_mode_extspkamp);
	if (ret < 0) {
		pr_err("Ext_Speaker_Amp_Change GetGPIO_Info FAIL!!!\n");
		return;
	}
#endif

	if (enable) {
		pr_debug("Ext_Speaker_Amp_Change ON+\n");
#ifndef CONFIG_MTK_SPEAKER
		pr_debug("Ext_Speaker_Amp_Change ON set GPIO\n");

#if defined(CONFIG_MTK_LEGACY)
		/* GPIO24: mode 0 */
		mt_set_gpio_mode(pin_extspkamp, GPIO_MODE_00);
		mt_set_gpio_pull_enable(pin_extspkamp, GPIO_PULL_ENABLE);
		mt_set_gpio_dir(pin_extspkamp, GPIO_DIR_OUT);	/* output */
		/* low disable */
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ZERO);
#else
		AudDrv_GPIO_EXTAMP_Select(false);
#endif /*CONFIG_MTK_LEGACY*/

		udelay(1000);

#if defined(CONFIG_MTK_LEGACY)
		/* output */
		mt_set_gpio_dir(pin_extspkamp, GPIO_DIR_OUT);

#ifdef AW8736_MODE_CTRL
		AW8736_MODE3;
#else
		/* high enable */
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ONE);
#endif

#else
		AudDrv_GPIO_EXTAMP_Select(true);
#endif /*CONFIG_MTK_LEGACY*/

		msleep(SPK_WARM_UP_TIME);
#endif
#ifdef CONFIG_KARNAK_PROJECT
		AW8737_MODE4;
#endif
		pr_debug("Ext_Speaker_Amp_Change ON-\n");
	} else {
		pr_debug("Ext_Speaker_Amp_Change OFF+\n");
#ifndef CONFIG_MTK_SPEAKER

#if defined(CONFIG_MTK_LEGACY)
		/* mt_set_gpio_mode(pin_extspkamp, GPIO_MODE_00); */
		/* GPIO24: mode 0 */
		/* output */
		mt_set_gpio_dir(pin_extspkamp, GPIO_DIR_OUT);
		/* low disbale */
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ZERO);
#else
		AudDrv_GPIO_EXTAMP_Select(false);
#endif

		udelay(500);
#endif
#ifdef CONFIG_KARNAK_PROJECT
		AudDrv_GPIO_EXTAMP_Select(false);
#endif
		pr_debug("Ext_Speaker_Amp_Change OFF-\n");
	}
}

#else
#ifndef CONFIG_MTK_SPEAKER
#ifdef AW8736_MODE_CTRL
/* 0.75us<TL<10us; 0.75us<TH<10us */
#define GAP (2)			/* unit: us */
#define AW8736_MODE1 /*1.2w*/ \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE)

#define AW8736_MODE2 /*1.0w*/ \
do { \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE); \
} while (0)

#define AW8736_MODE3 /*0.8w*/ \
do { \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE); \
} while (0)

#define AW8736_MODE4 /*it depends on THD, range: 1.5 ~ 2.0w*/ \
do { \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO); \
	udelay(GAP); \
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE); \
} while (0)
#endif
#endif

static void Ext_Speaker_Amp_Change(bool enable)
{
#define SPK_WARM_UP_TIME        (25)	/* unit is ms */
	if (enable) {
		pr_debug("Ext_Speaker_Amp_Change ON+\n");
#ifndef CONFIG_MTK_SPEAKER
		pr_debug("Ext_Speaker_Amp_Change ON set GPIO\n");
		/* GPIO117: DPI_D3, mode 0 */
		mt_set_gpio_mode(GPIO_EXT_SPKAMP_EN_PIN, GPIO_MODE_00);
		mt_set_gpio_pull_enable(GPIO_EXT_SPKAMP_EN_PIN,
			GPIO_PULL_ENABLE);
		/* output */
		mt_set_gpio_dir(GPIO_EXT_SPKAMP_EN_PIN, GPIO_DIR_OUT);
		/* low disable */
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO);
		udelay(1000);
		/* output */
		mt_set_gpio_dir(GPIO_EXT_SPKAMP_EN_PIN, GPIO_DIR_OUT);
#ifdef AW8736_MODE_CTRL
		AW8736_MODE3;
#else
		/* high enable */
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);
#endif
		msleep(SPK_WARM_UP_TIME);
#endif
		pr_debug("Ext_Speaker_Amp_Change ON-\n");
	} else {
		pr_debug("Ext_Speaker_Amp_Change OFF+\n");
#ifndef CONFIG_MTK_SPEAKER
		/* mt_set_gpio_mode(GPIO_EXT_SPKAMP_EN_PIN, GPIO_MODE_00); */
		/* GPIO117: DPI_D3, mode 0 */
		/* output */
		mt_set_gpio_dir(GPIO_EXT_SPKAMP_EN_PIN, GPIO_DIR_OUT);
		/* low disbale */
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO);
		udelay(500);
#endif
		pr_debug("Ext_Speaker_Amp_Change OFF-\n");
	}
}
#endif

static int Ext_Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	    mCodec_data->
	    mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP];
	return 0;
}

static int Ext_Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() gain = %ld\n",
		__func__, ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		Ext_Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP]
		= ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP]
		= ucontrol->value.integer.value[0];
		Ext_Speaker_Amp_Change(false);
	}
	return 0;
}

static void Headphone_Speaker_Switch_Change(bool enable)
{
	pr_debug("%s enable = %d\n", __func__, enable);

#ifndef CONFIG_MTK_SPEAKER
	if (enable) {
		pr_debug("%s ON set GPIO\n", __func__);
		AudDrv_GPIO_HP_SPK_Switch_Select(true);
	} else {
		pr_debug("%s OFF set GPIO\n", __func__);
		AudDrv_GPIO_HP_SPK_Switch_Select(false);
	}
#endif
}

static int Headphone_Speaker_Mux_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	mCodec_data->
	mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH];
	return 0;
}

static int Headphone_Speaker_Mux_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	pr_debug("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true) &&
		(mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH]
		== false)) {
		Headphone_Speaker_Switch_Change(true);
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH]
		= ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		(mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH]
		== true)) {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH]
		= ucontrol->value.integer.value[0];
		Headphone_Speaker_Switch_Change(false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static void Ext_Headphone_Amp_Change(bool enable)
{
	pr_debug("%s enable = %d\n", __func__, enable);

	if (enable) {
		pr_debug("%s ON set GPIO\n", __func__);
		AudDrv_GPIO_EXTHPAMP_Select(true);
		usleep_range(5*1000, 20*1000);
	} else {
		pr_debug("%s OFF set GPIO\n", __func__);
		AudDrv_GPIO_EXTHPAMP_Select(false);
	}
}

static int Ext_Headphone_Amp_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	mCodec_data->
	mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTHPAMP];
	return 0;
}

static int Ext_Headphone_Amp_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	pr_debug("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true) &&
		(mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTHPAMP]
		== false)) {
		Ext_Headphone_Amp_Change(true);
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTHPAMP]
		= ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		(mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTHPAMP]
		== true)) {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTHPAMP] =
		ucontrol->value.integer.value[0];
		Ext_Headphone_Amp_Change(false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static void Headset_Speaker_Amp_Change(bool enable)
{
	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower();
		pr_debug("turn on Headset_Speaker_Amp_Change\n");
		/* DC compensation setting */
		pr_debug("%s, mHpRightDcCalibration [%d] mHpLeftDcCalibration [%d]\n",
			__func__, mHpRightDcCalibration, mHpLeftDcCalibration);
		Ana_Set_Reg(ABB_AFE_CON3, mHpLeftDcCalibration, 0xffff);
		Ana_Set_Reg(ABB_AFE_CON4, mHpRightDcCalibration, 0xffff);
		/* enable DC cpmpensation */
		Ana_Set_Reg(ABB_AFE_CON10, 0x0001, 0x0001);
		DCChangeTrigger();	/* Trigger DC compensation */

		/* enable input short of HP to */
		/* prevent voice signal leakage . Enable 2.4V. */
		Ana_Set_Reg(AUDTOP_CON6, 0xF7F2, 0xffff);

		/* Depop. Enable audio clock */
		/* enable clean 1.35VCM buffer in audioUL */
		Ana_Set_Reg(AUDTOP_CON0, 0x7000, 0xf000);
		/* set RCH/LCH buffer gain to smallest -5dB */
		Ana_Set_Reg(AUDTOP_CON5, 0x0014, 0xffff);
		/* enable audio bias. enable audio DAC, HP buffers */
		Ana_Set_Reg(AUDTOP_CON4, 0x007C, 0xffff);
		mdelay(10);

		/* HP pre-charge function release, */
		/* disable depop mux of HP drivers. Disable depop VCM gen. */
		Ana_Set_Reg(AUDTOP_CON6, 0xF5BA, 0xffff);

		/* set RCH/LCH buffer gain to -1dB */
		Ana_Set_Reg(AUDTOP_CON5, 0x2214, 0xffff);

		/* enable voice buffer and -1dB gain. */
		/* Inter-connect voice buffer to SPK AMP */
		Ana_Set_Reg(AUDTOP_CON7, 0x35B0, 0xffff);

		/* Speaker clock */
		Ana_Set_Reg(TOP_CKPDN1_CLR, 0x000E, 0x000E);

#ifdef CONFIG_MTK_SPEAKER
		if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
			Speaker_ClassD_Open();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
			Speaker_ClassAB_Open();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
			Speaker_ReveiverMode_Open();
#endif

		/* spk output stage enable and enable spk amp */
		Ana_Set_Reg(SPK_CON12, 0x0A00, 0xffff);

		/* apply volume setting */
		HeadsetVolumeSet();
		Apply_Speaker_Gain();
	} else {
		pr_debug("turn off Headset_Speaker_Amp_Change\n");

#ifdef CONFIG_MTK_SPEAKER
		if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
			Speaker_ClassD_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
			Speaker_ClassAB_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
			Speaker_ReveiverMode_close();
#endif

		/* Disable SPK output stage, disable spk amp. */
		Ana_Set_Reg(SPK_CON12, 0x0000, 0xffff);

		/* Disable Speaker clock */
		Ana_Set_Reg(TOP_CKPDN1_SET, 0x000E, 0x000E);

		/* Voice buffer */
		/* set voice buffer gain as -22dB */
		Ana_Set_Reg(AUDTOP_CON7, 0x2500, 0xffff);
		/* Disable voice buffer */
		Ana_Set_Reg(AUDTOP_CON7, 0x2400, 0xffff);

		/* Audio buffer */
		/* Set RCH/LCH buffer to smallest gain -5dB */
		Ana_Set_Reg(AUDTOP_CON5, 0x0014, 0xffff);
		/* Reset pre-charge function, */
		/* Enable depop mux of HP drivers, Enable depop VCM gen */
		Ana_Set_Reg(AUDTOP_CON6, 0xF7F2, 0xffff);

		/* audio bias */
		/* Disable audio bias, audio DAC, HP buffers */
		Ana_Set_Reg(AUDTOP_CON4, 0x0000, 0xffff);

		/* common 1.35V */
		if (GetULStatus() == false) {
			/* Disable clean 1.35V VCM buffer in audio UL. */
			Ana_Set_Reg(AUDTOP_CON0, 0x0000, 0x1000);
		}

		/* Disable input short of HP drivers */
		/* for voice signal leakage prevent, */
		/* disable 2.4V reference buffer , */
		/* audio DAC clock. */
		Ana_Set_Reg(AUDTOP_CON6, 0x37E2, 0xffff);

		/* Set HPR/HPL gain as 0dB, step by step */
		HeadsetVoloumeRestore();

		if (GetDLStatus() == false) {
#if 0
			/* do not reset DC calibration value when*/
			/* turn off to fix pop noise issue when turn off */
			/* LCH cancel DC */
			Ana_Set_Reg(ABB_AFE_CON3, 0, 0xffff);
			/* RCH cancel DC */
			Ana_Set_Reg(ABB_AFE_CON4, 0, 0xffff);
			/* enable DC cpmpensation */
			Ana_Set_Reg(ABB_AFE_CON10, 0x0000, 0x0001);
			DCChangeTrigger();	/* Trigger DC compensation */
#endif
			TurnOffDacPower();
		}
	}
}


static int Headset_Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	    mCodec_data->
	    mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R];
	return 0;
}

static int Headset_Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	pr_debug("%s() gain = %lu\n",
		__func__, ucontrol->value.integer.value[0]);
	if ((ucontrol->value.integer.value[0] == true) &&
		(mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R]
		== false)) {
		Headset_Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R]
		= ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		(mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R]
		== true)) {
		mCodec_data->mAudio_Ana_DevicePower[
		AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R]
		= ucontrol->value.integer.value[0];
		Headset_Speaker_Amp_Change(false);
	}
	return 0;
}

#ifdef CONFIG_MTK_SPEAKER
static const char * const speaker_amp_function[] = {
	"CALSSD", "CLASSAB", "RECEIVER" };

static const char * const speaker_PGA_function[] = {
	"Mute", "0Db", "4Db", "5Db", "6Db", "7Db", "8Db", "9Db", "10Db",
	"11Db", "12Db", "13Db", "14Db", "15Db", "16Db", "17Db"
};
static const char * const speaker_OC_function[] = { "Off", "On" };
static const char * const speaker_CS_function[] = { "Off", "On" };
static const char * const speaker_CSPeakDetecReset_function[] = { "Off", "On" };

static int Audio_Speaker_Class_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	Speaker_mode = ucontrol->value.integer.value[0];
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int Audio_Speaker_Class_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = Speaker_mode;
	return 0;
}

static int Audio_Speaker_Pga_Gain_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	Speaker_pga_gain = ucontrol->value.integer.value[0];
	pr_debug("%s Speaker_pga_gain= %d\n", __func__, Speaker_pga_gain);
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	/* this will base on hw spec, use 15dB for */
	/* gain[] =  {mute,0,4,5,6,7,8,9,10,11,12,13,14,15,16,17}; */
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(speaker_PGA_function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	index = Speaker_pga_gain;

	if (index > 11)
		index = 11;

	if (index < 1)
		index = 1;	/* min to 0dB */

	pr_debug("%s(), index = %d\n", __func__, index);
	Ana_Set_Reg(SPK_CON9, index << 8, 0x00000f00);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Speaker_OcFlag_Get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	mSpeaker_Ocflag = GetSpeakerOcFlag();
	ucontrol->value.integer.value[0] = mSpeaker_Ocflag;
	return 0;
}

static int Audio_Speaker_OcFlag_Set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s is not support setting\n", __func__);
	return 0;
}

static int Audio_Speaker_Pga_Gain_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = Speaker_pga_gain;
	return 0;
}

static const struct soc_enum Audio_Speaker_Enum[] = {

	/* speaker class setting */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_amp_function),
		speaker_amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_PGA_function),
		speaker_PGA_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_OC_function),
		speaker_OC_function),
};

static const struct snd_kcontrol_new mt6323_snd_Speaker_controls[] = {

	SOC_ENUM_EXT("Audio_Speaker_class_Switch",
		     Audio_Speaker_Enum[0], Audio_Speaker_Class_Get,
		     Audio_Speaker_Class_Set),
	SOC_ENUM_EXT("Audio_Speaker_PGA_gain",
		     Audio_Speaker_Enum[1], Audio_Speaker_Pga_Gain_Get,
		     Audio_Speaker_Pga_Gain_Set),
	SOC_ENUM_EXT("Audio_Speaker_OC_Falg",
		     Audio_Speaker_Enum[2], Audio_Speaker_OcFlag_Get,
		     Audio_Speaker_OcFlag_Set),
};
#endif


static const char * const amp_function[] = { "Off", "On" };
static const char * const aud_clk_buf_function[] = { "Off", "On" };
static const char * const hp_spk_sel_function[] = { "Speaker", "Headphone" };

static const char * const DAC_DL_PGA_Headset_GAIN[] = { "9Db", "7Db",
"5Db", "3Db", "1Db", "-1Db", "-3Db", "-5Db" };

static const char * const DAC_DL_PGA_Handset_GAIN[] = { "XXX", "XXX", "XXX",
	"-16Db", "-14Db", "-12Db", "-10Db", "-8Db", "-6Db",
	"-4Db", "-2Db", "0Db", "2Db",
	"4Db", "6Db", "8Db"};

/* Lineout use */
static const char * const DAC_DL_PGA_Speaker_GAIN[] = { "8Db", "7Db", "6Db",
"5Db", "4Db", "3Db", "2Db", "1Db", "0Db", "-1Db", "-2Db", "-3Db", "-4Db",
"-5Db", "-6Db", "-7Db", "-8Db", "-9Db", "-10Db", "-40Db" };

/* ext amp use */
static const char * const EXT_AMP_GAIN[] = { "6dB", "12dB", "18dB", "24dB" };

/* static const char *Voice_Mux_function[] = {"Voice", "Speaker"}; */

static int Lineout_PGAL_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Speaker_PGA_Get = %d\n",
		 mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL];
	return 0;
}

static int Lineout_PGAL_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("%s(), index = %d\n",
		__func__, ucontrol->value.enumerated.item[0]);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	if (ucontrol->value.enumerated.item[0] ==
		(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
		index = 0x1f;
#if 0				/* todo */
	Ana_Set_Reg(ZCD_CON1, index, 0x001f);
#endif
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Lineout_PGAR_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s = %d\n",
		__func__,
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR];
	return 0;
}

static int Lineout_PGAR_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index = 0;

	pr_debug("%s(), index = %d\n",
		__func__, ucontrol->value.enumerated.item[0]);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	if (ucontrol->value.enumerated.item[0] ==
		(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
		index = 0x1f;
#if 0				/* todo */
	Ana_Set_Reg(ZCD_CON1, index << 7, 0x0f10);
#endif
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Handset_PGA_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Handset_PGA_Get = %d\n",
		 mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL];
	return 0;
}

static int Handset_PGA_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index = 0;
	int index2 = 15;
	/* gain[] =  {-21, -19, -17, -15, -13, */
	/* -11, -9, -7, -5, -3, -1, 1, 3, 5, 7, 9}; */
	pr_debug("%s(), index = %d\n",
		__func__, ucontrol->value.enumerated.item[0]);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	if (index > index2)
		index = index2;
	pr_debug("%s(), index = %d, index2 = %d\n", __func__, index, index2);
	Ana_Set_Reg(AUDTOP_CON7, index << 4, 0x000000f0);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Headset_PGAL_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Headset_PGAL_Get = %d\n",
		 mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];
	return 0;
}

static int Headset_PGAL_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index = 0;
	int index2 = 7;
	/* gain[] = {-5, -3, -1, 1, 3, 5, 7, 9}; */
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	if (index > index2)
		index = index2;
	index2 -= index;
	pr_debug("%s(), index = %d, index2 = %d\n", __func__, index, index2);
	Ana_Set_Reg(AUDTOP_CON5, index2 << 12, 0x00007000);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Headset_PGAR_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Headset_PGAR_Get = %d\n",
		 mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	return 0;
}


static int Headset_PGAR_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index = 0;
	int index2 = 7;
	/* gain[] = {-5, -3, -1, 1, 3, 5, 7, 9}; */
	pr_debug("%s(), index = %d\n",
		__func__, ucontrol->value.enumerated.item[0]);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	if (index > index2)
		index = index2;
	index2 -= index;
	pr_debug("%s(), index = %d, index2 = %d\n", __func__, index, index2);
	Ana_Set_Reg(AUDTOP_CON5, index2 << 8, 0x000000700);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] =
	    ucontrol->value.integer.value[0];
	return 0;
}


static int Apply_Ext_Amp_Gain(int index)
{
#ifndef CONFIG_MTK_SPEAKER
	pr_debug("%s index = %d\n", __func__, index);
	return AudDrv_GPIO_EXTAMP_Gain_Set(index);
#endif
	return 0;
}

static int Ext_Amp_Gain_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s index = %d\n", __func__,
		 mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_EXT_AMP]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_EXT_AMP];
	return 0;
}

static int Ext_Amp_Gain_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;
	int ret = 0;

	pr_debug("%s(), index = %d\n",
		__func__, ucontrol->value.enumerated.item[0]);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(EXT_AMP_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];

	ret = Apply_Ext_Amp_Gain(index);

	if (ret != 0)
		pr_err("%s failed!\n", __func__);

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_EXT_AMP] =
	    ucontrol->value.integer.value[0];

	return ret;
}

static int Aud_Clk_Buf_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = audck_buf_Count;
	return 0;
}

static int Aud_Clk_Buf_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), value = %d\n",
		__func__, ucontrol->value.enumerated.item[0]);
	if (ucontrol->value.integer.value[0])
		audckbufEnable(true);
	else
		audckbufEnable(false);
	return 0;
}

static const struct soc_enum Audio_DL_Enum[] = {

	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(hp_spk_sel_function),
		hp_spk_sel_function),
	/* here comes pga gain setting */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN),
		DAC_DL_PGA_Headset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN),
		DAC_DL_PGA_Headset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN),
		DAC_DL_PGA_Handset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN),
		DAC_DL_PGA_Speaker_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN),
		DAC_DL_PGA_Speaker_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(EXT_AMP_GAIN), EXT_AMP_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aud_clk_buf_function),
		aud_clk_buf_function),
};

static const struct snd_kcontrol_new mt6323_snd_controls[] = {

	SOC_ENUM_EXT("Audio_Amp_R_Switch",
		Audio_DL_Enum[0], Audio_AmpR_Get, Audio_AmpR_Set),
	SOC_ENUM_EXT("Audio_Amp_L_Switch",
		Audio_DL_Enum[1], Audio_AmpL_Get, Audio_AmpL_Set),
	SOC_ENUM_EXT("Voice_Amp_Switch",
		Audio_DL_Enum[2], Voice_Amp_Get, Voice_Amp_Set),
	SOC_ENUM_EXT("Speaker_Amp_Switch",
		Audio_DL_Enum[3], Speaker_Amp_Get, Speaker_Amp_Set),
	SOC_ENUM_EXT("Headset_Speaker_Amp_Switch",
		Audio_DL_Enum[4], Headset_Speaker_Amp_Get,
		     Headset_Speaker_Amp_Set),
	SOC_ENUM_EXT("Ext_Speaker_Amp_Switch",
		Audio_DL_Enum[5], Ext_Speaker_Amp_Get,
		     Ext_Speaker_Amp_Set),
	SOC_ENUM_EXT("Ext_Headphone_Amp_Switch",
		Audio_DL_Enum[6], Ext_Headphone_Amp_Get,
		     Ext_Headphone_Amp_Set),
	SOC_ENUM_EXT("Headphone_Speaker_Mux",
		Audio_DL_Enum[7], Headphone_Speaker_Mux_Get,
		      Headphone_Speaker_Mux_Set),
	SOC_ENUM_EXT("Headset_PGAL_GAIN",
		Audio_DL_Enum[8], Headset_PGAL_Get, Headset_PGAL_Set),
	SOC_ENUM_EXT("Headset_PGAR_GAIN",
		Audio_DL_Enum[9], Headset_PGAR_Get, Headset_PGAR_Set),
	SOC_ENUM_EXT("Handset_PGA_GAIN",
		Audio_DL_Enum[10], Handset_PGA_Get, Handset_PGA_Set),
	SOC_ENUM_EXT("Lineout_PGAR_GAIN",
		Audio_DL_Enum[11], Lineout_PGAR_Get, Lineout_PGAR_Set),
	SOC_ENUM_EXT("Lineout_PGAL_GAIN",
		Audio_DL_Enum[12], Lineout_PGAL_Get, Lineout_PGAL_Set),
	SOC_ENUM_EXT("Ext_Amp_Gain",
		Audio_DL_Enum[13], Ext_Amp_Gain_Get, Ext_Amp_Gain_Set),
	SOC_ENUM_EXT("AUD_CLK_BUF_Switch",
		Audio_DL_Enum[14], Aud_Clk_Buf_Get, Aud_Clk_Buf_Set),
};

static const struct snd_kcontrol_new mt6323_Voice_Switch[] = {
#if 0
	 SOC_DAPM_ENUM_EXT("Voice Mux",
		Audio_DL_Enum[10], Voice_Mux_Get, Voice_Mux_Set);
#endif
};

void SetMicPGAGain(void)
{
	int index = 0;
	int index2 = 5;
	/* set mic1 PGA gain */
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
	pr_debug("%s(), mic1 index = %d, index2 = %d\n",
		__func__, index, index2);
	if (index > index2)
		index = index2;
	/* const int PreAmpGain[] = {-6, 0, 6, 12, 18, 24}; */
	Ana_Set_Reg(AUDTOP_CON0, index << 4, 0x00000070);
	/* set mic2 PGA gain */
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
	pr_debug("%s(), mic2 index = %d, index2 = %d\n",
		__func__, index, index2);
	if (index > index2)
		index = index2;
	Ana_Set_Reg(AUDTOP_CON1, index << 8, 0x00000700);
}

static bool GetAdcStatus(void)
{
	int i = 0;

	for (i = AUDIO_ANALOG_DEVICE_IN_ADC1;
		i < AUDIO_ANALOG_DEVICE_MAX; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}

#if 0				/* not used */
static bool GetDacStatus(void)
{
	int i = 0;

	for (i = AUDIO_ANALOG_DEVICE_OUT_EARPIECER;
		i < AUDIO_ANALOG_DEVICE_2IN1_SPK; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}
#endif

static bool TurnOnADcPowerACC(int ADCType, bool enable)
{
#if 0				/* unused variable */
	bool refmic_using_ADC_L;
#endif
	pr_debug("%s ADCType=%d, enable=%x, AdcStatus=%x, DLStatus=%x\n",
		 __func__, ADCType, enable, GetAdcStatus(), GetDLStatus());
	if (enable) {
		uint32 SampleRate_VUL1 =
			mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];

		if (GetAdcStatus() == false) {
			/* here to set digital part */
			Topck_Enable(true);
			/* config UL up8x_rxif adc voice mode */
			Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x302F |
				(GetULNewIFFrequency(SampleRate_VUL1) << 10),
				0xffff);
			Ana_Set_Reg(ABB_AFE_CON1,
				GetULFrequency(SampleRate_VUL1) << 4, 0x0010);
			TopCtlChangeTrigger();
			/* turn on UL */
			Ana_Set_Reg(ABB_AFE_CON0, 0x0002, 0x0002);
		}
		SetMicPGAGain();

		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {
			/* main and headset mic */
			/* Enable LCH 1.4v, 2.4V */
			Ana_Set_Reg(AUDTOP_CON0, 0x7800, 0x7f80);
			/* Enable RCH 1.4V, 2.4V */
			Ana_Set_Reg(AUDTOP_CON2, 0x00F0, 0x00ff);
			Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000100);

			if (mCodec_data->
				mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				== 0) {
				/* "ADC1", main_mic */
				Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000100);
				Ana_Set_Reg(AUDTOP_CON0, 0, 0x0000000f);
			} else if (mCodec_data->
				mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				== 1) {
				/* "ADC2", headset mic */
				/* L */
				Ana_Set_Reg(AUDTOP_CON0, 1, 0x0000000f);
				Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000200);
				/* R */
				Ana_Set_Reg(AUDTOP_CON1, 0x0010, 0x000000f0);
			} else if (mCodec_data->
				mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				== 2) {
				/* "ADC3", ref mic */
				Ana_Set_Reg(AUDTOP_CON0, 4, 0x0000000f);
				Ana_Set_Reg(AUDTOP_CON3, 0x0100, 0x00000100);
			}
			/* Ana_Set_Reg(AUDTOP_CON1 , 0x0300, 0x0700); */
			/* RCH PGA gain +12dB */
			/* Ana_Set_Reg(AUDTOP_CON0 , 0x0030, 0x0070); */
			/* LCH PGA gain +12dB */
			/* MICBIAS */
			Ana_Set_Reg(AUDTOP_CON8, 0x0008, 0x0008);
			/* Enable LCH ADC, PGA */
			Ana_Set_Reg(AUDTOP_CON0, 0x0180, 0x0180);
			/* Enable RCH ADC, PGA */
			Ana_Set_Reg(AUDTOP_CON2, 0x0003, 0x0003);
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {
			/* ref mic */
			if (mCodec_data->mAudio_Ana_Mux[
				AUDIO_MICSOURCE_MUX_IN_2] == 2) {
				/* "ADC3", ref mic */
				Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000200);
				Ana_Set_Reg(AUDTOP_CON1, 0x0000, 0x000000f0);
			} else if (mCodec_data->mAudio_Ana_Mux[
				AUDIO_MICSOURCE_MUX_IN_2] == 1) {
				/* "ADC2", headset mic */
				Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000200);
				Ana_Set_Reg(AUDTOP_CON1, 0x0010, 0x000000f0);
			} else if (mCodec_data->mAudio_Ana_Mux[
				AUDIO_MICSOURCE_MUX_IN_2] == 0) {
				/* "ADC1", main mic */
				Ana_Set_Reg(AUDTOP_CON3, 0x0200, 0x00000200);
				Ana_Set_Reg(AUDTOP_CON1, 0x0040, 0x000000f0);
			} else {
				/* need to move to dual mic first */
				/* open when enable ADC1? */
				Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000200);
				Ana_Set_Reg(AUDTOP_CON1, 0x0000, 0x000000f0);
			}
		}
	} else {
		if (GetAdcStatus() == false) {
			/* Disable LCH ADC, PGA */
			Ana_Set_Reg(AUDTOP_CON0, 0x0000, 0x0180);
			/* Disable RCH ADC, PGA */
			Ana_Set_Reg(AUDTOP_CON2, 0x0000, 0x0003);

			if (GetDLStatus() == false) /* Disable LCH 1.4v, 2.4V */
				Ana_Set_Reg(AUDTOP_CON0, 0x6000, 0x7f80);
			else /* Disable LCH 2.4V, keep 1.4V */
				Ana_Set_Reg(AUDTOP_CON0, 0x7000, 0x7f80);

			/* Disable RCH 1.4V, 2.4V ALPS00824353 */
			/* always disable RG_AUDULR_VCMSEL */
			Ana_Set_Reg(AUDTOP_CON2, 0x00C0, 0x00ff);
			Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000100);
			Ana_Set_Reg(AUDTOP_CON0, 0x0003, 0x0000000f);
			Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000200);
			Ana_Set_Reg(AUDTOP_CON1, 0x0020, 0x000000f0);
			/* MICBIAS */
			Ana_Set_Reg(AUDTOP_CON8, 0x0000, 0x0008);
			/* turn off UL */
			Ana_Set_Reg(ABB_AFE_CON0, 0x0000, 0x0002);
			Topck_Enable(false);
		}
	}
	return true;
}

static bool TurnOnADcPowerDmic(int ADCType, bool enable)
{
	pr_debug("%s ADCType = %d enable = %d, AdcStatus=%x\n",
		__func__, ADCType, enable, GetAdcStatus());
	if (enable) {
		uint32 SampleRate_VUL1 =
			mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];

		if (GetAdcStatus() == false) {
			/* here to set digital part */
			Topck_Enable(true);
			/* config UL up8x_rxif adc voice mode */
			Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x302F |
				(GetULNewIFFrequency(SampleRate_VUL1) << 10),
				0xffff);
			Ana_Set_Reg(ABB_AFE_CON1,
				GetULFrequency(SampleRate_VUL1) << 4, 0x0010);
			TopCtlChangeTrigger();
			SetMicPGAGain();
			/* enable digital mic, 3.25M clock rate */
			Ana_Set_Reg(ABB_AFE_CON9, 0x0011, 0x0011);
			/* turn on UL */
			Ana_Set_Reg(ABB_AFE_CON0, 0x0002, 0x0002);
			/* MICBIAS, digital mic enable */
			Ana_Set_Reg(AUDTOP_CON8, 0x020C, 0x03FF);
		}
	} else {
		if (GetAdcStatus() == false) {
			/* MICBIAS, digital mic disable */
			Ana_Set_Reg(AUDTOP_CON8, 0x0000, 0x000C);
			/* disable digital mic */
			Ana_Set_Reg(ABB_AFE_CON9, 0x0000, 0x0010);
			/* turn off UL */
			Ana_Set_Reg(ABB_AFE_CON0, 0x0000, 0x0002);
			Topck_Enable(false);
		}
	}
	return true;
}

static bool TurnOnADcPowerDCC(int ADCType, bool enable)
{
	pr_debug("%s ADCType = %d enable = %d, not support\n",
		__func__, ADCType, enable);
	return true;
}


static bool TurnOnADcPowerDCCECM(int ADCType, bool enable)
{
	/* use TurnOnADcPowerDCC() with SetDCcoupleNP() setting ECM */
	/*  or not depending on mAudio_Analog_Mic1_mode */
	/* mAudio_Analog_Mic2_mode */
	TurnOnADcPowerDCC(ADCType, enable);
	return true;
}


/* here start uplink power function */
static const char * const ADC_function[] = { "Off", "On" };
static const char * const ADC_power_mode[] = { "normal", "lowpower" };
static const char * const PreAmp_Mux_function[] = { "OPEN", "IN_ADC1",
	"IN_ADC2", "IN_ADC3" };
static const char * const ADC_UL_PGA_GAIN[] = { "-6Db", "0Db", "6Db",
	"12Db", "18Db", "24Db" };
static const char * const Pmic_Digital_Mux[] = { "ADC1", "ADC2",
	"ADC3", "ADC4" };
static const char * const Adc_Input_Sel[] = { "idle", "AIN", "Preamp" };

static const char * const Audio_AnalogMic_Mode[] = {
	"ACCMODE", "DCCMODE", "DMIC", "DCCECMDIFFMODE", "DCCECMSINGLEMODE"};


static const struct soc_enum Audio_UL_Enum[] = {

	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(PreAmp_Mux_function),
		PreAmp_Mux_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(PreAmp_Mux_function),
		PreAmp_Mux_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode),
		Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode),
		Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode),
		Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode),
		Audio_AnalogMic_Mode),
};

static int Audio_ADC1_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_ADC1_Get = %d\n",
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1];
	return 0;
}

static int Audio_ADC1_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	mutex_lock(&Ana_Power_Mutex);
	if (ucontrol->value.integer.value[0]) {
		if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode ==
			AUDIO_ANALOGUL_MODE_DCCECMDIFF ||
			mAudio_Analog_Mic1_mode ==
			AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
		    ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode ==
			AUDIO_ANALOGUL_MODE_DCCECMDIFF ||
			mAudio_Analog_Mic1_mode ==
			AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC1,
				false);
	}
	mutex_unlock(&Ana_Power_Mutex);
	return 0;
}

static int Audio_ADC2_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_ADC2_Get = %d\n",
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2];
	return 0;
}

static int Audio_ADC2_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	mutex_lock(&Ana_Power_Mutex);
	if (ucontrol->value.integer.value[0]) {
		if (mAudio_Analog_Mic2_mode ==
			AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2,
				true);
		else if (mAudio_Analog_Mic2_mode ==
			AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2,
				true);
		else if (mAudio_Analog_Mic2_mode ==
			AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2,
				true);
		else if (mAudio_Analog_Mic2_mode ==
			AUDIO_ANALOGUL_MODE_DCCECMDIFF ||
			mAudio_Analog_Mic2_mode ==
			AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC2,
				true);
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
		    ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic2_mode ==
			AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2,
				false);
		else if (mAudio_Analog_Mic2_mode ==
			AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2,
				false);
		else if (mAudio_Analog_Mic2_mode ==
			AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2,
				false);
		else if (mAudio_Analog_Mic2_mode ==
			AUDIO_ANALOGUL_MODE_DCCECMDIFF ||
			mAudio_Analog_Mic2_mode ==
			AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC2,
				false);
	}
	mutex_unlock(&Ana_Power_Mutex);
	return 0;
}

static int Audio_ADC3_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}

static int Audio_ADC3_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}

static int Audio_ADC4_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}

static int Audio_ADC4_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}

static bool AudioPreAmp1_Sel(int Mul_Sel)
{
	pr_debug("%s Mul_Sel = %d\n", __func__, Mul_Sel);
#if 0				/* todo */
	if (Mul_Sel == 0) /* pinumx open */
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x00C0);
	else if (Mul_Sel == 1)
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0040, 0x00C0);	/* AIN0 */
	else if (Mul_Sel == 2)
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0080, 0x00C0);	/* AIN1 */
	else if (Mul_Sel == 3)
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x00C0, 0x00C0);	/* AIN2 */
	else
		pr_warn("AudioPreAmp1_Sel warning");
#endif
	return true;
}


static int Audio_PreAmp1_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] = %d\n",
		__func__,
		mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1];
	return 0;
}

static int Audio_PreAmp1_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] =
	    ucontrol->value.integer.value[0];
	AudioPreAmp1_Sel(mCodec_data->mAudio_Ana_Mux[
	AUDIO_ANALOG_MUX_IN_PREAMP_1]);
	pr_debug("%s() done\n", __func__);
	return 0;
}

static bool AudioPreAmp2_Sel(int Mul_Sel)
{
	pr_debug("%s Mul_Sel = %d\n", __func__, Mul_Sel);
#if 0				/* todo */
	if (Mul_Sel == 0) /* pinumx open */
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x00C0);
	else if (Mul_Sel == 1)
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x00C0, 0x00C0);	/* AIN2 */
	else if (Mul_Sel == 2)
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0080, 0x00C0);	/* AIN1 */
	else if (Mul_Sel == 3)
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0040, 0x00C0);	/* AIN0 */
	else
		pr_warn("AudioPreAmp1_Sel warning");
#endif
	return true;
}


static int Audio_PreAmp2_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2] = %d\n",
		__func__,
		mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2];
	return 0;
}

static int Audio_PreAmp2_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2] =
	    ucontrol->value.integer.value[0];
	AudioPreAmp2_Sel(mCodec_data->mAudio_Ana_Mux[
	AUDIO_ANALOG_MUX_IN_PREAMP_2]);
	pr_debug("%s() done\n", __func__);
	return 0;
}

/* PGA1: PGA_L */
static int Audio_PGA1_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_PGA1_Get = %d\n",
		 mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
	return 0;
}

static int Audio_PGA1_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;
	int index2 = 5;

	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	pr_debug("%s(), index=%d\n", __func__, index);
	/* index = index / 6; */
	if (index > index2)
		index = index2;
	/* const int PreAmpGain[] = {-6, 0, 6, 12, 18, 24}; */
	/* index2 -= index; */
	pr_debug("%s(), index=%d, index2=%d\n", __func__, index, index2);
	Ana_Set_Reg(AUDTOP_CON0, index << 4, 0x00000070);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] =
	    ucontrol->value.integer.value[0];
	return 0;
}

/* PGA2: PGA_R */
static int Audio_PGA2_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_PGA2_Get = %d\n",
		 mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
	return 0;
}

static int Audio_PGA2_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;
	int index2 = 5;

	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	pr_debug("%s(), index=%d\n", __func__, index);
	if (index > index2)
		index = index2;
	/* const int PreAmpGain[] = {-6, 0, 6, 12, 18, 24}; */
	pr_debug("%s(), index=%d\n", __func__, index);
	Ana_Set_Reg(AUDTOP_CON1, index << 8, 0x00000700);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_PGA3_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}

static int Audio_PGA3_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}

static int Audio_PGA4_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}

static int Audio_PGA4_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}

static int Audio_MicSource1_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_MicSource1_Get = %d\n",
		 mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1];
	return 0;
}

static int Audio_MicSource1_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* used for ADC1 Mic source selection,*/
	/*  "ADC1" is main_mic, "ADC2" is headset_mic, "ADC3" is ref main */
	int index = 0;

	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Digital_Mux)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	pr_debug("%s() index = %d done\n", __func__, index);
	mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_MicSource2_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_MicSource2_Get = %d\n",
		 mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2];
	return 0;
}

static int Audio_MicSource2_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Digital_Mux)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	pr_debug("%s() index = %d done\n", __func__, index);
	mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_MicSource3_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}

static int Audio_MicSource3_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}


static int Audio_MicSource4_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}

static int Audio_MicSource4_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* removed */
	return 0;
}

/* Mic ACC/DCC Mode Setting */
static int Audio_Mic1_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() mAudio_Analog_Mic1_mode = %d\n",
		__func__, mAudio_Analog_Mic1_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic1_mode;
	return 0;
}

static int Audio_Mic1_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic1_mode = ucontrol->value.integer.value[0];
	pr_debug("%s() mAudio_Analog_Mic1_mode = %d\n",
		__func__, mAudio_Analog_Mic1_mode);
	return 0;
}

static int Audio_Mic2_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() = %d\n", __func__, mAudio_Analog_Mic2_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic2_mode;
	return 0;
}

static int Audio_Mic2_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic2_mode = ucontrol->value.integer.value[0];
	pr_debug("%s() mAudio_Analog_Mic2_mode = %d\n",
		__func__, mAudio_Analog_Mic2_mode);
	return 0;
}


static int Audio_Mic3_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() = %d\n", __func__, mAudio_Analog_Mic3_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic3_mode;
	return 0;
}

static int Audio_Mic3_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic3_mode = ucontrol->value.integer.value[0];
	pr_debug("%s() mAudio_Analog_Mic3_mode = %d\n",
		__func__, mAudio_Analog_Mic3_mode);
	return 0;
}

static int Audio_Mic4_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() = %d\n", __func__, mAudio_Analog_Mic4_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic4_mode;
	return 0;
}

static int Audio_Mic4_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic4_mode = ucontrol->value.integer.value[0];
	pr_debug("%s() mAudio_Analog_Mic4_mode = %d\n",
		__func__, mAudio_Analog_Mic4_mode);
	return 0;
}

static bool SineTable_DL_DAC_flag;
static bool SineTable_UL_ADC_flag;
static const char * const Pmic_Test_function[] = {"Off", "On"};

static int SineTable_UL_ADC_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() enable = %d\n", __func__,
		ucontrol->value.enumerated.item[0]);

	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Pmic_Test_function)) {
		pr_err("%s return -EINVAL\n", __func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0]) {
		Ana_Set_Reg(ABB_AFE_CON8, 0x8012, 0xFFFF);
		Ana_Set_Reg(ABB_AFE_CON2, 0x0004, 0xFFFF);
	} else {
		Ana_Set_Reg(ABB_AFE_CON8, 0x0, 0xFFFF);
		Ana_Set_Reg(ABB_AFE_CON2, 0x0, 0xFFFF);
	}

	SineTable_UL_ADC_flag = ucontrol->value.integer.value[0];

	return 0;
}

static int SineTable_UL_ADC_Get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = SineTable_UL_ADC_flag;
	return 0;
}

static int SineTable_DL_DAC_Get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = SineTable_DL_DAC_flag;
	return 0;
}

static int SineTable_DL_DAC_Set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() enable = %d\n",
		__func__, ucontrol->value.enumerated.item[0]);

	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Pmic_Test_function)) {
		pr_err("%s return -EINVAL\n", __func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0]) {
		Ana_Set_Reg(ABB_AFE_CON8, 0x9012, 0xFFFF);
		Ana_Set_Reg(ABB_AFE_CON2, 0x0002, 0xFFFF);
	} else {
		Ana_Set_Reg(ABB_AFE_CON8, 0x0, 0xFFFF);
		Ana_Set_Reg(ABB_AFE_CON2, 0x0, 0xFFFF);
	}

	SineTable_DL_DAC_flag = ucontrol->value.integer.value[0];

	return 0;
}

static void Set_Mic_Source(int source) /* 1: headset mic 0: analog mic */
{
	pr_debug("%s source = %s\n", __func__, source?"Headset Mic":"AMIC");

	if (source) {
		mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] = 1;
		mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2] = 1;
	} else {
		mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] = 0;
		mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2] = 2;
	}
}

static void Enable_ADC_In(bool enable)
{
	pr_debug("%s enable = %d\n", __func__, enable);

	if (enable) {
		if (mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] ==
				false) {
			if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
				TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1,
					true);
			else if (mAudio_Analog_Mic1_mode ==
				AUDIO_ANALOGUL_MODE_DCC)
				TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1,
					true);
			else if (mAudio_Analog_Mic1_mode ==
				AUDIO_ANALOGUL_MODE_DMIC)
				TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1,
					true);
			else if (mAudio_Analog_Mic1_mode ==
				AUDIO_ANALOGUL_MODE_DCCECMDIFF ||
				mAudio_Analog_Mic1_mode ==
				AUDIO_ANALOGUL_MODE_DCCECMSINGLE) {
				TurnOnADcPowerDCCECM(
				AUDIO_ANALOG_DEVICE_IN_ADC1,
				true);
			}
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_IN_ADC1] = true;
		}

		if (mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] ==
			false) {
			if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_ACC)
				TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2,
					true);
			else if (mAudio_Analog_Mic2_mode ==
				AUDIO_ANALOGUL_MODE_DCC)
				TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2,
					true);
			else if (mAudio_Analog_Mic2_mode ==
				AUDIO_ANALOGUL_MODE_DMIC)
				TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2,
					true);
			else if (mAudio_Analog_Mic2_mode ==
				AUDIO_ANALOGUL_MODE_DCCECMDIFF ||
				mAudio_Analog_Mic2_mode ==
				AUDIO_ANALOGUL_MODE_DCCECMSINGLE) {
				TurnOnADcPowerDCCECM(
				AUDIO_ANALOG_DEVICE_IN_ADC2,
				true);
				}
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_IN_ADC2] = true;
		}
	} else {
		if (mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1]) {
			mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1]
			= false;
			if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
				TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1,
					false);
			else if (mAudio_Analog_Mic1_mode ==
				AUDIO_ANALOGUL_MODE_DCC)
				TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1,
				false);
			else if (mAudio_Analog_Mic1_mode ==
				AUDIO_ANALOGUL_MODE_DMIC)
				TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1,
					false);
			else if (mAudio_Analog_Mic1_mode ==
				AUDIO_ANALOGUL_MODE_DCCECMDIFF
				|| mAudio_Analog_Mic1_mode ==
				AUDIO_ANALOGUL_MODE_DCCECMSINGLE) {
				TurnOnADcPowerDCCECM(
					AUDIO_ANALOG_DEVICE_IN_ADC1,
					false);
			}
		}

		if (mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2]) {
			mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2]
			= false;
			if (mAudio_Analog_Mic2_mode ==
				AUDIO_ANALOGUL_MODE_ACC)
				TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2,
					false);
			else if (mAudio_Analog_Mic2_mode ==
				AUDIO_ANALOGUL_MODE_DCC)
				TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2,
					false);
			else if (mAudio_Analog_Mic2_mode ==
				AUDIO_ANALOGUL_MODE_DMIC)
				TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2,
					false);
			else if (mAudio_Analog_Mic2_mode ==
				AUDIO_ANALOGUL_MODE_DCCECMDIFF ||
				mAudio_Analog_Mic2_mode ==
				AUDIO_ANALOGUL_MODE_DCCECMSINGLE) {
				TurnOnADcPowerDCCECM(
				AUDIO_ANALOG_DEVICE_IN_ADC2,
				false);
			}
		}
	}
}

static void ADC_LOOP_DAC_Off(void)
{
	pr_debug("%s\n", __func__);

	Enable_ADC_In(false);

	if (mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL]
		== true) {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL]
		= false;
		Speaker_Amp_Change(false);
	}

	if (mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP]
		== true) {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP]
		= false;
		Ext_Speaker_Amp_Change(false);
	}

	if (mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTHPAMP]
		== true) {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTHPAMP]
		= false;
		Ext_Headphone_Amp_Change(false);
	}

	if (mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
		== true) {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
		= false;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
	}
	if (mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
		== true) {
		mCodec_data->
		mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
		= false;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
	}
}


static void ADC_LOOP_DAC_Func(int mode)
{
	pr_debug("%s value = %d\n", __func__, mode);

	mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = 48000;
	mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = 48000;

	switch (mode) {
	case AUDIO_ANALOG_HEADSET_MIC_LOOP_SPEAKER:
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] = 5;
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] = 5;

		mAudio_Analog_Mic1_mode  = AUDIO_ANALOGUL_MODE_ACC;
		mAudio_Analog_Mic2_mode  = AUDIO_ANALOGUL_MODE_ACC;

		Set_Mic_Source(1); /* 1 for headset mic */
		Enable_ADC_In(true);

#ifdef CONFIG_MTK_SPEAKER
		Speaker_pga_gain = 11;

		if (mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL]
			== false) {
			Speaker_Amp_Change(true);
			mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL]
			= true;
		}
#else
		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH]
			== true) {
			Headphone_Speaker_Switch_Change(false);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH] = false;
		}

		if (mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
			== false) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
			mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
			= true;
		}
		if (mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
			== false) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
			mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
			= true;
		}

		AudDrv_GPIO_EXTAMP_Gain_Set(3);

		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP]
			== false) {
			Ext_Speaker_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP] = true;
		}
#endif
		break;
	case AUDIO_ANALOG_HEADSET_MIC_LOOP_HP:
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] = 5;
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] = 5;

		mAudio_Analog_Mic1_mode  = AUDIO_ANALOGUL_MODE_ACC;
		mAudio_Analog_Mic2_mode  = AUDIO_ANALOGUL_MODE_ACC;

		Set_Mic_Source(1); /* 1 for headset mic */
		Enable_ADC_In(true);

		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] = 0;
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = 0;

#ifndef CONFIG_MTK_SPEAKER
		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH]
			== false) {
			Headphone_Speaker_Switch_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH] = true;
		}
#endif

		if (mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
			== false) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
			mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
			= true;
		}
		if (mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
			== false) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
			mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
			= true;
		}

		if (mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTHPAMP]
			== false) {
			Ext_Headphone_Amp_Change(true);
			mCodec_data->
			mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTHPAMP]
			= true;
		}
		break;
	case AUDIO_ANALOG_AMIC_LOOP_SPEAKER:
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] = 5;
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] = 5;

		mAudio_Analog_Mic1_mode  = AUDIO_ANALOGUL_MODE_ACC;
		mAudio_Analog_Mic2_mode  = AUDIO_ANALOGUL_MODE_ACC;

		Set_Mic_Source(0); /* 0 for analog mic */
		Enable_ADC_In(true);

#ifdef CONFIG_MTK_SPEAKER
		Speaker_pga_gain = 11;

		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_SPEAKERL]
			== false) {
			Speaker_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] = true;
		}
#else
		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH]
			== true) {
			Headphone_Speaker_Switch_Change(false);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH] = false;
		}

		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
			== false) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = true;
		}
		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
			== false) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = true;
		}

		AudDrv_GPIO_EXTAMP_Gain_Set(3);

		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP]
			== false) {
			Ext_Speaker_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP] = true;
		}
#endif
		break;
	case AUDIO_ANALOG_AMIC_LOOP_HP:
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] = 5;
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] = 5;

		mAudio_Analog_Mic1_mode  = AUDIO_ANALOGUL_MODE_ACC;
		mAudio_Analog_Mic2_mode  = AUDIO_ANALOGUL_MODE_ACC;

		Set_Mic_Source(0); /* 0 for analog mic */
		Enable_ADC_In(true);

		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] = 0;
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = 0;

#ifndef CONFIG_MTK_SPEAKER
		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH]
			== false) {
			Headphone_Speaker_Switch_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_HP_SPK_SWITCH] = true;
		}
#endif

		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETL]
			== false) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = true;
		}
		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETR]
			== false) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = true;
		}

		if (mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_EXTHPAMP]
			== false) {
			Ext_Headphone_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[
			AUDIO_ANALOG_DEVICE_OUT_EXTHPAMP] = true;
		}
		break;
	case AUDIO_ANALOG_LOOPBACK_OFF:
		ADC_LOOP_DAC_Off();
		break;
	default:
		pr_warn("unsupported codec loopback mode!!!\n");
	}
}

static const char * const Pmic_Loopback_Mode[] = {
	"OFF", "AMIC_TO_HP", "AMIC_TO_SPK",
	"HEADSET_MIC_TO_HP", "HEADSET_MIC_TO_SPK"};
static int Codec_Loopback_Mode;

static int Codec_Loopback_Mode_Get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = Codec_Loopback_Mode;
	return 0;
}

static int Codec_Loopback_Mode_Set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Pmic_Loopback_Mode)) {
		pr_err("%s return -EINVAL\n", __func__);
		return -EINVAL;
	}

	Codec_Loopback_Mode = ucontrol->value.integer.value[0];

	pr_debug("%s() Codec_Loopback_Mode = %d\n",
		__func__, Codec_Loopback_Mode);

	ADC_LOOP_DAC_Func(Codec_Loopback_Mode);

	return 0;
}


static const struct soc_enum Pmic_Test_Enum[] = {

	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Loopback_Mode), Pmic_Loopback_Mode),
};

static const struct snd_kcontrol_new mt6323_pmic_Test_controls[] = {

	SOC_ENUM_EXT("SineTable_DL_DAC", Pmic_Test_Enum[0],
		SineTable_DL_DAC_Get, SineTable_DL_DAC_Set),
	SOC_ENUM_EXT("SineTable_UL_ADC", Pmic_Test_Enum[1],
		SineTable_UL_ADC_Get, SineTable_UL_ADC_Set),
	SOC_ENUM_EXT("Codec_Loopback_Setting", Pmic_Test_Enum[2],
		Codec_Loopback_Mode_Get, Codec_Loopback_Mode_Set),
};

static const struct snd_kcontrol_new mt6323_UL_Codec_controls[] = {

	SOC_ENUM_EXT("Audio_Preamp1_Switch",
		Audio_UL_Enum[0], Audio_PreAmp1_Get, Audio_PreAmp1_Set),
	SOC_ENUM_EXT("Audio_Preamp2_Switch",
		Audio_UL_Enum[1], Audio_PreAmp2_Get, Audio_PreAmp2_Set),
	SOC_ENUM_EXT("Audio_ADC_1_Switch",
		Audio_UL_Enum[2], Audio_ADC1_Get, Audio_ADC1_Set),
	SOC_ENUM_EXT("Audio_ADC_2_Switch",
		Audio_UL_Enum[3], Audio_ADC2_Get, Audio_ADC2_Set),
	SOC_ENUM_EXT("Audio_ADC_3_Switch",
		Audio_UL_Enum[4], Audio_ADC3_Get, Audio_ADC3_Set),
	SOC_ENUM_EXT("Audio_ADC_4_Switch",
		Audio_UL_Enum[5], Audio_ADC4_Get, Audio_ADC4_Set),
	SOC_ENUM_EXT("Audio_PGA1_Setting",
		Audio_UL_Enum[6], Audio_PGA1_Get, Audio_PGA1_Set),
	SOC_ENUM_EXT("Audio_PGA2_Setting",
		Audio_UL_Enum[7], Audio_PGA2_Get, Audio_PGA2_Set),
	SOC_ENUM_EXT("Audio_PGA3_Setting",
		Audio_UL_Enum[8], Audio_PGA3_Get, Audio_PGA3_Set),
	SOC_ENUM_EXT("Audio_PGA4_Setting",
		Audio_UL_Enum[9], Audio_PGA4_Get, Audio_PGA4_Set),
	SOC_ENUM_EXT("Audio_MicSource1_Setting",
		Audio_UL_Enum[10], Audio_MicSource1_Get,
		     Audio_MicSource1_Set),
	SOC_ENUM_EXT("Audio_MicSource2_Setting",
		Audio_UL_Enum[11], Audio_MicSource2_Get,
		     Audio_MicSource2_Set),
	SOC_ENUM_EXT("Audio_MicSource3_Setting",
		Audio_UL_Enum[12], Audio_MicSource3_Get,
		     Audio_MicSource3_Set),
	SOC_ENUM_EXT("Audio_MicSource4_Setting",
		Audio_UL_Enum[13], Audio_MicSource4_Get,
		     Audio_MicSource4_Set),
	SOC_ENUM_EXT("Audio_MIC1_Mode_Select",
		Audio_UL_Enum[14], Audio_Mic1_Mode_Select_Get,
		     Audio_Mic1_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC2_Mode_Select",
		Audio_UL_Enum[15], Audio_Mic2_Mode_Select_Get,
		     Audio_Mic2_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC3_Mode_Select",
		Audio_UL_Enum[16], Audio_Mic3_Mode_Select_Get,
		     Audio_Mic3_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC4_Mode_Select",
		Audio_UL_Enum[17], Audio_Mic4_Mode_Select_Get,
		     Audio_Mic4_Mode_Select_Set),
};

static const struct snd_soc_dapm_widget mt6323_dapm_widgets[] = {

	/* Outputs */
	SND_SOC_DAPM_OUTPUT("EARPIECE"),
	SND_SOC_DAPM_OUTPUT("HEADSET"),
	SND_SOC_DAPM_OUTPUT("SPEAKER"),
#if 0
	   SND_SOC_DAPM_MUX_E("VOICE_Mux_E", SND_SOC_NOPM, 0,
	   0, &mt6323_Voice_Switch, codec_enable_rx_bias,
	   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
	   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
	   SND_SOC_DAPM_PRE_REG | SND_SOC_DAPM_POST_REG),
#endif

};

static const struct snd_soc_dapm_route mtk_audio_map[] = {

	{"VOICE_Mux_E", "Voice Mux", "SPEAKER PGA"},
};

static void mt6323_codec_init_reg(struct snd_soc_codec *codec)
{
	pr_debug("%s\n", __func__);
	/* need to set this when boot up */
	AudDrv_Clk_On();
	/* Set UL PGA L MUX as open */
	Ana_Set_Reg(AUDTOP_CON0, 0x0002, 0x000F);
	/* Set UL PGA R MUX as open */
	Ana_Set_Reg(AUDTOP_CON1, 0x0020, 0x00F0);
	/* Set audio DAC Bias to 50% */
	Ana_Set_Reg(AUDTOP_CON5, 0x1114, 0xFFFF);
	Ana_Set_Reg(AUDTOP_CON6, 0x37A2, 0xFFFF);
	/* Enable the depop MUX of HP drivers */
	Ana_Set_Reg(AUDTOP_CON6, 0x37E2, 0xFFFF);
	AudDrv_Clk_Off();
}

void InitCodecDefault(void)
{
	pr_debug("%s\n", __func__);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP3] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP4] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] = 8;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = 8;
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1] =
	    AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2] =
	    AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC3] =
	    AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC4] =
	    AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
}

static int mt6323_codec_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->component.dapm;

	pr_debug("%s()\n", __func__);
	if (mInitCodec == true)
		return 0;
	snd_soc_dapm_new_controls(dapm, mt6323_dapm_widgets,
		ARRAY_SIZE(mt6323_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, mtk_audio_map, ARRAY_SIZE(mtk_audio_map));
	/* add codec controls */
	snd_soc_add_codec_controls(codec, mt6323_snd_controls,
		ARRAY_SIZE(mt6323_snd_controls));
	snd_soc_add_codec_controls(codec, mt6323_UL_Codec_controls,
		ARRAY_SIZE(mt6323_UL_Codec_controls));
	snd_soc_add_codec_controls(codec, mt6323_Voice_Switch,
		ARRAY_SIZE(mt6323_Voice_Switch));
	snd_soc_add_codec_controls(codec, mt6323_pmic_Test_controls,
		ARRAY_SIZE(mt6323_pmic_Test_controls));
#ifdef CONFIG_MTK_SPEAKER
	snd_soc_add_codec_controls(codec, mt6323_snd_Speaker_controls,
				   ARRAY_SIZE(mt6323_snd_Speaker_controls));
#endif

	/* here to set  private data */
	mCodec_data = kzalloc(sizeof(struct mt6323_Codec_Data_Priv),
		GFP_KERNEL);

#if 0
	if (!mCodec_data) {
		pr_err("Failed to allocate private data\n");
		return -ENOMEM;
	}
#endif

	snd_soc_codec_set_drvdata(codec, mCodec_data);
	memset((void *)mCodec_data, 0, sizeof(struct mt6323_Codec_Data_Priv));
	mt6323_codec_init_reg(codec);
	InitCodecDefault();
	mInitCodec = true;
	return 0;
}

static int mt6323_codec_remove(struct snd_soc_codec *codec)
{
	pr_debug("%s()\n", __func__);
	return 0;
}

static unsigned int mt6323_read(struct snd_soc_codec *codec, unsigned int reg)
{
	pr_debug("mt6323_read reg = 0x%x", reg);
	Ana_Get_Reg(reg);
	return 0;
}

static int mt6323_write(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int value)
{
	pr_debug("mt6323_write reg = 0x%x  value= 0x%x\n", reg, value);
	Ana_Set_Reg(reg, value, 0xffffffff);
	return 0;
}

static struct snd_soc_codec_driver soc_mtk_codec = {

	.probe = mt6323_codec_probe,
	.remove = mt6323_codec_remove,

	.read = mt6323_read,
	.write = mt6323_write,

	/* use add control to replace */
	/* .controls = mt6323_snd_controls, */
	/* .num_controls = ARRAY_SIZE(mt6323_snd_controls), */
#if 0
	.dapm_widgets = mt6323_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6323_dapm_widgets),
	.dapm_routes = mtk_audio_map,
	.num_dapm_routes = ARRAY_SIZE(mtk_audio_map),
#endif
};

static int mtk_mt6323_codec_dev_probe(struct platform_device *pdev)
{
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (pdev->dev.dma_mask == NULL)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_CODEC_NAME);
	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_codec(&pdev->dev,
				      &soc_mtk_codec, mtk_6323_dai_codecs,
				      ARRAY_SIZE(mtk_6323_dai_codecs));
}

static int mtk_mt6323_codec_dev_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_codec_63xx_of_ids[] = {

	{.compatible = "mediatek,mt8163-soc-codec-63xx",},
	{}
};

#if 0
static int Auddrv_getGPIO_info(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL,
		"mediatek, mt8163-soc-codec-63xx");
	if (node) {
		if (of_property_read_u32_index(node, "extspkamp-gpio",
			0, &pin_extspkamp)) {
			if_config = 0;
			pr_err("extspkamp-gpio get pin fail!!!\n");
			return -1;
		}
		if (of_property_read_u32_index(node, "extspkamp-gpio",
			1, &pin_mode_extspkamp)) {
			if_config = 0;
			pr_err("extspkamp-gpio get pin_mode fail!!!\n");
			return -1;
		}
		if (of_property_read_u32_index(node, "vowclk-gpio",
			0, &pin_vowclk)) {
			if_config = 0;
			pr_err("vowclk-gpio get pin fail!!!\n");
			return -1;
		}
		if (of_property_read_u32_index(node, "vowclk-gpio",
			1, &pin_mode_vowclk)) {
			if_config = 0;
			pr_err("vowclk-gpio get pin_mode fail!!!\n");
			return -1;
		}
		if (of_property_read_u32_index(node, "audmiso-gpio",
			0, &pin_audmiso)) {
			if_config = 0;
			pr_err("audmiso-gpio get pin fail!!!\n");
			return -1;
		}
		if (of_property_read_u32_index(node, "audmiso-gpio",
			1, &pin_mode_audmiso)) {
			if_config = 0;
			pr_err("audmiso-gpio get pin_mode fail!!!\n");
			return -1;
		}
	} else {
		pr_err("[mt_soc_codec_63xx] node NULL, can't Auddrv_getGPIO_info!!!\n");
		return -1;
	}
	return 0;
}
#endif
#endif

static struct platform_driver mtk_codec_6323_driver = {

	.driver = {
		   .name = MT_SOC_CODEC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_codec_63xx_of_ids,
#endif
		   },
	.probe = mtk_mt6323_codec_dev_probe,
	.remove = mtk_mt6323_codec_dev_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_codec6323_dev;
#endif

static int __init mtk_mt6323_codec_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
#ifdef CONFIG_OF
	/* Auddrv_getGPIO_info(); */
#else
	soc_mtk_codec6323_dev = platform_device_alloc(MT_SOC_CODEC_NAME, -1);
	if (!soc_mtk_codec6323_dev)
		return -ENOMEM;
	ret = platform_device_add(soc_mtk_codec6323_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_codec6323_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_codec_6323_driver);
	return ret;
}
module_init(mtk_mt6323_codec_init);

static void __exit mtk_mt6323_codec_exit(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&mtk_codec_6323_driver);
}
module_exit(mtk_mt6323_codec_exit);

/* Module information */
MODULE_DESCRIPTION("MTK  codec driver");
MODULE_LICENSE("GPL v2");
