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
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "mt_soc_analog_type.h"
#ifdef _GIT318_READY
#include <mt_clkbuf_ctl.h>
#endif
#ifdef _GIT318_PMIC_READY
#include <mach/mt_pmic.h>
#endif
#ifdef CONFIG_MTK_VOW_SUPPORT
#include <mach/vow_api.h>
#endif
#include "mt_soc_afe_control.h"

#ifdef CONFIG_MTK_SPEAKER
#include "mt_soc_codec_speaker_63xx.h"
/* int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd); */
#endif

#include "mt_soc_pcm_common.h"
#include "AudDrv_Common_func.h"
#include "AudDrv_Gpio.h"

/*#define MT6755_AW8736_REWORK*/	/* use different GPIO for rework version */
#define AW8736_MODE_CTRL /* AW8736 PA output power mode control*/

#ifdef MT6755_AW8736_REWORK
#include "../../../../drivers/misc/mediatek/auxadc/mt6755/mt_auxadc_sw.h"
#endif

/* static function declaration */
static bool AudioPreAmp1_Sel(int Mul_Sel);
static bool GetAdcStatus(void);
static void Apply_Speaker_Gain(void);
static bool TurnOnVOWDigitalHW(bool enable);
static void TurnOffDacPower(void);
static void TurnOnDacPower(void);
static void setDlMtkifSrc(bool enable);
static void SetDcCompenSation(void);
static void Voice_Amp_Change(bool enable);
static void Speaker_Amp_Change(bool enable);
static bool TurnOnVOWADcPower(int MicType, bool enable);

#ifdef CONFIG_MTK_VOW_SUPPORT
static void TurnOnVOWPeriodicOnOff(int MicType, int On_period, int enable);
static void VOW_GPIO_Enable(bool enable);
static void VOW_Pwr_Enable(int MicType, bool enable);
static void VOW_DCC_CLK_Enable(bool enable);
static void VOW_MIC_DCC_Enable(int MicType, bool enable);
static void VOW_MIC_ACC_Enable(int MicType, bool enable);
static void VOW12MCK_Enable(bool enable);
static void VOW32KCK_Enable(bool enable);
#endif

/* extern int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd); */
/* extern kal_uint32 PMIC_IMM_GetOneChannelValue(mt6328_adc_ch_list_enum dwChannel,
		int deCount, int trimd); */
/* extern void upmu_set_rg_vio18_cal(kal_uint32 en);  // must disable in FIB IC */

static mt6331_Codec_Data_Priv *mCodec_data;
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

static int TrimOffset = 2048;
static const int DC1unit_in_uv = 19184;	/* in uv with 0DB */
/* static const int DC1unit_in_uv = 21500; */	/* in uv with 0DB */
static const int DC1devider = 8;	/* in uv */

enum hp_depop_flow {
	HP_DEPOP_FLOW_DEPOP_HW,
	HP_DEPOP_FLOW_33OHM,
	HP_DEPOP_FLOW_DEPOP_HW_33OHM,
	HP_DEPOP_FLOW_NONE,
};
static unsigned int mUseHpDepopFlow;

static int ANC_enabled;

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef EFUSE_HP_TRIM
static unsigned int RG_AUDHPLTRIM_VAUDP15, RG_AUDHPRTRIM_VAUDP15, RG_AUDHPLFINETRIM_VAUDP15,
	RG_AUDHPRFINETRIM_VAUDP15, RG_AUDHPLTRIM_VAUDP15_SPKHP, RG_AUDHPRTRIM_VAUDP15_SPKHP,
	RG_AUDHPLFINETRIM_VAUDP15_SPKHP, RG_AUDHPRFINETRIM_VAUDP15_SPKHP;
#endif
#endif

static unsigned int pin_extspkamp, pin_extspkamp_2, pin_vowclk, pin_audmiso, pin_rcvspkswitch;
static unsigned int pin_mode_extspkamp, pin_mode_extspkamp_2, pin_mode_vowclk, pin_mode_audmiso,
	pin_mode_rcvspkswitch;

#ifdef CONFIG_MTK_SPEAKER
static int Speaker_mode = AUDIO_SPEAKER_MODE_AB;
static unsigned int Speaker_pga_gain = 1;	/* default 0Db. */
static bool mSpeaker_Ocflag;
#endif
static int mAdc_Power_Mode;
static unsigned int dAuxAdcChannel = 16;
static const int mDcOffsetTrimChannel = 9;
static bool mInitCodec;
static uint32 MicbiasRef, GetMicbias;

static int reg_AFE_VOW_CFG0 = 0x0000;		/* VOW AMPREF Setting */
static int reg_AFE_VOW_CFG1 = 0x0000;		/* VOW A,B timeout initial value (timer) */
static int reg_AFE_VOW_CFG2 = 0x2222;		/* VOW A,B value setting (BABA) */
static int reg_AFE_VOW_CFG3 = 0x8767;		/* alhpa and beta K value setting (beta_rise,fall,alpha_rise,fall) */
static int reg_AFE_VOW_CFG4 = 0x006E;		/* gamma K value setting (gamma), bit4:8 should not modify */
static int reg_AFE_VOW_CFG5 = 0x0001;		/* N mini value setting (Nmin) */
static int reg_AFE_VOW_PERIODIC = 0x0000;	/* Periodic On/Off setting (On percent)*/
static bool mIsVOWOn;

/* VOW using */
typedef enum {
	AUDIO_VOW_MIC_TYPE_Handset_AMIC = 0,
	AUDIO_VOW_MIC_TYPE_Headset_MIC,
	AUDIO_VOW_MIC_TYPE_Handset_DMIC,	/* 1P6 */
	AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K,	/* 800K */
	AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCC,	/* DCC mems */
	AUDIO_VOW_MIC_TYPE_Headset_MIC_DCC,
	AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCCECM,	/* DCC ECM, dual differential */
	AUDIO_VOW_MIC_TYPE_Headset_MIC_DCCECM,	/* DCC ECM, signal differential */
	AUDIO_VOW_MIC_TYPE_NUM
} AUDIO_VOW_MIC_TYPE;

#ifdef CONFIG_MTK_VOW_SUPPORT

/* AUDIO_VOW_MIC_TYPE_Handset_DMIC */
/* AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K */
static uint16 Handset_DMIC_PeriodicOnOff[7][22] = {
	/*  PGA,  PreCG,    ADC,  glblp,   dmic, mbias0, mbias1,    pll,  pwrdm,    vow,   dmic, period */
	{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x81EC, 0x0000,
	 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x18F6, 0x0000},/* 90% */
	{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x847B, 0x0000,
	 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x18F6, 0x0000},/* 80% */
	{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x870A, 0x0000,
	 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x18F6, 0x0000},/* 70% */
	{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x899A, 0x0000,
	 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x18F6, 0x0000},/* 60% */
	{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8C29, 0x0000,
	 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x18F6, 0x0000},/* 50% */
	{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8EB8, 0x0000,
	 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x18F6, 0x0000},/* 40% */
	{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x9148, 0x0000,
	 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x18F6, 0x0000},/* 30% */
};

/* AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCC */
/* AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCCECM */
static uint16 Handset_AMIC_DCC_PeriodicOnOff[7][22] = {
	/*  PGA,  PreCG,    ADC,  glblp,   dmic, mbias0, mbias1,    pll,  pwrdm,    vow,   dmic, period */
	{0x8000, 0x8000, 0x81AA, 0x0000, 0x0000, 0x0000, 0x0000, 0x8000, 0xC000, 0x81EC, 0x0000,
	 0x1917, 0x8021, 0x1917, 0x0000, 0x0000, 0x0000, 0x0000, 0x1917, 0x1917, 0x18F6, 0x0000},/* 90% */
	{0x828F, 0x828F, 0x8439, 0x0000, 0x0000, 0x0000, 0x0000, 0x828F, 0xC28F, 0x847B, 0x0000,
	 0x1917, 0x82B0, 0x1917, 0x0000, 0x0000, 0x0000, 0x0000, 0x1917, 0x1917, 0x18F6, 0x0000},/* 80% */
	{0x851F, 0x851F, 0x86C9, 0x0000, 0x0000, 0x0000, 0x0000, 0x851F, 0xC51F, 0x870A, 0x0000,
	 0x1917, 0x853F, 0x1917, 0x0000, 0x0000, 0x0000, 0x0000, 0x1917, 0x1917, 0x18F6, 0x0000},/* 70% */
	{0x87AE, 0x87AE, 0x8958, 0x0000, 0x0000, 0x80A4, 0x0000, 0x87AE, 0xC0A4, 0x899A, 0x0000,
	 0x1917, 0x87CF, 0x1917, 0x0000, 0x0000, 0x1917, 0x0000, 0x1917, 0x1917, 0x18F6, 0x0000},/* 60% */
	{0x8A3D, 0x8A3D, 0x8BE7, 0x0000, 0x0000, 0x8333, 0x0000, 0x8A3D, 0xC333, 0x8C29, 0x0000,
	 0x1917, 0x8A5E, 0x1917, 0x0000, 0x0000, 0x1917, 0x0000, 0x1917, 0x1917, 0x18F6, 0x0000},/* 50% */
	{0x8CCD, 0x8CCD, 0x8E77, 0x0000, 0x0000, 0x85C3, 0x0000, 0x8CCD, 0xC5C3, 0x8EB8, 0x0000,
	 0x1917, 0x8CEE, 0x1917, 0x0000, 0x0000, 0x1917, 0x0000, 0x1917, 0x1917, 0x18F6, 0x0000},/* 40% */
	{0x8F5C, 0x8F5C, 0x9106, 0x0000, 0x0000, 0x8852, 0x0000, 0x8F5C, 0xC852, 0x9148, 0x0000,
	 0x1917, 0x8F7D, 0x1917, 0x0000, 0x0000, 0x1917, 0x0000, 0x1917, 0x1917, 0x18F6, 0x0000} /* 30% */
};

/* AUDIO_VOW_MIC_TYPE_Headset_MIC_DCC */
/* AUDIO_VOW_MIC_TYPE_Headset_MIC_DCCECM */
static uint16 Headset_MIC_DCC_PeriodicOnOff[7][22] = {
	/*  PGA,  PreCG,    ADC,  glblp,   dmic, mbias0, mbias1,    pll,  pwrdm,    vow,   dmic, period */
	{0x8000, 0x8000, 0x81AA, 0x0000, 0x0000, 0x0000, 0x0000, 0x8000, 0xC000, 0x81EC, 0x0000,
	 0x1917, 0x8021, 0x1917, 0x0000, 0x0000, 0x0000, 0x0000, 0x1917, 0x1917, 0x18F6, 0x0000},/* 90% */
	{0x8148, 0x8148, 0x82F2, 0x0000, 0x0000, 0x0000, 0x80A4, 0x8148, 0xC0A4, 0x8333, 0x0000,
	 0x17CF, 0x8168, 0x17CF, 0x0000, 0x0000, 0x0000, 0x17CF, 0x17CF, 0x17CF, 0x17AE, 0x0000},/* 80% */
	{0x828F, 0x828F, 0x8439, 0x0000, 0x0000, 0x0000, 0x81EC, 0x828F, 0xC1EC, 0x847B, 0x0000,
	 0x1687, 0x82B0, 0x1687, 0x0000, 0x0000, 0x0000, 0x1687, 0x1687, 0x1687, 0x1666, 0x0000},/* 70% */
	{0x83D7, 0x83D7, 0x8581, 0x0000, 0x0000, 0x0000, 0x8333, 0x83D7, 0xC333, 0x85C3, 0x0000,
	 0x153F, 0x83F8, 0x153F, 0x0000, 0x0000, 0x0000, 0x153F, 0x153F, 0x153F, 0x151F, 0x0000},/* 60% */
	{0x851F, 0x851F, 0x86C9, 0x0000, 0x0000, 0x0000, 0x847B, 0x851F, 0xC47B, 0x870A, 0x0000,
	 0x13F8, 0x853F, 0x13F8, 0x0000, 0x0000, 0x0000, 0x13F8, 0x13F8, 0x13F8, 0x13D7, 0x0000},/* 50% */
	{0x8666, 0x8666, 0x8810, 0x0000, 0x0000, 0x0000, 0x85C3, 0x8666, 0xC5C3, 0x8852, 0x0000,
	 0x12B0, 0x8687, 0x12B0, 0x0000, 0x0000, 0x0000, 0x12B0, 0x12B0, 0x12B0, 0x128F, 0x0000},/* 40% */
	{0x87AE, 0x87AE, 0x8958, 0x0000, 0x0000, 0x0000, 0x870A, 0x87AE, 0xC70A, 0x899A, 0x0000,
	 0x1168, 0x87CF, 0x1168, 0x0000, 0x0000, 0x0000, 0x1168, 0x1168, 0x1168, 0x1148, 0x0000} /* 30% */
};

#endif

static int mAudio_VOW_Mic_type = AUDIO_VOW_MIC_TYPE_Handset_AMIC;
static void Audio_Amp_Change(int channels, bool enable, bool is_anc);
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
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false, false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == true) {
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
			    false;
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false, false);
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
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true, false);
			mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
			    true;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] ==
		    true) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true, false);
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
	pr_aud("audckbufEnable(), audck_buf_Count = %d, enable = %d\n",
	       audck_buf_Count, enable);

	mutex_lock(&Ana_buf_Ctrl_Mutex);
	if (enable) {
		if (audck_buf_Count == 0) {
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef _GIT318_READY
			clk_buf_ctrl(CLK_BUF_AUDIO, true);
#endif
#endif
		}
		audck_buf_Count++;
	} else {
		audck_buf_Count--;
		if (audck_buf_Count == 0) {
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef _GIT318_READY
			clk_buf_ctrl(CLK_BUF_AUDIO, false);
#endif
#endif
		}
		if (audck_buf_Count < 0) {
			pr_warn("audck_buf_Count count < 0\n");
			audck_buf_Count = 0;
		}
	}
	mutex_unlock(&Ana_buf_Ctrl_Mutex);
}

static int ClsqCount;
static void ClsqEnable(bool enable)
{
	pr_aud("ClsqEnable ClsqCount = %d enable = %d\n", ClsqCount, enable);
	mutex_lock(&AudAna_lock);
	if (enable) {
		if (ClsqCount == 0) {
			Ana_Set_Reg(TOP_CLKSQ, 0x0001, 0x0001);
			/* Enable CLKSQ 26MHz */
			Ana_Set_Reg(TOP_CLKSQ_SET, 0x0001, 0x0001);
			/* Turn on 26MHz source clock */
		}
		ClsqCount++;
	} else {
		ClsqCount--;
		if (ClsqCount < 0) {
			pr_warn("ClsqEnable count <0\n");
			ClsqCount = 0;
		}
		if (ClsqCount == 0) {
			Ana_Set_Reg(TOP_CLKSQ_CLR, 0x0001, 0x0001);
			/* Turn off 26MHz source clock */
			Ana_Set_Reg(TOP_CLKSQ, 0x0000, 0x0001);
			/* Disable CLKSQ 26MHz */
		}
	}
	mutex_unlock(&AudAna_lock);
}

static int TopCkCount;
static void Topck_Enable(bool enable)
{
	pr_aud("Topck_Enable enable = %d TopCkCount = %d\n", enable, TopCkCount);
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (TopCkCount == 0) {
			Ana_Set_Reg(TOP_CKPDN_CON0_CLR, 0xf000, 0xf000);
			/* Turn on AUDNCP_CLKDIV engine clock,Turn on AUD 26M */
			/* Ana_Set_Reg(TOP_CKPDN_CON0, 0x00FD, 0xFFFF); */
			/* AUD clock power down released */
		}
		TopCkCount++;
	} else {
		TopCkCount--;
		if (TopCkCount == 0) {
			Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0xf000, 0xf000);
			/* Turn off AUDNCP_CLKDIV engine clock,Turn off AUD 26M */
			/* Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0xFF00, 0xFF00); */
		}

		if (TopCkCount < 0) {
			pr_warn("TopCkCount <0 =%d\n ", TopCkCount);
			TopCkCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

static int NvRegCount;
static void NvregEnable(bool enable)
{
	pr_aud("NvregEnable NvRegCount == %d enable = %d\n", NvRegCount, enable);
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (NvRegCount == 0) {
			/* Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xffff);  Enable AUDGLB */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0000, 0x1000);
			/* Enable AUDGLB */
		}
		NvRegCount++;
	} else {
		NvRegCount--;
		if (NvRegCount == 0) {
			/* Ana_Set_Reg(AUDDEC_ANA_CON9, 0x1155, 0xffff);  Disable AUDGLB */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x1000, 0x1000);
			/* Disable AUDGLB */
		}
		if (NvRegCount < 0) {
			pr_warn("NvRegCount <0 =%d\n ", NvRegCount);
			NvRegCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

static int anc_clk_counter;
static void anc_clk_enable(bool _enable)
{
	pr_aud("anc_clk_enable, anc_clk_counter = %d, enable = %d\n",
		anc_clk_counter, _enable);
	mutex_lock(&Ana_Clk_Mutex);
	if (_enable == true) {
		if (anc_clk_counter == 0) {
			Ana_Set_Reg(AFE_HPANC_CFG0, 0x0 << 0, 0x1 << 0);
			/* release hp_anc_pdn */
		}
		anc_clk_counter++;
	} else {
		anc_clk_counter--;
		if (anc_clk_counter == 0) {
			Ana_Set_Reg(AFE_HPANC_CFG0, 0x1 << 0, 0x1 << 0);
			/* pdn hp_anc_pdn */
		}
		if (anc_clk_counter < 0) {
			pr_warn("anc_clk_counter < 0 = %d\n", anc_clk_counter);
			anc_clk_counter = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

static int anc_ul_src_counter;
static void anc_ul_src_enable(bool _enable)
{
	pr_aud("anc_ul_src_enable, anc_ul_src_counter = %d, enable = %d\n",
		anc_ul_src_counter, _enable);
	mutex_lock(&Ana_Clk_Mutex);
	if (_enable == true) {
		if (anc_ul_src_counter == 0) {
			Ana_Set_Reg(AFE_HPANC_CFG0, 0x1 << 1, 0x1 << 1);
			/* enable anc ul src */
		}
		anc_ul_src_counter++;
	} else {
		anc_ul_src_counter--;
		if (anc_ul_src_counter == 0) {
			Ana_Set_Reg(AFE_HPANC_CFG0, 0x0 << 1, 0x1 << 1);
			/* disable anc ul src */
		}
		if (anc_ul_src_counter < 0) {
			pr_warn("anc_clk_counter < 0 = %d\n",
				anc_ul_src_counter);
			anc_ul_src_counter = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

bool hasHpDepopHw(void)
{
	return mUseHpDepopFlow == HP_DEPOP_FLOW_DEPOP_HW ||
	       mUseHpDepopFlow == HP_DEPOP_FLOW_DEPOP_HW_33OHM;
}

bool hasHp33Ohm(void)
{
	return mUseHpDepopFlow == HP_DEPOP_FLOW_33OHM ||
	       mUseHpDepopFlow == HP_DEPOP_FLOW_DEPOP_HW_33OHM;
}

#ifdef CONFIG_MTK_VOW_SUPPORT
static int VOW12MCKCount;
static int VOW32KCKCount;
#endif

#ifdef CONFIG_MTK_VOW_SUPPORT

static void VOW12MCK_Enable(bool enable)
{
	pr_warn("VOW12MCK_Enable VOW12MCKCount == %d enable = %d\n", VOW12MCKCount, enable);
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (VOW12MCKCount == 0)
			Ana_Set_Reg(TOP_CKPDN_CON5_CLR, 0x0080, 0x0080);
		/* Enable  TOP_CKPDN_CON5 bit7 for enable VOW 12M clock */
		VOW12MCKCount++;
	} else {
		VOW12MCKCount--;
		if (VOW12MCKCount == 0)
			Ana_Set_Reg(TOP_CKPDN_CON5_SET, 0x0080, 0x0080);
		/* disable TOP_CKPDN_CON5 bit7 for enable VOW 12M clock */

		if (VOW12MCKCount < 0) {
			pr_warn("VOW12MCKCount <0 =%d\n ", VOW12MCKCount);
			VOW12MCKCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

static void VOW32KCK_Enable(bool enable)
{
	pr_warn("VOW32KCK_Enable VOW32KCKCount == %d enable = %d\n", VOW32KCKCount, enable);
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (VOW32KCKCount == 0)
			Ana_Set_Reg(TOP_CKPDN_CON5_CLR, 0x0040, 0x0040);
		/* Enable  TOP_CKPDN_CON5 bit6 for enable VOW 32k clock (for periodic on/off use)*/
		VOW32KCKCount++;
	} else {
		VOW32KCKCount--;
		if (VOW32KCKCount == 0)
			Ana_Set_Reg(TOP_CKPDN_CON5_SET, 0x0040, 0x0040);
		/* disable TOP_CKPDN_CON5 bit6 for enable VOW 32k clock */

		if (VOW32KCKCount < 0) {
			pr_warn("VOW32KCKCount <0 =%d\n ", VOW32KCKCount);
			VOW32KCKCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}
#endif

void vow_irq_handler(void)
{
#ifdef CONFIG_MTK_VOW_SUPPORT

	pr_warn("vow_irq_handler,audio irq event....\n");
	/* TurnOnVOWADcPower(AUDIO_ANALOG_DEVICE_IN_ADC1, false); */
	/* TurnOnVOWDigitalHW(false); */
#if defined(VOW_TONE_TEST)
	EnableSineGen(Soc_Aud_InterConnectionOutput_O03, Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
#endif
	/* VowDrv_ChangeStatus(); */
#endif
}

/*extern kal_uint32 upmu_get_reg_value(kal_uint32 reg);*/

void Auddrv_Read_Efuse_HPOffset(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef EFUSE_HP_TRIM
	U32 ret = 0;
	U32 reg_val = 0;
	int i = 0, j = 0;
	U32 efusevalue[3];

	pr_warn("Auddrv_Read_Efuse_HPOffset(+)\n");

	/* 1. enable efuse ctrl engine clock */
	ret = pmic_config_interface(0x026C, 0x0040, 0xFFFF, 0);
	ret = pmic_config_interface(0x024E, 0x0004, 0xFFFF, 0);

	/* 2. */
	ret = pmic_config_interface(0x0C16, 0x1, 0x1, 0);

	/*  //MT6325
	   Audio data from 746 to 770
	   0xe 746 751
	   0xf 752 767
	   0x10 768 770
	 */

	/* MT6328 681 to 705
	   Audio data from 681 to 705
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
			pr_warn("Auddrv_Read_Efuse_HPOffset polling 0xC1A=0x%x\n", reg_val);
		}

		udelay(1000);	/* Need to delay at least 1ms for 0xC1A and than can read 0xC18 */

		/* 6. read data */
		efusevalue[j] = upmu_get_reg_value(0x0C18);
		pr_warn("HPoffset : efuse[%d]=0x%x\n", j, efusevalue[j]);
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

	pr_warn("RG_AUDHPLTRIM_VAUDP15 = %x\n", RG_AUDHPLTRIM_VAUDP15);
	pr_warn("RG_AUDHPRTRIM_VAUDP15 = %x\n", RG_AUDHPRTRIM_VAUDP15);
	pr_warn("RG_AUDHPLFINETRIM_VAUDP15 = %x\n", RG_AUDHPLFINETRIM_VAUDP15);
	pr_warn("RG_AUDHPRFINETRIM_VAUDP15 = %x\n", RG_AUDHPRFINETRIM_VAUDP15);
	pr_warn("RG_AUDHPLTRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPLTRIM_VAUDP15_SPKHP);
	pr_warn("RG_AUDHPRTRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPRTRIM_VAUDP15_SPKHP);
	pr_warn("RG_AUDHPLFINETRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPLFINETRIM_VAUDP15_SPKHP);
	pr_warn("RG_AUDHPRFINETRIM_VAUDP15_SPKHP = %x\n", RG_AUDHPRFINETRIM_VAUDP15_SPKHP);
#endif
#endif
	pr_warn("Auddrv_Read_Efuse_HPOffset(-)\n");
}
EXPORT_SYMBOL(Auddrv_Read_Efuse_HPOffset);

#ifdef CONFIG_MTK_SPEAKER
static void Apply_Speaker_Gain(void)
{
	pr_warn("%s Speaker_pga_gain= %d\n", __func__, Speaker_pga_gain);
	Ana_Set_Reg(SPK_ANA_CON0, Speaker_pga_gain << 11, 0x7800);
}
#else
static void Apply_Speaker_Gain(void)
{
	Ana_Set_Reg(ZCD_CON1,
		    (mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTR] << 7) |
		    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTL],
		    0x0f9f);
}
#endif

void setOffsetTrimMux(unsigned int Mux)
{
	pr_aud("%s Mux = %d\n", __func__, Mux);
	Ana_Set_Reg(AUDDEC_ANA_CON4, Mux << 8, 0xf << 8);	/* Audio offset trimming buffer mux selection */
}

void setOffsetTrimBufferGain(unsigned int gain)
{
	Ana_Set_Reg(AUDDEC_ANA_CON4, gain << 12, 0x3 << 12);	/* Audio offset trimming buffer gain selection */
}

static int mHplTrimOffset = 2048;
static int mHprTrimOffset = 2048;

void SetHplTrimOffset(int Offset)
{
	pr_warn("%s Offset = %d\n", __func__, Offset);
	mHplTrimOffset = Offset;
	if ((Offset > 2098) || (Offset < 1998)) {
		mHplTrimOffset = 2048;
		pr_err("SetHplTrimOffset offset may be invalid offset = %d\n", Offset);
	}
}

void SetHprTrimOffset(int Offset)
{
	pr_warn("%s Offset = %d\n", __func__, Offset);
	mHprTrimOffset = Offset;
	if ((Offset > 2098) || (Offset < 1998)) {
		mHprTrimOffset = 2048;
		pr_err("SetHprTrimOffset offset may be invalid offset = %d\n", Offset);
	}
}

void EnableTrimbuffer(bool benable)
{
	if (benable == true) {
		Ana_Set_Reg(AUDDEC_ANA_CON4, 0x0080, 0x0080);
		/* Audio offset trimming buffer enable */
	} else {
		Ana_Set_Reg(AUDDEC_ANA_CON4, 0x0000, 0x0080);
		/* Audio offset trimming buffer disable */
	}
}

void OpenTrimBufferHardware(bool enable)
{
	/* 0804 TODO!!! */
	if (enable) {
		pr_warn("%s true\n", __func__);
		TurnOnDacPower();

		/* AUXADC large scale - AUXADC_CON2(AUXADC ADC AVG SELECTION[9]) */
		Ana_Set_Reg(0x0EAA, 0x0200, 0x0200);

		/* set analog part (HP playback) */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
		/* Enable cap-less LDOs (1.6V) */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
		/* Enable NV regulator (-1.6V) */
		Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
		/* Disable AUD_ZCD */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
		/* Disable headphone, voice and short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
		/* Enable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0700, 0xffff);
		/* from yoyo HQA script */
		Ana_Set_Reg(ZCD_CON2, 0x0F9F, 0xffff);
		/* Set HPR/HPL gain as minimum (~ -40dB) */
		Ana_Set_Reg(ZCD_CON3, 0x001F, 0xffff);
		/* Set HS gain as minimum (~ -40dB) */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0001, 0x0001);
		/* De_OSC of HP */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		/* enable output STBENH */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		/* De_OSC of voice, enable output STBENH */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE090, 0xffff);
		/* Enable voice driver */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2140, 0xffff);
		/* Enable pre-charge buffer  */

		udelay(50);

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE09F, 0xffff);
		/* Enable Audio DAC  */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		/* Disable pre-charge buffer */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		/* Disable De_OSC of voice */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE08F, 0xffff);
		/* Disable voice buffer */

		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0300, 0xffff);
		/* from yoyo HQA script */
	} else {
		pr_warn("%s false\n", __func__);
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
		/* Disable Audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
		/* Audio decoder reset - bit 9 */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
		/* Disable IBIST  - bit 8 */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
		/* Disable NV regulator (-1.6V) */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
		/* Disable cap-less LDOs (1.6V) */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0000, 0x0001);
		/* De_OSC of HP */
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0xffff);
		/* from yoyo HQA script */
		TurnOffDacPower();
	}
}

void OpenAnalogTrimHardware(bool enable)
{
	if (enable) {
		pr_warn("%s true\n", __func__);
		TurnOnDacPower();
		/* set analog part (HP playback) */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
		/* Enable cap-less LDOs (1.6V) */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
		/* Enable NV regulator (-1.6V) */
		Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
		/* Disable AUD_ZCD */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
		/* Disable headphone, voice and short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
		/* Enable IBIST */
		Ana_Set_Reg(ZCD_CON2, 0x0F9F, 0xffff);
		/* Set HPR/HPL gain as minimum (~ -40dB) */
		Ana_Set_Reg(ZCD_CON3, 0x001F, 0xffff);
		/* Set HS gain as minimum (~ -40dB) */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0001, 0x0001);
		/* De_OSC of HP */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		/* enable output STBENH */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		/* De_OSC of voice, enable output STBENH */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE090, 0xffff);
		/* Enable voice driver */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2140, 0xffff);
		/* Enable pre-charge buffer  */

		udelay(50);

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE09F, 0xffff);
		/* Enable Audio DAC  */

		/* Apply digital DC compensation value to DAC */
		Ana_Set_Reg(ZCD_CON2, 0x0489, 0xffff);
		/* Set HPR/HPL gain to -1dB, step by step */
		/* SetDcCompenSation(); */

		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF49F, 0xffff);
		/* Switch HP MUX to audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF4FF, 0xffff);
		/* Enable HPR/HPL */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		/* Disable pre-charge buffer */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		/* Disable De_OSC of voice */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF4EF, 0xffff);
		/* Disable voice buffer */
	} else {
		pr_warn("%s false\n", __func__);
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
		/* Disable Audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
		/* Audio decoder reset - bit 9 */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
		/* Disable IBIST  - bit 8 */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
		/* Disable NV regulator (-1.6V) */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
		/* Disable cap-less LDOs (1.6V) */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0000, 0x0001);
		/* De_OSC of HP */
		TurnOffDacPower();
	}
}

void OpenAnalogHeadphone(bool bEnable)
{
	pr_warn("OpenAnalogHeadphone bEnable = %d", bEnable);
	if (bEnable) {
		SetHplTrimOffset(2048);
		SetHprTrimOffset(2048);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = 44100;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true, false);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = true;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true, false);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = true;
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = false;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false, false);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = false;
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false, false);
	}
}

static void HP_Switch_to_Ground(void)
{
	if (hasHpDepopHw()) {
		AudDrv_GPIO_HPDEPOP_Select(true);
		usleep_range(500, 800);
	}
}

static void HP_Switch_to_Release(void)
{
	if (hasHpDepopHw()) {
		usleep_range(500, 800);
		AudDrv_GPIO_HPDEPOP_Select(false);
	}
}

bool OpenHeadPhoneImpedanceSetting(bool bEnable)
{
	pr_aud("%s benable = %d\n", __func__, bEnable);
	if (GetDLStatus() == true || ANC_enabled)
		return false;

	if (bEnable == true) {
		TurnOnDacPower();
		/* upmu_set_rg_vio18_cal(1);// for MT6328 E1 VIO18 patch only */
#if 0				/* test 6328 sgen */
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0080, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
		Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);	/* power on clock */
		/* Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0002, 0x0002);   //UL from sinetable */
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0001, 0x0001);	/* DL from sinetable */
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG0, 0xf000, 0xffff);
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG1, 0xf000, 0xffff);
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG2, 0x0001, 0xffff);
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG0, 0x7f00, 0xffff);
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG1, 0x7f00, 0xffff);
		Ana_Set_Reg(AFE_DL_DC_COMP_CFG2, 0x0001, 0xffff);
#endif
		HP_Switch_to_Ground();

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
		/* Enable cap-less LDOs (1.6V) */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
		/* Enable NV regulator (-1.6V) */
		Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
		/* Disable AUD_ZCD */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
		/* Disable headphone, voice and short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0000, 0x0001);
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0000, 0x2000);
		/* De_OSC of HP and output STBENH disable(470Ohm disconnect) */
		Ana_Set_Reg(AUDDEC_ANA_CON4, 0x4000, 0x4000);
		/* HPDET circuit enable */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
		/* Enable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE089, 0xffff);
		/* Enable LCH Audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON5, 0x0009, 0xffff);
		/* Select HPR as HPDET output and select DACLP as HPDET circuit input */

		/* HP output swtich release to normal output */
		HP_Switch_to_Release();
	} else {
		HP_Switch_to_Ground();

		Ana_Set_Reg(AUDDEC_ANA_CON5, 0x0000, 0xffff);
		/* Disable headphone speaker detection */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
		/* Disable Audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
		/* Audio decoder reset - bit 9 */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
		/* Disable IBIST  - bit 8 */
		Ana_Set_Reg(AUDDEC_ANA_CON4, 0x0000, 0x4000);
		/* HPDET circuit disable */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
		/* Disable NV regulator (-1.6V) */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
		/* Disable cap-less LDOs (1.6V) */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0x2000);
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);

		/* HP output swtich release to normal output */
		HP_Switch_to_Release();

		TurnOffDacPower();
	}
	return true;
}

void setHpGainZero(void)
{
	Ana_Set_Reg(ZCD_CON2, 0x8 << 7, 0x0f80);
	Ana_Set_Reg(ZCD_CON2, 0x8, 0x001f);
}

void SetSdmLevel(unsigned int level)
{
	Ana_Set_Reg(AFE_DL_SDM_CON1, level, 0xffffffff);
}


static void SetHprOffset(int OffsetTrimming)
{
	short Dccompsentation = 0;
	int DCoffsetValue = 0;
	unsigned short RegValue = 0;

	DCoffsetValue = (OffsetTrimming * 11250 + 2048) / 4096;
	/* pr_warn("%s DCoffsetValue = %d\n", __func__, DCoffsetValue); */
	Dccompsentation = DCoffsetValue;
	RegValue = Dccompsentation;
	/* pr_warn("%s RegValue = 0x%x\n", __func__, RegValue); */
	Ana_Set_Reg(AFE_DL_DC_COMP_CFG1, RegValue, 0xffff);
}

static void SetHplOffset(int OffsetTrimming)
{
	short Dccompsentation = 0;
	int DCoffsetValue = 0;
	unsigned short RegValue = 0;

	DCoffsetValue = (OffsetTrimming * 11250 + 2048) / 4096;
	/* pr_warn("%s DCoffsetValue = %d\n", __func__, DCoffsetValue); */
	Dccompsentation = DCoffsetValue;
	RegValue = Dccompsentation;
	/* pr_warn("%s RegValue = 0x%x\n", __func__, RegValue); */
	Ana_Set_Reg(AFE_DL_DC_COMP_CFG0, RegValue, 0xffff);
}

static void EnableDcCompensation(bool bEnable)
{
#ifndef EFUSE_HP_TRIM
	Ana_Set_Reg(AFE_DL_DC_COMP_CFG2, bEnable, 0x1);
#endif
}

static void SetHprOffsetTrim(void)
{
	int OffsetTrimming = mHprTrimOffset - TrimOffset;
	pr_aud("%s OffsetTrimming = %d (mHprTrimOffset(%d)- TrimOffset(%d))\n", __func__,
	       OffsetTrimming, mHprTrimOffset, TrimOffset);
	SetHprOffset(OffsetTrimming);
}

static void SetHpLOffsetTrim(void)
{
	int OffsetTrimming = mHplTrimOffset - TrimOffset;
	pr_aud("%s OffsetTrimming = %d (mHplTrimOffset(%d)- TrimOffset(%d))\n", __func__,
	       OffsetTrimming, mHplTrimOffset, TrimOffset);
	SetHplOffset(OffsetTrimming);
}

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

static void SetDCcoupleNP(int MicBias, int mode)
{
	pr_aud("%s MicBias= %d mode = %d\n", __func__, MicBias, mode);
	switch (mode) {
	case AUDIO_ANALOGUL_MODE_ACC:
	case AUDIO_ANALOGUL_MODE_DCC:
	case AUDIO_ANALOGUL_MODE_DMIC:{
			if (MicBias == AUDIO_MIC_BIAS0)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x000E);
			else if (MicBias == AUDIO_MIC_BIAS1)
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x0006);
			else if (MicBias == AUDIO_MIC_BIAS2)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x0E00);
		}
		break;
	case AUDIO_ANALOGUL_MODE_DCCECMDIFF:{
			if (MicBias == AUDIO_MIC_BIAS0)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x000E, 0x000E);
			else if (MicBias == AUDIO_MIC_BIAS1)
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0004, 0x0006);
			else if (MicBias == AUDIO_MIC_BIAS2)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0E00, 0x0E00);
		}
		break;
	case AUDIO_ANALOGUL_MODE_DCCECMSINGLE:{
			if (MicBias == AUDIO_MIC_BIAS0)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0002, 0x000E);
			else if (MicBias == AUDIO_MIC_BIAS1)
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0004, 0x0006);
			else if (MicBias == AUDIO_MIC_BIAS2)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0200, 0x0E00);
		}
		break;
	default:
		break;
	}
}

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
	case 96000:
		return 4;
	case 192000:
		return 5;
	default:
		return 3;
	}
}


static int mt63xx_codec_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *Daiport)
{
	/* pr_warn("+mt63xx_codec_startup name = %s number = %d\n", substream->name, substream->number); */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE && substream->runtime->rate) {
		/* pr_warn("mt63xx_codec_startup set up SNDRV_PCM_STREAM_CAPTURE rate = %d\n",
		substream->runtime->rate); */
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = substream->runtime->rate;

	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK && substream->runtime->rate) {
		/* pr_warn("mt63xx_codec_startup set up SNDRV_PCM_STREAM_PLAYBACK rate = %d\n",
		substream->runtime->rate); */
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = substream->runtime->rate;
	}
	/* pr_warn("-mt63xx_codec_startup name = %s number = %d\n", substream->name, substream->number); */
	return 0;
}

static int mt63xx_codec_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *Daiport)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_aud("mt63xx_codec_prepare set up SNDRV_PCM_STREAM_CAPTURE rate = %d\n",
		       substream->runtime->rate);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = substream->runtime->rate;

	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_aud("mt63xx_codec_prepare set up SNDRV_PCM_STREAM_PLAYBACK rate = %d\n",
		       substream->runtime->rate);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = substream->runtime->rate;
	}
	return 0;
}

static int mt6323_codec_trigger(struct snd_pcm_substream *substream, int command,
				struct snd_soc_dai *Daiport)
{
	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}

	/* pr_warn("mt6323_codec_trigger command = %d\n ", command); */
	return 0;
}

static const struct snd_soc_dai_ops mt6323_aif1_dai_ops = {
	.startup = mt63xx_codec_startup,
	.prepare = mt63xx_codec_prepare,
	.trigger = mt6323_codec_trigger,
};

static struct snd_soc_dai_driver mtk_6331_dai_codecs[] = {
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
		     .rates = SOC_HIGH_USE_RATE,
		     .formats = SND_SOC_ADV_MT_FMTS,
		     },
	 },
	{
	 .name = MT_SOC_CODEC_TDMRX_DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .capture = {
		     .stream_name = MT_SOC_TDM_CAPTURE_STREAM_NAME,
		     .channels_min = 2,
		     .channels_max = 2,
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
		      }
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
	 .name = MT_SOC_CODEC_VOICE_ULTRADAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_VOICE_ULTRA_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 .capture = {
		     .stream_name = MT_SOC_VOICE_ULTRA_STREAM_NAME,
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = SNDRV_PCM_RATE_8000_192000,
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
	 },
	{
	 .name = MT_SOC_CODEC_TXDAI2_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_DL2_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	 },
	{
	 .name = MT_SOC_CODEC_OFFLOAD_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		      .stream_name = MT_SOC_OFFLOAD_STREAM_NAME,
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = SNDRV_PCM_RATE_8000_192000,
		      .formats = SND_SOC_ADV_MT_FMTS,
		      },
	},
	{
		.name = MT_SOC_CODEC_ANC_NAME,
		.ops = &mt6323_aif1_dai_ops,
		.playback = {
			.stream_name = MT_SOC_ANC_STREAM_NAME,
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SND_SOC_ADV_MT_FMTS,
			},
	}
};

uint32 GetDLNewIFFrequency(unsigned int frequency)
{
	uint32 Reg_value = 0;

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
	case 96000:
	case 192000:
		Reg_value = 8;
		break;
	default:
		pr_warn("GetDLNewIFFrequency invalid freq %d\n", frequency);
		break;
	}
	return Reg_value;
}

static void TurnOnDacPower(void)
{
	pr_warn("TurnOnDacPower\n");
	audckbufEnable(true);
	NvregEnable(true);	/* Enable AUDGLB */
	ClsqEnable(true);	/* Turn on 26MHz source clock */
	Topck_Enable(true);	/* Turn on AUDNCP_CLKDIV engine clock */
	/* Turn on AUD 26M */
	udelay(250);

	if (GetAdcStatus() == false)
		Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0020, 0xffff);	/* power on clock */
	else
		Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);	/* power on clock */

	/* set digital part */
	Ana_Set_Reg(AFE_NCP_CFG1, 0x1515, 0xffff);
	/* NCP: ck1 and ck2 clock frequecy adjust configure */
	Ana_Set_Reg(AFE_NCP_CFG0, 0x8C01, 0xffff);
	/* NCP enable */

	udelay(250);

	/* Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff); */
	/* Audio system digital clock power down release */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffff);
	/* sdm audio fifo clock power on */
	Ana_Set_Reg(AFUNC_AUD_CON0, 0xC3A1, 0xffff);
	/* scrambler clock on enable */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffff);
	/* sdm power on */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x000B, 0xffff);
	/* sdm fifo enable */
	Ana_Set_Reg(AFE_DL_SDM_CON1, 0x001E, 0xffff);
	/* set attenuation gain */
	Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);
	/* afe enable */

	setDlMtkifSrc(true);
}

static void TurnOffDacPower(void)
{
	pr_warn("TurnOffDacPower\n");

	setDlMtkifSrc(false);

	if (GetAdcStatus() == false)
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);	/* turn off afe */

	udelay(250);

	Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0040, 0x0040);	/* down-link power down */
	Ana_Set_Reg(AFE_NCP_CFG0, 0x4600, 0xffff);		/* NCP disable */

#if 0
	Ana_Set_Reg(AUDNCP_CLKDIV_CON0, 0x0000, 0xffff);	/* Toggle RG_DIVCKS_CHG */
	Ana_Set_Reg(AUDNCP_CLKDIV_CON1, 0x0000, 0xffff);	/* Turn off DA_600K_NCP_VA18 */
	Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x0001, 0xffff);	/* Disable NCP */
#endif
	/* upmu_set_rg_vio18_cal(0);// for MT6328 E1 VIO18 patch only */
	NvregEnable(false);
	ClsqEnable(false);
	Topck_Enable(false);
	audckbufEnable(false);
}

static void setDlMtkifSrc(bool enable)
{
	pr_warn("%s(), enable = %d, freq = %d\n",
		__func__,
		enable,
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]);
	if (enable) {
		Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0,
			    ((GetDLNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) << 12) |
			     0x330), 0xffff);
		Ana_Set_Reg(AFE_DL_SRC2_CON0_H,
			    ((GetDLNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) << 12) |
			     0x300), 0xffff);

		Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x1 << 15, 0x1 << 15);
		/* pmic rxif sck inverse */

		Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x0001, 0xffff);
		/* turn on dl */
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);
		/* set DL in normal path, not from sine gen table */
	} else {
		Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x0000, 0xffff);
		/* bit0, Turn off down-link */
	}
}

static bool is_valid_hp_pga_idx(int reg_idx)
{
	return (reg_idx >= 0 && reg_idx <= 0x12) || reg_idx == 0x1f;
}

static void HeadsetVoloumeRestore(void)
{
	int index = 0, oldindex = 0, offset = 0, count = 1, reg_idx;

	/*pr_warn("%s\n", __func__);*/
	index = 8;
	oldindex = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	if (index > oldindex) {
		pr_aud("index = %d oldindex = %d\n", index, oldindex);
		offset = index - oldindex;
		while (offset > 0) {
			reg_idx = oldindex + count;
			if (is_valid_hp_pga_idx(reg_idx)) {
				Ana_Set_Reg(ZCD_CON2,
					    (reg_idx << 7) | reg_idx,
					    0xf9f);
			}
			offset--;
			count++;
			udelay(100);
		}
	} else {
		pr_aud("index = %d oldindex = %d\n", index, oldindex);
		offset = oldindex - index;
		while (offset > 0) {
			reg_idx = oldindex - count;
			if (is_valid_hp_pga_idx(reg_idx)) {
				Ana_Set_Reg(ZCD_CON2,
					    (reg_idx << 7) | reg_idx,
					    0xf9f);
			}
			offset--;
			count++;
			udelay(100);
		}
	}
	Ana_Set_Reg(ZCD_CON2, 0x408, 0xf9f);
}

static void HeadsetVoloumeSet(void)
{
	int index = 0, oldindex = 0, offset = 0, count = 1, reg_idx;
	/* pr_warn("%s\n", __func__); */
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	oldindex = 8;
	if (index > oldindex) {
		pr_aud("index = %d oldindex = %d\n", index, oldindex);
		offset = index - oldindex;
		while (offset > 0) {
			reg_idx = oldindex + count;
			if (is_valid_hp_pga_idx(reg_idx)) {
				Ana_Set_Reg(ZCD_CON2,
					(reg_idx << 7) | reg_idx,
					    0xf9f);
			}
			offset--;
			count++;
			udelay(200);
		}
	} else {
		pr_aud("index = %d oldindex = %d\n", index, oldindex);
		offset = oldindex - index;
		while (offset > 0) {
			reg_idx = oldindex - count;
			if (is_valid_hp_pga_idx(reg_idx)) {
				Ana_Set_Reg(ZCD_CON2,
					    (reg_idx << 7) | reg_idx,
					    0xf9f);
			}
			offset--;
			count++;
			udelay(200);
		}
	}
	Ana_Set_Reg(ZCD_CON2, (index << 7) | (index), 0xf9f);
}

static void Hp_Zcd_Enable(bool _enable)
{
	if (_enable) {
		/* Enable ZCD, for minimize pop noise */
		/* when adjust gain during HP buffer on */
		Ana_Set_Reg(ZCD_CON0, 0x1 << 8, 0x7 << 8);
		Ana_Set_Reg(ZCD_CON0, 0x0 << 7, 0x1 << 7);

		/* timeout, 1=5ms, 0=30ms */
		Ana_Set_Reg(ZCD_CON0, 0x1 << 6, 0x1 << 6);

		Ana_Set_Reg(ZCD_CON0, 0x0 << 4, 0x3 << 4);
		Ana_Set_Reg(ZCD_CON0, 0x5 << 1, 0x7 << 1);
		Ana_Set_Reg(ZCD_CON0, 0x1 << 0, 0x1 << 0);
	} else {
		Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
	}
}

static void Audio_Amp_Change(int channels, bool enable, bool is_anc)
{
	pr_warn("%s(), enable %d, ANC %d, HSL %d, HSR %d, is_anc %d\n",
		__func__,
		enable,
		ANC_enabled,
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL],
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR],
		is_anc);

	if (enable) {
		if (GetDLStatus() == false && !ANC_enabled)
			TurnOnDacPower();

		if (GetDLStatus() == false && !is_anc) {
			/* reset sample rate */
			setDlMtkifSrc(false);
			setDlMtkifSrc(true);
		}

		/* here pmic analog control */
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false &&
		    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == false &&
		    !ANC_enabled) {
			pr_warn("%s\n", __func__);

			/* switch to ground to de pop-noise */
			HP_Switch_to_Ground();

			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
			/* Enable cap-less LDOs (1.6V) */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
			/* Enable NV regulator (-1.6V) */
			Hp_Zcd_Enable(false);
			/* Disable AUD_ZCD */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
			/* Disable headphone, voice and short-circuit protection */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
			/* Enable IBIST */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0700, 0xffff);
			/* from yoyo HQA script */
			Ana_Set_Reg(ZCD_CON2, 0x0F9F, 0xffff);
			/* Set HPR/HPL gain as minimum (~ -40dB) */
			Ana_Set_Reg(ZCD_CON3, 0x001F, 0xffff);
			/* Set HS gain as minimum (~ -40dB) */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0001, 0x0001);
			/* De_OSC of HP */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
			/* enable output STBENH */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
			/* De_OSC of voice, enable output STBENH */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE090, 0xffff);
			/* Enable voice driver */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2140, 0xffff);
			/* Enable pre-charge buffer  */

			udelay(50);

			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
			/* Enable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE09F, 0xffff);
			/* Enable Audio DAC  */

			/* Apply digital DC compensation value to DAC */
			setHpGainZero();
			SetDcCompenSation();

			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF49F, 0xffff);
			/* Switch HP MUX to audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF4FF, 0xffff);
			/* Enable HPR/HPL */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
			/* Disable pre-charge buffer */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
			/* Disable De_OSC of voice */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF4EF, 0xffff);
			/* Disable voice buffer */

			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0300, 0xffff);
			/* from yoyo HQA script */

			/* Enable ZCD, for minimize pop noise */
			/* when adjust gain during HP buffer on */
			Hp_Zcd_Enable(true);

			/* HP output swtich release to normal output */
			HP_Switch_to_Release();

			/* apply volume setting */
			HeadsetVoloumeSet();
		}

	} else {
		if (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false &&
		    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == false &&
		    !ANC_enabled) {
			pr_warn("Audio_Amp_Change off amp\n");

			/* Disable AUD_ZCD */
			Hp_Zcd_Enable(false);

			HeadsetVoloumeRestore();
			/* Set HPR/HPL gain as -1dB, step by step */

			/* Ana_Set_Reg(ZCD_CON2, 0x0F9F, 0xffff); */
			/* Set HPR/HPL gain as minimum (~ -40dB) */
			setHpGainZero();

			/* switch to ground to de pop-noise */
			HP_Switch_to_Ground();

			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xF40F, 0xffff);
			/* Disable HPR/HPL */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE00F, 0xffff);
			/* HPR/HPL mux to open */

			EnableDcCompensation(false);

			/* HP output swtich release to normal output */
			HP_Switch_to_Release();
		}

		if (GetDLStatus() == false && !ANC_enabled) {
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
			/* Disable Audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
			/* Audio decoder reset - bit 9 */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0xffff);
			/* from yoyo HQA script - reset*/
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
			/* Disable IBIST  - bit 8 */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
			/* Disable NV regulator (-1.6V) */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
			/* Disable cap-less LDOs (1.6V) */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0000, 0x0001);
			/* De_OSC of HP */

			TurnOffDacPower();
		}
	}
}

static int Audio_AmpL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Audio_AmpL_Get = %d\n",
	       mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL];
	return 0;
}

static int Audio_AmpL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	pr_aud("%s() enable = %ld\n ", __func__, ucontrol->value.integer.value[0]);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true, false);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] ==
		       true)) {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
		    ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false, false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int Audio_AmpR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Audio_AmpR_Get = %d\n",
	       mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR];
	return 0;
}

static int Audio_AmpR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	pr_aud("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true, false);
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] ==
		       true)) {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
		    ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false, false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int PMIC_REG_CLEAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
#if 0
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
#if 0				/* not used */
static void SetVoiceAmpVolume(void)
{
	int index;

	pr_warn("%s\n", __func__);
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL];
	Ana_Set_Reg(ZCD_CON3, index, 0x001f);
}
#endif

static void Voice_Amp_Change(bool enable)
{
	if (enable) {
		if (GetDLStatus() == false) {
			TurnOnDacPower();
			pr_warn("Voice_Amp_Change on amp\n");

			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
			/* Enable cap-less LDOs (1.6V) */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
			/* Enable NV regulator (-1.6V) */
			Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
			/* Disable AUD_ZCD */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
			/* Disable HS and short-circuit protection. HS MUX is opened */

			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
			/* Enable IBIST */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0400, 0xffff);
			/* from yoyo HQA script */
			Ana_Set_Reg(ZCD_CON3, 0x001F, 0xffff);
			/* Set HS gain as minimum (~ -40dB) */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0100, 0x0100);
			/* De_OSC of HS and enable output STBENH */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
			/* Enable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE089, 0xffff);
			/* Enable Audio DAC  */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE109, 0xffff);
			/* Switch HS MUX to audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE119, 0xffff);
			/* Enable HS */
			Ana_Set_Reg(ZCD_CON3, 0x0009, 0xffff);
			/* Set HS gain as 0dB */
		}
	} else {
		pr_warn("Voice_Amp_Change off amp\n");
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE109, 0xffff);
		/* Disable voice driver */

		if (GetDLStatus() == false) {
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
			/* Disable Audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
			/* Audio decoder reset - bit 9 */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0xffff);
			/* from yoyo HQA script - reset */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
			/* Disable IBIST  - bit 8 */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
			/* Disable NV regulator (-1.6V) */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
			/* Disable cap-less LDOs (1.6V) */

			TurnOffDacPower();
		}
	}
}

static int Voice_Amp_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Voice_Amp_Get = %d\n",
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL];
	return 0;
}

static int Voice_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	pr_aud("%s()\n", __func__);
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

/* For MT6351 LO */
static void Speaker_Amp_Change(bool enable)
{
	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower();

		pr_warn("%s\n", __func__);

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
		/* Enable cap-less LDOs (1.6V) */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
		/* Enable NV regulator (-1.6V) */
		Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
		/* Disable AUD_ZCD */
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4028, 0xffff);
		/* Disable line-out and short-circuit protection. LO MUX is opened */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
		/* Enable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0800, 0xffff);
		/* from yoyo HQA script */
		Ana_Set_Reg(ZCD_CON1, 0x0F9F, 0xffff);
		/* Set LOL gain as minimum (~ -40dB) */
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4228, 0xffff);
		/* De_OSC of LO and enable output STBENH */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE009, 0xffff);
		/* Enable Audio DAC */

		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4230, 0xffff);
		/* Switch LO MUX to audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4234, 0xffff);
		/* Enable LOL */
		Ana_Set_Reg(ZCD_CON1, 0x0F89, 0xffff);
		/* Set LOL gain as 0dB */

		Apply_Speaker_Gain();
	} else {
		/* pr_warn("turn off Speaker_Amp_Change\n"); */
#ifdef CONFIG_MTK_SPEAKER
		if (Speaker_mode == AUDIO_SPEAKER_MODE_D)
			Speaker_ClassD_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_AB)
			Speaker_ClassAB_close();
		else if (Speaker_mode == AUDIO_SPEAKER_MODE_RECEIVER)
			Speaker_ReveiverMode_close();
#endif
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4230, 0xffff);
		/* Disable LOL */
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4228, 0xffff);
		/* Switch LO MUX to ground */

		if (GetDLStatus() == false) {
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
			/* Disable Audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
			/* Audio decoder reset - bit 9 */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0xffff);
			/* from yoyo HQA script - reset */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
			/* Disable IBIST  - bit 8 */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
			/* Disable NV regulator (-1.6V) */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
			/* Disable cap-less LDOs (1.6V) */

			TurnOffDacPower();
		}
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



#ifdef CONFIG_OF

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

#define NULL_PIN_DEFINITION    (-1)
static void Ext_Speaker_Amp_Change(bool enable)
{
#define SPK_WARM_UP_TIME        (25)	/* unit is ms */
#ifndef CONFIG_FPGA_EARLY_PORTING
	if (enable) {
		pr_debug("Ext_Speaker_Amp_Change ON+\n");
#ifndef CONFIG_MTK_SPEAKER

#ifndef MT6755_AW8736_REWORK
		AudDrv_GPIO_EXTAMP_Select(false, 3);
#else
		if (pin_extspkamp != 54)
			AudDrv_GPIO_EXTAMP_Select(false, 3);
		else
			AudDrv_GPIO_EXTAMP2_Select(false, 3);
#endif

		/*udelay(1000); */
		usleep_range(1 * 1000, 20 * 1000);
#if defined(CONFIG_MTK_LEGACY)
		mt_set_gpio_dir(pin_extspkamp, GPIO_DIR_OUT);	/* output */
		if (pin_extspkamp_2 != NULL_PIN_DEFINITION)
			mt_set_gpio_dir(pin_extspkamp_2, GPIO_DIR_OUT);	/* output */

#ifdef AW8736_MODE_CTRL
		AW8736_MODE3;
#else
		mt_set_gpio_out(pin_extspkamp, GPIO_OUT_ONE);	/* high enable */
#endif				/*AW8736_MODE_CTRL */
		if (pin_extspkamp_2 != NULL_PIN_DEFINITION)
			mt_set_gpio_out(pin_extspkamp_2, GPIO_OUT_ONE);	/* high enable */
#else
#ifndef MT6755_AW8736_REWORK
		AudDrv_GPIO_EXTAMP_Select(true, 3);
#else
		if (pin_extspkamp != 54)
			AudDrv_GPIO_EXTAMP_Select(true, 3);
		else
			AudDrv_GPIO_EXTAMP2_Select(true, 3);
#endif
#endif				/*CONFIG_MTK_LEGACY */
		msleep(SPK_WARM_UP_TIME);
#endif
		pr_debug("Ext_Speaker_Amp_Change ON-\n");
	} else {
		pr_debug("Ext_Speaker_Amp_Change OFF+\n");
#ifndef CONFIG_MTK_SPEAKER

#ifndef MT6755_AW8736_REWORK
		AudDrv_GPIO_EXTAMP_Select(false, 3);
#else
		if (pin_extspkamp != 54)
			AudDrv_GPIO_EXTAMP_Select(false, 3);
		else
			AudDrv_GPIO_EXTAMP2_Select(false, 3);
#endif
		udelay(500);
#endif
		pr_debug("Ext_Speaker_Amp_Change OFF-\n");
	}
#endif
}

#else				/*CONFIG_OF */
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

static void Ext_Speaker_Amp_Change(bool enable)
{
#define SPK_WARM_UP_TIME        (25)	/* unit is ms */
#ifndef CONFIG_FPGA_EARLY_PORTING

	if (enable) {
		pr_debug("Ext_Speaker_Amp_Change ON+\n");
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

		msleep(SPK_WARM_UP_TIME);
#endif
		pr_debug("Ext_Speaker_Amp_Change ON-\n");
	} else {
		pr_debug("Ext_Speaker_Amp_Change OFF+\n");
#ifndef CONFIG_MTK_SPEAKER
		/* mt_set_gpio_mode(GPIO_EXT_SPKAMP_EN_PIN, GPIO_MODE_00); //GPIO117: DPI_D3, mode 0 */
		mt_set_gpio_dir(GPIO_EXT_SPKAMP_EN_PIN, GPIO_DIR_OUT);	/* output */
		mt_set_gpio_out(GPIO_EXT_SPKAMP_EN_PIN, GPIO_OUT_ZERO);	/* low disbale */
		udelay(500);
#endif
		pr_debug("Ext_Speaker_Amp_Change OFF-\n");
	}
#endif
}
#endif
#endif

static int Ext_Speaker_Amp_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP];
	return 0;
}

static int Ext_Speaker_Amp_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s() gain = %ld\n ", __func__, ucontrol->value.integer.value[0]);
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

static void Receiver_Speaker_Switch_Change(bool enable)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF
	pr_debug("%s\n", __func__);

	if (enable)
		AudDrv_GPIO_RCVSPK_Select(true);
	else
		AudDrv_GPIO_RCVSPK_Select(false);
#endif
#endif
}

static int Receiver_Speaker_Switch_Get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() : %d\n", __func__,
		 mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH];
	return 0;
}

static int Receiver_Speaker_Switch_Set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->mAudio_Ana_DevicePower
		[AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] == false)) {
		Receiver_Speaker_Switch_Change(true);
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   &&
		   (mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] == true)) {
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] =
		    ucontrol->value.integer.value[0];
		Receiver_Speaker_Switch_Change(false);
	}
	return 0;
}

static void Headset_Speaker_Amp_Change(bool enable)
{
	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower();

		pr_warn("%s\n", __func__);

		/* switch to ground to de pop-noise */
		HP_Switch_to_Ground();

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0xA000);
		/* Enable cap-less LDOs (1.6V) */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0100, 0x0100);
		/* Enable NV regulator (-1.6V) */
		Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff);
		/* Disable AUD_ZCD */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE080, 0xffff);
		/* Disable headphone, voice and short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4028, 0xffff);
		/* Disable line-out and short-circuit protection. LO MUX is opened */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0100);
		/* Enable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0F00, 0xffff);
		/* from yoyo HQA script */
		Ana_Set_Reg(ZCD_CON2, 0x0F9F, 0xffff);
		/* Set HPR/HPL gain as minimum (~ -40dB) */
		Ana_Set_Reg(ZCD_CON3, 0x001F, 0xffff);
		/* Set HS gain as minimum (~ -40dB) */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0001, 0x0001);
		/* De_OSC of HP */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		/* enable output STBENH */
		Ana_Set_Reg(ZCD_CON1, 0x0F9F, 0xffff);
		/* Set LOL gain as minimum (~ -40dB) */
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4228, 0xffff);
		/* De_OSC of LO and enable output STBENH */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		/* De_OSC of voice, enable output STBENH */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE090, 0xffff);
		/* Enable voice driver */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2140, 0xffff);
		/* Enable pre-charge buffer  */

		udelay(50);

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA255, 0x0200);
		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE09F, 0xffff);
		/* Enable Audio DAC  */

		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4230, 0xffff);
		/* Switch LO MUX to audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4234, 0xffff);
		/* Enable LOL */

		/* Apply digital DC compensation value to DAC */
		setHpGainZero();
		SetDcCompenSation();
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xEA9F, 0xffff);
		/* Switch HP MUX to audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xEAFF, 0xffff);
		/* Enable HPR/HPL */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2100, 0xffff);
		/* Disable pre-charge buffer */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x2000, 0xffff);
		/* Disable De_OSC of voice */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xEAEF, 0xffff);
		/* Disable voice buffer */

		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0B00, 0xffff);
		/* from yoyo HQA script */

		/* Enable ZCD, for minimize pop noise */
		/* when adjust gain during HP buffer on */
		Hp_Zcd_Enable(true);

		/* HP output swtich release to normal output */
		HP_Switch_to_Release();

		/* apply volume setting */
		HeadsetVoloumeSet();
		Apply_Speaker_Gain();
	} else {
		HeadsetVoloumeRestore();
		/* Set HPR/HPL gain as 0dB, step by step */
		setHpGainZero();

		HP_Switch_to_Ground();
		/* switch to ground to de pop-noise */

		/* Disable AUD_ZCD */
		Hp_Zcd_Enable(false);


		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xEA0F, 0xffff);
		/* Disable HPR/HPL */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE00F, 0xffff);
		/* HPR/HPL mux to open */

		HP_Switch_to_Release();
		/* HP output swtich release to normal output */

		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4230, 0xffff);
		/* Disable LOL */
		Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4228, 0xffff);
		/* Switch LO MUX to ground */

		if (GetDLStatus() == false) {
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xE000, 0xffff);
			/* Disable Audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA055, 0x0200);
			/* Audio decoder reset - bit 9 */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0xffff);
			/* from yoyo HQA script - reset */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xA155, 0x0100);
			/* Disable IBIST  - bit 8 */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0100);
			/* Disable NV regulator (-1.6V) */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0155, 0xA000);
			/* Disable cap-less LDOs (1.6V) */

			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0000, 0x0001);
			/* De_OSC of HP */

			TurnOffDacPower();
		}
		EnableDcCompensation(false);
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
	    && (mCodec_data->mAudio_Ana_DevicePower
		[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] == false)) {
		Headset_Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] = ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   &&
		   (mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] == true)) {
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] = ucontrol->value.integer.value[0];
		Headset_Speaker_Amp_Change(false);
	}
	return 0;
}

#ifdef CONFIG_MTK_SPEAKER
static const char *const speaker_amp_function[] = { "CALSSD", "CLASSAB", "RECEIVER" };

static const char *const speaker_PGA_function[] = {
	"4Db", "5Db", "6Db", "7Db", "8Db", "9Db", "10Db",
	"11Db", "12Db", "13Db", "14Db", "15Db", "16Db", "17Db"
};

static const char *const speaker_OC_function[] = { "Off", "On" };
static const char *const speaker_CS_function[] = { "Off", "On" };
static const char *const speaker_CSPeakDetecReset_function[] = { "Off", "On" };

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
	Ana_Set_Reg(SPK_ANA_CON0, Speaker_pga_gain << 11, 0x7800);
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
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_amp_function),
			    speaker_amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_PGA_function),
			    speaker_PGA_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_OC_function),
			    speaker_OC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_CS_function),
			    speaker_CS_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(speaker_CSPeakDetecReset_function),
			    speaker_CSPeakDetecReset_function),
};

static const struct snd_kcontrol_new mt6331_snd_Speaker_controls[] = {
	SOC_ENUM_EXT("Audio_Speaker_class_Switch", Audio_Speaker_Enum[0],
		     Audio_Speaker_Class_Get,
		     Audio_Speaker_Class_Set),
	SOC_ENUM_EXT("Audio_Speaker_PGA_gain", Audio_Speaker_Enum[1],
		     Audio_Speaker_Pga_Gain_Get,
		     Audio_Speaker_Pga_Gain_Set),
	SOC_ENUM_EXT("Audio_Speaker_OC_Falg", Audio_Speaker_Enum[2],
		     Audio_Speaker_OcFlag_Get,
		     Audio_Speaker_OcFlag_Set),
	SOC_ENUM_EXT("Audio_Speaker_CurrentSensing", Audio_Speaker_Enum[3],
		     Audio_Speaker_Current_Sensing_Get,
		     Audio_Speaker_Current_Sensing_Set),
	SOC_ENUM_EXT("Audio_Speaker_CurrentPeakDetector",
		     Audio_Speaker_Enum[4],
		     Audio_Speaker_Current_Sensing_Peak_Detector_Get,
		     Audio_Speaker_Current_Sensing_Peak_Detector_Set),
};

int Audio_AuxAdcData_Get_ext(void)
{
#ifdef _GIT318_PMIC_READY
	int dRetValue = PMIC_IMM_GetOneChannelValue(AUX_ICLASSAB_AP, 1, 0);

	pr_warn("%s dRetValue 0x%x\n", __func__, dRetValue);
	return dRetValue;
#else
return 0;
#endif
}


#endif

static int Audio_AuxAdcData_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

#ifdef CONFIG_MTK_SPEAKER
	ucontrol->value.integer.value[0] = Audio_AuxAdcData_Get_ext();
	/* PMIC_IMM_GetSPK_THR_IOneChannelValue(0x001B, 1, 0); */
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
	SOC_SINGLE_EXT("Audio AUXADC Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_AuxAdcData_Get,
		       Audio_AuxAdcData_Set),
};


static const char *const amp_function[] = { "Off", "On" };
static const char *const aud_clk_buf_function[] = { "Off", "On" };

/* static const char *DAC_SampleRate_function[] = {"8000", "11025", "16000", "24000", "32000", "44100", "48000"}; */
static const char *const DAC_DL_PGA_Headset_GAIN[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db",
	"-1Db", "-2Db", "-3Db",
	"-4Db", "-5Db", "-6Db", "-7Db", "-8Db", "-9Db", "-10Db", "-40Db"
};

static const char *const DAC_DL_PGA_Handset_GAIN[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db",
	"-1Db", "-2Db", "-3Db",
	"-4Db", "-5Db", "-6Db", "-7Db", "-8Db", "-9Db", "-10Db", "-40Db"
};

static const char *const DAC_DL_PGA_Speaker_GAIN[] = {
	"8Db", "7Db", "6Db", "5Db", "4Db", "3Db", "2Db", "1Db", "0Db",
	"-1Db", "-2Db", "-3Db",
	"-4Db", "-5Db", "-6Db", "-7Db", "-8Db", "-9Db", "-10Db", "-40Db"
};

/* static const char *Voice_Mux_function[] = {"Voice", "Speaker"}; */

static int Lineout_PGAL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Speaker_PGA_Get = %d\n",
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTL]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTL];
	return 0;
}

static int Lineout_PGAL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	pr_aud("%s(), index = %d\n", __func__, index);

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
		index = 0x1f;

	Ana_Set_Reg(ZCD_CON1, index, 0x001f);

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTL] = index;
	return 0;
}

static int Lineout_PGAR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s  = %d\n", __func__,
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTR]);
	ucontrol->value.integer.value[0] =
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTR];
	return 0;
}

static int Lineout_PGAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	pr_aud("%s(), index = %d\n", __func__, index);

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
		index = 0x1f;

	Ana_Set_Reg(ZCD_CON1, index << 7, 0x0f10);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTR] = index;
	return 0;
}

static int Handset_PGA_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Handset_PGA_Get = %d\n",
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL];
	return 0;
}

static int Handset_PGA_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	pr_aud("%s(), index = %d\n", __func__, index);

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN) - 1))
		index = 0x1f;

	Ana_Set_Reg(ZCD_CON3, index, 0x001f);

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL] = index;
	return 0;
}

static int Headset_PGAL_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Headset_PGAL_Get = %d\n",
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];
	return 0;
}

static int Headset_PGAL_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	/* pr_warn("%s(), index = %d arraysize = %d\n", __func__,
	index, ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)); //mark for 64bit build fail */

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1))
		index = 0x1f;

	Ana_Set_Reg(ZCD_CON2, index, 0x001f);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] = index;
	return 0;
}

static int Headset_PGAR_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Headset_PGAR_Get = %d\n",
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];
	return 0;
}


static int Headset_PGAR_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	pr_aud("%s(), index = %d\n", __func__, index);

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1))
		index = 0x1f;

	Ana_Set_Reg(ZCD_CON2, index << 7, 0x0f80);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = index;
	return 0;
}

static uint32 mHp_Impedance = 32;

static int Audio_Hp_Impedance_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Audio_Hp_Impedance_Get = %d\n", mHp_Impedance);
	ucontrol->value.integer.value[0] = mHp_Impedance;
	return 0;

}

static int Audio_Hp_Impedance_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	mHp_Impedance = ucontrol->value.integer.value[0];
	pr_aud("%s Audio_Hp_Impedance_Set = 0x%x\n", __func__, mHp_Impedance);
	return 0;
}

static int Aud_Clk_Buf_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("\%s n", __func__);
	ucontrol->value.integer.value[0] = audck_buf_Count;
	return 0;
}

static int Aud_Clk_Buf_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s(), value = %d\n", __func__, ucontrol->value.enumerated.item[0]);

	if (ucontrol->value.integer.value[0])
		audckbufEnable(true);
	else
		audckbufEnable(false);

	return 0;
}

void enable_anc_loopback3(void)
{
	Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x1 << 12, 0x1 << 12);
}

void disable_anc_loopback3(void)
{
	Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0 << 12, 0x1 << 12);
}

void pmic_select_dl_only(void)
{
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x0 << 4,
			0x1 << 4); /* dac_hp_anc_ch1_sel(0: dl, 1: anc) */
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x0 << 5,
			0x1 << 5); /* dac_hp_anc_ch2_7el(0: dl, 1: anc) */
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x0 << 6,
			0x1 << 6); /* hp_anc_fpga_de bug_ch1(0: add dl, 1: pure anc) */
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x0 << 7,
			0x1 << 7); /* hp_anc_fpga_de bug_ch2(0: add dl, 1: pure anc) */
}

void pmic_select_dl_and_anc(void)
{
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x1 << 4,
			0x1 << 4); /* dac_hp_anc_ch1_sel(0: dl, 1: anc) */
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x1 << 5,
			0x1 << 5); /* dac_hp_anc_ch2_7el(0: dl, 1: anc) */
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x0 << 6,
			0x1 << 6); /* hp_anc_fpga_de bug_ch1(0: add dl, 1: pure anc) */
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x0 << 7,
			0x1 << 7); /* hp_anc_fpga_de bug_ch2(0: add dl, 1: pure anc) */
}

void pmic_select_anc_only(void)
{
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x1 << 4,
			0x1 << 4); /* dac_hp_anc_ch1_sel(0: dl, 1: anc) */
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x1 << 5,
			0x1 << 5); /* dac_hp_anc_ch2_7el(0: dl, 1: anc) */
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x1 << 6,
			0x1 << 6); /* hp_anc_fpga_de bug_ch1(0: add dl, 1: pure anc) */
	Ana_Set_Reg(AFE_HPANC_CFG0, 0x1 << 7,
			0x1 << 7); /* hp_anc_fpga_de bug_ch2(0: add dl, 1: pure anc) */
}
static int Set_ANC_Enable(bool enable)
{
	pr_warn("%s(), value = %d\n", __func__, enable);
	if (enable == true) {
		pmic_select_dl_only();

		Audio_Amp_Change(0, true, true);
		ANC_enabled = true;

		/* mtkif sclk invert */
		Ana_Set_Reg(AFE_ADDA2_PMIC_NEWIF_CFG2, 0x1 << 15, 0x1 << 15);

		/* reset timing workaround */
		Ana_Set_Reg(AFE_ADDA2_PMIC_NEWIF_CFG1, 0x0 << 13, 0x1 << 13); /* pmic rx */
		msleep(20);
		Ana_Set_Reg(AFE_HPANC_CFG0, 0x1 << 2, 0x1 << 2); /* toggle cic */
		Ana_Set_Reg(AFE_HPANC_CFG0, 0x0 << 2, 0x1 << 2);
		Ana_Set_Reg(AFE_ADDA2_PMIC_NEWIF_CFG1, 0x1 << 13, 0x1 << 13); /* pmic rx */
		msleep(20);
		pmic_select_dl_and_anc();
	} else {
		pmic_select_dl_only();
		ANC_enabled = false;
		Audio_Amp_Change(0, false, true);
	}
	return 0;
}

static int Audio_ANC_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s(), value = %d\n", __func__);
	ucontrol->value.integer.value[0] = ANC_enabled;
	return 0;
}

static int Audio_ANC_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s(), value = %d\n", __func__);
	if (ucontrol->value.integer.value[0])
		Set_ANC_Enable(true);
	else
		Set_ANC_Enable(false);
	return 0;
}

static const struct soc_enum Audio_DL_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
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
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aud_clk_buf_function),
			    aud_clk_buf_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(amp_function), amp_function),
};

static const struct snd_kcontrol_new mt6331_snd_controls[] = {
	SOC_ENUM_EXT("Audio_Amp_R_Switch", Audio_DL_Enum[0], Audio_AmpR_Get,
		     Audio_AmpR_Set),
	SOC_ENUM_EXT("Audio_Amp_L_Switch", Audio_DL_Enum[1], Audio_AmpL_Get,
		     Audio_AmpL_Set),
	SOC_ENUM_EXT("Voice_Amp_Switch", Audio_DL_Enum[2], Voice_Amp_Get,
		     Voice_Amp_Set),
	SOC_ENUM_EXT("Speaker_Amp_Switch", Audio_DL_Enum[3],
		     Speaker_Amp_Get, Speaker_Amp_Set),
	SOC_ENUM_EXT("Headset_Speaker_Amp_Switch", Audio_DL_Enum[4],
		     Headset_Speaker_Amp_Get,
		     Headset_Speaker_Amp_Set),
	SOC_ENUM_EXT("Headset_PGAL_GAIN", Audio_DL_Enum[5],
		     Headset_PGAL_Get, Headset_PGAL_Set),
	SOC_ENUM_EXT("Headset_PGAR_GAIN", Audio_DL_Enum[6],
		     Headset_PGAR_Get, Headset_PGAR_Set),
	SOC_ENUM_EXT("Handset_PGA_GAIN", Audio_DL_Enum[7], Handset_PGA_Get,
		     Handset_PGA_Set),
	SOC_ENUM_EXT("Lineout_PGAR_GAIN", Audio_DL_Enum[8],
		     Lineout_PGAR_Get, Lineout_PGAR_Set),
	SOC_ENUM_EXT("Lineout_PGAL_GAIN", Audio_DL_Enum[9],
		     Lineout_PGAL_Get, Lineout_PGAL_Set),
	SOC_ENUM_EXT("AUD_CLK_BUF_Switch", Audio_DL_Enum[10],
		     Aud_Clk_Buf_Get, Aud_Clk_Buf_Set),
	SOC_ENUM_EXT("Ext_Speaker_Amp_Switch", Audio_DL_Enum[11],
		     Ext_Speaker_Amp_Get,
		     Ext_Speaker_Amp_Set),
	SOC_ENUM_EXT("Receiver_Speaker_Switch", Audio_DL_Enum[11],
		     Receiver_Speaker_Switch_Get,
		     Receiver_Speaker_Switch_Set),
	SOC_SINGLE_EXT("Audio HP Impedance", SND_SOC_NOPM, 0, 512, 0,
		       Audio_Hp_Impedance_Get,
		       Audio_Hp_Impedance_Set),
	SOC_ENUM_EXT("PMIC_REG_CLEAR", Audio_DL_Enum[12], PMIC_REG_CLEAR_Get, PMIC_REG_CLEAR_Set),
	SOC_ENUM_EXT("Audio_ANC_Switch", Audio_DL_Enum[0], Audio_ANC_Get, Audio_ANC_Set),
};

static const struct snd_kcontrol_new mt6331_Voice_Switch[] = {
	/* SOC_DAPM_ENUM_EXT("Voice Mux", Audio_DL_Enum[10], Voice_Mux_Get, Voice_Mux_Set), */
};

void SetMicPGAGain(void)
{
	int index = 0;

	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
	pr_aud("%s  AUDIO_ANALOG_VOLUME_MICAMP1 index =%d\n", __func__, index);
	Ana_Set_Reg(AUDENC_ANA_CON0, index << 8, 0x0700);
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
	Ana_Set_Reg(AUDENC_ANA_CON1, index << 8, 0x0700);

}

static bool GetAdcStatus(void)
{
	int i = 0;

	for (i = AUDIO_ANALOG_DEVICE_IN_ADC1; i < AUDIO_ANALOG_DEVICE_MAX; i++) {
		if ((mCodec_data->mAudio_Ana_DevicePower[i] == true)
		    && (i != AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH))
			return true;
	}
	return false;
}

static bool GetDacStatus(void)
{
	int i = 0;

	for (i = AUDIO_ANALOG_DEVICE_OUT_EARPIECER; i < AUDIO_ANALOG_DEVICE_2IN1_SPK; i++) {
		if (mCodec_data->mAudio_Ana_DevicePower[i] == true)
			return true;
	}
	return false;
}


static bool TurnOnADcPowerACC(int ADCType, bool enable)
{
	bool refmic_using_ADC_L = false;
	uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];

	pr_warn("%s ADCType = %d enable = %d, refmic_using_ADC_L=%d\n", __func__, ADCType,
	       enable, refmic_using_ADC_L);

	if (enable) {
		if (GetAdcStatus() == false) {
			audckbufEnable(true);
			/* Ana_Set_Reg(LDO_VCON1, 0x0301, 0xffff);
			   //VA28 remote sense //removed in MT6328 */

			Ana_Set_Reg(LDO_VUSB33_CON0, 0x1A62, 0x000A);
			/* LDO enable control by RG_VUSB33_EN, Enable VUSB33_LDO (AVDD30_AUD )
			   (Default on) */
			Ana_Set_Reg(LDO_VA18_CON0, 0x1A62, 0x000A);
			/* LDO enable control by RG_VA18_EN, Enable VA18_LDO (AVDD18_CODEC )
			   (Default on) */

			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0400);
			/* Globe bias VOW LPW mode (Default off) */
			NvregEnable(true);
			/* Enable audio globe bias */
			/* Ana_Set_Reg(TOP_CLKSQ, 0x0800, 0x0C00); */
			/* RG_CLKSQ_IN_SEL_VA18_SWCTRL */
			ClsqEnable(true);
			/* Enable CLKSQ 26MHz */

			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x0070);
			/* Enable audio ADC CLKGEN */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x000C);
			/* ADC CLK from CLKGEN (13MHz) */
		}

		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {	/* main and headset mic */
			pr_warn("%s  AUDIO_ANALOG_DEVICE_IN_ADC1 mux =%d\n",
				__func__, mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]);
			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				/* "ADC1", main_mic */
				SetDCcoupleNP(AUDIO_MIC_BIAS0, mAudio_Analog_Mic1_mode);
				/* micbias0 DCCopuleNP */
				/* Ana_Set_Reg(AUDENC_ANA_CON9, 0x0201, 0xff09);
				   //Enable MICBIAS0, MISBIAS0 = 1P9V */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0x00f1);
				/* Enable MICBIAS0, MISBIAS0 = 1P9V */
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				if (GetMicbias == 0) {
					MicbiasRef = Ana_Get_Reg(AUDENC_ANA_CON10) & 0x0070;
					/* save current micbias1 ref set by accdet */
					pr_warn("ACCDET Micbias1Ref=0x%x\n", MicbiasRef);
					GetMicbias = 1;
				}
				/* "ADC2", headset mic */
				SetDCcoupleNP(AUDIO_MIC_BIAS1, mAudio_Analog_Mic1_mode);
				/* micbias1 DCCopuleNP */
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0001, 0x0081);
				/* MIC Bias 1 high power mode, power on */
			}
			/* Ana_Set_Reg(AUDENC_ANA_CON15, 0x0003, 0x000f); //Audio L PGA 18 dB gain(SMT) */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0300, 0x0f00);	/* Audio L PGA 18 dB gain(SMT) */

		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {	/* ref mic */
			pr_warn
			    ("%s  AUDIO_ANALOG_DEVICE_IN_ADC2 refmic_using_ADC_L =%d\n",
			     __func__, refmic_using_ADC_L);
			SetDCcoupleNP(AUDIO_MIC_BIAS2, mAudio_Analog_Mic2_mode);
			/* micbias0 DCCopuleNP */
			/* Ana_Set_Reg(AUDENC_ANA_CON9, 0x0201, 0xff09);
			   //Enable MICBIAS0, MISBIAS0 = 1P9V */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x2100, 0xf100);
			/* Enable MICBIAS2, MISBIAS2 = 1P9V */

			if (refmic_using_ADC_L == false) {
				/* Ana_Set_Reg(AUDENC_ANA_CON15, 0x0030, 0x00f0);
				   //Audio R PGA 18 dB gain(SMT) */
				Ana_Set_Reg(AUDENC_ANA_CON1, 0x0300, 0x0f00);
				/* Audio R PGA 18 dB gain(SMT) */
			} else {
				/* Ana_Set_Reg(AUDENC_ANA_CON15, 0x0003, 0x000f);
				   //Audio L PGA 18 dB gain(SMT) */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0300, 0x0f00);
				/* Audio L PGA 18 dB gain(SMT) */
			}
		}

		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {	/* main and headset mic */

			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				/* "ADC1", main_mic */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0311, 0x003f);
				/* Audio L preamplifier input sel : AIN0. Enable audio L PGA */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5311, 0xf000);
				/* Audio L ADC input sel : L PGA. Enable audio L ADC */
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				/* "ADC2", headset mic */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0221, 0x003f);
				/* Audio L preamplifier input sel : AIN1. Enable audio L PGA */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5221, 0xf000);
				/* Audio L ADC input sel : L PGA. Enable audio L ADC */

			}
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {
			/* ref mic */

			if (refmic_using_ADC_L == false) {
				Ana_Set_Reg(AUDENC_ANA_CON1, 0x0331, 0x003f);
				/* Audio R preamplifier input sel : AIN2. Enable audio R PGA */
				Ana_Set_Reg(AUDENC_ANA_CON1, 0x5331, 0xf000);
				/* Audio R ADC input sel : R PGA. Enable audio R ADC */
			} else {
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0331, 0x003f);
				/* Audio L preamplifier input sel : AIN2. Enable audio L PGA */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5331, 0xf000);
				/* Audio L ADC input sel : L PGA. Enable audio L ADC */
			}
		}

		SetMicPGAGain();

		if (GetAdcStatus() == false) {
			/* here to set digital part */
			Topck_Enable(true);
			/* AdcClockEnable(true); */
			if (GetDacStatus() == false) {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0040, 0xffff);
				/* power on clock */
			} else {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);
				/* power on clock */
			}
			Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);
			/* configure ADC setting */
			/* Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffff);
			//sdm audio fifo clock power on */
			/* Ana_Set_Reg(AFUNC_AUD_CON0, 0xc3a1, 0xffff);
			//scrambler clock on enable */
			/* Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffff);
			//sdm power on */
			/* Ana_Set_Reg(AFUNC_AUD_CON2, 0x000b, 0xffff);
			//sdm fifo enable */
			/* Ana_Set_Reg(AFE_DL_SDM_CON1, 0x001e, 0xffff);
			//set attenuation gain */
			Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);
			/* [0] afe enable */

			Ana_Set_Reg(AFE_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL1)
							 << 1), 0x7 << 1);
			/* UL sample rate and mode configure */

			/* fixed 260k path for 8/16/32/48 */
			if (SampleRate_VUL1 <= 48000) {
				/* anc ul path src on */
				anc_ul_src_enable(true);
				/* ANC clk pdn release */
				anc_clk_enable(true);
			}

			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0001, 0xffff);
			/* UL turn on */
		}
	} else {
		if (GetAdcStatus() == false) {
			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0x0001);
			/* UL turn off */

			if (SampleRate_VUL1 <= 48000) {
				/* anc ul path src off */
				anc_ul_src_enable(false);
				/* ANC clk pdn enable */
				anc_clk_enable(false);
			}

			Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0020, 0x0020);
			/* up-link power down */
		}
		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {
			/* main and headset mic */
			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				/* "ADC1", main_mic */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0311, 0xf000);
				/* Audio L ADC input sel : off, disable audio L ADC */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
				/* Audio L preamplifier input sel : off, Audio L PGA 0 dB gain */
				/* Disable audio L PGA */
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				/* "ADC2", headset mic */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0221, 0xf000);
				/* Audio L ADC input sel : off, disable audio L ADC */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
				/* Audio L preamplifier input sel : off, Audio L PGA 0 dB gain */
				/* Disable audio L PGA */
			}

			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				/* "ADC1", main_mic */
				/* Ana_Set_Reg(AUDENC_ANA_CON9, (MicbiasRef|0x0000), 0xff09);
				   //disable MICBIAS0, restore to micbias set by accdet */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x00ff);
				/* disable MICBIAS0, MISBIAS0 = 1P7 */
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				/* "ADC2", headset mic */
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x0001);
				/* MIC Bias 1 power down */
				GetMicbias = 0;
			}
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {	/* ref mic */
			if (refmic_using_ADC_L == false) {
				Ana_Set_Reg(AUDENC_ANA_CON1, 0x0331, 0xf000);
				/* Audio R ADC input sel : off, disable audio R ADC */
				Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x0fff);
				/* Audio R preamplifier input sel : off, Audio R PGA 0 dB gain */
				/* Disable audio R PGA */
			} else {
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0331, 0xf000);
				/* Audio L ADC input sel : off, disable audio L ADC */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
				/* Audio L preamplifier input sel : off, Audio L PGA 0 dB gain */
				/* Disable audio L PGA */
			}

			/* Ana_Set_Reg(AUDENC_ANA_CON9, (MicbiasRef|0x0000), 0xff09);
			   //disable MICBIAS0, restore to micbias set by accdet */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0xff00);
			/* disable MICBIAS2, MISBIAS2 = 1P7 */
		}
		if (GetAdcStatus() == false) {
			/* Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0000, 0x0006); */
			/* LCLDO_ENC remote sense off */
			/* Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0004, 0x0104); */
			/* disable LCLDO_ENC 1P8V */

			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x000f);
			/* disable ADC CLK from CLKGEN (13MHz) */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0x0070);
			/* disable audio ADC CLKGEN */

			if (GetDLStatus() == false) {
				Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
				/* afe disable */
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x00C4, 0x00C4);
				/* afe power down and total audio clk disable */
			}

			/* AdcClockEnable(false); */
			Topck_Enable(false);
			/* ClsqAuxEnable(false); */
			ClsqEnable(false);
			NvregEnable(false);
			audckbufEnable(false);
		}

	}
	return true;
}

static bool TurnOnADcPowerDmic(int ADCType, bool enable)
{
	pr_warn("%s ADCType = %d enable = %d\n", __func__, ADCType, enable);
	if (enable) {
		uint32 ULIndex = GetULFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]);
		uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];
		/* uint32 SampleRate_VUL2 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC_2]; */

		if (GetAdcStatus() == false) {
			audckbufEnable(true);
			Ana_Set_Reg(LDO_VUSB33_CON0, 0x1A62, 0x000A);
			/* LDO enable control by RG_VUSB33_EN, Enable VUSB33_LDO (AVDD30_AUD )
			   (Default on) */
			Ana_Set_Reg(LDO_VA18_CON0, 0x1A62, 0x000A);
			/* LDO enable control by RG_VA18_EN, Enable VA18_LDO (AVDD18_CODEC )
			   (Default on) */

			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0400);
			/* Globe bias VOW LPW mode (Default off) */
			NvregEnable(true);
			/* ClsqAuxEnable(true); */
			ClsqEnable(true);

			SetDCcoupleNP(AUDIO_MIC_BIAS0, mAudio_Analog_Mic1_mode);
			/* micbias0 DCCopuleNP */
			/* Ana_Set_Reg(AUDENC_ANA_CON9, 0x0201, 0xff09); //Enable MICBIAS0, MISBIAS0 = 1P9V */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0x00ff);
			/* Enable MICBIAS0 , MISBIAS0 = 1P9V */
			Ana_Set_Reg(AUDENC_ANA_CON8, 0x0005, 0x0007);
			/* DMIC enable */

			/* here to set digital part */
			Topck_Enable(true);
			/* AdcClockEnable(true); */
			if (GetDacStatus() == false) {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0040, 0xffff);
				/* power on clock  // ? MT6328 George check */
				/* George  ]0x00O}A]0x40ODAC ,   SW]0x5aODADCclockAhٹq */
			} else {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);
				/* power on clock */
			}
			Ana_Set_Reg(PMIC_AFE_TOP_CON0, (ULIndex << 7) | (ULIndex << 6), 0xffff);
			/* dmic sample rate, ch1 and ch2 set to 3.25MHz 48k // ? MT6328 George check */
			Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);
			/* [0] afe enable */

			Ana_Set_Reg(AFE_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL1)
							 << 1), 0x000E);
			/* UL sample rate and mode configure */
			Ana_Set_Reg(AFE_UL_SRC_CON0_H, 0x00e0, 0xfff0);
			/* 2-wire dmic mode, ch1 and ch2 digital mic ON */

			/* use 260k/130/65k@18bit mt6797:LP uplink */
			if (SampleRate_VUL1 <= 48000) {
				/* use cic out */
				Ana_Set_Reg(AFE_UL_SRC_CON0_H, 0x1 << 4, 0x1 << 4);
			} else {	/* hires */
				/* use 4.33MHz mode */
				Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x1 << 6, 0x1 << 6);
			}

			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0003, 0xffBf);
			/* digmic input mode 3.25MHz, select SDM 3-level mode, UL turn on */
		}
	} else {
		if (GetAdcStatus() == false) {
			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0xffff);
			/* UL turn off */
			Ana_Set_Reg(AFE_UL_SRC_CON0_H, 0x0000, 0xfff0);
			/* ch1 and ch2 digital mic OFF */
			Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0020, 0x0020);
			/* up-link power down // ? MT6328 George check */

			Ana_Set_Reg(AUDENC_ANA_CON8, 0x0004, 0x0007);
			/* DMIC disable */

			/* Ana_Set_Reg(AUDENC_ANA_CON9, (MicbiasRef|0x0000), 0xff09);
			   //MICBIAS0(1.7v), powen down, restore to micbias set by accdet */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x00ff);
			/* MICBIAS0(1.7v), Disable MICBIAS0 */
			if (GetDLStatus() == false) {
				Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
				/* afe disable */
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x00C4, 0x00C4);
				/* afe power down and total audio clk disable */
			}

			/* AdcClockEnable(false); */
			Topck_Enable(false);
			/* ClsqAuxEnable(false); */
			ClsqEnable(false);
			NvregEnable(false);
			audckbufEnable(false);
		}
	}
	return true;
}

static bool TurnOnADcPowerDCC(int ADCType, bool enable, int ECMmode)
{
	uint32 SampleRate_VUL1 = mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];

	pr_warn("%s(), enable %d, ADCType %d, AUDIO_MICSOURCE_MUX_IN_1 %d\n",
		__func__,
		enable,
		ADCType,
		mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]);

	if (enable) {
		if (GetAdcStatus() == false) {
			audckbufEnable(true);
			/* Ana_Set_Reg(LDO_VCON1, 0x0301, 0xffff);
			   //VA28 remote sense //removed in MT6328 */

			Ana_Set_Reg(LDO_VUSB33_CON0, 0x1A62, 0x000A);
			/* LDO enable control by RG_VUSB33_EN, Enable VUSB33_LDO (AVDD30_AUD )
			   (Default on) */
			Ana_Set_Reg(LDO_VA18_CON0, 0x1A62, 0x000A);
			/* LDO enable control by RG_VA18_EN, Enable VA18_LDO (AVDD18_CODEC )
			   (Default on) */

			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0400);
			/* Globe bias VOW LPW mode (Default off) */
			NvregEnable(true);
			/* Enable audio globe bias */
			/* Ana_Set_Reg(TOP_CLKSQ, 0x0800, 0x0C00); */
			/* RG_CLKSQ_IN_SEL_VA18_SWCTRL */
			ClsqEnable(true);
			/* Enable CLKSQ 26MHz */

			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x0070);
			/* Enable audio ADC CLKGEN */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x000C);
			/* ADC CLK from CLKGEN (13MHz) */

			/* DCC 50k CLK (from 26M) */
			Ana_Set_Reg(TOP_CKPDN_CON0, 0x0000, 0x6000);
			/* bit[14]+bit[13] AUD_CK power on=0 MT6328 */
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
			/* dcclk_div=11'b00100000011, dcclk_ref_ck_sel=2'b00 */
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2060, 0xffff);
			/* dcclk_pdn=1'b0 */
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2061, 0xffff);
			/* dcclk_gen_on=1'b1 */

			/* Ana_Set_Reg(AFE_DCCLK_CFG1, 0x0100, 0x0100); */
			/* dcclk_resync_bypass=1'b1 MT6328 requested by chen-chien(2014/10/30) */
		}
		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {
			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				/* "ADC1", phone mic */
				SetDCcoupleNP(AUDIO_MIC_BIAS0, mAudio_Analog_Mic1_mode);
				/* micbias0 DCCopuleNP */
				/* Ana_Set_Reg(AUDENC_ANA_CON9, 0x0201, 0xff09);
				   //Enable MICBIAS0, MISBIAS0 = 1P9V */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0x00f1);
				/* Enable MICBIAS0, MISBIAS0 = 1P9V */
#if 0
				if (ECMmode == 1) {	/* differential */
					/* Ana_Set_Reg(AUDENC_ANA_CON9, 0x0217, 0xff1f);
					   //Enable MICBIAS0 and MICBIAS1, MISBIAS0 = 1P9V, [6752] ALPS01824949 */
					Ana_Set_Reg(AUDENC_ANA_CON9, 0x000E, 0x000E);
					/* Enable MICBIAS0 and MICBIAS1, MISBIAS0 = 2P5V, [6752] ALPS01824949 */

				} else if (ECMmode == 2) {	/* single end */
					/* Ana_Set_Reg(AUDENC_ANA_CON9, 0x0213, 0xff1f);
					   //Enable MICBIAS0 and MICBIAS1, MISBIAS0 = 1P9V, [6752] ALPS01824949 */
					Ana_Set_Reg(AUDENC_ANA_CON9, 0x0002, 0x000E);
					/* Enable MICBIAS0 and MICBIAS1, MISBIAS0 = 2P5V */
				} else {	/* MEMS */
					/* Ana_Set_Reg(AUDENC_ANA_CON9, 0x0211, 0xff1f);
					   //Enable MICBIAS0 and MICBIAS1, MISBIAS0 = 1P9V , [6752] ALPS01824949 */
					Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x000E);
					/* Enable MICBIAS0 and MICBIAS1, MISBIAS0 = 2P5V */
				}
#endif
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0317, 0x073f);
				/* Audio L preamplifier DCC precharge, Audio L preamplifier input sel : AIN0
				   Audio L PGA 18 dB gain */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5317, 0xf000);
				/* Audio L ADC input sel : L PGA. Enable audio L ADC */
				udelay(100);
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5313, 0x0004);
				/* Audio L preamplifier DCC precharge off */
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				/* "ADC2", headset mic */
				if (GetMicbias == 0) {
					MicbiasRef = Ana_Get_Reg(AUDENC_ANA_CON10) & 0x0070;
					/* save current micbias1 ref set by accdet */
					pr_warn("ACCDET Micbias1Ref=0x%x\n", MicbiasRef);
					GetMicbias = 1;
				}
				SetDCcoupleNP(AUDIO_MIC_BIAS1, mAudio_Analog_Mic1_mode);
				/* micbias1 DCCopuleNP */
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0001, 0x0081);
				/* MIC Bias 1 high power mode, power on */
#if 0
				if (ECMmode == 1) {	/* differential */
					Ana_Set_Reg(AUDENC_ANA_CON10, 0x0004, 0x0006);
					/* Enable MICBIAS1, SwithP/N MISBIAS1 = 2P5V */
				} else if (ECMmode == 2) {	/* single end */
					Ana_Set_Reg(AUDENC_ANA_CON10, 0x0004, 0x0006);
					/* Enable MICBIAS1 and MICBIAS1, MISBIAS0 = 1P9V, [6752] ALPS01824949 */
				} else {	/* MEMS */
					/* Ana_Set_Reg(AUDENC_ANA_CON9, 0x0710, 0xfff0); //MICBIAS1 DCC  on //DCC */
					Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x0006);
					/* MICBIAS1 DCC  on //DCC, bit 5 to 1 to match ACCDET */
				}
#endif
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0327, 0x073f);
				/* Audio L preamplifier DCC precharge, Audio L preamplifier input sel : AIN1
				   Audio L PGA 18 dB gain */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5327, 0xf000);
				/* Audio L ADC input sel : L PGA. Enable audio L ADC */
				udelay(100);
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5323, 0x0004);
				/* Audio L preamplifier DCC precharge off */
			}
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {
			SetDCcoupleNP(AUDIO_MIC_BIAS0, mAudio_Analog_Mic2_mode);
			/* micbias0 DCCopuleNP */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x2100, 0xf100);
			/* Enable MICBIAS2, MISBIAS2 = 1P9V */
#if 0
			if (ECMmode == 1) {	/* differenital */
				/* Ana_Set_Reg(AUDENC_ANA_CON9, 0x0217, 0xff1f);
				   //Enable MICBIAS0 and MICBIAS1, MISBIAS0 = 1P9V , [6752] ALPS01824949 */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0717, 0xff1f);
				/* Enable MICBIAS0 and MICBIAS1, MISBIAS0 = 2P5V */

			} else if (ECMmode == 2) {	/* single end */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0713, 0xff1f);
				/* Enable MICBIAS0 and MICBIAS1, MISBIAS0 = 2P5V */
			} else {	/* MEMS */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0711, 0xff1f);
				/* Enable MICBIAS0 and MICBIAS1, MISBIAS0 = 2P5V */
			}
#endif
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0300, 0x0f00);
			/* Audio R PGA 18 dB gain(SMT) */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0337, 0x003f);
			/* Audio R preamplifier input sel : AIN2. Enable audio R PGA */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x5337, 0xf000);
			/* Audio R ADC input sel : R PGA. Enable audio R ADC */
			udelay(100);
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x5333, 0x0004);
			/* Audio R preamplifier DCC precharge off */
		}

		SetMicPGAGain();

		if (GetAdcStatus() == false) {
			/* here to set digital part */
			Topck_Enable(true);
			/* AdcClockEnable(true); */

			/* Audio system digital clock power down release */
			if (GetDacStatus() == false) {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0040, 0xffff);
				/* power on clock */
			} else {
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);
				/* power on clock */
			}

			Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);
			/* configure ADC setting */
			Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);
			/* [0] afe enable */

			Ana_Set_Reg(AFE_UL_SRC_CON0_H, (ULSampleRateTransform(SampleRate_VUL1)
							 << 1), 0x7 << 1);
			/* UL sample rate and mode configure */

			/* fixed 260k path for 8/16/32/48 */
			if (SampleRate_VUL1 <= 48000) {
				/* anc ul path src on */
				anc_ul_src_enable(true);
				/* ANC clk pdn release */
				anc_clk_enable(true);
			}

			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0001, 0xffff);
			/* UL turn on */
		}
#if 0
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0002, 0x2);	/* set DL sine gen table */
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0080, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
#endif
	} else {
		if (GetAdcStatus() == false) {
			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0xffff);
			/* UL turn off */

			if (SampleRate_VUL1 <= 48000) {
				/* anc ul path src off */
				anc_ul_src_enable(false);
				/* ANC clk pdn enable */
				anc_clk_enable(false);
			}

			Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0020, 0x0020);
			/* up-link power down */
		}
		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {
			/* main and headset mic */
			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				/* "ADC1", main_mic */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0313, 0xf000);
				/* Audio L ADC input sel : off, disable audio L ADC */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
				/* Audio L preamplifier input sel : off, Audio L PGA 0 dB gain */
				/* Disable audio L PGA */
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				/* "ADC2", headset mic */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0323, 0xf000);
				/* Audio L ADC input sel : off, disable audio L ADC */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
				/* Audio L preamplifier input sel : off, Audio L PGA 0 dB gain */
				/* Disable audio L PGA */
			}

			if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] == 0) {
				/* "ADC1", main_mic */
				/* Ana_Set_Reg(AUDENC_ANA_CON9, (MicbiasRef|0x0000), 0xff09);
				   //disable MICBIAS0, restore to micbias set by accdet */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x00ff);
				/* disable MICBIAS0 and MICBIAS1, restore to micbias set by accdet */
			} else if (mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]
				   == 1) {
				/* "ADC2", headset mic */
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x0001);
				/* disable MICBIAS1 */
				GetMicbias = 0;
			}
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {
			/* ref mic */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0333, 0xf000);
			/* Audio R ADC input sel : off, disable audio R ADC */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x0fff);
			/* Audio R preamplifier input sel : off, Audio R PGA 0 dB gain */
			/* Disable audio R PGA */

			/* Ana_Set_Reg(AUDENC_ANA_CON9, (MicbiasRef|0x0000), 0xff09);
			   //disable MICBIAS0, restore to micbias set by accdet */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0xff00);
			/* disable MICBIAS2, MISBIAS2 = 1P7 */
		}

		if (GetAdcStatus() == false) {
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2060, 0x0001);
			/* dcclk_gen_on=1'b0 */
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x0fe2, 0xffff);
			/* Default Value, dcclk_pdn=1'b0 */

			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0040, 0x000f);
			/* disable ADC CLK from CLKGEN (13MHz) */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0x0070);
			/* disable audio ADC CLKGEN */

			if (GetDLStatus() == false) {
				Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
				/* afe disable */
				Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x00C4, 0x00C4);
				/* afe power down and total audio clk disable */
			}

			/* AdcClockEnable(false); */
			Topck_Enable(false);
			/* ClsqAuxEnable(false); */
			ClsqEnable(false);
			NvregEnable(false);
			audckbufEnable(false);
		}
	}
	return true;
}


static bool TurnOnVOWDigitalHW(bool enable)
{
#ifdef CONFIG_MTK_VOW_SUPPORT
	pr_warn("%s enable = %d\n", __func__, enable);
	if (enable) {
		/*move to vow driver*/
#ifdef VOW_STANDALONE_CONTROL
		if (mAudio_VOW_Mic_type == AUDIO_VOW_MIC_TYPE_Handset_DMIC)
			Ana_Set_Reg(AFE_VOW_TOP, 0x6850, 0xffff);   /*VOW enable*/
		else
			Ana_Set_Reg(AFE_VOW_TOP, 0x4810, 0xffff);   /*VOW enable*/
#endif
	} else {
		/*disable VOW interrupt here*/
		/*Ana_Set_Reg(INT_CON0, 0x0015, 0x0800); //disable VOW interrupt. BIT11*/
#ifdef VOW_STANDALONE_CONTROL
		/*move to vow driver*/

		Ana_Set_Reg(AFE_VOW_TOP, 0x4010, 0xffff);   /*VOW disable*/
		Ana_Set_Reg(AFE_VOW_TOP, 0xC010, 0xffff);   /*VOW clock power down*/
#endif
	}
#endif
	return true;
}

#ifdef CONFIG_MTK_VOW_SUPPORT
static void TurnOnVOWPeriodicOnOff(int MicType, int On_period, int enable)
{
	int i = 0;
	uint16 (*pBuf)[22];

	if ((MicType == AUDIO_VOW_MIC_TYPE_Headset_MIC)
	 || (MicType == AUDIO_VOW_MIC_TYPE_Handset_AMIC)
	 || (MicType >= AUDIO_VOW_MIC_TYPE_NUM)
	 || (MicType < 0)) {
		pr_warn("MicType:%d, No support periodic On/Off\n", MicType);
		return;
	}

	if (enable == 0) {
		pr_warn("%s, enable:%d\n", __func__, enable);

		VOW32KCK_Enable(false);
		Ana_Set_Reg(AFE_VOW_PERIODIC_CFG13, 0x8000, 0x8000);
		Ana_Set_Reg(AFE_VOW_PERIODIC_CFG14, 0x0000, 0x8000);
		for (i = 0; i < 22; i++)
			Ana_Set_Reg(AFE_VOW_PERIODIC_CFG2 + (i<<1), 0x0000, 0xFFFF);

		/* Set Period */
		Ana_Set_Reg(AFE_VOW_PERIODIC_CFG0, 0x0000, 0xFFFF);

	} else {
		pr_warn("%s, MicType:%d, On_period:%d, enable:%d\n", __func__, MicType, 100 - (On_period * 10), enable);

		VOW32KCK_Enable(true);
		switch (MicType) {
		case AUDIO_VOW_MIC_TYPE_Handset_DMIC:
		case AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K:
			pBuf = Handset_DMIC_PeriodicOnOff;
			break;

		case AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCC:
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCCECM:
			pBuf = Handset_AMIC_DCC_PeriodicOnOff;
			break;

		case AUDIO_VOW_MIC_TYPE_Headset_MIC_DCC:
		case AUDIO_VOW_MIC_TYPE_Headset_MIC_DCCECM:
			pBuf = Headset_MIC_DCC_PeriodicOnOff;
			break;

		default:
			break;
		}
		if (On_period > 0) {
			/*  <Periodic ON>  */
			/* 32k_switch, [15]=0 */
			/* vow_pwrapper_write_bits(AFE_VOW_PERIODIC_CFG13, 0, 15, 1); */
			Ana_Set_Reg(AFE_VOW_PERIODIC_CFG13, 0x0000, 0x8000);
			/* vow_snrdet_periodic_cfg  = 1 */
			Ana_Set_Reg(AFE_VOW_PERIODIC_CFG14, 0x8000, 0x8000);
			for (i = 0; i < 22; i++) {
				Ana_Set_Reg(AFE_VOW_PERIODIC_CFG2 + (i<<1), pBuf[On_period - 1][i], 0xFFFF);
				pr_aud("Addr:%x, Value:%x\n",
					AFE_VOW_PERIODIC_CFG2 + (i<<1), pBuf[On_period - 1][i]);
			}
		} else {
			Ana_Set_Reg(AFE_VOW_PERIODIC_CFG13, 0x8000, 0x8000);
			Ana_Set_Reg(AFE_VOW_PERIODIC_CFG14, 0x0000, 0x8000);
			for (i = 0; i < 22; i++)
				Ana_Set_Reg(AFE_VOW_PERIODIC_CFG2 + (i<<1), 0x0000, 0xFFFF);
		}
		/* Set Period */
		Ana_Set_Reg(AFE_VOW_PERIODIC_CFG0, 0x999A, 0xFFFF);
		pr_warn("AFE_VOW_PERIODIC_CFG0:%x\n", Ana_Get_Reg(AFE_VOW_PERIODIC_CFG0));
	}
}

static void VOW_GPIO_Enable(bool enable)
{
	if (enable == true) {
		/* set AP side GPIO */
		/*Enable VOW_CLK_MISO*/
		/*Enable VOW_DAT_MISO*/
		AudDrv_GPIO_Request(true, Soc_Aud_Digital_Block_ADDA_VOW);
		/*set PMIC side GPIO*/
		Ana_Set_Reg(GPIO_MODE3, 0x1252, 0x0007); /* 0x60D0 GPIO Set to VOW data */
	} else {
		/* set AP side GPIO */
		/*Disable VOW_CLK_MISO*/
		/*Disable VOW_DAT_MISO*/
		AudDrv_GPIO_Request(false, Soc_Aud_Digital_Block_ADDA_VOW);
		/*set PMIC GPIO*/
		Ana_Set_Reg(GPIO_MODE3, 0x1249, 0x0007); /* 0x60D0 GPIO Set to VOW data */
	}
}

static void VOW_Pwr_Enable(int MicType, bool enable)
{
	if ((MicType >= AUDIO_VOW_MIC_TYPE_NUM) || (MicType < 0)) {
		pr_warn("%s(),Not support this Mic Type\n", __func__);
		return;
	}

	if (enable == true) {
		/* 0x0D06 Enable Globe bias VOW LPW mode */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0400, 0x0400);

		NvregEnable(true); /* 0x0D04 Enable audio globe bias */

		if ((MicType != AUDIO_VOW_MIC_TYPE_Handset_DMIC)
		 && (MicType != AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K)) {
			/* 0x0D0C Enable audio uplink LPW mode, Enable Audio ADC 1st, 2nd & 3rd Stage LPW,
			Enable Audio ADC flash Audio ADC flash */
			Ana_Set_Reg(AUDENC_ANA_CON2,  0x0039, 0x0039);
		}

		/* 0x0D26 set PLL VCOBAND */
		Ana_Set_Reg(AUDENC_ANA_CON15, 0x0023, 0x0038);
		/* 0x0D24 PLL low power */
		Ana_Set_Reg(AUDENC_ANA_CON14, 0x8180, 0x8000);
		/* 0x0D22 PLL devider ratio, Enable fbdiv relatch, Set DCKO = 1/4 F_PLL, Enable VOWPLL CLK */
		Ana_Set_Reg(AUDENC_ANA_CON13, 0x06F8, 0x07FC);
		/* 0x0D22 PLL devider ratio, Enable fbdiv relatch, Set DCKO = 1/4 F_PLL, Enable VOWPLL CLK */
		Ana_Set_Reg(AUDENC_ANA_CON13, 0x06F9, 0x0001);
	} else {
		Ana_Set_Reg(AUDENC_ANA_CON13, 0x06F8, 0x0001); /*0x0D22*/
		Ana_Set_Reg(AUDENC_ANA_CON13, 0x0310, 0x07FC); /*0x0D22*/
		Ana_Set_Reg(AUDENC_ANA_CON14, 0x0180, 0x8000); /*0x0D24*/
		Ana_Set_Reg(AUDENC_ANA_CON15, 0x0013, 0x0038); /*0x0D26*/
		if ((MicType != AUDIO_VOW_MIC_TYPE_Handset_DMIC)
		 && (MicType != AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K)) {
			/* 0x0D0C Disable audio uplink LPW mode, Disable Audio ADC 1st, 2nd & 3rd Stage LPW,
			Disable Audio ADC flash Audio ADC flash */
			Ana_Set_Reg(AUDENC_ANA_CON2,  0x0000, 0x0039);
		}

		NvregEnable(false); /* 0x0D04 Disable audio globe bias */

		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0x0400); /*0x0D06*/
	}
}

static void VOW_DCC_CLK_Enable(bool enable)
{
	if (enable == true) {
		VOW12MCK_Enable(true); /* 0x0258 VOW12M_CK power on */

		/* 0x2070 VOW source clock power on, vow_1p6m_800k_sel=1.6m */
		Ana_Set_Reg(AFE_VOW_TOP,      0x4000, 0x8000);
		/* 0x2090 PGA DCC CLK = 13M, PGA DCC CLK reference select: 01_vow 12.854M,
		PGA DCC CLK power down gating release, Enable PGA DCC CLK */
		Ana_Set_Reg(AFE_DCCLK_CFG0,   0x2064, 0xFFEE);
		/* 0x2090 PGA DCC CLK = 13M, PGA DCC CLK reference select: 01_vow 12.854M,
		PGA DCC CLK power down gating release, Enable PGA DCC CLK */
		Ana_Set_Reg(AFE_DCCLK_CFG0,   0x2065, 0x0001);
	} else {
		Ana_Set_Reg(AFE_DCCLK_CFG0,   0x2064, 0x0001); /*0x2090*/
		Ana_Set_Reg(AFE_DCCLK_CFG0,   0x0FE2, 0xFFEE); /*0x2090*/
		Ana_Set_Reg(AFE_VOW_TOP,      0xC000, 0x8000); /*0x2070*/

		VOW12MCK_Enable(false); /*VOW clock power down enable*/
	}
}

static void VOW_ACC_CLK_Enable(bool enable)
{
	if (enable == true) {
		VOW12MCK_Enable(true); /* 0x0258 VOW12M_CK power on */

		/* 0x2070 VOW source clock power on, vow_1p6m_800k_sel=1.6m */
		Ana_Set_Reg(AFE_VOW_TOP, 0x4000, 0x8000);
	} else {
		Ana_Set_Reg(AFE_VOW_TOP, 0xC000, 0x8000); /*0x2070*/

		VOW12MCK_Enable(false); /*VOW clock power down enable*/
	}
}

static void VOW_DMIC_CLK_Enable(bool enable)
{
	if (enable == true) {
		VOW12MCK_Enable(true); /* 0x0258 VOW12M_CK power on */

		/* 0x2070 VOW source clock power on, vow_1p6m_800k_sel=1.6m */
		Ana_Set_Reg(AFE_VOW_TOP, 0x4000, 0x8000);
	} else {
		Ana_Set_Reg(AFE_VOW_TOP, 0xC000, 0x8000); /*0x2070*/

		VOW12MCK_Enable(false); /*VOW clock power down enable*/
	}
}

static void VOW_MIC_DCC_Enable(int MicType, bool enable)
{
	if ((MicType >= AUDIO_VOW_MIC_TYPE_NUM) || (MicType < 0)) {
		pr_warn("%s(),Not support this Mic Type\n", __func__);
		return;
	}
	if (enable == true) {
		/* 0x0D0E ADC CLK from: 10_12.58MHz from 32KHz PLL, Enable Audio ADC FBDAC 0.25FS LPW */
		Ana_Set_Reg(AUDENC_ANA_CON3, 0x0009, 0x000D);
		Ana_Set_Reg(AUDENC_ANA_CON6, 0x0030, 0xFFFF); /* for little signal be broken issue */

		switch (MicType) {
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCC:
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCCECM:
			/* 0x0D1A MIC Bias 0 LowPower enable, MISBIAS0 = 1.9V, Enable MICBIAS0 */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x00A1, 0x00F1);
			if (MicType == AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCCECM)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x000E, 0x000E); /* ECM diff mode */
			else
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x000E); /* normal mode */

			/* 0x0D08 Enable audio L PGA */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x03D7, 0x07F7);
			break;

		case AUDIO_VOW_MIC_TYPE_Headset_MIC_DCC:
		case AUDIO_VOW_MIC_TYPE_Headset_MIC_DCCECM:
		default:
			/* 0x0D1C MIC Bias 1 LowPower: 0_Normal, 1_LPW, MISBIAS1 = 2P7V, Enable MICBIAS1 */
			Ana_Set_Reg(AUDENC_ANA_CON10, 0x00F1, 0x00F1);
			if (MicType == AUDIO_VOW_MIC_TYPE_Headset_MIC_DCCECM)
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0002, 0x0006); /* ECM single mode */
			else
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x0006); /* normal mode */

			/* 0x0D08 Enable audio L PGA */
			Ana_Set_Reg(AUDENC_ANA_CON0,  0x03E7, 0x07F7);
			break;
		}
		/* 0x0D08 Audio L ADC input sel : L PGA, Enable audio L ADC */
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x5000, 0x7000);

		msleep(20);/* delay */

		/* 0x0D08 Audio L PGA DCC precharge off */
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0004);
	} else {

		switch (MicType) {
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCC:
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCCECM:
			Ana_Set_Reg(AUDENC_ANA_CON0,  0x03D3, 0x7000); /*0x0D08*/
			Ana_Set_Reg(AUDENC_ANA_CON0,  0x0000, 0x07F7); /*0x0D08*/
			Ana_Set_Reg(AUDENC_ANA_CON9,  0x0000, 0x00FF); /*0x0D1A*/
			Ana_Set_Reg(AUDENC_ANA_CON3,  0x0000, 0x000D); /*0x0D0E*/
			break;
		case AUDIO_VOW_MIC_TYPE_Headset_MIC_DCC:
		case AUDIO_VOW_MIC_TYPE_Headset_MIC_DCCECM:
		default:
			Ana_Set_Reg(AUDENC_ANA_CON0,  0x03E3, 0x7000); /*0x0D08*/
			Ana_Set_Reg(AUDENC_ANA_CON0,  0x0000, 0x07F7); /*0x0D08*/
			Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x00F7); /*0x0D1C*/
			Ana_Set_Reg(AUDENC_ANA_CON3,  0x0000, 0x000D); /*0x0D0E*/
			break;
		}
		Ana_Set_Reg(AUDENC_ANA_CON6, 0x1515, 0xFFFF); /* for little signal be broken issue */
	}
}

static void VOW_MIC_ACC_Enable(int MicType, bool enable)
{
	if ((MicType >= AUDIO_VOW_MIC_TYPE_NUM) || (MicType < 0)) {
		pr_warn("%s(),Not support this Mic Type\n", __func__);
		return;
	}
	if (enable == true) {
		/* 0x0D0E ADC CLK from: 10_12.58MHz from 32KHz PLL, Enable Audio ADC FBDAC 0.25FS LPW */
		Ana_Set_Reg(AUDENC_ANA_CON3, 0x0009, 0x000D);
		Ana_Set_Reg(AUDENC_ANA_CON6, 0x0030, 0xFFFF); /* for little signal be broken issue */

		switch (MicType) {
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC:
			/* 0x0D1A MIC Bias 0 LowPower: 0_Normal, 1_LPW, MISBIAS1 = 2P7V, Enable MICBIAS1 */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x00A1, 0x00F1);
			/* 0x0D08 Audio L PGA precharge off, Audio L PGA mode: 0_ACC,
			Audio L preamplifier input sel : AIN0, Audio L PGA 18 dB gain, Enable audio L PGA */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0311, 0x0737);
			break;
		case AUDIO_VOW_MIC_TYPE_Headset_MIC:
		default:
			/* 0x0D1C MIC Bias 1 LowPower: 0_Normal, 1_LPW, MISBIAS1 = 2P7V, Enable MICBIAS1 */
			Ana_Set_Reg(AUDENC_ANA_CON10, 0x00F1, 0x00F1);
			/* 0x0D08 Audio L PGA precharge off, Audio L PGA mode: 0_ACC,
			Audio L preamplifier input sel : AIN1, Audio L PGA 18 dB gain, Enable audio L PGA */
			Ana_Set_Reg(AUDENC_ANA_CON0,  0x0321, 0x0737);
			break;
		}

		/* 0x0D08 Audio L ADC input sel : L PGA, Enable audio L ADC */
		Ana_Set_Reg(AUDENC_ANA_CON0,  0x5000, 0x7000);
	} else {
		switch (MicType) {
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC:
			Ana_Set_Reg(AUDENC_ANA_CON0,  0x0311, 0x7000); /*0x0D08*/
			Ana_Set_Reg(AUDENC_ANA_CON0,  0x0000, 0x0737); /*0x0D08*/
			Ana_Set_Reg(AUDENC_ANA_CON9,  0x0000, 0x00F1); /*0x0D1A*/
			Ana_Set_Reg(AUDENC_ANA_CON3,  0x0000, 0x000D); /*0x0D0E*/
			break;
		case AUDIO_VOW_MIC_TYPE_Headset_MIC:
		default:
			Ana_Set_Reg(AUDENC_ANA_CON0,  0x0321, 0x7000); /*0x0D08*/
			Ana_Set_Reg(AUDENC_ANA_CON0,  0x0000, 0x0737); /*0x0D08*/
			Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x00F1); /*0x0D1C*/
			Ana_Set_Reg(AUDENC_ANA_CON3,  0x0000, 0x000D); /*0x0D0E*/
			break;
		}
		Ana_Set_Reg(AUDENC_ANA_CON6, 0x1515, 0xFFFF); /* for little signal be broken issue */
	}
}
#endif

static bool TurnOnVOWADcPower(int MicType, bool enable)
{
#ifdef CONFIG_MTK_VOW_SUPPORT
	pr_warn("%s MicType = %d enable = %d, mIsVOWOn=%d, mAudio_VOW_Mic_type=%d\n",
		__func__, MicType, enable, mIsVOWOn, mAudio_VOW_Mic_type);

	if ((MicType >= AUDIO_VOW_MIC_TYPE_NUM) || (MicType < 0)) {
		pr_warn("%s(),Not support this Mic Type\n", __func__);
		return false;
	}

	/*already on, no need to set again*/
	if (enable == mIsVOWOn)
		return true;

	if (enable) {
		mIsVOWOn = true;
		SetVOWStatus(mIsVOWOn);

		if (GetMicbias == 0) {
			/* save current micbias ref set by accdet */
			MicbiasRef = Ana_Get_Reg(AUDENC_ANA_CON9) & 0x0700;
			pr_warn("MicbiasRef=0x%x\n", MicbiasRef);
			GetMicbias = 1;
		}

		VOW_Pwr_Enable(MicType, true);

		switch (MicType) {
		/* for ACC Mic */
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC:  /* AMIC_ACC */
		case AUDIO_VOW_MIC_TYPE_Headset_MIC: /* Earphone_ACC */
			VOW_ACC_CLK_Enable(true);
			VOW_MIC_ACC_Enable(MicType, true);
			break;
		/* for DCC Mic */
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCC:  /* AMIC_DCC */
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCCECM:
		case AUDIO_VOW_MIC_TYPE_Headset_MIC_DCC:   /* Earphone_DCC */
		case AUDIO_VOW_MIC_TYPE_Headset_MIC_DCCECM:
			VOW_DCC_CLK_Enable(true);
			VOW_MIC_DCC_Enable(MicType, true);
			break;
		/* for Digital Mic */
		case AUDIO_VOW_MIC_TYPE_Handset_DMIC:  /* DMIC */
		case AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K:
			VOW_DMIC_CLK_Enable(true);
			/* 0x0D1A MIC Bias 0 LowPower: 1_LPW, MISBIAS0 = 1P9V, Enable MICBIAS0 */
			Ana_Set_Reg(AUDENC_ANA_CON9,  0x00A1, 0x00F1);
			Ana_Set_Reg(AUDENC_ANA_CON8,  0x0005, 0x0007); /* 0xD18 Enable DMIC*/
			break;
		default:
			break;
		}

		VOW_GPIO_Enable(true);

		/* VOW AMPREF Setting, set by MD32 after DC calibration */
		Ana_Set_Reg(AFE_VOW_CFG0, reg_AFE_VOW_CFG0, 0xffff);   /* 0xffff */

		Ana_Set_Reg(AFE_VOW_CFG1, reg_AFE_VOW_CFG1, 0xffff);   /*VOW A,B timeout initial value 0x0200*/
#if 0
		if (MicType == AUDIO_VOW_MIC_TYPE_Handset_DMIC)
			Ana_Set_Reg(AFE_VOW_POSDIV_CFG0, 0x0C00, 0xffff);/* 1.6m */
		else if (MicType == AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K)
			Ana_Set_Reg(AFE_VOW_POSDIV_CFG0, 0x0C08, 0xffff);
#endif
		Ana_Set_Reg(AFE_VOW_CFG2, reg_AFE_VOW_CFG2, 0xffff);   /*VOW A,B value setting 0x2424*/
		Ana_Set_Reg(AFE_VOW_CFG3, reg_AFE_VOW_CFG3, 0xffff);   /*alhpa and beta K value setting 0xDBAC*/
		Ana_Set_Reg(AFE_VOW_CFG4, reg_AFE_VOW_CFG4, 0x000f);   /*gamma K value setting 0x029E*/
		Ana_Set_Reg(AFE_VOW_CFG5, reg_AFE_VOW_CFG5, 0xffff);   /*N mini value setting 0x0000*/

		/* 16K */
		Ana_Set_Reg(AFE_VOW_CFG4, 0x029E, 0xfff0); /* 16k */
		if (MicType == AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K)
			Ana_Set_Reg(AFE_VOW_POSDIV_CFG0, 0x0C08, 0xffff); /* 800k */
		else
			Ana_Set_Reg(AFE_VOW_POSDIV_CFG0, 0x0C00, 0xffff); /* 1.6m */

		TurnOnVOWPeriodicOnOff(MicType, reg_AFE_VOW_PERIODIC, true);

#ifndef VOW_STANDALONE_CONTROL
		if (MicType == AUDIO_VOW_MIC_TYPE_Handset_DMIC) {
			/*digital MIC need to config bit13 and bit6, (bit7 need to check)  0x6840*/

			VowDrv_SetDmicLowPower(false);

			Ana_Set_Reg(AFE_VOW_TOP, 0x20C0, 0x20C0);   /*VOW enable, with bit7*/
		} else if (MicType == AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K) {

			VowDrv_SetDmicLowPower(true);

			Ana_Set_Reg(AFE_VOW_TOP, 0x20C0, 0x20C0);   /*VOW enable, with bit7*/
		} else {
			/* Normal */
			VowDrv_SetDmicLowPower(false);
		}
#endif /* #ifndef VOW_STANDALONE_CONTROL */


		/*VOW enable, set AFE_VOW_TOP in VOW kernel driver*/
		/*need to inform VOW driver mic type*/
		VowDrv_EnableHW(true);
		pr_warn("%s, VowDrv_ChangeStatus set\n", __func__);
		VowDrv_ChangeStatus();

	} else { /* disable VOW */

		TurnOnVOWPeriodicOnOff(MicType, reg_AFE_VOW_PERIODIC, false);

		/*Set VOW driver disable, vow driver will do close all digital part setting*/
		VowDrv_EnableHW(false);
		pr_warn("%s, VowDrv_ChangeStatus set\n", __func__);
		VowDrv_ChangeStatus();
		msleep(20);

		VOW_GPIO_Enable(false);

		switch (MicType) {
		/* for ACC Mic */
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC:
		case AUDIO_VOW_MIC_TYPE_Headset_MIC:
			/*turn off analog part*/
			VOW_MIC_ACC_Enable(MicType, false);
			VOW_ACC_CLK_Enable(false);
			break;
		/* for DCC Mic */
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCC:
		case AUDIO_VOW_MIC_TYPE_Handset_AMIC_DCCECM:
		case AUDIO_VOW_MIC_TYPE_Headset_MIC_DCC:
		case AUDIO_VOW_MIC_TYPE_Headset_MIC_DCCECM:
			/*turn off analog part*/
			VOW_MIC_DCC_Enable(MicType, false);
			VOW_DCC_CLK_Enable(false);
			break;
		/* for Digital Mic */
		case AUDIO_VOW_MIC_TYPE_Handset_DMIC:
		case AUDIO_VOW_MIC_TYPE_Handset_DMIC_800K:
			Ana_Set_Reg(AUDENC_ANA_CON8,  0x0004, 0x0007); /*0x0D08*/
			Ana_Set_Reg(AUDENC_ANA_CON9,  0x0000, 0x00F1); /*0x0D1A*/
			VOW_DMIC_CLK_Enable(false);
			break;
		default:
			break;
		}

		VOW_Pwr_Enable(MicType, false);

		mIsVOWOn = false;
		SetVOWStatus(mIsVOWOn);
		GetMicbias = 0;
	}
#endif /* #ifdef CONFIG_MTK_VOW_SUPPORT */
	return true;
}


/* here start uplink power function */
static const char *const ADC_function[] = { "Off", "On" };
static const char *const ADC_power_mode[] = { "normal", "lowpower" };
static const char *const PreAmp_Mux_function[] = { "OPEN", "IN_ADC1", "IN_ADC2", "IN_ADC3" };

/* OPEN:0, IN_ADC1: 1, IN_ADC2:2, IN_ADC3:3 */
static const char *const ADC_UL_PGA_GAIN[] = { "0Db", "6Db", "12Db", "18Db", "24Db", "30Db" };
static const char *const Pmic_Digital_Mux[] = { "ADC1", "ADC2", "ADC3", "ADC4" };
static const char *const Adc_Input_Sel[] = { "idle", "AIN", "Preamp" };

static const char *const Audio_AnalogMic_Mode[] = {
	"ACCMODE", "DCCMODE", "DMIC", "DCCECMDIFFMODE", "DCCECMSINGLEMODE"
};
static const char *const Audio_VOW_ADC_Function[] = { "Off", "On" };
static const char *const Audio_VOW_Digital_Function[] = { "Off", "On" };

static const char *const Audio_VOW_MIC_Type[] = {
	"HandsetAMIC", "HeadsetMIC", "HandsetDMIC", "HandsetDMIC_800K",
	"HandsetAMIC_DCC",
	"HeadsetMIC_DCC", "HandsetAMIC_DCCECM", "HeadsetMIC_DCCECM"
};
/* here start uplink power function */
static const char * const Pmic_Test_function[] = { "Off", "On" };
static const char * const Pmic_LPBK_function[] = { "Off", "LPBK3" };

static const struct soc_enum Audio_UL_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(PreAmp_Mux_function),
			    PreAmp_Mux_function),
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
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode),
			    Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode),
			    Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode),
			    Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_AnalogMic_Mode),
			    Audio_AnalogMic_Mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_power_mode), ADC_power_mode),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_ADC_Function),
			    Audio_VOW_ADC_Function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(PreAmp_Mux_function),
			    PreAmp_Mux_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_Digital_Function),
			    Audio_VOW_Digital_Function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_MIC_Type),
			    Audio_VOW_MIC_Type),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
};

static int Audio_ADC1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Audio_ADC1_Get = %d\n",
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1];
	return 0;
}

static int Audio_ADC1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s()\n", __func__);
	mutex_lock(&Ana_Power_Mutex);
	if (ucontrol->value.integer.value[0]) {
		if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, true, 0);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, true, 1);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, true, 2);

		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
		    ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, false, 0);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, false, 1);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, false, 2);

	}
	mutex_unlock(&Ana_Power_Mutex);
	return 0;
}

static int Audio_ADC2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Audio_ADC2_Get = %d\n",
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2];
	return 0;
}

static int Audio_ADC2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s()\n", __func__);
	mutex_lock(&Ana_Power_Mutex);
	if (ucontrol->value.integer.value[0]) {
		if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2, true);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, true, 0);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2, true);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, true, 1);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, true, 2);

		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
		    ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, false, 0);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, false, 1);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, false, 2);

	}
	mutex_unlock(&Ana_Power_Mutex);
	return 0;
}

static int Audio_ADC3_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC4_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
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
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x00 << 13), 0x6000);	/* pinumx sel */
	else if (ucontrol->value.integer.value[0] == 1)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x01 << 13), 0x6000);	/* AIN0 */
	else if (ucontrol->value.integer.value[0] == 2)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x02 << 13), 0x6000);	/* Left preamp */
	else
		pr_warn("%s() [AudioWarn]\n ", __func__);

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
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x00 << 13), 0x6000);	/* pinumx sel */
	else if (ucontrol->value.integer.value[0] == 1)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x01 << 13), 0x6000);	/* AIN0 */
	else if (ucontrol->value.integer.value[0] == 2)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x02 << 13), 0x6000);	/* Right preamp */
	else
		pr_warn("%s() [AudioWarn]\n ", __func__);

	pr_warn("%s() done\n", __func__);
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2] = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_ADC3_Sel_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC3_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC4_Sel_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC4_Sel_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}


static bool AudioPreAmp1_Sel(int Mul_Sel)
{
	pr_aud("%s Mul_Sel = %d ", __func__, Mul_Sel);
	if (Mul_Sel == 0)
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0030);	/* pinumx open */
	else if (Mul_Sel == 1)
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0010, 0x0030);	/* AIN0 */
	else if (Mul_Sel == 2)
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0020, 0x0030);	/* AIN1 */
	else if (Mul_Sel == 3)
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x0030, 0x0030);	/* AIN2 */
	else
		pr_warn("[AudioWarn] AudioPreAmp1_Sel Mul_Sel=%d", Mul_Sel);

	return true;
}


static int Audio_PreAmp1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1];
	return 0;
}

static int Audio_PreAmp1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] =
	    ucontrol->value.integer.value[0];
	AudioPreAmp1_Sel(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);

	return 0;
}

static bool AudioPreAmp2_Sel(int Mul_Sel)
{
	pr_aud("%s Mul_Sel = %d ", __func__, Mul_Sel);

	if (Mul_Sel == 0)
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x0030);	/* pinumx open */
	else if (Mul_Sel == 1)
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0010, 0x0030);	/* AIN0 */
	else if (Mul_Sel == 2)
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0020, 0x0030);	/* AIN3 */
	else if (Mul_Sel == 3)
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0030, 0x0030);	/* AIN2 */
	else
		pr_warn("[AudioWarn] AudioPreAmp1_Sel, Mul_Sel=%d", Mul_Sel);

	return true;
}


static int Audio_PreAmp2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2];
	return 0;
}

static int Audio_PreAmp2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2] =
	    ucontrol->value.integer.value[0];
	AudioPreAmp2_Sel(mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2]);
	return 0;
}

/* PGA1: PGA_L */
static int Audio_PGA1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Audio_AmpR_Get = %d\n",
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
	return 0;
}

static int Audio_PGA1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_aud("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	Ana_Set_Reg(AUDENC_ANA_CON0, (index << 8), 0x0700);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] =
	    ucontrol->value.integer.value[0];
	return 0;
}

/* PGA2: PGA_R */
static int Audio_PGA2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Audio_PGA2_Get = %d\n",
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
	return 0;
}

static int Audio_PGA2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_aud("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	Ana_Set_Reg(AUDENC_ANA_CON1, index << 8, 0x0700);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_PGA3_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_PGA3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_PGA4_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_PGA4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_MicSource1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("Audio_MicSource1_Get = %d\n",
		mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]);
	ucontrol->value.integer.value[0] = mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1];
	return 0;
}

static int Audio_MicSource1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* ADC1 Mic source selection, "ADC1" is main_mic, "ADC2" is headset_mic */
	int index = 0;

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Digital_Mux)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	pr_aud("%s() index = %d done\n", __func__, index);
	mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] = ucontrol->value.integer.value[0];

	return 0;
}

static int Audio_MicSource2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_MicSource2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_MicSource3_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_MicSource3_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}


static int Audio_MicSource4_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_MicSource4_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

/* Mic ACC/DCC Mode Setting */
static int Audio_Mic1_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s() mAudio_Analog_Mic1_mode = %d\n", __func__, mAudio_Analog_Mic1_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic1_mode;
	return 0;
}

static int Audio_Mic1_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic1_mode = ucontrol->value.integer.value[0];
	pr_aud("%s() mAudio_Analog_Mic1_mode = %d\n", __func__, mAudio_Analog_Mic1_mode);
	return 0;
}

static int Audio_Mic2_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s()  = %d\n", __func__, mAudio_Analog_Mic2_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic2_mode;
	return 0;
}

static int Audio_Mic2_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic2_mode = ucontrol->value.integer.value[0];
	pr_aud("%s() mAudio_Analog_Mic2_mode = %d\n", __func__, mAudio_Analog_Mic2_mode);
	return 0;
}


static int Audio_Mic3_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s()  = %d\n", __func__, mAudio_Analog_Mic3_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic3_mode;
	return 0;
}

static int Audio_Mic3_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic3_mode = ucontrol->value.integer.value[0];
	pr_aud("%s() mAudio_Analog_Mic3_mode = %d\n", __func__, mAudio_Analog_Mic3_mode);
	return 0;
}

static int Audio_Mic4_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_aud("%s()  = %d\n", __func__, mAudio_Analog_Mic4_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic4_mode;
	return 0;
}

static int Audio_Mic4_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic4_mode = ucontrol->value.integer.value[0];
	pr_aud("%s() mAudio_Analog_Mic4_mode = %d\n", __func__, mAudio_Analog_Mic4_mode);
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
		pr_err("return -EINVAL\n");
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
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		TurnOnVOWADcPower(mAudio_VOW_Mic_type, true);
	else
		TurnOnVOWADcPower(mAudio_VOW_Mic_type, false);


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
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		TurnOnVOWDigitalHW(true);
	else
		TurnOnVOWDigitalHW(false);


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
		pr_err("return -EINVAL\n");
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
	/* pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]); */
	/* reg_AFE_VOW_CFG5 = ucontrol->value.integer.value[0]; */
	return 0;
}

static int Audio_Vow_Periodic_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int value = /*Ana_Get_Reg(AFE_VOW_CFG5) */ reg_AFE_VOW_PERIODIC;

	pr_warn("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Periodic_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	/* Ana_Set_Reg(AFE_VOW_CFG5, ucontrol->value.integer.value[0], 0xffff); */
	reg_AFE_VOW_PERIODIC = ucontrol->value.integer.value[0];
	return 0;
}

static bool ul_lr_swap_enable;

static int Audio_UL_LR_Swap_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ul_lr_swap_enable;
	return 0;
}

static int Audio_UL_LR_Swap_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	ul_lr_swap_enable = ucontrol->value.integer.value[0];
	Ana_Set_Reg(AFE_UL_DL_CON0, ul_lr_swap_enable << 15, 0x1 << 15);
	return 0;
}

static bool SineTable_DAC_HP_flag;
static bool SineTable_UL2_flag;

static int SineTable_UL2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0]) {
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0002, 0x2);	/* set UL from sinetable */
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0080, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
	} else {
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0x2);	/* set UL from normal path */
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0000, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
	}
	SineTable_UL2_flag = ucontrol->value.integer.value[0];
	return 0;
}

static int SineTable_UL2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = SineTable_UL2_flag;
	return 0;
}

static int32 Pmic_Loopback_Type;

static int Pmic_Loopback_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = Pmic_Loopback_Type;
	return 0;
}

static int Pmic_Loopback_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_LPBK_function)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0) { /* disable pmic lpbk */
		Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0x0005); /* power off uplink */
		Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x0000, 0x0001); /* turn off mute function and turn on dl */
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);   /* turn off afe UL & DL */
		Topck_Enable(false);
		ClsqEnable(false);
		audckbufEnable(false);
	} else if (ucontrol->value.integer.value[0] > 0) { /* enable pmic lpbk */
		audckbufEnable(true);
		ClsqEnable(true);
		Topck_Enable(true);

		pr_warn("set PMIC LPBK3, DLSR=%d, ULSR=%d\n",
			mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC],
			mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]);

		Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);	/* power on clock */

		/* Se DL Part */
		Ana_Set_Reg(AFE_DL_SDM_CON1, 0x001e, 0xffffffff);	/* set attenuation gain */
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0xffffffff);	/* [0] afe enable */

		Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0, 0x7330, 0xffffffff);
		Ana_Set_Reg(AFE_DL_SRC2_CON0_H, 0x7300, 0xffffffff);	/* set DL sample rate */

		/* set DL sample rate */
		Ana_Set_Reg(AFE_PMIC_NEWIF_CFG0,
			GetDLNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) << 12 | 0x330,
			0xffff);
		Ana_Set_Reg(AFE_DL_SRC2_CON0_H,
			GetDLNewIFFrequency(mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]) << 12 | 0x300,
			0xffff);

		Ana_Set_Reg(AFE_PMIC_NEWIF_CFG2, 0x1 << 15, 0x1 << 15);
		/* pmic rxif sck inverse */

		Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x0001, 0x0001);	/* turn off mute function and turn on dl */
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0x0001);	/* set DL in normal path, not from sine gen table */

		/* Set UL Part */
		/* set UL sample rate */
		Ana_Set_Reg(AFE_UL_SRC_CON0_H,
			(ULSampleRateTransform(mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]) << 1),
			0x000E);
		Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0005, 0xffff);   /* power on uplink, and loopback from DL */
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0x0002);       /* configure ADC setting */
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0xffff);   /* turn on afe */
	}

	pr_warn("%s() done\n", __func__);
	Pmic_Loopback_Type = ucontrol->value.integer.value[0];
	return 0;
}

static int SineTable_DAC_HP_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = SineTable_DAC_HP_flag;
	return 0;
}

static int SineTable_DAC_HP_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 TODO? */
	pr_warn("%s()\n", __func__);
	return 0;
}

static void ADC_LOOP_DAC_Func(int command)
{
	/* 6752 TODO? */
}

static bool DAC_LOOP_DAC_HS_flag;
static int ADC_LOOP_DAC_HS_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = DAC_LOOP_DAC_HS_flag;
	return 0;
}

static int ADC_LOOP_DAC_HS_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
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
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = DAC_LOOP_DAC_HP_flag;
	return 0;
}

static int ADC_LOOP_DAC_HP_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{

	pr_warn("%s()\n", __func__);
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
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = Voice_Call_DAC_DAC_HS_flag;
	return 0;
}

static int Voice_Call_DAC_DAC_HS_Set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 TODO */
	pr_warn("%s()\n", __func__);
	return 0;
}

static const struct soc_enum Pmic_Test_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_Test_function), Pmic_Test_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Pmic_LPBK_function), Pmic_LPBK_function),
};

static const struct snd_kcontrol_new mt6331_pmic_Test_controls[] = {
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
};

static const struct snd_kcontrol_new mt6331_UL_Codec_controls[] = {
	SOC_ENUM_EXT("Audio_ADC_1_Switch", Audio_UL_Enum[0], Audio_ADC1_Get,
		     Audio_ADC1_Set),
	SOC_ENUM_EXT("Audio_ADC_2_Switch", Audio_UL_Enum[1], Audio_ADC2_Get,
		     Audio_ADC2_Set),
	SOC_ENUM_EXT("Audio_ADC_3_Switch", Audio_UL_Enum[2], Audio_ADC3_Get,
		     Audio_ADC3_Set),
	SOC_ENUM_EXT("Audio_ADC_4_Switch", Audio_UL_Enum[3], Audio_ADC4_Get,
		     Audio_ADC4_Set),
	SOC_ENUM_EXT("Audio_Preamp1_Switch", Audio_UL_Enum[4],
		     Audio_PreAmp1_Get,
		     Audio_PreAmp1_Set),
	SOC_ENUM_EXT("Audio_ADC_1_Sel", Audio_UL_Enum[5],
		     Audio_ADC1_Sel_Get, Audio_ADC1_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_2_Sel", Audio_UL_Enum[6],
		     Audio_ADC2_Sel_Get, Audio_ADC2_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_3_Sel", Audio_UL_Enum[7],
		     Audio_ADC3_Sel_Get, Audio_ADC3_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_4_Sel", Audio_UL_Enum[8],
		     Audio_ADC4_Sel_Get, Audio_ADC4_Sel_Set),
	SOC_ENUM_EXT("Audio_PGA1_Setting", Audio_UL_Enum[9], Audio_PGA1_Get,
		     Audio_PGA1_Set),
	SOC_ENUM_EXT("Audio_PGA2_Setting", Audio_UL_Enum[10],
		     Audio_PGA2_Get, Audio_PGA2_Set),
	SOC_ENUM_EXT("Audio_PGA3_Setting", Audio_UL_Enum[11],
		     Audio_PGA3_Get, Audio_PGA3_Set),
	SOC_ENUM_EXT("Audio_PGA4_Setting", Audio_UL_Enum[12],
		     Audio_PGA4_Get, Audio_PGA4_Set),
	SOC_ENUM_EXT("Audio_MicSource1_Setting", Audio_UL_Enum[13],
		     Audio_MicSource1_Get,
		     Audio_MicSource1_Set),
	SOC_ENUM_EXT("Audio_MicSource2_Setting", Audio_UL_Enum[14],
		     Audio_MicSource2_Get,
		     Audio_MicSource2_Set),
	SOC_ENUM_EXT("Audio_MicSource3_Setting", Audio_UL_Enum[15],
		     Audio_MicSource3_Get,
		     Audio_MicSource3_Set),
	SOC_ENUM_EXT("Audio_MicSource4_Setting", Audio_UL_Enum[16],
		     Audio_MicSource4_Get,
		     Audio_MicSource4_Set),
	SOC_ENUM_EXT("Audio_MIC1_Mode_Select", Audio_UL_Enum[17],
		     Audio_Mic1_Mode_Select_Get,
		     Audio_Mic1_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC2_Mode_Select", Audio_UL_Enum[18],
		     Audio_Mic2_Mode_Select_Get,
		     Audio_Mic2_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC3_Mode_Select", Audio_UL_Enum[19],
		     Audio_Mic3_Mode_Select_Get,
		     Audio_Mic3_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC4_Mode_Select", Audio_UL_Enum[20],
		     Audio_Mic4_Mode_Select_Get,
		     Audio_Mic4_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_Mic_Power_Mode", Audio_UL_Enum[21],
		     Audio_Adc_Power_Mode_Get,
		     Audio_Adc_Power_Mode_Set),
	SOC_ENUM_EXT("Audio_Vow_ADC_Func_Switch", Audio_UL_Enum[22],
		     Audio_Vow_ADC_Func_Switch_Get,
		     Audio_Vow_ADC_Func_Switch_Set),
	SOC_ENUM_EXT("Audio_Preamp2_Switch", Audio_UL_Enum[23],
		     Audio_PreAmp2_Get,
		     Audio_PreAmp2_Set),
	SOC_ENUM_EXT("Audio_Vow_Digital_Func_Switch", Audio_UL_Enum[24],
		     Audio_Vow_Digital_Func_Switch_Get,
		     Audio_Vow_Digital_Func_Switch_Set),
	SOC_ENUM_EXT("Audio_Vow_MIC_Type_Select", Audio_UL_Enum[25],
		     Audio_Vow_MIC_Type_Select_Get,
		     Audio_Vow_MIC_Type_Select_Set),
	SOC_SINGLE_EXT("Audio VOWCFG0 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg0_Get,
		       Audio_Vow_Cfg0_Set),
	SOC_SINGLE_EXT("Audio VOWCFG1 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg1_Get,
		       Audio_Vow_Cfg1_Set),
	SOC_SINGLE_EXT("Audio VOWCFG2 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg2_Get,
		       Audio_Vow_Cfg2_Set),
	SOC_SINGLE_EXT("Audio VOWCFG3 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg3_Get,
		       Audio_Vow_Cfg3_Set),
	SOC_SINGLE_EXT("Audio VOWCFG4 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg4_Get,
		       Audio_Vow_Cfg4_Set),
	SOC_SINGLE_EXT("Audio VOWCFG5 Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Cfg5_Get,
		       Audio_Vow_Cfg5_Set),
	SOC_SINGLE_EXT("Audio_VOW_State", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_State_Get,
		       Audio_Vow_State_Set),
	SOC_SINGLE_EXT("Audio_VOW_Periodic", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_Vow_Periodic_Get,
		       Audio_Vow_Periodic_Set),
	SOC_ENUM_EXT("Audio_UL_LR_Swap", Audio_UL_Enum[26],
		     Audio_UL_LR_Swap_Get,
		     Audio_UL_LR_Swap_Set),
};

static const struct snd_soc_dapm_widget mt6331_dapm_widgets[] = {
	/* Outputs */
	SND_SOC_DAPM_OUTPUT("EARPIECE"),
	SND_SOC_DAPM_OUTPUT("HEADSET"),
	SND_SOC_DAPM_OUTPUT("SPEAKER"),
	/*
	   SND_SOC_DAPM_MUX_E("VOICE_Mux_E", SND_SOC_NOPM, 0, 0  , &mt6331_Voice_Switch, codec_enable_rx_bias,
	   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
	   SND_SOC_DAPM_PRE_REG | SND_SOC_DAPM_POST_REG),
	 */

};

static const struct snd_soc_dapm_route mtk_audio_map[] = {
	{"VOICE_Mux_E", "Voice Mux", "SPEAKER PGA"},
};

static void mt6331_codec_init_reg(struct snd_soc_codec *codec)
{
	pr_warn("%s\n", __func__);

	audckbufEnable(true);
	Ana_Set_Reg(TOP_CLKSQ, 0x0, 0x0001);
	/* Disable CLKSQ 26MHz */
	Ana_Set_Reg(AUDDEC_ANA_CON9, 0x1000, 0x1000);
	/* disable AUDGLB */
	Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0x3800, 0x3800);
	/* Turn off AUDNCP_CLKDIV engine clock,Turn off AUD 26M */
	Ana_Set_Reg(AUDDEC_ANA_CON0, 0xe000, 0xe000);
	/* Disable HeadphoneL/HeadphoneR/voice short circuit protection */
	/* Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x0010); */
	/* power off mic bias1 */
	Ana_Set_Reg(AUDDEC_ANA_CON3, 0x4228, 0xffff);
	/* [5] = 1, disable LO buffer left short circuit protection */
	Ana_Set_Reg(DRV_CON2, 0xe << 4, 0xf << 4);
	/* PAD_AUD_DAT_MISO gpio driving MAX */
	audckbufEnable(false);
}

void InitCodecDefault(void)
{
	pr_warn("%s\n", __func__);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP3] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP4] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = 8;
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

static void InitGlobalVarDefault(void)
{
	mCodec_data = NULL;
	mAudio_Vow_Analog_Func_Enable = false;
	mAudio_Vow_Digital_Func_Enable = false;
	mIsVOWOn = false;
	mAdc_Power_Mode = 0;
	mInitCodec = false;
	mAnaSuspend = false;
	audck_buf_Count = 0;
	ClsqCount = 0;
	TopCkCount = 0;
	NvRegCount = 0;

#ifdef CONFIG_MTK_SPEAKER
	mSpeaker_Ocflag = false;
#endif

}

static int mt6331_codec_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	pr_warn("%s()\n", __func__);

	if (mInitCodec == true)
		return 0;

	ANC_enabled = 0;

#ifdef MT6755_AW8736_REWORK
	int data[4];
	int rawdata;
	int ret;
#endif

	pin_extspkamp = pin_extspkamp_2 = pin_vowclk = pin_audmiso = pin_rcvspkswitch = 0;
	pin_mode_extspkamp = pin_mode_extspkamp_2 = pin_mode_vowclk =
	    pin_mode_audmiso = pin_mode_rcvspkswitch = 0;

	snd_soc_dapm_new_controls(dapm, mt6331_dapm_widgets, ARRAY_SIZE(mt6331_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, mtk_audio_map, ARRAY_SIZE(mtk_audio_map));

	/* add codec controls */
	snd_soc_add_codec_controls(codec, mt6331_snd_controls, ARRAY_SIZE(mt6331_snd_controls));
	snd_soc_add_codec_controls(codec, mt6331_UL_Codec_controls,
				   ARRAY_SIZE(mt6331_UL_Codec_controls));
	snd_soc_add_codec_controls(codec, mt6331_Voice_Switch, ARRAY_SIZE(mt6331_Voice_Switch));
	snd_soc_add_codec_controls(codec, mt6331_pmic_Test_controls,
				   ARRAY_SIZE(mt6331_pmic_Test_controls));

#ifdef CONFIG_MTK_SPEAKER
	snd_soc_add_codec_controls(codec, mt6331_snd_Speaker_controls,
				   ARRAY_SIZE(mt6331_snd_Speaker_controls));
#endif

	snd_soc_add_codec_controls(codec, Audio_snd_auxadc_controls,
				   ARRAY_SIZE(Audio_snd_auxadc_controls));

	/* here to set  private data */
	mCodec_data = kzalloc(sizeof(mt6331_Codec_Data_Priv), GFP_KERNEL);
	if (!mCodec_data) {
		/*pr_warn("Failed to allocate private data\n");*/
		return -ENOMEM;
	}
	snd_soc_codec_set_drvdata(codec, mCodec_data);

	memset((void *)mCodec_data, 0, sizeof(mt6331_Codec_Data_Priv));
	mt6331_codec_init_reg(codec);
	InitCodecDefault();
	mInitCodec = true;

	return 0;
}

static int mt6331_codec_remove(struct snd_soc_codec *codec)
{
	pr_warn("%s()\n", __func__);
	return 0;
}

static unsigned int mt6331_read(struct snd_soc_codec *codec, unsigned int reg)
{
	pr_warn("mt6331_read reg = 0x%x", reg);
	Ana_Get_Reg(reg);
	return 0;
}

static int mt6331_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value)
{
	pr_warn("mt6331_write reg = 0x%x  value= 0x%x\n", reg, value);
	Ana_Set_Reg(reg, value, 0xffffffff);
	return 0;
}

static struct snd_soc_codec_driver soc_mtk_codec = {
	.probe = mt6331_codec_probe,
	.remove = mt6331_codec_remove,

	.read = mt6331_read,
	.write = mt6331_write,

	/* use add control to replace */
	/* .controls = mt6331_snd_controls, */
	/* .num_controls = ARRAY_SIZE(mt6331_snd_controls), */

	.dapm_widgets = mt6331_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6331_dapm_widgets),
	.dapm_routes = mtk_audio_map,
	.num_dapm_routes = ARRAY_SIZE(mtk_audio_map),

};

static int mtk_mt6331_codec_dev_probe(struct platform_device *pdev)
{
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (pdev->dev.dma_mask == NULL)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;


	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", MT_SOC_CODEC_NAME);

		/* check if use hp depop flow */
		of_property_read_u32(pdev->dev.of_node,
				     "use_hp_depop_flow",
				     &mUseHpDepopFlow);
		pr_warn("%s(), use_hp_depop_flow = %d\n",
			__func__,
			mUseHpDepopFlow);
	} else {
		pr_warn("%s(), pdev->dev.of_node = NULL!!!\n", __func__);
	}

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_codec(&pdev->dev,
				      &soc_mtk_codec, mtk_6331_dai_codecs,
				      ARRAY_SIZE(mtk_6331_dai_codecs));
}

static int mtk_mt6331_codec_dev_remove(struct platform_device *pdev)
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
#endif

static struct platform_driver mtk_codec_6331_driver = {
	.driver = {
		   .name = MT_SOC_CODEC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_codec_63xx_of_ids,
#endif
		   },
	.probe = mtk_mt6331_codec_dev_probe,
	.remove = mtk_mt6331_codec_dev_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_codec6331_dev;
#endif

static int __init mtk_mt6331_codec_init(void)
{
	pr_warn("%s:\n", __func__);
#ifndef CONFIG_OF
	int ret = 0;

	soc_mtk_codec6331_dev = platform_device_alloc(MT_SOC_CODEC_NAME, -1);

	if (!soc_mtk_codec6331_dev)
		return -ENOMEM;


	ret = platform_device_add(soc_mtk_codec6331_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_codec6331_dev);
		return ret;
	}
#endif
	InitGlobalVarDefault();

	return platform_driver_register(&mtk_codec_6331_driver);
}

module_init(mtk_mt6331_codec_init);

static void __exit mtk_mt6331_codec_exit(void)
{
	pr_warn("%s:\n", __func__);

	platform_driver_unregister(&mtk_codec_6331_driver);
}

module_exit(mtk_mt6331_codec_exit);

/* Module information */
MODULE_DESCRIPTION("MTK  codec driver");
MODULE_LICENSE("GPL v2");
