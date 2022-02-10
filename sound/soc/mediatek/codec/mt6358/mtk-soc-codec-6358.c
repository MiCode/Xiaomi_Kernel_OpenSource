/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
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
 ******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/types.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

#ifdef CONFIG_MTK_ACCDET
#include "accdet.h"
#endif

#include <mt-plat/mtk_auxadc_intf.h>

#include "mtk-auddrv-def.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-gpio.h"

#include "mtk-soc-analog-type.h"
#include "mtk-soc-codec-63xx.h"
#ifdef CONFIG_MTK_VOW_SUPPORT
#include <mach/vow_api.h>
#endif

#include <linux/of_gpio.h>
#include <linux/gpio.h>
#ifdef CONFIG_MT8183_QUERY_PCB_ID_METHOD
#include<mtk_auxadc.h>
#endif


#define ANALOG_HPTRIM

/* HP IMPEDANCE Current Calibration from EFUSE */
/* #define EFUSE_HP_IMPEDANCE */

/* static function declaration */
static bool AudioPreAmp1_Sel(int Mul_Sel);
static bool GetAdcStatus(void);
static bool TurnOnVOWDigitalHW(bool enable);
static void TurnOffDacPower(void);
static void TurnOnDacPower(int device);
static void setDlMtkifSrc(bool enable);
#ifndef ANALOG_HPTRIM
static int SetDcCompenSation(bool enable);
#endif
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
static void VOW13MCK_Enable(bool enable);
static void VOW32KCK_Enable(bool enable);
#endif

static struct mt6358_codec_priv *mCodec_data;
static unsigned int mBlockSampleRate[ANA_DEV_IN_OUT_MAX] = {
	48000, 48000, 48000
};

#define MAX_DL_SAMPLE_RATE (192000)
#define MAX_UL_SAMPLE_RATE (192000)

static DEFINE_MUTEX(Ana_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_buf_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_Clk_Mutex);
static DEFINE_MUTEX(Ana_Power_Mutex);
static DEFINE_MUTEX(AudAna_lock);

static int mAudio_Analog_Mic1_mode = ANA_UL_MODE_ACC;
static int mAudio_Analog_Mic2_mode = ANA_UL_MODE_ACC;
static int mAudio_Analog_Mic3_mode = ANA_UL_MODE_ACC;
static int mAudio_Analog_Mic4_mode = ANA_UL_MODE_ACC;

static int mAudio_Vow_Analog_Func_Enable;
static int mAudio_Vow_Digital_Func_Enable;

enum {
	AUXADC_AVG_1,
	AUXADC_AVG_4,
	AUXADC_AVG_8,
	AUXADC_AVG_16,
	AUXADC_AVG_32,
	AUXADC_AVG_64,
	AUXADC_AVG_128,
	AUXADC_AVG_256,
};

enum {
	MIC_BIAS_1p7 = 0,
	MIC_BIAS_1p8,
	MIC_BIAS_1p9,
	MIC_BIAS_2p0,
	MIC_BIAS_2p1,
	MIC_BIAS_2p5,
	MIC_BIAS_2p6,
	MIC_BIAS_2p7,
};

enum {
	DL_GAIN_8DB = 0,
	DL_GAIN_0DB = 8,
	DL_GAIN_N_1DB = 9,
	DL_GAIN_N_10DB = 18,
	DL_GAIN_N_40DB = 0x1f,
};
#define DL_GAIN_N_10DB_REG (DL_GAIN_N_10DB << 7 | DL_GAIN_N_10DB)
#define DL_GAIN_N_40DB_REG (DL_GAIN_N_40DB << 7 | DL_GAIN_N_40DB)
#define DL_GAIN_REG_MASK 0x0f9f

enum hp_depop_flow {
	HP_DEPOP_FLOW_DEPOP_HW,
	HP_DEPOP_FLOW_33OHM,
	HP_DEPOP_FLOW_DEPOP_HW_33OHM,
	HP_DEPOP_FLOW_NONE,
};
static unsigned int mUseHpDepopFlow;

enum DBG_TYPE {
	DBG_DCTRIM_BYPASS_4POLE = 0x1 << 0,
	DBG_DCTRIM_4POLE_LOG = 0x1 << 1,
};

enum DBG_TYPE codec_debug_enable;

static int low_power_mode;

#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef EFUSE_HP_TRIM
static unsigned int RG_AUDHPLTRIM_VAUDP15, RG_AUDHPRTRIM_VAUDP15,
	RG_AUDHPLFINETRIM_VAUDP15,
	RG_AUDHPRFINETRIM_VAUDP15, RG_AUDHPLTRIM_VAUDP15_SPKHP,
	RG_AUDHPRTRIM_VAUDP15_SPKHP,
	RG_AUDHPLFINETRIM_VAUDP15_SPKHP, RG_AUDHPRFINETRIM_VAUDP15_SPKHP;
#endif
#endif

static int mAdc_Power_Mode;
static int apply_n12db_gain;
static unsigned int dAuxAdcChannel = 16;
static const int mDcOffsetTrimChannel = 9;
static bool mInitCodec;
static bool mIsNeedPullDown = true;

int (*enable_dc_compensation)(bool enable) = NULL;
int (*set_lch_dc_compensation)(int value) = NULL;
int (*set_rch_dc_compensation)(int value) = NULL;
int (*set_ap_dmic)(bool enable) = NULL;
int (*set_hp_impedance_ctl)(bool enable) = NULL;
#ifdef CONFIG_MTK_VOW_SUPPORT
static unsigned int MicbiasRef, GetMicbias;
#endif /* #ifdef CONFIG_MTK_VOW_SUPPORT */

static int reg_AFE_VOW_CFG0; /* VOW AMPREF Setting */
static int reg_AFE_VOW_CFG1; /* VOW A,B timeout initial value (timer) */
static int reg_AFE_VOW_CFG2 = 0x2222; /* VOW A,B value setting (BABA) */
/* alhpa and beta K value setting (beta_rise,fall,alpha_rise,fall) */
static int reg_AFE_VOW_CFG3 = 0x8767;
/* gamma K value setting (gamma), bit4:8 should not modify */
static int reg_AFE_VOW_CFG4 = 0x006E;
static int reg_AFE_VOW_CFG5 = 0x0001; /* N mini value setting (Nmin) */
static int reg_AFE_VOW_PERIODIC; /* Periodic On/Off setting (On percent)*/
static bool mIsVOWOn;

/* VOW using */
enum audio_vow_mic_type {
	VOW_MIC_TYPE_Handset_AMIC = 0,
	VOW_MIC_TYPE_Headset_MIC,
	VOW_MIC_TYPE_Handset_DMIC, /* 1P6 */
	VOW_MIC_TYPE_Handset_DMIC_800K, /* 800K */
	VOW_MIC_TYPE_Handset_AMIC_DCC, /* DCC mems */
	VOW_MIC_TYPE_Headset_MIC_DCC,
	VOW_MIC_TYPE_Handset_AMIC_DCCECM, /* DCC ECM, dual differential */
	VOW_MIC_TYPE_Headset_MIC_DCCECM,/* DCC ECM, signal differential */
	VOW_MIC_TYPE_Handset_DMIC_VENDOR01, /* DMIC Vendor01 */
	VOW_MIC_TYPE_NUM
};

/* Jogi: Need? @{ */
#define SND_SOC_ADV_MT_FMTS (\
				SNDRV_PCM_FMTBIT_S16_LE |\
				SNDRV_PCM_FMTBIT_S16_BE |\
				SNDRV_PCM_FMTBIT_U16_LE |\
				SNDRV_PCM_FMTBIT_U16_BE |\
				SNDRV_PCM_FMTBIT_S24_LE |\
				SNDRV_PCM_FMTBIT_S24_BE |\
				SNDRV_PCM_FMTBIT_U24_LE |\
				SNDRV_PCM_FMTBIT_U24_BE |\
				SNDRV_PCM_FMTBIT_S32_LE |\
				SNDRV_PCM_FMTBIT_S32_BE |\
				SNDRV_PCM_FMTBIT_U32_LE |\
				SNDRV_PCM_FMTBIT_U32_BE)

#define SND_SOC_STD_MT_FMTS (\
				SNDRV_PCM_FMTBIT_S16_LE |\
				SNDRV_PCM_FMTBIT_S16_BE |\
				SNDRV_PCM_FMTBIT_U16_LE |\
				SNDRV_PCM_FMTBIT_U16_BE)
/* @} Build pass: */
#define SOC_HIGH_USE_RATE	(SNDRV_PCM_RATE_CONTINUOUS |\
				 SNDRV_PCM_RATE_8000_192000)
#ifdef CONFIG_MTK_VOW_SUPPORT

/* VOW_MIC_TYPE_Handset_AMIC_DCC */
/* VOW_MIC_TYPE_Handset_AMIC_DCCECM */
static const unsigned short Handset_AMIC_DCC_PeriodicOnOff[7][22] = {
	{
		0x8000, 0x8000, 0x81AA, 0x0000, 0x0000, 0x0000,
			 0x0000, 0x8000, 0x0000, 0x81EC, 0x0000,
		0x1917, 0x8021, 0x1917, 0x0000, 0x0000, 0x0000,
			 0x0000, 0x1917, 0x0000, 0x18F6, 0x0000
	},/* 90% */
	{
		0x828F, 0x828F, 0x8439, 0x0000, 0x0000, 0x0000,
			0x0000, 0x828F, 0x0000, 0x847B, 0x0000,
		0x1917, 0x82B0, 0x1917, 0x0000, 0x0000, 0x0000,
		0x0000, 0x1917, 0x0000, 0x18F6, 0x0000
	},/* 80% */
	{
		0x851F, 0x851F, 0x86C9, 0x0000, 0x0000, 0x0000,
			0x0000, 0x851F, 0x0000, 0x870A, 0x0000,
		0x1917, 0x853F, 0x1917, 0x0000, 0x0000, 0x0000,
		0x0000, 0x1917, 0x0000, 0x18F6, 0x0000
	},/* 70% */
	{
		0x87AE, 0x87AE, 0x8958, 0x0000, 0x0000, 0x80A4,
			0x0000, 0x87AE, 0xC0A4, 0x899A, 0x0000,
		0x1917, 0x87CF, 0x1917, 0x0000, 0x0000, 0x1917,
		0x0000, 0x1917, 0x1917, 0x18F6, 0x0000
	},/* 60% */
	{
		0x8A3D, 0x8A3D, 0x8BE7, 0x0000, 0x0000, 0x8333,
			0x0000, 0x8A3D, 0xC333, 0x8C29, 0x0000,
		0x1917, 0x8A5E, 0x1917, 0x0000, 0x0000, 0x1917,
		0x0000, 0x1917, 0x1917, 0x18F6, 0x0000
	},/* 50% */
	{
		0x8CCD, 0x8CCD, 0x8E77, 0x0000, 0x0000, 0x85C3,
			0x0000, 0x8CCD, 0xC5C3, 0x8EB8, 0x0000,
		0x1917, 0x8CEE, 0x1917, 0x0000, 0x0000, 0x1917,
		0x0000, 0x1917, 0x1917, 0x18F6, 0x0000
	},/* 40% */
	{
		0x8F5C, 0x8F5C, 0x9106, 0x0000, 0x0000, 0x8852,
			0x0000, 0x8F5C, 0xC852, 0x9148, 0x0000,
		0x1917, 0x8F7D, 0x1917, 0x0000, 0x0000, 0x1917,
		0x0000, 0x1917, 0x1917, 0x18F6, 0x0000
	} /* 30% */
};

/* VOW_MIC_TYPE_Headset_MIC_DCC */
/* VOW_MIC_TYPE_Headset_MIC_DCCECM */
static const unsigned short Headset_MIC_DCC_PeriodicOnOff[7][22] = {
	{
		0x8000, 0x8000, 0x81AA, 0x0000, 0x0000, 0x0000,
			0x0000, 0x8000, 0xC000, 0x81EC, 0x0000,
		0x1917, 0x8021, 0x1917, 0x0000, 0x0000, 0x0000,
		0x0000, 0x1917, 0x1917, 0x18F6, 0x0000
	},/* 90% */
	{
		0x8148, 0x8148, 0x82F2, 0x0000, 0x0000, 0x0000,
			0x80A4, 0x8148, 0xC0A4, 0x8333, 0x0000,
		0x17CF, 0x8168, 0x17CF, 0x0000, 0x0000, 0x0000,
		0x17CF, 0x17CF, 0x17CF, 0x17AE, 0x0000
	},/* 80% */
	{
		0x828F, 0x828F, 0x8439, 0x0000, 0x0000, 0x0000,
			0x81EC, 0x828F, 0xC1EC, 0x847B, 0x0000,
		0x1687, 0x82B0, 0x1687, 0x0000, 0x0000, 0x0000,
		0x1687, 0x1687, 0x1687, 0x1666, 0x0000
	},/* 70% */
	{
		0x83D7, 0x83D7, 0x8581, 0x0000, 0x0000, 0x0000,
			0x8333, 0x83D7, 0xC333, 0x85C3, 0x0000,
		0x153F, 0x83F8, 0x153F, 0x0000, 0x0000, 0x0000,
		0x153F, 0x153F, 0x153F, 0x151F, 0x0000
	},/* 60% */
	{
		0x851F, 0x851F, 0x86C9, 0x0000, 0x0000, 0x0000,
			0x847B, 0x851F, 0xC47B, 0x870A, 0x0000,
		0x13F8, 0x853F, 0x13F8, 0x0000, 0x0000, 0x0000,
		0x13F8, 0x13F8, 0x13F8, 0x13D7, 0x0000
	},/* 50% */
	{
		0x8666, 0x8666, 0x8810, 0x0000, 0x0000, 0x0000,
			0x85C3, 0x8666, 0xC5C3, 0x8852, 0x0000,
		0x12B0, 0x8687, 0x12B0, 0x0000, 0x0000, 0x0000,
		0x12B0, 0x12B0, 0x12B0, 0x128F, 0x0000
	},/* 40% */
	{
		0x87AE, 0x87AE, 0x8958, 0x0000, 0x0000, 0x0000,
			0x870A, 0x87AE, 0xC70A, 0x899A, 0x0000,
		0x1168, 0x87CF, 0x1168, 0x0000, 0x0000, 0x0000,
		0x1168, 0x1168, 0x1168, 0x1148, 0x0000
	} /* 30% */
};

#endif
static int mAudio_VOW_Mic_type = VOW_MIC_TYPE_Handset_AMIC;

static void Audio_Amp_Change(int channels, bool enable);
static void SavePowerState(void)
{
	int i = 0;

	for (i = 0; i < ANA_DEV_MAX; i++) {
		mCodec_data->backup_dev_power[i] =
			mCodec_data->dev_power[i];
	}
}

static void RestorePowerState(void)
{
	int i = 0;

	for (i = 0; i < ANA_DEV_MAX; i++) {
		mCodec_data->dev_power[i] =
			mCodec_data->backup_dev_power[i];
	}
}

static bool GetDLStatus(void)
{
	int i = 0;

	for (i = 0; i < ANA_DEV_2IN1_SPK; i++) {
		if (mCodec_data->dev_power[i] == true)
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
		/*Ana_Log_Print();*/
		SavePowerState();
		if (mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL] ==
		    true) {
			mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL] =
				false;
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
		}
		if (mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR] ==
		    true) {
			mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR] =
				false;
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
		}
		if (mCodec_data->dev_power[ANA_DEV_OUT_EARPIECEL] ==
		    true) {
			mCodec_data->dev_power[ANA_DEV_OUT_EARPIECEL] =
				false;
			Voice_Amp_Change(false);
		}
		if (mCodec_data->dev_power[ANA_DEV_OUT_SPEAKERL] ==
		    true) {
			mCodec_data->dev_power[ANA_DEV_OUT_SPEAKERL] =
				false;
			Speaker_Amp_Change(false);
		}
		/*Ana_Log_Print();*/
		mAnaSuspend = true;
	} else if ((bEnable == false) && (mAnaSuspend == true)) {
		/*Ana_Log_Print();*/
		if (mCodec_data->backup_dev_power[ANA_DEV_OUT_HEADSETL]
		    ==
		    true) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
			mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL] =
				true;
		}
		if (mCodec_data->backup_dev_power[ANA_DEV_OUT_HEADSETR]
		    ==
		    true) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
			mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR] =
				false;
		}
		if (mCodec_data->backup_dev_power[ANA_DEV_OUT_EARPIECEL]
		    ==
		    true) {
			Voice_Amp_Change(true);
			mCodec_data->dev_power[ANA_DEV_OUT_EARPIECEL] =
				false;
		}
		if (mCodec_data->backup_dev_power[ANA_DEV_OUT_SPEAKERL]
		    ==
		    true) {
			Speaker_Amp_Change(true);
			mCodec_data->dev_power[ANA_DEV_OUT_SPEAKERL] =
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
	mutex_lock(&Ana_buf_Ctrl_Mutex);
	if (enable) {
		if (audck_buf_Count == 0) {
			Ana_Set_Reg(DCXO_CW14, 0x1 << 13, 0x1 << 13);
			pr_debug("-PMIC DCXO XO_AUDIO_EN_M enable, DCXO_CW14 = 0x%x\n",
				 Ana_Get_Reg(DCXO_CW14));
		}
		audck_buf_Count++;
	} else {
		audck_buf_Count--;
		if (audck_buf_Count == 0) {
			Ana_Set_Reg(DCXO_CW14, 0x0 << 13, 0x1 << 13);
			pr_debug("-PMIC DCXO XO_AUDIO_EN_M disable, DCXO_CW14 = 0x%x\n",
				 Ana_Get_Reg(DCXO_CW14));
			/* if didn't close, 60uA leak at audio analog */
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
	mutex_lock(&AudAna_lock);
	if (enable) {
		if (ClsqCount == 0) {
			/* Enable CLKSQ, Audio CLK source: internal CLK DCXO */
			Ana_Set_Reg(AUDENC_ANA_CON6, 0x0001, 0x0003);
		}
		ClsqCount++;
	} else {
		ClsqCount--;
		if (ClsqCount < 0) {
			pr_warn("%s(), count <0\n", __func__);
			ClsqCount = 0;
		}
		if (ClsqCount == 0) {
			/* disable CLKSQ */
			Ana_Set_Reg(AUDENC_ANA_CON6, 0x0000, 0x0003);
		}
	}
	mutex_unlock(&AudAna_lock);
}

static int TopCkCount;
static void Topck_Enable(bool enable)
{
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (TopCkCount == 0) {
			Ana_Set_Reg(AUD_TOP_CKPDN_CON0, 0x0, 0x66);
			/* Turn on AUDNCP_CLKDIV engine clock,Turn on AUD 26M */
		}
		TopCkCount++;
	} else {
		TopCkCount--;
		if (TopCkCount == 0) {
			Ana_Set_Reg(AUD_TOP_CKPDN_CON0, 0x66, 0x66);
			/* Turn off AUDNCP_CLKDIV engine clock, AUD 26M */
		}

		if (TopCkCount < 0) {
			pr_warn("%s(), <0 =%d\n ", __func__, TopCkCount);
			TopCkCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

static int NvRegCount;
static void NvregEnable(bool enable)
{
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (NvRegCount == 0) {
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1 << 4);
			/* Enable AUDGLB */
		}
		NvRegCount++;
	} else {
		NvRegCount--;
		if (NvRegCount == 0) {
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1 << 4, 0x1 << 4);
			/* Disable AUDGLB */
		}
		if (NvRegCount < 0) {
			pr_warn("%s(), <0 =%d\n ", __func__, NvRegCount);
			NvRegCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

static void set_playback_gpio(bool enable)
{
	if (enable) {
		/* set gpio mosi mode */
		Ana_Set_Reg(GPIO_MODE2_CLR, 0xffff, 0xffff);
		Ana_Set_Reg(GPIO_MODE2_SET, 0x0249, 0xffff);
		Ana_Set_Reg(GPIO_MODE2, 0x0249, 0xffff);
	} else {
		/* set pad_aud_*_mosi to GPIO mode and dir input
		 * reason:
		 * pad_aud_dat_mosi*, because the pin is used as boot strap
		 */
		Ana_Set_Reg(GPIO_MODE2_CLR, 0xffff, 0xffff);
		Ana_Set_Reg(GPIO_MODE2, 0x0000, 0xffff);
		Ana_Set_Reg(GPIO_DIR0, 0x0, 0xf << 8);
	}
}

static void set_capture_gpio(bool enable)
{
	if (enable) {
		/* set gpio miso mode */
		Ana_Set_Reg(GPIO_MODE3_CLR, 0xffff, 0xffff);
		Ana_Set_Reg(GPIO_MODE3_SET, 0x0249, 0xffff);
		Ana_Set_Reg(GPIO_MODE3, 0x0249, 0xffff);
	} else {
		/* set pad_aud_*_miso to GPIO mode and dir input
		 * reason:
		 * pad_aud_clk_miso, because when playback only the miso_clk
		 * will also have 26m, so will have power leak
		 * pad_aud_dat_miso*, because the pin is used as boot strap
		 */
		Ana_Set_Reg(GPIO_MODE3_CLR, 0xffff, 0xffff);
		Ana_Set_Reg(GPIO_MODE3, 0x0000, 0xffff);
		Ana_Set_Reg(GPIO_DIR0, 0x0, 0xf << 12);
	}
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

int set_codec_ops(struct mtk_codec_ops *ops)
{
	enable_dc_compensation = ops->enable_dc_compensation;
	set_lch_dc_compensation = ops->set_lch_dc_compensation;
	set_rch_dc_compensation = ops->set_rch_dc_compensation;
	set_ap_dmic = ops->set_ap_dmic;
	set_hp_impedance_ctl = ops->set_hp_impedance_ctl;

	return 0;
}

static int audio_get_auxadc_value(void)
{
	return pmic_get_auxadc_value(AUXADC_LIST_HPOFS_CAL);
}

static int get_accdet_auxadc(void)
{
	return pmic_get_auxadc_value(AUXADC_LIST_ACCDET);
}

#ifdef CONFIG_MTK_VOW_SUPPORT
static int VOW13MCKCount;
static int VOW32KCKCount;
#endif

#ifdef CONFIG_MTK_VOW_SUPPORT

static void VOW13MCK_Enable(bool enable)
{
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (VOW13MCKCount == 0)
			Ana_Set_Reg(AUD_TOP_CKPDN_CON0_CLR, 0x2000, 0x2000);
		/* Enable  AUD_TOP_CKPDN_CON0 bit13 for enable VOW 13M clock */
		VOW13MCKCount++;
	} else {
		VOW13MCKCount--;
		if (VOW13MCKCount == 0)
			Ana_Set_Reg(AUD_TOP_CKPDN_CON0_SET, 0x2000, 0x2000);
		/* disable AUD_TOP_CKPDN_CON0 bit13 for enable VOW 13M clock */

		if (VOW13MCKCount < 0) {
			pr_debug("VOW13MCKCount <0 =%d\n ", VOW13MCKCount);
			VOW13MCKCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

static void VOW32KCK_Enable(bool enable)
{
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (VOW32KCKCount == 0)
			Ana_Set_Reg(AUD_TOP_CKPDN_CON0_CLR, 0x1000, 0x1000);
		/* Enable AUD_TOP_CKPDN_CON0 bit12 for enable VOW 32k clock
		 * (for periodic on/off use)
		 */
		VOW32KCKCount++;
	} else {
		VOW32KCKCount--;
		if (VOW32KCKCount == 0)
			Ana_Set_Reg(AUD_TOP_CKPDN_CON0_SET, 0x1000, 0x1000);
		/* disable AUD_TOP_CKPDN_CON0 bit12 for enable VOW 32k clock */

		if (VOW32KCKCount < 0) {
			pr_debug("VOW32KCKCount <0 =%d\n ", VOW32KCKCount);
			VOW32KCKCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}
#endif

void vow_irq_handler(void)
{
#ifdef CONFIG_MTK_VOW_SUPPORT

	pr_debug("vow_irq,audio irq event....\n");
	/* TurnOnVOWADcPower(ANA_DEV_IN_ADC1, false); */
	/* TurnOnVOWDigitalHW(false); */
#if defined(VOW_TONE_TEST)
	EnableSineGen(Soc_Aud_InterConnectionOutput_O03,
		      Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
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

	/* 1. enable efuse ctrl engine clock */
	ret = pmic_config_interface(0x026C, 0x0040, 0xFFFF, 0);
	ret = pmic_config_interface(0x024E, 0x0004, 0xFFFF, 0);

	/* 2. */
	ret = pmic_config_interface(0x0C16, 0x1, 0x1, 0);

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
		while (reg_val == 1)
			ret = pmic_read_interface(0xC1A, &reg_val, 0x1, 0);

		udelay(1000);
		/* delay at least 1ms for 0xC1A and than can read 0xC18 */

		/* 6. read data */
		efusevalue[j] = upmu_get_reg_value(0x0C18);
		pr_debug("HPoffset : efuse[%d]=0x%x\n", j, efusevalue[j]);
		j++;
	}

	/* 7. Disable efuse ctrl engine clock */
	ret = pmic_config_interface(0x024C, 0x0004, 0xFFFF, 0);
	ret = pmic_config_interface(0x026A, 0x0040, 0xFFFF, 0);

	RG_AUDHPLTRIM_VAUDP15 = (efusevalue[0] >> 10) & 0xf;
	RG_AUDHPRTRIM_VAUDP15 = ((efusevalue[0] >> 14) & 0x3) + ((
					efusevalue[1] & 0x3) << 2);
	RG_AUDHPLFINETRIM_VAUDP15 = (efusevalue[1] >> 3) & 0x3;
	RG_AUDHPRFINETRIM_VAUDP15 = (efusevalue[1] >> 5) & 0x3;
	RG_AUDHPLTRIM_VAUDP15_SPKHP = (efusevalue[1] >> 7) & 0xF;
	RG_AUDHPRTRIM_VAUDP15_SPKHP = (efusevalue[1] >> 11) & 0xF;
	RG_AUDHPLFINETRIM_VAUDP15_SPKHP =
		((efusevalue[1] >> 15) & 0x1) + ((efusevalue[2] & 0x1) << 1);
	RG_AUDHPRFINETRIM_VAUDP15_SPKHP = ((efusevalue[2] >> 1) & 0x3);

	pr_debug("RG_AUDHPLTRIM_VAUDP15 = %x\n", RG_AUDHPLTRIM_VAUDP15);
	pr_debug("RG_AUDHPRTRIM_VAUDP15 = %x\n", RG_AUDHPRTRIM_VAUDP15);
	pr_debug("RG_AUDHPLFINETRIM_VAUDP15 = %x\n",
		RG_AUDHPLFINETRIM_VAUDP15);
	pr_debug("RG_AUDHPRFINETRIM_VAUDP15 = %x\n",
		RG_AUDHPRFINETRIM_VAUDP15);
	pr_debug("RG_AUDHPLTRIM_VAUDP15_SPKHP = %x\n",
		RG_AUDHPLTRIM_VAUDP15_SPKHP);
	pr_debug("RG_AUDHPRTRIM_VAUDP15_SPKHP = %x\n",
		RG_AUDHPRTRIM_VAUDP15_SPKHP);
	pr_debug("RG_AUDHPLFINETRIM_VAUDP15_SPKHP = %x\n",
		 RG_AUDHPLFINETRIM_VAUDP15_SPKHP);
	pr_debug("RG_AUDHPRFINETRIM_VAUDP15_SPKHP = %x\n",
		 RG_AUDHPRFINETRIM_VAUDP15_SPKHP);
#endif
#endif
}
EXPORT_SYMBOL(Auddrv_Read_Efuse_HPOffset);

static void setHpGainZero(void)
{
	Ana_Set_Reg(ZCD_CON2, DL_GAIN_0DB << 7, 0x0f80);
	Ana_Set_Reg(ZCD_CON2, DL_GAIN_0DB, 0x001f);
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

static void hp_main_output_ramp(bool up)
{
	int i = 0, stage = 0;
	int target = 0;

	/* Enable/Reduce HPL/R main output stage step by step */
	target = (low_power_mode == 1) ? 3 : 7;
	for (i = 0; i <= target; i++) {
		stage = up ? i : target - i;
		Ana_Set_Reg(AUDDEC_ANA_CON1, stage << 8, 0x7 << 8);
		Ana_Set_Reg(AUDDEC_ANA_CON1, stage << 11, 0x7 << 11);
		udelay(600);
	}
}

static void hp_aux_feedback_loop_gain_ramp(bool up)
{
	int i = 0, stage = 0;

	/* Reduce HP aux feedback loop gain step by step */
	for (i = 0; i <= 0xf; i++) {
		stage = up ? i : 0xf - i;
		Ana_Set_Reg(AUDDEC_ANA_CON9, stage << 12, 0xf << 12);
		udelay(600);
	}
}

static void hp_pull_down(bool enable)
{
	int from = enable ? 0x0 : 0x6;
	int to = enable ? 0x6 : 0x0;
	int i;

	if (enable) {
		for (i = from; i <= to; i++) {
			Ana_Set_Reg(AUDDEC_ANA_CON4, i, 0x7);
			udelay(600);
		}
	} else {
		for (i = from; i >= to; i--) {
			Ana_Set_Reg(AUDDEC_ANA_CON4, i, 0x7);
			udelay(600);
		}
	}
}

static bool is_valid_hp_pga_idx(int reg_idx)
{
	return (reg_idx >= DL_GAIN_8DB && reg_idx <= DL_GAIN_N_10DB) ||
	       reg_idx == DL_GAIN_N_40DB;
}

static void headset_volume_ramp(int from, int to)
{
	int offset = 0, count = 1, reg_idx;

	if (!is_valid_hp_pga_idx(from) || !is_valid_hp_pga_idx(to))
		pr_warn("%s(), volume index is not valid, from %d, to %d\n",
			__func__, from, to);

	if (to > from) {
		offset = to - from;
		while (offset > 0) {
			reg_idx = from + count;
			if (is_valid_hp_pga_idx(reg_idx)) {
				Ana_Set_Reg(ZCD_CON2,
					    (reg_idx << 7) | reg_idx,
					    DL_GAIN_REG_MASK);
				usleep_range(200, 300);
			}
			offset--;
			count++;
		}
	} else if (to < from) {
		offset = from - to;
		while (offset > 0) {
			reg_idx = from - count;
			if (is_valid_hp_pga_idx(reg_idx)) {
				Ana_Set_Reg(ZCD_CON2,
					    (reg_idx << 7) | reg_idx,
					    DL_GAIN_REG_MASK);
				usleep_range(200, 300);
			}
			offset--;
			count++;
		}
	}
}

static void setOffsetTrimMux(unsigned int Mux)
{
	/* Audio offset trimming buffer mux selection */
	Ana_Set_Reg(AUDDEC_ANA_CON8, Mux, 0xf);
}

static void setOffsetTrimBufferGain(unsigned int gain)
{
	/* Audio offset trimming buffer gain selection */
	Ana_Set_Reg(AUDDEC_ANA_CON8, gain << 4, 0x3 << 4);
}

static void EnableTrimbuffer(bool benable)
{
	if (benable == true) {
		Ana_Set_Reg(AUDDEC_ANA_CON8, 0x1 << 6, 0x1 << 6);
		/* Audio offset trimming buffer enable */
	} else {
		Ana_Set_Reg(AUDDEC_ANA_CON8, 0x0, 0x1 << 6);
		/* Audio offset trimming buffer disable */
	}
}

/* For MT6358 LO */
#if 0
static void Apply_Speaker_Gain(void)
{
	Ana_Set_Reg(ZCD_CON1,
		    (mCodec_data->ana_gain[ANA_GAIN_LINEOUTR] << 7) |
		    mCodec_data->ana_gain[ANA_GAIN_LINEOUTL],
		    DL_GAIN_REG_MASK);
}
#else
static void Apply_Speaker_Gain(int spk_pga_gain)
{
	Ana_Set_Reg(ZCD_CON1, (spk_pga_gain << 7) | spk_pga_gain,
		DL_GAIN_REG_MASK);
}
#endif

#ifndef CONFIG_FPGA_EARLY_PORTING
static void OpenTrimBufferHardware(bool enable, bool buffer_on)
{
	pr_debug("%s(), enable %d, buffer_on %d\n",
		__func__, enable, buffer_on);

	if (enable) {
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0xa0, 0xff);

		TurnOnDacPower(ANA_DEV_OUT_HEADSETL);

		/* sdm output mute enable */
		/* Ana_Set_Reg(AFUNC_AUD_CON1, 0x0000, 0xffff); */
		/* Ana_Set_Reg(AFUNC_AUD_CON0, 0xcbad, 0xffff); */

		/* Disable handset short-circuit protection */
		/* Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff); */
		/* Disable linout short-circuit protection */
		/* Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff); */

		/* Disable headphone short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);


		/* Reduce ESD resistance of AU_REFN */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc000, 0xffff);

		/* Set HPL/HPR gain to -10dB*/
		Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_10DB_REG, 0xffff);

		/* Turn on DA_600K_NCP_VA18 */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON1, 0x0001, 0xffff);

		/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON2, 0x002c, 0xffff);

		/* Toggle RG_DIVCKS_CHG */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON0, 0x0001, 0xffff);

		/* Set NCP soft start mode as default mode: 150us */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON4, 0x0002, 0xffff);

		/* Enable NCP */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x0000, 0xffff);
		udelay(250);

		/* Enable cap-less LDOs (1.5V), LCLDO local sense */
		Ana_Set_Reg(AUDDEC_ANA_CON14, 0x1055, 0x1055);

		/* Enable NV regulator (-1.2V) */
		Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0001, 0xffff);
		udelay(100);

		/* Disable AUD_ZCD */
		/* Ana_Set_Reg(ZCD_CON0, 0x0000, 0xffff); */
		Hp_Zcd_Enable(false);

		/* Enable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

		/* Set HP DR bias current optimization, 010: 6uA */
		Ana_Set_Reg(AUDDEC_ANA_CON11, 0x4900, 0xffff);

		/* Set HP & ZCD bias current optimization */
		/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

		/* Set HPP/N STB enhance circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc033, 0xffff);

		if (buffer_on) {
			/* Enable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x000c, 0xffff);
			/* Enable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x003c, 0xffff);
			/* Enable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0c80, 0xffff);

			/* Enable HP driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x30c0, 0xffff);

			/* Enable HP driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x30f0, 0xffff);

			/* Short HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00fc, 0x00ff);

			/* Enable HP main CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0e80, 0xffff);

			/* Disable HP aux CMFB loop	*/
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0280, 0xffff);

			/* Select CMFB resistor bulk to AC mode	*/
			/* Selec HS/LO cap size (6.5pF default) */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0xffff);

			/* Enable HS driver bias circuits */
			/* Disable HS main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
			/* Enable LO driver bias circuits */
			/* Disable LO main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);

			/* Enable HP main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00ff, 0x00ff);

			/* Enable HPR/L main output stage step by step */
			hp_main_output_ramp(true);

			/* Reduce HP aux feedback loop gain step by step */
			hp_aux_feedback_loop_gain_ramp(true);

			/* Disable HP aux feedback loop	*/
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fcf, 0xffff);

			/* Increase HPL/HPR gain to normal gain step by step */
			headset_volume_ramp(DL_GAIN_N_10DB,
				mCodec_data->ana_gain[ANA_GAIN_HPOUTL]);

			/* Disable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fc3, 0xffff);

			/* Unshort HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3f03, 0xffff);
			udelay(1000);

			/* Disable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1);

			/* Disable Audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x000f);

			/* Disable low-noise mode of DAC*/
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0000, 0x0001);

			/* Switch HPL/HPR MUX to open */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x0f00);

			/* Disable Pull-down HPL/R to AVSS28_AUD*/
			hp_pull_down(false);

			/* Enable Trim buffer VA28 reference */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x1<<1, 0x1<<1);

		} else {
			/* Enable HP driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x30c0, 0xffff);
			/* Disable HP main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0000, 0xffff);
			/* Enable HS driver bias circuits */
			/* Disable HS main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0012, 0xffff);
			/* Enable LO driver bias circuits */
			/* Disable LO main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0012, 0xffff);
		}

		/* Enable AUD_CLK */
		/* Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1); */

		/* Enable low-noise mode of DAC */
		/* Ana_Set_Reg(AUDDEC_ANA_CON9, 0x1, 0x1); */
		udelay(100);
	} else {

		/* Pull-down HPL/R to AVSS28_AUD */
		hp_pull_down(true);

		/* HPR/HPL mux to open */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x0f00);

		/* Disable low-noise mode of DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0, 0x1);

		/* Disable Audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x000f);

		/* Disable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1);

		/* Short HP main output to HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fc3, 0xffff);

		/* Enable HP aux output stage*/
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fcf, 0xffff);

		/* decrease HPL/R gain to normal gain step by step */
		headset_volume_ramp(mCodec_data->ana_gain[ANA_GAIN_HPOUTL],
				    DL_GAIN_N_10DB);

		/* Enable HP aux feedback loop */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fff, 0xffff);

		/* set HP aux feedback loop gain to max step by step */
		hp_aux_feedback_loop_gain_ramp(false);

		/* decrease HPR/L main output stage step by step */
		hp_main_output_ramp(false);

		/* Disable HP main output stage	*/
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3);

		/* Enable HP aux CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0e00, 0x0f00);

		/* Disable HP main CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0c00, 0x0f00);

		/* Unshort HP main output to HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3 << 6);

		/* Disable HP driver core circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x30c0, 0xffff);

		/* Disable HP driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);

		/* Disable HP aux CMFB loop	*/
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0, 0xffff);

		/* Open HP aux feedback loop */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0xc, 0xffff);

		/* Disable HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0xffff);

		/* Disable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x1 << 8, 0x1 << 8);
		/* Disable NV regulator (-1.2V) */
		Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0, 0x1);
		/* Disable cap-less LDOs (1.5V) */
		Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0, 0x1055);
		/* Disable NCP */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x1, 0x1);

		/* Set HPL/HPR gain to mute	*/
		Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_40DB_REG, 0xffff);

		/* Increase ESD resistance of AU_REFN */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0, 0x1 << 14);

		TurnOffDacPower();

	}
}

static void set_input_mux(unsigned int Mux)
{
	Ana_Set_Reg(AUDDEC_ANA_CON0, Mux << 8, 0x3 << 8);
}
static void enable_lo_buffer(bool benable)
{
	Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0110, 0xffff);
	Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0112, 0xffff);
	Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0113, 0xffff);
}
static void OpenTrimBufferHardware_withLO(bool enable, bool buffer_on)
{
	pr_debug("%s(), enable %d, buffer_on %d\n",
		__func__, enable, buffer_on);
	if (enable) {
		TurnOnDacPower(ANA_DEV_OUT_HEADSETL);
		/* HP IVBUF (Vin path) de-gain enable: -12dB */
		if (apply_n12db_gain)
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0004, 0xff);

		/* Audio left headphone input multiplexor selection : LOL*/
		set_input_mux(1);

		/* Disable headphone short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xf0ff);

		/* Disable lineout short-ckt protection */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x1 << 4, 0x1 << 4);

		/* Reduce ESD resistance of AU_REFN */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc000, 0xffff);

		/* Set HPL/HPR gain to -10dB*/
		Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_10DB_REG, 0xffff);

		/* Set LOLR/LOLL gain to -10dB */
		Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_10DB_REG, 0xffff);

		/* Turn on DA_600K_NCP_VA18 */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON1, 0x0001, 0xffff);

		/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON2, 0x002c, 0xffff);

		/* Toggle RG_DIVCKS_CHG */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON0, 0x0001, 0xffff);

		/* Set NCP soft start mode as default mode: 150us */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON4, 0x0002, 0xffff);

		/* Enable NCP */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x0000, 0xffff);
		udelay(250);

		/* Enable cap-less LDOs (1.5V), LCLDO local sense */
		Ana_Set_Reg(AUDDEC_ANA_CON14, 0x1055, 0x1055);

		/* Enable NV regulator (-1.2V) */
		Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0001, 0xffff);
		udelay(100);

		/* Disable AUD_ZCD */
		Hp_Zcd_Enable(false);

		/* Enable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

		/* Set HP DR bias current optimization, 010: 6uA */
		Ana_Set_Reg(AUDDEC_ANA_CON11, 0x4900, 0xffff);

		/* Set HP & ZCD bias current optimization */
		/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

		/* Set HPP/N STB enhance circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc033, 0xffff);

		if (buffer_on) {
			if (apply_n12db_gain) {
				/* HP IVBUF (Vin path) de-gain enable: -12dB */
				Ana_Set_Reg(AUDDEC_ANA_CON10, 0x04, 0xff);
			}
			/* Set LO STB enhance circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x1 << 8, 0x1 << 8);

			/* Enable LO driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x1 << 1, 0x1 << 1);

			/* Enable LO driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x1, 0x1);

			/* Set LOL gain to normal gain step by step */
			Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_10DB_REG, 0xffff);

			/* Switch HPL MUX to Line-out */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x01 << 8, 0x3 << 8);

			/* Enable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0c, 0xff);

			/* Enable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3c, 0xff);

			/* Enable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0c00, 0xff00);

			/* Enable HP driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xc0, 0xf0);

			/* Enable HP driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xf0, 0xf0);

			/* Short HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0xfc, 0xff);

			/* Enable HP main CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0e00, 0xff00);

			/* Disable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0200, 0xff00);

			/* Select CMFB resistor bulk to AC mode */
			/* Selec HS/LO cap size (6.5pF default) */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0xffff);

			/* Enable HS driver bias circuits */
			/* Disable HS main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
			/* Enable LO driver bias circuits */
			/* Disable LO main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);

			/* Enable LO main output stage */
			enable_lo_buffer(true);
			Apply_Speaker_Gain(DL_GAIN_0DB);

			/* Enable HP main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00ff, 0x00ff);

			/* Enable HPR/L main output stage step by step */
			hp_main_output_ramp(true);

			/* Reduce HP aux feedback loop gain step by step */
			hp_aux_feedback_loop_gain_ramp(true);

			/* Disable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00cf, 0x00ff);

			/* Increase HPL/HPR gain to normal gain step by step */
			headset_volume_ramp(DL_GAIN_N_10DB,
				mCodec_data->ana_gain[ANA_GAIN_HPOUTL]);

			/* Disable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00c3, 0x00ff);

			/* Unshort HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0003, 0x00ff);
			udelay(1000);

			/* HP ESD resistor @AU_REFN short enable */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc033, 0xffff);

		}
		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);

		/* Enable low-noise mode of DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x1, 0x1);
		udelay(100);

		/* Switch LOL MUX to audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x2 << 2, 0x3 << 2);

		/* Disable Pull-down HPL/R to AVSS28_AUD */
		hp_pull_down(false);

	} else {
		/* Pull-down HPL/R to AVSS28_AUD */
		hp_pull_down(true);

		/* Disable low-noise mode of DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0, 0x1);

		/* Disable Audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0xf);

		/* Disable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1);

		/* Short HP main output to HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fc3, 0xffff);

		/* Enable HP aux output stage*/
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fcf, 0xffff);

		/* decrease HPL/R gain to normal gain step by step */
		headset_volume_ramp(mCodec_data->ana_gain[ANA_GAIN_HPOUTL],
				    DL_GAIN_N_10DB);
		Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_10DB_REG, 0xffff);

		/* set HP aux feedback loop gain to max */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xf200, 0xff00);

		/* Enable HP aux feedback loop */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0xff, 0xff);

		/* Reduce HP aux feedback loop gain */
		hp_aux_feedback_loop_gain_ramp(false);

		/* decrease HPR/L main output stage step by step */
		hp_main_output_ramp(false);

		/* Switch HPL/HPR MUX to open */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0 << 8, 0xf << 8);

		/* Switch LOL MUX to open */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0 << 2, 0x3 << 2);

		/* Disable HP main output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3);

		/* Enable HP aux CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0e00, 0x0f00);

		/* Disable HP main CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0c00, 0x0f00);

		/* Unshort HP main output to HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3 << 6);

		/* Disable HP driver core circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x30c0, 0xffff);

		/* Disable HP driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);

		/* Disable HP aux CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0, 0xffff);

		/* Open HP aux feedback loop */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0xc, 0xffff);

		/* Disable HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0xffff);

		/* decrease LOL gain to minimum gain step by step */
		Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_40DB_REG, 0xffff);

		/* Disable LO driver core circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0, 0x1);

		/* Disable LO driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0 << 1, 0x1 << 1);

		/* Disable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x1 << 8, 0x1 << 8);
		/* Disable NV regulator (-1.2V) */
		Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0, 0x1);
		/* Disable cap-less LDOs (1.5V) */
		Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0, 0x1055);
		/* Disable NCP */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x1, 0x1);

		/* Set HPL/HPR gain to mute */
		Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_40DB_REG, 0xffff);

		/* Increase ESD resistance of AU_REFN */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0, 0x1 << 14);

		TurnOffDacPower();
	}
}

#endif

static bool OpenHeadPhoneImpedanceSetting(bool bEnable)
{
	pr_debug("%s benable = %d\n", __func__, bEnable);
	if (GetDLStatus() == true)
		return false;

	if (bEnable == true) {
		mIsNeedPullDown = false;
		TurnOnDacPower(ANA_DEV_OUT_HEADSETL);

		/* Disable headphone short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);

		/* Reduce ESD resistance of AU_REFN */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x4000, 0xffff);
		/* Turn on DA_600K_NCP_VA18 */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON1, 0x0001, 0xffff);
		/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON2, 0x002c, 0xffff);
		/* Toggle RG_DIVCKS_CHG */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON0, 0x0001, 0xffff);
		/* Set NCP soft start mode as default mode: 150us */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON4, 0x0002, 0xffff);
		/* Enable NCP */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x0000, 0xffff);
		udelay(250);

		/* Enable cap-less LDOs (1.5V) */
		Ana_Set_Reg(AUDDEC_ANA_CON14, 0x1055, 0x1055);
		/* Enable NV regulator (-1.2V) */
		Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0001, 0xffff);
		udelay(100);

		/* Disable AUD_ZCD */
		Hp_Zcd_Enable(false);

		/* Enable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

		/* Disable HPR/L STB enhance circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x4000, 0xffff);

		/* Disable Pull-down HPL/R to AVSS28_AUD */
		Ana_Set_Reg(AUDDEC_ANA_CON4, 0x0000, 0xffff);

		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);

		/* Enable Audio L channel DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3009, 0xffff);

		/* Enable Trim buffer VA28 reference */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0002, 0x00ff);

		/* Enable HPDET circuit,
		 * select DACLP as HPDET input and HPR as HPDET output
		 */
		Ana_Set_Reg(AUDDEC_ANA_CON8, 0x1900, 0xffff);

		/* Enable TRIMBUF circuit, select HPR as TRIMBUF input */
		/* Set TRIMBUF gain as 18dB */
		Ana_Set_Reg(AUDDEC_ANA_CON8, 0x1972, 0xffff);
	} else {
		/* disable HPDET circuit */
		Ana_Set_Reg(AUDDEC_ANA_CON8, 0x0000, 0xff00);

		/* Disable Trim buffer VA28 reference */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0000, 0x00ff);

		/* Disable Audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x000f);

		/* Disable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1);

		/* Disable HP main output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3);

		/* Disable HP driver core circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0x3 << 4);

		/* Disable HP driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0x3 << 6);

		/* Disable HP aux CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0, 0xff << 8);

		/* Disable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x1 << 8, 0x1 << 8);

		/* Disable NV regulator (-1.2V) */
		Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0, 0x1);

		/* Disable cap-less LDOs (1.5V) */
		Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0, 0x1055);

		/* Disable NCP */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x1, 0x1);

		/* Set HPL/HPR gain to mute */
		Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_10DB_REG, 0xffff);

		/* Set HPP/N STB enhance circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x33, 0xff);

		/* Increase ESD resistance of AU_REFN */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0, 0x1 << 14);

		TurnOffDacPower();
		mIsNeedPullDown = true;
	}
	return true;
}

/* Headphone Impedance Detection */
/* Pmic Headphone Impedance variable */
struct mtk_hpdet_param {
	int auxadc_upper_bound;
	int dc_Step;
	int dc_Phase0;
	int dc_Phase1;
	int dc_Phase2;
	int resistance_first_threshold;
	int resistance_second_threshold;
};

static int hp_impedance;
static const int auxcable_impedance = 5000;
static int efuse_current_calibrate;

static void mtk_read_hp_detection_parameter(struct mtk_hpdet_param *hpdet_param)
{
	hpdet_param->auxadc_upper_bound =
		32630; /* should little lower than auxadc max resolution */
	hpdet_param->dc_Step = 96; /* Dc ramp up and ramp down step */
	hpdet_param->dc_Phase0 =
		288; /* Phase 0 : high impedance with worst resolution */
	hpdet_param->dc_Phase1 =
		1440; /* Phase 1 : median impedance with normal resolution */
	hpdet_param->dc_Phase2 =
		6048; /* Phase 2 : low impedance with better resolution */
	hpdet_param->resistance_first_threshold =
		250; /* Resistance Threshold of phase 2 and phase 1 */
	hpdet_param->resistance_second_threshold =
		1000; /* Resistance Threshold of phase 1 and phase 0 */
}

static int mtk_calculate_impedance_formula(int pcm_offset, int aux_diff)
{
	/* The formula is from DE programming guide */
	/* should be mantain by pmic owner */
	/* R = V /I */
	/* V = auxDiff * (1800mv /auxResolution)  /TrimBufGain */
	/* I =  pcmOffset * DAC_constant * Gsdm * Gibuf */

	long val = 3600000 / pcm_offset * aux_diff;

	return (int)DIV_ROUND_CLOSEST(val, 7832);
}

static int mtk_calculate_hp_impedance(int dc_init, int dc_input,
				      short pcm_offset,
				      const unsigned int detect_times)
{
	int dc_value;
	int r_tmp = 0;

	if (dc_input < dc_init) {
		pr_warn("%s(), Wrong[%d] : dc_input(%d) > dc_init(%d)\n",
			__func__, pcm_offset, dc_input, dc_init);
		return 0;
	}

	dc_value = dc_input - dc_init;
	r_tmp = mtk_calculate_impedance_formula(pcm_offset, dc_value);
	r_tmp = DIV_ROUND_CLOSEST(r_tmp, detect_times);

	/* Efuse calibration */
	if ((efuse_current_calibrate != 0) && (r_tmp != 0)) {
		pr_debug("%s(), Before Calibration from EFUSE: %d, R: %d\n",
			 __func__, efuse_current_calibrate, r_tmp);
		r_tmp = DIV_ROUND_CLOSEST(r_tmp * 128 + efuse_current_calibrate,
					  128);
	}

	pr_debug("%s(), pcm_offset %d dcoffset %d detected resistor is %d\n",
		 __func__, pcm_offset, dc_value, r_tmp);

	return r_tmp;
}

static int detect_impedance(void)
{
	const unsigned int kDetectTimes = 8;
	unsigned int counter;
	int dcSum = 0, detectSum = 0;
	int detectsOffset[kDetectTimes];
	int pick_impedance = 0, impedance = 0, phase_flag = 0;
	int dcValue = 0;
	int old_value_auxadc_con1 = Ana_Get_Reg(AUXADC_CON1);
	struct mtk_hpdet_param hpdet_param;

	if (enable_dc_compensation &&
	    set_lch_dc_compensation &&
	    set_rch_dc_compensation) {
		set_lch_dc_compensation(0);
		set_rch_dc_compensation(0);
		enable_dc_compensation(true);
	} else {
		pr_warn("%s(), dc compensation ops not ready\n", __func__);
		return 0;
	}

	mtk_read_hp_detection_parameter(&hpdet_param);

	Ana_Set_Reg(AUXADC_CON10, AUXADC_AVG_64, 0x7);

	/* Set AUXADC_SPL_NUM as 0xC for hp imp detect */
	Ana_Set_Reg(AUXADC_CON1, 0xC << 6, 0xf << 6);

	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPR);
	setOffsetTrimBufferGain(3); /* HPDET trim. buffer gain : 18db */
	EnableTrimbuffer(true);
	setHpGainZero();

	for (dcValue = 0; dcValue <= hpdet_param.dc_Phase2;
	     dcValue += hpdet_param.dc_Step) {

		/* apply dc by dc compensation: 16bit MSB and negative value */
		set_lch_dc_compensation(-dcValue << 16);
		set_rch_dc_compensation(-dcValue << 16);

		/* save for DC =0 offset */
		if (dcValue == 0) {
			usleep_range(1*1000, 1*1000);
			dcSum = 0;
			for (counter = 0; counter < kDetectTimes; counter++) {
				detectsOffset[counter] =
					audio_get_auxadc_value();
				dcSum = dcSum + detectsOffset[counter];
			}

			if ((dcSum / kDetectTimes) >
			    hpdet_param.auxadc_upper_bound) {
				pr_debug("%s(), dcValue == 0, auxadc value %d > auxadc_upper_bound %d\n",
					 __func__, dcSum / kDetectTimes,
					 hpdet_param.auxadc_upper_bound);
				impedance = auxcable_impedance;
				break;
			}
		}

		/* start checking */
		if (dcValue == hpdet_param.dc_Phase0) {
			usleep_range(1*1000, 1*1000);
			detectSum = 0;
			detectSum = audio_get_auxadc_value();

			if ((dcSum / kDetectTimes) == detectSum) {
				pr_debug("%s(), dcSum / kDetectTimes %d == detectSum %d\n",
					 __func__,
					 dcSum / kDetectTimes, detectSum);
				impedance = auxcable_impedance;
				break;
			}

			pick_impedance = mtk_calculate_hp_impedance(
						dcSum/kDetectTimes,
						detectSum, dcValue, 1);

			if (pick_impedance <
				hpdet_param.resistance_first_threshold) {
				phase_flag = 2;
				continue;
			} else if (pick_impedance <
			hpdet_param.resistance_second_threshold) {
				phase_flag = 1;
				continue;
			}

			/* Phase 0 : detect  range 1kohm to 5kohm impedance */
			for (counter = 1; counter < kDetectTimes; counter++) {
				detectsOffset[counter] =
					audio_get_auxadc_value();
				detectSum = detectSum + detectsOffset[counter];
			}

			if ((detectSum / kDetectTimes) >
				hpdet_param.auxadc_upper_bound)
				impedance = auxcable_impedance;
			else
				impedance = mtk_calculate_hp_impedance(
						dcSum, detectSum,
						dcValue, kDetectTimes);
			break;
		}

		/* Phase 1 : detect  range 250ohm to 1000ohm impedance */
		if (dcValue == hpdet_param.dc_Phase1 && phase_flag == 1) {
			usleep_range(1*1000, 1*1000);
			detectSum = 0;
			for (counter = 0; counter < kDetectTimes; counter++) {
				detectsOffset[counter] =
					audio_get_auxadc_value();
				detectSum = detectSum + detectsOffset[counter];
			}
			impedance = mtk_calculate_hp_impedance(dcSum, detectSum,
							       dcValue,
							       kDetectTimes);
			break;
		}

		/* Phase 2 : detect under 250ohm impedance */
		if (dcValue == hpdet_param.dc_Phase2 && phase_flag == 2) {
			usleep_range(1*1000, 1*1000);
			detectSum = 0;
			for (counter = 0; counter < kDetectTimes; counter++) {
				detectsOffset[counter] =
					audio_get_auxadc_value();
				detectSum = detectSum + detectsOffset[counter];
			}
			impedance = mtk_calculate_hp_impedance(dcSum, detectSum,
							       dcValue,
							       kDetectTimes);
			break;
		}
		usleep_range(1*200, 1*200);
	}

	pr_debug("%s(), phase %d [dc,detect]Sum %d times [%d,%d], hp_impedance %d, pick_impedance %d, AUXADC_CON1 0x%x, AUXADC_CON10 0x%x\n",
		 __func__, phase_flag, kDetectTimes, dcSum, detectSum,
		 impedance, pick_impedance,
		 Ana_Get_Reg(AUXADC_CON1),
		 Ana_Get_Reg(AUXADC_CON10));

	/* Ramp-Down */
	while (dcValue > 0) {
		dcValue = dcValue - hpdet_param.dc_Step;
		/* apply dc by dc compensation: 16bit MSB and negative value */
		set_lch_dc_compensation(-dcValue << 16);
		set_rch_dc_compensation(-dcValue << 16);
		usleep_range(1*200, 1*200);
	}

	set_lch_dc_compensation(0);
	set_rch_dc_compensation(0);
	enable_dc_compensation(false);
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
	EnableTrimbuffer(false);

	/* Restore AUXADC_CON1 after hp imp detect */
	Ana_Set_Reg(AUXADC_CON1, old_value_auxadc_con1, 0xffff);

	return impedance;
}

/* 1.7V * 0.5kohm / (2.5 + 0.5)kohm = 0.283V, support 1k ~ 14k, 0.5k margin */
#define MIC_VINP_4POLE_THRES_MV 283
#define VINP_NORMALIZED_TO_MV 1700

static int dctrim_calibrated;
static int hpl_dc_offset, hpr_dc_offset;
static int mic_vinp_mv;
static int spkl_dc_offset;
#ifndef ANALOG_HPTRIM
static int last_lch_comp_value, last_rch_comp_value;
static int last_spk2hp_lch_comp_value, last_spk2hp_rch_comp_value;
#else
struct anaoffset {
	int enable;
	int hpl_trimecode;
	int hpr_trimecode;
	int hpl_finetrim;
	int hpr_finetrim;
};

static struct anaoffset hp_3pole_anaoffset, hp_4pole_anaoffset,
	       spk_3pole_anaoffset, spk_4pole_anaoffset;

static int dc_compensation_disabled;
static unsigned int hp_3_pole_trim_setting;
static unsigned int hp_4_pole_trim_setting;
static unsigned int spk_hp_3_pole_trim_setting;
static unsigned int spk_hp_4_pole_trim_setting;
#endif
static int mHplTrimOffset;
static int mHprTrimOffset;

static const int dBFactor_Den = 8192;
/* 1 / (10 ^ (dB / 20)) * dBFactor_Den */
static const int dBFactor_Nom[32] = {
	3261, 3659, 4106, 4607,
	5169, 5799, 6507, 7301,
	8192, 9192, 10313, 11572,
	12983, 14568, 16345, 18340,
	20577, 23088, 25905, 819200,
	819200, 819200, 819200, 819200,
	819200, 819200, 819200, 819200,
	819200, 819200, 819200, 819200,
};
#ifndef ANALOG_HPTRIM
static int get_mic_bias_mv(void)
{
	unsigned int mic_bias = (Ana_Get_Reg(AUDENC_ANA_CON10) >> 4) & 0x7;

	switch (mic_bias) {
	case MIC_BIAS_1p7:
		return 1700;
	case MIC_BIAS_1p8:
		return 1800;
	case MIC_BIAS_1p9:
		return 1900;
	case MIC_BIAS_2p0:
		return 2000;
	case MIC_BIAS_2p1:
		return 2100;
	case MIC_BIAS_2p5:
		return 2500;
	case MIC_BIAS_2p6:
		return 2600;
	case MIC_BIAS_2p7:
		return 2700;
	default:
		pr_warn("%s(), invalid mic_bias %d\n", __func__, mic_bias);
		return 2600;
	};
}

static int calOffsetToDcComp(int offset, int vol_type)
{
	int gain = mCodec_data->ana_gain[vol_type];
	int mic_bias_mv;
	int real_mic_vinp_mv;

	int offset_scale = DIV_ROUND_CLOSEST(offset * dBFactor_Nom[gain],
					     dBFactor_Den);

	if (mic_vinp_mv > MIC_VINP_4POLE_THRES_MV &&
	    ((codec_debug_enable & DBG_DCTRIM_BYPASS_4POLE) == 0)) {
		int v_diff_bias_vinp;
		int v_diff_bias_vinp_scale;

		/* refine mic bias influence on 4 pole headset */
		mic_bias_mv = get_mic_bias_mv();
		real_mic_vinp_mv = DIV_ROUND_CLOSEST(mic_vinp_mv * mic_bias_mv,
						     VINP_NORMALIZED_TO_MV);

		v_diff_bias_vinp = mic_bias_mv - real_mic_vinp_mv;
		v_diff_bias_vinp_scale = DIV_ROUND_CLOSEST((v_diff_bias_vinp) *
					 dBFactor_Nom[gain],
					 dBFactor_Den);

		if ((codec_debug_enable & DBG_DCTRIM_4POLE_LOG) != 0) {
			pr_debug("%s(), mic_bias_mv %d, mic_vinp_mv %d, real_mic_vinp_mv %d\n",
				 __func__,
				 mic_bias_mv, mic_vinp_mv, real_mic_vinp_mv);

			pr_debug("%s(), a %d, b %d\n", __func__,
				 DIV_ROUND_CLOSEST(offset_scale * 2804225,
						   32768),
				 DIV_ROUND_CLOSEST(v_diff_bias_vinp_scale *
						   1782,
						   1800));
		}

		return DIV_ROUND_CLOSEST(offset_scale * 2804225, 32768) -
		       DIV_ROUND_CLOSEST(v_diff_bias_vinp_scale * 1782, 1800);
	} else {
		/* The formula is from DE programming guide */
		/* should be mantain by pmic owner */
		return DIV_ROUND_CLOSEST(offset_scale * 2804225, 32768);
	}
}

static int get_dc_ramp_step(int gain)
{
	/* each step should be smaller than 100uV */
	/* 1 pcm of dc compensation = 0.0808uV HP buffer voltage @ 0dB*/
	/* 90uV / 0.0808uV(0dB) = 1113.73 */
	int step_0db = 1114;

	/* scale for specific gain */
	return step_0db * dBFactor_Nom[gain] / dBFactor_Den;
}

static int SetDcCompenSation(bool enable)
{
	int lch_value = 0, rch_value = 0, tmp_ramp = 0;
	int times = 0, i = 0;
	int sign_lch = 0, sign_rch = 0;
	int abs_lch = 0, abs_rch = 0;
	int index_lgain = mCodec_data->ana_gain[ANA_GAIN_HPOUTL];
	int diff_lch = 0, diff_rch = 0, ramp_l = 0, ramp_r = 0;
	int ramp_step = get_dc_ramp_step(index_lgain);

	if (enable_dc_compensation == NULL ||
	    set_lch_dc_compensation == NULL ||
	    set_rch_dc_compensation == NULL) {
		pr_warn("%s(), function not ready, enable %p, lch %p, rch %p\n",
			__func__, enable_dc_compensation,
			set_lch_dc_compensation, set_rch_dc_compensation);
		return -EFAULT;
	}

	if (enable && index_lgain == DL_GAIN_N_40DB) {
		pr_debug("%s(), -40dB skip dc compensation\n", __func__);
		return 0;
	}

	lch_value = calOffsetToDcComp(hpl_dc_offset,
				      ANA_GAIN_HPOUTL);
	rch_value = calOffsetToDcComp(hpr_dc_offset,
				      ANA_GAIN_HPOUTR);
	diff_lch = enable ? lch_value - last_lch_comp_value : lch_value;
	diff_rch = enable ? rch_value - last_rch_comp_value : rch_value;
	sign_lch = diff_lch < 0 ? -1 : 1;
	sign_rch = diff_rch < 0 ? -1 : 1;
	abs_lch = sign_lch * diff_lch;
	abs_rch = sign_rch * diff_rch;
	times = abs_lch > abs_rch ?
		(abs_lch / ramp_step) : (abs_rch / ramp_step);
	pr_debug("%s(), enable = %d, index_gain = %d, times = %d, lch_value = %d -> %d, rch_value = %d -> %d, ramp_step %d, mic_vinp_mv %d\n",
		 __func__, enable, index_lgain, times,
		 last_lch_comp_value, lch_value,
		 last_rch_comp_value, rch_value, ramp_step, mic_vinp_mv);

	if (enable) {
		enable_dc_compensation(true);
		for (i = 1; i <= times; i++) {
			tmp_ramp = i * ramp_step;
			if (tmp_ramp < abs_lch) {
				ramp_l = last_lch_comp_value +
					sign_lch * tmp_ramp;
				set_lch_dc_compensation(ramp_l << 8);
			}
			if (tmp_ramp < abs_rch) {
				ramp_r = last_rch_comp_value +
					sign_rch * tmp_ramp;
				set_rch_dc_compensation(ramp_r << 8);
			}
			udelay(100);
		}
		set_lch_dc_compensation(lch_value << 8);
		set_rch_dc_compensation(rch_value << 8);
		last_lch_comp_value = lch_value;
		last_rch_comp_value = rch_value;
	} else {
		for (i = times; i >= 0; i--) {
			tmp_ramp = i * ramp_step;
			if (tmp_ramp < abs_lch)
				set_lch_dc_compensation(tmp_ramp << 8);

			if (tmp_ramp < abs_rch)
				set_rch_dc_compensation(tmp_ramp << 8);
			udelay(100);
		}
		set_lch_dc_compensation(0);
		set_rch_dc_compensation(0);
		enable_dc_compensation(false);
		last_lch_comp_value = 0;
		last_rch_comp_value = 0;
	}

	return 0;
}
#endif
static int calculate_trim_result(int *on_value, int *off_value, int trimTime,
				 int discard_num, int useful_num)
{
	int i = 0, j = 0, tmp = 0, offset = 0;

	/* sort */
	for (i = 0; i < trimTime - 1; i++) {
		for (j = 0; j < trimTime - 1 - i; j++) {
			if (on_value[j] > on_value[j + 1]) {
				tmp = on_value[j + 1];
				on_value[j + 1] = on_value[j];
				on_value[j] = tmp;
			}
			if (off_value[j] > off_value[j + 1]) {
				tmp = off_value[j + 1];
				off_value[j + 1] = off_value[j];
				off_value[j] = tmp;
			}
		}
	}
	/* calculate result */
	for (i = discard_num; i < trimTime - discard_num; i++)
		offset += on_value[i] - off_value[i];

	return DIV_ROUND_CLOSEST(offset, useful_num);
}
static void get_hp_trim_offset(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING

#ifdef ANALOG_HPTRIM
#define TRIM_TIMES 7
#else
#define TRIM_TIMES 26
#endif
#define TRIM_DISCARD_NUM 1
#define TRIM_USEFUL_NUM (TRIM_TIMES - (TRIM_DISCARD_NUM * 2))

	int on_valueL[TRIM_TIMES], on_valueR[TRIM_TIMES];
	int off_valueL[TRIM_TIMES], off_valueR[TRIM_TIMES];
	int i;

	Ana_Set_Reg(AUXADC_CON10, AUXADC_AVG_256, 0x7);

	/* Step1. turn on buffer */
	OpenTrimBufferHardware(true, true);

	/* L Channel */
	/* Step2. Enable TRIMBUF circuit,
	 * select HPL as TRIMBUF input and set TRIMBUF gain as 18dB
	 */
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPL);
	setOffsetTrimBufferGain(3); /* 18db */
	EnableTrimbuffer(true);

	/* Step3. Request AuxADC to get HP on dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		on_valueL[i] = audio_get_auxadc_value();

	/* Step4. select AUDREFN as TRIMBUF input */
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_AU_REFN);

	/* Step5. Request AuxADC to get HP off dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		off_valueL[i] = audio_get_auxadc_value();

	/* Step6. Enable TRIMBUF circuit,
	 * select HPR as TRIMBUF input and set TRIMBUF gain as 18dB
	 */
	/* R Channel */
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPR);

	/* Step7. Request AuxADC to get HP on dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		on_valueR[i] = audio_get_auxadc_value();

	/* Step8. select AUDREFN as TRIMBUF input */
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_AU_REFN);

	/* Step9. Request AuxADC to get HP off dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		off_valueR[i] = audio_get_auxadc_value();

	/* Step10. turn off buffer */
	EnableTrimbuffer(false);
	OpenTrimBufferHardware(false, true);

	mHplTrimOffset = calculate_trim_result(on_valueL, off_valueL,
					       TRIM_TIMES,
					       TRIM_DISCARD_NUM,
					       TRIM_USEFUL_NUM);
	mHprTrimOffset = calculate_trim_result(on_valueR, off_valueR,
					       TRIM_TIMES,
					       TRIM_DISCARD_NUM,
					       TRIM_USEFUL_NUM);
	pr_debug("%s(), channeL = %d, channeR = %d\n", __func__, mHplTrimOffset,
		 mHprTrimOffset);

#endif
}

static int get_spk_trim_offset(int channel)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int on_value[TRIM_TIMES];
	int off_value[TRIM_TIMES];
	int offset = 0;
	int i;

	Ana_Set_Reg(AUXADC_CON10, AUXADC_AVG_256, 0x7);

	/* Step1. turn on buffer */
	OpenTrimBufferHardware_withLO(true, true);

	/* Step2. Enable TRIMBUF circuit and set TRIMBUF gain as 18dB */
	setOffsetTrimMux(channel);
	setOffsetTrimBufferGain(3); /* 18db */
	EnableTrimbuffer(true);

	/* Step3. Request AuxADC to get HP on dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		on_value[i] = audio_get_auxadc_value();

	/* Step4. select AUDREFN as TRIMBUF input */
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_AU_REFN);

	/* Step5. Request AuxADC to get HP off dc value */
	usleep_range(1 * 1000, 10 * 1000);
	for (i = 0; i < TRIM_TIMES; i++)
		off_value[i] = audio_get_auxadc_value();

	/* Step6. turn off buffer */
	EnableTrimbuffer(false);
	OpenTrimBufferHardware_withLO(false, true);

	offset = calculate_trim_result(on_value, off_value, TRIM_TIMES,
				       TRIM_DISCARD_NUM, TRIM_USEFUL_NUM);
	pr_debug("%s(), channel = %d, offset = %d\n",
		__func__, channel, offset);

	return offset;
#endif
}
#ifdef ANALOG_HPTRIM

#define HPTRIM_L_SHIFT 0
#define HPTRIM_R_SHIFT 4
#define HPFINETRIM_L_SHIFT 8
#define HPFINETRIM_R_SHIFT 10
#define HPTRIM_EN_SHIFT 12

#define HPTRIM_L_MASK (0xf << HPTRIM_L_SHIFT)
#define HPTRIM_R_MASK (0xf << HPTRIM_R_SHIFT)
#define HPFINETRIM_L_MASK (0x3 << HPFINETRIM_L_SHIFT)
#define HPFINETRIM_R_MASK (0x3 << HPFINETRIM_R_SHIFT)
#define HPTRIM_EN_MASK (0x1 << HPTRIM_EN_SHIFT)

static int pick_hp_finetrim(int offset_base,
			    int offset_finetrim_1,
			    int offset_finetrim_3)
{
	if (abs(offset_base) < abs(offset_finetrim_1)) {
		if (abs(offset_base) < abs(offset_finetrim_3))
			return 0x0;
		else
			return 0x3;
	} else {
		if (abs(offset_finetrim_1) < abs(offset_finetrim_3))
			return 0x1;
		else
			return 0x3;
	}
}

static int pick_spk_finetrim(int offset_base,
			     int offset_finetrim_2,
			     int offset_finetrim_3)
{
	if (abs(offset_base) < abs(offset_finetrim_2)) {
		if (abs(offset_base) < abs(offset_finetrim_3))
			return 0x0;
		else
			return 0x3;
	} else {
		if (abs(offset_finetrim_2) < abs(offset_finetrim_3))
			return 0x2;
		else
			return 0x3;
	}
}

static void set_lr_trim_code(void)
{
	int hpl_base = 0, hpr_base = 0;
	int hpl_min = 0, hpr_min = 0;
	int hpl_ceiling = 0, hpr_ceiling = 0;
	int hpl_floor = 0, hpr_floor = 0;
	int hpl_finetrim_1 = 0, hpr_finetrim_1 = 0;
	int hpl_finetrim_3 = 0, hpr_finetrim_3 = 0;
	int trimcodel = 0, trimcoder = 0;
	int trimcodel_ceiling = 0, trimcoder_ceiling = 0;
	int trimcodel_floor = 0, trimcoder_floor = 0;
	int finetriml = 0, finetrimr = 0;
	int trimcode_tmpl = 0, trimcode_tmpr = 0;
	int tmp = 0;
	bool code_change = false;

	pr_debug("%s(), Start DCtrim Calibrating, AUDDEC_ELR_0 = 0x%x\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0));

	Ana_Set_Reg(AUDDEC_ELR_0, 0x1 << HPTRIM_EN_SHIFT, HPTRIM_EN_MASK);
	hp_3pole_anaoffset.enable = 1;
	hp_4pole_anaoffset.enable = 1;
	/* channel L */
	Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPTRIM_L_SHIFT, HPTRIM_L_MASK);
	Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPFINETRIM_L_SHIFT, HPFINETRIM_L_MASK);
	/* channel R */
	Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPTRIM_R_SHIFT, HPTRIM_R_MASK);
	Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPFINETRIM_R_SHIFT, HPFINETRIM_R_MASK);
	pr_debug("%s(), AUDDEC_ELR_0 = 0x%x\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
	get_hp_trim_offset();
	hpl_base = mHplTrimOffset;
	hpr_base = mHprTrimOffset;
	mdelay(10);

	/* Step1: get trim code */
	if (hpl_base == 0)
		goto EXIT;
	if (hpr_base == 0)
		goto EXIT;

	if (hpl_base > 0 || hpr_base > 0) {
		if (hpl_base > 0) {
			Ana_Set_Reg(AUDDEC_ELR_0, 0x2 << HPTRIM_L_SHIFT,
				    HPTRIM_L_MASK);
			code_change = true;
		}
		if (hpr_base > 0) {
			Ana_Set_Reg(AUDDEC_ELR_0, 0x2 << HPTRIM_R_SHIFT,
				    HPTRIM_R_MASK);
			code_change = true;
		}

		pr_debug("%s(), step1 > 0 set 4 level AUDDEC_ELR_0 = 0x%x  trimcode(L/R) = %d/%d\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0),
			 trimcodel, trimcoder);
		if (code_change) {
			get_hp_trim_offset();
			code_change  = false;
			hpl_min = mHplTrimOffset;
			hpr_min = mHprTrimOffset;

			Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPTRIM_L_SHIFT,
				    HPTRIM_L_MASK);
			Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPTRIM_R_SHIFT,
				    HPTRIM_R_MASK);
			mdelay(10);

			/* Check floor & ceiling to avoid rounding error */
			if (hpl_base > 0) {
				trimcodel_floor = (abs(hpl_base)*3) /
						  (abs(hpl_base-hpl_min));
				trimcodel_ceiling = trimcodel_floor + 1;
			}
			if (hpr_base > 0) {
				trimcoder_floor = (abs(hpr_base)*3) /
						  (abs(hpr_base-hpr_min));
				trimcoder_ceiling = trimcoder_floor + 1;
			}
		}
	}
	if (hpl_base < 0 || hpr_base < 0) {
		if (hpl_base < 0) {
			Ana_Set_Reg(AUDDEC_ELR_0, 0xa << HPTRIM_L_SHIFT,
				    HPTRIM_L_MASK);
			code_change = true;
		}
		if (hpr_base < 0) {
			Ana_Set_Reg(AUDDEC_ELR_0, 0xa << HPTRIM_R_SHIFT,
				    HPTRIM_R_MASK);
			code_change = true;
		}

		pr_debug("%s(), step1 < 0 set 4 level AUDDEC_ELR_0 = 0x%x  trimcode(L/R) = %d/%d\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0),
			 trimcodel, trimcoder);
		if (code_change) {
			get_hp_trim_offset();
			code_change  = false;
			hpl_min = mHplTrimOffset;
			hpr_min = mHprTrimOffset;

			Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPTRIM_L_SHIFT,
				    HPTRIM_L_MASK);
			Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPTRIM_R_SHIFT,
				    HPTRIM_R_MASK);
			mdelay(10);
			/* Check floor & ceiling to avoid rounding error */
			if (hpl_base < 0) {
				trimcodel_floor = (abs(hpl_base)*3) /
						  (abs(hpl_base-hpl_min)) + 8;
				trimcodel_ceiling = trimcodel_floor + 1;
			}
			if (hpr_base < 0) {
				trimcoder_floor = (abs(hpr_base)*3) /
						  (abs(hpr_base-hpr_min)) + 8;
				trimcoder_ceiling = trimcoder_floor + 1;
			}
		}
	}

	/* Get the best trim code from floor and ceiling value */
	/* Get floor trim code */
	Ana_Set_Reg(AUDDEC_ELR_0, trimcodel_floor << HPTRIM_L_SHIFT,
		    HPTRIM_L_MASK);
	Ana_Set_Reg(AUDDEC_ELR_0, trimcoder_floor << HPTRIM_R_SHIFT,
		    HPTRIM_R_MASK);
	get_hp_trim_offset();
	hpl_floor = mHplTrimOffset;
	hpr_floor = mHprTrimOffset;
	mdelay(10);
	/* Get ceiling trim code */
	Ana_Set_Reg(AUDDEC_ELR_0, trimcodel_ceiling << HPTRIM_L_SHIFT,
		    HPTRIM_L_MASK);
	Ana_Set_Reg(AUDDEC_ELR_0, trimcoder_ceiling << HPTRIM_R_SHIFT,
		    HPTRIM_R_MASK);
	get_hp_trim_offset();
	hpl_ceiling = mHplTrimOffset;
	hpr_ceiling = mHprTrimOffset;
	mdelay(10);
	/* Choose the best */
	if (abs(hpl_ceiling) < abs(hpl_floor)) {
		hpl_base = hpl_ceiling;
		trimcodel = trimcodel_ceiling;
	} else {
		hpl_base = hpl_floor;
		trimcodel = trimcodel_floor;
	}
	if (abs(hpr_ceiling) < abs(hpr_floor)) {
		hpr_base = hpr_ceiling;
		trimcoder = trimcoder_ceiling;
	} else {
		hpr_base = hpr_floor;
		trimcoder = trimcoder_floor;
	}

	Ana_Set_Reg(AUDDEC_ELR_0, trimcodel << HPTRIM_L_SHIFT, HPTRIM_L_MASK);
	Ana_Set_Reg(AUDDEC_ELR_0, trimcoder << HPTRIM_R_SHIFT, HPTRIM_R_MASK);
	pr_debug("%s(), step1 result AUDDEC_ELR_0 = 0x%x  hp_base(L/R) = %d/%d, trimcode(L/R) = %d/%d\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0),
		 hpl_base, hpr_base, trimcodel, trimcoder);

	/* Step2: Trim code refine +1/0/-1 */
	trimcode_tmpl = trimcodel;
	trimcode_tmpr = trimcoder;
	mdelay(10);
	if (hpl_base == 0)
		goto EXIT;
	if (hpr_base == 0)
		goto EXIT;

	if (hpl_base > 0 || hpr_base > 0) {
		if ((hpl_base > 0) &&
		    (trimcodel != 0x7) && (trimcodel != 0x8)) {
			tmp = trimcodel + ((trimcodel > 7) ? -1 : 1);
			Ana_Set_Reg(AUDDEC_ELR_0, tmp << HPTRIM_L_SHIFT,
				    HPTRIM_L_MASK);
			code_change = true;
		}
		if ((hpr_base > 0) &&
		    (trimcoder != 0x7) && (trimcoder != 0x8)) {
			tmp = trimcoder + ((trimcoder > 7) ? -1 : 1);
			Ana_Set_Reg(AUDDEC_ELR_0, tmp << HPTRIM_R_SHIFT,
				    HPTRIM_R_MASK);
			code_change = true;
		}
		pr_debug("%s(), step2 > 0 AUDDEC_ELR_0 = 0x%x  trimcode_tmp = %d\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0), tmp);
		if (code_change) {
			get_hp_trim_offset();
			code_change = false;
			hpl_min = mHplTrimOffset;
			hpr_min = mHprTrimOffset;

			mdelay(10);
			if ((hpl_base > 0) &&
			    (hpl_min >= 0 || abs(hpl_min) < abs(hpl_base))) {
				if ((trimcodel != 0x7) && (trimcodel != 0x8)) {
					trimcode_tmpl =
						trimcodel +
						((trimcodel > 7) ? -1 : 1);
				} else {
					trimcode_tmpl = trimcodel;
					pr_debug("%s(), [Step2][L>0, bit-overflow!!], don't refine, trimcodel = %d\n",
						 __func__, trimcodel);
				}
			}
			if ((hpr_base > 0) &&
			    (hpr_min >= 0 || abs(hpr_min) < abs(hpr_base))) {
				if ((trimcoder != 0x7) && (trimcoder != 0x8)) {
					trimcode_tmpr =
						trimcoder +
						((trimcoder > 7) ? -1 : 1);
				} else {
					trimcode_tmpr = trimcoder;
					pr_debug("%s(), [Step2][R>0, bit-overflow!!], don't refine, trimcoder = %d\n",
						 __func__, trimcoder);
				}
			}
		}
	}
	Ana_Set_Reg(AUDDEC_ELR_0, trimcodel << HPTRIM_L_SHIFT, HPTRIM_L_MASK);
	Ana_Set_Reg(AUDDEC_ELR_0, trimcoder << HPTRIM_R_SHIFT, HPTRIM_R_MASK);

	if (hpl_base < 0 || hpr_base < 0) {
		if ((hpl_base < 0) && (trimcodel != 0) && (trimcodel != 0xf)) {
			tmp = trimcodel - ((trimcodel > 7) ? -1 : 1);
			Ana_Set_Reg(AUDDEC_ELR_0, tmp << HPTRIM_L_SHIFT,
				    HPTRIM_L_MASK);
			code_change = true;
		}
		if ((hpr_base < 0) && (trimcoder != 0) && (trimcoder != 0xf)) {
			tmp = trimcoder - ((trimcoder > 7) ? -1 : 1);
			Ana_Set_Reg(AUDDEC_ELR_0, tmp << HPTRIM_R_SHIFT,
				    HPTRIM_R_MASK);
			code_change = true;
		}
		pr_debug("%s(), step2 < 0 AUDDEC_ELR_0 = 0x%x trimcode_tmp(L/R) = %d/%d\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0),
			 trimcode_tmpl, trimcode_tmpr);
		if (code_change) {
			get_hp_trim_offset();
			code_change = false;
			hpl_min = mHplTrimOffset;
			hpr_min = mHprTrimOffset;
			mdelay(10);
			if ((hpl_base < 0) &&
			    (hpl_min <= 0 || abs(hpl_min) < abs(hpl_base))) {
				if ((trimcodel != 0) && (trimcodel != 0xf)) {
					trimcode_tmpl =
						trimcodel -
						((trimcodel > 7) ? -1 : 1);
				} else {
					trimcode_tmpl = trimcodel;
					pr_debug("%s(), [Step2][L<0, bit-overflow!!], don't refine, trimcodel = %d\n",
						 __func__, trimcodel);
				}
			}
			if ((hpr_base < 0) &&
			    (hpr_min <= 0 || abs(hpr_min) < abs(hpr_base))) {
				if ((trimcoder != 0) && (trimcoder != 0xf)) {
					trimcode_tmpr =
						trimcoder -
						((trimcoder > 7) ? -1 : 1);
				} else {
					trimcode_tmpr = trimcoder;
					pr_debug("%s(), [Step2][R<0, bit-overflow!!], don't refine, trimcoder = %d\n",
						 __func__, trimcoder);
				}
			}
		}
	}

	trimcodel = trimcode_tmpl;
	trimcoder = trimcode_tmpr;
	/* channel L */
	Ana_Set_Reg(AUDDEC_ELR_0, trimcodel << HPTRIM_L_SHIFT, HPTRIM_L_MASK);
	/* channel R */
	Ana_Set_Reg(AUDDEC_ELR_0, trimcoder << HPTRIM_R_SHIFT, HPTRIM_R_MASK);
	pr_debug("%s(), step2 result AUDDEC_ELR_0 = 0x%x trimcode(L/R) = %d/%d\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0), trimcodel, trimcoder);

	/*Step3: Trim code fine tune*/
	get_hp_trim_offset();
	hpl_base = mHplTrimOffset;
	hpr_base = mHprTrimOffset;
	mdelay(10);

	if (hpl_base == 0)
		goto EXIT;
	if (hpr_base == 0)
		goto EXIT;
	if (hpl_base > 0 || hpr_base > 0) {
		if (hpl_base > 0) {
			Ana_Set_Reg(AUDDEC_ELR_0, 0x1 << HPFINETRIM_L_SHIFT,
				    HPFINETRIM_L_MASK);
			code_change = true;
		}
		if (hpr_base > 0) {
			Ana_Set_Reg(AUDDEC_ELR_0, 0x1 << HPFINETRIM_R_SHIFT,
				    HPFINETRIM_R_MASK);
			code_change = true;
		}
		pr_debug("%s(), step3 > 0 AUDDEC_ELR_0 = 0x%x\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
		if (code_change) {
			get_hp_trim_offset();
			code_change = false;
			hpl_finetrim_1 = mHplTrimOffset;
			hpr_finetrim_1 = mHprTrimOffset;
			Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPFINETRIM_L_SHIFT,
				    HPFINETRIM_L_MASK);
			Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPFINETRIM_R_SHIFT,
				    HPFINETRIM_R_MASK);
			mdelay(10);
			if ((hpl_base > 0) &&
			    (hpl_finetrim_1 >= 0 &&
			     abs(hpl_finetrim_1) < abs(hpl_base)))
				finetriml = 0x1;
			if ((hpr_base > 0) &&
			    (hpr_finetrim_1 >= 0 &&
			     abs(hpr_finetrim_1) < abs(hpr_base)))
				finetrimr = 0x1;
			if (hpl_finetrim_1 < 0 || hpr_finetrim_1 < 0) {
				/* base and finetrim=1 across zero. */
				/* Choose base, finetrim=1, and finetrim=3 */
				if (hpl_finetrim_1 < 0  && hpl_base > 0) {
					/* channel L */
					Ana_Set_Reg(AUDDEC_ELR_0,
						    0x3 << HPFINETRIM_L_SHIFT,
						    HPFINETRIM_L_MASK);
					code_change = true;
				}
				if (hpr_finetrim_1 < 0  && hpr_base > 0) {
					/* channel R */
					Ana_Set_Reg(AUDDEC_ELR_0,
						    0x3 << HPFINETRIM_R_SHIFT,
						    HPFINETRIM_R_MASK);
					code_change = true;
				}
				pr_debug("%s(), step3_2 > 0 AUDDEC_ELR_0 = 0x%x\n",
					 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
				if (code_change) {
					get_hp_trim_offset();
					code_change = false;
					hpl_finetrim_3 = mHplTrimOffset;
					hpr_finetrim_3 = mHprTrimOffset;
					Ana_Set_Reg(AUDDEC_ELR_0,
						    0x0 << HPFINETRIM_L_SHIFT,
						    HPFINETRIM_L_MASK);
					Ana_Set_Reg(AUDDEC_ELR_0,
						    0x0 << HPFINETRIM_R_SHIFT,
						    HPFINETRIM_R_MASK);
					mdelay(10);

					if (hpl_base > 0) {
						finetriml =
						pick_hp_finetrim(hpl_base,
							hpl_finetrim_1,
							hpl_finetrim_3);
						pr_debug("%s(), [Step3] refine finetriml = %d\n",
							 __func__, finetriml);
					}
					if (hpr_base > 0) {
						finetrimr =
						pick_hp_finetrim(hpr_base,
							hpr_finetrim_1,
							hpr_finetrim_3);
						pr_debug("%s(), [Step3] refine finetrimr = %d\n",
							 __func__, finetrimr);
					}
				}
			}
		}
	}
	if (hpl_base < 0 || hpr_base < 0) {
		if (hpl_base < 0) {
			Ana_Set_Reg(AUDDEC_ELR_0, 0x2 << HPFINETRIM_L_SHIFT,
				    HPFINETRIM_L_MASK);
			code_change = true;
		}
		if (hpr_base < 0) {
			Ana_Set_Reg(AUDDEC_ELR_0, 0x2 << HPFINETRIM_R_SHIFT,
				    HPFINETRIM_R_MASK);
			code_change = true;
		}
		pr_debug("%s(), step3 < 0 AUDDEC_ELR_0 = 0x%x\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
		if (code_change) {
			get_hp_trim_offset();
			code_change = false;
			hpl_min = mHplTrimOffset;
			hpr_min = mHprTrimOffset;
			Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPFINETRIM_L_SHIFT,
				    HPFINETRIM_L_MASK);
			Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << HPFINETRIM_R_SHIFT,
				    HPFINETRIM_R_MASK);
			mdelay(10);
			if ((hpl_base < 0) &&
			    (hpl_min <= 0 || abs(hpl_min) < abs(hpl_base)))
				finetriml = 0x2;
			if ((hpr_base < 0) &&
			    (hpr_min <= 0 || abs(hpr_min) < abs(hpr_base)))
				finetrimr = 0x2;
		}
	}
	/* channel L */
	Ana_Set_Reg(AUDDEC_ELR_0, finetriml << HPFINETRIM_L_SHIFT,
		    HPFINETRIM_L_MASK);
	Ana_Set_Reg(AUDDEC_ELR_0, finetrimr << HPFINETRIM_R_SHIFT,
		    HPFINETRIM_R_MASK);
	pr_debug("%s(), step3 result AUDDEC_ELR_0 = 0x%x\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0));

EXIT:

	/* 4 pole fine trim */
	get_hp_trim_offset();
	hpl_min = mHplTrimOffset;
	hpr_min = mHprTrimOffset;
	mdelay(10);
	hp_3pole_anaoffset.hpl_trimecode = trimcodel;
	hp_3pole_anaoffset.hpl_finetrim = finetriml;
	hp_3pole_anaoffset.hpr_trimecode = trimcoder;
	hp_3pole_anaoffset.hpr_finetrim = finetrimr;

	/* check trimcode is valid */
	if ((trimcodel < 0 || trimcodel > 0xf) ||
	    (finetriml < 0 || finetriml > 0x3) ||
	    (trimcoder < 0 || trimcoder > 0xf) ||
	    (finetrimr < 0 || finetrimr > 0x3))
		pr_info("%s(), [Warning], invalid trimcode(3pole), trimcodel = %d, finetriml = %d, trimcoder = %d, finetrimr = %d\n",
			__func__, trimcodel, finetriml, trimcoder, finetrimr);

	if ((hpl_min < 0) && (finetriml == 0x0)) {
		finetriml = 0x2;
	} else if ((hpl_min < 0) && (finetriml == 0x2)) {
		if ((trimcodel != 0) && (trimcodel != 0xf)) {
			finetriml = 0x0;
			trimcodel = trimcodel - ((trimcodel > 7) ? -1 : 1);
		} else {
			pr_debug("%s(), [Step4][bit-overflow!!], don't refine, trimcodel = %d, finetriml = %d\n",
				 __func__, trimcodel, finetriml);
		}
	}
	if ((hpr_min < 0) && (finetrimr == 0x0)) {
		finetrimr = 0x2;
	} else if ((hpr_min < 0) && (finetrimr == 0x2)) {
		if ((trimcoder != 0) && (trimcoder != 0xf)) {
			finetrimr = 0x0;
			trimcoder = trimcoder - ((trimcoder > 7) ? -1 : 1);
		} else {
			pr_debug("%s(), [Step4][bit-overflow!!], don't refine, trimcoder = %d, finetrimr = %d\n",
				 __func__, trimcoder, finetrimr);
		}
	}
	hp_4pole_anaoffset.hpl_trimecode = trimcodel;
	hp_4pole_anaoffset.hpl_finetrim = finetriml;
	hp_4pole_anaoffset.hpr_trimecode = trimcoder;
	hp_4pole_anaoffset.hpr_finetrim = finetrimr;

	/* check trimcode is valid */
	if ((trimcodel < 0 || trimcodel > 0xf) ||
	    (finetriml < 0 || finetriml > 0x3) ||
	    (trimcoder < 0 || trimcoder > 0xf) ||
	    (finetrimr < 0 || finetrimr > 0x3))
		pr_info("%s(), [Warning], invalid trimcode(4pole), trimcodel = %d, finetriml = %d, trimcoder = %d, finetrimr = %d\n",
			__func__, trimcodel, finetriml, trimcoder, finetrimr);

	hp_3_pole_trim_setting =
		(hp_3pole_anaoffset.enable << HPTRIM_EN_SHIFT) |
		(hp_3pole_anaoffset.hpr_finetrim << HPFINETRIM_R_SHIFT) |
		(hp_3pole_anaoffset.hpl_finetrim << HPFINETRIM_L_SHIFT) |
		(hp_3pole_anaoffset.hpr_trimecode << HPTRIM_R_SHIFT) |
		(hp_3pole_anaoffset.hpl_trimecode << HPTRIM_L_SHIFT);

	hp_4_pole_trim_setting =
		(hp_4pole_anaoffset.enable << HPTRIM_EN_SHIFT) |
		(hp_4pole_anaoffset.hpr_finetrim << HPFINETRIM_R_SHIFT) |
		(hp_4pole_anaoffset.hpl_finetrim << HPFINETRIM_L_SHIFT) |
		(hp_4pole_anaoffset.hpr_trimecode << HPTRIM_R_SHIFT) |
		(hp_4pole_anaoffset.hpl_trimecode << HPTRIM_L_SHIFT);

	pr_debug("%s(), Final result AUDDEC_ELR_0 = 0x%x, hp_3pole_anaoffset= 0x%x, hp_4pole_anaoffset= 0x%x\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0),
		 hp_3_pole_trim_setting, hp_4_pole_trim_setting);
	pr_debug("%s(), get hp offset L:%d, R:%d\n",
		 __func__, mHplTrimOffset, mHprTrimOffset);
}

static void set_lr_trim_code_spk(int channel)
{
	int hpl_base = 0;
	int hpl_min = 0;
	int hpl_ceiling = 0;
	int hpl_floor = 0;
	int hpl_finetrim_2 = 0;
	int hpl_finetrim_3 = 0;
	int trimcode = 0;
	int trimcode_ceiling = 0;
	int trimcode_floor = 0;
	int finetrim = 0;
	int trimcode_tmp = 0;
	int trim_shift = 0, trim_mask = 0;
	int fine_shift = 0, fine_mask = 0;

	if (channel == AUDIO_OFFSET_TRIM_MUX_HPL) {
		trim_shift = HPTRIM_L_SHIFT;
		trim_mask = HPTRIM_L_MASK;
		fine_shift = HPFINETRIM_L_SHIFT;
		fine_mask = HPFINETRIM_L_MASK;
	} else if (channel == AUDIO_OFFSET_TRIM_MUX_HPR) {
		trim_shift = HPTRIM_R_SHIFT;
		trim_mask = HPTRIM_R_MASK;
		fine_shift = HPFINETRIM_R_SHIFT;
		fine_mask = HPFINETRIM_R_MASK;
	}
	pr_debug("%s(), Start DCtrim Calibrating, channel = %d, AUDDEC_ELR_0 = 0x%x\n",
		 __func__, channel, Ana_Get_Reg(AUDDEC_ELR_0));

	/* Step1: get trim code */
	Ana_Set_Reg(AUDDEC_ELR_0, 0x1 << HPTRIM_EN_SHIFT, HPTRIM_EN_MASK);
	spk_3pole_anaoffset.enable = 0x1;
	spk_4pole_anaoffset.enable = 0x1;

	Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << trim_shift, trim_mask);
	Ana_Set_Reg(AUDDEC_ELR_0, 0x0 << fine_shift, fine_mask);
	pr_debug("%s(), AUDDEC_ELR_0 = 0x%x\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
	hpl_base = get_spk_trim_offset(channel);
	mdelay(10);
	if (hpl_base == 0)
		goto EXIT;

	if (hpl_base > 0) {
		Ana_Set_Reg(AUDDEC_ELR_0, 0x2 << trim_shift, trim_mask);
		pr_debug("%s(), step1 > 0 AUDDEC_ELR_0 = 0x%x  trimcode = %d\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0), trimcode);
		hpl_min = get_spk_trim_offset(channel);
		mdelay(10);

		/* Check floor and ceiling value to avoid rounding error */
		trimcode_floor = (abs(hpl_base)*3)/abs(hpl_base-hpl_min);
		trimcode_ceiling = trimcode_floor + 1;
		pr_debug("%s(), step1 > 0, get trim level trimcode_floor = %d, trimcode_ceiling = %d\n",
			__func__, trimcode_floor, trimcode_ceiling);

	} else {
		Ana_Set_Reg(AUDDEC_ELR_0, 0xa << trim_shift, trim_mask);
		pr_debug("%s(), step1 < 0 AUDDEC_ELR_0 = 0x%x  trimcode = %d\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0), trimcode);
		hpl_min = get_spk_trim_offset(channel);
		mdelay(10);

		/* Check floor and ceiling value to avoid rounding error */
		trimcode_floor = (abs(hpl_base)*3)/abs(hpl_base-hpl_min) + 8;
		trimcode_ceiling = trimcode_floor + 1;
		pr_debug("%s(), step1 < 0, get trim level trimcode_floor = %d, trimcode_ceiling = %d\n",
			 __func__, trimcode_floor, trimcode_ceiling);
	}

	/* Get the best trim code from floor and ceiling value */
	/* Get floor trim code */
	Ana_Set_Reg(AUDDEC_ELR_0, trimcode_floor << trim_shift, trim_mask);
	pr_debug("%s(), step1 floor AUDDEC_ELR_0 = 0x%x  trimcode_floor = %d\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0), trimcode_floor);
	hpl_floor = get_spk_trim_offset(channel);
	mdelay(10);

	/* Get ceiling trim code */
	Ana_Set_Reg(AUDDEC_ELR_0, trimcode_ceiling << trim_shift, trim_mask);
	pr_debug("%s(), step1 floor AUDDEC_ELR_0 = 0x%x  trimcode_ceiling = %d\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0), trimcode_ceiling);
	hpl_ceiling = get_spk_trim_offset(channel);
	mdelay(10);

	/* Choose the best */
	if (abs(hpl_ceiling) < abs(hpl_floor)) {
		hpl_base = hpl_ceiling;
		trimcode = trimcode_ceiling;
	} else {
		hpl_base = hpl_floor;
		trimcode = trimcode_floor;
	}
	Ana_Set_Reg(AUDDEC_ELR_0, trimcode << trim_shift, trim_mask);
	pr_debug("%s(), step1 result AUDDEC_ELR_0 = 0x%x, hp_base = %d, trimcode = %d\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0), hpl_base, trimcode);

	/* Step2: Trim code refine +1/0/-1 */
	hpl_base = get_spk_trim_offset(channel);
	mdelay(10);

	if (hpl_base == 0)
		goto EXIT;

	if (hpl_base > 0) {
		if ((trimcode != 0x7) && (trimcode != 0x8)) {
			trimcode_tmp = trimcode + ((trimcode > 7) ? -1 : 1);
			Ana_Set_Reg(AUDDEC_ELR_0, trimcode_tmp << trim_shift,
				    trim_mask);
			pr_debug("%s(), step2 > 0 AUDDEC_ELR_0 = 0x%x  trimcode_tmp = %d\n",
				 __func__, Ana_Get_Reg(AUDDEC_ELR_0),
				 trimcode_tmp);
			hpl_min = get_spk_trim_offset(channel);
			mdelay(10);
			if (hpl_min >= 0 ||  abs(hpl_min) < abs(hpl_base)) {
				trimcode = trimcode_tmp;
				hpl_base = hpl_min;
			}
		}
	} else {
		if ((trimcode != 0) && (trimcode != 0xf)) {
			trimcode_tmp = trimcode - ((trimcode > 7) ? -1 : 1);
			Ana_Set_Reg(AUDDEC_ELR_0, trimcode_tmp << trim_shift,
				    trim_mask);
			pr_debug("%s(), step2 < 0 AUDDEC_ELR_0 = 0x%x trimcode_tmp = %d\n",
				 __func__, Ana_Get_Reg(AUDDEC_ELR_0),
				 trimcode_tmp);
			hpl_min = get_spk_trim_offset(channel);
			mdelay(10);
			if (hpl_min <= 0 ||  abs(hpl_min) < abs(hpl_base)) {
				trimcode = trimcode_tmp;
				hpl_base = hpl_min;
			}
		}
	}
	Ana_Set_Reg(AUDDEC_ELR_0, trimcode << trim_shift, trim_mask);
	pr_debug("%s(), step2 result AUDDEC_ELR_0 = 0x%x trimcode = %d\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0), trimcode);

	/* Step3: Trim code fine tune */
	mdelay(10);

	if (hpl_base == 0)
		goto EXIT;
	if (hpl_base > 0) {
		Ana_Set_Reg(AUDDEC_ELR_0, 0x1 << fine_shift, fine_mask);
		pr_debug("%s(), step3 > 0 AUDDEC_ELR_0 = 0x%x\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
		hpl_min = get_spk_trim_offset(channel);
		mdelay(10);

		if (hpl_min >= 0 || abs(hpl_min) < abs(hpl_base)) {
			finetrim = 0x1;
			hpl_base = hpl_min;
		} else {
			Ana_Set_Reg(AUDDEC_ELR_0, 0x3 << fine_shift, fine_mask);
			pr_debug("%s(), step3_2 > 0 AUDDEC_ELR_0 = 0x%x\n ",
				 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
			hpl_min = get_spk_trim_offset(channel);
			mdelay(10);
			if (hpl_min >= 0 && abs(hpl_min) < abs(hpl_base)) {
				finetrim = 0x3;
				hpl_base = hpl_min;
			}
		}
	}	else {
		/* SPK+HP finetrim=3 compensates positive DC value */
		/* choose the best fine trim */
		Ana_Set_Reg(AUDDEC_ELR_0, 0x2 << fine_shift, fine_mask);
		pr_debug("%s(), step3 < 0 AUDDEC_ELR_0 = 0x%x\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
		hpl_finetrim_2 = get_spk_trim_offset(channel);
		mdelay(10);
		if (hpl_finetrim_2 <= 0 &&
		    abs(hpl_finetrim_2) < abs(hpl_base)) {
			finetrim = 0x2;
			hpl_base = hpl_finetrim_2;
		} else {
			/* base and finetrim=2 across zero */
			/* Choose best from base, finetrim=2, and finetrim=3 */
			Ana_Set_Reg(AUDDEC_ELR_0, 0x3 << fine_shift, fine_mask);
			pr_debug("%s(), step3_2 < 0 AUDDEC_ELR_0 = 0x%x\n ",
				 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
			hpl_finetrim_3 = get_spk_trim_offset(channel);
			mdelay(10);
			finetrim = pick_spk_finetrim(hpl_base,
						     hpl_finetrim_2,
						     hpl_finetrim_3);
			if (finetrim == 0x2)
				hpl_base = hpl_finetrim_2;
			else if (finetrim == 0x3)
				hpl_base = hpl_finetrim_3;

		}
	}
	Ana_Set_Reg(AUDDEC_ELR_0, finetrim << fine_shift, fine_mask);

EXIT:
	spk_3pole_anaoffset.hpl_trimecode = trimcode;
	spk_3pole_anaoffset.hpl_finetrim = finetrim;
	spk_3pole_anaoffset.hpr_trimecode = hp_3pole_anaoffset.hpr_trimecode;
	spk_3pole_anaoffset.hpr_finetrim = hp_3pole_anaoffset.hpr_finetrim;
	pr_debug("%s(), step3 result AUDDEC_ELR_0 = 0x%x, 3-pole trimcode = %d, finetrim = %d\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0), trimcode, finetrim);

	/* check trimcode is valid */
	if ((trimcode < 0 || trimcode > 0xf) ||
	    (finetrim < 0 || finetrim > 0x3))
		pr_info("%s(), [Warning], invalid trimcode(3pole), trimcode = %d, finetrim = %d\n",
			__func__, trimcode, finetrim);

	/* 4 pole fine trim */
	hpl_base = get_spk_trim_offset(channel);
	mdelay(10);

	if ((hpl_base < 0) && (finetrim == 0x0)) {
		finetrim = 0x2;
	} else if ((hpl_base < 0) && (finetrim == 0x2)) {
		if ((trimcode != 0) && (trimcode != 0xf)) {
			finetrim = 0x0;
			trimcode = trimcode - ((trimcode > 7) ? -1 : 1);
		}
	}

	spk_4pole_anaoffset.hpl_trimecode = trimcode;
	spk_4pole_anaoffset.hpl_finetrim = finetrim;
	spk_4pole_anaoffset.hpr_trimecode = hp_4pole_anaoffset.hpr_trimecode;
	spk_4pole_anaoffset.hpr_finetrim =  hp_4pole_anaoffset.hpr_finetrim;
	pr_debug("%s(), step4 result AUDDEC_ELR_0 = 0x%x, 4-pole trimcode = %d, finetrim = %d\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0), trimcode, finetrim);

	/* check trimcode is valid */
	if ((trimcode < 0 || trimcode > 0xf) ||
	    (finetrim < 0 || finetrim > 0x3))
		pr_info("%s(), [Warning], invalid trimcode(4pole), trimcode = %d, finetrim = %d\n",
			__func__, trimcode, finetrim);

	spk_hp_3_pole_trim_setting =
		(spk_3pole_anaoffset.enable << HPTRIM_EN_SHIFT) |
		(spk_3pole_anaoffset.hpr_finetrim << HPFINETRIM_R_SHIFT) |
		(spk_3pole_anaoffset.hpl_finetrim << HPFINETRIM_L_SHIFT) |
		(spk_3pole_anaoffset.hpr_trimecode << HPTRIM_R_SHIFT) |
		(spk_3pole_anaoffset.hpl_trimecode << HPTRIM_L_SHIFT);
	spk_hp_4_pole_trim_setting =
		(spk_4pole_anaoffset.enable << HPTRIM_EN_SHIFT) |
		(spk_4pole_anaoffset.hpr_finetrim << HPFINETRIM_R_SHIFT) |
		(spk_4pole_anaoffset.hpl_finetrim << HPFINETRIM_L_SHIFT) |
		(spk_4pole_anaoffset.hpr_trimecode << HPTRIM_R_SHIFT) |
		(spk_4pole_anaoffset.hpl_trimecode << HPTRIM_L_SHIFT);

	pr_debug("%s(), Final result AUDDEC_ELR_0 = 0x%x, spk_3pole_anaoffset= 0x%x, spk_4pole_anaoffset= 0x%x\n",
		 __func__, Ana_Get_Reg(AUDDEC_ELR_0),
		 spk_hp_3_pole_trim_setting, spk_hp_4_pole_trim_setting);
	pr_debug("%s(), get spkl offset : %d\n",
		 __func__, get_spk_trim_offset(channel));
}
#endif

static void get_hp_lr_trim_offset(void)
{
	pr_debug("%s(), Start DCtrim Calibrating", __func__);
#ifdef ANALOG_HPTRIM
	set_lr_trim_code();
	hpl_dc_offset = mHplTrimOffset;
	hpr_dc_offset = mHprTrimOffset;
	pr_debug("%s(), hpl_dc_offset: %d, hpr_dc_offset: %d\n",
		 __func__, hpl_dc_offset, hpr_dc_offset);

	set_lr_trim_code_spk(AUDIO_OFFSET_TRIM_MUX_HPL);
	spkl_dc_offset = get_spk_trim_offset(AUDIO_OFFSET_TRIM_MUX_HPL);
	pr_debug("%s(), spkl_dc_offset: %d\n", __func__, spkl_dc_offset);
#else
	hpl_dc_offset = get_hp_trim_offset(AUDIO_OFFSET_TRIM_MUX_HPL);
	hpr_dc_offset = get_hp_trim_offset(AUDIO_OFFSET_TRIM_MUX_HPR);
#endif
	udelay(1000);
	dctrim_calibrated = 2;
	pr_debug("%s(), End DCtrim Calibrating", __func__);
}
static int mt63xx_codec_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *Daiport)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		mBlockSampleRate[ANA_DEV_IN_ADC] = substream->runtime->rate;

	else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mBlockSampleRate[ANA_DEV_OUT_DAC] = substream->runtime->rate;

	return 0;
}

static const struct snd_soc_dai_ops mt6323_aif1_dai_ops = {
	.prepare = mt63xx_codec_prepare,
};

static struct snd_soc_dai_driver mtk_6358_dai_codecs[] = {
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
			.formats = (SNDRV_PCM_FMTBIT_U8 |
				    SNDRV_PCM_FMTBIT_S8 |
				    SNDRV_PCM_FMTBIT_U16_LE |
				    SNDRV_PCM_FMTBIT_S16_LE |
				    SNDRV_PCM_FMTBIT_U16_BE |
				    SNDRV_PCM_FMTBIT_S16_BE |
				    SNDRV_PCM_FMTBIT_U24_LE |
				    SNDRV_PCM_FMTBIT_S24_LE |
				    SNDRV_PCM_FMTBIT_U24_BE |
				    SNDRV_PCM_FMTBIT_S24_BE |
				    SNDRV_PCM_FMTBIT_U24_3LE |
				    SNDRV_PCM_FMTBIT_S24_3LE |
				    SNDRV_PCM_FMTBIT_U24_3BE |
				    SNDRV_PCM_FMTBIT_S24_3BE |
				    SNDRV_PCM_FMTBIT_U32_LE |
				    SNDRV_PCM_FMTBIT_S32_LE |
				    SNDRV_PCM_FMTBIT_U32_BE |
				    SNDRV_PCM_FMTBIT_S32_BE),
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
		.name = MT_SOC_CODEC_DEEPBUFFER_TX_DAI_NAME,
		.ops = &mt6323_aif1_dai_ops,
		.playback = {
			.stream_name = MT_SOC_DEEP_BUFFER_DL_STREAM_NAME,
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
		.name = MT_SOC_CODEC_SPKSCPTXDAI_NAME,
		.ops = &mt6323_aif1_dai_ops,
		.playback = {
			.stream_name = MT_SOC_DL1SCPSPK_STREAM_NAME,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 192000,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SND_SOC_ADV_MT_FMTS,
		}
	},
#ifdef _NON_COMMON_FEATURE_READY
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
#endif
	{
		.name = MT_SOC_CODEC_VOICE_USBDAI_NAME,
		.ops = &mt6323_aif1_dai_ops,
		.playback = {
			.stream_name = MT_SOC_VOICE_USB_STREAM_NAME,
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SND_SOC_ADV_MT_FMTS,
		},
		.capture = {
			.stream_name = MT_SOC_VOICE_USB_STREAM_NAME,
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SND_SOC_ADV_MT_FMTS,
		},
	},
	{
		.name = MT_SOC_CODEC_VOICE_USB_ECHOREF_DAI_NAME,
		.ops = &mt6323_aif1_dai_ops,
		.playback = {
			.stream_name = MT_SOC_VOICE_USB_ECHOREF_STREAM_NAME,
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats = SND_SOC_ADV_MT_FMTS,
		},
		.capture = {
			.stream_name = MT_SOC_VOICE_USB_ECHOREF_STREAM_NAME,
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
#ifdef _NON_COMMON_FEATURE_READY
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
#endif
};

static void TurnOnDacPower(int device)
{
	pr_debug("%s()\n", __func__);

	/* gpio mosi mode */
	set_playback_gpio(true);

	/* Enable AUDGLB */
	NvregEnable(true);

	if (mIsNeedPullDown) {
		/* Pull-down HPL/R to AVSS28_AUD */
		hp_pull_down(true);
	}
	/* release HP CMFB gate rstb */
	Ana_Set_Reg(AUDDEC_ANA_CON4, 0x1 << 6, 0x1 << 6);

	/* XO_AUDIO_EN_M Enable */
	audckbufEnable(true);

	/* Enable CLKSQ */
	ClsqEnable(true);

	/* Turn on AUDNCP_CLKDIV engine clock, Turn on AUD 26M*/
	Topck_Enable(true);

	usleep_range(250, 270);

	/* Audio system digital clock power down release */
	Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0000, 0x00c5);

	usleep_range(250, 270);

	/* sdm audio fifo clock power on */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffff);
	/* scrambler clock on enable */
	Ana_Set_Reg(AFUNC_AUD_CON0, 0xCBA1, 0xffff);
	/* sdm power on */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffff);
	/* sdm fifo enable */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x000B, 0xffff);

	/* afe enable, dl_lr_swap = 0 */
	Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x4001);

	setDlMtkifSrc(true);
}

static void TurnOffDacPower(void)
{
	pr_debug("%s()\n", __func__);
	setDlMtkifSrc(false);

	/* DL scrambler disabling sequence */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0000, 0xffff);
	Ana_Set_Reg(AFUNC_AUD_CON0, 0xcba0, 0xffff);

	if (GetAdcStatus() == false) {
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
		/* turn off afe */
		/* all power down */
		Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x00ff, 0x00ff);
	} else {
		/* down-link power down */
		Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0040, 0x0040);
	}

	usleep_range(250, 270);

	set_playback_gpio(false);

	Topck_Enable(false);
	ClsqEnable(false);

	/* Set HP CMFB gate rstb */
	Ana_Set_Reg(AUDDEC_ANA_CON4, 0x0, 0x1 << 6);
	if (mIsNeedPullDown) {
		/* disable Pull-down HPL/R to AVSS28_AUD */
		hp_pull_down(false);
	}

	NvregEnable(false);
	audckbufEnable(false);
}

static void setDlMtkifSrc(bool enable)
{
	pr_debug("%s(), enable = %d, freq = %d\n",
		 __func__,
		 enable,
		 mBlockSampleRate[ANA_DEV_OUT_DAC]);
	if (enable) {
		Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x0001, 0xffff);
		/* turn on dl */

		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);
		/* set DL in normal path, not from sine gen table */
	} else {
		Ana_Set_Reg(AFE_DL_SRC2_CON0_L, 0x0000, 0xffff);
		/* bit0, Turn off down-link */
	}
}

static void Audio_Amp_Change(int channels, bool enable)
{
	pr_debug("%s(), enable %d, HSL %d, HSR %d\n",
		 __func__,
		 enable,
		 mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL],
		 mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR]);

#ifdef ANALOG_HPTRIM
	pr_debug("%s(), mic_vinp_mv = %d, dc_compensation_disabled = %d, old trim_setting = 0x%x\n",
		 __func__, mic_vinp_mv,
		 dc_compensation_disabled, Ana_Get_Reg(AUDDEC_ELR_0));

	if (!dc_compensation_disabled) {
		if (mic_vinp_mv > MIC_VINP_4POLE_THRES_MV &&
		   ((codec_debug_enable & DBG_DCTRIM_BYPASS_4POLE) == 0)) {
			Ana_Set_Reg(AUDDEC_ELR_0,
				    hp_4_pole_trim_setting, 0xffff);
			pr_debug("%s(), set hp_4_pole_trim_setting = 0x%x\n",
				 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
		} else {
			Ana_Set_Reg(AUDDEC_ELR_0,
				    hp_3_pole_trim_setting, 0xffff);
			pr_debug("%s(), set hp_3_pole_trim_setting = 0x%x\n",
				 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
		}
	} else {
		Ana_Set_Reg(AUDDEC_ELR_0, 0x0, 0xffff);
		pr_debug("%s(), dc_compensation_disabled, set trim_setting = 0x%x\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0));
	}
#endif

	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower(ANA_DEV_OUT_HEADSETL);

		/* here pmic analog control */
		if (mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL] ==
		    false &&
		    mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR] ==
		    false) {

			/* Disable headphone short-circuit protection */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);
			/* Reduce ESD resistance of AU_REFN */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc000, 0xffff);
			/* Set HPR/HPL gain to -10dB */
			Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_10DB_REG, 0xffff);

			/* Turn on DA_600K_NCP_VA18 */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON1, 0x0001, 0xffff);
			/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON2, 0x002c, 0xffff);
			/* Toggle RG_DIVCKS_CHG */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON0, 0x0001, 0xffff);
			/* Set NCP soft start mode as default mode: 150us */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON4, 0x0002, 0xffff);
			/* Enable NCP */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x0000, 0xffff);
			usleep_range(250, 270);

			/* Enable cap-less LDOs (1.5V) */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x1055, 0x1055);
			/* Enable NV regulator (-1.2V) */
			Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0001, 0xffff);
			usleep_range(100, 120);

			/* Disable AUD_ZCD */
			Hp_Zcd_Enable(false);

			/* Enable IBIST */
			Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

			/* Set HP DR bias current optimization, 010: 6uA */
			Ana_Set_Reg(AUDDEC_ANA_CON11, 0x4900, 0xffff);
			/* Set HP & ZCD bias current optimization */
			/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
			Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);
			/* Set HPP/N STB enhance circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc033, 0xffff);

			/* Enable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x000c, 0xffff);
			/* Enable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x003c, 0xffff);
			/* Enable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0c00, 0xffff);
			/* Enable HP driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x30c0, 0xffff);
			/* Enable HP driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x30f0, 0xffff);
			/* Short HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00fc, 0x00ff);

			/* Enable HP main CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0e00, 0xffff);
			/* Disable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0200, 0xffff);

			/* Select CMFB resistor bulk to AC mode */
			/* Selec HS/LO cap size (6.5pF default) */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0xffff);

			/* Enable HS driver bias circuits */
			/* Disable HS main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
			/* Enable LO driver bias circuits */
			/* Disable LO main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);

			/* Enable HP main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00ff, 0x00ff);
			/* Enable HPR/L main output stage step by step */
			hp_main_output_ramp(true);

			/* Reduce HP aux feedback loop gain */
			hp_aux_feedback_loop_gain_ramp(true);
			/* Disable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fcf, 0xffff);

			/* apply volume setting */
			headset_volume_ramp(DL_GAIN_N_10DB,
				mCodec_data->ana_gain[ANA_GAIN_HPOUTL]);

			/* Disable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fc3, 0xffff);
			/* Unshort HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3f03, 0xffff);
			usleep_range(100, 120);

			/* Enable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);
			/* Enable Audio DAC  */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0xf, 0xf);
			/* Enable low-noise mode of DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x1, 0x1);
			usleep_range(100, 120);

			/* Switch HPL MUX to audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0200, 0x0f00);
			/* Switch HPR MUX to audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0a00, 0x0f00);
#ifndef ANALOG_HPTRIM
			/* Apply digital DC compensation value to DAC */
			SetDcCompenSation(true);
#endif
			/* Disable Pull-down HPL/R to AVSS28_AUD */
			hp_pull_down(false);
		}

	} else {
		if (mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL] ==
		    false &&
		    mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR] ==
		    false) {
			/* Pull-down HPL/R to AVSS28_AUD */
			hp_pull_down(true);
#ifndef ANALOG_HPTRIM
			SetDcCompenSation(false);
#endif
			/* HPR/HPL mux to open */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x0f00);

			/* Disable low-noise mode of DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0000, 0x0001);

			/* Disable Audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x000f);

			/* Disable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1);

			/* Short HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fc3, 0xffff);
			/* Enable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fcf, 0xffff);

			/* decrease HPL/R gain to normal gain step by step */
			headset_volume_ramp(
				mCodec_data->ana_gain[ANA_GAIN_HPOUTL],
				DL_GAIN_N_10DB);

			/* Enable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fff, 0xffff);

			/* Reduce HP aux feedback loop gain */
			hp_aux_feedback_loop_gain_ramp(false);

			/* decrease HPR/L main output stage step by step */
			hp_main_output_ramp(false);

			/* Disable HP main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3);

			/* Enable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0e00, 0x0f00);

			/* Disable HP main CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0c00, 0x0f00);

			/* Unshort HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3 << 6);

			/* Disable HP driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0x3 << 4);

			/* Disable HP driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0x3 << 6);

			/* Disable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0000, 0xffff);

			/* Disable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3 << 4);

			/* Disable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0xffff);

			/* Disable IBIST */
			Ana_Set_Reg(AUDDEC_ANA_CON12, 0x1 << 8, 0x1 << 8);

			/* Disable NV regulator (-1.2V) */
			Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0, 0x1);
			/* Disable cap-less LDOs (1.5V) */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0, 0x1055);
			/* Disable NCP */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x1, 0x1);

			/* Set HPL/HPR gain to mute */
			Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_10DB_REG, 0xffff);

			/* Increase ESD resistance of AU_REFN */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0, 0x1 << 14);

			if (GetDLStatus() == false)
				TurnOffDacPower();
		}
	}
}

static int Audio_AmpL_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL];
	return 0;
}

static int Audio_AmpL_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	pr_debug("%s(): enable = %ld, dev_power[ANA_DEV_OUT_HEADSETL] = %d\n",
		 __func__, ucontrol->value.integer.value[0],
		 mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL]);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL] ==
		false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
		mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL] =
			ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL] ==
		       true)) {
		mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL] =
			ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int Audio_AmpR_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR];
	return 0;
}

static int Audio_AmpR_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	pr_debug("%s(): enable = %ld, dev_power[ANA_DEV_OUT_HEADSETR] = %d\n",
		 __func__, ucontrol->value.integer.value[0],
		 mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR]);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR] ==
		false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
		mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR] =
			ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR] ==
		       true)) {
		mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR] =
			ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int PMIC_REG_CLEAR_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);

	/* receiver downlink */
	set_playback_gpio(true);
	Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1 << 4);
	/* Pull-down HPL/R to AVSS28_AUD */
	hp_pull_down(true);
	/* release HP CMFB gate rstb */
	Ana_Set_Reg(AUDDEC_ANA_CON4, 0x1 << 6, 0x1 << 6);

	Ana_Set_Reg(DCXO_CW14, 0x1 << 13, 0x1 << 13);
	Ana_Set_Reg(AUDENC_ANA_CON6, 0x0001, 0x0003);
	Ana_Set_Reg(AUD_TOP_CKPDN_CON0, 0x0, 0x66);
	usleep_range(250, 270);
	/* Audio system digital clock power down release */
	Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0000, 0x00c5);
	usleep_range(250, 270);

	/* sdm audio fifo clock power on */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffff);
	/* scrambler clock on enable */
	Ana_Set_Reg(AFUNC_AUD_CON0, 0xCBA1, 0xffff);
	/* sdm power on */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffff);
	/* sdm fifo enable */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x000B, 0xffff);

	/* afe enable, dl_lr_swap = 0 */
	Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x4001);

	setDlMtkifSrc(true);

	/* Reduce ESD resistance of AU_REFN */
	Ana_Set_Reg(AUDDEC_ANA_CON2, 0x4000, 0xffff);

	/* Turn on DA_600K_NCP_VA18 */
	Ana_Set_Reg(AUDNCP_CLKDIV_CON1, 0x0001, 0xffff);
	/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
	Ana_Set_Reg(AUDNCP_CLKDIV_CON2, 0x002c, 0xffff);
	/* Toggle RG_DIVCKS_CHG */
	Ana_Set_Reg(AUDNCP_CLKDIV_CON0, 0x0001, 0xffff);
	/* Set NCP soft start mode as default mode: 100us */
	Ana_Set_Reg(AUDNCP_CLKDIV_CON4, 0x0003, 0xffff);
	/* Enable NCP */
	Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x0000, 0xffff);
	usleep_range(250, 270);

	/* Enable cap-less LDOs (1.5V) */
	Ana_Set_Reg(AUDDEC_ANA_CON14, 0x1055, 0x1055);
	/* Enable NV regulator (-1.2V) */
	Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0001, 0xffff);
	usleep_range(100, 120);

	/* Disable AUD_ZCD */
	Hp_Zcd_Enable(false);

	/* Disable handset short-circuit protection */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
	/* Enable IBIST */
	Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);
	/* Set HP DR bias current optimization, 010: 6uA */
	Ana_Set_Reg(AUDDEC_ANA_CON11, 0x4900, 0xffff);
	/* Set HP & ZCD bias current optimization */
	/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
	Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);
	/* Set HS STB enhance circuits */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0090, 0xffff);

	/* Disable HP main CMFB loop */
	Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0000, 0xffff);
	/* Select CMFB resistor bulk to AC mode */
	/* Selec HS/LO cap size (6.5pF default) */
	Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0xffff);

	/* Enable HS driver bias circuits */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0092, 0xffff);
	/* Enable HS driver core circuits */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0093, 0xffff);

	/* Set HS gain to normal gain step by step */
	Ana_Set_Reg(ZCD_CON3,
		    mCodec_data->ana_gain[ANA_GAIN_HSOUTL],
		    0xffff);

	/* Enable AUD_CLK */
	Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);
	/* Enable Audio DAC  */
	Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0009, 0xffff);
	/* Enable low-noise mode of DAC */
	Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0001, 0xffff);
	/* Switch HS MUX to audio DAC */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x009b, 0xffff);

	/* phone mic dcc */

	/* Enable audio ADC CLKGEN  */
	Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1 << 5, 0x1 << 5);
	/* ADC CLK from CLKGEN (13MHz) */
	Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0xffff);
	/* Enable  LCLDO_ENC 1P8V */
	Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0100, 0x2500);
	/* LCLDO_ENC remote sense */
	Ana_Set_Reg(AUDDEC_ANA_CON14, 0x2500, 0x2500);

	Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
	Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
	Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2060, 0xffff);
	Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2061, 0xffff);
	Ana_Set_Reg(AFE_DCCLK_CFG1, 0x0100, 0xffff);

	/* phone mic */
	/* Enable MICBIAS0, MISBIAS0 = 1P9V */
	Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0xffff);

	Ana_Set_Reg(AUDENC_ANA_CON0, 0x4 << 8, 0x0700);
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x4 << 8, 0x0700);

	/* Audio L preamplifier DCC precharge */
	Ana_Set_Reg(AUDENC_ANA_CON0, 0x0004, 0xf8ff);

	/* "ADC1", main_mic */
	/* Audio L preamplifier input sel : AIN0. Enable audio L PGA */
	Ana_Set_Reg(AUDENC_ANA_CON0, 0x0045, 0xf0ff);
	/* Audio L preamplifier DCCEN */
	Ana_Set_Reg(AUDENC_ANA_CON0, 0x1 << 1, 0x1 << 1);
	/* Audio L ADC input sel : L PGA. Enable audio L ADC */
	Ana_Set_Reg(AUDENC_ANA_CON0, 0x5047, 0xf000);

	usleep_range(100, 150);
	/* Audio L preamplifier DCC precharge off */
	Ana_Set_Reg(AUDENC_ANA_CON0, 0x0, 0x1 << 2);

	/* Audio R preamplifier DCC precharge */
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x0004, 0xf8ff);

	/* ref mic */
	/* Audio R preamplifier input sel : AIN2. Enable audio R PGA */
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x00c5, 0xf0ff);
	/* Audio R preamplifier DCCEN */
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x1 << 1, 0x1 << 1);
	/* Audio R ADC input sel : R PGA. Enable audio R ADC */
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x50c7, 0xf000);

	usleep_range(100, 150);
	/* Audio R preamplifier DCC precharge off */
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x0, 0x1 << 2);

	/* Short body to ground in PGA */
	Ana_Set_Reg(AUDENC_ANA_CON3, 0x0, 0x1 << 12);

	/* here to set digital part */

	/* set gpio miso mode */
	set_capture_gpio(true);

	/* power on clock */
	Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0000, 0x00bf);

	/* configure ADC setting */
	Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);

	/* [0] afe enable */
	Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);

	/* MTKAIF TX format setting */
	Ana_Set_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0, 0x0000, 0xffff);

	/* enable aud_pad TX fifos */
	Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x3100, 0xff00);

	/* UL dmic setting */
	Ana_Set_Reg(AFE_UL_SRC_CON0_H, 0x0000, 0xffff);

	/* UL turn on */
	Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0001, 0xffff);

	return 0;
}

static int PMIC_REG_CLEAR_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), not support\n", __func__);

	return 0;
}
#if 0				/* not used */
static void SetVoiceAmpVolume(void)
{
	int index;

	pr_debug("%s\n", __func__);
	index = mCodec_data->ana_gain[ANA_GAIN_HSOUTL];
	Ana_Set_Reg(ZCD_CON3, index, 0x001f);
}
#endif

static void Voice_Amp_Change(bool enable)
{
	if (enable) {
		if (GetDLStatus() == false) {
			TurnOnDacPower(ANA_DEV_OUT_EARPIECEL);
			pr_debug("%s(), amp on\n", __func__);

			/* Reduce ESD resistance of AU_REFN */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x4000, 0xffff);

			/* Turn on DA_600K_NCP_VA18 */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON1, 0x0001, 0xffff);
			/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON2, 0x002c, 0xffff);
			/* Toggle RG_DIVCKS_CHG */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON0, 0x0001, 0xffff);
			/* Set NCP soft start mode as default mode: 100us */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON4, 0x0003, 0xffff);
			/* Enable NCP */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x0000, 0xffff);
			usleep_range(250, 270);

			/* Enable cap-less LDOs (1.5V) */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x1055, 0x1055);
			/* Enable NV regulator (-1.2V) */
			Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0001, 0xffff);
			usleep_range(100, 120);

			/* Disable AUD_ZCD */
			Hp_Zcd_Enable(false);

			/* Disable handset short-circuit protection */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
			/* Enable IBIST */
			Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);
			/* Set HP DR bias current optimization, 010: 6uA */
			Ana_Set_Reg(AUDDEC_ANA_CON11, 0x4900, 0xffff);
			/* Set HP & ZCD bias current optimization */
			/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
			Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);
			/* Set HS STB enhance circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0090, 0xffff);

			/* Disable HP main CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0000, 0xffff);
			/* Select CMFB resistor bulk to AC mode */
			/* Selec HS/LO cap size (6.5pF default) */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0xffff);

			/* Enable HS driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0092, 0xffff);
			/* Enable HS driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0093, 0xffff);

			/* Set HS gain to normal gain step by step */
			Ana_Set_Reg(ZCD_CON3,
				    mCodec_data->ana_gain[ANA_GAIN_HSOUTL],
				    0xffff);

			/* Enable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);
			/* Enable Audio DAC  */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0009, 0xffff);
			/* Enable low-noise mode of DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0001, 0xffff);
			/* Switch HS MUX to audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x009b, 0xffff);
		}
	} else {
		pr_debug("%s(), amp off\n", __func__);
		/* HS mux to open */
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0x3 << 2);

		if (GetDLStatus() == false) {
			/* Disable Audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x000f);

			/* Disable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1);

			/* decrease HS gain to minimum gain step by step */
			Ana_Set_Reg(ZCD_CON3, DL_GAIN_N_40DB, 0xffff);

			/* Disable HS driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0, 0x1);

			/* Disable HS driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0000, 0x1 << 1);

			/* Disable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0, 0xff << 8);

			/* Enable HP main CMFB Switch */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x2 << 8, 0xff << 8);

			/* Disable IBIST */
			Ana_Set_Reg(AUDDEC_ANA_CON12, 0x1 << 8, 0x1 << 8);
			/* Disable NV regulator (-1.2V) */
			Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0, 0x1);
			/* Disable cap-less LDOs (1.5V) */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0, 0x1055);
			/* Disable NCP */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x1, 0x1);

			TurnOffDacPower();
		}
	}
}

static int Voice_Amp_Get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s = %d\n", __func__,
		 mCodec_data->dev_power[ANA_DEV_OUT_EARPIECEL]);
	ucontrol->value.integer.value[0] =
		mCodec_data->dev_power[ANA_DEV_OUT_EARPIECEL];
	return 0;
}

static int Voice_Amp_Set(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	pr_debug("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->dev_power[ANA_DEV_OUT_EARPIECEL] ==
		false)) {
		Voice_Amp_Change(true);
		mCodec_data->dev_power[ANA_DEV_OUT_EARPIECEL] =
			ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->dev_power[ANA_DEV_OUT_EARPIECEL] ==
		       true)) {
		mCodec_data->dev_power[ANA_DEV_OUT_EARPIECEL] =
			ucontrol->value.integer.value[0];
		Voice_Amp_Change(false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static void Speaker_Amp_Change(bool enable)
{
	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower(ANA_DEV_OUT_SPEAKERL);

		pr_debug("%s(), enable %d\n", __func__, enable);

		/* Reduce ESD resistance of AU_REFN */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x4000, 0xffff);

		/* Turn on DA_600K_NCP_VA18 */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON1, 0x0001, 0xffff);
		/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON2, 0x002c, 0xffff);
		/* Toggle RG_DIVCKS_CHG */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON0, 0x0001, 0xffff);
		/* Set NCP soft start mode as default mode: 100us */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON4, 0x0003, 0xffff);
		/* Enable NCP */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x0000, 0xffff);
		usleep_range(250, 270);

		/* Enable cap-less LDOs (1.5V) */
		Ana_Set_Reg(AUDDEC_ANA_CON14, 0x1055, 0x1055);
		/* Enable NV regulator (-1.2V) */
		Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0001, 0xffff);
		usleep_range(100, 120);

		/* Disable AUD_ZCD */
		Hp_Zcd_Enable(false);

		/* Disable linout short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);

		/* Enable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);
		/* Set HP DR bias current optimization, 010: 6uA */
		Ana_Set_Reg(AUDDEC_ANA_CON11, 0x4900, 0xffff);
		/* Set HP & ZCD bias current optimization */
		/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

		/* Set LO STB enhance circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0110, 0xffff);

		/* Disable HP main CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0000, 0xffff);
		/* Select CMFB resistor bulk to AC mode */
		/* Selec HS/LO cap size (6.5pF default) */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0xffff);

		/* Enable LO driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0112, 0xffff);
		/* Enable LO driver core circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0113, 0xffff);

		/* Set LOL gain to normal gain step by step */
		Apply_Speaker_Gain(
			mCodec_data->ana_gain[ANA_GAIN_LINEOUTR]);

		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);
		/* Enable Audio DAC  */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0009, 0xffff);
		/* Enable low-noise mode of DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0001, 0xffff);
		/* Switch LOL MUX to audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x011b, 0xffff);
	} else {
		pr_debug("%s(), enable %d\n", __func__, enable);

		/* LOL mux to open */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0000, 0x3 << 2);

		if (GetDLStatus() == false) {
			/* Disable Audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x000f);

			/* Disable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1);

			/* decrease LOL gain to minimum gain step by step */
			Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_40DB_REG, 0xffff);

			/* Disable LO driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0, 0x1);

			/* Disable LO driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0000, 0x1 << 1);

			/* Disable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0, 0xff << 8);

			/* Disable IBIST */
			Ana_Set_Reg(AUDDEC_ANA_CON12, 0x1 << 8, 0x1 << 8);
			/* Disable NV regulator (-1.2V) */
			Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0, 0x1);
			/* Disable cap-less LDOs (1.5V) */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0, 0x1055);
			/* Disable NCP */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x1, 0x1);

			TurnOffDacPower();
		}

	}
}

static int Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->dev_power[ANA_DEV_OUT_SPEAKERL];
	return 0;
}

static int Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() value = %ld\n ", __func__,
		ucontrol->value.integer.value[0]);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->dev_power[ANA_DEV_OUT_SPEAKERL] ==
		false)) {
		Speaker_Amp_Change(true);
		mCodec_data->dev_power[ANA_DEV_OUT_SPEAKERL] =
			ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   && (mCodec_data->dev_power[ANA_DEV_OUT_SPEAKERL] ==
		       true)) {
		mCodec_data->dev_power[ANA_DEV_OUT_SPEAKERL] =
			ucontrol->value.integer.value[0];
		Speaker_Amp_Change(false);
	}
	return 0;
}

static void Ext_Speaker_Amp_Change(bool enable)
{
#define SPK_WARM_UP_TIME        (25)	/* unit is ms */
	if (enable) {
		pr_debug("%s() ON+\n", __func__);

		AudDrv_GPIO_EXTAMP_Select(false, 3);

		/*udelay(1000); */
		usleep_range(1 * 1000, 20 * 1000);

		AudDrv_GPIO_EXTAMP_Select(true, 3);

		msleep(SPK_WARM_UP_TIME);

		pr_debug("%s() ON-\n", __func__);
	} else {
		pr_debug("%s(), OFF+\n", __func__);

		AudDrv_GPIO_EXTAMP_Select(false, 3);

		udelay(500);

		pr_debug("%s(), OFF-\n", __func__);
	}
}

static int Ext_Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->dev_power[ANA_DEV_OUT_EXTSPKAMP];
	return 0;
}

static int Ext_Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0]) {
		Ext_Speaker_Amp_Change(true);
		mCodec_data->dev_power[ANA_DEV_OUT_EXTSPKAMP] =
			ucontrol->value.integer.value[0];
	} else {
		mCodec_data->dev_power[ANA_DEV_OUT_EXTSPKAMP] =
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
		 mCodec_data->dev_power[ANA_DEV_RECEIVER_SPEAKER_SWITCH]);
	ucontrol->value.integer.value[0] =
		mCodec_data->dev_power[ANA_DEV_RECEIVER_SPEAKER_SWITCH];
	return 0;
}

static int Receiver_Speaker_Switch_Set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->dev_power
		[ANA_DEV_RECEIVER_SPEAKER_SWITCH] == false)) {
		Receiver_Speaker_Switch_Change(true);
		mCodec_data->dev_power
		[ANA_DEV_RECEIVER_SPEAKER_SWITCH] =
			ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   &&
		   (mCodec_data->dev_power
		    [ANA_DEV_RECEIVER_SPEAKER_SWITCH] == true)) {
		mCodec_data->dev_power
		[ANA_DEV_RECEIVER_SPEAKER_SWITCH] =
			ucontrol->value.integer.value[0];
		Receiver_Speaker_Switch_Change(false);
	}
	return 0;
}

static void Headset_Speaker_Amp_Change(bool enable)
{
#ifdef ANALOG_HPTRIM
	if (apply_n12db_gain) {
		pr_debug("%s(), current AUDDEC_ELR_0 = 0x%x, mic_vinp_mv %d\n",
			 __func__, Ana_Get_Reg(AUDDEC_ELR_0), mic_vinp_mv);

		if (mic_vinp_mv > MIC_VINP_4POLE_THRES_MV &&
		    ((codec_debug_enable & DBG_DCTRIM_BYPASS_4POLE) == 0)) {
			Ana_Set_Reg(AUDDEC_ELR_0,
				spk_4pole_anaoffset.enable << 12
				    | spk_4pole_anaoffset.hpr_finetrim << 10
				    | spk_4pole_anaoffset.hpl_finetrim << 8
				    | spk_4pole_anaoffset.hpr_trimecode << 4
				    | spk_4pole_anaoffset.hpl_trimecode << 0,
				    0xffff);
		} else {
			Ana_Set_Reg(AUDDEC_ELR_0,
				spk_3pole_anaoffset.enable << 12
				    | spk_3pole_anaoffset.hpr_finetrim << 10
				    | spk_3pole_anaoffset.hpl_finetrim << 8
				    | spk_3pole_anaoffset.hpr_trimecode << 4
				    | spk_3pole_anaoffset.hpl_trimecode << 0,
				    0xffff);
		}
		pr_debug("%s(), new AUDDEC_ELR_0 0x%x\n",
			__func__, Ana_Get_Reg(AUDDEC_ELR_0));
	}
#endif
	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower(ANA_DEV_OUT_SPEAKER_HEADSET_L);

		pr_debug("%s(), enable %d\n", __func__, enable);


		if (apply_n12db_gain)
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0004, 0xff);

		/* Audio left headphone input multiplexor selection : LOL*/
		set_input_mux(1);

		/* Disable headphone short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xf0ff);

		/* Disable lineout short-ckt protection */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x1 << 4, 0x1 << 4);

		/* Reduce ESD resistance of AU_REFN */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc000, 0xffff);

		/* Set HPL/HPR gain to -10dB*/
		Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_10DB_REG, 0xffff);

		/* Set LOLR/LOLL gain to -10dB */
		Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_10DB_REG, 0xffff);

		/* Turn on DA_600K_NCP_VA18 */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON1, 0x0001, 0xffff);

		/* Set NCP clock as 604kHz // 26MHz/43 = 604KHz */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON2, 0x002c, 0xffff);

		/* Toggle RG_DIVCKS_CHG */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON0, 0x0001, 0xffff);

		/* Set NCP soft start mode as default mode: 150us */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON4, 0x0002, 0xffff);

		/* Enable NCP */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x0000, 0xffff);
		udelay(250);

		/* Enable cap-less LDOs (1.5V), LCLDO local sense */
		Ana_Set_Reg(AUDDEC_ANA_CON14, 0x1055, 0x1055);

		/* Enable NV regulator (-1.2V) */
		Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0001, 0xffff);
		udelay(100);

		/* Disable AUD_ZCD */
		Hp_Zcd_Enable(false);

		/* Enable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

		/* Set HP DR bias current optimization, 010: 6uA */
		Ana_Set_Reg(AUDDEC_ANA_CON11, 0x4900, 0xffff);

		/* Set HP & ZCD bias current optimization */
		/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

		/* Set HPP/N STB enhance circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc033, 0xffff);

		if (apply_n12db_gain) {
			/* HP IVBUF (Vin path) de-gain enable: -12dB */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0x04, 0xff);
		}
		/* Set LO STB enhance circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x1 << 8, 0x1 << 8);

		/* Enable LO driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x1 << 1, 0x1 << 1);

		/* Enable LO driver core circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x1, 0x1);

		/* Set LOL gain to normal gain step by step */
		Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_10DB_REG, 0xffff);

		/* Switch HPL MUX to Line-out */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x01 << 8, 0x3 << 8);

		/* Switch HPR MUX to DAC-R */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x2 << 10, 0x3 << 10);

		/* Enable HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0c, 0xff);

		/* Enable HP aux feedback loop */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3c, 0xff);

		/* Enable HP aux CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0c00, 0xff00);

		/* Enable HP driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xc0, 0xf0);

		/* Enable HP driver core circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xf0, 0xf0);

		/* Short HP main output to HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0xfc, 0xff);

		/* Enable HP main CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0e00, 0xff00);

		/* Disable HP aux CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0200, 0xff00);

		/* Select CMFB resistor bulk to AC mode */
		/* Selec HS/LO cap size (6.5pF default) */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0000, 0xffff);

		/* Enable HS driver bias circuits */
		/* Disable HS main output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
		/* Enable LO driver bias circuits */
		/* Disable LO main output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);

		/* Enable LO main output stage */
		enable_lo_buffer(true);
		Apply_Speaker_Gain(DL_GAIN_0DB);

		/* Enable HP main output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00ff, 0x00ff);

		/* Enable HPR/L main output stage step by step */
		hp_main_output_ramp(true);

		/* Reduce HP aux feedback loop gain step by step */
		hp_aux_feedback_loop_gain_ramp(true);

		/* Disable HP aux feedback loop */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00cf, 0x00ff);

		/* Increase HPL/HPR gain to normal gain step by step */
		headset_volume_ramp(DL_GAIN_N_10DB,
				    mCodec_data->ana_gain[ANA_GAIN_HPOUTL]);

		/* Disable HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00c3, 0x00ff);

		/* Unshort HP main output to HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0003, 0x00ff);
		udelay(1000);

		/* HP ESD resistor @AU_REFN short enable */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc033, 0xffff);

		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);

		/* Enable Audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0xf, 0xf);

		/* Enable low-noise mode of DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x1, 0x1);
		udelay(100);

		/* Switch LOL MUX to audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x2 << 2, 0x3 << 2);

		/* Disable Pull-down HPL/R to AVSS28_AUD */
		hp_pull_down(false);

	} else {
		pr_debug("%s(), enable %d\n", __func__, enable);
		if (GetDLStatus() == false) {
			/* Pull-down HPL/R to AVSS28_AUD */
			hp_pull_down(true);

			/* Disable low-noise mode of DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0, 0x1);

			/* Disable Audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0xf);

			/* Disable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1);

			/* Short HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fc3, 0xffff);

			/* Enable HP aux output stage*/
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fcf, 0xffff);

			/* decrease HPL/R gain to normal gain step by step */
			headset_volume_ramp(
				mCodec_data->ana_gain[ANA_GAIN_HPOUTL],
				DL_GAIN_N_10DB);
			Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_10DB_REG, 0xffff);

			/* set HP aux feedback loop gain to max */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xf200, 0xff00);

			/* Enable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0xff, 0xff);

			/* Reduce HP aux feedback loop gain */
			hp_aux_feedback_loop_gain_ramp(false);

			/* decrease HPR/L main output stage step by step */
			hp_main_output_ramp(false);

			/* Switch HPL/HPR MUX to open */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0 << 8, 0xf << 8);

			/* Switch LOL MUX to open */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0 << 2, 0x3 << 2);

			/* Disable HP main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3);

			/* Enable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0e00, 0x0f00);

			/* Disable HP main CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0c00, 0x0f00);

			/* Unshort HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3 << 6);

			/* Disable HP driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x30c0, 0xffff);

			/* Disable HP driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);

			/* Disable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0, 0xffff);

			/* Open HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0xc, 0xffff);

			/* Disable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0xffff);

			/* decrease LOL gain to minimum gain step by step */
			Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_40DB_REG, 0xffff);

			/* Disable LO driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0, 0x1);

			/* Disable LO driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0 << 1, 0x1 << 1);

			/* Disable IBIST */
			Ana_Set_Reg(AUDDEC_ANA_CON12, 0x1 << 8, 0x1 << 8);
			/* Disable NV regulator (-1.2V) */
			Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0, 0x1);
			/* Disable cap-less LDOs (1.5V) */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0, 0x1055);
			/* Disable NCP */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x1, 0x1);

			/* Set HPL/HPR gain to mute */
			Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_40DB_REG, 0xffff);

			/* Increase ESD resistance of AU_REFN */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0, 0x1 << 14);

			TurnOffDacPower();
		}
	}
}


static int Headset_Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
		mCodec_data->dev_power[ANA_DEV_OUT_SPEAKER_HEADSET_R];
	return 0;
}

static int Headset_Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	pr_debug("%s() gain = %lu\n ", __func__,
	ucontrol->value.integer.value[0]);
	if ((ucontrol->value.integer.value[0] == true)
	    && (mCodec_data->dev_power
		[ANA_DEV_OUT_SPEAKER_HEADSET_R] == false)) {
		Headset_Speaker_Amp_Change(true);
		mCodec_data->dev_power
		[ANA_DEV_OUT_SPEAKER_HEADSET_R] =
		ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false)
		   &&
		   (mCodec_data->dev_power
		    [ANA_DEV_OUT_SPEAKER_HEADSET_R] == true)) {
		mCodec_data->dev_power
		[ANA_DEV_OUT_SPEAKER_HEADSET_R] =
		ucontrol->value.integer.value[0];
		Headset_Speaker_Amp_Change(false);
	}
	return 0;
}

static int Audio_AuxAdcData_Get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;

	pr_debug("%s dMax = 0x%lx\n", __func__,
		ucontrol->value.integer.value[0]);
	return 0;

}

static int Audio_AuxAdcData_Set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	dAuxAdcChannel = ucontrol->value.integer.value[0];
	pr_debug("%s dAuxAdcChannel = 0x%x\n", __func__, dAuxAdcChannel);
	return 0;
}


static const struct snd_kcontrol_new Audio_snd_auxadc_controls[] = {
	SOC_SINGLE_EXT("Audio AUXADC Data", SND_SOC_NOPM, 0, 0x80000, 0,
		       Audio_AuxAdcData_Get,
		       Audio_AuxAdcData_Set),
};


static const char *const amp_function[] = { "Off", "On" };
static const char *const aud_clk_buf_function[] = { "Off", "On" };

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

static int Lineout_PGAL_Get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_gain[ANA_GAIN_LINEOUTL];

	if (ucontrol->value.integer.value[0] == DL_GAIN_N_40DB)
		ucontrol->value.integer.value[0] =
		ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1;

	return 0;
}

static int Lineout_PGAL_Set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
		index = DL_GAIN_N_40DB;

	Ana_Set_Reg(ZCD_CON1, index, 0x001f);

	mCodec_data->ana_gain[ANA_GAIN_LINEOUTL] = index;
	return 0;
}

static int Lineout_PGAR_Get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_gain[ANA_GAIN_LINEOUTR];

	if (ucontrol->value.integer.value[0] == DL_GAIN_N_40DB)
		ucontrol->value.integer.value[0] =
		ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1;

	return 0;
}

static int Lineout_PGAR_Set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
		index = DL_GAIN_N_40DB;

	Ana_Set_Reg(ZCD_CON1, index << 7, 0x0f80);
	mCodec_data->ana_gain[ANA_GAIN_LINEOUTR] = index;
	return 0;
}

static int Handset_PGA_Get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_gain[ANA_GAIN_HSOUTL];

	if (ucontrol->value.integer.value[0] == DL_GAIN_N_40DB)
		ucontrol->value.integer.value[0] =
		ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN) - 1;

	return 0;
}

static int Handset_PGA_Set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN) - 1))
		index = DL_GAIN_N_40DB;

	Ana_Set_Reg(ZCD_CON3, index, 0x001f);

	mCodec_data->ana_gain[ANA_GAIN_HSOUTL] = index;
	return 0;
}

static int Headset_PGAL_Get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() = %d\n", __func__,
		 mCodec_data->ana_gain[ANA_GAIN_HPOUTL]);
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_gain[ANA_GAIN_HPOUTL];

	if (ucontrol->value.integer.value[0] == DL_GAIN_N_40DB)
		ucontrol->value.integer.value[0] =
		ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1;

	return 0;
}

static int Headset_PGAL_Set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];
	int old_idx = mCodec_data->ana_gain[ANA_GAIN_HPOUTL];

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1))
		index = DL_GAIN_N_40DB;

	mCodec_data->ana_gain[ANA_GAIN_HPOUTL] = index;
	mCodec_data->ana_gain[ANA_GAIN_HPOUTR] = index;

	if (mCodec_data->dev_power[ANA_DEV_OUT_HEADSETL]) {
		headset_volume_ramp(old_idx, index);
#ifndef ANALOG_HPTRIM
		SetDcCompenSation(true);
#endif
	}

	return 0;
}

static int Headset_PGAR_Get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_gain[ANA_GAIN_HPOUTR];

	if (ucontrol->value.integer.value[0] == DL_GAIN_N_40DB)
		ucontrol->value.integer.value[0] =
			ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1;

	return 0;
}


static int Headset_PGAR_Set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];
	int old_idx = mCodec_data->ana_gain[ANA_GAIN_HPOUTR];

	pr_debug("%s(), index = %d\n", __func__, index);

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1))
		index = DL_GAIN_N_40DB;

	mCodec_data->ana_gain[ANA_GAIN_HPOUTL] = index;
	mCodec_data->ana_gain[ANA_GAIN_HPOUTR] = index;

	if (mCodec_data->dev_power[ANA_DEV_OUT_HEADSETR]) {
		headset_volume_ramp(old_idx, index);
#ifndef ANALOG_HPTRIM
		SetDcCompenSation(true);
#endif
	}

	return 0;
}

static int codec_adc_sample_rate_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mBlockSampleRate[ANA_DEV_IN_ADC] = %d\n", __func__,
		 mBlockSampleRate[ANA_DEV_IN_ADC]);
	ucontrol->value.integer.value[0] = mBlockSampleRate[ANA_DEV_IN_ADC];
	return 0;

}

static int codec_adc_sample_rate_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	mBlockSampleRate[ANA_DEV_IN_ADC] = ucontrol->value.integer.value[0];
	pr_debug("%s mBlockSampleRate[ANA_DEV_IN_ADC] = %d\n", __func__,
		 mBlockSampleRate[ANA_DEV_IN_ADC]);
	return 0;
}

static int codec_dac_sample_rate_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mBlockSampleRate[ANA_DEV_OUT_DAC] = %d\n", __func__,
		 mBlockSampleRate[ANA_DEV_OUT_DAC]);
	ucontrol->value.integer.value[0] =
		mBlockSampleRate[ANA_DEV_OUT_DAC];
	return 0;

}

static int codec_dac_sample_rate_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	mBlockSampleRate[ANA_DEV_OUT_DAC] =
		ucontrol->value.integer.value[0];
	pr_debug("%s mBlockSampleRate[ANA_DEV_OUT_DAC] = %d\n", __func__,
		 mBlockSampleRate[ANA_DEV_OUT_DAC]);
	return 0;
}

static int Aud_Clk_Buf_Get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = audck_buf_Count;
	return 0;
}

static int Aud_Clk_Buf_Set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0])
		audckbufEnable(true);
	else
		audckbufEnable(false);

	return 0;
}

static int pmic_dc_offset_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), %d, %d\n", __func__, hpl_dc_offset, hpr_dc_offset);

	pr_debug("%s(), hp_trim(3pole/4pole) = 0x%x/0x%x, hp+spk_trim(3pole/4pole) = 0x%x/0x%x\n",
		 __func__,
		 hp_3_pole_trim_setting, hp_4_pole_trim_setting,
		 spk_hp_3_pole_trim_setting, spk_hp_4_pole_trim_setting);

	ucontrol->value.integer.value[0] = hpl_dc_offset;
	ucontrol->value.integer.value[1] = hpr_dc_offset;
	return 0;
}

static int pmic_dc_offset_set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), %ld, %ld\n", __func__, ucontrol->value.integer.value[0],
		 ucontrol->value.integer.value[1]);
	hpl_dc_offset = ucontrol->value.integer.value[0];
	hpr_dc_offset = ucontrol->value.integer.value[1];
	return 0;
}

#ifdef ANALOG_HPTRIM
static int disable_pmic_dctrim_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), dc_compensation_disabled: %d\n",
		 __func__, dc_compensation_disabled);
	ucontrol->value.integer.value[0] = dc_compensation_disabled;
	return 0;
}

static int disable_pmic_dctrim_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), dc_compensation_disabled: %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	dc_compensation_disabled = ucontrol->value.integer.value[0];
	return 0;

}
#endif

static const char * const dctrim_control_state[] = {
	"Not_Yet", "Calibrating", "Calibrated", "Reset"
};

static int pmic_dctrim_control_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), dctrim_calibrated = %d\n", __func__, dctrim_calibrated);
	ucontrol->value.integer.value[0] = dctrim_calibrated;
	return 0;
}

static int pmic_dctrim_control_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(dctrim_control_state)) {
		pr_warn("%s(), return -EINVAL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s()+, dctrim_calibrated = %d\n",
		 __func__, dctrim_calibrated);
	if (ucontrol->value.integer.value[0] == 1) {
		get_hp_lr_trim_offset();
	} else if (ucontrol->value.integer.value[0] == 3) {
		memset(&hp_3pole_anaoffset, 0, sizeof(hp_3pole_anaoffset));
		memset(&hp_4pole_anaoffset, 0, sizeof(hp_4pole_anaoffset));
		memset(&spk_3pole_anaoffset, 0, sizeof(spk_3pole_anaoffset));
		memset(&spk_4pole_anaoffset, 0, sizeof(spk_4pole_anaoffset));
		dctrim_calibrated = ucontrol->value.integer.value[0];
	} else {
		dctrim_calibrated = ucontrol->value.integer.value[0];
	}
	pr_debug("%s()-, dctrim_calibrated = %d\n",
		 __func__, dctrim_calibrated);
	return 0;
}

/*
 * Temp solution, query gpio of PCB_ID of board
 */
enum AUDIO_MIC_MODE {
	AUDIO_MIC_MODE_ACC = 1,
	AUDIO_MIC_MODE_DCC,
	AUDIO_MIC_MODE_DMIC,
	AUDIO_MIC_MODE_DMIC_LP,
	AUDIO_MIC_MODE_DCCECMDIFF,
	AUDIO_MIC_MODE_DCCECMSINGLE,
};

#ifndef CONFIG_MT8183_QUERY_PCB_ID_METHOD
enum pcb_id_index {
	PCD_ID_1 = 0, /* GPIO175 */
	PCD_ID_2 = 1, /* GPIO111 */
	PCD_ID_NUM = 2,
};

static int get_pcb_id_state(int pcd_id)
{
	struct device_node *node = NULL;
	int gpionum;
	int ret = -1;

	pr_debug("%s\n", __func__);

	node = of_find_compatible_node(NULL, NULL,
				       "mediatek,mt_soc_codec_63xx");

	if (!node) {
		pr_debug("%s(), cannot find dts node!\n", __func__);
		return ret;
	}

	gpionum = of_get_named_gpio(node, "pcbinfo", pcd_id);
	if (gpionum < 0) {
		pr_debug("%s(), cannot find pcbinfo node!\n", __func__);
		return ret;
	}

	ret = gpio_request(gpionum, "info");
	if (ret) {
		pr_debug("%s(), request pcbinfo failed!\n", __func__);
		return ret;
	}

	ret = gpio_get_value(gpionum);
	pr_debug("%s(), gpio(%d) value = %d\n", __func__, gpionum, ret);

	gpio_free(gpionum);

	return ret;
}
#endif

static int get_mic_mode(void)
{
	int ret = -1;

#ifdef CONFIG_MT8183_QUERY_PCB_ID_METHOD
	int data[4];
	int rawdata;

	ret = IMM_GetOneChannelValue(2, data, &rawdata);
	/*
	 *   Mapping rule:
	 *
	 *   AUXIN2    PCB_ID  Mic_Mode
	 *   0~0.5V     SMT1     DCC
	 *   0.7~1.1V   SMT2     ACC
	 *
	 */
	if (ret < 0 || rawdata < 0) {
		pr_debug("%s(), get auxadc channel value error %d %d\n",
			__func__, ret,
			 rawdata);
		return -1;
	}

	if ((rawdata >= 0) && (rawdata <= 1366))
		ret = AUDIO_MIC_MODE_DCC;
	else if ((rawdata >= 1911) && (rawdata <= 3004))
		ret = AUDIO_MIC_MODE_ACC;
	else
		ret = AUDIO_MIC_MODE_ACC;

#else
	int gpioval_1 = 0, gpioval_2 = 0;

	gpioval_1 = get_pcb_id_state(PCD_ID_1);
	if (gpioval_1 < 0) {
		pr_debug("%s(), gpioval_1(%d) invalid\n", __func__, gpioval_1);
		return ret;
	}

	gpioval_2 = get_pcb_id_state(PCD_ID_2);
	if (gpioval_2 < 0) {
		pr_debug("%s(), gpioval_2(%d) invalid\n", __func__, gpioval_2);
		return ret;
	}
	/*
	 *   Mapping rule:
	 *
	 *   PCB_ID2 PCB_ID1  Mic_Mode
	 *       0     0      EVB_DCC
	 *       0     1      Phone_DCC
	 *       1     0      Phone_ACC
	 *       1     1      Reserved
	 *
	 */
	if ((gpioval_2 == 0) && (gpioval_1 == 0))
		ret = AUDIO_MIC_MODE_DCC;
	else if ((gpioval_2 == 0) && (gpioval_1 == 1))
		ret = AUDIO_MIC_MODE_DCC;
	else if ((gpioval_2 == 1) && (gpioval_1 == 0))
		ret = AUDIO_MIC_MODE_ACC;
	else
		ret = AUDIO_MIC_MODE_ACC;
#endif
	pr_debug("%s(), return  mic_mode = %d\n", __func__, ret);

	return ret;
}

static int Audio_MIC_Mode_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int mic_mode = AUDIO_MIC_MODE_ACC;

	pr_debug("%s()\n", __func__);
	mic_mode = get_mic_mode();

	if (mic_mode != -1)
		ucontrol->value.integer.value[0] = mic_mode;
	else
		ucontrol->value.integer.value[0] = AUDIO_MIC_MODE_ACC;
	pr_info("%s(), return MIC_MODE: %ld\n",
		__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int Audio_MIC_Mode_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), not support\n", __func__);
	return 0;
}

static int hp_impedance_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	if (!set_hp_impedance_ctl) {
		pr_warn("%s(), set_hp_impedance_ctl == NULL\n", __func__);
		return 0;
	}

	set_hp_impedance_ctl(true);
	if (OpenHeadPhoneImpedanceSetting(true)) {
		hp_impedance = detect_impedance();
		OpenHeadPhoneImpedanceSetting(false);
	} else
		pr_warn("%s(), pmic dl busy, do nothing\n", __func__);
	set_hp_impedance_ctl(false);


	ucontrol->value.integer.value[0] = hp_impedance;

	pr_debug("%s(), hp_impedance = %d, efuse = %d\n",
		 __func__, hp_impedance, efuse_current_calibrate);
	return 0;
}

static int hp_impedance_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), hp_impedance = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int hp_plugged;
static int hp_plugged_in_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), hp_plugged = %d\n", __func__, hp_plugged);
	ucontrol->value.integer.value[0] = hp_plugged;
	return 0;
}

static int hp_plugged_in_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(amp_function)) {
		pr_warn("%s(), return -EINVAL\n", __func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 1) {
		mic_vinp_mv = get_accdet_auxadc();
		pr_info("%s(), mic_vinp_mv = %d\n", __func__, mic_vinp_mv);
	}

	hp_plugged = ucontrol->value.integer.value[0];

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
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dctrim_control_state),
	dctrim_control_state)
};

static const struct snd_kcontrol_new mt6358_snd_controls[] = {
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
	SOC_ENUM_EXT("Handset_PGA_GAIN", Audio_DL_Enum[7],
	Handset_PGA_Get,
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
	SOC_ENUM_EXT("PMIC_REG_CLEAR", Audio_DL_Enum[12],
	PMIC_REG_CLEAR_Get, PMIC_REG_CLEAR_Set),
	SOC_SINGLE_EXT("Codec_ADC_SampleRate", SND_SOC_NOPM, 0,
	MAX_UL_SAMPLE_RATE, 0,
		       codec_adc_sample_rate_get,
		       codec_adc_sample_rate_set),
	SOC_SINGLE_EXT("Codec_DAC_SampleRate", SND_SOC_NOPM, 0,
	MAX_DL_SAMPLE_RATE, 0,
		       codec_dac_sample_rate_get,
		       codec_dac_sample_rate_set),
	SOC_DOUBLE_EXT("DcTrim_DC_Offset", SND_SOC_NOPM, 0, 1, 0x20000, 0,
		       pmic_dc_offset_get, pmic_dc_offset_set),
	SOC_ENUM_EXT("Dctrim_Control_Switch", Audio_DL_Enum[13],
		     pmic_dctrim_control_get, pmic_dctrim_control_set),
	SOC_SINGLE_EXT("Audio HP ImpeDance Setting",
		       SND_SOC_NOPM, 0, 0x10000, 0,
		       hp_impedance_get, hp_impedance_set),
	SOC_ENUM_EXT("Headphone Plugged In", Audio_DL_Enum[0],
		     hp_plugged_in_get, hp_plugged_in_set),
	SOC_SINGLE_EXT("Audio_MIC_Mode", SND_SOC_NOPM, 0, 6, 0,
		       Audio_MIC_Mode_Get, Audio_MIC_Mode_Set),
#ifdef ANALOG_HPTRIM
	SOC_ENUM_EXT("Disable Analog DC Compensation", Audio_DL_Enum[0],
		     disable_pmic_dctrim_get,
		     disable_pmic_dctrim_set),
#endif

};

void SetMicPGAGain(void)
{
	int index = 0;

	index = mCodec_data->ana_gain[ANA_GAIN_MICAMP1];
	pr_debug("%s  ANA_GAIN_MICAMP1 index =%d\n", __func__, index);
	Ana_Set_Reg(AUDENC_ANA_CON0, index << 8, 0x0700);
	index = mCodec_data->ana_gain[ANA_GAIN_MICAMP2];
	Ana_Set_Reg(AUDENC_ANA_CON1, index << 8, 0x0700);

}

static bool GetAdcStatus(void)
{
	int i = 0;

	for (i = ANA_DEV_IN_ADC1; i < ANA_DEV_MAX; i++) {
		if ((mCodec_data->dev_power[i] == true)
		    && (i != ANA_DEV_RECEIVER_SPEAKER_SWITCH))
			return true;
	}
	return false;
}

static bool TurnOnADcPowerACC(int ADCType, bool enable)
{
	pr_debug("%s ADCType = %d enable = %d\n", __func__, ADCType, enable);

	if (enable) {
		if (GetAdcStatus() == false) {
			audckbufEnable(true);

			/* Enable audio globe bias */
			NvregEnable(true);

			/* Enable CLKSQ 26MHz */
			ClsqEnable(true);

			/* Enable audio ADC CLKGEN  */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1 << 5, 0x1 << 5);
			/* ADC CLK from CLKGEN (13MHz) */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0xffff);
			/* Enable  LCLDO_ENC 1P8V */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0100, 0x2500);
			/* LCLDO_ENC remote sense */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x2500, 0x2500);

			/* mic bias */
			if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1] == 0) {
				/* phone mic */
				/* Enable MICBIAS0, MISBIAS0 = 1P9V */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0xffff);
			} else if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1]
			== 1) {
				/* headset mic */
				/* Enable MICBIAS1, MISBIAS1 = 2P6V */
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0061, 0xffff);
			}

			SetMicPGAGain();
		}

		if (ADCType == ANA_DEV_IN_ADC1) {
			if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1] == 0) {
				/* "ADC1", main_mic */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0041, 0xf0ff);
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5041, 0xf000);
			} else if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1]
			== 1) {
				/* "ADC2", headset mic */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0081, 0xf0ff);
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5081, 0xf000);
			}
		} else if (ADCType == ANA_DEV_IN_ADC2) {
			/* ref mic */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x00c1, 0xf0ff);
			/* Audio R ADC input sel : R PGA. Enable audio R ADC */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x50c1, 0xf000);
		}

		if (GetAdcStatus() == false) {
			/* set gpio miso mode */
			set_capture_gpio(true);

			/* here to set digital part */
			Topck_Enable(true);

			/* power on clock */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0000, 0x00bf);

			/* configure ADC setting */
			Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);

			/* [0] afe enable */
			Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);

			/* MTKAIF TX format setting */
			Ana_Set_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0, 0x0000, 0xffff);

			/* enable aud_pad TX fifos */
			Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x3100, 0xff00);

			/* UL dmic setting */
			Ana_Set_Reg(AFE_UL_SRC_CON0_H, 0x0000, 0xffff);

			/* UL turn on */
			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0001, 0xffff);
		}
	} else {
		if (GetAdcStatus() == false) {
			/* UL turn off */
			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0x0001);

			/* disable aud_pad TX fifos */
			Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x3000, 0xff00);

			if (GetDLStatus() == false) {
				/* afe disable */
				Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
				/* afe power down and total audio clk disable */
				Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x00ff,
				0x00ff);
			}

			/* up-link power down */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0020, 0x0020);

			set_capture_gpio(false);
		}

		if (ADCType == ANA_DEV_IN_ADC1) {
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0xf000);
			/* Audio L ADC input sel : off, disable audio L ADC */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
			/* Disable audio L PGA */
		} else if (ADCType == ANA_DEV_IN_ADC2) {
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0xf000);
			/* Audio R ADC input sel : off, disable audio R ADC */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x0fff);
			/* Disable audio R PGA */
		}

		if (GetAdcStatus() == false) {
			/* mic bias */
			if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1] == 0) {
				/* phone mic */
				/* Disable MICBIAS0, MISBIAS0 = 1P7V */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0xffff);
			} else if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1]
				== 1) {
				/* headset mic */
				/* Disable MICBIAS1 */
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x0001);
			}

			/* LCLDO_ENC remote sense off */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0100, 0x2500);
			/* disable LCLDO_ENC 1P8V */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0000, 0x2500);

			/* ADC CLK from CLKGEN (13MHz) */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0xffff);
			/* disable audio ADC CLKGEN  */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0 << 5, 0x1 << 5);

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
	pr_debug("%s(), ADCType = %d enable = %d\n", __func__, ADCType, enable);
	if (enable) {
		if (GetAdcStatus() == false) {
			if (set_ap_dmic != NULL)
				set_ap_dmic(true);
			else
				pr_warn("%s(), set_ap_dmic == NULL\n",
				__func__);

			audckbufEnable(true);

			/* Enable audio globe bias */
			NvregEnable(true);

			/* Enable CLKSQ 26MHz */
			ClsqEnable(true);

			/* mic bias */
			/* Enable MICBIAS0, MISBIAS0 = 1P9V */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0xffff);

			/* RG_BANDGAPGEN=1'b0 */
			Ana_Set_Reg(AUDENC_ANA_CON10, 0x0, 0x1 << 12);

			/* DMIC enable */
			Ana_Set_Reg(AUDENC_ANA_CON8, 0x0005, 0xffff);

			/* here to set digital part */

			/* set gpio miso mode */
			set_capture_gpio(true);

			/* AdcClockEnable(true); */
			Topck_Enable(true);

			/* power on clock */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0000, 0x00bf);

			/* configure ADC setting */
			Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);

			/* [0] afe enable */
			Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);

			/* MTKAIF TX format setting */
			Ana_Set_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0, 0x0000, 0xffff);

			/* enable aud_pad TX fifos */
			Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x3100, 0xff00);

			/* UL dmic setting */
			Ana_Set_Reg(AFE_UL_SRC_CON0_H, 0x0080, 0xffff);

			/* UL turn on */
			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0003, 0xffff);
		}
	} else {
		if (GetAdcStatus() == false) {
			/* UL turn off */
			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0x0003);

			/* disable aud_pad TX fifos */
			Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x3000, 0xff00);

			if (GetDLStatus() == false) {
				/* afe disable */
				Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
				/* afe power down and total audio clk disable */
				Ana_Set_Reg(PMIC_AUDIO_TOP_CON0,
				0x00ff, 0x00ff);
			}

			/* up-link power down */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0020, 0x0020);

			set_capture_gpio(false);

			/* DMIC disable */
			Ana_Set_Reg(AUDENC_ANA_CON8, 0x0000, 0xffff);

			/* mic bias */
			/* MISBIAS0 = 1P7V */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0001, 0xffff);

			/* RG_BANDGAPGEN=1'b0 */
			Ana_Set_Reg(AUDENC_ANA_CON10, 0x0, 0x1 << 12);

			/* MICBIA0 disable */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0xffff);

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
	pr_debug("%s(), enable %d, ADCType %d, MICSOURCE_MUX_IN_1 %d, ECMmode %d\n",
		 __func__,
		 enable,
		 ADCType,
		 mCodec_data->ana_mux[MICSOURCE_MUX_IN_1],
		 ECMmode);

	if (enable) {
		if (GetAdcStatus() == false) {
			audckbufEnable(true);

			/* Enable audio globe bias */
			NvregEnable(true);

			/* Enable CLKSQ 26MHz */
			ClsqEnable(true);

			/* Enable audio ADC CLKGEN  */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1 << 5, 0x1 << 5);
			/* ADC CLK from CLKGEN (13MHz) */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0xffff);
			/* Enable  LCLDO_ENC 1P8V */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0100, 0x2500);
			/* LCLDO_ENC remote sense */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x2500, 0x2500);

			/* AdcClockEnable(true); */
			Topck_Enable(true);
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2060, 0xffff);
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2061, 0xffff);
			Ana_Set_Reg(AFE_DCCLK_CFG1, 0x0100, 0xffff);

			/* mic bias */
			if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1] == 0) {
				/* phone mic */
				switch (ECMmode) {
				case 1: /* AUDIO_MIC_MODE_DCCECMDIFF */
					Ana_Set_Reg(AUDENC_ANA_CON9,
						0x7700, 0xff00);
					break;
				case 2:/* AUDIO_MIC_MODE_DCCECMSINGLE */
					Ana_Set_Reg(AUDENC_ANA_CON9,
						0x1100, 0xff00);
					break;
				default:
					Ana_Set_Reg(AUDENC_ANA_CON9,
						0x0000, 0xff00);
					break;
				}
				/* Enable MICBIAS0, MISBIAS0 = 1P9V */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0x00ff);
			} else if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1]
			== 1) {
				/* headset mic */
				/* Enable MICBIAS1, MISBIAS1 = 2P6V */
				if (ECMmode == 2)
					Ana_Set_Reg(AUDENC_ANA_CON10,
					0x0161, 0xffff);
				else
					Ana_Set_Reg(AUDENC_ANA_CON10,
					0x0061, 0xffff);
			}

			SetMicPGAGain();
		}

		if (ADCType == ANA_DEV_IN_ADC1) {
			/* Audio L preamplifier DCC precharge */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0004, 0xf8ff);

			if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1] == 0) {
				/* "ADC1", main_mic */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0045, 0xf0ff);
				/* Audio L preamplifier DCCEN */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x1 << 1,
				0x1 << 1);
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5047, 0xf000);
			} else if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1]
			== 1) {
				/* "ADC2", headset mic */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0085, 0xf0ff);
				/* Audio L preamplifier DCCEN */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x1 << 1,
				0x1 << 1);
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5087, 0xf000);
			}

			usleep_range(100, 150);
			/* Audio L preamplifier DCC precharge off */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0, 0x1 << 2);
		} else if (ADCType == ANA_DEV_IN_ADC2) {
			/* Audio R preamplifier DCC precharge */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0004, 0xf8ff);

			/* ref mic */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x00c5, 0xf0ff);
			/* Audio R preamplifier DCCEN */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x1 << 1, 0x1 << 1);
			/* Audio R ADC input sel : R PGA. Enable audio R ADC */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x50c7, 0xf000);

			usleep_range(100, 150);
			/* Audio R preamplifier DCC precharge off */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0, 0x1 << 2);
		}

		if (GetAdcStatus() == false) {
			/* Short body to ground in PGA */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0, 0x1 << 12);

			/* here to set digital part */

			/* set gpio miso mode */
			set_capture_gpio(true);

			/* power on clock */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0000, 0x00bf);

			/* configure ADC setting */
			Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0xffff);

			/* [0] afe enable */
			Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);

			/* MTKAIF TX format setting */
			Ana_Set_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0, 0x0000, 0xffff);

			/* enable aud_pad TX fifos */
			Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x3100, 0xff00);

			/* UL dmic setting */
			Ana_Set_Reg(AFE_UL_SRC_CON0_H, 0x0000, 0xffff);

			/* UL turn on */
			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0001, 0xffff);
		}
	} else {
		if (GetAdcStatus() == false) {
			/* UL turn off */
			Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0x0001);

			/* disable aud_pad TX fifos */
			Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x3000, 0xff00);

			if (GetDLStatus() == false) {
				/* afe disable */
				Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
				/* afe power down and total audio clk disable */
				Ana_Set_Reg(PMIC_AUDIO_TOP_CON0,
				0x00ff, 0x00ff);
			}

			/* up-link power down */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0020, 0x0020);

			set_capture_gpio(false);
		}

		if (ADCType == ANA_DEV_IN_ADC1) {
			/* Audio L ADC input sel : off, disable audio L ADC */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0xf000);
			/* Audio L preamplifier DCCEN */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0 << 1, 0x1 << 1);
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0xfffb);
			/* Disable audio L PGA */

			/* disable Audio L preamplifier DCC precharge */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0, 0x1 << 2);
		} else if (ADCType == ANA_DEV_IN_ADC2) {
			/* Audio R ADC input sel : off, disable audio R ADC */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0xf000);
			/* Audio r preamplifier DCCEN */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0 << 1, 0x1 << 1);
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x0ffb);
			/* Disable audio R PGA */

			/* disable Audio R preamplifier DCC precharge */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0, 0x1 << 2);
		}

		if (GetAdcStatus() == false) {
			/* mic bias */
			if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1] == 0) {
				/* phone mic */
				/* Disable MICBIAS0, MISBIAS0 = 1P7V */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0xffff);
			} else if (mCodec_data->ana_mux[MICSOURCE_MUX_IN_1]
				== 1) {
				/* headset mic */
				/* Disable MICBIAS1 */
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0000, 0x0001);
			}

			/* dcclk_gen_on=1'b0 */
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2060, 0xffff);
			/* dcclk_pdn=1'b1 */
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
			/* dcclk_ref_ck_sel=2'b00 */
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
			/* dcclk_div=11'b00100000011 */
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);

			/* LCLDO_ENC remote sense off */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0100, 0x2500);
			/* disable LCLDO_ENC 1P8V */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0000, 0x2500);

			/* ADC CLK from CLKGEN (13MHz) */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0xffff);
			/* disable audio ADC CLKGEN  */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0 << 5, 0x1 << 5);

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
	pr_debug("%s enable = %d\n", __func__, enable);
	if (enable) {
		/*move to vow driver*/
#ifdef VOW_STANDALONE_CONTROL
		if (mAudio_VOW_Mic_type == VOW_MIC_TYPE_Handset_DMIC)
			Ana_Set_Reg(AFE_VOW_TOP, 0x6850, 0xffff);
		else
			Ana_Set_Reg(AFE_VOW_TOP, 0x4810, 0xffff);
#endif
	} else {
		/*disable VOW interrupt here*/
#ifdef VOW_STANDALONE_CONTROL
		/*move to vow driver*/

		Ana_Set_Reg(AFE_VOW_TOP, 0x4010, 0xffff);   /*VOW disable*/
		Ana_Set_Reg(AFE_VOW_TOP, 0xC010, 0xffff);
#endif
	}
#endif
	return true;
}

#ifdef CONFIG_MTK_VOW_SUPPORT
static void TurnOnVOWPeriodicOnOff(int MicType, int On_period, int enable)
{
	int i = 0;
	const unsigned short (*pBuf)[22];

	/* give a default value */
	pBuf = Handset_AMIC_DCC_PeriodicOnOff;

	if ((MicType == VOW_MIC_TYPE_Headset_MIC)
	    || (MicType == VOW_MIC_TYPE_Handset_AMIC)
	    || (MicType == VOW_MIC_TYPE_Handset_DMIC)
	    || (MicType == VOW_MIC_TYPE_Handset_DMIC_800K)
	    || (MicType == VOW_MIC_TYPE_Handset_DMIC_VENDOR01)
	    || (MicType >= VOW_MIC_TYPE_NUM)
	    || (MicType < 0)) {
		pr_debug("MicType:%d, No support periodic On/Off\n", MicType);
		return;
	}

	if (enable == 0) {
		VOW32KCK_Enable(false);
		Ana_Set_Reg(AFE_VOW_PERIODIC_CFG13, 0x8000, 0x8000);
		Ana_Set_Reg(AFE_VOW_PERIODIC_CFG14, 0x0000, 0x8000);
		for (i = 0; i < 22; i++)
			Ana_Set_Reg(AFE_VOW_PERIODIC_CFG2 + (i<<1), 0x0000,
			0xFFFF);

		/* Set Period */
		Ana_Set_Reg(AFE_VOW_PERIODIC_CFG0, 0x0000, 0xFFFF);

	} else {
		pr_debug("%s, On_period:%d\n",
			__func__, 100 - (On_period * 10));

		VOW32KCK_Enable(true);
		switch (MicType) {
		case VOW_MIC_TYPE_Handset_AMIC_DCC:
		case VOW_MIC_TYPE_Handset_AMIC_DCCECM:
			pBuf = Handset_AMIC_DCC_PeriodicOnOff;
			break;

		case VOW_MIC_TYPE_Headset_MIC_DCC:
		case VOW_MIC_TYPE_Headset_MIC_DCCECM:
			pBuf = Headset_MIC_DCC_PeriodicOnOff;
			break;

		default:
			break;
		}
		if (On_period > 0) {
			/*  <Periodic ON>  */
			/* 32k_switch, [15]=0 */
			Ana_Set_Reg(AFE_VOW_PERIODIC_CFG13, 0x0000, 0x8000);
			/* vow_snrdet_periodic_cfg  = 1 */
			Ana_Set_Reg(AFE_VOW_PERIODIC_CFG14, 0x8000, 0x8000);
			for (i = 0; i < 22; i++) {
				Ana_Set_Reg(AFE_VOW_PERIODIC_CFG2 + (i<<1),
					pBuf[On_period - 1][i], 0xFFFF);
			}
		} else {
			Ana_Set_Reg(AFE_VOW_PERIODIC_CFG13, 0x8000, 0x8000);
			Ana_Set_Reg(AFE_VOW_PERIODIC_CFG14, 0x0000, 0x8000);
			for (i = 0; i < 22; i++)
				Ana_Set_Reg(AFE_VOW_PERIODIC_CFG2 + (i<<1),
				0x0000, 0xFFFF);
		}
		/* Set Period */
		Ana_Set_Reg(AFE_VOW_PERIODIC_CFG0, 0x999A, 0xFFFF);
		pr_debug("AFE_VOW_PERIODIC_CFG0:%x\n",
			Ana_Get_Reg(AFE_VOW_PERIODIC_CFG0));
	}
}

static void VOW_GPIO_Enable(bool enable)
{
	pr_debug("%s():%x\n", __func__, enable);
	if (enable == true) {
		/* set AP side GPIO */
		/* Enable AUD_DAT_MISO1 */
		/* Enable AUD_DAT_MISO0 */
		AudDrv_GPIO_Request(true, Soc_Aud_Digital_Block_ADDA_VOW);
		/* set PMIC side GPIO */
		Ana_Set_Reg(GPIO_MODE3, 0x0120, 0x01F8);
		/* GPIO Set to VOW data */
	} else {
		/* set AP side GPIO */
		/* Disable AUD_DAT_MISO1 */
		/* Disable AUD_DAT_MISO0 */
		AudDrv_GPIO_Request(false, Soc_Aud_Digital_Block_ADDA_VOW);
		/* set PMIC GPIO */
		Ana_Set_Reg(GPIO_MODE3, 0x0240, 0x01F8);
		/* GPIO Set to VOW data */
	}
}

static void VOW_Pwr_Enable(int MicType, bool enable)
{
	if ((MicType >= VOW_MIC_TYPE_NUM) || (MicType < 0)) {
		pr_debug("%s(),Not support this Mic Type\n", __func__);
		return;
	}
	if (enable == true) {

		NvregEnable(true); /* 0x0D04 Enable audio globe bias */

		if ((MicType != VOW_MIC_TYPE_Handset_DMIC)
		    && (MicType != VOW_MIC_TYPE_Handset_DMIC_800K)) {
			/* Enable Audio ADC flash Audio ADC flash */
			Ana_Set_Reg(AUDENC_ANA_CON2,  0x003F, 0x0039);
		}
		/* XO_AUDIO_EN_M Enable */
		Ana_Set_Reg(DCXO_CW14,  0xA25B, 0x2000);
		/* Enable audio ADC CLKGEN */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0020, 0x0020);
		/* Enable XO_AUDIO_VOW */
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0800, 0x0800);
		/* Enable XO_VOW */
		Ana_Set_Reg(DCXO_CW13,  0x9929, 0x0100);

		if ((MicType != VOW_MIC_TYPE_Handset_DMIC)
		    && (MicType != VOW_MIC_TYPE_Handset_DMIC_800K)) {
			/* Enable VOW CLKSQ */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x8800, 0x8000);
			/* Enable  LCLDO_ENC 1P8V */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0100, 0x0100);
			/* LCLDO_ENC remote sense */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x2500, 0x2400);
		}

	} else {

		if ((MicType != VOW_MIC_TYPE_Handset_DMIC)
		    && (MicType != VOW_MIC_TYPE_Handset_DMIC_800K)) {
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0100, 0x2400);
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0x000C);
			/* Disable VOW CLKSQ */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0800, 0x8000);
		}
		/* Disable XO_VOW */
		Ana_Set_Reg(DCXO_CW13, 0x9829, 0x0100);
		/* Disable XO_AUDIO_VOW */
		Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x0800);
		/* XO_AUDIO_EN_M Enable, [13] xo_audio_en_m */
		Ana_Set_Reg(DCXO_CW14, 0xA2B5, 0x2000);

		if ((MicType != VOW_MIC_TYPE_Handset_DMIC)
		    && (MicType != VOW_MIC_TYPE_Handset_DMIC_800K)) {
			/* Disable Audio ADC flash Audio ADC flash */
			Ana_Set_Reg(AUDENC_ANA_CON2,  0x0000, 0x0039);
		}

		NvregEnable(false); /* 0x0D04 Disable audio globe bias */

	}
}

static void VOW_DCC_CLK_Enable(bool enable)
{
	unsigned int pmic_version = Ana_Get_Reg(SWCID);

	if (enable == true) {
		VOW13MCK_Enable(true); /* 0x0258 VOW13M_CK power on */
		/* DCC mode MT6358 E1 and E2 work around */
		if ((pmic_version == 0x5810) || (pmic_version == 0x5820))
			ClsqEnable(true);
		/* PGA DCC CLK divider */
		Ana_Set_Reg(AFE_DCCLK_CFG0,   0x2062, 0xFFE0);
		/* DCC power down control: DCC clk output */
		Ana_Set_Reg(AFE_DCCLK_CFG0,   0x2060, 0x0002);
		/* DCC generate enable*/
		Ana_Set_Reg(AFE_DCCLK_CFG0,   0x2061, 0x0001);
		/* DCCLK resync bypass */
		Ana_Set_Reg(AFE_DCCLK_CFG1,   0x0100, 0x0100);
	} else {
		/* Disable PGA DCC CLK */
		Ana_Set_Reg(AFE_DCCLK_CFG0,   0x2060, 0x0001);
		/* VOW source clock power off */
		Ana_Set_Reg(AFE_DCCLK_CFG0,   0x2062, 0x0002);
		/* PGA DCC CLK divider */
		Ana_Set_Reg(AFE_DCCLK_CFG0,   0x2062, 0xFFE0);
		/* DCC mode MT6358 E1 and E2 work around */
		if ((pmic_version == 0x5810) || (pmic_version == 0x5820))
			ClsqEnable(false);
		VOW13MCK_Enable(false); /*VOW clock power down enable*/
	}
}

static void VOW_ACC_CLK_Enable(bool enable)
{
	unsigned int pmic_version = Ana_Get_Reg(SWCID);

	if (enable == true) {
		VOW13MCK_Enable(true); /* VOW13M_CK power on */
		/* DCC mode MT6358 E1 work around */
		if (pmic_version == 0x5810)
			ClsqEnable(true);
		/* VOW source clock power on, vow_1p6m_800k_sel=1.6m */
		Ana_Set_Reg(AFE_VOW_TOP, 0x4000, 0x8000);
	} else {
		Ana_Set_Reg(AFE_VOW_TOP, 0xC000, 0x8000);
		/* DCC mode MT6358 E1 work around */
		if (pmic_version == 0x5810)
			ClsqEnable(false);
		VOW13MCK_Enable(false); /*VOW clock power down enable*/
	}
}

static void VOW_DMIC_CLK_Enable(bool enable)
{
	if (enable == true) {
		VOW13MCK_Enable(true); /* 0x0258 VOW13M_CK power on */

		/* 0x2070 VOW source clock power on, vow_1p6m_800k_sel=1.6m */
		Ana_Set_Reg(AFE_VOW_TOP, 0x4000, 0x8000);
	} else {
		Ana_Set_Reg(AFE_VOW_TOP, 0xC000, 0x8000); /*0x2070*/

		VOW13MCK_Enable(false); /*VOW clock power down enable*/
	}
}

static void VOW_MIC_DCC_Enable(int MicType, bool enable)
{
	if ((MicType >= VOW_MIC_TYPE_NUM) || (MicType < 0)) {
		pr_debug("%s(),Not support this Mic Type\n", __func__);
		return;
	}
	if (enable == true) {
		Ana_Set_Reg(AUDENC_ANA_CON3, 0x0009, 0X000D);
		/* for little signal be broken issue */
		Ana_Set_Reg(AUDENC_ANA_CON5, 0x0030, 0xFFFF);

		switch (MicType) {
		case VOW_MIC_TYPE_Handset_AMIC_DCC:
		case VOW_MIC_TYPE_Handset_AMIC_DCCECM:
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0025, 0x0075);
			if (MicType == VOW_MIC_TYPE_Handset_AMIC_DCCECM)
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0700, 0x0700);
			else
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x0700);

			/* 0x0D08 Enable audio L PGA */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0347, 0x07C7);
			break;

		case VOW_MIC_TYPE_Headset_MIC_DCC:
		case VOW_MIC_TYPE_Headset_MIC_DCCECM:
			Ana_Set_Reg(AUDENC_ANA_CON11, 0x00F1, 0x00F1);
			if (MicType == VOW_MIC_TYPE_Headset_MIC_DCCECM)
				Ana_Set_Reg(AUDENC_ANA_CON11, 0x0002, 0x0006);
			else
				Ana_Set_Reg(AUDENC_ANA_CON11, 0x0000, 0x0006);

			/* 0x0D08 Enable audio L PGA */
			Ana_Set_Reg(AUDENC_ANA_CON0,  0x0387, 0x07C7);
			break;
		default:
			break;
		}
		/* 0x0D08 Audio L ADC input sel : L PGA, Enable audio L ADC */
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x5347, 0x7000);

		msleep(20);/* delay */

		/* 0x0D08 Audio L PGA DCC precharge off */
		Ana_Set_Reg(AUDENC_ANA_CON0, 0x5343, 0x0004);

	} else {

		/* Audio L ADC input sel : off, Disable audio L ADC */
		Ana_Set_Reg(AUDENC_ANA_CON0,  0x0343, 0x7000);
		/* Disable audio L PGA */
		Ana_Set_Reg(AUDENC_ANA_CON0,  0x0000, 0x07C7);
		switch (MicType) {
		case VOW_MIC_TYPE_Handset_AMIC_DCC:
		case VOW_MIC_TYPE_Handset_AMIC_DCCECM:
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0x0075);
			break;
		case VOW_MIC_TYPE_Headset_MIC_DCC:
		case VOW_MIC_TYPE_Headset_MIC_DCCECM:
			/* Disable MICBIAS1 */
			Ana_Set_Reg(AUDENC_ANA_CON11, 0x0000, 0x00F7);
			break;
		default:
			break;
		}
		Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0X000D);
		/* for little signal be broken issue */
		Ana_Set_Reg(AUDENC_ANA_CON5, 0x1515, 0xFFFF);
	}
}

static void VOW_MIC_ACC_Enable(int MicType, bool enable)
{
	if ((MicType >= VOW_MIC_TYPE_NUM) || (MicType < 0)) {
		pr_debug("%s(),Not support this Mic Type\n", __func__);
		return;
	}
	if (enable == true) {
		/* for little signal be broken issue */
		Ana_Set_Reg(AUDENC_ANA_CON5, 0x0030, 0xFFFF);

		switch (MicType) {
		case VOW_MIC_TYPE_Handset_AMIC:
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0009, 0x000D);
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0025, 0x0075);
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x5000, 0x7000);
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x5041, 0x00C1);
			/* Short body to ground in PGA */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0009, 0x1000);
			/* Audio L PGA 18 dB gain */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x5341, 0x0700);
			break;
		case VOW_MIC_TYPE_Headset_MIC:
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0009, 0x000D);
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0025, 0x0075);
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x5000, 0x7000);
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x5081, 0x00C1);
			/* Short body to ground in PGA */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0009, 0x1000);
			/* Audio L PGA 18 dB gain */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x5381, 0x0700);
			break;
		default:
			break;
		}
	} else {
		/* Audio L ADC input sel : off, Disable audio L ADC */
		Ana_Set_Reg(AUDENC_ANA_CON0,  0x0341, 0x7000);
		/* Disable audio L PGA */
		Ana_Set_Reg(AUDENC_ANA_CON0,  0x0000, 0x07C7);
		switch (MicType) {
		case VOW_MIC_TYPE_Handset_AMIC:
			/* Disable MICBIAS0 */
			Ana_Set_Reg(AUDENC_ANA_CON9,  0x0000, 0x0075);
			break;
		case VOW_MIC_TYPE_Headset_MIC:
			/* Disable MICBIAS0 */
			Ana_Set_Reg(AUDENC_ANA_CON9,  0x0000, 0x0075);
			break;
		default:
			break;
		}
		Ana_Set_Reg(AUDENC_ANA_CON3,  0x0000, 0x000D);
		/* for little signal be broken issue */
		Ana_Set_Reg(AUDENC_ANA_CON5, 0x1515, 0xFFFF);
	}
}
#endif

static bool TurnOnVOWADcPower(int MicType, bool enable)
{
#ifdef CONFIG_MTK_VOW_SUPPORT
	pr_debug("%s MicType = %d enable = %d, mIsVOWOn=%d, mAudio_VOW_Mic_type=%d\n",
		 __func__, MicType, enable, mIsVOWOn, mAudio_VOW_Mic_type);

	if ((MicType >= VOW_MIC_TYPE_NUM) || (MicType < 0)) {
		pr_debug("%s(),Not support this Mic Type\n", __func__);
		return false;
	}

	/*already on, no need to set again*/
	if (enable == mIsVOWOn)
		return true;

	if (enable) {
		mIsVOWOn = true;
		/* SetVOWStatus(mIsVOWOn); */

		if (GetMicbias == 0) {
			/* save current micbias ref set by accdet */
			MicbiasRef = Ana_Get_Reg(AUDENC_ANA_CON9) & 0x0700;
			pr_debug("MicbiasRef=0x%x\n", MicbiasRef);
			GetMicbias = 1;
		}

		VOW_Pwr_Enable(MicType, true);

		switch (MicType) {
		/* for ACC Mic */
		case VOW_MIC_TYPE_Handset_AMIC:  /* AMIC_ACC */
		case VOW_MIC_TYPE_Headset_MIC: /* Earphone_ACC */
			VOW_ACC_CLK_Enable(true);
			VOW_MIC_ACC_Enable(MicType, true);
			break;
		/* for DCC Mic */
		case VOW_MIC_TYPE_Handset_AMIC_DCC:  /* AMIC_DCC */
		case VOW_MIC_TYPE_Handset_AMIC_DCCECM:
		case VOW_MIC_TYPE_Headset_MIC_DCC:   /* Earphone_DCC */
		case VOW_MIC_TYPE_Headset_MIC_DCCECM:
			VOW_DCC_CLK_Enable(true);
			VOW_MIC_DCC_Enable(MicType, true);
			break;
		/* for Digital Mic */
		case VOW_MIC_TYPE_Handset_DMIC:  /* DMIC */
		case VOW_MIC_TYPE_Handset_DMIC_800K:
			VOW_DMIC_CLK_Enable(true);
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0025, 0x0075);
			Ana_Set_Reg(AUDENC_ANA_CON8, 0x0005, 0x0007);
			/* 0xD18 Enable DMIC*/
			break;
		case VOW_MIC_TYPE_Handset_DMIC_VENDOR01:
			VOW_DMIC_CLK_Enable(true);
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0025, 0x0075);
			Ana_Set_Reg(AUDENC_ANA_CON8, 0x0005, 0x0007);
			/* 0xD18 Enable DMIC*/
			/* Set Eint GPIO */
			VowDrv_SetSmartDevice_GPIO(true);
			break;
		default:
			break;
		}

		VOW_GPIO_Enable(true);

		/* VOW AMPREF Setting, set by MD32 after DC calibration */
		Ana_Set_Reg(AFE_VOW_CFG0, reg_AFE_VOW_CFG0, 0xffff);
		/* 0xffff */
		Ana_Set_Reg(AFE_VOW_CFG1, reg_AFE_VOW_CFG1,
			    0xffff);   /*VOW A,B timeout initial value 0x0200*/
		Ana_Set_Reg(AFE_VOW_CFG2, reg_AFE_VOW_CFG2,
			    0xffff);   /*VOW A,B value setting 0x2424*/
		Ana_Set_Reg(AFE_VOW_CFG3, reg_AFE_VOW_CFG3,
			    0xffff);
		Ana_Set_Reg(AFE_VOW_CFG4, reg_AFE_VOW_CFG4,
			    0x000f);   /*gamma K value setting 0x029E*/
		Ana_Set_Reg(AFE_VOW_CFG5, reg_AFE_VOW_CFG5,
			0xffff);/*N mini value setting 0x0000*/

		if (MicType == VOW_MIC_TYPE_Handset_DMIC_VENDOR01) {
			Ana_Set_Reg(AFE_VOW_CFG4, 0x024E, 0xfff0);
			/* 16k */
			Ana_Set_Reg(AFE_VOW_POSDIV_CFG0, 0x0C0A, 0xffff);
			/* 812.5k */
		} else if (MicType == VOW_MIC_TYPE_Handset_DMIC_800K) {
			/* gamma K value and vow mtkaif tx setting */
			Ana_Set_Reg(AFE_VOW_CFG4, 0x024E, 0xfff0); /* 16k */
			/* vow posdiv and cic mode configure */
			Ana_Set_Reg(AFE_VOW_POSDIV_CFG0, 0x0C0A, 0xffff);
			/* 812.5k */
		} else if (MicType == VOW_MIC_TYPE_Handset_DMIC) {
			/* gamma K value and vow mtkaif tx setting */
			Ana_Set_Reg(AFE_VOW_CFG4, 0x029E, 0xfff0); /* 16k */
			/* vow posdiv and cic mode configure */
			Ana_Set_Reg(AFE_VOW_POSDIV_CFG0, 0x0C00, 0xffff);
			/* 1625k */
		} else {
			/* gamma K value and vow mtkaif tx setting */
			Ana_Set_Reg(AFE_VOW_CFG4, 0x029E, 0xfff0); /* 16k */
			/* vow posdiv and cic mode configure */
			Ana_Set_Reg(AFE_VOW_POSDIV_CFG0, 0x0C00, 0xffff);
			/* 1.6m */
		}
		TurnOnVOWPeriodicOnOff(MicType, reg_AFE_VOW_PERIODIC, true);

#ifndef VOW_STANDALONE_CONTROL
		if (MicType == VOW_MIC_TYPE_Handset_DMIC) {

			/* VowDrv_SetDmicLowPower(false); */
			VowDrv_SetMtkifType(2);  /* 2: DMIC */

			Ana_Set_Reg(AFE_VOW_TOP, 0x20C0, 0x20C0);
			/*VOW enable, with bit7*/
		} else if (MicType == VOW_MIC_TYPE_Handset_DMIC_800K) {

			/* VowDrv_SetDmicLowPower(true); */
			VowDrv_SetMtkifType(3);  /* 3: DMIC_LP */

			Ana_Set_Reg(AFE_VOW_TOP, 0x20C0, 0x20C0);
			/*VOW enable, with bit7*/
		} else if (MicType == VOW_MIC_TYPE_Handset_DMIC_VENDOR01) {
			/* same as VOW_MIC_TYPE_Handset_DMIC_800K */
			VowDrv_SetMtkifType(3);  /* 3: DMIC_LP */
		} else {
			/* Normal */
			/* VowDrv_SetDmicLowPower(false); */
			VowDrv_SetMtkifType(1);  /* 1: AMIC */
		}
#endif /* #ifndef VOW_STANDALONE_CONTROL */


		/*VOW enable, set AFE_VOW_TOP in VOW kernel driver*/
		/*need to inform VOW driver mic type*/
		VowDrv_EnableHW(true);
		VowDrv_ChangeStatus();

	} else { /* disable VOW */

		TurnOnVOWPeriodicOnOff(MicType, reg_AFE_VOW_PERIODIC, false);

		VowDrv_EnableHW(false);
		VowDrv_ChangeStatus();
		msleep(20);

		VOW_GPIO_Enable(false);

		VowDrv_SetMtkifType(0);  /* 0: NONE */
		if ((MicType == VOW_MIC_TYPE_Handset_DMIC)
		    || (MicType == VOW_MIC_TYPE_Handset_DMIC_800K)) {
			/* VowDrv_SetDmicLowPower(false); */
			Ana_Set_Reg(AFE_VOW_TOP, 0x0000, 0x20C0);
			/*VOW disable, with bit7*/
		}
		switch (MicType) {
		/* for ACC Mic */
		case VOW_MIC_TYPE_Handset_AMIC:
		case VOW_MIC_TYPE_Headset_MIC:
			/*turn off analog part*/
			VOW_MIC_ACC_Enable(MicType, false);
			VOW_ACC_CLK_Enable(false);
			break;
		/* for DCC Mic */
		case VOW_MIC_TYPE_Handset_AMIC_DCC:
		case VOW_MIC_TYPE_Handset_AMIC_DCCECM:
		case VOW_MIC_TYPE_Headset_MIC_DCC:
		case VOW_MIC_TYPE_Headset_MIC_DCCECM:
			/*turn off analog part*/
			VOW_MIC_DCC_Enable(MicType, false);
			VOW_DCC_CLK_Enable(false);
			break;
		/* for Digital Mic */
		case VOW_MIC_TYPE_Handset_DMIC:
		case VOW_MIC_TYPE_Handset_DMIC_800K:
			/* Disable DMIC */
			Ana_Set_Reg(AUDENC_ANA_CON8,  0x0004, 0x0007);
			/*0x0D08*/
			Ana_Set_Reg(AUDENC_ANA_CON9,  0x0000, 0x0075);
			/*0x0D1A*/
			VOW_DMIC_CLK_Enable(false);
			break;
		case VOW_MIC_TYPE_Handset_DMIC_VENDOR01:
			/* Set Eint GPIO */
			VowDrv_SetSmartDevice_GPIO(false);
			Ana_Set_Reg(AUDENC_ANA_CON9,  0x0000, 0x0075);
			/*0x0D1A*/
			Ana_Set_Reg(AFE_VOW_TOP, 0x0000, 0x0080);
			/* bit7 , clock select */
			VOW_DMIC_CLK_Enable(false);
			break;
		default:
			break;
		}

		VOW_Pwr_Enable(MicType, false);

		mIsVOWOn = false;
		/* SetVOWStatus(mIsVOWOn); */
		GetMicbias = 0;
	}
#endif /* #ifdef CONFIG_MTK_VOW_SUPPORT */
	return true;
}

static const char *const Audio_VOW_ADC_Function[] = { "Off", "On" };
static const char *const Audio_VOW_Digital_Function[] = { "Off", "On" };

static const char *const Audio_VOW_MIC_Type[] = {
	"HandsetAMIC", "HeadsetMIC", "HandsetDMIC", "HandsetDMIC_800K",
	"HandsetAMIC_DCC", "HeadsetMIC_DCC", "HandsetAMIC_DCCECM",
	"HeadsetMIC_DCCECM", "HandsetDMIC_VENDOR01"
};
/* here start uplink power function */
static const char *const ADC_function[] = { "Off", "On" };
static const char *const ADC_power_mode[] = { "normal", "lowpower" };
static const char *const PreAmp_Mux_function[] = {
	"OPEN", "IN_ADC1", "IN_ADC2", "IN_ADC3"
};
enum preamp_input_select {
	PREAMP_INPUT_SELECT_NONE = 0,
	PREAMP_INPUT_SELECT_AIN0,
	PREAMP_INPUT_SELECT_AIN1,
	PREAMP_INPUT_SELECT_AIN2,
	NUM_PREAMP_INPUT_SELECT,
};

/* OPEN:0, IN_ADC1: 1, IN_ADC2:2, IN_ADC3:3 */
static const char *const ADC_UL_PGA_GAIN[] = {
	"0Db", "6Db", "12Db", "18Db", "24Db", "30Db"
};
static const char *const Pmic_Digital_Mux[] = {
	"ADC1", "ADC2", "ADC3", "ADC4"
};
static const char *const Adc_Input_Sel[] = {
	"idle", "AIN", "Preamp"
};

static const char *const Audio_AnalogMic_Mode[] = {
	"ACCMODE", "DCCMODE", "DMIC", "DCCECMDIFFMODE", "DCCECMSINGLEMODE"
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
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(PreAmp_Mux_function),
			    PreAmp_Mux_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ADC_function), ADC_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_ADC_Function),
			    Audio_VOW_ADC_Function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_Digital_Function),
			    Audio_VOW_Digital_Function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_VOW_MIC_Type),
			    Audio_VOW_MIC_Type),
};

static int Audio_ADC1_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->dev_power[ANA_DEV_IN_ADC1];
	return 0;
}

static int Audio_ADC1_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	mutex_lock(&Ana_Power_Mutex);
	if (ucontrol->value.integer.value[0]) {
		if (mAudio_Analog_Mic1_mode == ANA_UL_MODE_ACC)
			TurnOnADcPowerACC(ANA_DEV_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode == ANA_UL_MODE_DCC)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC1, true, 0);
		else if (mAudio_Analog_Mic1_mode == ANA_UL_MODE_DMIC)
			TurnOnADcPowerDmic(ANA_DEV_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode == ANA_UL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC1, true, 1);
		else if (mAudio_Analog_Mic1_mode == ANA_UL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC1, true, 2);

		mCodec_data->dev_power[ANA_DEV_IN_ADC1] =
			ucontrol->value.integer.value[0];
	} else {
		mCodec_data->dev_power[ANA_DEV_IN_ADC1] =
			ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic1_mode == ANA_UL_MODE_ACC)
			TurnOnADcPowerACC(ANA_DEV_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == ANA_UL_MODE_DCC)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC1, false, 0);
		else if (mAudio_Analog_Mic1_mode == ANA_UL_MODE_DMIC)
			TurnOnADcPowerDmic(ANA_DEV_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == ANA_UL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC1, false, 1);
		else if (mAudio_Analog_Mic1_mode == ANA_UL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC1, false, 2);

	}
	mutex_unlock(&Ana_Power_Mutex);
	return 0;
}

static int Audio_ADC2_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->dev_power[ANA_DEV_IN_ADC2];
	return 0;
}

static int Audio_ADC2_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	mutex_lock(&Ana_Power_Mutex);
	if (ucontrol->value.integer.value[0]) {
		if (mAudio_Analog_Mic2_mode == ANA_UL_MODE_ACC)
			TurnOnADcPowerACC(ANA_DEV_IN_ADC2, true);
		else if (mAudio_Analog_Mic2_mode == ANA_UL_MODE_DCC)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC2, true, 0);
		else if (mAudio_Analog_Mic2_mode == ANA_UL_MODE_DMIC)
			TurnOnADcPowerDmic(ANA_DEV_IN_ADC2, true);
		else if (mAudio_Analog_Mic2_mode == ANA_UL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC2, true, 1);
		else if (mAudio_Analog_Mic2_mode == ANA_UL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC2, true, 2);

		mCodec_data->dev_power[ANA_DEV_IN_ADC2] =
			ucontrol->value.integer.value[0];
	} else {
		mCodec_data->dev_power[ANA_DEV_IN_ADC2] =
			ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic2_mode == ANA_UL_MODE_ACC)
			TurnOnADcPowerACC(ANA_DEV_IN_ADC2, false);
		else if (mAudio_Analog_Mic2_mode == ANA_UL_MODE_DCC)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC2, false, 0);
		else if (mAudio_Analog_Mic2_mode == ANA_UL_MODE_DMIC)
			TurnOnADcPowerDmic(ANA_DEV_IN_ADC2, false);
		else if (mAudio_Analog_Mic2_mode == ANA_UL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC2, false, 1);
		else if (mAudio_Analog_Mic2_mode == ANA_UL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(ANA_DEV_IN_ADC2, false, 2);

	}
	mutex_unlock(&Ana_Power_Mutex);
	return 0;
}

static int Audio_ADC3_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC3_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC4_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC4_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC1_Sel_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() = %d\n", __func__,
		 mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_MIC1]);
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_MIC1];
	return 0;
}

static int Audio_ADC1_Sel_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x00 << 13), 0x6000);
	/* pinumx sel */
	else if (ucontrol->value.integer.value[0] == 1)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x01 << 13), 0x6000);
	/* AIN0 */
	else if (ucontrol->value.integer.value[0] == 2)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x02 << 13), 0x6000);
	/* Left preamp */
	else
		pr_warn("%s() [AudioWarn]\n ", __func__);

	pr_debug("%s() done\n", __func__);
	mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_MIC1] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_ADC2_Sel_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() = %d\n", __func__,
		 mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_MIC2]);
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_MIC2];
	return 0;
}

static int Audio_ADC2_Sel_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x00 << 13), 0x6000);
	/* pinumx sel */
	else if (ucontrol->value.integer.value[0] == 1)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x01 << 13), 0x6000);
	/* AIN0 */
	else if (ucontrol->value.integer.value[0] == 2)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x02 << 13), 0x6000);
	/* Right preamp */
	else
		pr_warn("%s() [AudioWarn]\n ", __func__);

	pr_debug("%s() done\n", __func__);
	mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_MIC2] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_ADC3_Sel_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC3_Sel_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC4_Sel_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_ADC4_Sel_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static bool AudioPreAmp1_Sel(int Mul_Sel)
{
	if (Mul_Sel >= 0 && Mul_Sel < NUM_PREAMP_INPUT_SELECT)
		Ana_Set_Reg(AUDENC_ANA_CON0, Mul_Sel << 6, 0x3 << 6);
	else
		pr_warn("%s(), error, Mul_Sel = %d", __func__, Mul_Sel);

	return true;
}

static int Audio_PreAmp1_Get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_PREAMP_1];
	return 0;
}

static int Audio_PreAmp1_Set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] =
		ucontrol->value.integer.value[0];
	AudioPreAmp1_Sel(mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);

	return 0;
}

static bool AudioPreAmp2_Sel(int Mul_Sel)
{
	pr_debug("%s Mul_Sel = %d ", __func__, Mul_Sel);

	if (Mul_Sel >= 0 && Mul_Sel < NUM_PREAMP_INPUT_SELECT)
		Ana_Set_Reg(AUDENC_ANA_CON1, Mul_Sel << 6, 0x3 << 6);
	else
		pr_warn("%s(), error, Mul_Sel = %d", __func__, Mul_Sel);

	return true;
}

static int Audio_PreAmp2_Get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_PREAMP_2];
	return 0;
}

static int Audio_PreAmp2_Set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(PreAmp_Mux_function)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_PREAMP_2] =
		ucontrol->value.integer.value[0];
	AudioPreAmp2_Sel(mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_PREAMP_2]);
	return 0;
}

/* PGA1: PGA_L */
static int Audio_PGA1_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_gain[ANA_GAIN_MICAMP1];
	return 0;
}

static int Audio_PGA1_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	Ana_Set_Reg(AUDENC_ANA_CON0, (index << 8), 0x0700);
	mCodec_data->ana_gain[ANA_GAIN_MICAMP1] =
		ucontrol->value.integer.value[0];
	return 0;
}

/* PGA2: PGA_R */
static int Audio_PGA2_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_gain[ANA_GAIN_MICAMP2];
	return 0;
}

static int Audio_PGA2_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	Ana_Set_Reg(AUDENC_ANA_CON1, index << 8, 0x0700);
	mCodec_data->ana_gain[ANA_GAIN_MICAMP2] =
		ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_PGA3_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_PGA3_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_PGA4_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_PGA4_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_MicSource1_Get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		mCodec_data->ana_mux[MICSOURCE_MUX_IN_1];
	return 0;
}

static int Audio_MicSource1_Set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	/* ADC1 Mic source selection,
	 * "ADC1" is main_mic, "ADC2" is headset_mic
	 */
	int index = 0;

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Pmic_Digital_Mux)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	pr_debug("%s() index = %d done\n", __func__, index);
	mCodec_data->ana_mux[MICSOURCE_MUX_IN_1] =
		ucontrol->value.integer.value[0];

	return 0;
}

static int Audio_MicSource2_Get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_MicSource2_Set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_MicSource3_Get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_MicSource3_Set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}


static int Audio_MicSource4_Get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

static int Audio_MicSource4_Set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 removed */
	return 0;
}

/* Mic ACC/DCC Mode Setting */
static int Audio_Mic1_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic1_mode;
	return 0;
}

static int Audio_Mic1_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic1_mode = ucontrol->value.integer.value[0];
	pr_debug("%s() mAudio_Analog_Mic1_mode = %d\n", __func__,
		 mAudio_Analog_Mic1_mode);
	return 0;
}

static int Audio_Mic2_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic2_mode;
	return 0;
}

static int Audio_Mic2_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_warn("return -EINVAL\n");
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
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic3_mode;
	return 0;
}

static int Audio_Mic3_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_warn("return -EINVAL\n");
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
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic4_mode;
	return 0;
}

static int Audio_Mic4_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic4_mode = ucontrol->value.integer.value[0];
	pr_debug("%s() mAudio_Analog_Mic4_mode = %d\n",
		 __func__, mAudio_Analog_Mic4_mode);
	return 0;
}

static int Audio_Adc_Power_Mode_Get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %d\n", __func__, mAdc_Power_Mode);
	ucontrol->value.integer.value[0] = mAdc_Power_Mode;
	return 0;
}

static int Audio_Adc_Power_Mode_Set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_power_mode)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mAdc_Power_Mode = ucontrol->value.integer.value[0];
	pr_debug("%s() mAdc_Power_Mode = %d\n", __func__, mAdc_Power_Mode);
	return 0;
}

static int Audio_Vow_ADC_Func_Switch_Get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %d\n", __func__, mAudio_Vow_Analog_Func_Enable);
	ucontrol->value.integer.value[0] = mAudio_Vow_Analog_Func_Enable;
	return 0;
}

static int Audio_Vow_ADC_Func_Switch_Set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Audio_VOW_ADC_Function)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		TurnOnVOWADcPower(mAudio_VOW_Mic_type, true);
	else
		TurnOnVOWADcPower(mAudio_VOW_Mic_type, false);


	mAudio_Vow_Analog_Func_Enable = ucontrol->value.integer.value[0];

	return 0;
}

static int Audio_Vow_Digital_Func_Switch_Get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %d\n", __func__, mAudio_Vow_Digital_Func_Enable);
	ucontrol->value.integer.value[0] = mAudio_Vow_Digital_Func_Enable;
	return 0;
}

static int Audio_Vow_Digital_Func_Switch_Set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(
		    Audio_VOW_Digital_Function)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0])
		TurnOnVOWDigitalHW(true);
	else
		TurnOnVOWDigitalHW(false);


	mAudio_Vow_Digital_Func_Enable = ucontrol->value.integer.value[0];

	return 0;
}


static int VOW_MIC_TYPE_Select_Get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %d\n", __func__, mAudio_VOW_Mic_type);
	ucontrol->value.integer.value[0] = mAudio_VOW_Mic_type;
	return 0;
}

static int VOW_MIC_TYPE_Select_Set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Audio_VOW_MIC_Type)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_VOW_Mic_type = ucontrol->value.integer.value[0];
	pr_debug("%s() mAudio_VOW_Mic_type = %d\n",
		__func__, mAudio_VOW_Mic_type);
	return 0;
}


static int Audio_Vow_Cfg0_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int value = reg_AFE_VOW_CFG0;

	pr_debug("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg0_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %d\n",
		__func__, (int)(ucontrol->value.integer.value[0]));
	reg_AFE_VOW_CFG0 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg1_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int value = reg_AFE_VOW_CFG1;

	pr_debug("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg1_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	reg_AFE_VOW_CFG1 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg2_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int value = reg_AFE_VOW_CFG2;

	pr_debug("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg2_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	reg_AFE_VOW_CFG2 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg3_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int value = reg_AFE_VOW_CFG3;

	pr_debug("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg3_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	reg_AFE_VOW_CFG3 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg4_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int value = reg_AFE_VOW_CFG4;

	pr_debug("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg4_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	reg_AFE_VOW_CFG4 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_Cfg5_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int value = reg_AFE_VOW_CFG5;

	pr_debug("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Cfg5_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	reg_AFE_VOW_CFG5 = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Vow_State_Get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int value = mIsVOWOn;

	pr_debug("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_State_Set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int Audio_Vow_Periodic_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int value = reg_AFE_VOW_PERIODIC;

	pr_debug("%s()  = %d\n", __func__, value);
	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int Audio_Vow_Periodic_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %ld\n", __func__, ucontrol->value.integer.value[0]);
	reg_AFE_VOW_PERIODIC = ucontrol->value.integer.value[0];
	return 0;
}

static bool ul_lr_swap_enable;

static int Audio_UL_LR_Swap_Get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ul_lr_swap_enable;
	return 0;
}

static int Audio_UL_LR_Swap_Set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ul_lr_swap_enable = ucontrol->value.integer.value[0];
	Ana_Set_Reg(AFE_UL_DL_CON0, ul_lr_swap_enable << 15, 0x1 << 15);
	return 0;
}

static bool SineTable_DAC_HP_flag;
static bool SineTable_UL2_flag;

static int SineTable_UL2_Set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0]) {
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0002, 0x2);
		/* set UL from sinetable */
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0080, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
	} else {
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000, 0x2);
		/* set UL from normal path */
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0000, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
	}
	SineTable_UL2_flag = ucontrol->value.integer.value[0];
	return 0;
}

static int SineTable_UL2_Get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = SineTable_UL2_flag;
	return 0;
}

static int Pmic_Loopback_Type;

static int Pmic_Loopback_Get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = Pmic_Loopback_Type;
	return 0;
}

static int Pmic_Loopback_Set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(Pmic_LPBK_function)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0) {
		/* disable pmic lpbk */
		Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000, 0x0001);
		/* power off uplink */
		Ana_Set_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0, 0x0, 0xffff);
		/* disable new lpbk 2 */
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001);
		/* turn off afe UL & DL */

		/* disable aud_pad RX & TX fifos */
		Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x0, 0x101);

		set_capture_gpio(false);
		TurnOffDacPower();
	} else if (ucontrol->value.integer.value[0] > 0) {
	/* enable pmic lpbk */
		pr_debug("set PMIC LPBK3, DLSR=%d, ULSR=%d\n",
			 mBlockSampleRate[ANA_DEV_OUT_DAC],
			 mBlockSampleRate[ANA_DEV_IN_ADC]);

		/* set dl part */
		TurnOnDacPower(ANA_DEV_OUT_HEADSETL);
		set_capture_gpio(true);

		Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x8000, 0xffff);
		/* power on clock */

		/* enable aud_pad TX fifos */
		Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x3100, 0xff00);

		/* Set UL Part */
		Ana_Set_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0, 0x2, 0xffff);
		/* enable new lpbk 2 */

		Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0001, 0xffff);
		/* power on uplink */

		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000,
			    0x0002);       /* configure ADC setting */
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0xffff);
		/* turn on afe */
	}

	/* remember to set, AP side 0xe00 [1] = 1, for new lpbk2 */

	pr_debug("%s() done\n", __func__);
	Pmic_Loopback_Type = ucontrol->value.integer.value[0];
	return 0;
}

static int SineTable_DAC_HP_Get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = SineTable_DAC_HP_flag;
	return 0;
}

static int SineTable_DAC_HP_Set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	/* 6752 TODO? */
	return 0;
}

static void ADC_LOOP_DAC_Func(int command)
{
	/* 6752 TODO? */
}

static bool DAC_LOOP_DAC_HS_flag;
static int ADC_LOOP_DAC_HS_Get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = DAC_LOOP_DAC_HS_flag;
	return 0;
}

static int ADC_LOOP_DAC_HS_Set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
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
static int ADC_LOOP_DAC_HP_Get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = DAC_LOOP_DAC_HP_flag;
	return 0;
}

static int ADC_LOOP_DAC_HP_Set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
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
	return 0;
}

static int codec_debug_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = codec_debug_enable;
	return 0;

}

static int codec_debug_set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	codec_debug_enable = ucontrol->value.integer.value[0];
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

static const struct snd_kcontrol_new mt6358_pmic_Test_controls[] = {
	SOC_ENUM_EXT("SineTable_DAC_HP", Pmic_Test_Enum[0],
		SineTable_DAC_HP_Get,
		     SineTable_DAC_HP_Set),
	SOC_ENUM_EXT("DAC_LOOP_DAC_HS", Pmic_Test_Enum[1], ADC_LOOP_DAC_HS_Get,
		     ADC_LOOP_DAC_HS_Set),
	SOC_ENUM_EXT("DAC_LOOP_DAC_HP", Pmic_Test_Enum[2], ADC_LOOP_DAC_HP_Get,
		     ADC_LOOP_DAC_HP_Set),
	SOC_ENUM_EXT("Voice_Call_DAC_DAC_HS", Pmic_Test_Enum[3],
	Voice_Call_DAC_DAC_HS_Get,
		     Voice_Call_DAC_DAC_HS_Set),
	SOC_ENUM_EXT("SineTable_UL2", Pmic_Test_Enum[4], SineTable_UL2_Get,
	SineTable_UL2_Set),
	SOC_ENUM_EXT("Pmic_Loopback", Pmic_Test_Enum[5], Pmic_Loopback_Get,
	Pmic_Loopback_Set),
	SOC_SINGLE_EXT("Codec_Debug_Enable", SND_SOC_NOPM, 0, 0xffffffff, 0,
		       codec_debug_get,
		       codec_debug_set),
};

static const struct snd_kcontrol_new mt6358_UL_Codec_controls[] = {
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
	SOC_ENUM_EXT("Audio_Preamp2_Switch", Audio_UL_Enum[22],
		     Audio_PreAmp2_Get,
		     Audio_PreAmp2_Set),
	SOC_ENUM_EXT("Audio_UL_LR_Swap", Audio_UL_Enum[23],
		     Audio_UL_LR_Swap_Get,
		     Audio_UL_LR_Swap_Set),
	SOC_ENUM_EXT("Audio_Vow_ADC_Func_Switch", Audio_UL_Enum[24],
		     Audio_Vow_ADC_Func_Switch_Get,
		     Audio_Vow_ADC_Func_Switch_Set),
	SOC_ENUM_EXT("Audio_Vow_Digital_Func_Switch", Audio_UL_Enum[25],
		     Audio_Vow_Digital_Func_Switch_Get,
		     Audio_Vow_Digital_Func_Switch_Set),
	SOC_ENUM_EXT("VOW_MIC_TYPE_Select", Audio_UL_Enum[26],
		     VOW_MIC_TYPE_Select_Get,
		     VOW_MIC_TYPE_Select_Set),
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
};

static int read_efuse_hp_impedance_current_calibration(void)
{
	int ret = 0;
	int value, sign;

	pr_debug("+%s()\n", __func__);

	/* 1. enable efuse ctrl engine clock */
	Ana_Set_Reg(TOP_CKHWEN_CON0_CLR, 0x1 << 2, 0x1 << 2);
	Ana_Set_Reg(TOP_CKPDN_CON0_CLR, 0x1 << 4, 0x1 << 4);

	/* 2. set RG_OTP_RD_SW */
	Ana_Set_Reg(OTP_CON11, 0x0001, 0x0001);

	/* 3. set EFUSE addr */
	/* HPDET_COMP[6:0] @ efuse bit 1840 ~ 1846 */
	/* HPDET_COMP_SIGN @ efuse bit 1847 */
	/* 1840 / 8 = 230 --> 0xe6 */
	Ana_Set_Reg(OTP_CON0, 0xe6, 0xff);

	/* 4. Toggle RG_OTP_RD_TRIG */
	ret = Ana_Get_Reg(OTP_CON8);
	if (ret == 0)
		Ana_Set_Reg(OTP_CON8, 0x0001, 0x0001);
	else
		Ana_Set_Reg(OTP_CON8, 0x0000, 0x0001);

	/* 5. Polling RG_OTP_RD_BUSY */
	do {
		ret = Ana_Get_Reg(OTP_CON13) & 0x0001;
		usleep_range(100, 200);
		pr_debug("%s(), polling OTP_CON13 = 0x%x\n", __func__, ret);
	} while (ret == 1);

	/* Need to delay at least 1ms for 0xC1A and than can read */
	usleep_range(500, 1000);

	/* 6. Read RG_OTP_DOUT_SW */
	ret = Ana_Get_Reg(OTP_CON12);
	pr_debug("%s(), efuse = 0x%x\n", __func__, ret);
	sign = (ret >> 7) & 0x1;
	value = ret & 0x7f;
	value = sign ? -value : value;

	/* 7. Disables efuse_ctrl egine clock */
	Ana_Set_Reg(OTP_CON11, 0x0000, 0x0001);
	Ana_Set_Reg(TOP_CKPDN_CON0_SET, 0x1 << 4, 0x1 << 4);
	Ana_Set_Reg(TOP_CKHWEN_CON0_SET, 0x1 << 2, 0x1 << 2);

	pr_debug("-%s(), efuse: %d\n", __func__, value);
	return value;
}

static void mt6358_codec_init_reg(struct snd_soc_component *codec)
{
	pr_debug("%s\n", __func__);

	audckbufEnable(true);
	/* disable CLKSQ */
	Ana_Set_Reg(AUDENC_ANA_CON6, 0x0000, 0x0003);
	/* disable AUDGLB */
	Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1 << 4, 0x1 << 4);
	/* Turn off AUDNCP_CLKDIV engine clock,Turn off AUD 26M */
	Ana_Set_Reg(AUD_TOP_CKPDN_CON0, 0x66, 0x66);
	/* set pdn for golden setting */
	Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x00ff, 0x00ff);
	/* Disable HeadphoneL/HeadphoneR short circuit protection */
	Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3 << 12, 0x3 << 12);
	/* Disable voice short circuit protection */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x1 << 4, 0x1 << 4);
	/* disable LO buffer left short circuit protection */
	Ana_Set_Reg(AUDDEC_ANA_CON7, 0x1 << 4, 0x1 << 4);
	/* gpio miso driving set to default 4mA */
	Ana_Set_Reg(DRV_CON3, 0x8888, 0xffff);

	/* Enable mtkaif gpio SMT mode */
	Ana_Set_Reg(SMT_CON1, 0x0ff0, 0x0ff0);

	/* Set HP_EINT trigger level to 2.0v */
	Ana_Set_Reg(AUDENC_ANA_CON11, 0x1 << 10, 0x1 << 10);

	/* set gpio */
	set_playback_gpio(false);
	set_capture_gpio(false);

	audckbufEnable(false);
}

void InitCodecDefault(void)
{
	pr_debug("%s\n", __func__);
	mCodec_data->ana_gain[ANA_GAIN_MICAMP1] = 3;
	mCodec_data->ana_gain[ANA_GAIN_MICAMP2] = 3;
	mCodec_data->ana_gain[ANA_GAIN_MICAMP3] = 3;
	mCodec_data->ana_gain[ANA_GAIN_MICAMP4] = 3;
	mCodec_data->ana_gain[ANA_GAIN_HPOUTL] = 8;
	mCodec_data->ana_gain[ANA_GAIN_HPOUTR] = 8;
	mCodec_data->ana_gain[ANA_GAIN_HSOUTL] = 8;
	mCodec_data->ana_gain[ANA_GAIN_HSOUTR] = 8;

	mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_MIC1] =
		AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
	mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_MIC2] =
		AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
	mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_MIC3] =
		AUDIO_ANALOG_AUDIOANALOG_INPUT_PREAMP;
	mCodec_data->ana_mux[AUDIO_ANALOG_MUX_IN_MIC4] =
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
	mIsNeedPullDown = true;
}

static struct task_struct *dc_trim_task;
static int dc_trim_thread(void *arg)
{
	pr_debug("%s()\n", __func__);
	get_hp_lr_trim_offset();

#ifdef CONFIG_MTK_ACCDET
	/* By default, set mic mode as AUDIO_MIC_MODE_ACC */
	accdet_late_init(AUDIO_MIC_MODE_ACC);
#endif

	do_exit(0);
	return 0;
}

static int mt6358_component_probe(struct snd_soc_component *component)
{
	int ret = 0;

	pr_debug("%s()\n", __func__);

	if (mInitCodec == true)
		return 0;

	/* add codec controls */
	snd_soc_add_component_controls(component, mt6358_snd_controls,
				   ARRAY_SIZE(mt6358_snd_controls));
	snd_soc_add_component_controls(component, mt6358_UL_Codec_controls,
				   ARRAY_SIZE(mt6358_UL_Codec_controls));
	snd_soc_add_component_controls(component, mt6358_pmic_Test_controls,
				   ARRAY_SIZE(mt6358_pmic_Test_controls));
	snd_soc_add_component_controls(component, Audio_snd_auxadc_controls,
				   ARRAY_SIZE(Audio_snd_auxadc_controls));

	/* here to set  private data */
	mCodec_data = kzalloc(sizeof(struct mt6358_codec_priv), GFP_KERNEL);
	if (!mCodec_data) {
		/*pr_warn("Failed to allocate private data\n");*/
		return -ENOMEM;
	}
	snd_soc_component_set_drvdata(component, mCodec_data);

	memset((void *)mCodec_data, 0, sizeof(struct mt6358_codec_priv));
	mt6358_codec_init_reg(component);
	InitCodecDefault();
	efuse_current_calibrate = read_efuse_hp_impedance_current_calibration();
	mInitCodec = true;
	apply_n12db_gain = 0;

	dc_trim_task = kthread_create(dc_trim_thread, NULL, "dc_trim_thread");
	if (IS_ERR(dc_trim_task)) {
		ret = PTR_ERR(dc_trim_task);
		dc_trim_task = NULL;
		pr_debug("%s(), create dc_trim_thread failed, ret %d\n",
			__func__, ret);
	} else {
		wake_up_process(dc_trim_task);
	}
	return 0;
}

static int mt6358_component_remove(struct snd_soc_component *component)
{
	return 0;
}

static unsigned int mt6358_component_read(struct snd_soc_component *component,
					  unsigned int reg)
{
	Ana_Get_Reg(reg);
	return 0;
}

static int mt6358_component_write(struct snd_soc_component *component,
				  unsigned int reg,
				  unsigned int value)
{
	Ana_Set_Reg(reg, value, 0xffffffff);
	return 0;
}

static const struct snd_soc_component_driver mt6358_component_driver = {
	.name = CODEC_MT6358_NAME,
	.probe = mt6358_component_probe,
	.remove = mt6358_component_remove,
	.read = mt6358_component_read,
	.write = mt6358_component_write,
};

static int mtk_mt6358_codec_dev_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", MT_SOC_CODEC_NAME);
		pdev->name = pdev->dev.kobj.name;

		/* check if use hp depop flow */
		of_property_read_u32(pdev->dev.of_node,
				     "use_hp_depop_flow",
				     &mUseHpDepopFlow);

		pr_debug("%s(), use_hp_depop_flow = %d\n",
			 __func__, mUseHpDepopFlow);

	} else {
		pr_info("%s(), pdev->dev.of_node = NULL!!!\n", __func__);
	}

	pr_info("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev,
				      &mt6358_component_driver, mtk_6358_dai_codecs,
				      ARRAY_SIZE(mtk_6358_dai_codecs));
}

static int mtk_mt6358_codec_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;

}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_codec_63xx_of_ids[] = {
	{.compatible = "mediatek,mt_soc_codec_63xx",},
	{}
};
#endif

static struct platform_driver mtk_codec_6358_driver = {
	.driver = {
		.name = MT_SOC_CODEC_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt_soc_codec_63xx_of_ids,
#endif
	},
	.probe = mtk_mt6358_codec_dev_probe,
	.remove = mtk_mt6358_codec_dev_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_codec6358_dev;
#endif

static int __init mtk_mt6358_codec_init(void)
{
	pr_info("%s:\n", __func__);
#ifndef CONFIG_OF
	int ret = 0;

	soc_mtk_codec6358_dev = platform_device_alloc(MT_SOC_CODEC_NAME, -1);

	if (!soc_mtk_codec6358_dev)
		return -ENOMEM;


	ret = platform_device_add(soc_mtk_codec6358_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_codec6358_dev);
		return ret;
	}
#endif
	InitGlobalVarDefault();

	return platform_driver_register(&mtk_codec_6358_driver);
}

module_init(mtk_mt6358_codec_init);

static void __exit mtk_mt6358_codec_exit(void)
{
	platform_driver_unregister(&mtk_codec_6358_driver);
}

module_exit(mtk_mt6358_codec_exit);

/* Module information */
MODULE_DESCRIPTION("MTK  codec driver");
MODULE_LICENSE("GPL v2");
