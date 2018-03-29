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
 *   mtk_soc_codec_63xx
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   Audio codec stub file
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
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#if !defined(CONFIG_MTK_LEGACY)
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#else
#include <mt-plat/mt_gpio.h>
#endif

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "mt_soc_analog_type.h"
#include <mach/mt_clkbuf_ctl.h>


#include <sound/mt_soc_audio.h>
#include "mt_soc_afe_control.h"
#include <mt-plat/upmu_common.h>

#ifdef CONFIG_MTK_SPEAKER
#include "mt_soc_codec_speaker_63xx.h"
#endif

#include "mt_soc_pcm_common.h"

/* AW8736 PA output power mode control */
/* #define AW8736_MODE_CTRL */

/* static function declaration */
static bool AudioPreAmp1_Sel(int Mul_Sel);
static bool GetAdcStatus(void);
static void Apply_Speaker_Gain(void);
static bool TurnOnVOWDigitalHW(bool enable);
static void TurnOffDacPower(void);
static void TurnOnDacPower(void);
/* static void SetDcCompenSation(void); */
static void Voice_Amp_Change(bool enable);
static void Speaker_Amp_Change(bool enable);
static bool TurnOnVOWADcPowerACC(int MicType, bool enable);

static mt6350_Codec_Data_Priv *mCodec_data;
static uint32 mBlockSampleRate[AUDIO_ANALOG_DEVICE_INOUT_MAX] = { 48000, 48000, 48000 };

static DEFINE_MUTEX(Ana_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_buf_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_Clk_Mutex);
static DEFINE_MUTEX(Ana_Power_Mutex);
static DEFINE_MUTEX(AudAna_lock);

static int mAudio_Analog_Mic1_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic2_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic3_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic4_mode = AUDIO_ANALOGUL_MODE_ACC;

static int mAudio_Vow_Analog_Func_Enable;
static int mAudio_Vow_Digital_Func_Enable;

/* static int TrimOffset = 2048; */
static const int DC1unit_in_uv = 19184;	/* in uv with 0DB */
static const int DC1devider = 8;	/* in uv */
/* Headphone DC calibration */
static int mHpLeftDcCalibration;
static int mHpRightDcCalibration;

#ifdef RAINIER_NEED_CHECK
static uint32 RG_AUDHPLTRIM_VAUDP15, RG_AUDHPRTRIM_VAUDP15, RG_AUDHPLFINETRIM_VAUDP15,
	RG_AUDHPRFINETRIM_VAUDP15, RG_AUDHPLTRIM_VAUDP15_SPKHP, RG_AUDHPRTRIM_VAUDP15_SPKHP,
	RG_AUDHPLFINETRIM_VAUDP15_SPKHP, RG_AUDHPRFINETRIM_VAUDP15_SPKHP;
#endif

static int Speaker_mode = AUDIO_SPEAKER_MODE_D;	/* default use type D on 82 */
#ifdef CONFIG_MTK_SPEAKER
static unsigned int Speaker_pga_gain = 1;	/* default 0Db. */
static bool mSpeaker_Ocflag;
#endif
static int mAdc_Power_Mode;
static unsigned int dAuxAdcChannel = 16;
static const int mDcOffsetTrimChannel = 9;
static bool mInitCodec;

static int reg_AFE_VOW_CFG0 = 0x0000;	/* VOW AMPREF Setting */
static int reg_AFE_VOW_CFG1 = 0x0000;	/* VOW A,B timeout initial value (timer) */
static int reg_AFE_VOW_CFG2 = 0x2222;	/* VOW A,B value setting (BABA) */
static int reg_AFE_VOW_CFG3 = 0x8767;	/* alhpa and beta K value setting (beta_rise,fall,alpha_rise,fall) */
static int reg_AFE_VOW_CFG4 = 0x006E;	/* gamma K value setting (gamma), bit4:8 should not modify */
static int reg_AFE_VOW_CFG5 = 0x0001;	/* N mini value setting (Nmin) */
static bool mIsVOWOn;
static bool mIsExtSPKUse;

/* VOW using */
typedef enum {
	AUDIO_VOW_MIC_TYPE_Handset_AMIC = 0,
	AUDIO_VOW_MIC_TYPE_Headset_MIC,
	AUDIO_VOW_MIC_TYPE_Handset_DMIC,	/* 1P6 */
	AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K,	/* 800K */
	AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCC,	/* DCC mems */
	AUDIO_VOW_MIC_TYPE_Headset_MIC_DCC,
	AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCCECM,	/* DCC ECM, dual differential */
	AUDIO_VOW_MIC_TYPE_Headset_MIC_DCCECM	/* DCC ECM, signal differential */
} AUDIO_VOW_MIC_TYPE;

static int mAudio_VOW_Mic_type = AUDIO_VOW_MIC_TYPE_Handset_AMIC;
static void Audio_Amp_Change(int channels, bool enable);
static void SavePowerState(void)
{
	int i = 0;

	for (i = 0; i < AUDIO_ANALOG_DEVICE_MAX; i++) {
		mCodec_data->mAudio_BackUpAna_DevicePower[i] =
		    mCodec_data->mAudio_Ana_DevicePower[i];
	}
}

static void RestorePowerState(void)
{
	int i = 0;

	for (i = 0; i < AUDIO_ANALOG_DEVICE_MAX; i++) {
		mCodec_data->mAudio_Ana_DevicePower[i] =
		    mCodec_data->mAudio_BackUpAna_DevicePower[i];
	}
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

	for (i = AUDIO_ANALOG_DEVICE_2IN1_SPK; i <= AUDIO_ANALOG_DEVICE_IN_DIGITAL_MIC; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}


static bool mAnaSuspend;
void SetAnalogSuspend(bool bEnable)
{
	pr_warn("%s bEnable ==%d mAnaSuspend = %d\n", __func__, bEnable, mAnaSuspend);
	if ((bEnable == true) && (mAnaSuspend == false)) {
		/*Ana_Log_Print();*/
		SavePowerState();
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == true) {
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
			    false;
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == true) {
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
			    false;
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] == true) {
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] =
			    false;
			Voice_Amp_Change(false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] == true) {
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] =
			    false;
			Speaker_Amp_Change(false);
		}
		/*Ana_Log_Print();*/
		mAnaSuspend = true;
	} else if ((bEnable == false) && (mAnaSuspend == true)) {
		/*Ana_Log_Print();*/
		if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] ==
		    true) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
			    true;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] ==
		    true) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
			    false;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] ==
		    true) {
			Voice_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] =
			    false;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] ==
		    true) {
			Speaker_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] =
			    false;
		}
		RestorePowerState();
		/*Ana_Log_Print();*/
		mAnaSuspend = false;
	}
}

static int audck_buf_Count;
void audckbufEnable(bool enable)
{
	pr_warn("audckbufEnable audck_buf_Count = %d enable = %d\n", audck_buf_Count, enable);
	mutex_lock(&Ana_buf_Ctrl_Mutex);
	if (enable) {
		if (audck_buf_Count == 0) {
#ifdef CONFIG_FPGA_EARLY_PORTING
			pr_warn("fpga bypass clk_buf_ctrl(CLK_BUF_AUDIO,true)\n");
#else
			pr_warn("+clk_buf_ctrl(CLK_BUF_AUDIO,true)\n");
			/* system 26M clk from RF */
			clk_buf_ctrl(CLK_BUF_AUDIO, true);
			pr_warn("-clk_buf_ctrl(CLK_BUF_AUDIO,true)\n");
#endif
		}
		audck_buf_Count++;
	} else {
		audck_buf_Count--;
		if (audck_buf_Count == 0) {
#ifdef CONFIG_FPGA_EARLY_PORTING
			pr_warn("fpga bypass clk_buf_ctrl(CLK_BUF_AUDIO,false)\n");
#else
			pr_warn("+clk_buf_ctrl(CLK_BUF_AUDIO,false)\n");
			clk_buf_ctrl(CLK_BUF_AUDIO, false);
			pr_warn("-clk_buf_ctrl(CLK_BUF_AUDIO,false)\n");
#endif
		}
		if (audck_buf_Count < 0) {
			pr_warn("audck_buf_Count count <0\n");
			audck_buf_Count = 0;
		}
	}
	mutex_unlock(&Ana_buf_Ctrl_Mutex);
}

static int ClsqCount;
static void ClsqEnable(bool enable)
{
	pr_debug("ClsqEnable not support ClsqCount = %d enable = %d\n", ClsqCount, enable);
}

static int TopCkCount;
static void Topck_Enable(bool enable)
{
	pr_warn("Topck_Enable enable = %d TopCkCount = %d\n", enable, TopCkCount);

	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (TopCkCount == 0)
			Ana_Set_Reg(TOP_CKPDN1_CLR, 0x0100, 0x0100);	/* AUD 26M clock power down release */

		TopCkCount++;
	} else {
		TopCkCount--;
		if (TopCkCount == 0)
			Ana_Set_Reg(TOP_CKPDN1_SET, 0x0100, 0x0100);	/* AUD 26M clock power down */

		if (TopCkCount <= 0) {
			pr_warn("TopCkCount <0 =%d\n ", TopCkCount);
			TopCkCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);

	pr_warn("Topck_Enable enable-\n");

}

static int NvRegCount;
static void NvregEnable(bool enable)
{
	pr_debug("NvregEnable not support NvRegCount == %d enable = %d\n", NvRegCount, enable);
}
static void TopCtlChangeTrigger(void)
{
	uint32 top_ctrl_status_now = Ana_Get_Reg(ABB_AFE_CON11);

	Ana_Set_Reg(ABB_AFE_CON11, ((top_ctrl_status_now & 0x0001) ? 0 : 1) << 8, 0x0100);
}

static void DCChangeTrigger(void)
{
	uint32_t dc_status_now = Ana_Get_Reg(0x4016);

	Ana_Set_Reg(0x4016, ((dc_status_now & 0x0002) ? 0 : 1) << 9, 0x0200);
	/* return NO_ERROR; */
}

/* extern kal_uint32 upmu_get_reg_value(kal_uint32 reg); */

void Auddrv_Read_Efuse_HPOffset(void)
{
	pr_debug("Auddrv_Read_Efuse_HPOffset(+)\n");
#ifdef RAINIER_NEED_CHECK
	U32 ret = 0;
	U32 reg_val = 0;
	int i = 0, j = 0;
	U32 efusevalue[3];

	/* 1. enable efuse ctrl engine clock */
	ret = pmic_config_interface(0x026C, 0x0040, 0xFFFF, 0);
	ret = pmic_config_interface(0x024E, 0x0004, 0xFFFF, 0);

	/* 2. */
	ret = pmic_config_interface(0x0C16, 0x1, 0x1, 0);

	/*
	   Audio data from 746 to 770
	   0xe 746 751
	   0xf 752 767
	   0x10 768 770
	 */

	for (i = 0xe; i <= 0x10; i++) {
		/* 3. set row to read */
		ret = pmic_config_interface(0x0C00, i, 0x1F, 1);

		/* 4. Toggle */
		ret = pmic_read_interface(0xC10, &reg_val, 0x1, 0);
		if (reg_val == 0)
			ret = pmic_config_interface(0xC10, 1, 0x1, 0);
		else
			ret = pmic_config_interface(0xC10, 0, 0x1, 0);

		/* 5. polling Reg[0xC1A] */
		reg_val = 1;
		while (reg_val == 1) {
			ret = pmic_read_interface(0xC1A, &reg_val, 0x1, 0);
			pr_debug("Auddrv_Read_Efuse_HPOffset polling 0xC1A=0x%x\n", reg_val);
		}

		udelay(1000);	/* Need to delay at least 1ms for 0xC1A and than can read 0xC18 */

		/* 6. read data */
		efusevalue[j] = upmu_get_reg_value(0x0C18);
		pr_debug("HPoffset : efuse[%d]=0x%x\n", j, efusevalue[j]);
		j++;
	}

	/* 7. Disable efuse ctrl engine clock */
	ret = pmic_config_interface(0x024C, 0x0004, 0xFFFF, 0);
	ret = pmic_config_interface(0x026A, 0x0040, 0xFFFF, 0);

	RG_AUDHPLTRIM_VAUDP15 = (efusevalue[0] >> 10) & 0xf;
	RG_AUDHPRTRIM_VAUDP15 = ((efusevalue[0] >> 14) & 0x3) + ((efusevalue[1] & 0x3) << 2);
	RG_AUDHPLFINETRIM_VAUDP15 = (efusevalue[1] >> 3) & 0x3;
	RG_AUDHPRFINETRIM_VAUDP15 = (efusevalue[1] >> 5) & 0x3;
	RG_AUDHPLTRIM_VAUDP15_SPKHP = (efusevalue[1] >> 7) & 0xF;
	RG_AUDHPRTRIM_VAUDP15_SPKHP = (efusevalue[1] >> 11) & 0xF;
	RG_AUDHPLFINETRIM_VAUDP15_SPKHP =
	    ((efusevalue[1] >> 15) & 0x1) + ((efusevalue[2] & 0x1) << 1);
	RG_AUDHPRFINETRIM_VAUDP15_SPKHP = ((efusevalue[2] >> 1) & 0x3);

	pr_debug("RG_AUDHPLTRIM_VAUDP15 = %x\n", RG_AUDHPLTRIM_VAUDP15);
	pr_debug("RG_AUDHPRTRIM_VAUDP15 = %x\n", RG_AUDHPRTRIM_VAUDP15);
	pr_debug("RG_AUDHPLFINETRIM_VAUDP15 = %x\n", RG_AUDHPLFINETRIM_VAUDP15);
	pr_debug("RG_AUDHPRFINETRIM_VAUDP15 = %x\n", RG_AUDHPRFINETRIM_VAUDP15);
	pr_debug("RG_AUDHPLTRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPLTRIM_VAUDP15_SPKHP);
	pr_debug("RG_AUDHPRTRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPRTRIM_VAUDP15_SPKHP);
	pr_debug("RG_AUDHPLFINETRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPLFINETRIM_VAUDP15_SPKHP);
	pr_debug("RG_AUDHPRFINETRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPRFINETRIM_VAUDP15_SPKHP);
#else
	pr_debug("Auddrv_Read_Efuse_HPOffset not support\n");
#endif
	pr_debug("Auddrv_Read_Efuse_HPOffset(-)\n");
}
EXPORT_SYMBOL(Auddrv_Read_Efuse_HPOffset);

#ifdef CONFIG_MTK_SPEAKER
static void Apply_Speaker_Gain(void)
{
	int index = Speaker_pga_gain;
	uint32 currentIdx, i;

	pr_warn("%s Speaker_pga_gain= %d\n", __func__, Speaker_pga_gain);

	if (index > 11)
		index = 11;

	if (index < 1)
		index = 1;	/* min to 0dB */

	currentIdx = (Ana_Get_Reg(SPK_CON9) >> 8) & 0xF;
	pr_warn("%s(), index = %d, currentIdx=%d\n", __func__, index, currentIdx);

	if (index > currentIdx) {
		for (i = (currentIdx + 1); i <= index; i++) {
			Ana_Set_Reg(SPK_CON9, i << 8, 0x0F00);
			/* msleep(3); */
			mdelay(3);
		}
	} else if ((index < currentIdx) && (currentIdx != 0)) {
		for (i = currentIdx - 1; i >= index; i--) {
			Ana_Set_Reg(SPK_CON9, i << 8, 0x0F00);
			/* msleep(3); */
			mdelay(3);
		}
	}
	pr_warn("%s--\n", __func__);
}
#else
static void Apply_Speaker_Gain(void)
{
}
#endif

void setHpDcCalibration(unsigned int type, int dc_cali_value)
{
	pr_warn("%s ,type=%d, dc_cali_value = %d\n", __func__, type, dc_cali_value);
	if (type == AUDIO_ANALOG_DEVICE_OUT_HEADSETR) {
		/* ABB_AFE_CON3, 1 step is (1/32768) * 1800mV = 0.0549 */
		/* mHpRightDcCalibration = dc_cali_value/0.0549 (= dc_cali_value*18 ) */
		mHpRightDcCalibration = dc_cali_value * 18;
	} else if (type == AUDIO_ANALOG_DEVICE_OUT_HEADSETL)
		mHpLeftDcCalibration = dc_cali_value * 18;
	else
		pr_warn("%s, wrong type, invalid operation\n", __func__);
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

	Ana_Set_Reg(AUDDEC_ANA_CON3, gain << 5, 0x3 << 5);	/* Audio offset trimming buffer gain selection */
#endif
}

#if 0
static int mHplTrimOffset = 2048;
static int mHprTrimOffset = 2048;
#endif

void SetHplTrimOffset(int Offset)
{
	pr_warn("%s Offset = %d\n", __func__, Offset);
	/* transform to 1.8V scale */
	setHpDcCalibration(AUDIO_ANALOG_DEVICE_OUT_HEADSETL, (Offset * 18) / 10);
}

void SetHprTrimOffset(int Offset)
{
	pr_warn("%s Offset = %d\n", __func__, Offset);
	/* transform to 1.8V scale */
	setHpDcCalibration(AUDIO_ANALOG_DEVICE_OUT_HEADSETR, (Offset * 18) / 10);
}

void EnableTrimbuffer(bool benable)
{
	pr_warn("%s , no this\n", __func__);
}


void OpenTrimBufferHardware(bool enable)	/*  */
{
	pr_warn("%s , no this\n", __func__);
}

void OpenAnalogTrimHardware(bool enable)
{
	pr_warn("%s , no this\n", __func__);
}

void setHpDcCalibrationGain(unsigned int type, int gain_value)
{
	uint32 index = 7;

	pr_warn("%s ,type=%d, gain_value = %d\n", __func__, type, gain_value);
	/* this will base on hw spec. */
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
		pr_warn("%s, wrong type, invalid operation\n", __func__);
}


void OpenAnalogHeadphone(bool bEnable)
{
	pr_warn("OpenAnalogHeadphone bEnable = %d", bEnable);
	if (bEnable) {
		/* SetHplTrimOffset(2048); */
		/* SetHprTrimOffset(2048); */
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = 44100;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = true;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = true;
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = false;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = false;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
	}
}

bool OpenHeadPhoneImpedanceSetting(bool bEnable)
{
	pr_warn("%s, not support  benable = %d\n", __func__, bEnable);
	return true;
}

void setHpGainZero(void)
{
	pr_warn("%s , no this\n", __func__);
}

void SetSdmLevel(unsigned int level)
{
#if 0				/* Todo */
	Ana_Set_Reg(AFE_DL_SDM_CON1, level, 0xffffffff);
#endif
}

#if 0				/* Todo */
static void SetHprOffset(int OffsetTrimming)
{
	short Dccompsentation = 0;
	int DCoffsetValue = 0;
	unsigned short RegValue = 0;

	pr_warn("%s OffsetTrimming = %d\n", __func__, OffsetTrimming);
	DCoffsetValue = OffsetTrimming * 1000000;
	DCoffsetValue = (DCoffsetValue / DC1devider);	/* in uv */
	pr_warn("%s DCoffsetValue = %d\n", __func__, DCoffsetValue);
	DCoffsetValue = (DCoffsetValue / DC1unit_in_uv);
	pr_warn("%s DCoffsetValue = %d\n", __func__, DCoffsetValue);
	Dccompsentation = DCoffsetValue;
	RegValue = Dccompsentation;
	pr_warn("%s RegValue = 0x%x\n", __func__, RegValue);

	Ana_Set_Reg(AFE_DL_DC_COMP_CFG1, RegValue, 0xffff);
}
#endif

#if 0				/* Todo */
static void SetHplOffset(int OffsetTrimming)
{
	short Dccompsentation = 0;
	int DCoffsetValue = 0;
	unsigned short RegValue = 0;

	pr_warn("%s OffsetTrimming = %d\n", __func__, OffsetTrimming);
	DCoffsetValue = OffsetTrimming * 1000000;
	DCoffsetValue = (DCoffsetValue / DC1devider);	/* in uv */
	pr_warn("%s DCoffsetValue = %d\n", __func__, DCoffsetValue);
	DCoffsetValue = (DCoffsetValue / DC1unit_in_uv);
	pr_warn("%s DCoffsetValue = %d\n", __func__, DCoffsetValue);
	Dccompsentation = DCoffsetValue;
	RegValue = Dccompsentation;
	pr_warn("%s RegValue = 0x%x\n", __func__, RegValue);

	Ana_Set_Reg(AFE_DL_DC_COMP_CFG0, RegValue, 0xffff);
}
#endif

#if 0				/* Todo */
static void EnableDcCompensation(bool bEnable)
{
#ifndef EFUSE_HP_TRIM
	Ana_Set_Reg(AFE_DL_DC_COMP_CFG2, bEnable, 0x1);
#endif
}
#endif

#if 0
static void SetHprOffsetTrim(void)
{
	int OffsetTrimming = mHprTrimOffset - TrimOffset;

	pr_warn("%s mHprTrimOffset = %d TrimOffset = %d\n", __func__, mHprTrimOffset, TrimOffset);
	SetHprOffset(OffsetTrimming);
}

static void SetHpLOffsetTrim(void)
{
	int OffsetTrimming = mHplTrimOffset - TrimOffset;

	pr_warn("%s mHprTrimOffset = %d TrimOffset = %d\n", __func__, mHplTrimOffset, TrimOffset);
	SetHplOffset(OffsetTrimming);
}
#endif

#if 0				/* Todo */
static void SetDcCompenSation(void)
{
#ifndef EFUSE_HP_TRIM
	SetHprOffsetTrim();
	SetHpLOffsetTrim();
	EnableDcCompensation(true);
#else				/* use efuse trim */
	Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0800, 0x0800);	/* Enable trim circuit of HP */
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPLTRIM_VAUDP15 << 3, 0x0078);	/* Trim offset voltage of HPL */
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPRTRIM_VAUDP15 << 7, 0x0780);	/* Trim offset voltage of HPR */
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPLFINETRIM_VAUDP15 << 12, 0x3000);	/* Fine trim offset voltage of HPL */
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPRFINETRIM_VAUDP15 << 14, 0xC000);	/* Fine trim offset voltage of HPR */
#endif
}
#endif

#if 0				/* Todo */
static void SetDcCompenSation_SPKHP(void)
{
#ifdef EFUSE_HP_TRIM		/* use efuse trim */
	Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0800, 0x0800);	/* Enable trim circuit of HP */
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPLTRIM_VAUDP15_SPKHP << 3, 0x0078);	/* Trim offset voltage of HPL */
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPRTRIM_VAUDP15_SPKHP << 7, 0x0780);	/* Trim offset voltage of HPR */
	/* Fine trim offset voltage of HPL */
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPLFINETRIM_VAUDP15_SPKHP << 12, 0x3000);
	/* Fine trim offset voltage of HPR */
	Ana_Set_Reg(AUDDEC_ANA_CON2, RG_AUDHPRFINETRIM_VAUDP15_SPKHP << 14, 0xC000);
#endif
}
#endif

#if 0
static void SetDCcoupleNP(int MicBias, int mode)
{
	pr_warn("%s MicBias= %d mode = %d\n", __func__, MicBias, mode);
#if 0				/* Todo */

	switch (mode) {
	case AUDIO_ANALOGUL_MODE_ACC:
	case AUDIO_ANALOGUL_MODE_DCC:
	case AUDIO_ANALOGUL_MODE_DMIC:
		{
			if (MicBias == AUDIO_MIC_BIAS0)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x0006);
			else if (MicBias == AUDIO_MIC_BIAS1)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x0060);
		}
		break;
	case AUDIO_ANALOGUL_MODE_DCCECMDIFF:
		{
			if (MicBias == AUDIO_MIC_BIAS0)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0006, 0x0006);
			else if (MicBias == AUDIO_MIC_BIAS1)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0060, 0x0060);
		}
		break;
	case AUDIO_ANALOGUL_MODE_DCCECMSINGLE:
		{
			if (MicBias == AUDIO_MIC_BIAS0)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0002, 0x0006);
			else if (MicBias == AUDIO_MIC_BIAS1)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0020, 0x0060);
		}
		break;
	default:
		break;
	}
#endif
}
#endif

uint32 GetULFrequency(uint32 frequency)
{
	uint32 Reg_value = 0;

	pr_warn("%s frequency =%d\n", __func__, frequency);
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

	pr_warn("%s frequency =%d\n", __func__, frequency);
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
		break;
	default:
		pr_warn("GetDLFrequency with frequency = %d", frequency);
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


static int mt63xx_codec_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *Daiport)
{
	/* printk("+mt63xx_codec_startup name = %s number = %d\n", substream->name, substream->number); */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE && substream->runtime->rate) {
		/* pr_warn("mt63xx_codec_startup set up CAPTURE rate = %d\n", substream->runtime->rate); */
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = substream->runtime->rate;

	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && substream->runtime->rate) {
		/* pr_warn("mt63xx_codec_startup set up PLAYBACK rate = %d\n", substream->runtime->rate); */
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = substream->runtime->rate;
	}
	/* printk("-mt63xx_codec_startup name = %s number = %d\n", substream->name, substream->number); */
	return 0;
}

static int mt63xx_codec_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *Daiport)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_warn("mt63xx_codec_prepare set up SNDRV_PCM_STREAM_CAPTURE rate = %d\n",
		       substream->runtime->rate);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = substream->runtime->rate;

	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_warn("mt63xx_codec_prepare set up SNDRV_PCM_STREAM_PLAYBACK rate = %d\n",
		       substream->runtime->rate);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = substream->runtime->rate;
	}
	return 0;
}

static int mt6350_codec_trigger(struct snd_pcm_substream *substream, int command,
				struct snd_soc_dai *Daiport)
{
	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}

	/* printk("mt6350_codec_trigger command = %d\n ", command); */
	return 0;
}

static const struct snd_soc_dai_ops mt6350_aif1_dai_ops = {
	.startup = mt63xx_codec_startup,
	.prepare = mt63xx_codec_prepare,
	.trigger = mt6350_codec_trigger,
};

static struct snd_soc_dai_driver mtk_6350_dai_codecs[] = {
	{
	 .name = MT_SOC_CODEC_TXDAI_NAME,
	 .ops = &mt6350_aif1_dai_ops,
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
	 .ops = &mt6350_aif1_dai_ops,
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
	 .ops = &mt6350_aif1_dai_ops,
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
	 .ops = &mt6350_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_I2SDL1_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rate_min = 8000,
		      .rate_max = 192000,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      }
	 },
	{
	 .name = MT_SOC_CODEC_VOICE_MD1DAI_NAME,
	 .ops = &mt6350_aif1_dai_ops,
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
	 .ops = &mt6350_aif1_dai_ops,
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
	 .ops = &mt6350_aif1_dai_ops,
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
	 .ops = &mt6350_aif1_dai_ops,
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
	 .ops = &mt6350_aif1_dai_ops,
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
	 .ops = &mt6350_aif1_dai_ops,
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
	 .ops = &mt6350_aif1_dai_ops,
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
	 .ops = &mt6350_aif1_dai_ops,
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
	 .ops = &mt6350_aif1_dai_ops,
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
	{
	 .name = MT_SOC_CODEC_TXDAI2_NAME,
	 .ops = &mt6350_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_DL2_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
};


uint32 GetDLNewIFFrequency(unsigned int frequency)
{
	uint32 Reg_value = 0;
	/* printk("AudioPlatformDevice ApplyDLNewIFFrequency ApplyDLNewIFFrequency = %d", frequency); */
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
		break;
	default:
		pr_warn("ApplyDLNewIFFrequency with frequency = %d", frequency);
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
		break;
	default:
		pr_warn("GetULNewIFFrequency with frequency = %d", frequency);
	}
	pr_warn("GetULNewIFFrequency Reg_value = %d", Reg_value);
	return Reg_value;
}

static void TurnOnDacPower(void)
{
	uint32 dlFreq;

	pr_warn("TurnOnDacPower\n");
	audckbufEnable(true);
	ClsqEnable(true);
	Topck_Enable(true);
	/* udelay(250); */
	NvregEnable(true);
	if (GetAdcStatus() == false) {
#if 0				/* todo */
		Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x003a, 0xffff);	/* power on clock */
#endif
	} else {
#if 0				/* todo */
		Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);	/* power on clock */
#endif
	}

	/* set digital part */
	Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0,
		    GetDLNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) << 12 |
		    0x330, 0xffff);
	dlFreq = GetDLFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]);
	Ana_Set_Reg(ABB_AFE_CON1, dlFreq, 0x000f);	/* DL sampling rate */
	TopCtlChangeTrigger();
	Ana_Set_Reg(ABB_AFE_CON0, 0x0001, 0x0001);	/* DL turn on enable */

#if 0				/* todo */
	/* Ana_Set_Reg(ABB_AFE_TOP_CON0 , 0x0000 , 0xffff); //set DL in normal path, not from sine gen table */
#endif
}

static void TurnOffDacPower(void)
{
	pr_warn("TurnOffDacPower\n");

	if (GetAdcStatus() == false) {
#if 0				/* todo */
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0xffff);	/* turn off afe */
#endif
	}
	/* udelay(250); */
#if 0				/* todo */
	Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0040, 0x0040);	/* down-link power down */
#else
	Ana_Set_Reg(ABB_AFE_CON0, 0x0000, 0x0001);	/* turn off DL */
	TopCtlChangeTrigger();
#endif

	NvregEnable(false);
	ClsqEnable(false);
	Topck_Enable(false);
	audckbufEnable(false);
}

static void HeadsetVoloumeRestore(void)
{
#if 0				/* todo */
	int index = 0, oldindex = 0, offset = 0, count = 1;

	pr_warn("%s\n", __func__);
	index = 8;
	oldindex = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	if (index > oldindex) {
		pr_debug("index = %d oldindex = %d\n", index, oldindex);
		offset = index - oldindex;
		while (offset > 0) {
			Ana_Set_Reg(ZCD_CON2, ((oldindex + count) << 7) | (oldindex + count),
				    0xf9f);
			offset--;
			count++;
			udelay(100);
		}
	} else {
		pr_debug("index = %d oldindex = %d\n", index, oldindex);
		offset = oldindex - index;
		while (offset > 0) {
			Ana_Set_Reg(ZCD_CON2, ((oldindex - count) << 7) | (oldindex - count),
				    0xf9f);
			offset--;
			count++;
			udelay(100);
		}
	}
	Ana_Set_Reg(ZCD_CON2, 0x0489, 0xf9f);
#else
	pr_warn("%s no this\n", __func__);
#endif
}

static void HeadsetVolumeSet(void)
{
	/* Left channel */
	int index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];
	int index2 = 7;

	pr_warn("%s(), Lindex = %d\n", __func__, index);

	/* gain[] = {-5, -3, -1, 1, 3, 5, 7, 9}; */

	if (index > index2)
		index = index2;

	index2 -= index;

	pr_warn("%s(), Lindex = %d, Lindex2 = %d\n", __func__, index, index2);

	Ana_Set_Reg(AUDTOP_CON5, index2 << 12, 0x00007000);

	/* right channel */
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	index2 = 7;
	/* gain[] = {-5, -3, -1, 1, 3, 5, 7, 9}; */

	pr_warn("%s(), Rindex = %d\n", __func__, index);

	if (index > index2) {
		/* use maximum value */
		index = index2;
	}
	index2 -= index;

	/* pr_debug("%s(), Rindex = %d, Rindex2 = %d\n", __func__, index,index2); */

	Ana_Set_Reg(AUDTOP_CON5, index2 << 8, 0x000000700);

}

static void Audio_Amp_Change(int channels, bool enable)
{
	if (enable) {

		if (GetDLStatus() == false)
			TurnOnDacPower();

		/* here pmic analog control */
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false
		    && mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] ==
		    false) {
			pr_warn("%s\n", __func__);

			/* need to modify sequence */
			/* DC compensation setting */
			pr_warn
			    ("%s, mHpRightDcCalibration [%d] mHpLeftDcCalibration [%d], mIsExtSPKUse=%x\n",
			     __func__, mHpRightDcCalibration, mHpLeftDcCalibration, mIsExtSPKUse);
			Ana_Set_Reg(ABB_AFE_CON3, mHpLeftDcCalibration, 0xffff);	/* LCH cpmpensation value */
			Ana_Set_Reg(ABB_AFE_CON4, mHpRightDcCalibration, 0xffff);	/* RCH cpmpensation value */
			Ana_Set_Reg(ABB_AFE_CON10, 0x0001, 0x0001);	/* enable DC cpmpensation */
			DCChangeTrigger();	/* Trigger DC compensation */

			/* Enable 2.4V. Enable HalfV buffer for HP VCM generation.Enable audio clock */
			Ana_Set_Reg(AUDTOP_CON6, 0xF7F2, 0xffff);
			Ana_Set_Reg(AUDTOP_CON0, 0x7000, 0xf000);	/* enable clean 1.35VCM buffer in audioUL */
			Ana_Set_Reg(AUDTOP_CON5, 0x0014, 0xffff);	/* set RCH/LCH buffer gain to smallest -5dB */
			if (mIsExtSPKUse) {
				/* enable audio bias. only enable audio-R DAC, HP buffers (L needs to turn off) */
				Ana_Set_Reg(AUDTOP_CON4, 0x005C, 0xffff);
			} else {
				/* enable audio bias. enable audio DAC, HP buffers */
				Ana_Set_Reg(AUDTOP_CON4, 0x007C, 0xffff);
			}
			/* udelay(10 * 1000); */
			mdelay(10);
			/* HP pre-charge function release, disable depop mux of HP drivers. Disable depop VCM gen. */
			Ana_Set_Reg(AUDTOP_CON6, 0xF5BA, 0xffff);
			Ana_Set_Reg(AUDTOP_CON5, 0x2214, 0xffff);	/* set RCH/LCH buffer gain to -1dB */

			HeadsetVolumeSet();
		} else if (channels == AUDIO_ANALOG_CHANNELS_LEFT1) {
			/* Reserve */
		} else if (channels == AUDIO_ANALOG_CHANNELS_RIGHT1) {
			/* Reserve */
		}
	} else {
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false
		    && mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] ==
		    false) {
			pr_warn("Audio_Amp_Change off amp\n");
			HeadsetVoloumeRestore();	/* Set HPR/HPL gain as -1dB, step by step */
			/* Ana_Set_Reg(ZCD_CON2, 0x0F9F, 0xffff); //Set HPR/HPL gain as minimum (~ -40dB) */
#if 0				/* todo */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF40F, 0xffff);	/* Disable HPR/HPL */
#else
			Ana_Set_Reg(AUDTOP_CON5, 0x0014, 0xffff);	/* Set RCH/LCH buffer to smallest gain -5dB */
			/* Reset pre-charge function, Enable depop mux of HP drivers, Enable depop VCM gen */
			Ana_Set_Reg(AUDTOP_CON6, 0xF7F2, 0xffff);
			Ana_Set_Reg(AUDTOP_CON4, 0x0000, 0xffff);	/* Disable audio bias, audio DAC, HP buffers */

			if (GetULStatus() == false) {
				/* Disable clean 1.35V VCM buffer in audio UL. */
				Ana_Set_Reg(AUDTOP_CON0, 0x0000, 0x1000);
			}

			/* Disable input short of HP drivers for voice signal leakage prevent and disable 2.4V
				reference buffer, audio DAC clock. */
			Ana_Set_Reg(AUDTOP_CON6, 0x37E2, 0xffff);

#endif
		} else if (channels == AUDIO_ANALOG_CHANNELS_LEFT1) {
			/* researve */
		} else if (channels == AUDIO_ANALOG_CHANNELS_RIGHT1) {
			/* researve */
		}

#if 0	/* no need to reset the DC compensation value to fix the pop noise when turn off */
		Ana_Set_Reg(ABB_AFE_CON3, 0, 0xffff);	/* LCH cancel DC */
		Ana_Set_Reg(ABB_AFE_CON4, 0, 0xffff);	/* RCH cancel DC */
		Ana_Set_Reg(ABB_AFE_CON10, 0x0000, 0x0001);	/* enable DC cpmpensation */
		DCChangeTrigger();	/* Trigger DC compensation */
#endif
		if (GetDLStatus() == false) {
#if 0				/* todo */
			Ana_Set_Reg(AUDDEC_ANA_CON4, 0x0000, 0xffff);	/* Disable drivers bias circuit */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0xffff);	/* Disable Audio DAC */
			/* Disable AUD_CLK, bit2/4/8 is for ADC, do not set */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x2AC0, 0xfeeb);
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0000, 0x8000);	/* Disable NV regulator (-1.5V) */
			/* Disable cap-less LDOs (1.5V) & Disable IBIST */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0001, 0xfeeb);
#endif
			TurnOffDacPower();
		}

	}
}

static int Audio_AmpL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_AmpL_Get = %d\n",
	       mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL];
	return 0;
}

static int Audio_AmpL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	pr_warn("%s() gain = %ld\n ", __func__, ucontrol->value.integer.value[0]);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] ==
		       true)) {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
		    ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int Audio_AmpR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_AmpR_Get = %d\n",
	       mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR];
	return 0;
}

static int Audio_AmpR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	pr_warn("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] ==
		       true)) {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
		    ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int PMIC_REG_CLEAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
#ifdef PMIC_RESET_WORKAROUND
	pr_warn("%s(), do PMIC_REG_CLEAR_Set\n", __func__);
	Ana_Set_Reg(ABB_AFE_CON2, 0, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON4, 0, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON5, 0x28, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON6, 0x218, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON7, 0x204, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON8, 0x0, 0xffff);
	Ana_Set_Reg(ABB_AFE_CON9, 0x0, 0xffff);
	Ana_Set_Reg(AFE_PMIC_NEWIF_CFG1, 0x18, 0xffff);
	Ana_Set_Reg(AFE_PMIC_NEWIF_CFG3, 0xf872, 0xffff);
#endif
	return 0;
}

static int PMIC_REG_CLEAR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s(), not support\n", __func__);

	return 0;
}

#if 0	/* not used */
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
	uint16 rReg = 0;

	if (enable) {
		pr_warn("%s\n", __func__);
		if (GetDLStatus() == false) {
			TurnOnDacPower();
			pr_warn("Voice_Amp_Change on amp\n");

			/* set analog part (voice HS playback) */
			Ana_Set_Reg(AUDTOP_CON7, 0x2430, 0xffff);	/* Set voice buffer to smallest -22dB. */
			/* enable input short of HP to prevent voice signal leakage . Enable 2.4V. */
			Ana_Set_Reg(AUDTOP_CON6, 0xB7F6, 0xffff);
			/* Depop. Enable audio clock */
			Ana_Set_Reg(AUDTOP_CON0, 0x7000, 0xf000);	/* enable clean 1.35VCM buffer in audioUL */
			Ana_Set_Reg(AUDTOP_CON4, 0x0014, 0xffff);	/* enable audio bias. enable LCH DAC */
			for (i = 3; i < 11; i++) {
				/* udelay(5 * 1000); */
				mdelay(5);
				rReg = 0x2500 + (i << 4);
				/* enable voice buffer and -1dB gain. ramp up volume from -21dB to -1dB here */
				Ana_Set_Reg(AUDTOP_CON7, rReg, 0xffff);
			}
		}
	} else {
		pr_warn("Voice_Amp_Change turn off\n");
		/* uint16 i; */
		i = (Ana_Get_Reg(AUDTOP_CON7) & 0xf0) >> 4;
		i = (i < 4) ? 4 : i;
		i = (i > 16) ? 16 : i;
		for (i = i - 1; i >= 3; i--) {
			rReg = 0x2500 + (i << 4);
			/* udelay(5 * 1000); */
			mdelay(5);
			/* disable voice buffer and -21dB gain. ramp down volume from current to -21dB here */
			Ana_Set_Reg(AUDTOP_CON7, rReg, 0xffff);
		}
		Ana_Set_Reg(AUDTOP_CON7, 0x2500, 0xffff);	/* set voice buffer gain as -22dB */
		Ana_Set_Reg(AUDTOP_CON7, 0x2400, 0xffff);	/* Disable voice buffer */
		Ana_Set_Reg(AUDTOP_CON4, 0x0000, 0xffff);	/* Disable audio bias and L-DAC */

		if (GetULStatus() == false)
			Ana_Set_Reg(AUDTOP_CON0, 0x0000, 0x1000);	/* Disable 1.4v common mdoe */

		/* Disable input short of HP drivers for voice signal leakage prevent and disable 2.4V reference buffer,
			audio DAC clock. */
		Ana_Set_Reg(AUDTOP_CON6, 0x37E2, 0xffff);
		TurnOffDacPower();
	}
}

static int Voice_Amp_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Voice_Amp_Get = %d\n",
	       mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL];
	return 0;
}

static int Voice_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	pr_warn("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] == false)) {
		Voice_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] ==
		       true)) {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] =
		    ucontrol->value.integer.value[0];
		Voice_Amp_Change(false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static void Speaker_Amp_Change(bool enable)
{
	uint16 i = 0;

	if (enable) {
		if (GetDLStatus() == false) {
			/* TurnOnDacPower if not open */
			TurnOnDacPower();
		}
		pr_warn("%s\n", __func__);

		Ana_Set_Reg(AUDTOP_CON7, 0x2400, 0xffff);	/* Set voice buffer to smallest -22dB. */
		/* enable input short of HP to prevent voice signal leakage . Enable 2.4V. */
		Ana_Set_Reg(AUDTOP_CON6, 0xB7F6, 0xffff);
		/* Depop. Enable audio clock */
		Ana_Set_Reg(AUDTOP_CON0, 0x7000, 0xf000);	/* enable clean 1.35VCM buffer in audioUL */
		Ana_Set_Reg(AUDTOP_CON4, 0x0014, 0xffff);	/* enable audio bias. enable LCH DAC */
		/* udelay(10 * 1000); */
		mdelay(10);
		if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER) {
			/* enable voice buffer and +1dB gain. Inter-connect voice buffer to SPK AMP */
			Ana_Set_Reg(AUDTOP_CON7, 0x35B0, 0xffff);
		} else {
			/* enable voice buffer and -11dB gain. Inter-connect voice buffer to SPK AMP */
			Ana_Set_Reg(AUDTOP_CON7, 0x3550, 0xffff);
		}

		Ana_Set_Reg(TOP_CKPDN1_CLR, 0x000E, 0x000E);	/* Speaker clock */


#ifdef CONFIG_MTK_SPEAKER
		if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
			Speaker_ClassD_Open();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
			Speaker_ClassAB_Open();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
			Speaker_ReveiverMode_Open();
#endif
		Ana_Set_Reg(SPK_CON12, 0x0A00, 0xffff);	/* spk output stage enable and enable */

		/* 2in1 speaker do not need this */
		if (Speaker_mode != AUDIO_SPEAKER_MODE_RECEIVER) {
			for (i = 6; i <= 11; i++) {
				udelay(1 * 1000);
				/* enable voice buffer and +1dB gain. Inter-connect voice buffer to SPK AMP */
				Ana_Set_Reg(AUDTOP_CON7, (0x3500 | (i << 4)), 0xffff);
			}
		}
		Apply_Speaker_Gain();
	} else {
		pr_warn("turn off Speaker_Amp_Change\n");
		/* 2in1 speaker do not need this */
		if (Speaker_mode != AUDIO_SPEAKER_MODE_RECEIVER) {
#if 0
			for (i = 10; i >= 5; i--) {
				/* ramp to -11dB. Inter-connect voice buffer to SPK AMP */
				Ana_Set_Reg(AUDTOP_CON7, (0x3500 | (i << 4)), 0xffff);
				udelay(1 * 1000);
			}
#endif

			{	/* Add speaker ramp function */
				uint32 index;
				uint32 currentIdx;

				index = 4;
				currentIdx = (Ana_Get_Reg(SPK_CON9) >> 8) & 0xF;
				pr_warn("%s, SPK_CON9 index= %d Cur = %d\n", __func__, index,
				       currentIdx);
				if (index > currentIdx) {
					for (i = (currentIdx + 1); i <= index; i++) {
						Ana_Set_Reg(SPK_CON9, i << 8, 0x0F00);
						/* printk("AnalogClose RampUp SPK_CON9 %x index= %d Cur = %d",
						Ana_Get_Reg(SPK_CON9), index, currentIdx); */
						/* msleep(3); */
						mdelay(3);
					}
				} else if ((index < currentIdx) && (currentIdx != 0)) {
					for (i = currentIdx - 1; i >= index; i--) {
						Ana_Set_Reg(SPK_CON9, i << 8, 0x0F00);
						/* printk("AnalogClose RampDown SPK_CON9 %x index= %d Cur = %d",
						Ana_Get_Reg(SPK_CON9), index, currentIdx); */
						/* msleep(3); */
						mdelay(3);
					}
				}
				/* Add voice buffer ramp function */
				index = 0xB;
				currentIdx = (Ana_Get_Reg(AUDTOP_CON7) >> 4) & 0xF;
				pr_warn("%s, AUDTOP_CON7 index= %d Cur = %d\n", __func__, index,
				       currentIdx);
				if (index > currentIdx) {
					for (i = (currentIdx + 1); i <= index; i++) {
						Ana_Set_Reg(AUDTOP_CON7, i << 4, 0x00F0);
						/* pr_debug("AnalogClose RampUp AUDTOP_CON7 %x index= %d Cur = %d",
						Ana_Get_Reg(AUDTOP_CON7), index, currentIdx); */
						/* msleep(3); */
						mdelay(3);
					}
				} else if ((index < currentIdx) && (currentIdx != 0)) {
					for (i = currentIdx - 1; i >= index; i--) {
						Ana_Set_Reg(AUDTOP_CON7, i << 4, 0x00F0);
						/* pr_debug("AnalogClose RampDown AUDTOP_CON7 %x index= %d Cur = %d",
						Ana_Get_Reg(AUDTOP_CON7), index, currentIdx); */
						/* msleep(3); */
						mdelay(3);
					}
				}
			}	/* Add speaker ramp function done */

		}
#ifdef CONFIG_MTK_SPEAKER
		if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
			Speaker_ClassD_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
			Speaker_ClassAB_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
			Speaker_ReveiverMode_close();
#endif

		Ana_Set_Reg(SPK_CON12, 0x0000, 0xffff);	/* Disable SPK output stage, disable spk amp. */
		Ana_Set_Reg(TOP_CKPDN1_SET, 0x000E, 0x000E);	/* Disable Speaker clock */
		Ana_Set_Reg(AUDTOP_CON7, 0x2500, 0xffff);	/* set voice buffer gain as -22dB */
		Ana_Set_Reg(AUDTOP_CON7, 0x2400, 0xffff);	/* Disable voice buffer */
		Ana_Set_Reg(AUDTOP_CON4, 0x0000, 0xffff);	/* Disable audio bias and L-DAC */

		if (GetULStatus() == false) {
			/* Disable 1.4v common mdoe */
			Ana_Set_Reg(AUDTOP_CON0, 0x0000, 0x1000);
		}

		/* Disable input short of HP drivers for voice signal leakage prevent and disable 2.4V reference buffer,
			audio DAC clock. */
		Ana_Set_Reg(AUDTOP_CON6, 0x37E2, 0xffff);

		if (GetDLStatus() == false)
			TurnOffDacPower();
	}
}

static int Speaker_Amp_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL];
	return 0;
}

static int Speaker_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s() value = %ld\n ", __func__, ucontrol->value.integer.value[0]);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] == false)) {
		Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] ==
		       true)) {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] =
		    ucontrol->value.integer.value[0];
		Speaker_Amp_Change(false);
	}
	return 0;
}

static unsigned int pin_extspkamp;
static unsigned int pin_mode_extspkamp;
/* static unsigned int pin_vowclk, pin_audmiso;*/
/* static unsigned int pin_mode_vowclk, pin_mode_audmiso; */

#ifdef CONFIG_OF

#define GAP (2)			/* unit: us */
#ifdef CONFIG_MTK_LEGACY
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

#define NULL_PIN_DEFINITION    (-1)
static void Ext_Speaker_Amp_Change(bool enable)
{
#define SPK_WARM_UP_TIME        (25)	/* unit is ms */
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_MTK_LEGACY
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
#ifdef CONFIG_MTK_LEGACY

		ret = GetGPIO_Info(10, &pin_extspkamp_2, &pin_mode_extspkamp_2);
		pr_warn("Ext_Speaker_Amp_Change ON set GPIO\n");
		mt_set_gpio_mode(pin_extspkamp, GPIO_MODE_00);	/* GPIO117: DPI_D3, mode 0 */
		mt_set_gpio_pull_enable(pin_extspkamp, GPIO_PULL_ENABLE);
		mt_set_gpio_dir(pin_extspkamp, GPIO_DIR_OUT);	/* output */
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ZERO);	/* low disable */
		if (pin_extspkamp_2 != NULL_PIN_DEFINITION) {
			mt_set_gpio_mode(pin_extspkamp_2, GPIO_MODE_00);	/* GPIO117: DPI_D3, mode 0 */
			mt_set_gpio_pull_enable(pin_extspkamp_2, GPIO_PULL_ENABLE);
			mt_set_gpio_dir(pin_extspkamp_2, GPIO_DIR_OUT);	/* output */
			mt_set_gpio_out(pin_extspkamp_2, GPIO_OUT_ZERO);	/* low disable */
		}
#else
		AudDrv_GPIO_EXTAMP_Select(false);
		AudDrv_GPIO_EXTAMP2_Select(false);
#endif /*CONFIG_MTK_LEGACY*/

		/*udelay(1000);*/
		usleep_range(1*1000, 20*1000);
#ifdef CONFIG_MTK_LEGACY
		mt_set_gpio_dir(pin_extspkamp, GPIO_DIR_OUT);	/* output */
		if (pin_extspkamp_2 != NULL_PIN_DEFINITION)
			mt_set_gpio_dir(pin_extspkamp_2, GPIO_DIR_OUT);	/* output */

#ifdef AW8736_MODE_CTRL
		AW8736_MODE3;
#else
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ONE);	/* high enable */
#endif /*AW8736_MODE_CTRL*/
		if (pin_extspkamp_2 != NULL_PIN_DEFINITION)
			mt_set_gpio_out(pin_extspkamp_2, GPIO_OUT_ONE);	/* high enable */
#else
		AudDrv_GPIO_EXTAMP_Select(true);
		AudDrv_GPIO_EXTAMP2_Select(true);
#endif /*CONFIG_MTK_LEGACY*/
		mdelay(SPK_WARM_UP_TIME);
#endif
		pr_debug("Ext_Speaker_Amp_Change ON-\n");
	} else {
		pr_debug("Ext_Speaker_Amp_Change OFF+\n");
#ifndef CONFIG_MTK_SPEAKER
#ifdef CONFIG_MTK_LEGACY
		ret = GetGPIO_Info(10, &pin_extspkamp_2, &pin_mode_extspkamp_2);
		/* mt_set_gpio_mode(pin_extspkamp, GPIO_MODE_00); //GPIO117: DPI_D3, mode 0 */
		mt_set_gpio_dir(pin_extspkamp, GPIO_DIR_OUT);	/* output */
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ZERO);	/* low disbale */
		if (pin_extspkamp_2 != NULL_PIN_DEFINITION) {
			mt_set_gpio_dir(pin_extspkamp_2, GPIO_DIR_OUT);	/* output */
			mt_set_gpio_out(pin_extspkamp_2, GPIO_OUT_ZERO);	/* low disbale */
		}
#else
		AudDrv_GPIO_EXTAMP_Select(false);
		AudDrv_GPIO_EXTAMP2_Select(false);
#endif
		udelay(500);
#endif
		pr_debug("Ext_Speaker_Amp_Change OFF-\n");
	}
#endif
}

#else
#ifndef CONFIG_MTK_SPEAKER
#ifdef AW8736_MODE_CTRL
/* 0.75us<TL<10us; 0.75us<TH<10us */
#define GAP (2)			/* unit: us */
/*1.2w*/
static void AW8736_MODE1(void)
{
	mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);
}
/*1.0w*/
static void AW8736_MODE2(void)
{
	do {
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);
	} while (0)
}
/*0.8w*/
static void AW8736_MODE3(void)
{
	do {
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);
	} while (0)
}

/*it depends on THD, range: 1.5 ~ 2.0w*/
static void AW8736_MODE4(void)
{
	do {
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO);
		udelay(GAP);
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);
	} while (0)
}
#endif
#endif

static void Ext_Speaker_Amp_Change(bool enable)
{
#define SPK_WARM_UP_TIME        (25)	/* unit is ms */

	if (enable) {
		pr_warn("Ext_Speaker_Amp_Change ON+\n");
#ifndef CONFIG_MTK_SPEAKER
		pr_warn("Ext_Speaker_Amp_Change ON set GPIO\n");
		mt_set_gpio_mode(GPIO_EXT_SPKAMP_EN_PIN, GPIO_MODE_00);	/* GPIO117: DPI_D3, mode 0 */
		mt_set_gpio_pull_enable(GPIO_EXT_SPKAMP_EN_PIN, GPIO_PULL_ENABLE);
		mt_set_gpio_dir(GPIO_EXT_SPKAMP_EN_PIN, GPIO_DIR_OUT);	/* output */
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO);	/* low disable */
		udelay(1000);
		mt_set_gpio_dir(GPIO_EXT_SPKAMP_EN_PIN, GPIO_DIR_OUT);	/* output */

#ifdef AW8736_MODE_CTRL
		AW8736_MODE3;
#else
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ONE);	/* high enable */
#endif
		/* msleep(SPK_WARM_UP_TIME); */
		mdelay(SPK_WARM_UP_TIME);
#endif
		pr_warn("Ext_Speaker_Amp_Change ON-\n");
	} else {
		pr_warn("Ext_Speaker_Amp_Change OFF+\n");
#ifndef CONFIG_MTK_SPEAKER
		/* mt_set_gpio_mode(GPIO_EXT_SPKAMP_EN_PIN, GPIO_MODE_00); //GPIO117: DPI_D3, mode 0 */
		mt_set_gpio_dir(GPIO_EXT_SPKAMP_EN_PIN, GPIO_DIR_OUT);	/* output */
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO);	/* low disbale */
		udelay(500);
#endif
		pr_warn("Ext_Speaker_Amp_Change OFF-\n");
	}
}
#endif

static int Ext_Speaker_Amp_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP];
	return 0;
}

static int Ext_Speaker_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

	pr_warn("%s() gain = %ld\n ", __func__, ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		Ext_Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP] =
		    ucontrol->value.integer.value[0];
		Ext_Speaker_Amp_Change(false);
	}
	return 0;
}


static int Ext_Speaker_Use_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = mIsExtSPKUse;
	return 0;
}

static int Ext_Speaker_Use_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

	pr_warn("%s() set = %ld\n ", __func__, ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0])
		mIsExtSPKUse = true;
	else
		mIsExtSPKUse = false;
	return 0;
}

static void Headset_Speaker_Amp_Change(bool enable)
{
	pr_warn("%s()\n", __func__);
	if (enable) {
		if (GetDLStatus() == false) {
			/* TurnOnDacPower if no DL active */
			TurnOnDacPower();
		}
		pr_warn("turn on Speaker_Amp_Change\n");

		/* DC compensation setting */
		pr_warn("%s, mHpRightDcCalibration [%d] mHpLeftDcCalibration [%d]", __func__,
		       mHpRightDcCalibration, mHpLeftDcCalibration);
		Ana_Set_Reg(ABB_AFE_CON3, mHpLeftDcCalibration, 0xffff);	/* LCH cpmpensation value */
		Ana_Set_Reg(ABB_AFE_CON4, mHpRightDcCalibration, 0xffff);	/* RCH cpmpensation value */
		Ana_Set_Reg(ABB_AFE_CON10, 0x0001, 0x0001);	/* enable DC cpmpensation */
		DCChangeTrigger();	/* Trigger DC compensation */

		/* enable input short of HP to prevent voice signal leakage . Enable 2.4V. */
		Ana_Set_Reg(AUDTOP_CON6, 0xF7F2, 0xffff);
		/* Depop. Enable audio clock */
		Ana_Set_Reg(AUDTOP_CON0, 0x7000, 0xf000);	/* enable clean 1.35VCM buffer in audioUL */
		Ana_Set_Reg(AUDTOP_CON5, 0x0014, 0xffff);	/* set RCH/LCH buffer gain to smallest -5dB */
		Ana_Set_Reg(AUDTOP_CON4, 0x007C, 0xffff);	/* enable audio bias. enable audio DAC, HP buffers */
		/* udelay(10 * 1000); */
		mdelay(10);
		/* HP pre-charge function release, disable depop mux of HP drivers. Disable depop VCM gen. */
		Ana_Set_Reg(AUDTOP_CON6, 0xF5BA, 0xffff);
		Ana_Set_Reg(AUDTOP_CON5, 0x2214, 0xffff);	/* set RCH/LCH buffer gain to -1dB */
		/* enable voice buffer and -1dB gain. Inter-connect voice buffer to SPK AMP */
		Ana_Set_Reg(AUDTOP_CON7, 0x35B0, 0xffff);

		Ana_Set_Reg(TOP_CKPDN1_CLR, 0x000E, 0x000E);	/* Speaker clock */

#ifdef CONFIG_MTK_SPEAKER
		if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
			Speaker_ClassD_Open();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
			Speaker_ClassAB_Open();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
			Speaker_ReveiverMode_Open();
#endif

		Ana_Set_Reg(SPK_CON12, 0x0A00, 0xffff);	/* spk output stage enable and enable spk amp */
		/* apply volume setting */
		HeadsetVolumeSet();
		Apply_Speaker_Gain();
	} else {
#ifdef CONFIG_MTK_SPEAKER
		if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
			Speaker_ClassD_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
			Speaker_ClassAB_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
			Speaker_ReveiverMode_close();
#endif


		Ana_Set_Reg(SPK_CON12, 0x0000, 0xffff);	/* Disable SPK output stage, disable spk amp. */
		Ana_Set_Reg(TOP_CKPDN1_SET, 0x000E, 0x000E);	/* Disable Speaker clock */
		/* Voice buffer */
		Ana_Set_Reg(AUDTOP_CON7, 0x2500, 0xffff);	/* set voice buffer gain as -22dB */
		Ana_Set_Reg(AUDTOP_CON7, 0x2400, 0xffff);	/* Disable voice buffer */
		/* Audio buffer */
		Ana_Set_Reg(AUDTOP_CON5, 0x0014, 0xffff);	/* Set RCH/LCH buffer to smallest gain -5dB */
		/* Reset pre-charge function, Enable depop mux of HP drivers, Enable depop VCM gen */
		Ana_Set_Reg(AUDTOP_CON6, 0xF7F2, 0xffff);
		/* audio bias */
		Ana_Set_Reg(AUDTOP_CON4, 0x0000, 0xffff);	/* Disable audio bias, audio DAC, HP buffers */

		/* common 1.35V */
		if (GetULStatus() == false) {
			/* Disable clean 1.35V VCM buffer in audio UL. */
			Ana_Set_Reg(AUDTOP_CON0, 0x0000, 0x1000);
		}

		/* Disable input short of HP drivers for voice signal leakage prevent and disable 2.4V reference buffer,
			audio DAC clock. */
		Ana_Set_Reg(AUDTOP_CON6, 0x37E2, 0xffff);

		HeadsetVoloumeRestore();	/* Set HPR/HPL gain as 0dB, step by step */

		/* Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF20F, 0xffff); //Disable HPR/HPL */
		if (GetDLStatus() == false) {
#if 0	/* do not reset DC calibration value when turn off to fix pop noise issue when turn off */
			Ana_Set_Reg(ABB_AFE_CON3, 0, 0xffff);	/* LCH cancel DC */
			Ana_Set_Reg(ABB_AFE_CON4, 0, 0xffff);	/* RCH cancel DC */
			Ana_Set_Reg(ABB_AFE_CON10, 0x0000, 0x0001);	/* enable DC cpmpensation */
			DCChangeTrigger();	/* Trigger DC compensation */
#endif
			TurnOffDacPower();
		}
	}
}


static int Headset_Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R];
	return 0;
}

static int Headset_Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	pr_warn("%s() gain = %lu\n ", __func__, ucontrol->value.integer.value[0]);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] ==
		false)) {
		Headset_Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->
		       mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] == true)) {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] =
		    ucontrol->value.integer.value[0];
		Headset_Speaker_Amp_Change(false);
	}
	return 0;
}

#ifdef CONFIG_MTK_SPEAKER
static const char *const speaker_amp_function[] = { "CALSSD", "CLASSAB", "RECEIVER" };

static const char * const speaker_PGA_function[] = { "MUTE", "0Db", "4Db", "5Db", "6Db",
"7Db", "8Db", "9Db", "10Db", "11Db", "12Db", "13Db", "14Db", "15Db", "16Db", "17Db" };
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
	Speaker_pga_gain = ucontrol->value.integer.value[0];

	pr_warn("%s Speaker_pga_gain= %d\n", __func__, Speaker_pga_gain);
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	/* int index = 0; */
	/* this will base on hw spec, use 15dB for */
	/* int index2 = 13; */
	/* gain[] =  {mute,0,4,5,6,7,8,9,10,11,12,13,14,15,16,17}; */

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(speaker_PGA_function)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
#if 0
	index = Speaker_pga_gain;
	/* condition for gainb not mute */
	if (index > 11) {
		/* max to index 11 */
		index = 11;
	}

	if (index < 1) {
		/* min to 0dB */
		index = 1;
	}

	pr_warn("%s(), index = %d\n", __func__, index);

	Ana_Set_Reg(SPK_CON9, index << 8, 0x00000f00);
#else
	Apply_Speaker_Gain();
#endif

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL] = ucontrol->value.integer.value[0];
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
	pr_warn("%s is not support setting\n", __func__);
	return 0;
}

static int Audio_Speaker_Pga_Gain_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = Speaker_pga_gain;
	return 0;
}

static int Audio_Speaker_Current_Sensing_Set(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{

	if (ucontrol->value.integer.value[0])
		Ana_Set_Reg(SPK_CON12, 0x9300, 0xff00);
	else
		Ana_Set_Reg(SPK_CON12, 0x1300, 0xff00);

	return 0;
}

static int Audio_Speaker_Current_Sensing_Get(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = (Ana_Get_Reg(SPK_CON12) >> 15) & 0x01;
	return 0;
}

static int Audio_Speaker_Current_Sensing_Peak_Detector_Set(struct snd_kcontrol *kcontrol,
							   struct snd_ctl_elem_value *ucontrol)
{

	if (ucontrol->value.integer.value[0])
		Ana_Set_Reg(SPK_CON12, 1 << 14, 1 << 14);
	else
		Ana_Set_Reg(SPK_CON12, 0, 1 << 14);

	return 0;
}

static int Audio_Speaker_Current_Sensing_Peak_Detector_Get(struct snd_kcontrol *kcontrol,
							   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = (Ana_Get_Reg(SPK_CON12) >> 14) & 0x01;
	return 0;
}


static const struct soc_enum Audio_Speaker_Enum[] = {
	/* speaker class setting */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_amp_function), speaker_amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_PGA_function), speaker_PGA_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_OC_function), speaker_OC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_CS_function), speaker_CS_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_CSPeakDetecReset_function),
			    speaker_CSPeakDetecReset_function),
};

static const struct snd_kcontrol_new mt6350_snd_Speaker_controls[] = {
	SOC_ENUM_EXT("Audio_Speaker_class_Switch", Audio_Speaker_Enum[0], Audio_Speaker_Class_Get,
		     Audio_Speaker_Class_Set),
	SOC_ENUM_EXT("Audio_Speaker_PGA_gain", Audio_Speaker_Enum[1], Audio_Speaker_Pga_Gain_Get,
		     Audio_Speaker_Pga_Gain_Set),
	SOC_ENUM_EXT("Audio_Speaker_OC_Falg", Audio_Speaker_Enum[2], Audio_Speaker_OcFlag_Get,
		     Audio_Speaker_OcFlag_Set),
	SOC_ENUM_EXT("Audio_Speaker_CurrentSensing", Audio_Speaker_Enum[3],
		     Audio_Speaker_Current_Sensing_Get, Audio_Speaker_Current_Sensing_Set),
	SOC_ENUM_EXT("Audio_Speaker_CurrentPeakDetector", Audio_Speaker_Enum[4],
		     Audio_Speaker_Current_Sensing_Peak_Detector_Get,
		     Audio_Speaker_Current_Sensing_Peak_Detector_Set),
};

int Audio_AuxAdcData_Get_ext(void)
{
#ifdef RAINIER_NEED_CHECK	/* todo */
	/* int dRetValue = PMIC_IMM_GetOneChannelValue(AUX_ICLASSAB_AP, 1, 0); */
	/* int dRetValue = PMIC_IMM_GetOneChannelValue(MT6350_AUX_CH9, 1, 0); */
	pr_warn("%s dRetValue 0x%x\n", __func__, dRetValue);
	return dRetValue;
#else
	pr_warn("%s , todo\n", __func__);
#endif
	return 0;
}


#endif

static int Audio_AuxAdcData_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

#ifdef CONFIG_MTK_SPEAKER
	/* PMIC_IMM_GetSPK_THR_IOneChannelValue(0x001B, 1, 0); */
	ucontrol->value.integer.value[0] = Audio_AuxAdcData_Get_ext();
#else
	ucontrol->value.integer.value[0] = 0;
#endif
	pr_warn("%s dMax = 0x%lx\n", __func__, ucontrol->value.integer.value[0]);
	return 0;

}

static int Audio_AuxAdcData_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	dAuxAdcChannel = ucontrol->value.integer.value[0];
	pr_warn("%s dAuxAdcChannel = 0x%x\n", __func__, dAuxAdcChannel);
	return 0;
}


static const struct snd_kcontrol_new Audio_snd_auxadc_controls[] = {
	SOC_SINGLE_EXT("Audio AUXADC Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_AuxAdcData_Get,
		       Audio_AuxAdcData_Set),
};


static const char * const amp_function[] = { "Off", "On" };
static const char * const aud_clk_buf_function[] = { "Off", "On" };

/* static const char * const DAC_SampleRate_function[] = {"8000", "11025", "16000", "24000",
"32000", "44100", "48000"}; */
static const char * const DAC_DL_PGA_Headset_GAIN[] = { "9Db", "7Db", "5Db", "3Db",
"1Db", "-1Db", "-3Db", "-5Db" };

static const char * const DAC_DL_PGA_Handset_GAIN[] = { "-21Db", "-19Db", "-17Db", "-15Db",
"-13Db", "-11Db", "-9Db", "-7Db", "-5Db", "-3Db", "-1Db", "1Db", "3Db", "5Db", "7Db", "9Db"
};

/* Lineout use */
static const char * const DAC_DL_PGA_Speaker_GAIN[] = { "8Db", "7Db", "6Db", "5Db", "4Db",
	"3Db", "2Db", "1Db", "0Db", "-1Db", "-2Db", "-3Db", "-4Db", "-5Db", "-6Db",
	"-7Db", "-8Db", "-9Db", "-10Db", "-40Db"
};


/* static const char *Voice_Mux_function[] = {"Voice", "Speaker"}; */

static int Lineout_PGAL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Speaker_PGA_Get = %d\n", mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL]);
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL];
	return 0;
}

static int Lineout_PGAL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_warn("%s(), index = %d\n", __func__, ucontrol->value.enumerated.item[0]);

	if (ucontrol->value.enumerated.item[0] >= ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
		index = 0x1f;

#if 0				/* todo */
	Ana_Set_Reg(ZCD_CON1, index, 0x001f);
#endif
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKL] = ucontrol->value.integer.value[0];
	return 0;
}

static int Lineout_PGAR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s  = %d\n", __func__, mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR]);
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR];
	return 0;
}

static int Lineout_PGAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index = 0;

	pr_warn("%s(), index = %d\n", __func__, ucontrol->value.enumerated.item[0]);

	if (ucontrol->value.enumerated.item[0] >= ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	if (ucontrol->value.enumerated.item[0] == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1)) {
		/* use index as maximum */
		index = 0x1f;
	}
#if 0				/* todo */
	Ana_Set_Reg(ZCD_CON1, index << 7, 0x0f10);
#endif
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_SPKR] = ucontrol->value.integer.value[0];
	return 0;
}

static int Handset_PGA_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Handset_PGA_Get = %d\n",
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL];
	return 0;
}

static int Handset_PGA_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index = 0;
	int index2 = 15;
	/* gain[] =  {-21, -19, -17, -15, -13, -11, -9, -7, -5, -3, -1, 1, 3, 5, 7, 9}; */
	pr_warn("%s(), index = %d\n", __func__, ucontrol->value.enumerated.item[0]);

	if (ucontrol->value.enumerated.item[0] >= ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];

	if (index > index2) {
		/* use index2 as maximum */
		index = index2;
	}

	pr_warn("%s(), index = %d, index2 = %d\n", __func__, index, index2);

	Ana_Set_Reg(AUDTOP_CON7, index << 4, 0x000000f0);

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Headset_PGAL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Headset_PGAL_Get = %d\n",
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];
	return 0;
}

static int Headset_PGAL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index = 0;
	int index2 = 7;
	/* gain[] = {-5, -3, -1, 1, 3, 5, 7, 9}; */
	/* pr_warn("%s(), index = %d arraysize = %d\n", __func__, ucontrol->value.enumerated.item[0],
		ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)); */

	if (ucontrol->value.enumerated.item[0] >= ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	if (index > index2) {
		/* use index2 as maximum */
		index = index2;
	}
	index2 -= index;

	pr_warn("%s(), index = %d, index2 = %d\n", __func__, index, index2);

	Ana_Set_Reg(AUDTOP_CON5, index2 << 12, 0x00007000);

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Headset_PGAR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Headset_PGAR_Get = %d\n",
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	return 0;
}


static int Headset_PGAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */
	int index = 0;
	int index2 = 7;
	/* gain[] = {-5, -3, -1, 1, 3, 5, 7, 9}; */

	pr_warn("%s(), index = %d\n", __func__, ucontrol->value.enumerated.item[0]);

	if (ucontrol->value.enumerated.item[0] >= ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	if (index > index2)
		index = index2;

	index2 -= index;

	pr_warn("%s(), index = %d, index2 = %d\n", __func__, index, index2);

	Ana_Set_Reg(AUDTOP_CON5, index2 << 8, 0x000000700);

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static uint32 mHp_Impedance = 32;

static int Audio_Hp_Impedance_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_Hp_Impedance_Get = %d\n", mHp_Impedance);
	ucontrol->value.integer.value[0] = mHp_Impedance;
	return 0;

}

static int Audio_Hp_Impedance_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	mHp_Impedance = ucontrol->value.integer.value[0];
	pr_warn("%s Audio_Hp_Impedance_Set = 0x%x\n", __func__, mHp_Impedance);
	return 0;
}

static int Aud_Clk_Buf_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("\%s n", __func__);
	ucontrol->value.integer.value[0] = audck_buf_Count;
	return 0;
}

static int Aud_Clk_Buf_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s(), value = %d\n", __func__, ucontrol->value.enumerated.item[0]);
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
	/* here comes pga gain setting */
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN), DAC_DL_PGA_Headset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN), DAC_DL_PGA_Headset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN), DAC_DL_PGA_Handset_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN), DAC_DL_PGA_Speaker_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN), DAC_DL_PGA_Speaker_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aud_clk_buf_function), aud_clk_buf_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
};

static const struct snd_kcontrol_new mt6350_snd_controls[] = {
	SOC_ENUM_EXT("Audio_Amp_R_Switch", Audio_DL_Enum[0], Audio_AmpR_Get, Audio_AmpR_Set),
	SOC_ENUM_EXT("Audio_Amp_L_Switch", Audio_DL_Enum[1], Audio_AmpL_Get, Audio_AmpL_Set),
	SOC_ENUM_EXT("Voice_Amp_Switch", Audio_DL_Enum[2], Voice_Amp_Get, Voice_Amp_Set),
	SOC_ENUM_EXT("Speaker_Amp_Switch", Audio_DL_Enum[3], Speaker_Amp_Get, Speaker_Amp_Set),
	SOC_ENUM_EXT("Headset_Speaker_Amp_Switch", Audio_DL_Enum[4], Headset_Speaker_Amp_Get,
		     Headset_Speaker_Amp_Set),
	SOC_ENUM_EXT("Headset_PGAL_GAIN", Audio_DL_Enum[5], Headset_PGAL_Get, Headset_PGAL_Set),
	SOC_ENUM_EXT("Headset_PGAR_GAIN", Audio_DL_Enum[6], Headset_PGAR_Get, Headset_PGAR_Set),
	SOC_ENUM_EXT("Handset_PGA_GAIN", Audio_DL_Enum[7], Handset_PGA_Get, Handset_PGA_Set),
	SOC_ENUM_EXT("Lineout_PGAR_GAIN", Audio_DL_Enum[8], Lineout_PGAR_Get, Lineout_PGAR_Set),
	SOC_ENUM_EXT("Lineout_PGAL_GAIN", Audio_DL_Enum[9], Lineout_PGAL_Get, Lineout_PGAL_Set),
	SOC_ENUM_EXT("AUD_CLK_BUF_Switch", Audio_DL_Enum[10], Aud_Clk_Buf_Get, Aud_Clk_Buf_Set),
	SOC_ENUM_EXT("Ext_Speaker_Amp_Switch", Audio_DL_Enum[11], Ext_Speaker_Amp_Get,
		     Ext_Speaker_Amp_Set),
	SOC_SINGLE_EXT("Audio HP Impedance", SND_SOC_NOPM, 0, 512, 0, Audio_Hp_Impedance_Get,
		       Audio_Hp_Impedance_Set),
	SOC_ENUM_EXT("PMIC_REG_CLEAR", Audio_DL_Enum[12], PMIC_REG_CLEAR_Get, PMIC_REG_CLEAR_Set),
	SOC_ENUM_EXT("Ext_Speaker_Use", Audio_DL_Enum[13], Ext_Speaker_Use_Get,
		     Ext_Speaker_Use_Set),
};

static const struct snd_kcontrol_new mt6350_Voice_Switch[] = {
	/* SOC_DAPM_ENUM_EXT("Voice Mux", Audio_DL_Enum[10], Voice_Mux_Get, Voice_Mux_Set), */
};

void SetMicPGAGain(void)
{
	int index = 0;
	int index2 = 5;

	/* set mic1 PGA gain */
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
	pr_warn("%s(),mic1 index=%d,index2=%d\n", __func__, index, index2);
	if (index > index2)
		index = index2;

	/* const int PreAmpGain[] = {-6, 0, 6, 12, 18, 24}; */

	Ana_Set_Reg(AUDTOP_CON0, index << 4, 0x00000070);

	/* set mic2 PGA gain */
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
	pr_warn("%s(),mic2 index=%d,index2=%d\n", __func__, index, index2);
	if (index > index2)
		index = index2;

	Ana_Set_Reg(AUDTOP_CON1, index << 8, 0x00000700);

}

static bool GetAdcStatus(void)
{
	int i = 0;

	for (i = AUDIO_ANALOG_DEVICE_IN_ADC1; i < AUDIO_ANALOG_DEVICE_MAX; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}

/*
static bool GetDacStatus(void)
{
	int i = 0;

	for (i = AUDIO_ANALOG_DEVICE_OUT_EARPIECER; i < AUDIO_ANALOG_DEVICE_2IN1_SPK; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}
*/

static bool TurnOnADcPowerACC(int ADCType, bool enable)
{
	/* bool refmic_using_ADC_L; */
	pr_warn("%s, ADCType=%d, enable=%x,AdcStatus=%x ,DLStatus=%x\n", __func__, ADCType, enable,
	       GetAdcStatus(), GetDLStatus());
	if (enable) {
		uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];

		if (GetAdcStatus() == false) {
			audckbufEnable(true);

			/* here to set digital part */
			Topck_Enable(true);
			/* config UL up8x_rxif adc voice mode */
			Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x302F | (GetULNewIFFrequency(SampleRate_VUL1) << 10), 0xffff);
			/* UL sampling rate */
			Ana_Set_Reg(ABB_AFE_CON1, GetULFrequency(SampleRate_VUL1) << 4, 0x0010);

#ifdef PMIC_RESET_WORKAROUND
			pr_warn("%s ,PMIC_RESET_WORKAROUND reset ABB_AFE_CON2/ABB_AFE_CON4\n",
			       __func__);
			Ana_Set_Reg(ABB_AFE_CON2, 0x0, 0xffff);	/* reset to default value */
			Ana_Set_Reg(ABB_AFE_CON4, 0x0, 0xffff);	/* reset to default value */
#endif
			TopCtlChangeTrigger();
			Ana_Set_Reg(ABB_AFE_CON0, 0x0002, 0x0002);	/* turn on UL */
		}

		SetMicPGAGain();

		/* L channel  //main and headset mic */
		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {
			Ana_Set_Reg(AUDTOP_CON0, 0x7800, 0x7f80);	/* Enable LCH 1.4v, 2.4V */
			Ana_Set_Reg(AUDTOP_CON2, 0x00F0, 0x00ff);	/* Enable RCH 1.4V, 2.4V */

			/* Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000100); */
			/* "ADC1", main_mic */
			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0)	{
				Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000100);
				Ana_Set_Reg(AUDTOP_CON0, 0, 0x0000000f);
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 1) {
				/* "ADC2", headset mic */
				Ana_Set_Reg(AUDTOP_CON0, 1, 0x0000000f);	/* L */
				Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000200);
				Ana_Set_Reg(AUDTOP_CON1, 0x0010, 0x000000f0);	/* R */
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 2) {
				/* "ADC3", ref mic */
				Ana_Set_Reg(AUDTOP_CON0, 4, 0x0000000f);
				Ana_Set_Reg(AUDTOP_CON3, 0x0100, 0x00000100);
			}
			/* Ana_Set_Reg(AUDTOP_CON1 , 0x0300, 0x0700);//RCH PGA gain +12dB */
			/* Ana_Set_Reg(AUDTOP_CON0 , 0x0030, 0x0070);//LCH PGA gain +12dB */
			Ana_Set_Reg(AUDTOP_CON8, 0x0008, 0x0008);	/* MICBIAS */
			Ana_Set_Reg(AUDTOP_CON0, 0x0180, 0x0180);	/* Enable LCH ADC, PGA */
			Ana_Set_Reg(AUDTOP_CON2, 0x0003, 0x0003);	/* Enable RCH ADC, PGA */

		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {
			/* Right Channel //ref mic */
			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2] == 2)	{
				/* "ADC3", ref mic */
				/* need to move to dual mic first open when enable ADC1? */
				Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000200);
				Ana_Set_Reg(AUDTOP_CON1, 0x0000, 0x000000f0);
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2] == 1) {
				/* "ADC2", headset  mic */
				Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000100);
				Ana_Set_Reg(AUDTOP_CON1, 0x0010, 0x000000f0);	/* R */
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2] == 0) {
				/* "ADC1", main mic */
				Ana_Set_Reg(AUDTOP_CON3, 0x0200, 0x00000200);
				Ana_Set_Reg(AUDTOP_CON1, 0x0040, 0x000000f0);
			} else {
				/* need to move to dual mic first open when enable ADC1? */
				Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000200);
				Ana_Set_Reg(AUDTOP_CON1, 0x0000, 0x000000f0);
			}
		}


	} else {

		if (GetAdcStatus() == false) {
			Ana_Set_Reg(AUDTOP_CON0, 0x0000, 0x0180);	/* Disable LCH ADC, PGA */
			Ana_Set_Reg(AUDTOP_CON2, 0x0000, 0x0003);	/* Disable RCH ADC, PGA */

			if (GetDLStatus() == false)
				Ana_Set_Reg(AUDTOP_CON0, 0x6000, 0x7f80);	/* Disable LCH 1.4v, 2.4V */
			else
				Ana_Set_Reg(AUDTOP_CON0, 0x7000, 0x7f80);	/* Disable LCH 2.4V, keep 1.4V */

			/* Disable RCH 1.4V, 2.4V ALPS00824353 , always disable RG_AUDULR_VCMSEL */
			Ana_Set_Reg(AUDTOP_CON2, 0x00C0, 0x00ff);

			Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000100);
			Ana_Set_Reg(AUDTOP_CON0, 0x0003, 0x0000000f);
			Ana_Set_Reg(AUDTOP_CON3, 0x0000, 0x00000200);
			Ana_Set_Reg(AUDTOP_CON1, 0x0020, 0x000000f0);

			Ana_Set_Reg(AUDTOP_CON8, 0x0000, 0x0008);	/* MICBIAS */
			Ana_Set_Reg(ABB_AFE_CON0, 0x0000, 0x0002);	/* turn off UL */
			Topck_Enable(false);

			audckbufEnable(false);
		}
	}
	return true;
}

static bool TurnOnADcPowerDmic(int ADCType, bool enable)
{
	pr_warn("%s ADCType = %d enable = %d, AdcStatus=%x\n", __func__, ADCType, enable,
	       GetAdcStatus());

	if (enable) {
		uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];

		if (GetAdcStatus() == false) {
			audckbufEnable(true);
			/* here to set digital part */
			Topck_Enable(true);
			/* config UL up8x_rxif adc voice mode */
			Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x302F | (GetULNewIFFrequency(SampleRate_VUL1) << 10), 0xffff);
			/* UL sampling rate */
			Ana_Set_Reg(ABB_AFE_CON1, GetULFrequency(SampleRate_VUL1) << 4, 0x0010);

#ifdef PMIC_RESET_WORKAROUND
			pr_warn("%s ,PMIC_RESET_WORKAROUND reset ABB_AFE_CON2/ABB_AFE_CON4\n",
			       __func__);
			Ana_Set_Reg(ABB_AFE_CON2, 0x0, 0xffff);	/* reset to default value */
			Ana_Set_Reg(ABB_AFE_CON4, 0x0, 0xffff);	/* reset to default value */
#endif

			TopCtlChangeTrigger();
			SetMicPGAGain();
			/* enable digital mic, 3.25M clock rate, bit8:0(one wire)/1(two wire), one wire */
			/* Ana_Set_Reg(ABB_AFE_CON9, 0x0011, 0x0011); */
			/* enable digital mic, 3.25M clock rate, bit8:0(one wire)/1(two wire), two wire */
			Ana_Set_Reg(ABB_AFE_CON9, 0x0111, 0x0111);
			Ana_Set_Reg(ABB_AFE_CON0, 0x0002, 0x0002);	/* turn on UL */

			Ana_Set_Reg(AUDTOP_CON8, 0x020C, 0x03FF);	/* MICBIAS, digital mic enable */
		}

	} else {
		if (GetAdcStatus() == false) {
			Ana_Set_Reg(AUDTOP_CON8, 0x0000, 0x000C);	/* MICBIAS, digital mic disable */
			Ana_Set_Reg(ABB_AFE_CON9, 0x0000, 0x0010);	/* disable digital mic */
			Ana_Set_Reg(ABB_AFE_CON0, 0x0000, 0x0002);	/* turn off UL */

			Topck_Enable(false);
			audckbufEnable(false);
		}
	}
	return true;
}

static bool TurnOnADcPowerDCC(int ADCType, bool enable)
{
	pr_warn("%s ADCType = %d enable = %d, not support\n", __func__, ADCType, enable);

	return true;
}


static bool TurnOnADcPowerDCCECM(int ADCType, bool enable)
{
	/* use TurnOnADcPowerDCC() with SetDCcoupleNP() setting ECM or not depending on
	mAudio_Analog_Mic1_mode/mAudio_Analog_Mic2_mode */
	TurnOnADcPowerDCC(ADCType, enable);
	return true;
}

static bool TurnOnVOWDigitalHW(bool enable)
{
	pr_debug("%s enable = %d 82 no this\n", __func__, enable);

	return true;
}

static bool TurnOnVOWADcPowerACC(int MicType, bool enable)
{
	pr_debug("%s, MicType=%d, enable = %d 82 no this\n", __func__, MicType, enable);
	return true;
}



/* here start uplink power function */
static const char * const ADC_function[] = { "Off", "On" };
static const char * const ADC_power_mode[] = { "normal", "lowpower" };
/* OPEN:0, IN_ADC1: 1, IN_ADC2:2, IN_ADC3:3 */
static const char * const PreAmp_Mux_function[] = { "OPEN", "IN_ADC1", "IN_ADC2", "IN_ADC3" };
static const char * const ADC_UL_PGA_GAIN[] = { "-6Db", "0Db", "6Db", "12Db", "18Db", "24Db" };
static const char * const Pmic_Digital_Mux[] = { "ADC1", "ADC2", "ADC3", "ADC4" };
static const char * const Adc_Input_Sel[] = { "idle", "AIN", "Preamp" };
static const char * const Audio_AnalogMic_Mode[] = {"ACCMODE", "DCCMODE", "DMIC",
"DCCECMDIFFMODE", "DCCECMSINGLEMODE" };
static const char * const Audio_VOW_ADC_Function[] = { "Off", "On" };
static const char * const Audio_VOW_Digital_Function[] = { "Off", "On" };
static const char * const Audio_VOW_MIC_Type[] = { "HandsetAMIC", "HeadsetMIC", "HandsetDMIC",
"HandsetDMIC_800K", "HandsetAMIC_DCC", "HeadsetMIC_DCC", "HandsetAMIC_DCCECM", "HeadsetMIC_DCCECM"};


static const struct soc_enum Audio_UL_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(PreAmp_Mux_function), PreAmp_Mux_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Adc_Input_Sel), Adc_Input_Sel),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_UL_PGA_GAIN), ADC_UL_PGA_GAIN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Digital_Mux), Pmic_Digital_Mux),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode), Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_power_mode), ADC_power_mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_ADC_Function), Audio_VOW_ADC_Function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(PreAmp_Mux_function), PreAmp_Mux_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_Digital_Function), Audio_VOW_Digital_Function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_MIC_Type), Audio_VOW_MIC_Type),
};

static int Audio_ADC1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_ADC1_Get = %d\n",
	       mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1];
	return 0;
}

static int Audio_ADC1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	mutex_lock(&Ana_Power_Mutex);
	if (ucontrol->value.integer.value[0]) {
		if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF
			   || mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC1, true);

		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
		    ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF
			   || mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
	}
	mutex_unlock(&Ana_Power_Mutex);
	return 0;
}

static int Audio_ADC2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_ADC2_Get = %d\n",
	       mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2];
	return 0;
}

static int Audio_ADC2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	mutex_lock(&Ana_Power_Mutex);
	if (ucontrol->value.integer.value[0]) {
		if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2, true);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, true);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2, true);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF
			   || mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC2, true);

		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
		    ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF
			   || mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCCECM(AUDIO_ANALOG_DEVICE_IN_ADC2, false);

	}
	mutex_unlock(&Ana_Power_Mutex);
	return 0;
}

static int Audio_ADC3_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_ADC3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_ADC4_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_ADC4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_ADC1_Sel_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s() = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1]);
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1];
	return 0;
}

static int Audio_ADC1_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
#if 0	/* todo */
	if (ucontrol->value.integer.value[0] == 0) {
		/* pinumx sel */
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x0000 << 9), 0x0600);
	} else if (ucontrol->value.integer.value[0] == 1) {
		/* AIN0 */
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x0001 << 9), 0x0600);
	} else if (ucontrol->value.integer.value[0] == 2) {
		/* ADC2 */
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x0002 << 9), 0x0600);	/* Left preamp */
	} else {
		pr_err("%s() warning\n ", __func__);
	}
#endif
	pr_warn("%s() done\n", __func__);
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1] = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_ADC2_Sel_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s() = %d\n", __func__, mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2]);
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2];
	return 0;
}

static int Audio_ADC2_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
#if 0	/* todo */
	if (ucontrol->value.integer.value[0] == 0) {
		/* pinumx sel */
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x0000 << 9), 0x0600);
	} else if (ucontrol->value.integer.value[0] == 1) {
		/* AIN2 */
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x0001 << 9), 0x0600);
	} else if (ucontrol->value.integer.value[0] == 2) {
		/* Right preamp */
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x0002 << 9), 0x0600);
	} else {
		pr_err("%s() warning\n ", __func__);
	}
#endif
	pr_warn("%s() done\n", __func__);
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2] = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_ADC3_Sel_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_ADC3_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_ADC4_Sel_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_ADC4_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}


static bool AudioPreAmp1_Sel(int Mul_Sel)
{
	pr_debug("%s Mul_Sel = %d ", __func__, Mul_Sel);
#if 0	/* todo */
	if (Mul_Sel == 0) {
		/* pinumx open */
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x00C0);
	} else if (Mul_Sel == 1) {
		/* AIN0 */
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0040, 0x00C0);
	} else if (Mul_Sel == 2) {
		/* AIN1 */
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0080, 0x00C0);
	} else if (Mul_Sel == 3) {
		/* AIN2 */
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x00C0, 0x00C0);
	} else {
		pr_err("AudioPreAmp1_Sel warning");
	}
#endif
	return true;
}


static int Audio_PreAmp1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]; = %d\n", __func__,
	       mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1];
	return 0;
}

static int Audio_PreAmp1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] =
	    ucontrol->value.integer.value[0];
	AudioPreAmp1_Sel(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);
	pr_debug("%s() done\n", __func__);
	return 0;
}

static bool AudioPreAmp2_Sel(int Mul_Sel)
{
	pr_debug("%s Mul_Sel = %d ", __func__, Mul_Sel);
#if 0				/* todo */
	if (Mul_Sel == 0) {
		/* pinumx open */
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x00C0);
	} else if (Mul_Sel == 1) {
		/* AIN2 */
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x00C0, 0x00C0);
	} else if (Mul_Sel == 2) {
		/* AIN1 */
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0080, 0x00C0);
	} else if (Mul_Sel == 3) {
		/* AIN0 */
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0040, 0x00C0);
	} else {
		pr_err("AudioPreAmp1_Sel warning");
	}
#endif
	return true;
}


static int Audio_PreAmp2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2]; = %d\n", __func__,
	       mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2];
	return 0;
}

static int Audio_PreAmp2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2] =
	    ucontrol->value.integer.value[0];
	AudioPreAmp2_Sel(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2]);
	pr_debug("%s() done\n", __func__);
	return 0;
}

/* PGA1: PGA_L */
static int Audio_PGA1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_PGA1_Get = %d\n",
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
	return 0;
}

static int Audio_PGA1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;
	int index2 = 5;

	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	pr_warn("%s(),index=%d\n", __func__, index);
	/* index = index / 6; */
	if (index > index2)
		index = index2;

	/* const int PreAmpGain[] = {-6, 0, 6, 12, 18, 24}; */
	/* index2 -= index; */
	pr_warn("%s(),index=%d,index2=%d\n", __func__, index, index2);

	Ana_Set_Reg(AUDTOP_CON0, index << 4, 0x00000070);

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] =
	    ucontrol->value.integer.value[0];
	return 0;
}

/* PGA2: PGA_R */
static int Audio_PGA2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_PGA2_Get = %d\n",
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
	return 0;
}

static int Audio_PGA2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;
	int index2 = 5;

	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	pr_warn("%s(),index=%d\n", __func__, index);

	if (index > index2)
		index = index2;

	/* const int PreAmpGain[] = {-6, 0, 6, 12, 18, 24}; */

	pr_warn("%s(),index=%d\n", __func__, index);

	Ana_Set_Reg(AUDTOP_CON1, index << 8, 0x00000700);

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_PGA3_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_PGA3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_PGA4_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_PGA4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_MicSource1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_MicSource1_Get = %d\n",
	       mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]);
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1];
	return 0;
}

static int Audio_MicSource1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 80 used for ADC1 Mic source selection, "ADC1" is main_mic, "ADC2" is headset_mic, "ADC3" is ref main */
	int index = 0;

	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Digital_Mux)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	pr_warn("%s() index = %d done\n", __func__, index);
	mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] = ucontrol->value.integer.value[0];

	return 0;
}

static int Audio_MicSource2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_MicSource2_Get = %d\n",
	       mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2]);
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2];
	return 0;
}

static int Audio_MicSource2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Digital_Mux)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	pr_warn("%s() index = %d done\n", __func__, index);
	mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_2] = ucontrol->value.integer.value[0];

	return 0;
}

static int Audio_MicSource3_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_MicSource3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}


static int Audio_MicSource4_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_MicSource4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

/* Mic ACC/DCC Mode Setting */
static int Audio_Mic1_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s() mAudio_Analog_Mic1_mode = %d\n", __func__, mAudio_Analog_Mic1_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic1_mode;
	return 0;
}

static int Audio_Mic1_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic1_mode = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_Analog_Mic1_mode = %d\n", __func__, mAudio_Analog_Mic1_mode);
	return 0;
}

static int Audio_Mic2_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_Analog_Mic2_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic2_mode;
	return 0;
}

static int Audio_Mic2_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic2_mode = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_Analog_Mic2_mode = %d\n", __func__, mAudio_Analog_Mic2_mode);
	return 0;
}


static int Audio_Mic3_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_Analog_Mic3_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic3_mode;
	return 0;
}

static int Audio_Mic3_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic3_mode = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_Analog_Mic3_mode = %d\n", __func__, mAudio_Analog_Mic3_mode);
	return 0;
}

static int Audio_Mic4_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_Analog_Mic4_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic4_mode;
	return 0;
}

static int Audio_Mic4_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic4_mode = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_Analog_Mic4_mode = %d\n", __func__, mAudio_Analog_Mic4_mode);
	return 0;
}

static int Audio_Adc_Power_Mode_Get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAdc_Power_Mode);
	ucontrol->value.integer.value[0] = mAdc_Power_Mode;
	return 0;
}

static int Audio_Adc_Power_Mode_Set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_power_mode)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mAdc_Power_Mode = ucontrol->value.integer.value[0];
	pr_warn("%s() mAdc_Power_Mode = %d\n", __func__, mAdc_Power_Mode);
	return 0;
}


static int Audio_Vow_ADC_Func_Switch_Get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_Vow_Analog_Func_Enable);
	ucontrol->value.integer.value[0] = mAudio_Vow_Analog_Func_Enable;
	return 0;
}

static int Audio_Vow_ADC_Func_Switch_Set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_VOW_ADC_Function)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		TurnOnVOWADcPowerACC(mAudio_VOW_Mic_type, true);
	else
		TurnOnVOWADcPowerACC(mAudio_VOW_Mic_type, false);

	mAudio_Vow_Analog_Func_Enable = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_Vow_Analog_Func_Enable = %d\n", __func__,
	       mAudio_Vow_Analog_Func_Enable);
	return 0;
}

static int Audio_Vow_Digital_Func_Switch_Get(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_Vow_Digital_Func_Enable);
	ucontrol->value.integer.value[0] = mAudio_Vow_Digital_Func_Enable;
	return 0;
}

static int Audio_Vow_Digital_Func_Switch_Set(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_VOW_Digital_Function)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0]) {
		/* turn on VOW hardware */
		TurnOnVOWDigitalHW(true);
	} else {
		/* turn off VOW hardware */
		TurnOnVOWDigitalHW(false);
	}

	mAudio_Vow_Digital_Func_Enable = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_Vow_Digital_Func_Enable = %d\n", __func__,
	       mAudio_Vow_Digital_Func_Enable);
	return 0;
}


static int Audio_Vow_MIC_Type_Select_Get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, mAudio_VOW_Mic_type);
	ucontrol->value.integer.value[0] = mAudio_VOW_Mic_type;
	return 0;
}

static int Audio_Vow_MIC_Type_Select_Set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_VOW_MIC_Type)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_VOW_Mic_type = ucontrol->value.integer.value[0];
	pr_warn("%s() mAudio_VOW_Mic_type = %d\n", __func__, mAudio_VOW_Mic_type);
	return 0;
}


static int Audio_Vow_Cfg0_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value = /*Ana_Get_Reg(AFE_VOW_CFG0) */ reg_AFE_VOW_CFG0;

	pr_warn("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg0_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %d\n", __func__, (int)(ucontrol->value.integer.value[0]));
	/* Ana_Set_Reg(AFE_VOW_CFG0, ucontrol->value.integer.value[0], 0xffff); */
	reg_AFE_VOW_CFG0 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value = /*Ana_Get_Reg(AFE_VOW_CFG1) */ reg_AFE_VOW_CFG1;

	pr_warn("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	/* Ana_Set_Reg(AFE_VOW_CFG1, ucontrol->value.integer.value[0], 0xffff); */
	reg_AFE_VOW_CFG1 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value = /*Ana_Get_Reg(AFE_VOW_CFG2) */ reg_AFE_VOW_CFG2;

	pr_warn("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	/* Ana_Set_Reg(AFE_VOW_CFG2, ucontrol->value.integer.value[0], 0xffff); */
	reg_AFE_VOW_CFG2 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg3_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value = /*Ana_Get_Reg(AFE_VOW_CFG3) */ reg_AFE_VOW_CFG3;

	pr_warn("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	/* Ana_Set_Reg(AFE_VOW_CFG3, ucontrol->value.integer.value[0], 0xffff); */
	reg_AFE_VOW_CFG3 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg4_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value = /*Ana_Get_Reg(AFE_VOW_CFG4) */ reg_AFE_VOW_CFG4;

	pr_warn("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	/* Ana_Set_Reg(AFE_VOW_CFG4, ucontrol->value.integer.value[0], 0xffff); */
	reg_AFE_VOW_CFG4 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg5_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value = /*Ana_Get_Reg(AFE_VOW_CFG5) */ reg_AFE_VOW_CFG5;

	pr_warn("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg5_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	/* Ana_Set_Reg(AFE_VOW_CFG5, ucontrol->value.integer.value[0], 0xffff); */
	reg_AFE_VOW_CFG5 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_State_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value = mIsVOWOn;

	pr_warn("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_State_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* printk("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]); */
	/* reg_AFE_VOW_CFG5 = ucontrol->value.integer.value[0]; */
	return 0;
}

static bool SineTable_DAC_HP_flag;
static bool SineTable_UL2_flag;

static int SineTable_UL2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
#if 0				/* todo */

	if (ucontrol->value.integer.value[0]) {
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0002, 0x2);	/* set DL sine gen table */
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0080, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
	} else {
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0002, 0x2);	/* set DL sine gen table */
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0000, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
	}
#endif
	return 0;
}

static int SineTable_UL2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = SineTable_UL2_flag;
	return 0;
}

static int SineTable_DAC_HP_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = SineTable_DAC_HP_flag;
	return 0;
}

static int SineTable_DAC_HP_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* TODO? */
	pr_debug("%s()\n", __func__);
	return 0;
}

static void ADC_LOOP_DAC_Func(int command)
{
	/* TODO? */
}

static bool DAC_LOOP_DAC_HS_flag;
static int ADC_LOOP_DAC_HS_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = DAC_LOOP_DAC_HS_flag;
	return 0;
}

static int ADC_LOOP_DAC_HS_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.integer.value[0]) {
		DAC_LOOP_DAC_HS_flag = ucontrol->value.integer.value[0];
		ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HS_ON);
	} else {
		DAC_LOOP_DAC_HS_flag = ucontrol->value.integer.value[0];
		ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HS_OFF);
	}
	return 0;
}

static bool DAC_LOOP_DAC_HP_flag;
static int ADC_LOOP_DAC_HP_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = DAC_LOOP_DAC_HP_flag;
	return 0;
}

static int ADC_LOOP_DAC_HP_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

	pr_debug("%s()\n", __func__);
	if (ucontrol->value.integer.value[0]) {
		DAC_LOOP_DAC_HP_flag = ucontrol->value.integer.value[0];
		ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HP_ON);
	} else {
		DAC_LOOP_DAC_HP_flag = ucontrol->value.integer.value[0];
		ADC_LOOP_DAC_Func(AUDIO_ANALOG_DAC_LOOP_DAC_HP_OFF);
	}
	return 0;
}

static bool Voice_Call_DAC_DAC_HS_flag;
static int Voice_Call_DAC_DAC_HS_Get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = Voice_Call_DAC_DAC_HS_flag;
	return 0;
}

static int Voice_Call_DAC_DAC_HS_Set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	return 0;
}

static const char * const Pmic_Test_function[] = { "Off", "On" };
static const char * const Pmic_LPBK_function[] = { "Off", "LPBK3" };
static int32 Pmic_Loopback_Type;
static int32 TurnOn_ULDL_16K_Type;

static int Pmic_Loopback_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = Pmic_Loopback_Type;
	return 0;
}

static int Pmic_Loopback_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_LPBK_function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	if (ucontrol->value.integer.value[0] == 0) { /* disable pmic lpbk */

		/* downlink data loop back to uplink */
		Ana_Set_Reg(ABB_AFE_CON2, 0x0, 0x8);
		Ana_Set_Reg(ABB_AFE_CON0, 0x0000, 0x0003);	/* turn off UL */
		Topck_Enable(false);
		ClsqEnable(false);
		audckbufEnable(false);
	} else if (ucontrol->value.integer.value[0] > 0) { /* enable pmic lpbk */
		audckbufEnable(true);
		ClsqEnable(true);
		Topck_Enable(true);

		/* downlink data loop back to uplink */
		Ana_Set_Reg(ABB_AFE_CON2, 0x8, 0x8);

		/* turn on DL & UL */
		Ana_Set_Reg(ABB_AFE_CON0, 0x0003, 0x0003);
	}

	pr_warn("%s() done\n", __func__);
	Pmic_Loopback_Type = ucontrol->value.integer.value[0];
	return 0;
}

static int TurnOn_ULDL_16K_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = TurnOn_ULDL_16K_Type;
	return 0;
}

static int TurnOn_ULDL_16K_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	uint32 top_ctrl_status_now;
	uint16 rReg = 0;
	uint16 i = 3;

	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Test_function)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0]) {		/* enable pmic lpbk */
		/* clk */
		Ana_Set_Reg(TOP_CKPDN1, 0x12ff, 0xffff);
		Ana_Set_Reg(TOP_CKPDN1_CLR, 0x100, 0xffff);

		Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0, 0x3300, 0xffff);
		Ana_Set_Reg(ABB_AFE_CON1, 0x4, 0xffff);
		Ana_Set_Reg(ABB_AFE_CON11, 0x0200, 0x0200);
		top_ctrl_status_now = Ana_Get_Reg(ABB_AFE_CON11);
		Ana_Set_Reg(ABB_AFE_CON11, ((top_ctrl_status_now & 0x0001) ? 0 : 1) << 8, 0x0100);
		Ana_Set_Reg(ABB_AFE_CON0, 0x0001, 0x0001);	/* turn on DL */

		/* set analog part (voice HS playback) */
		Ana_Set_Reg(AUDTOP_CON7, 0x2430, 0xffff);	/* Set voice buffer to smallest -22dB. */
		/* enable input short of HP to prevent voice signal leakage . Enable 2.4V. */
		Ana_Set_Reg(AUDTOP_CON6, 0xB7F6, 0xffff);
		/* Depop. Enable audio clock */
		Ana_Set_Reg(AUDTOP_CON0, 0x7000, 0xf000);	/* enable clean 1.35VCM buffer in audioUL */
		Ana_Set_Reg(AUDTOP_CON4, 0x0014, 0xffff);	/* enable audio bias. enable LCH DAC */
		for (i = 3; i < 11; i++) {
			/* udelay(5 * 1000); */
			mdelay(5);
			rReg = 0x2500 + (i << 4);
			/* enable voice buffer and -1dB gain. ramp up volume from -21dB to -1dB here */
			Ana_Set_Reg(AUDTOP_CON7, rReg, 0xffff);
		}

		Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x342f, 0xffff);
		Ana_Set_Reg(ABB_AFE_CON1, 0x4, 0xffff);
		top_ctrl_status_now = Ana_Get_Reg(ABB_AFE_CON11);
		Ana_Set_Reg(ABB_AFE_CON11, ((top_ctrl_status_now & 0x0001) ? 0 : 1) << 8, 0x0100);
		Ana_Set_Reg(ABB_AFE_CON0, 0x0002, 0x0002);	/* turn on UL */

		Ana_Set_Reg(AUDTOP_CON0, 0x7810, 0xffff);
		Ana_Set_Reg(AUDTOP_CON1, 0x100, 0xffff);
		Ana_Set_Reg(AUDTOP_CON2, 0xf0, 0xffff);
		Ana_Set_Reg(AUDTOP_CON8, 0x208, 0xffff);
		Ana_Set_Reg(AUDTOP_CON0, 0x7990, 0xffff);
		Ana_Set_Reg(AUDTOP_CON2, 0xf3, 0xffff);

	} else {						/* disable pmic lpbk */
		Voice_Amp_Change(false);
		TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
	}

	pr_warn("%s() done\n", __func__);
	Pmic_Loopback_Type = ucontrol->value.integer.value[0];
	return 0;
}

/* here start uplink power function */

static const struct soc_enum Pmic_Test_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_LPBK_function), Pmic_LPBK_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
};

static const struct snd_kcontrol_new mt6350_pmic_Test_controls[] = {
	SOC_ENUM_EXT("SineTable_DAC_HP", Pmic_Test_Enum[0], SineTable_DAC_HP_Get,
		     SineTable_DAC_HP_Set),
	SOC_ENUM_EXT("DAC_LOOP_DAC_HS", Pmic_Test_Enum[1], ADC_LOOP_DAC_HS_Get,
		     ADC_LOOP_DAC_HS_Set),
	SOC_ENUM_EXT("DAC_LOOP_DAC_HP", Pmic_Test_Enum[2], ADC_LOOP_DAC_HP_Get,
		     ADC_LOOP_DAC_HP_Set),
	SOC_ENUM_EXT("Voice_Call_DAC_DAC_HS", Pmic_Test_Enum[3], Voice_Call_DAC_DAC_HS_Get,
		     Voice_Call_DAC_DAC_HS_Set),
	SOC_ENUM_EXT("SineTable_UL2", Pmic_Test_Enum[4], SineTable_UL2_Get, SineTable_UL2_Set),
	SOC_ENUM_EXT("Pmic_Loopback", Pmic_Test_Enum[5], Pmic_Loopback_Get, Pmic_Loopback_Set),
	SOC_ENUM_EXT("TurnOn_ULDL_16K", Pmic_Test_Enum[6], TurnOn_ULDL_16K_Get, TurnOn_ULDL_16K_Set),
};

static const struct snd_kcontrol_new mt6350_UL_Codec_controls[] = {
	SOC_ENUM_EXT("Audio_ADC_1_Switch", Audio_UL_Enum[0], Audio_ADC1_Get, Audio_ADC1_Set),
	SOC_ENUM_EXT("Audio_ADC_2_Switch", Audio_UL_Enum[1], Audio_ADC2_Get, Audio_ADC2_Set),
	SOC_ENUM_EXT("Audio_ADC_3_Switch", Audio_UL_Enum[2], Audio_ADC3_Get, Audio_ADC3_Set),
	SOC_ENUM_EXT("Audio_ADC_4_Switch", Audio_UL_Enum[3], Audio_ADC4_Get, Audio_ADC4_Set),
	SOC_ENUM_EXT("Audio_Preamp1_Switch", Audio_UL_Enum[4], Audio_PreAmp1_Get,
		     Audio_PreAmp1_Set),
	SOC_ENUM_EXT("Audio_ADC_1_Sel", Audio_UL_Enum[5], Audio_ADC1_Sel_Get, Audio_ADC1_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_2_Sel", Audio_UL_Enum[6], Audio_ADC2_Sel_Get, Audio_ADC2_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_3_Sel", Audio_UL_Enum[7], Audio_ADC3_Sel_Get, Audio_ADC3_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_4_Sel", Audio_UL_Enum[8], Audio_ADC4_Sel_Get, Audio_ADC4_Sel_Set),
	SOC_ENUM_EXT("Audio_PGA1_Setting", Audio_UL_Enum[9], Audio_PGA1_Get, Audio_PGA1_Set),
	SOC_ENUM_EXT("Audio_PGA2_Setting", Audio_UL_Enum[10], Audio_PGA2_Get, Audio_PGA2_Set),
	SOC_ENUM_EXT("Audio_PGA3_Setting", Audio_UL_Enum[11], Audio_PGA3_Get, Audio_PGA3_Set),
	SOC_ENUM_EXT("Audio_PGA4_Setting", Audio_UL_Enum[12], Audio_PGA4_Get, Audio_PGA4_Set),
	SOC_ENUM_EXT("Audio_MicSource1_Setting", Audio_UL_Enum[13], Audio_MicSource1_Get,
		     Audio_MicSource1_Set),
	SOC_ENUM_EXT("Audio_MicSource2_Setting", Audio_UL_Enum[14], Audio_MicSource2_Get,
		     Audio_MicSource2_Set),
	SOC_ENUM_EXT("Audio_MicSource3_Setting", Audio_UL_Enum[15], Audio_MicSource3_Get,
		     Audio_MicSource3_Set),
	SOC_ENUM_EXT("Audio_MicSource4_Setting", Audio_UL_Enum[16], Audio_MicSource4_Get,
		     Audio_MicSource4_Set),
	SOC_ENUM_EXT("Audio_MIC1_Mode_Select", Audio_UL_Enum[17], Audio_Mic1_Mode_Select_Get,
		     Audio_Mic1_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC2_Mode_Select", Audio_UL_Enum[18], Audio_Mic2_Mode_Select_Get,
		     Audio_Mic2_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC3_Mode_Select", Audio_UL_Enum[19], Audio_Mic3_Mode_Select_Get,
		     Audio_Mic3_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC4_Mode_Select", Audio_UL_Enum[20], Audio_Mic4_Mode_Select_Get,
		     Audio_Mic4_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_Mic_Power_Mode", Audio_UL_Enum[21], Audio_Adc_Power_Mode_Get,
		     Audio_Adc_Power_Mode_Set),
	SOC_ENUM_EXT("Audio_Vow_ADC_Func_Switch", Audio_UL_Enum[22], Audio_Vow_ADC_Func_Switch_Get,
		     Audio_Vow_ADC_Func_Switch_Set),
	SOC_ENUM_EXT("Audio_Preamp2_Switch", Audio_UL_Enum[23], Audio_PreAmp2_Get,
		     Audio_PreAmp2_Set),
	SOC_ENUM_EXT("Audio_Vow_Digital_Func_Switch", Audio_UL_Enum[24],
		     Audio_Vow_Digital_Func_Switch_Get, Audio_Vow_Digital_Func_Switch_Set),
	SOC_ENUM_EXT("Audio_Vow_MIC_Type_Select", Audio_UL_Enum[25], Audio_Vow_MIC_Type_Select_Get,
		     Audio_Vow_MIC_Type_Select_Set),
	SOC_SINGLE_EXT("Audio VOWCFG0 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg0_Get,
		       Audio_Vow_Cfg0_Set),
	SOC_SINGLE_EXT("Audio VOWCFG1 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg1_Get,
		       Audio_Vow_Cfg1_Set),
	SOC_SINGLE_EXT("Audio VOWCFG2 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg2_Get,
		       Audio_Vow_Cfg2_Set),
	SOC_SINGLE_EXT("Audio VOWCFG3 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg3_Get,
		       Audio_Vow_Cfg3_Set),
	SOC_SINGLE_EXT("Audio VOWCFG4 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg4_Get,
		       Audio_Vow_Cfg4_Set),
	SOC_SINGLE_EXT("Audio VOWCFG5 Data", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_Cfg5_Get,
		       Audio_Vow_Cfg5_Set),
	SOC_SINGLE_EXT("Audio_VOW_State", SND_SOC_NOPM, 0, 0x80000, 0, Audio_Vow_State_Get,
		       Audio_Vow_State_Set),
};

static const struct snd_soc_dapm_widget mt6350_dapm_widgets[] = {
	/* Outputs */
	SND_SOC_DAPM_OUTPUT("EARPIECE"),
	SND_SOC_DAPM_OUTPUT("HEADSET"),
	SND_SOC_DAPM_OUTPUT("SPEAKER"),
	/*
	   SND_SOC_DAPM_MUX_E("VOICE_Mux_E", SND_SOC_NOPM, 0, 0  , &mt6350_Voice_Switch, codec_enable_rx_bias,
	   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
	   SND_SOC_DAPM_PRE_REG | SND_SOC_DAPM_POST_REG),
	 */

};

static const struct snd_soc_dapm_route mtk_audio_map[] = {
	{"VOICE_Mux_E", "Voice Mux", "SPEAKER PGA"},
};

static void mt6350_codec_init_reg(struct snd_soc_codec *codec)
{
	pr_warn("%s\n", __func__);
#if 0	/* todo */
	Ana_Set_Reg(TOP_CLKSQ, 0x0, 0x0001);	/* Disable CLKSQ 26MHz */
	Ana_Set_Reg(AUDDEC_ANA_CON8, 0x0002, 0x0002);	/* disable AUDGLB */
	/* Turn off AUDNCP_CLKDIV engine clock,Turn off AUD 26M */
	Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0x3800, 0x3800);
	/* Disable HeadphoneL/HeadphoneR/voice short circuit protection */
	Ana_Set_Reg(AUDDEC_ANA_CON0, 0xe000, 0xe000);
#else
	/* need to set this when boot up */
	AudDrv_Clk_On();
	audckbufEnable(true);
	pr_warn("%s, reset pmic\n", __func__);
	Ana_Set_Reg(TOP_RST_CON_SET, 0x4, 0x0004);
	Ana_Set_Reg(TOP_RST_CON_CLR, 0x4, 0x0004);
	pr_warn("%s, reset pmic  done\n", __func__);
	Ana_Set_Reg(AUDTOP_CON0, 0x0002, 0x000F);	/* Set UL PGA L MUX as open */
	Ana_Set_Reg(AUDTOP_CON1, 0x0020, 0x00F0);	/* Set UL PGA R MUX as open */
	Ana_Set_Reg(AUDTOP_CON5, 0x1114, 0xFFFF);	/* Set audio DAC Bias to 50% */
	Ana_Set_Reg(AUDTOP_CON6, 0x37A2, 0xFFFF);
	Ana_Set_Reg(AUDTOP_CON6, 0x37E2, 0xFFFF);	/* Enable the depop MUX of HP drivers */
	pr_warn("%s, inverse pmic clk gpio\n", __func__);
#ifndef CONFIG_FPGA_EARLY_PORTING
	pmic_config_interface(0xc060, 0x1, 0x1, 7); /* [7:7]: GPIO_inv, inverse the PMIC gpio clk, ALPS02247074 */
#endif
	audckbufEnable(false);
	AudDrv_Clk_Off();
#endif
}

void InitCodecDefault(void)
{
	pr_warn("%s\n", __func__);
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

static int mt6350_codec_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	pr_warn("%s()\n", __func__);
	if (mInitCodec == true)
		return 0;

	pin_extspkamp = pin_mode_extspkamp = 0;

	snd_soc_dapm_new_controls(dapm, mt6350_dapm_widgets, ARRAY_SIZE(mt6350_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, mtk_audio_map, ARRAY_SIZE(mtk_audio_map));

	/* add codec controls */
	snd_soc_add_codec_controls(codec, mt6350_snd_controls, ARRAY_SIZE(mt6350_snd_controls));
	snd_soc_add_codec_controls(codec, mt6350_UL_Codec_controls,
				   ARRAY_SIZE(mt6350_UL_Codec_controls));
	snd_soc_add_codec_controls(codec, mt6350_Voice_Switch, ARRAY_SIZE(mt6350_Voice_Switch));
	snd_soc_add_codec_controls(codec, mt6350_pmic_Test_controls,
				   ARRAY_SIZE(mt6350_pmic_Test_controls));

#ifdef CONFIG_MTK_SPEAKER
	snd_soc_add_codec_controls(codec, mt6350_snd_Speaker_controls,
				   ARRAY_SIZE(mt6350_snd_Speaker_controls));
#endif

	snd_soc_add_codec_controls(codec, Audio_snd_auxadc_controls,
				   ARRAY_SIZE(Audio_snd_auxadc_controls));

	/* here to set  private data */
	mCodec_data = kzalloc(sizeof(mt6350_Codec_Data_Priv), GFP_KERNEL);
	if (!mCodec_data) {
		pr_warn("Failed to allocate private data\n");
		return -ENOMEM;
	}
	snd_soc_codec_set_drvdata(codec, mCodec_data);

	memset((void *)mCodec_data, 0, sizeof(mt6350_Codec_Data_Priv));
	mt6350_codec_init_reg(codec);
	InitCodecDefault();
	mInitCodec = true;

	return 0;
}

static int mt6350_codec_remove(struct snd_soc_codec *codec)
{
	pr_warn("%s()\n", __func__);
	return 0;
}

static unsigned int mt6350_read(struct snd_soc_codec *codec, unsigned int reg)
{
	pr_warn("mt6350_read reg = 0x%x", reg);
	Ana_Get_Reg(reg);
	return 0;
}

static int mt6350_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value)
{
	pr_warn("mt6350_write reg = 0x%x  value= 0x%x\n", reg, value);
	Ana_Set_Reg(reg, value, 0xffffffff);
	return 0;
}

static struct snd_soc_codec_driver soc_mtk_codec = {
	.probe = mt6350_codec_probe,
	.remove = mt6350_codec_remove,

	.read = mt6350_read,
	.write = mt6350_write,


	/* use add control to replace */
	/* .controls = mt6350_snd_controls, */
	/* .num_controls = ARRAY_SIZE(mt6350_snd_controls), */

	.dapm_widgets = mt6350_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6350_dapm_widgets),
	.dapm_routes = mtk_audio_map,
	.num_dapm_routes = ARRAY_SIZE(mtk_audio_map),

};

static int mtk_mt6350_codec_dev_probe(struct platform_device *pdev)
{
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (pdev->dev.dma_mask == NULL)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_CODEC_NAME);

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_codec(&pdev->dev,
				      &soc_mtk_codec, mtk_6350_dai_codecs,
				      ARRAY_SIZE(mtk_6350_dai_codecs));
}

static int mtk_mt6350_codec_dev_remove(struct platform_device *pdev)
{
	pr_warn("%s:\n", __func__);

	snd_soc_unregister_codec(&pdev->dev);
	return 0;

}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_codec_63xx_of_ids[] = {
	{.compatible = "mediatek,mt_soc_codec_63xx",},
	{}
};

#if 0
static int Auddrv_getGPIO_info(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt_soc_codec_63xx");
	if (node) {
		if (of_property_read_u32_index(node, "extspkamp-gpio", 0, &pin_extspkamp)) {
			if_config = 0;
			pr_err("extspkamp-gpio get pin fail!!!\n");
			return -1;
		}
		if (of_property_read_u32_index(node, "extspkamp-gpio", 1, &pin_mode_extspkamp)) {
			if_config = 0;
			pr_err("extspkamp-gpio get pin_mode fail!!!\n");
			return -1;
		}

		if (of_property_read_u32_index(node, "vowclk-gpio", 0, &pin_vowclk)) {
			if_config = 0;
			pr_err("vowclk-gpio get pin fail!!!\n");
			return -1;
		}
		if (of_property_read_u32_index(node, "vowclk-gpio", 1, &pin_mode_vowclk)) {
			if_config = 0;
			pr_err("vowclk-gpio get pin_mode fail!!!\n");
			return -1;
		}

		if (of_property_read_u32_index(node, "audmiso-gpio", 0, &pin_audmiso)) {
			if_config = 0;
			pr_err("audmiso-gpio get pin fail!!!\n");
			return -1;
		}
		if (of_property_read_u32_index(node, "audmiso-gpio", 1, &pin_mode_audmiso)) {
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

static struct platform_driver mtk_codec_6350_driver = {
	.driver = {
		   .name = MT_SOC_CODEC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_codec_63xx_of_ids,
#endif
		   },
	.probe = mtk_mt6350_codec_dev_probe,
	.remove = mtk_mt6350_codec_dev_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_codec6350_dev;
#endif

static int __init mtk_mt6350_codec_init(void)
{
	int ret = 0;

	pr_warn("%s:\n", __func__);
#ifdef CONFIG_OF
	/* Auddrv_getGPIO_info(); */
#else
	soc_mtk_codec6350_dev = platform_device_alloc(MT_SOC_CODEC_NAME, -1);
	if (!soc_mtk_codec6350_dev) {
		pr_err("%s: no mem\n", __func__);
		return -ENOMEM;
	}

	ret = platform_device_add(soc_mtk_codec6350_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_codec6350_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_codec_6350_driver);
	return ret;
}
module_init(mtk_mt6350_codec_init);

static void __exit mtk_mt6350_codec_exit(void)
{
	pr_warn("%s:\n", __func__);

	platform_driver_unregister(&mtk_codec_6350_driver);
}
module_exit(mtk_mt6350_codec_exit);

/* Module information */
MODULE_DESCRIPTION("MTK  codec driver");
MODULE_LICENSE("GPL v2");
