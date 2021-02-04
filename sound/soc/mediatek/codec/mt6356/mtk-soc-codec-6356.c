/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *	mtk_soc_codec_63xx
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *	Audio codec stub file
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */

/*****************************************************************************
 *					 C O M P I L E R	F L A G S
 *****************************************************************************/

/*****************************************************************************
 *				E X T E R N A L	R E F E R E N C E S
 *****************************************************************************/

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

#ifdef CONFIG_MTK_ACCDET
#include "accdet.h"
#endif

#ifdef CONFIG_MTK_AUXADC_INTF
#include <mt-plat/mtk_auxadc_intf.h>
#include <mach/mtk_pmic.h>
#endif

#include "mtk-auddrv-def.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-gpio.h"

#include "mtk-soc-analog-type.h"
#include "mtk-soc-codec-63xx.h"

/* HP IMPEDANCE Current Calibration from EFUSE */
/* #define EFUSE_HP_IMPEDANCE */

/* static function declaration */
static bool AudioPreAmp1_Sel(int Mul_Sel);
static bool GetAdcStatus(void);
static void TurnOffDacPower(void);
static void TurnOnDacPower(int device);
static void setDlMtkifSrc(bool enable);
static int SetDcCompenSation(bool enable);
static void Voice_Amp_Change(bool enable);
static void Speaker_Amp_Change(bool enable);

static struct mt6356_codec_priv *mCodec_data;
static unsigned int mBlockSampleRate[AUDIO_ANALOG_DEVICE_INOUT_MAX] = {
	48000, 48000, 48000};

#define MAX_DL_SAMPLE_RATE (192000)
#define MAX_UL_SAMPLE_RATE (192000)

static DEFINE_MUTEX(Ana_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_buf_Ctrl_Mutex);
static DEFINE_MUTEX(Ana_Clk_Mutex);
static DEFINE_MUTEX(Ana_Power_Mutex);
static DEFINE_MUTEX(AudAna_lock);

static int mAudio_Analog_Mic1_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic2_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic3_mode = AUDIO_ANALOGUL_MODE_ACC;
static int mAudio_Analog_Mic4_mode = AUDIO_ANALOGUL_MODE_ACC;

enum { AUXADC_AVG_1,
	   AUXADC_AVG_4,
	   AUXADC_AVG_8,
	   AUXADC_AVG_16,
	   AUXADC_AVG_32,
	   AUXADC_AVG_64,
	   AUXADC_AVG_128,
	   AUXADC_AVG_256,
};

enum { MIC_BIAS_1p7 = 0,
	   MIC_BIAS_1p8,
	   MIC_BIAS_1p9,
	   MIC_BIAS_2p0,
	   MIC_BIAS_2p1,
	   MIC_BIAS_2p5,
	   MIC_BIAS_2p6,
	   MIC_BIAS_2p7,
};

enum { DL_GAIN_8DB = 0,
	   DL_GAIN_0DB = 8,
	   DL_GAIN_N_1DB = 9,
	   DL_GAIN_N_10DB = 18,
	   DL_GAIN_N_40DB = 0x1f,
};
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
	RG_AUDHPLFINETRIM_VAUDP15, RG_AUDHPRFINETRIM_VAUDP15,
	RG_AUDHPLTRIM_VAUDP15_SPKHP, RG_AUDHPRTRIM_VAUDP15_SPKHP,
	RG_AUDHPLFINETRIM_VAUDP15_SPKHP, RG_AUDHPRFINETRIM_VAUDP15_SPKHP;
#endif
#endif

static int mAdc_Power_Mode;
static unsigned int dAuxAdcChannel = 16;
static const int mDcOffsetTrimChannel = 9;
static bool mInitCodec;

int (*enable_dc_compensation)(bool enable) = NULL;
int (*set_lch_dc_compensation)(int value) = NULL;
int (*set_rch_dc_compensation)(int value) = NULL;
int (*set_ap_dmic)(bool enable) = NULL;
int (*set_hp_impedance_ctl)(bool enable) = NULL;

/* Jogi: Need? @{ */
#define SND_SOC_ADV_MT_FMTS                                                   \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |                  \
	 SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_U16_BE |                  \
	 SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE |                  \
	 SNDRV_PCM_FMTBIT_U24_LE | SNDRV_PCM_FMTBIT_U24_BE |                  \
	 SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S32_BE |                  \
	 SNDRV_PCM_FMTBIT_U32_LE | SNDRV_PCM_FMTBIT_U32_BE)

#define SND_SOC_STD_MT_FMTS                                                   \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |                  \
	 SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_U16_BE)
/* @} Build pass: */
#define SOC_HIGH_USE_RATE                                                     \
	(SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_192000)

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

static bool mAnaSuspend;
void SetAnalogSuspend(bool bEnable)
{
	pr_debug("%s bEnable ==%d mAnaSuspend = %d\n", __func__, bEnable,
		 mAnaSuspend);
	if ((bEnable == true) && (mAnaSuspend == false)) {
		/*Ana_Log_Print();*/
		SavePowerState();
		if (mCodec_data->mAudio_Ana_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == true) {
			mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = false;
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == true) {
			mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = false;
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] == true) {
			mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] = false;
			Voice_Amp_Change(false);
		}
		if (mCodec_data->mAudio_Ana_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] == true) {
			mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] = false;
			Speaker_Amp_Change(false);
		}
		/*Ana_Log_Print();*/
		mAnaSuspend = true;
	} else if ((bEnable == false) && (mAnaSuspend == true)) {
		/*Ana_Log_Print();*/
		if (mCodec_data->mAudio_BackUpAna_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == true) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
			mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = true;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == true) {
			Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
			mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = false;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] == true) {
			Voice_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] = false;
		}
		if (mCodec_data->mAudio_BackUpAna_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] == true) {
			Speaker_Amp_Change(true);
			mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] = false;
		}
		RestorePowerState();
		/*Ana_Log_Print();*/
		mAnaSuspend = false;
	}
}

static int audck_buf_Count;
void audckbufEnable(bool enable)
{
	pr_debug("%s(), audck_buf_Count = %d, enable = %d\n", __func__,
	       audck_buf_Count, enable);

	mutex_lock(&Ana_buf_Ctrl_Mutex);
	if (enable) {
		if (audck_buf_Count == 0) {
			Ana_Set_Reg(DCXO_CW14, 0x1 << 13, 0x1 << 13);
			pr_debug(
			    "-PMIC DCXO XO_AUDIO_EN_M enable, DCXO_CW14 = 0x%x\n",
			    Ana_Get_Reg(DCXO_CW14));
		}
		audck_buf_Count++;
	} else {
		audck_buf_Count--;
		if (audck_buf_Count == 0) {
			Ana_Set_Reg(DCXO_CW14, 0x0 << 13, 0x1 << 13);
			pr_debug(
				"-PMIC DCXO XO_AUDIO_EN_M disable, DCXO_CW14 = 0x%x\n",
				 Ana_Get_Reg(DCXO_CW14));
			/* if didn't close, 60uA leak at audio analog */
		}
		if (audck_buf_Count < 0) {
			pr_debug("audck_buf_Count count < 0\n");
			audck_buf_Count = 0;
		}
	}
	mutex_unlock(&Ana_buf_Ctrl_Mutex);
}

static int ClsqCount;
static void ClsqEnable(bool enable)
{
	pr_debug(
		"%s ClsqCount = %d enable = %d\n",
		__func__, ClsqCount, enable);
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
			pr_debug("%s count <0\n", __func__);
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
	pr_debug("%s enable = %d TopCkCount = %d\n", __func__, enable,
	       TopCkCount);
	mutex_lock(&Ana_Clk_Mutex);
	if (enable == true) {
		if (TopCkCount == 0) {
			Ana_Set_Reg(AUD_TOP_CKPDN_CON0, 0x0, 0x66);
			/* Turn on AUDNCP_CLKDIV engine clock,Turn on AUD 26M
			 */
		}
		TopCkCount++;
	} else {
		TopCkCount--;
		if (TopCkCount == 0) {
			Ana_Set_Reg(AUD_TOP_CKPDN_CON0, 0x66, 0x66);
			/* Turn off AUDNCP_CLKDIV engine clock,Turn off AUD 26M
			 */
		}

		if (TopCkCount < 0) {
			pr_debug("TopCkCount <0 =%d\n ", TopCkCount);
			TopCkCount = 0;
		}
	}
	mutex_unlock(&Ana_Clk_Mutex);
}

static int NvRegCount;
static void NvregEnable(bool enable)
{
	pr_debug("%s NvRegCount == %d enable = %d\n", __func__, NvRegCount,
	       enable);
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
			pr_debug("NvRegCount <0 =%d\n ", NvRegCount);
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
#ifdef CONFIG_MTK_AUXADC_INTF
	return pmic_get_auxadc_value(AUXADC_LIST_HPOFS_CAL);
#else
	return 0;
#endif
}

static int get_accdet_auxadc(void)
{
#ifdef CONFIG_MTK_AUXADC_INTF
	return pmic_get_auxadc_value(AUXADC_LIST_ACCDET);
#else
	return 0;
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

	pr_debug("%s(+)\n", __func__);

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
		while (reg_val == 1) {
			ret = pmic_read_interface(0xC1A, &reg_val, 0x1, 0);
			pr_debug(
			    "%s polling 0xC1A=0x%x\n", __func__,
			    reg_val);
		}

		udelay(1000); /* Need to delay at least 1ms for 0xC1A and than
			       * can read 0xC18
			       */
		/* 6. read data */
		efusevalue[j] = upmu_get_reg_value(0x0C18);
		pr_debug("HPoffset : efuse[%d]=0x%x\n", j, efusevalue[j]);
		j++;
	}

	/* 7. Disable efuse ctrl engine clock */
	ret = pmic_config_interface(0x024C, 0x0004, 0xFFFF, 0);
	ret = pmic_config_interface(0x026A, 0x0040, 0xFFFF, 0);

	RG_AUDHPLTRIM_VAUDP15 = (efusevalue[0] >> 10) & 0xf;
	RG_AUDHPRTRIM_VAUDP15 =
	    ((efusevalue[0] >> 14) & 0x3) + ((efusevalue[1] & 0x3) << 2);
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
	pr_debug("%s(-)\n", __func__);
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

static bool is_valid_hp_pga_idx(int reg_idx)
{
	return (reg_idx >= DL_GAIN_8DB && reg_idx <= DL_GAIN_N_10DB) ||
	       reg_idx == DL_GAIN_N_40DB;
}

static void headset_volume_ramp(int from, int to)
{
	int offset = 0, count = 1, reg_idx;

	if (!is_valid_hp_pga_idx(from) || !is_valid_hp_pga_idx(to))
		pr_debug("%s(), volume index is not valid, from %d, to %d\n",
			__func__, from, to);

	pr_debug("%s, from %d, to %d\n", __func__, from, to);

	if (to > from) {
		offset = to - from;
		while (offset > 0) {
			reg_idx = from + count;
			if (is_valid_hp_pga_idx(reg_idx)) {
				Ana_Set_Reg(ZCD_CON2, (reg_idx << 7) | reg_idx,
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
				Ana_Set_Reg(ZCD_CON2, (reg_idx << 7) | reg_idx,
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
	pr_debug("%s Mux = %d\n", __func__, Mux);
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

static void OpenTrimBufferHardware(bool enable, bool buffer_on)
{
	pr_debug("%s(), enable %d, buffer_on %d\n", __func__, enable,
		 buffer_on);

	if (enable) {
		TurnOnDacPower(AUDIO_ANALOG_DEVICE_OUT_HEADSETL);

		/* sdm output mute enable */
		Ana_Set_Reg(AFUNC_AUD_CON1, 0x0000, 0xffff);
		Ana_Set_Reg(AFUNC_AUD_CON0, 0xcbad, 0xffff);

		/* Disable headphone short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);
		/* Disable handset short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
		/* Disable linout short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);
		/* Reduce ESD resistance of AU_REFN */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc000, 0xffff);
		/* Set HPR/HPL gain as minimum (~ -40dB) */
		Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_40DB_REG, 0xffff);

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
		udelay(250);

		/* Enable cap-less LDOs (1.5V), LCLDO local sense */
		Ana_Set_Reg(AUDDEC_ANA_CON14, 0x1015, 0x1055);
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
			/* No Pull-down HPL/R to AVSS28_AUD */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x4033, 0xffff);
		} else {
			/* Pull-down HPL/R to AVSS28_AUD */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc033, 0xffff);
		}

		if (buffer_on) {
			/* Enable HP driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x30c0, 0xffff);
			/* Enable HP driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x30f0, 0xffff);
			/* Enable HP main CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0200, 0xffff);

			/* Enable HP main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0003, 0xffff);
			/* Enable HPR/L main output stage step by step */
			hp_main_output_ramp(true);
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

		/* apply volume setting, 0dB */
		headset_volume_ramp(DL_GAIN_N_40DB, DL_GAIN_0DB);

		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);

		/* Enable low-noise mode of DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0203, 0xffff);
		udelay(100);
	} else {
		/* HPR/HPL mux to open */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x0f00);
		/* Disable Audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x000f);

		/* Disable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1);

		/* decrease HPL/R gain to normal gain step by step */
		headset_volume_ramp(DL_GAIN_0DB, DL_GAIN_N_40DB);

		/* decrease HPR/L main output stage step by step */
		hp_main_output_ramp(false);

		/* disable HS driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
		/* disable LO driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);

		/* Disable HP main output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3);

		/* Disable HP driver core circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0x3 << 4);

		/* Disable HP driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0x3 << 6);

		/* Disable HP aux CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0, 0xff << 8);

		/* Enable HP main CMFB Switch */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x2 << 8, 0xff << 8);

		/* Pull-down HPL/R, HS, LO to AVSS28_AUD */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0xa8, 0xff);

		/* Disable IBIST */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x1 << 8, 0x1 << 8);
		/* Disable NV regulator (-1.2V) */
		Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0, 0x1);
		/* Disable cap-less LDOs (1.5V) */
		Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0, 0x1055);
		/* Disable NCP */
		Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x1, 0x1);

		/* Disable Pull-down HPL/R to AVSS28_AUD */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x4033, 0xffff);

		TurnOffDacPower();
	}
}

static bool OpenHeadPhoneImpedanceSetting(bool bEnable)
{
	pr_debug("%s benable = %d\n", __func__, bEnable);
	if (GetDLStatus() == true)
		return false;

	if (bEnable == true) {
		TurnOnDacPower(AUDIO_ANALOG_DEVICE_OUT_HEADSETL);

		/* Disable headphone short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);
		/* Disable handset short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
		/* Disable linout short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);
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
		/* Enable HP main CMFB Switch */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0200, 0xffff);

		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);
		/* Enable Audio L channel DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3009, 0xffff);

		/* Enable HPDET circuit, select DACLP as HPDET input and HPR as
		 * HPDET output
		 */
		Ana_Set_Reg(AUDDEC_ANA_CON8, 0x1900, 0xffff);

		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0203, 0xffff);
	} else {
		/* disable HPDET circuit */
		Ana_Set_Reg(AUDDEC_ANA_CON8, 0x0000, 0xff00);

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

		/* Enable HP main CMFB Switch */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x2 << 8, 0xff << 8);

		/* Pull-down HPL/R, HS, LO to AVSS28_AUD */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0xa8, 0xff);

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
	return true;
}

/* Headphone Impedance Detection */
/* Pmic Headphone Impedance varible */
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

static void
mtk_read_hp_detection_parameter(struct mtk_hpdet_param *hpdet_param)
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
	return DIV_ROUND_CLOSEST(3600000 / pcm_offset * aux_diff, 7832);
}

static int mtk_calculate_hp_impedance(int dc_init, int dc_input,
				      short pcm_offset,
				      const unsigned int detect_times)
{
	int dc_value;
	int r_tmp = 0;

	if (dc_input < dc_init) {
		pr_debug("%s(), Wrong[%d] : dc_input(%d) > dc_init(%d)\n",
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
		r_tmp = DIV_ROUND_CLOSEST(
		    r_tmp * 128 + efuse_current_calibrate, 128);
	}

	pr_debug("%s(), pcm_offset %d dcoffset %d detected resistor is %d\n",
	       __func__, pcm_offset, dc_value, r_tmp);

	return r_tmp;
}

#define PARALLEL_OHM 470
static int detect_impedance(void)
{
	const unsigned int kDetectTimes = 8;
	unsigned int counter;
	int dcSum = 0, detectSum = 0;
	int detectsOffset[kDetectTimes];
	int pick_impedance = 0, impedance = 0, phase_flag = 0;
	int dcValue = 0;
	struct mtk_hpdet_param hpdet_param;

	if (enable_dc_compensation && set_lch_dc_compensation &&
	    set_rch_dc_compensation) {
		set_lch_dc_compensation(0);
		set_rch_dc_compensation(0);
		enable_dc_compensation(true);
	} else {
		pr_debug("%s(), dc compensation ops not ready\n", __func__);
		return 0;
	}

	mtk_read_hp_detection_parameter(&hpdet_param);

	Ana_Set_Reg(AUXADC_CON10, AUXADC_AVG_64, 0x7);

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
			usleep_range(1 * 1000, 1 * 1000);
			dcSum = 0;
			for (counter = 0; counter < kDetectTimes; counter++) {
				detectsOffset[counter] =
				    audio_get_auxadc_value();
				dcSum = dcSum + detectsOffset[counter];
			}

			if ((dcSum / kDetectTimes) >
			    hpdet_param.auxadc_upper_bound) {
				pr_info(
				    "%s(), dcValue == 0, auxadc value %d > auxadc_upper_bound %d\n",
				    __func__, dcSum / kDetectTimes,
				    hpdet_param.auxadc_upper_bound);
				impedance = auxcable_impedance;
				break;
			}
		}

		/* start checking */
		if (dcValue == hpdet_param.dc_Phase0) {
			usleep_range(1 * 1000, 1 * 1000);
			detectSum = 0;
			detectSum = audio_get_auxadc_value();

			if ((dcSum / kDetectTimes) == detectSum) {
				pr_info("%s(), dcSum / kDetectTimes %d == detectSum %d\n",
					__func__, dcSum / kDetectTimes,
					detectSum);
				impedance = auxcable_impedance;
				break;
			}

			pick_impedance = mtk_calculate_hp_impedance(
			    dcSum / kDetectTimes, detectSum, dcValue, 1);

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
			/* if detect auxadc value over 32630 , the hpImpedance
			 * is over 5k ohm
			 */
			if ((detectSum / kDetectTimes) >
			    hpdet_param.auxadc_upper_bound)
				impedance = auxcable_impedance;
			else
				impedance = mtk_calculate_hp_impedance(
				    dcSum, detectSum, dcValue, kDetectTimes);
			break;
		}

		/* Phase 1 : detect  range 250ohm to 1000ohm impedance */
		if (dcValue == hpdet_param.dc_Phase1 && phase_flag == 1) {
			usleep_range(1 * 1000, 1 * 1000);
			detectSum = 0;
			for (counter = 0; counter < kDetectTimes; counter++) {
				detectsOffset[counter] =
				    audio_get_auxadc_value();
				detectSum = detectSum + detectsOffset[counter];
			}
			impedance = mtk_calculate_hp_impedance(
			    dcSum, detectSum, dcValue, kDetectTimes);
			break;
		}

		/* Phase 2 : detect under 250ohm impedance */
		if (dcValue == hpdet_param.dc_Phase2 && phase_flag == 2) {
			usleep_range(1 * 1000, 1 * 1000);
			detectSum = 0;
			for (counter = 0; counter < kDetectTimes; counter++) {
				detectsOffset[counter] =
				    audio_get_auxadc_value();
				detectSum = detectSum + detectsOffset[counter];
			}
			impedance = mtk_calculate_hp_impedance(
			    dcSum, detectSum, dcValue, kDetectTimes);
			break;
		}
		usleep_range(1 * 200, 1 * 200);
	}

	if (PARALLEL_OHM != 0) {
		if (impedance < PARALLEL_OHM) {
			impedance =
			    DIV_ROUND_CLOSEST(impedance * PARALLEL_OHM,
					      PARALLEL_OHM - impedance);
		} else {
			pr_debug("%s(), PARALLEL_OHM %d <= impedance %d\n",
				__func__, PARALLEL_OHM, impedance);
		}
	}

	pr_debug(
	    "%s(), phase %d [dc,detect]Sum %d times [%d,%d], hp_impedance %d, pick_impedance %d, AUXADC_CON10 0x%x\n",
	    __func__, phase_flag, kDetectTimes, dcSum, detectSum, impedance,
	    pick_impedance, Ana_Get_Reg(AUXADC_CON10));

	/* Ramp-Down */
	while (dcValue > 0) {
		dcValue = dcValue - hpdet_param.dc_Step;
		/* apply dc by dc compensation: 16bit MSB and negative value */
		set_lch_dc_compensation(-dcValue << 16);
		set_rch_dc_compensation(-dcValue << 16);
		usleep_range(1 * 200, 1 * 200);
	}

	set_lch_dc_compensation(0);
	set_rch_dc_compensation(0);
	enable_dc_compensation(false);
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
	EnableTrimbuffer(false);

	return impedance;
}

/* 1.7V * 0.5kohm / (2.5 + 0.5)kohm = 0.283V, support 1k ~ 14k, 0.5k margin */
#define MIC_VINP_4POLE_THRES_MV 283
#define VINP_NORMALIZED_TO_MV 1700

static int dctrim_calibrated;
static int hpl_dc_offset, hpr_dc_offset;
static int mic_vinp_mv;

static int last_lch_comp_value, last_rch_comp_value;

static const int dBFactor_Den = 8192;
/* 1 / (10 ^ (dB / 20)) * dBFactor_Den */
static const int dBFactor_Nom[32] = {
	3261,   3659,   4106,   4607,   5169,   5799,   6507,   7301,
	8192,   9192,   10313,  11572,  12983,  14568,  16345,  18340,
	20577,  23088,  25905,  819200, 819200, 819200, 819200, 819200,
	819200, 819200, 819200, 819200, 819200, 819200, 819200, 819200,
};

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
		pr_debug("%s(), invalid mic_bias %d\n", __func__, mic_bias);
		return 2600;
	};
}

static int calOffsetToDcComp(int offset, int vol_type)
{
	int gain = mCodec_data->mAudio_Ana_Volume[vol_type];
	int mic_bias_mv;
	int real_mic_vinp_mv;

	int offset_scale =
	    DIV_ROUND_CLOSEST(offset * dBFactor_Nom[gain], dBFactor_Den);

	if (mic_vinp_mv > MIC_VINP_4POLE_THRES_MV &&
	    ((codec_debug_enable & DBG_DCTRIM_BYPASS_4POLE) == 0)) {
		int v_diff_bias_vinp;
		int v_diff_bias_vinp_scale;

		/* refine mic bias influence on 4 pole headset */
		mic_bias_mv = get_mic_bias_mv();
		real_mic_vinp_mv = DIV_ROUND_CLOSEST(mic_vinp_mv * mic_bias_mv,
						     VINP_NORMALIZED_TO_MV);

		v_diff_bias_vinp = mic_bias_mv - real_mic_vinp_mv;
		v_diff_bias_vinp_scale = DIV_ROUND_CLOSEST(
		    (v_diff_bias_vinp)*dBFactor_Nom[gain], dBFactor_Den);

		if ((codec_debug_enable & DBG_DCTRIM_4POLE_LOG) != 0) {
			pr_debug("%s(), mic_bias_mv %d, mic_vinp_mv %d, real_mic_vinp_mv %d\n",
				 __func__, mic_bias_mv, mic_vinp_mv,
				 real_mic_vinp_mv);

			pr_debug(
			    "%s(), a %d, b %d\n", __func__,
			    DIV_ROUND_CLOSEST(offset_scale * 2804225, 32768),
			    DIV_ROUND_CLOSEST(v_diff_bias_vinp_scale * 1782,
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
	/* 80uV / 0.0808uV(0dB) = 990.099 */
	int step_0db = 990;

	/* scale for specific gain */
	return step_0db * dBFactor_Nom[gain] / dBFactor_Den;
}

static int SetDcCompenSation(bool enable)
{
	int lch_value = 0, rch_value = 0, tmp_ramp = 0;
	int times = 0, i = 0;
	int sign_lch = 0, sign_rch = 0;
	int abs_lch = 0, abs_rch = 0;
	int index_lgain =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];
	int diff_lch = 0, diff_rch = 0, ramp_l = 0, ramp_r = 0;
	int ramp_step = get_dc_ramp_step(index_lgain);

	if (enable_dc_compensation == NULL ||
	    set_lch_dc_compensation == NULL ||
	    set_rch_dc_compensation == NULL) {
		pr_debug(
		    "%s(), function not ready, enable %p, lch %p, rch %p\n",
		    __func__, enable_dc_compensation, set_lch_dc_compensation,
		    set_rch_dc_compensation);
		return -EFAULT;
	}

	if (enable && index_lgain == DL_GAIN_N_40DB) {
		pr_debug("%s(), -40dB skip dc compensation\n", __func__);
		return 0;
	}

	lch_value =
	    calOffsetToDcComp(hpl_dc_offset, AUDIO_ANALOG_VOLUME_HPOUTL);
	rch_value =
	    calOffsetToDcComp(hpr_dc_offset, AUDIO_ANALOG_VOLUME_HPOUTR);
	diff_lch = enable ? lch_value - last_lch_comp_value : lch_value;
	diff_rch = enable ? rch_value - last_rch_comp_value : rch_value;
	sign_lch = diff_lch < 0 ? -1 : 1;
	sign_rch = diff_rch < 0 ? -1 : 1;
	abs_lch = sign_lch * diff_lch;
	abs_rch = sign_rch * diff_rch;
	times =
	    abs_lch > abs_rch ? (abs_lch / ramp_step) : (abs_rch / ramp_step);
	pr_debug("%s(), enable = %d, index_gain = %d, times = %d, lch_value = %d -> %d, rch_value = %d -> %d, ramp_step %d, mic_vinp_mv %d\n",
		 __func__, enable, index_lgain, times, last_lch_comp_value,
		 lch_value, last_rch_comp_value, rch_value, ramp_step,
		 mic_vinp_mv);

	if (enable) {
		enable_dc_compensation(true);
		for (i = 1; i <= times; i++) {
			tmp_ramp = i * ramp_step;
			if (tmp_ramp < abs_lch) {
				ramp_l =
				    last_lch_comp_value + sign_lch * tmp_ramp;
				set_lch_dc_compensation(ramp_l << 8);
			}
			if (tmp_ramp < abs_rch) {
				ramp_r =
				    last_rch_comp_value + sign_rch * tmp_ramp;
				set_rch_dc_compensation(ramp_r << 8);
			}
			udelay(600);
		}
		set_lch_dc_compensation(lch_value << 8);
		set_rch_dc_compensation(rch_value << 8);
		last_lch_comp_value = lch_value;
		last_rch_comp_value = rch_value;
	} else {
		for (i = times; i >= 0; i--) {
			tmp_ramp = i * ramp_step;
			if (tmp_ramp < abs_lch)
				set_lch_dc_compensation(sign_lch * tmp_ramp
							<< 8);

			if (tmp_ramp < abs_rch)
				set_rch_dc_compensation(sign_rch * tmp_ramp
							<< 8);
			udelay(600);
		}
		set_lch_dc_compensation(0);
		set_rch_dc_compensation(0);
		enable_dc_compensation(false);
		last_lch_comp_value = 0;
		last_rch_comp_value = 0;
	}

	return 0;
}

static int get_hp_trim_offset(int channel)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#define TRIM_TIMES 26
#define TRIM_DISCARD_NUM 3
#define TRIM_USEFUL_NUM (TRIM_TIMES - (TRIM_DISCARD_NUM * 2))

	int on_value[TRIM_TIMES];
	int off_value[TRIM_TIMES];
	int offset = 0;
	int i, j, tmp;

	if (channel != AUDIO_OFFSET_TRIM_MUX_HPL &&
	    channel != AUDIO_OFFSET_TRIM_MUX_HPR) {
		pr_debug("%s(), channel %d not support\n", __func__, channel);
		return 0;
	}

	Ana_Set_Reg(AUXADC_CON10, AUXADC_AVG_256, 0x7);

	/* get buffer on auxadc value  */
	OpenTrimBufferHardware(true, true);

	setOffsetTrimMux(channel);
	setOffsetTrimBufferGain(3); /* 18db */
	EnableTrimbuffer(true);
	usleep_range(1 * 1000, 10 * 1000);

	for (i = 0; i < TRIM_TIMES; i++)
		on_value[i] = audio_get_auxadc_value();

	EnableTrimbuffer(false);
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
	OpenTrimBufferHardware(false, true);

	/* get buffer off auxadc value */
	OpenTrimBufferHardware(true, false);

	setOffsetTrimMux(channel);
	setOffsetTrimBufferGain(3); /* 18db */
	EnableTrimbuffer(true);
	usleep_range(1 * 1000, 10 * 1000);

	for (i = 0; i < TRIM_TIMES; i++)
		off_value[i] = audio_get_auxadc_value();

	EnableTrimbuffer(false);
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);

	OpenTrimBufferHardware(false, false);

	/* sort */
	for (i = 0; i < TRIM_TIMES - 1; i++) {
		for (j = 0; j < TRIM_TIMES - 1 - i; j++) {
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
	for (i = TRIM_DISCARD_NUM; i < TRIM_TIMES - TRIM_DISCARD_NUM; i++) {
		offset += on_value[i] - off_value[i];
		pr_debug("%s(), offset diff %d, on %d, off %d\n", __func__,
			 on_value[i] - off_value[i], on_value[i],
			 off_value[i]);
	}

	offset = DIV_ROUND_CLOSEST(offset, TRIM_USEFUL_NUM);

	pr_debug("%s(), channel = %d, offset = %d\n", __func__, channel,
		 offset);

	return offset;
#else
	return 0;
#endif
}

static void get_hp_lr_trim_offset(void)
{
	pr_debug("%s(), Start DCtrim Calibrating", __func__);
	hpl_dc_offset = get_hp_trim_offset(AUDIO_OFFSET_TRIM_MUX_HPL);
	hpr_dc_offset = get_hp_trim_offset(AUDIO_OFFSET_TRIM_MUX_HPR);
	dctrim_calibrated = 2;
	pr_debug("%s(), End DCtrim Calibrating", __func__);
}

static int mt63xx_codec_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *Daiport)
{
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_debug("%s set up SNDRV_PCM_STREAM_CAPTURE rate = %d\n",
			__func__, substream->runtime->rate);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] =
		    substream->runtime->rate;

	} else if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pr_debug("%s set up SNDRV_PCM_STREAM_PLAYBACK rate = %d\n",
		       __func__, substream->runtime->rate);
		mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] =
		    substream->runtime->rate;
	}
	return 0;
}

static const struct snd_soc_dai_ops mt6323_aif1_dai_ops = {
	.prepare = mt63xx_codec_prepare,
};

static struct snd_soc_dai_driver mtk_6356_dai_codecs[] = {
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
		    .formats =
			(SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S8 |
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
	{.name = MT_SOC_CODEC_I2S0TXDAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		 .stream_name = MT_SOC_I2SDL1_STREAM_NAME,
		 .channels_min = 1,
		 .channels_max = 2,
		 .rate_min = 8000,
		 .rate_max = 192000,
		 .rates = SNDRV_PCM_RATE_8000_192000,
		 .formats = SND_SOC_ADV_MT_FMTS,
	     } },
	{.name = MT_SOC_CODEC_DEEPBUFFER_TX_DAI_NAME,
	 .ops = &mt6323_aif1_dai_ops,
	 .playback = {
		 .stream_name = MT_SOC_DEEP_BUFFER_DL_STREAM_NAME,
		 .channels_min = 1,
		 .channels_max = 2,
		 .rate_min = 8000,
		 .rate_max = 192000,
		 .rates = SNDRV_PCM_RATE_8000_192000,
		 .formats = SND_SOC_ADV_MT_FMTS,
	     } },
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
	pr_debug("%s\n", __func__);
	audckbufEnable(true);

	/* gpio mosi mode */
	set_playback_gpio(true);

	/* Enable HP main CMFB Switch */
	Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0200, 0xffff);
	/* Pull-down HPL/R, HS, LO to AVSS28_AUD */
	Ana_Set_Reg(AUDDEC_ANA_CON10, 0x00a8, 0xffff);

	switch (device) {
	case AUDIO_ANALOG_DEVICE_OUT_EARPIECEL:
	case AUDIO_ANALOG_DEVICE_OUT_EARPIECER:
		/* Release HS CMFB pull down */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0088, 0xffff);
		break;
	case AUDIO_ANALOG_DEVICE_OUT_SPEAKERL:
	case AUDIO_ANALOG_DEVICE_OUT_SPEAKERR:
		/* Release LO CMFB pull down */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0028, 0xffff);
		break;
	case AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_L:
	case AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R:
		/* Dsiable HP main CMFB Switch */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0000, 0xffff);
		/* Release HP CMFB pull down */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x00A0, 0xffff);
		/* Release LO CMFB pull down */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0020, 0xffff);
		break;
	case AUDIO_ANALOG_DEVICE_OUT_HEADSETL:
	case AUDIO_ANALOG_DEVICE_OUT_HEADSETR:
	default:
		/* Dsiable HP main CMFB Switch */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0000, 0xffff);
		/* Release HP CMFB pull down */
		Ana_Set_Reg(AUDDEC_ANA_CON10, 0x00a0, 0xffff);
		break;
	};

	NvregEnable(true);  /* Enable AUDGLB */
	ClsqEnable(true);   /* Turn on 26MHz source clock */
	Topck_Enable(true); /* Turn on AUDNCP_CLKDIV engine clock */
	/* Turn on AUD 26M */
	udelay(250);

	Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0000, 0x00c4); /* power on clock */

	udelay(250);

	/* enable aud_pad RX fifos */
	Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x0031, 0x00ff);

	/* Audio system digital clock power down release */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffff);
	/* sdm audio fifo clock power on */
	Ana_Set_Reg(AFUNC_AUD_CON0, 0xCBA1, 0xffff);
	/* scrambler clock on enable */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffff);
	/* sdm power on */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x000B, 0xffff);
	/* sdm fifo enable */

	/* afe enable */
	Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);

	setDlMtkifSrc(true);
}

static void TurnOffDacPower(void)
{
	pr_debug("%s\n", __func__);
	setDlMtkifSrc(false);

	/* DL scrambler disabling sequence */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0000, 0xffff);
	Ana_Set_Reg(AFUNC_AUD_CON0, 0xcba0, 0xffff);

	if (GetAdcStatus() == false) {
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000, 0x0001); /* turn off afe */
		/* all power down */
		Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x00ff, 0x00ff);
	} else {
		/* down-link power down */
		Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0040, 0x0040);
	}

	/* disable aud_pad RX fifos */
	Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x0000, 0x00ff);

	udelay(250);

	set_playback_gpio(false);

	Topck_Enable(false);
	ClsqEnable(false);
	NvregEnable(false);
	audckbufEnable(false);
}

static void setDlMtkifSrc(bool enable)
{
	pr_debug("%s(), enable = %d, freq = %d\n", __func__, enable,
		 mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]);
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
	pr_debug(
	    "%s(), enable %d, HSL %d, HSR %d\n", __func__, enable,
	    mCodec_data
		->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL],
	    mCodec_data
		->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]);

	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower(AUDIO_ANALOG_DEVICE_OUT_HEADSETL);

		/* here pmic analog control */
		if (mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false &&
		    mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == false) {
			/* switch to ground to de pop-noise */
			/*HP_Switch_to_Ground();*/

			/* Pull-down HPL/R to AVSS28_AUD */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x8000, 0xffff);

			/* Disable headphone short-circuit protection */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);
			/* Disable handset short-circuit protection */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
			/* Disable linout short-circuit protection */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);
			/* Reduce ESD resistance of AU_REFN */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc000, 0xffff);
			/* Set HPR/HPL gain as minimum (~ -40dB) */
			Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_40DB_REG, 0xffff);

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
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00fc, 0xffff);

			/* Enable HP main CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0e00, 0xffff);
			/* Disable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0200, 0xffff);
			/* Enable HP main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00ff, 0xffff);
			/* Enable HPR/L main output stage step by step */
			hp_main_output_ramp(true);

			/* Reduce HP aux feedback loop gain */
			hp_aux_feedback_loop_gain_ramp(true);
			/* Disable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fcf, 0xffff);

			/* apply volume setting */
			headset_volume_ramp(DL_GAIN_N_40DB,
					    mCodec_data->mAudio_Ana_Volume
						[AUDIO_ANALOG_VOLUME_HPOUTL]);

			/* Disable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fc3, 0xffff);
			/* Unshort HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3f03, 0xffff);
			udelay(100);

			/* Enable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);
			/* Enable Audio DAC  */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x30ff, 0xffff);
			/* Enable low-noise mode of DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xf201, 0xffff);
			udelay(100);

			/* Switch HPL MUX to audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x32ff, 0xffff);
			/* Switch HPR MUX to audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3aff, 0xffff);

			/* Apply digital DC compensation value to DAC */
			SetDcCompenSation(true);

			/* No Pull-down HPL/R to AVSS28_AUD */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x4033, 0xffff);
		}

	} else {
		if (mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == false &&
		    mCodec_data->mAudio_Ana_DevicePower
			    [AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == false) {
			/* Pull-down HPL/R to AVSS28_AUD */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc033, 0xffff);

			SetDcCompenSation(false);

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
			headset_volume_ramp(mCodec_data->mAudio_Ana_Volume
						[AUDIO_ANALOG_VOLUME_HPOUTL],
					    DL_GAIN_N_40DB);

			/* Enable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fff, 0xffff);

			/* Reduce HP aux feedback loop gain */
			hp_aux_feedback_loop_gain_ramp(false);

			/* decrease HPR/L main output stage step by step */
			hp_main_output_ramp(false);

			/* Disable HP main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3);

			/* Enable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0e00, 0xffff);

			/* Disable HP main CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0c00, 0xffff);

			/* Unshort HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3 << 6);

			/* Disable HP driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0x3 << 4);

			/* Disable HP driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0x3 << 6);

			/* Disable HP aux CMFB loop, Enable HP main CMFB for HP
			 * off state
			 */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0200, 0xffff);

			/* Disable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3 << 4);

			/* Disable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3 << 2);

			/* Pull-down HPL/R, HS, LO to AVSS28_AUD */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0xa8, 0xff);

			/* Disable IBIST */
			Ana_Set_Reg(AUDDEC_ANA_CON12, 0x1 << 8, 0x1 << 8);

			/* Disable AUD_ZCD */
			Hp_Zcd_Enable(false);

			/* Disable NV regulator (-1.2V) */
			Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0, 0x1);
			/* Disable cap-less LDOs (1.5V) */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0, 0x1055);
			/* Disable NCP */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x1, 0x1);

			/* Disable Pull-down HPL/R to AVSS28_AUD */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x4033, 0xffff);

			if (GetDLStatus() == false)
				TurnOffDacPower();
		}
	}
}

static int Audio_AmpL_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s = %d\n", __func__,
	       mCodec_data
		   ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data
		->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL];
	return 0;
}

static int Audio_AmpL_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	pr_debug(
		"%s(): enable = %ld, mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] = %d\n",
	       __func__, ucontrol->value.integer.value[0],
	       mCodec_data
		   ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]);
	if ((ucontrol->value.integer.value[0] == true) &&
	    (mCodec_data
		 ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] ==
	     false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, true);
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		   (mCodec_data->mAudio_Ana_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_HEADSETL] == true)) {
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_HEADSETL] =
		    ucontrol->value.integer.value[0];
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_LEFT1, false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

static int Audio_AmpR_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s = %d\n", __func__,
	       mCodec_data
		   ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]);
	ucontrol->value.integer.value[0] =
	    mCodec_data
		->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR];
	return 0;
}

static int Audio_AmpR_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);

	pr_debug(
		"%s(): enable = %ld, mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] = %d\n",
	       __func__, ucontrol->value.integer.value[0],
	       mCodec_data
		   ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]);
	if ((ucontrol->value.integer.value[0] == true) &&
	    (mCodec_data
		 ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] ==
	     false)) {
		Audio_Amp_Change(AUDIO_ANALOG_CHANNELS_RIGHT1, true);
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		   (mCodec_data->mAudio_Ana_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_HEADSETR] == true)) {
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_HEADSETR] =
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

	/* dcxo */
	Ana_Set_Reg(DCXO_CW14, 0x1 << 13, 0x1 << 13);

	/* gpio mosi mode */
	set_playback_gpio(true);

	/* Enable HP main CMFB Switch */
	Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0200, 0xffff);
	/* Pull-down HPL/R, HS, LO to AVSS28_AUD */
	Ana_Set_Reg(AUDDEC_ANA_CON10, 0x00a8, 0xffff);
	/* Release HS CMFB pull down */
	Ana_Set_Reg(AUDDEC_ANA_CON10, 0x0088, 0xffff);
	/* Enable AUDGLB */
	Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0000, 0xffff);
	/* Pull-down HPL/R to AVSS28_AUD */
	Ana_Set_Reg(AUDDEC_ANA_CON2, 0x8000, 0xffff);
	/* Turn on 26MHz source clock */
	Ana_Set_Reg(TOP_CLKSQ, 0x0001, 0x0001);
	Ana_Set_Reg(TOP_CLKSQ_SET, 0x0001, 0x0001);
	/* Turn on AUDNCP_CLKDIV engine clock */
	Ana_Set_Reg(AUD_TOP_CKPDN_CON0, 0x0, 0x66);

	udelay(250);

	/* power on clock */
	Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x8000, 0xBfff);

	udelay(250);

	/* enable aud_pad RX fifos */
	Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x0031, 0x00ff);

	/* Audio system digital clock power down release */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0006, 0xffff);
	/* sdm audio fifo clock power on */
	Ana_Set_Reg(AFUNC_AUD_CON0, 0xCBA1, 0xffff);
	/* scrambler clock on enable */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x0003, 0xffff);
	/* sdm power on */
	Ana_Set_Reg(AFUNC_AUD_CON2, 0x000B, 0xffff);
	/* sdm fifo enable */

	/* afe enable */
	Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0x0001);

	setDlMtkifSrc(true);

	/* Disable headphone short-circuit protection */
	Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);
	/* Disable handset short-circuit protection */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
	/* Disable linout short-circuit protection */
	Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);
	/* Reduce ESD resistance of AU_REFN */
	Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc000, 0xffff);
	/* Set HS gain as minimum (~ -40dB) */
	Ana_Set_Reg(ZCD_CON3, DL_GAIN_N_40DB, 0xffff);

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
	/* Set HP DR bias current optimization, 010: 6uA */
	Ana_Set_Reg(AUDDEC_ANA_CON11, 0x4900, 0xffff);
	/* Set HP & ZCD bias current optimization */
	/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
	Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

	/* Set HS STB enhance circuits */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0090, 0xffff);
	/* Enable HS driver bias circuits */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0092, 0xffff);
	/* Enable HS driver core circuits */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0093, 0xffff);
	/* Set HS gain to 0dB */
	Ana_Set_Reg(ZCD_CON3, DL_GAIN_0DB, 0xffff);
	/* Enable AUD_CLK */
	Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);
	/* Enable Audio DAC  */
	Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3009, 0xffff);
	/* Enable low-noise mode of DAC */
	Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0201, 0xffff);
	/* Switch HS MUX to audio DAC */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x009b, 0xffff);

	/* phone mic dcc */

	/* set gpio miso mode */
	set_capture_gpio(true);

	/* gpio miso driving set to default 6mA, 0xcccc */
	Ana_Set_Reg(DRV_CON3, 0xcccc, 0xffff);

	/* Enable audio ADC CLKGEN  */
	Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1 << 5, 0x1 << 5);
	/* ADC CLK from CLKGEN (13MHz) */
	Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0xffff);
	/* Enable  LCLDO_ENC 1P8V */
	Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0100, 0x2500);
	/* LCLDO_ENC remote sense */
	Ana_Set_Reg(AUDDEC_ANA_CON14, 0x2500, 0x2500);

	/* DCC 50k CLK (from 26M) */
	Ana_Set_Reg(TOP_CLKSQ_SET, 0x3, 0x3);

	Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
	Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
	Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2060, 0xffff);
	Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2061, 0xffff);
	Ana_Set_Reg(AFE_DCCLK_CFG1, 0x1105, 0xffff);

	/* phone mic bias */
	/* Enable MICBIAS0, MISBIAS0 = 1P9V */
	Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0xffff);

	Ana_Set_Reg(AUDENC_ANA_CON0, 0x4 << 8, 0x0700);
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x4 << 8, 0x0700);

	/* Audio L preamplifier DCC precharge */
	Ana_Set_Reg(AUDENC_ANA_CON0, 0x0004, 0xffff);
	/* "ADC1", main_mic */
	/* Audio L preamplifier input sel : AIN0. Enable audio L PGA */
	Ana_Set_Reg(AUDENC_ANA_CON0, 0x0041, 0xf0ff);
	/* Audio L preamplifier DCCEN */
	Ana_Set_Reg(AUDENC_ANA_CON0, 0x1 << 1, 0x1 << 1);
	/* Audio L ADC input sel : L PGA. Enable audio L ADC */
	Ana_Set_Reg(AUDENC_ANA_CON0, 0x5041, 0xf000);

	/* Audio R preamplifier DCC precharge */
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x0004, 0xffff);
	/* ref mic */
	/* Audio R preamplifier input sel : AIN2. Enable audio R PGA */
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x00c1, 0xf0ff);
	/* Audio R preamplifier DCCEN */
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x1 << 1, 0x1 << 1);
	/* Audio R ADC input sel : R PGA. Enable audio R ADC */
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x50c1, 0xf000);

	/* Short body to VCM in PGA */
	Ana_Set_Reg(AUDENC_ANA_CON7, 0x0400, 0xffff);
	usleep_range(100, 150);

	/* Audio R preamplifier DCC precharge off */
	Ana_Set_Reg(AUDENC_ANA_CON1, 0x0, 0x1 << 2);
	/* Audio L preamplifier DCC precharge off */
	Ana_Set_Reg(AUDENC_ANA_CON0, 0x0, 0x1 << 2);

	/* here to set digital part */
	/* power on clock */
	Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x8000, 0xffff);

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
#if 0 /* not used */
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
	if (enable) {
		if (GetDLStatus() == false) {
			TurnOnDacPower(AUDIO_ANALOG_DEVICE_OUT_EARPIECEL);
			pr_debug("%s(), amp on\n", __func__);

			/* Disable headphone short-circuit protection */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);
			/* Disable handset short-circuit protection */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
			/* Disable linout short-circuit protection */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);
			/* Reduce ESD resistance of AU_REFN */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc000, 0xffff);
			/* Set HS gain as minimum (~ -40dB) */
			Ana_Set_Reg(ZCD_CON3, DL_GAIN_N_40DB, 0xffff);

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
			/* Set HP DR bias current optimization, 010: 6uA */
			Ana_Set_Reg(AUDDEC_ANA_CON11, 0x4900, 0xffff);
			/* Set HP & ZCD bias current optimization */
			/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
			Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

			/* Set HS STB enhance circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0090, 0xffff);
			/* Enable HS driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0092, 0xffff);
			/* Enable HS driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0093, 0xffff);
			/* Set HS gain to normal gain step by step */
			Ana_Set_Reg(ZCD_CON3, mCodec_data->mAudio_Ana_Volume
						  [AUDIO_ANALOG_VOLUME_HSOUTL],
				    0xffff);
			/* Enable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);
			/* Enable Audio DAC  */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3009, 0xffff);
			/* Enable low-noise mode of DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0201, 0xffff);
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

			/* Pull-down HPL/R, HS, LO to AVSS28_AUD */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0xa8, 0xff);

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
	pr_debug(
	    "%s = %d\n", __func__,
	    mCodec_data
		->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data
		->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL];
	return 0;
}

static int Voice_Amp_Set(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&Ana_Ctrl_Mutex);
	pr_debug("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true) &&
	    (mCodec_data
		 ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] ==
	     false)) {
		Voice_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		   (mCodec_data->mAudio_Ana_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] == true)) {
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_EARPIECEL] =
		    ucontrol->value.integer.value[0];
		Voice_Amp_Change(false);
	}
	mutex_unlock(&Ana_Ctrl_Mutex);
	return 0;
}

/* For MT6356 LO */
static void Apply_Speaker_Gain(void)
{
	Ana_Set_Reg(
	    ZCD_CON1,
	    (mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTR]
	     << 7) |
		mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTL],
	    DL_GAIN_REG_MASK);
}

static void Speaker_Amp_Change(bool enable)
{
	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower(AUDIO_ANALOG_DEVICE_OUT_SPEAKERL);

		pr_debug("%s(), enable %d\n", __func__, enable);

		/* Disable headphone short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);
		/* Disable handset short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
		/* Disable linout short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);
		/* Reduce ESD resistance of AU_REFN */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc000, 0xffff);
		/* Set HS gain as minimum (~ -40dB) */
		Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_40DB_REG, 0xffff);

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
		/* Set HP DR bias current optimization, 010: 6uA */
		Ana_Set_Reg(AUDDEC_ANA_CON11, 0x4900, 0xffff);
		/* Set HP & ZCD bias current optimization */
		/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);

		/* Set LO STB enhance circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0110, 0xffff);
		/* Enable LO driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0112, 0xffff);
		/* Enable LO driver core circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0113, 0xffff);

		/* Set LOL gain to normal gain step by step */
		Apply_Speaker_Gain();

		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);
		/* Enable Audio DAC  */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3009, 0xffff);
		/* Enable low-noise mode of DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0201, 0xffff);
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

			/* Enable HP main CMFB Switch */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x2 << 8, 0xff << 8);

			/* Pull-down HPL/R, HS, LO to AVSS28_AUD */
			Ana_Set_Reg(AUDDEC_ANA_CON10, 0xa8, 0xff);

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
	    mCodec_data
		->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL];
	return 0;
}

static int Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() value = %ld\n ", __func__,
		 ucontrol->value.integer.value[0]);
	if ((ucontrol->value.integer.value[0] == true) &&
	    (mCodec_data
		 ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] ==
	     false)) {
		Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		   (mCodec_data->mAudio_Ana_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] == true)) {
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_SPEAKERL] =
		    ucontrol->value.integer.value[0];
		Speaker_Amp_Change(false);
	}
	return 0;
}

static void Ext_Speaker_Amp_Change(bool enable)
{
#define SPK_WARM_UP_TIME (25) /* unit is ms */
	if (enable) {
		pr_debug("%s ON+\n", __func__);

		AudDrv_GPIO_EXTAMP_Select(false, 3);

		/*udelay(1000); */
		usleep_range(1 * 1000, 20 * 1000);

		AudDrv_GPIO_EXTAMP_Select(true, 3);

		msleep(SPK_WARM_UP_TIME);

		pr_debug("%s ON-\n", __func__);
	} else {
		pr_debug("%s OFF+\n", __func__);

		AudDrv_GPIO_EXTAMP_Select(false, 3);

		udelay(500);

		pr_debug("%s OFF-\n", __func__);
	}
}

static int Ext_Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	    mCodec_data
		->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP];
	return 0;
}

static int Ext_Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() gain = %ld\n ", __func__,
	       ucontrol->value.integer.value[0]);
	if (ucontrol->value.integer.value[0]) {
		Ext_Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_EXTSPKAMP] =
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
		 mCodec_data->mAudio_Ana_DevicePower
		     [AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower
		[AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH];
	return 0;
}

static int Receiver_Speaker_Switch_Set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if ((ucontrol->value.integer.value[0] == true) &&
	    (mCodec_data->mAudio_Ana_DevicePower
		 [AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] == false)) {
		Receiver_Speaker_Switch_Change(true);
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		   (mCodec_data->mAudio_Ana_DevicePower
			[AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] ==
		    true)) {
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH] =
		    ucontrol->value.integer.value[0];
		Receiver_Speaker_Switch_Change(false);
	}
	return 0;
}

static void Headset_Speaker_Amp_Change(bool enable)
{
	pr_debug("%s(), enable %d\n", __func__, enable);
	if (enable) {
		if (GetDLStatus() == false)
			TurnOnDacPower(
			    AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_L);

		/* Pull-down HPL/R to AVSS28_AUD */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x8000, 0xffff);

		/* Disable headphone short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3000, 0xffff);
		/* Disable handset short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON6, 0x0010, 0xffff);
		/* Disable linout short-circuit protection */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0010, 0xffff);
		/* Reduce ESD resistance of AU_REFN */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc000, 0xffff);

		/* Set LO gain as minimum (~ -40dB) */
		Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_40DB_REG, 0xffff);
		/* Set HPR/HPL gain as minimum (~ -40dB) */
		Ana_Set_Reg(ZCD_CON2, DL_GAIN_N_40DB_REG, 0xffff);

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
		/* Set HP DR bias current optimization, 010: 6uA */
		Ana_Set_Reg(AUDDEC_ANA_CON11, 0x4900, 0xffff);
		/* Set HP & ZCD bias current optimization */
		/* 01: ZCD: 4uA, HP/HS/LO: 5uA */
		Ana_Set_Reg(AUDDEC_ANA_CON12, 0x0055, 0xffff);
		/* Set HPP/N STB enhance circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc033, 0xffff);

		/* Set LO STB enhance circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0110, 0xffff);
		/* Enable LO driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0112, 0xffff);
		/* Enable LO driver core circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0113, 0xffff);

		/* Set LOL gain to normal gain step by step */
		Apply_Speaker_Gain();

		/* Switch HPL MUX to Line-out */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3100, 0xffff);
		/* Switch HPR MUX to Line-out */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3500, 0xffff);
		/* Enable HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x000c, 0xffff);
		/* Enable HP aux feedback loop */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x003c, 0xffff);

		/* Enable HP aux CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0c00, 0xffff);

		/* Enable HP driver bias circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x35c0, 0xffff);
		/* Enable HP driver core circuits */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x35f0, 0xffff);

		/* Short HP main output to HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00fc, 0xffff);

		/* Enable HP main CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0e00, 0xffff);
		/* Disable HP aux CMFB loop */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0200, 0xffff);

		/* Enable HP main output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x00ff, 0xffff);

		/* Enable HPR/L main output stage step by step */
		hp_main_output_ramp(true);

		/* Reduce HP aux feedback loop gain */
		hp_aux_feedback_loop_gain_ramp(true);
		/* Disable HP aux feedback loop */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fcf, 0xffff);

		/* apply volume setting */
		headset_volume_ramp(
		    DL_GAIN_N_40DB,
		    mCodec_data
			->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL]);

		/* Disable HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fc3, 0xffff);

		/* Unshort HP main output to HP aux output stage */
		Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3f03, 0xffff);

		/* Enable AUD_CLK */
		Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1, 0x1);
		/* Enable Audio DAC  */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x35F9, 0xffff);
		/* Enable low-noise mode of DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON9, 0xf201, 0xffff);
		/* Switch LOL MUX to audio DAC */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x011b, 0xffff);
		udelay(100);

		/* Switch HPL/R MUX to Line-out */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x35f9, 0xffff);

		/* No Pull-down HPL/R to AVSS28_AUD */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0x4033, 0xffff);

	} else {
		/* Pull-down HPL/R to AVSS28_AUD */
		Ana_Set_Reg(AUDDEC_ANA_CON2, 0xc033, 0xffff);

		/* HPR/HPL mux to open */
		Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x0f00);
		/* LOL mux to open */
		Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0000, 0x3 << 2);

		if (GetDLStatus() == false) {
			/* Disable low-noise mode of DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x0, 0x1);

			/* Disable Audio DAC */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0000, 0x000f);

			/* Disable AUD_CLK */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x0, 0x1);

			/* Short HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fc3, 0xffff);
			/* Enable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fcf, 0xffff);

			/* decrease HPL/R gain to normal gain step by step */
			headset_volume_ramp(mCodec_data->mAudio_Ana_Volume
						[AUDIO_ANALOG_VOLUME_HPOUTL],
					    DL_GAIN_N_40DB);

			/* Enable HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x3fff, 0xffff);

			/* Reduce HP aux feedback loop gain */
			hp_aux_feedback_loop_gain_ramp(false);

			/* decrease HPR/L main output stage step by step */
			hp_main_output_ramp(false);

			/* Disable HP main output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3);

			/* Disable HP aux CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xe << 8, 0xff << 8);

			/* Disable HP main CMFB loop */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0xc << 8, 0xff << 8);

			/* Unshort HP main output to HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3 << 6);

			/* Disable HP driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0x3 << 4);
			/* Disable HP driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON0, 0x0, 0x3 << 6);

			/* Disable HP aux CMFB loop, Enable HP main CMFB for HP
			 * off state
			 */
			Ana_Set_Reg(AUDDEC_ANA_CON9, 0x2 << 8, 0xff << 8);

			/* Open HP aux feedback loop */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3 << 4);

			/* Disable HP aux output stage */
			Ana_Set_Reg(AUDDEC_ANA_CON1, 0x0, 0x3 << 2);

			/* decrease LOL gain to minimum gain step by step */
			Ana_Set_Reg(ZCD_CON1, DL_GAIN_N_40DB_REG, 0xffff);

			/* Disable LO driver core circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0, 0x1);
			/* Disable LO driver bias circuits */
			Ana_Set_Reg(AUDDEC_ANA_CON7, 0x0000, 0x1 << 1);

			/* Disable IBIST */
			Ana_Set_Reg(AUDDEC_ANA_CON12, 0x1 << 8, 0x1 << 8);
			/* Disable NV regulator (-1.2V) */
			Ana_Set_Reg(AUDDEC_ANA_CON15, 0x0, 0x1);
			/* Disable cap-less LDOs (1.5V) */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0, 0x1055);
			/* Disable NCP */
			Ana_Set_Reg(AUDNCP_CLKDIV_CON3, 0x1, 0x1);

			/* Increase ESD resistance of AU_REFN */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x8033, 0xffff);

			/* Disable Pull-down HPL/R to AVSS28_AUD */
			Ana_Set_Reg(AUDDEC_ANA_CON2, 0x0033, 0xffff);

			TurnOffDacPower();
		}
	}
}

static int Headset_Speaker_Amp_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_DevicePower
		[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R];
	return 0;
}

static int Headset_Speaker_Amp_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	/* struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol); */

	pr_debug("%s() gain = %lu\n ", __func__,
		 ucontrol->value.integer.value[0]);
	if ((ucontrol->value.integer.value[0] == true) &&
	    (mCodec_data->mAudio_Ana_DevicePower
		 [AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] == false)) {
		Headset_Speaker_Amp_Change(true);
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] =
		    ucontrol->value.integer.value[0];
	} else if ((ucontrol->value.integer.value[0] == false) &&
		   (mCodec_data->mAudio_Ana_DevicePower
			[AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] == true)) {
		mCodec_data->mAudio_Ana_DevicePower
		    [AUDIO_ANALOG_DEVICE_OUT_SPEAKER_HEADSET_R] =
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
		   Audio_AuxAdcData_Get, Audio_AuxAdcData_Set),
};

static const char *const amp_function[] = {"Off", "On"};
static const char *const aud_clk_buf_function[] = {"Off", "On"};

/* static const char *DAC_SampleRate_function[] = {"8000", "11025", "16000",
 * "24000", "32000", "44100", "48000"};
 */
static const char *const DAC_DL_PGA_Headset_GAIN[] = {
	"8Db",  "7Db",  "6Db",  "5Db",  "4Db",   "3Db",  "2Db",
	"1Db",  "0Db",  "-1Db", "-2Db", "-3Db",  "-4Db", "-5Db",
	"-6Db", "-7Db", "-8Db", "-9Db", "-10Db", "-40Db"};

static const char *const DAC_DL_PGA_Handset_GAIN[] = {
	"8Db",  "7Db",  "6Db",  "5Db",  "4Db",   "3Db",  "2Db",
	"1Db",  "0Db",  "-1Db", "-2Db", "-3Db",  "-4Db", "-5Db",
	"-6Db", "-7Db", "-8Db", "-9Db", "-10Db", "-40Db"};

static const char *const DAC_DL_PGA_Speaker_GAIN[] = {
	"8Db",  "7Db",  "6Db",  "5Db",  "4Db",   "3Db",  "2Db",
	"1Db",  "0Db",  "-1Db", "-2Db", "-3Db",  "-4Db", "-5Db",
	"-6Db", "-7Db", "-8Db", "-9Db", "-10Db", "-40Db"};

/* static const char *Voice_Mux_function[] = {"Voice", "Speaker"}; */

static int Lineout_PGAL_Get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Speaker_PGA_Get = %d\n",
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTL];

	if (ucontrol->value.integer.value[0] == DL_GAIN_N_40DB)
		ucontrol->value.integer.value[0] =
		    ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1;

	return 0;
}

static int Lineout_PGAL_Set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	pr_debug("%s(), index = %d\n", __func__, index);

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
		index = DL_GAIN_N_40DB;

	Ana_Set_Reg(ZCD_CON1, index, 0x001f);

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTL] = index;
	return 0;
}

static int Lineout_PGAR_Get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s  = %d\n", __func__,
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTR]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTR];

	if (ucontrol->value.integer.value[0] == DL_GAIN_N_40DB)
		ucontrol->value.integer.value[0] =
		    ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1;

	return 0;
}

static int Lineout_PGAR_Set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	pr_debug("%s(), index = %d\n", __func__, index);

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Speaker_GAIN) - 1))
		index = DL_GAIN_N_40DB;

	Ana_Set_Reg(ZCD_CON1, index << 7, 0x0f80);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_LINEOUTR] = index;
	return 0;
}

static int Handset_PGA_Get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s = %d\n", __func__,
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL];

	if (ucontrol->value.integer.value[0] == DL_GAIN_N_40DB)
		ucontrol->value.integer.value[0] =
		    ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN) - 1;

	return 0;
}

static int Handset_PGA_Set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];

	pr_debug("%s(), index = %d\n", __func__, index);

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Handset_GAIN) - 1))
		index = DL_GAIN_N_40DB;

	Ana_Set_Reg(ZCD_CON3, index, 0x001f);

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL] = index;
	return 0;
}

static int Headset_PGAL_Get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s = %d\n", __func__,
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];

	if (ucontrol->value.integer.value[0] == DL_GAIN_N_40DB)
		ucontrol->value.integer.value[0] =
		    ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1;

	return 0;
}

static int Headset_PGAL_Set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];
	int old_idx =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL];

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1))
		index = DL_GAIN_N_40DB;

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] = index;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = index;

	if (mCodec_data
		->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETL]) {
		headset_volume_ramp(old_idx, index);
		SetDcCompenSation(true);
	}

	return 0;
}

static int Headset_PGAR_Get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s = %d\n", __func__,
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];

	if (ucontrol->value.integer.value[0] == DL_GAIN_N_40DB)
		ucontrol->value.integer.value[0] =
		    ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1;

	return 0;
}

static int Headset_PGAR_Set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	int index = ucontrol->value.integer.value[0];
	int old_idx =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR];

	pr_debug("%s(), index = %d\n", __func__, index);

	if (index >= ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}

	if (index == (ARRAY_SIZE(DAC_DL_PGA_Headset_GAIN) - 1))
		index = DL_GAIN_N_40DB;

	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTL] = index;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = index;

	if (mCodec_data
		->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_OUT_HEADSETR]) {
		headset_volume_ramp(old_idx, index);
		SetDcCompenSation(true);
	}

	return 0;
}

static int codec_adc_sample_rate_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = %d\n",
		 __func__, mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]);
	ucontrol->value.integer.value[0] =
	    mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC];
	return 0;
}

static int codec_adc_sample_rate_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] =
	    ucontrol->value.integer.value[0];
	pr_debug("%s mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC] = %d\n",
		 __func__, mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]);
	return 0;
}

static int codec_dac_sample_rate_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = %d\n",
		 __func__, mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]);
	ucontrol->value.integer.value[0] =
	    mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC];
	return 0;
}

static int codec_dac_sample_rate_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] =
	    ucontrol->value.integer.value[0];
	pr_debug("%s mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC] = %d\n",
		 __func__, mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC]);
	return 0;
}

static int Aud_Clk_Buf_Get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("\%s n", __func__);
	ucontrol->value.integer.value[0] = audck_buf_Count;
	return 0;
}

static int Aud_Clk_Buf_Set(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), value = %d\n", __func__,
	       ucontrol->value.enumerated.item[0]);

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
	ucontrol->value.integer.value[0] = hpl_dc_offset;
	ucontrol->value.integer.value[1] = hpr_dc_offset;
	return 0;
}

static int pmic_dc_offset_set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), %ld, %ld\n", __func__,
		 ucontrol->value.integer.value[0],
		 ucontrol->value.integer.value[1]);
	hpl_dc_offset = ucontrol->value.integer.value[0];
	hpr_dc_offset = ucontrol->value.integer.value[1];
	return 0;
}

static const char *const dctrim_control_state[] = {"Not_Yet", "Calibrating",
						   "Calibrated"};

static int pmic_dctrim_control_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), dctrim_calibrated = %d\n", __func__,
		 dctrim_calibrated);
	ucontrol->value.integer.value[0] = dctrim_calibrated;
	return 0;
}

static int pmic_dctrim_control_set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(dctrim_control_state)) {
		pr_debug("%s(), return -EINVAL\n", __func__);
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 1)
		get_hp_lr_trim_offset();
	else
		dctrim_calibrated = ucontrol->value.integer.value[0];

	return 0;
}

static int hp_impedance_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	if (!set_hp_impedance_ctl) {
		pr_debug("%s(), set_hp_impedance_ctl == NULL\n", __func__);
		return 0;
	}

	set_hp_impedance_ctl(true);
	if (OpenHeadPhoneImpedanceSetting(true)) {
		hp_impedance = detect_impedance();
		OpenHeadPhoneImpedanceSetting(false);
	} else
		pr_debug("%s(), pmic dl busy, do nothing\n", __func__);
	set_hp_impedance_ctl(false);

	ucontrol->value.integer.value[0] = hp_impedance;

	pr_debug("%s(), hp_impedance = %d, efuse = %d\n", __func__,
		 hp_impedance, efuse_current_calibrate);
	return 0;
}

static int hp_impedance_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), hp_impedance = %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
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
		pr_debug("%s(), return -EINVAL\n", __func__);
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
			dctrim_control_state)};

static const struct snd_kcontrol_new mt6356_snd_controls[] = {
	SOC_ENUM_EXT("Audio_Amp_R_Switch", Audio_DL_Enum[0], Audio_AmpR_Get,
		 Audio_AmpR_Set),
	SOC_ENUM_EXT("Audio_Amp_L_Switch", Audio_DL_Enum[1], Audio_AmpL_Get,
		 Audio_AmpL_Set),
	SOC_ENUM_EXT("Voice_Amp_Switch", Audio_DL_Enum[2], Voice_Amp_Get,
		 Voice_Amp_Set),
	SOC_ENUM_EXT("Speaker_Amp_Switch", Audio_DL_Enum[3], Speaker_Amp_Get,
		 Speaker_Amp_Set),
	SOC_ENUM_EXT("Headset_Speaker_Amp_Switch", Audio_DL_Enum[4],
		 Headset_Speaker_Amp_Get, Headset_Speaker_Amp_Set),
	SOC_ENUM_EXT("Headset_PGAL_GAIN", Audio_DL_Enum[5], Headset_PGAL_Get,
		 Headset_PGAL_Set),
	SOC_ENUM_EXT("Headset_PGAR_GAIN", Audio_DL_Enum[6], Headset_PGAR_Get,
		 Headset_PGAR_Set),
	SOC_ENUM_EXT("Handset_PGA_GAIN", Audio_DL_Enum[7], Handset_PGA_Get,
		 Handset_PGA_Set),
	SOC_ENUM_EXT("Lineout_PGAR_GAIN", Audio_DL_Enum[8], Lineout_PGAR_Get,
		 Lineout_PGAR_Set),
	SOC_ENUM_EXT("Lineout_PGAL_GAIN", Audio_DL_Enum[9], Lineout_PGAL_Get,
		 Lineout_PGAL_Set),
	SOC_ENUM_EXT("AUD_CLK_BUF_Switch", Audio_DL_Enum[10], Aud_Clk_Buf_Get,
		 Aud_Clk_Buf_Set),
	SOC_ENUM_EXT("Ext_Speaker_Amp_Switch", Audio_DL_Enum[11],
		 Ext_Speaker_Amp_Get, Ext_Speaker_Amp_Set),
	SOC_ENUM_EXT("Receiver_Speaker_Switch", Audio_DL_Enum[11],
		 Receiver_Speaker_Switch_Get, Receiver_Speaker_Switch_Set),
	SOC_ENUM_EXT("PMIC_REG_CLEAR", Audio_DL_Enum[12], PMIC_REG_CLEAR_Get,
		 PMIC_REG_CLEAR_Set),
	SOC_SINGLE_EXT("Codec_ADC_SampleRate", SND_SOC_NOPM, 0,
		MAX_UL_SAMPLE_RATE, 0, codec_adc_sample_rate_get,
		codec_adc_sample_rate_set),
	SOC_SINGLE_EXT("Codec_DAC_SampleRate", SND_SOC_NOPM, 0,
		MAX_DL_SAMPLE_RATE, 0, codec_dac_sample_rate_get,
		codec_dac_sample_rate_set),
	SOC_DOUBLE_EXT("DcTrim_DC_Offset", SND_SOC_NOPM, 0, 1, 0x20000, 0,
		   pmic_dc_offset_get, pmic_dc_offset_set),
	SOC_ENUM_EXT("Dctrim_Control_Switch", Audio_DL_Enum[13],
		 pmic_dctrim_control_get, pmic_dctrim_control_set),
	SOC_SINGLE_EXT("Audio HP ImpeDance Setting", SND_SOC_NOPM, 0,
		0x10000, 0, hp_impedance_get, hp_impedance_set),
	SOC_ENUM_EXT("Headphone Plugged In", Audio_DL_Enum[0],
		hp_plugged_in_get, hp_plugged_in_set),
};

void SetMicPGAGain(void)
{
	int index = 0;

	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
	pr_debug("%s AUDIO_ANALOG_VOLUME_MICAMP1 index =%d\n", __func__, index);
	Ana_Set_Reg(AUDENC_ANA_CON0, index << 8, 0x0700);
	index = mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
	Ana_Set_Reg(AUDENC_ANA_CON1, index << 8, 0x0700);
}

static bool GetAdcStatus(void)
{
	int i = 0;

	for (i = AUDIO_ANALOG_DEVICE_IN_ADC1; i < AUDIO_ANALOG_DEVICE_MAX;
	     i++) {
		if ((mCodec_data->mAudio_Ana_DevicePower[i] == true) &&
		    (i != AUDIO_ANALOG_DEVICE_RECEIVER_SPEAKER_SWITCH))
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

			/* set gpio miso mode */
			set_capture_gpio(true);

			/* Enable audio ADC CLKGEN  */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1 << 5, 0x1 << 5);
			/* ADC CLK from CLKGEN (13MHz) */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0xffff);
			/* Enable  LCLDO_ENC 1P8V */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0100, 0x2500);
			/* LCLDO_ENC remote sense */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x2500, 0x2500);

			/* mic bias */
			if (mCodec_data
				->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] ==
			    0) {
				/* phone mic */
				/* Enable MICBIAS0, MISBIAS0 = 1P9V */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0xffff);
			} else if (mCodec_data->mAudio_Ana_Mux
				       [AUDIO_MICSOURCE_MUX_IN_1] == 1) {
				/* headset mic */
				/* Enable MICBIAS1, MISBIAS1 = 2P6V */
				Ana_Set_Reg(AUDENC_ANA_CON10, 0x0061, 0xffff);
			}

			SetMicPGAGain();
		}

		if (ADCType ==
		    AUDIO_ANALOG_DEVICE_IN_ADC1) { /* main and headset mic */
			if (mCodec_data
				->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] ==
			    0) {
				/* "ADC1", main_mic */
				/* Audio L preamplifier input sel : AIN0.
				 * Enable
				 * audio L PGA
				 */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0041, 0xf0ff);
				/* Audio L ADC input sel : L PGA. Enable audio
				 * L
				 * ADC
				 */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5041, 0xf000);
			} else if (mCodec_data->mAudio_Ana_Mux
				       [AUDIO_MICSOURCE_MUX_IN_1] == 1) {
				/* "ADC2", headset mic */
				/* Audio L preamplifier input sel : AIN1.
				 * Enable
				 * audio L PGA
				 */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0081, 0xf0ff);
				/* Audio L ADC input sel : L PGA. Enable audio
				 * L
				 * ADC
				 */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5081, 0xf000);
			}
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {
			/* ref mic */
			/* Audio R preamplifier input sel : AIN2. Enable audio
			 * R
			 * PGA
			 */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x00c1, 0xf0ff);
			/* Audio R ADC input sel : R PGA. Enable audio R ADC */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x50c1, 0xf000);
		}

		if (GetAdcStatus() == false) {
			/* here to set digital part */
			/* AdcClockEnable(true); */
			Topck_Enable(true);

			/* power on clock */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x8000, 0xdfbf);

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
				/* afe power down and total audio clk disable
				 */
				Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x00ff,
					    0x00ff);
			}

			/* up-link power down */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0020, 0x0020);
		}

		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0xf000);
			/* Audio L ADC input sel : off, disable audio L ADC */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0x0fff);
			/* Audio L preamplifier input sel : off, Audio L PGA 0
			 * dB gain
			 */
			/* Disable audio L PGA */
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0xf000);
			/* Audio R ADC input sel : off, disable audio R ADC */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x0fff);
			/* Audio R preamplifier input sel : off, Audio R PGA 0
			 * dB gain
			 */
			/* Disable audio R PGA */
		}

		if (GetAdcStatus() == false) {
			/* mic bias */
			if (mCodec_data
				->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] ==
			    0) {
				/* phone mic */
				/* Disable MICBIAS0, MISBIAS0 = 1P7V */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0xffff);
			} else if (mCodec_data->mAudio_Ana_Mux
				       [AUDIO_MICSOURCE_MUX_IN_1] == 1) {
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

			set_capture_gpio(false);

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
	pr_debug("%s(), ADCType = %d enable = %d\n", __func__, ADCType,
		 enable);
	if (enable) {
		if (GetAdcStatus() == false) {
			if (set_ap_dmic != NULL)
				set_ap_dmic(true);
			else
				pr_debug("%s(), set_ap_dmic == NULL\n",
					__func__);

			audckbufEnable(true);

			/* Enable audio globe bias */
			NvregEnable(true);

			/* Enable CLKSQ 26MHz */
			ClsqEnable(true);

			/* set gpio miso mode */
			set_capture_gpio(true);

			/* mic bias */
			/* Enable MICBIAS0, MISBIAS0 = 1P9V */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0xffff);

			/* RG_BANDGAPGEN=1'b0 */
			Ana_Set_Reg(AUDENC_ANA_CON10, 0x0, 0x1 << 12);

			/* DMIC enable */
			Ana_Set_Reg(AUDENC_ANA_CON8, 0x0005, 0xffff);

			/* here to set digital part */
			/* AdcClockEnable(true); */
			Topck_Enable(true);

			/* power on clock */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x8000, 0xdfbf);

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
				/* afe power down and total audio clk disable
				 */
				Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x00ff,
					    0x00ff);
			}

			/* up-link power down */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0020, 0x0020);

			/* DMIC disable */
			Ana_Set_Reg(AUDENC_ANA_CON8, 0x0000, 0xffff);

			/* mic bias */
			/* MISBIAS0 = 1P7V */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0001, 0xffff);

			/* RG_BANDGAPGEN=1'b0 */
			Ana_Set_Reg(AUDENC_ANA_CON10, 0x0, 0x1 << 12);

			/* MICBIA0 disable */
			Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0xffff);

			set_capture_gpio(false);

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
	pr_debug("%s(), enable %d, ADCType %d, AUDIO_MICSOURCE_MUX_IN_1 %d, ECMmode %d\n",
		 __func__, enable, ADCType,
		 mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1],
		 ECMmode);

	if (enable) {
		if (GetAdcStatus() == false) {
			audckbufEnable(true);

			/* Enable audio globe bias */
			NvregEnable(true);

			/* Enable CLKSQ 26MHz */
			ClsqEnable(true);

			/* set gpio miso mode */
			set_capture_gpio(true);

			/* Enable audio ADC CLKGEN  */
			Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1 << 5, 0x1 << 5);
			/* ADC CLK from CLKGEN (13MHz) */
			Ana_Set_Reg(AUDENC_ANA_CON3, 0x0000, 0xffff);
			/* Enable  LCLDO_ENC 1P8V */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x0100, 0x2500);
			/* LCLDO_ENC remote sense */
			Ana_Set_Reg(AUDDEC_ANA_CON14, 0x2500, 0x2500);

			/* DCC 50k CLK (from 26M) */
			Ana_Set_Reg(TOP_CLKSQ_SET, 0x3, 0x3);
			/* AdcClockEnable(true); */
			Topck_Enable(true);
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2062, 0xffff);
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2060, 0xffff);
			Ana_Set_Reg(AFE_DCCLK_CFG0, 0x2061, 0xffff);
			Ana_Set_Reg(AFE_DCCLK_CFG1, 0x1105, 0xffff);

			/* mic bias */
			if (mCodec_data
				->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] ==
			    0) {
				/* phone mic */
				switch (ECMmode) {
				case 1: /* AUDIO_MIC_MODE_DCCECMDIFF */
					Ana_Set_Reg(AUDENC_ANA_CON9, 0x7700,
						    0xff00);
					break;
				case 2: /* AUDIO_MIC_MODE_DCCECMSINGLE */
					Ana_Set_Reg(AUDENC_ANA_CON9, 0x1100,
						    0xff00);
					break;
				default:
					Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000,
						    0xff00);
					break;
				}
				/* Enable MICBIAS0, MISBIAS0 = 1P9V */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0021, 0x00ff);
			} else if (mCodec_data->mAudio_Ana_Mux
				       [AUDIO_MICSOURCE_MUX_IN_1] == 1) {
				/* headset mic */
				/* Enable MICBIAS1, MISBIAS1 = 2P6V */
				if (ECMmode == 2)
					Ana_Set_Reg(AUDENC_ANA_CON10, 0x0161,
						    0xffff);
				else
					Ana_Set_Reg(AUDENC_ANA_CON10, 0x0061,
						    0xffff);
			}

			SetMicPGAGain();
		}

		if (ADCType ==
		    AUDIO_ANALOG_DEVICE_IN_ADC1) { /* main and headset mic */
			/* Audio L preamplifier DCC precharge */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0004, 0xf8ff);

			if (mCodec_data
				->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] ==
			    0) {
				/* "ADC1", main_mic */
				/* Audio L preamplifier input sel : AIN0.
				 * Enable
				 * audio L PGA
				 */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0045, 0xf0ff);
				/* Audio L preamplifier DCCEN */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x1 << 1,
					    0x1 << 1);
				/* Audio L ADC input sel : L PGA. Enable audio
				 * L
				 * ADC
				 */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5047, 0xf000);
			} else if (mCodec_data->mAudio_Ana_Mux
				       [AUDIO_MICSOURCE_MUX_IN_1] == 1) {
				/* "ADC2", headset mic */
				/* Audio L preamplifier input sel : AIN1.
				 * Enable
				 * audio L PGA
				 */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x0085, 0xf0ff);
				/* Audio L preamplifier DCCEN */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x1 << 1,
					    0x1 << 1);
				/* Audio L ADC input sel : L PGA. Enable audio
				 * L
				 * ADC
				 */
				Ana_Set_Reg(AUDENC_ANA_CON0, 0x5087, 0xf000);
			}

			usleep_range(100, 150);
			/* Audio L preamplifier DCC precharge off */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0, 0x1 << 2);
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {
			/* Audio R preamplifier DCC precharge */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0004, 0xf8ff);

			/* ref mic */
			/* Audio R preamplifier input sel : AIN2. Enable audio
			 * R
			 * PGA
			 */
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
			/* Short body to VCM in PGA */
			Ana_Set_Reg(AUDENC_ANA_CON7, 0x0400, 0xffff);

			/* here to set digital part */
			/* power on clock */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x8000, 0xdfbf);

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
				/* afe power down and total audio clk disable
				 */
				Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x00ff,
					    0x00ff);
			}

			/* up-link power down */
			Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x0020, 0x0020);

			/* Disable Short body to VCM in PGA */
			Ana_Set_Reg(AUDENC_ANA_CON7, 0x0000, 0xffff);
		}

		if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC1) {
			/* Audio L ADC input sel : off, disable audio L ADC */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0xf000);
			/* Audio L preamplifier DCCEN */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0 << 1, 0x1 << 1);
			/* Audio L preamplifier input sel : off, Audio L PGA 0
			 * dB gain
			 */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0000, 0xfffb);
			/* Disable audio L PGA */

			/* disable Audio L preamplifier DCC precharge */
			Ana_Set_Reg(AUDENC_ANA_CON0, 0x0, 0x1 << 2);
		} else if (ADCType == AUDIO_ANALOG_DEVICE_IN_ADC2) {
			/* Audio R ADC input sel : off, disable audio R ADC */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0xf000);
			/* Audio r preamplifier DCCEN */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0 << 1, 0x1 << 1);
			/* Audio R preamplifier input sel : off, Audio R PGA 0
			 * dB gain
			 */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0000, 0x0ffb);
			/* Disable audio R PGA */

			/* disable Audio R preamplifier DCC precharge */
			Ana_Set_Reg(AUDENC_ANA_CON1, 0x0, 0x1 << 2);
		}

		if (GetAdcStatus() == false) {
			/* mic bias */
			if (mCodec_data
				->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1] ==
			    0) {
				/* phone mic */
				/* Disable MICBIAS0, MISBIAS0 = 1P7V */
				Ana_Set_Reg(AUDENC_ANA_CON9, 0x0000, 0xffff);
			} else if (mCodec_data->mAudio_Ana_Mux
				       [AUDIO_MICSOURCE_MUX_IN_1] == 1) {
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

			set_capture_gpio(false);

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

/* here start uplink power function */
static const char *const ADC_function[] = {"Off", "On"};
static const char *const ADC_power_mode[] = {"normal", "lowpower"};
static const char *const PreAmp_Mux_function[] = {"OPEN", "IN_ADC1", "IN_ADC2",
						  "IN_ADC3"};
enum preamp_input_select {
	PREAMP_INPUT_SELECT_NONE = 0,
	PREAMP_INPUT_SELECT_AIN0,
	PREAMP_INPUT_SELECT_AIN1,
	PREAMP_INPUT_SELECT_AIN2,
	NUM_PREAMP_INPUT_SELECT,
};

/* OPEN:0, IN_ADC1: 1, IN_ADC2:2, IN_ADC3:3 */
static const char *const ADC_UL_PGA_GAIN[] = {"0Db",  "6Db",  "12Db",
					      "18Db", "24Db", "30Db"};
static const char *const Pmic_Digital_Mux[] = {"ADC1", "ADC2", "ADC3", "ADC4"};
static const char *const Adc_Input_Sel[] = {"idle", "AIN", "Preamp"};

static const char *const Audio_AnalogMic_Mode[] = {
	"ACCMODE", "DCCMODE", "DMIC", "DCCECMDIFFMODE", "DCCECMSINGLEMODE"};

/* here start uplink power function */
static const char *const Pmic_Test_function[] = {"Off", "On"};
static const char *const Pmic_LPBK_function[] = {"Off", "LPBK3"};

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
};

static int Audio_ADC1_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s = %d\n", __func__,
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1]);
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
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, true,
					  0);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, true);
		else if (mAudio_Analog_Mic1_mode ==
			 AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, true,
					  1);
		else if (mAudio_Analog_Mic1_mode ==
			 AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, true,
					  2);

		mCodec_data
		    ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data
		    ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC1] =
		    ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, false,
					  0);
		else if (mAudio_Analog_Mic1_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC1, false);
		else if (mAudio_Analog_Mic1_mode ==
			 AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, false,
					  1);
		else if (mAudio_Analog_Mic1_mode ==
			 AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC1, false,
					  2);
	}
	mutex_unlock(&Ana_Power_Mutex);
	return 0;
}

static int Audio_ADC2_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug(
	    "%s = %d\n", __func__,
	    mCodec_data->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2]);
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
		if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2, true);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, true,
					  0);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2, true);
		else if (mAudio_Analog_Mic2_mode ==
			 AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, true,
					  1);
		else if (mAudio_Analog_Mic2_mode ==
			 AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, true,
					  2);

		mCodec_data
		    ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
		    ucontrol->value.integer.value[0];
	} else {
		mCodec_data
		    ->mAudio_Ana_DevicePower[AUDIO_ANALOG_DEVICE_IN_ADC2] =
		    ucontrol->value.integer.value[0];
		if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_ACC)
			TurnOnADcPowerACC(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DCC)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, false,
					  0);
		else if (mAudio_Analog_Mic2_mode == AUDIO_ANALOGUL_MODE_DMIC)
			TurnOnADcPowerDmic(AUDIO_ANALOG_DEVICE_IN_ADC2, false);
		else if (mAudio_Analog_Mic2_mode ==
			 AUDIO_ANALOGUL_MODE_DCCECMDIFF)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, false,
					  1);
		else if (mAudio_Analog_Mic2_mode ==
			 AUDIO_ANALOGUL_MODE_DCCECMSINGLE)
			TurnOnADcPowerDCC(AUDIO_ANALOG_DEVICE_IN_ADC2, false,
					  2);
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
		 mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1];
	return 0;
}

static int Audio_ADC1_Sel_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x00 << 13),
			    0x6000); /* pinumx sel */
	else if (ucontrol->value.integer.value[0] == 1)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x01 << 13), 0x6000); /* AIN0 */
	else if (ucontrol->value.integer.value[0] == 2)
		Ana_Set_Reg(AUDENC_ANA_CON0, (0x02 << 13),
			    0x6000); /* Left preamp */
	else
		pr_debug("%s() [AudioWarn]\n ", __func__);

	pr_debug("%s() done\n", __func__);
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC1] =
	    ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_ADC2_Sel_Get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s() = %d\n", __func__,
		 mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2];
	return 0;
}

static int Audio_ADC2_Sel_Set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Adc_Input_Sel)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x00 << 13),
			    0x6000); /* pinumx sel */
	else if (ucontrol->value.integer.value[0] == 1)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x01 << 13), 0x6000); /* AIN0 */
	else if (ucontrol->value.integer.value[0] == 2)
		Ana_Set_Reg(AUDENC_ANA_CON1, (0x02 << 13),
			    0x6000); /* Right preamp */
	else
		pr_debug("%s() [AudioWarn]\n ", __func__);

	pr_debug("%s() done\n", __func__);
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_MIC2] =
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
	pr_debug("%s Mul_Sel = %d ", __func__, Mul_Sel);
	if (Mul_Sel >= 0 && Mul_Sel < NUM_PREAMP_INPUT_SELECT)
		Ana_Set_Reg(AUDENC_ANA_CON0, Mul_Sel << 6, 0x3 << 6);
	else
		pr_debug("%s(), error, Mul_Sel = %d", __func__, Mul_Sel);

	return true;
}

static int Audio_PreAmp1_Get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
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
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1] =
	    ucontrol->value.integer.value[0];
	AudioPreAmp1_Sel(
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_1]);

	return 0;
}

static bool AudioPreAmp2_Sel(int Mul_Sel)
{
	pr_debug("%s Mul_Sel = %d ", __func__, Mul_Sel);

	if (Mul_Sel >= 0 && Mul_Sel < NUM_PREAMP_INPUT_SELECT)
		Ana_Set_Reg(AUDENC_ANA_CON1, Mul_Sel << 6, 0x3 << 6);
	else
		pr_debug("%s(), error, Mul_Sel = %d", __func__, Mul_Sel);

	return true;
}

static int Audio_PreAmp2_Get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
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
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2] =
	    ucontrol->value.integer.value[0];
	AudioPreAmp2_Sel(
	    mCodec_data->mAudio_Ana_Mux[AUDIO_ANALOG_MUX_IN_PREAMP_2]);
	return 0;
}

/* PGA1: PGA_L */
static int Audio_PGA1_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_AmpR_Get = %d\n",
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1];
	return 0;
}

static int Audio_PGA1_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	Ana_Set_Reg(AUDENC_ANA_CON0, (index << 8), 0x0700);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] =
	    ucontrol->value.integer.value[0];
	return 0;
}

/* PGA2: PGA_R */
static int Audio_PGA2_Get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s = %d\n", __func__,
	       mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2];
	return 0;
}

static int Audio_PGA2_Set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ADC_UL_PGA_GAIN)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	Ana_Set_Reg(AUDENC_ANA_CON1, index << 8, 0x0700);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] =
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
	pr_debug("%s = %d\n", __func__,
	       mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1]);
	ucontrol->value.integer.value[0] =
	    mCodec_data->mAudio_Ana_Mux[AUDIO_MICSOURCE_MUX_IN_1];
	return 0;
}

static int Audio_MicSource1_Set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	/* ADC1 Mic source selection, "ADC1" is main_mic, "ADC2" is headset_mic
	 */
	int index = 0;

	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(Pmic_Digital_Mux)) {
		pr_debug("return -EINVAL\n");
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
	pr_debug("%s() mAudio_Analog_Mic1_mode = %d\n", __func__,
	       mAudio_Analog_Mic1_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic1_mode;
	return 0;
}

static int Audio_Mic1_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_debug("return -EINVAL\n");
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
	pr_debug("%s()  = %d\n", __func__, mAudio_Analog_Mic2_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic2_mode;
	return 0;
}

static int Audio_Mic2_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic2_mode = ucontrol->value.integer.value[0];
	pr_debug("%s() mAudio_Analog_Mic2_mode = %d\n", __func__,
	       mAudio_Analog_Mic2_mode);
	return 0;
}

static int Audio_Mic3_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %d\n", __func__, mAudio_Analog_Mic3_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic3_mode;
	return 0;
}

static int Audio_Mic3_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic3_mode = ucontrol->value.integer.value[0];
	pr_debug("%s() mAudio_Analog_Mic3_mode = %d\n", __func__,
	       mAudio_Analog_Mic3_mode);
	return 0;
}

static int Audio_Mic4_Mode_Select_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()  = %d\n", __func__, mAudio_Analog_Mic4_mode);
	ucontrol->value.integer.value[0] = mAudio_Analog_Mic4_mode;
	return 0;
}

static int Audio_Mic4_Mode_Select_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(Audio_AnalogMic_Mode)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Analog_Mic4_mode = ucontrol->value.integer.value[0];
	pr_debug("%s() mAudio_Analog_Mic4_mode = %d\n", __func__,
	       mAudio_Analog_Mic4_mode);
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
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	mAdc_Power_Mode = ucontrol->value.integer.value[0];
	pr_debug("%s() mAdc_Power_Mode = %d\n", __func__, mAdc_Power_Mode);
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
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0002,
			    0x2); /* set UL from sinetable */
		Ana_Set_Reg(AFE_SGEN_CFG0, 0x0080, 0xffff);
		Ana_Set_Reg(AFE_SGEN_CFG1, 0x0101, 0xffff);
	} else {
		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000,
			    0x2); /* set UL from normal path */
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
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}

	if (ucontrol->value.integer.value[0] == 0) { /* disable pmic lpbk */
		Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0000,
			    0x0001); /* power off uplink */
		Ana_Set_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0, 0x0,
			    0xffff); /* disable new lpbk 2 */
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0000,
			    0x0001); /* turn off afe UL & DL */

		/* disable aud_pad RX & TX fifos */
		Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x0, 0x101);

		TurnOffDacPower();
	} else if (ucontrol->value.integer.value[0] >
		   0) { /* enable pmic lpbk */
		pr_debug("set PMIC LPBK3, DLSR=%d, ULSR=%d\n",
			 mBlockSampleRate[AUDIO_ANALOG_DEVICE_OUT_DAC],
			 mBlockSampleRate[AUDIO_ANALOG_DEVICE_IN_ADC]);

		/* set dl part */
		TurnOnDacPower(AUDIO_ANALOG_DEVICE_OUT_HEADSETL);

		Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x8000,
			    0xffff); /* power on clock */

		/* enable aud_pad TX fifos */
		Ana_Set_Reg(AFE_AUD_PAD_TOP, 0x3100, 0xff00);

		/* Set UL Part */
		Ana_Set_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0, 0x2,
			    0xffff); /* enable new lpbk 2 */

		Ana_Set_Reg(AFE_UL_SRC_CON0_L, 0x0001,
			    0xffff); /* power on uplink */

		Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0000,
			    0x0002); /* configure ADC setting */
		Ana_Set_Reg(AFE_UL_DL_CON0, 0x0001, 0xffff); /* turn on afe */
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

static void ADC_LOOP_DAC_Func(int command) { /* 6752 TODO? */}

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

static const struct snd_kcontrol_new mt6356_pmic_Test_controls[] = {
	SOC_ENUM_EXT("SineTable_DAC_HP", Pmic_Test_Enum[0],
		SineTable_DAC_HP_Get, SineTable_DAC_HP_Set),
	SOC_ENUM_EXT("DAC_LOOP_DAC_HS", Pmic_Test_Enum[1], ADC_LOOP_DAC_HS_Get,
		 ADC_LOOP_DAC_HS_Set),
	SOC_ENUM_EXT("DAC_LOOP_DAC_HP", Pmic_Test_Enum[2], ADC_LOOP_DAC_HP_Get,
		 ADC_LOOP_DAC_HP_Set),
	SOC_ENUM_EXT("Voice_Call_DAC_DAC_HS", Pmic_Test_Enum[3],
		 Voice_Call_DAC_DAC_HS_Get, Voice_Call_DAC_DAC_HS_Set),
	SOC_ENUM_EXT("SineTable_UL2", Pmic_Test_Enum[4], SineTable_UL2_Get,
		 SineTable_UL2_Set),
	SOC_ENUM_EXT("Pmic_Loopback", Pmic_Test_Enum[5], Pmic_Loopback_Get,
		 Pmic_Loopback_Set),
	SOC_SINGLE_EXT("Codec_Debug_Enable", SND_SOC_NOPM, 0, 0xffffffff, 0,
		   codec_debug_get, codec_debug_set),
};

static const struct snd_kcontrol_new mt6356_UL_Codec_controls[] = {
	SOC_ENUM_EXT("Audio_ADC_1_Switch", Audio_UL_Enum[0], Audio_ADC1_Get,
		 Audio_ADC1_Set),
	SOC_ENUM_EXT("Audio_ADC_2_Switch", Audio_UL_Enum[1], Audio_ADC2_Get,
		 Audio_ADC2_Set),
	SOC_ENUM_EXT("Audio_ADC_3_Switch", Audio_UL_Enum[2], Audio_ADC3_Get,
		 Audio_ADC3_Set),
	SOC_ENUM_EXT("Audio_ADC_4_Switch", Audio_UL_Enum[3], Audio_ADC4_Get,
		 Audio_ADC4_Set),
	SOC_ENUM_EXT("Audio_Preamp1_Switch", Audio_UL_Enum[4],
		Audio_PreAmp1_Get, Audio_PreAmp1_Set),
	SOC_ENUM_EXT("Audio_ADC_1_Sel", Audio_UL_Enum[5], Audio_ADC1_Sel_Get,
		 Audio_ADC1_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_2_Sel", Audio_UL_Enum[6], Audio_ADC2_Sel_Get,
		 Audio_ADC2_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_3_Sel", Audio_UL_Enum[7], Audio_ADC3_Sel_Get,
		 Audio_ADC3_Sel_Set),
	SOC_ENUM_EXT("Audio_ADC_4_Sel", Audio_UL_Enum[8], Audio_ADC4_Sel_Get,
		 Audio_ADC4_Sel_Set),
	SOC_ENUM_EXT("Audio_PGA1_Setting", Audio_UL_Enum[9], Audio_PGA1_Get,
		 Audio_PGA1_Set),
	SOC_ENUM_EXT("Audio_PGA2_Setting", Audio_UL_Enum[10], Audio_PGA2_Get,
		 Audio_PGA2_Set),
	SOC_ENUM_EXT("Audio_PGA3_Setting", Audio_UL_Enum[11], Audio_PGA3_Get,
		 Audio_PGA3_Set),
	SOC_ENUM_EXT("Audio_PGA4_Setting", Audio_UL_Enum[12], Audio_PGA4_Get,
		 Audio_PGA4_Set),
	SOC_ENUM_EXT("Audio_MicSource1_Setting", Audio_UL_Enum[13],
		 Audio_MicSource1_Get, Audio_MicSource1_Set),
	SOC_ENUM_EXT("Audio_MicSource2_Setting", Audio_UL_Enum[14],
		 Audio_MicSource2_Get, Audio_MicSource2_Set),
	SOC_ENUM_EXT("Audio_MicSource3_Setting", Audio_UL_Enum[15],
		 Audio_MicSource3_Get, Audio_MicSource3_Set),
	SOC_ENUM_EXT("Audio_MicSource4_Setting", Audio_UL_Enum[16],
		 Audio_MicSource4_Get, Audio_MicSource4_Set),
	SOC_ENUM_EXT("Audio_MIC1_Mode_Select", Audio_UL_Enum[17],
		 Audio_Mic1_Mode_Select_Get, Audio_Mic1_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC2_Mode_Select", Audio_UL_Enum[18],
		 Audio_Mic2_Mode_Select_Get, Audio_Mic2_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC3_Mode_Select", Audio_UL_Enum[19],
		 Audio_Mic3_Mode_Select_Get, Audio_Mic3_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_MIC4_Mode_Select", Audio_UL_Enum[20],
		 Audio_Mic4_Mode_Select_Get, Audio_Mic4_Mode_Select_Set),
	SOC_ENUM_EXT("Audio_Mic_Power_Mode", Audio_UL_Enum[21],
		 Audio_Adc_Power_Mode_Get, Audio_Adc_Power_Mode_Set),
	SOC_ENUM_EXT("Audio_Preamp2_Switch", Audio_UL_Enum[22],
		Audio_PreAmp2_Get, Audio_PreAmp2_Set),
	SOC_ENUM_EXT("Audio_UL_LR_Swap", Audio_UL_Enum[23],
		Audio_UL_LR_Swap_Get, Audio_UL_LR_Swap_Set),
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
	/* HPDET_COMP[6:0] @ efuse bit 1696 ~ 1702 */
	/* HPDET_COMP_SIGN @ efuse bit 1703 */
	/* 1696 / 8 = 212 --> 0xd4 */
	Ana_Set_Reg(OTP_CON0, 0xd4, 0xff);

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

static void mt6356_codec_init_reg(struct snd_soc_codec *codec)
{
	pr_debug("%s\n", __func__);

	audckbufEnable(true);
	/* Disable CLKSQ 26MHz */
	Ana_Set_Reg(TOP_CLKSQ_CLR, 0x1, 0x0001);
	Ana_Set_Reg(TOP_CLKSQ, 0x0, 0x0001);
	/* disable AUDGLB */
	Ana_Set_Reg(AUDDEC_ANA_CON13, 0x1 << 4, 0x1 << 4);
	/* Turn off AUDNCP_CLKDIV engine clock,Turn off AUD 26M */
	Ana_Set_Reg(AUD_TOP_CKPDN_CON0, 0x66, 0x66);
	/* set pdn for golden setting */
	Ana_Set_Reg(PMIC_AUDIO_TOP_CON0, 0x20ff, 0x20ff);
	/* Disable HeadphoneL/HeadphoneR short circuit protection */
	Ana_Set_Reg(AUDDEC_ANA_CON0, 0x3 << 12, 0x3 << 12);
	/* Disable voice short circuit protection */
	Ana_Set_Reg(AUDDEC_ANA_CON6, 0x1 << 4, 0x1 << 4);
	/* disable LO buffer left short circuit protection */
	Ana_Set_Reg(AUDDEC_ANA_CON7, 0x1 << 4, 0x1 << 4);
	/* gpio miso driving set to default 6mA, 0xcccc */
	Ana_Set_Reg(DRV_CON3, 0xcccc, 0xffff);
	/* set gpio */
	set_playback_gpio(false);
	set_capture_gpio(false);

	audckbufEnable(false);
}

void InitCodecDefault(void)
{
	pr_debug("%s\n", __func__);
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP1] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP2] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP3] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_MICAMP4] = 3;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = 8;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HPOUTR] = 8;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTL] = 8;
	mCodec_data->mAudio_Ana_Volume[AUDIO_ANALOG_VOLUME_HSOUTR] = 8;

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
	mAdc_Power_Mode = 0;
	mInitCodec = false;
	mAnaSuspend = false;
	audck_buf_Count = 0;
	ClsqCount = 0;
	TopCkCount = 0;
	NvRegCount = 0;
}

static struct task_struct *dc_trim_task;
static int dc_trim_thread(void *arg)
{
	get_hp_lr_trim_offset();
#ifdef CONFIG_MTK_ACCDET
	accdet_late_init(0);
#endif
	do_exit(0);
	return 0;
}

static int mt6356_codec_probe(struct snd_soc_codec *codec)
{
	int ret;

	pr_debug("%s()\n", __func__);

	if (mInitCodec == true)
		return 0;

	/* add codec controls */
	snd_soc_add_codec_controls(codec, mt6356_snd_controls,
				   ARRAY_SIZE(mt6356_snd_controls));
	snd_soc_add_codec_controls(codec, mt6356_UL_Codec_controls,
				   ARRAY_SIZE(mt6356_UL_Codec_controls));
	snd_soc_add_codec_controls(codec, mt6356_pmic_Test_controls,
				   ARRAY_SIZE(mt6356_pmic_Test_controls));

	snd_soc_add_codec_controls(codec, Audio_snd_auxadc_controls,
				   ARRAY_SIZE(Audio_snd_auxadc_controls));

	/* here to set  private data */
	mCodec_data = kzalloc(sizeof(struct mt6356_codec_priv), GFP_KERNEL);
	if (!mCodec_data) {
		/*pr_debug("Failed to allocate private data\n");*/
		return -ENOMEM;
	}
	snd_soc_codec_set_drvdata(codec, mCodec_data);

	memset((void *)mCodec_data, 0, sizeof(struct mt6356_codec_priv));
	mt6356_codec_init_reg(codec);
	InitCodecDefault();
	efuse_current_calibrate =
	    read_efuse_hp_impedance_current_calibration();
	mInitCodec = true;

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

static int mt6356_codec_remove(struct snd_soc_codec *codec) { return 0; }

static unsigned int mt6356_read(struct snd_soc_codec *codec, unsigned int reg)
{
	Ana_Get_Reg(reg);
	return 0;
}

static int mt6356_write(struct snd_soc_codec *codec, unsigned int reg,
			unsigned int value)
{
	Ana_Set_Reg(reg, value, 0xffffffff);
	return 0;
}

static struct snd_soc_codec_driver soc_mtk_codec = {
	.probe = mt6356_codec_probe,
	.remove = mt6356_codec_remove,

	.read = mt6356_read,
	.write = mt6356_write,
};

static int mtk_mt6356_codec_dev_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", MT_SOC_CODEC_NAME);

		/* check if use hp depop flow */
		of_property_read_u32(pdev->dev.of_node, "use_hp_depop_flow",
				     &mUseHpDepopFlow);

		pr_debug("%s(), use_hp_depop_flow = %d\n", __func__,
			 mUseHpDepopFlow);

	} else {
		pr_debug("%s(), pdev->dev.of_node = NULL!!!\n", __func__);
	}

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_codec(&pdev->dev, &soc_mtk_codec,
				      mtk_6356_dai_codecs,
				      ARRAY_SIZE(mtk_6356_dai_codecs));
}

static int mtk_mt6356_codec_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_codec_63xx_of_ids[] = {
	{
	.compatible = "mediatek,mt_soc_codec_63xx",
	},
	{} };
#endif

static struct platform_driver mtk_codec_6356_driver = {
	.driver = {
	    .name = MT_SOC_CODEC_NAME,
	    .owner = THIS_MODULE,
#ifdef CONFIG_OF
	    .of_match_table = mt_soc_codec_63xx_of_ids,
#endif
	},
	.probe = mtk_mt6356_codec_dev_probe,
	.remove = mtk_mt6356_codec_dev_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_codec6356_dev;
#endif

static int __init mtk_mt6356_codec_init(void)
{
	pr_debug("%s:\n", __func__);
#ifndef CONFIG_OF
	int ret = 0;

	soc_mtk_codec6356_dev = platform_device_alloc(MT_SOC_CODEC_NAME, -1);

	if (!soc_mtk_codec6356_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_codec6356_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_codec6356_dev);
		return ret;
	}
#endif
	InitGlobalVarDefault();

	return platform_driver_register(&mtk_codec_6356_driver);
}

module_init(mtk_mt6356_codec_init);

static void __exit mtk_mt6356_codec_exit(void)
{
	platform_driver_unregister(&mtk_codec_6356_driver);
}

module_exit(mtk_mt6356_codec_exit);

/* Module information */
MODULE_DESCRIPTION("MTK  codec driver");
MODULE_LICENSE("GPL v2");
