/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/dma-mapping.h>
#include <mt-plat/upmu_common.h>
/* #include <mach/upmu_sw.h> */
/* #include <mach/upmu_hw.h> */
/* #include <mach/mt_pmic_wrap.h> */
/* #include <mach/mt_gpio.h> */
#include <linux/time.h>
/* #include <mach/pmic_mt6325_sw.h> */
/* #include <cust_pmic.h> */
/* #include <cust_battery_meter.h> */

#ifndef CONFIG_MTK_CLKMGR
#include <linux/clk.h>
#else
#include <mach/mt_clkmgr.h>
#endif

/* #include <mach/mt_pm_ldo.h> */
/* #include <cust_gpio_usage.h> */

#include "mt_soc_afe_common.h"
#include "mt_soc_afe_def.h"
#include "mt_soc_afe_reg.h"
#include "mt_soc_afe_clk.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_codec_63xx.h"
#include "mt_soc_pcm_common.h"
#include "mt_auddrv_devtree_parser.h"
#include "auddrv_underflow_mach.h"
#include "mt_soc_afe_gpio.h"


/*
 *    function implementation
 */

static int mtk_afe_routing_probe(struct platform_device *pdev);
static int mtk_routing_pcm_close(struct snd_pcm_substream *substream);
static int mtk_asoc_routing_pcm_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_routing_platform_probe(struct snd_soc_platform *platform);

static int mDac_Sinegen = 21;
static const char * const DAC_DL_SIDEGEN[] = { "I0I1", "I2", "I3I4",
	"I5I6", "I7I8", "I9", "I10I11", "I12I13", "I14", "I15I16",
	"O0O1", "O2", "O3O4", "O5O6", "O7O8", "O9O10", "O11", "O12",
	"O13O14", "O15O16", "O17O18", "OFF"
};

static int mDac_SampleRate = 8;
static const char * const DAC_DL_SIDEGEN_SAMEPLRATE[] = {
	"8K", "11K", "12K", "16K", "22K", "24K", "32K", "44K", "48K" };

static int mDac_Sidegen_Amplitude = 6;
static const char * const DAC_DL_SIDEGEN_AMPLITUE[] = {
	"1/128", "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1" };

static bool mEnableSidetone;
static const char * const ENABLESIDETONE[] = { "Off", "On" };

static int mAudio_Mode;
static const char * const ANDROID_AUDIO_MODE[] = {
	"Normal_Mode", "Ringtone_Mode", "Incall_Mode", "Communication_Mode",
	"Incall2_Mode", "Incall_External_Mode" };

static const char * const InterModemPcm_ASRC_Switch[] = { "Off", "On" };
static const char * const Audio_Debug_Setting[] = { "Off", "On" };
static const char * const Audio_IPOH_State[] = { "Off", "On" };
static const char * const Audio_I2S1_Setting[] = { "Off", "On" };


static bool AudDrvSuspendStatus;

#if 0				/* not used */
static bool mModemPcm_ASRC_on;
#endif

static bool AudioI2S1Setting;
static bool mHplCalibrated;
static int mHplOffset;
static bool mHprCalibrated;
static int mHprOffset;
static bool AudDrvSuspend_ipoh_Status;

#define AUXADC_HP_L_CHANNEL 15
#define AUXADC_HP_R_CHANNEL 14

int Get_Audio_Mode(void)
{
	return mAudio_Mode;
}

static int Audio_SideGen_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_SideGen_Get = %d\n", mDac_Sinegen);
	ucontrol->value.integer.value[0] = mDac_Sinegen;
	return 0;
}

static int Audio_SideGen_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_SIDEGEN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	switch (index) {
	case 0:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionInput_I00,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT);
		break;
	case 1:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionInput_I02,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT);
		break;
	case 2:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionInput_I03,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT);
		break;
	case 3:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionInput_I05,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT);
		break;
	case 4:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionInput_I07,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT);
		break;
	case 5:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionInput_I09,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT);
		break;
	case 6:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionInput_I11,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT);
		break;
	case 7:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionInput_I12,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT);
		break;
	case 8:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionInput_I14,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT);
		break;
	case 9:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionInput_I15,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT);
		break;
	case 10:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O01,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
		break;
	case 11:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O02,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
		break;
	case 12:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O03,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
		break;
	case 13:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O05,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
		break;
	case 14:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O07,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
		break;
	case 15:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O09,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
		break;
	case 16:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O11,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
		break;
	case 17:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O12,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
		break;
	case 18:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O13,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
		break;
	case 19:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O15,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
		break;
	case 20:
		mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O17,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
		break;
	default:
		mt_afe_disable_sinegen_hw();
		break;
	}
	mDac_Sinegen = index;
	return 0;
}

static int Audio_SideGen_SampleRate_Get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s\n", __func__);
	ucontrol->value.integer.value[0] = mDac_SampleRate;
	return 0;
}

static int Audio_SideGen_SampleRate_Set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_SIDEGEN_SAMEPLRATE)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];

	switch (index) {
	case 0:
		SetSideGenSampleRate(8000);
		break;
	case 1:
		SetSideGenSampleRate(11025);
		break;
	case 2:
		SetSideGenSampleRate(12000);
		break;
	case 3:
		SetSideGenSampleRate(16000);
		break;
	case 4:
		SetSideGenSampleRate(22050);
		break;
	case 5:
		SetSideGenSampleRate(24000);
		break;
	case 6:
		SetSideGenSampleRate(32000);
		break;
	case 7:
		SetSideGenSampleRate(44100);
		break;
	case 8:
		SetSideGenSampleRate(48000);
		break;
	default:
		SetSideGenSampleRate(32000);
		break;
	}
	mDac_SampleRate = index;
	return 0;
}

static int Audio_SideGen_Amplitude_Get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_SideGen_Amplitude_Get = %d\n", mDac_Sidegen_Amplitude);
	return 0;
}

static int Audio_SideGen_Amplitude_Set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_SIDEGEN_AMPLITUE)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];
	mDac_Sidegen_Amplitude = index;
	return 0;
}

static int Audio_SideTone_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_SideTone_Get = %d\n", mEnableSidetone);
	ucontrol->value.integer.value[0] = mEnableSidetone;
	return 0;
}

static int Audio_SideTone_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ENABLESIDETONE)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	index = ucontrol->value.integer.value[0];
	if (mEnableSidetone != index) {
		mEnableSidetone = index;
		EnableSideToneFilter(mEnableSidetone);
	}

	return 0;
}

#if 0				/* not used */
static int Audio_ModemPcm_ASRC_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), mModemPcm_ASRC_on=%d\n", __func__, mModemPcm_ASRC_on);
	ucontrol->value.integer.value[0] = mModemPcm_ASRC_on;
	return 0;
}
#endif

static int AudioDebug_Setting_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_Debug_Setting)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionOutput_O03,
		Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT);
	msleep(5 * 1000);
	mt_afe_disable_sinegen_hw();
	mt_afe_enable_sinegen_hw(Soc_Aud_InterConnectionInput_I03,
		Soc_Aud_MemIF_Direction_DIRECTION_INPUT);
	msleep(5 * 1000);
	mt_afe_disable_sinegen_hw();
	analog_print();
	/*Afe_Log_Print();*/
	return 0;
}

static void Auddrv_I2S1GpioSet(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* I2S1 gpio set */
#ifdef CONFIG_OF

#if defined(CONFIG_MTK_LEGACY)
	AUDDRV_I2S_ATTRIBUTE * I2S1Settingws =
	    GetI2SSetting(Auddrv_I2S1_Setting, Auddrv_I2S_Setting_ws);
	AUDDRV_I2S_ATTRIBUTE *I2S1Settingbck =
	    GetI2SSetting(Auddrv_I2S1_Setting, Auddrv_I2S_Setting_bck);
	AUDDRV_I2S_ATTRIBUTE *I2S1SettingD00 =
	    GetI2SSetting(Auddrv_I2S1_Setting, Auddrv_I2S_Setting_D00);
	AUDDRV_I2S_ATTRIBUTE *I2S1SettingMclk =
	    GetI2SSetting(Auddrv_I2S1_Setting, Auddrv_I2S_Setting_Mclk);

	mt_set_gpio_mode(I2S1Settingws->Gpio_Number, I2S1Settingws->Gpio_Mode);
	mt_set_gpio_mode(I2S1Settingbck->Gpio_Number, I2S1Settingws->Gpio_Mode);
	mt_set_gpio_mode(I2S1SettingD00->Gpio_Number, I2S1Settingws->Gpio_Mode);
	mt_set_gpio_mode(I2S1SettingMclk->Gpio_Number, I2S1Settingws->Gpio_Mode);
#else
	AudDrv_GPIO_I2S_Select(true);
#endif

#else
	mt_set_gpio_mode(GPIO_I2S1_CK_PIN, GPIO_MODE_01);
	mt_set_gpio_mode(GPIO_I2S1_DAT_PIN, GPIO_MODE_01);
	mt_set_gpio_mode(GPIO_I2S1_MCLK_PIN, GPIO_MODE_01);
	mt_set_gpio_mode(GPIO_I2S1_WS_PIN, GPIO_MODE_01);
#endif
#endif
}

static void Auddrv_I2S1GpioReset(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef CONFIG_OF

#if defined(CONFIG_MTK_LEGACY)
	AUDDRV_I2S_ATTRIBUTE *I2S1Settingws =
	    GetI2SSetting(Auddrv_I2S1_Setting, Auddrv_I2S_Setting_ws);
	AUDDRV_I2S_ATTRIBUTE *I2S1Settingbck =
	    GetI2SSetting(Auddrv_I2S1_Setting, Auddrv_I2S_Setting_bck);
	AUDDRV_I2S_ATTRIBUTE *I2S1SettingD00 =
	    GetI2SSetting(Auddrv_I2S1_Setting, Auddrv_I2S_Setting_D00);
	AUDDRV_I2S_ATTRIBUTE *I2S1SettingMclk =
	    GetI2SSetting(Auddrv_I2S1_Setting, Auddrv_I2S_Setting_Mclk);

	mt_set_gpio_mode(I2S1Settingws->Gpio_Number, 0);
	mt_set_gpio_mode(I2S1Settingbck->Gpio_Number, 0);
	mt_set_gpio_mode(I2S1SettingD00->Gpio_Number, 0);
	mt_set_gpio_mode(I2S1SettingMclk->Gpio_Number, 0);
#else
	AudDrv_GPIO_I2S_Select(false);
#endif

#else
	mt_set_gpio_mode(GPIO_I2S1_CK_PIN, GPIO_MODE_00);
	mt_set_gpio_mode(GPIO_I2S1_DAT_PIN, GPIO_MODE_00);
	mt_set_gpio_mode(GPIO_I2S1_MCLK_PIN, GPIO_MODE_00);
	mt_set_gpio_mode(GPIO_I2S1_WS_PIN, GPIO_MODE_00);
#endif
#endif
}

static int AudioDebug_Setting_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	return 0;
}

static int AudioI2S1_Setting_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_I2S1_Setting)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	AudioI2S1Setting = ucontrol->value.enumerated.item[0];

	if (AudioI2S1Setting == true)
		Auddrv_I2S1GpioSet();
	else
		Auddrv_I2S1GpioReset();

	return 0;
}

static int AudioI2S1_Setting_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.enumerated.item[0] = AudioI2S1Setting;
	return 0;
}

#if 0
static int Audio_ModemPcm_ASRC_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("+%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(InterModemPcm_ASRC_Switch)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mModemPcm_ASRC_on = (bool) ucontrol->value.integer.value[0];
	Audio_ModemPcm2_ASRC_Set(mModemPcm_ASRC_on);
	pr_debug("-%s(), mModemPcm_ASRC_on=%d\n", __func__, mModemPcm_ASRC_on);
	return 0;
}
#endif

static int Audio_Ipoh_Setting_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	PRINTK_AUDDRV("%s()\n", __func__);
	ucontrol->value.integer.value[0] = AudDrvSuspend_ipoh_Status;
	return 0;
}

static int Audio_Ipoh_Setting_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	PRINTK_AUDDRV("+%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_IPOH_State)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	AudDrvSuspend_ipoh_Status = ucontrol->value.integer.value[0];

	return 0;
}

static int Audio_Mode_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	PRINTK_AUDDRV("Audio_SideTone_Get = %d\n", mAudio_Mode);
	ucontrol->value.integer.value[0] = mAudio_Mode;
	return 0;
}

static int Audio_Mode_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	PRINTK_AUDDRV("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ANDROID_AUDIO_MODE)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Mode = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Irqcnt1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	PRINTK_AUDDRV("Audio_Irqcnt1_Get\n");
	mt_afe_ana_clk_on();
	mt_afe_main_clk_on();
	ucontrol->value.integer.value[0] = mt_afe_get_reg(AFE_IRQ_MCU_CNT1);
	mt_afe_main_clk_off();
	mt_afe_ana_clk_off();
	return 0;
}

static int Audio_Irqcnt1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	uint32_t irq1_cnt = ucontrol->value.integer.value[0];

	PRINTK_AUDDRV("%s()\n", __func__);
	mt_afe_ana_clk_on();
	mt_afe_main_clk_on();
	mt_afe_set_reg(AFE_IRQ_MCU_CNT1, irq1_cnt, 0xffffffff);
	mt_afe_main_clk_off();
	mt_afe_ana_clk_off();
	return 0;
}

static int Audio_Irqcnt2_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	PRINTK_AUDDRV("Audio_Irqcnt2_Get\n");
	mt_afe_ana_clk_on();
	mt_afe_main_clk_on();
	ucontrol->value.integer.value[0] = mt_afe_get_reg(AFE_IRQ_MCU_CNT2);
	mt_afe_main_clk_off();
	mt_afe_ana_clk_off();
	return 0;
}

static int Audio_Irqcnt2_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	uint32_t irq1_cnt = ucontrol->value.integer.value[0];

	PRINTK_AUDDRV("%s()\n", __func__);
	mt_afe_ana_clk_on();
	mt_afe_main_clk_on();
	mt_afe_set_reg(AFE_IRQ_MCU_CNT2, irq1_cnt, 0xffffffff);
	mt_afe_main_clk_off();
	mt_afe_ana_clk_off();
	return 0;
}

#if 0
static struct snd_dma_buffer *Dl1_Playback_dma_buf;
#endif

static void GetAudioTrimOffset(int channels)
{
	int Buffer_on_value = 0, Buffer_offl_value = 0, Buffer_offr_value = 0;
	int count = 0, countlimit = 5;
	int val_hpr_on_sum = 0, val_hpl_on_sum = 0;
	const int off_counter = 20, on_counter = 20, Const_DC_OFFSET = 0;

	pr_debug("%s channels = %d\n", __func__, channels);
	/* open headphone and digital part */
	mt_afe_main_clk_on();
	mt_afe_emi_clk_on();
	OpenAfeDigitaldl1(true);

	setHpDcCalibration(AUDIO_ANALOG_DEVICE_OUT_HEADSETR, 0);
	setHpDcCalibration(AUDIO_ANALOG_DEVICE_OUT_HEADSETL, 0);
	/* get DC value when off */
	Buffer_offl_value = PMIC_IMM_GetOneChannelValue(AUXADC_HP_L_CHANNEL, off_counter, 0);
	pr_debug("%s, Buffer_offl_value = %d\n", __func__, Buffer_offl_value);

	Buffer_offr_value = PMIC_IMM_GetOneChannelValue(AUXADC_HP_R_CHANNEL, off_counter, 0);
	pr_debug("%s, Buffer_offr_value = %d\n", __func__, Buffer_offr_value);

	OpenAnalogHeadphone(true);
	setHpDcCalibrationGain(AUDIO_ANALOG_DEVICE_OUT_HEADSETR, 10);	/* -1dB, (9-(-1) = 10) */
	setHpDcCalibrationGain(AUDIO_ANALOG_DEVICE_OUT_HEADSETL, 10);

	usleep_range(10*1000, 15*1000);

	for (count = 0; count < countlimit; count++) {
		Buffer_on_value = PMIC_IMM_GetOneChannelValue(AUXADC_HP_L_CHANNEL, on_counter, 0);
		val_hpl_on_sum += Buffer_on_value;
		pr_debug
		("%s,on=%d,offl=%d,hpl_on_sum=%d\n", __func__, Buffer_on_value, Buffer_offl_value, val_hpl_on_sum);

		Buffer_on_value = PMIC_IMM_GetOneChannelValue(AUXADC_HP_R_CHANNEL, on_counter, 0);
		val_hpr_on_sum += Buffer_on_value;
		pr_debug
		("%s,on=%d,offr=%d,hpr_on_sum=%d\n", __func__, Buffer_on_value, Buffer_offr_value, val_hpr_on_sum);
	}
	mHplOffset = (val_hpl_on_sum / countlimit) - Buffer_offl_value + Const_DC_OFFSET;
	mHprOffset = (val_hpr_on_sum / countlimit) - Buffer_offr_value + Const_DC_OFFSET;
	pr_debug("%s, mHplOffset = %d, mHprOffset=%d\n", __func__, mHplOffset, mHprOffset);

	OpenAnalogHeadphone(false);

	OpenAfeDigitaldl1(false);

	mt_afe_emi_clk_off();
	mt_afe_main_clk_off();
}

static int Audio_Hpl_Offset_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
#ifndef EFUSE_HP_TRIM
	pr_debug("%s\n", __func__);
	mt_afe_ana_clk_on();
	mt_afe_main_clk_on();
	if (mHplCalibrated == false) {
		GetAudioTrimOffset(AUDIO_OFFSET_TRIM_MUX_HPL);
		SetHprTrimOffset(mHprOffset);
		SetHplTrimOffset(mHplOffset);
		mHplCalibrated = true;
		mHprCalibrated = true;
	}
	ucontrol->value.integer.value[0] = mHplOffset;
	mt_afe_main_clk_off();
	mt_afe_ana_clk_off();
#else
	ucontrol->value.integer.value[0] = 2048;
#endif
	return 0;
}

static int Audio_Hpl_Offset_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
#ifndef EFUSE_HP_TRIM
	mHplOffset = ucontrol->value.integer.value[0];
	SetHplTrimOffset(mHplOffset);
#else
	mHplOffset = ucontrol->value.integer.value[0];
#endif
	return 0;
}

static int Audio_Hpr_Offset_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s\n", __func__);
#ifndef EFUSE_HP_TRIM
	/*mt_afe_ana_clk_on();*/
	mt_afe_main_clk_on();
	if (mHprCalibrated == false) {
		GetAudioTrimOffset(AUDIO_OFFSET_TRIM_MUX_HPR);
		SetHprTrimOffset(mHprOffset);
		SetHplTrimOffset(mHplOffset);
		mHplCalibrated = true;
		mHprCalibrated = true;
	}
	ucontrol->value.integer.value[0] = mHprOffset;
	mt_afe_main_clk_off();
	/*mt_afe_ana_clk_off();*/
#else
	ucontrol->value.integer.value[0] = 2048;
#endif
	return 0;
}

static int Audio_Hpr_Offset_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
#ifndef EFUSE_HP_TRIM
	pr_debug("%s()\n", __func__);
	mHprOffset = ucontrol->value.integer.value[0];
	SetHprTrimOffset(mHprOffset);
#else
	mHprOffset = ucontrol->value.integer.value[0];
#endif
	return 0;
}

static const struct soc_enum Audio_Routing_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_SIDEGEN), DAC_DL_SIDEGEN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_SIDEGEN_SAMEPLRATE), DAC_DL_SIDEGEN_SAMEPLRATE),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_SIDEGEN_AMPLITUE), DAC_DL_SIDEGEN_AMPLITUE),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ENABLESIDETONE), ENABLESIDETONE),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ANDROID_AUDIO_MODE), ANDROID_AUDIO_MODE),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(InterModemPcm_ASRC_Switch), InterModemPcm_ASRC_Switch),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_Debug_Setting), Audio_Debug_Setting),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_IPOH_State), Audio_IPOH_State),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_I2S1_Setting), Audio_I2S1_Setting),
};

static const struct snd_kcontrol_new Audio_snd_routing_controls[] = {
	SOC_ENUM_EXT("Audio_SideGen_Switch", Audio_Routing_Enum[0], Audio_SideGen_Get,
		     Audio_SideGen_Set),
	SOC_ENUM_EXT("Audio_SideGen_SampleRate", Audio_Routing_Enum[1],
		     Audio_SideGen_SampleRate_Get,
		     Audio_SideGen_SampleRate_Set),
	SOC_ENUM_EXT("Audio_SideGen_Amplitude", Audio_Routing_Enum[2], Audio_SideGen_Amplitude_Get,
		     Audio_SideGen_Amplitude_Set),
	SOC_ENUM_EXT("Audio_Sidetone_Switch", Audio_Routing_Enum[3], Audio_SideTone_Get,
		     Audio_SideTone_Set),
	SOC_ENUM_EXT("Audio_Mode_Switch", Audio_Routing_Enum[4], Audio_Mode_Get,
		     Audio_Mode_Set),
	SOC_SINGLE_EXT("Audio IRQ1 CNT", SND_SOC_NOPM, 0, 65536, 0, Audio_Irqcnt1_Get,
		       Audio_Irqcnt1_Set),
	SOC_SINGLE_EXT("Audio IRQ2 CNT", SND_SOC_NOPM, 0, 65536, 0, Audio_Irqcnt2_Get,
		       Audio_Irqcnt2_Set),
	SOC_SINGLE_EXT("Audio HPL Offset", SND_SOC_NOPM, 0, 0x20000, 0, Audio_Hpl_Offset_Get,
		       Audio_Hpl_Offset_Set),
	SOC_SINGLE_EXT("Audio HPR Offset", SND_SOC_NOPM, 0, 0x20000, 0, Audio_Hpr_Offset_Get,
		       Audio_Hpr_Offset_Set),
#if 0
	SOC_ENUM_EXT("InterModemPcm_ASRC_Switch", Audio_Routing_Enum[5], Audio_ModemPcm_ASRC_Get,
		     Audio_ModemPcm_ASRC_Set),
#endif
	SOC_ENUM_EXT("Audio_Debug_Setting", Audio_Routing_Enum[6], AudioDebug_Setting_Get,
		     AudioDebug_Setting_Set),
	SOC_ENUM_EXT("Audio_Ipoh_Setting", Audio_Routing_Enum[7], Audio_Ipoh_Setting_Get,
		     Audio_Ipoh_Setting_Set),
	SOC_ENUM_EXT("Audio_I2S1_Setting", Audio_Routing_Enum[8], AudioI2S1_Setting_Get,
		     AudioI2S1_Setting_Set),
};


void EnAble_Anc_Path(int state)
{
	/* 6752 todo? */
	pr_debug("%s not supported in 6752!!!\n ", __func__);
}

static int m_Anc_State = AUDIO_ANC_ON;
static int Afe_Anc_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	ucontrol->value.integer.value[0] = m_Anc_State;
	return 0;
}

static int Afe_Anc_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	EnAble_Anc_Path(ucontrol->value.integer.value[0]);
	m_Anc_State = ucontrol->value.integer.value[0];
	return 0;
}

/* here start uplink power function */
static const char * const Afe_Anc_function[] = { "ANCON", "ANCOFF" };

static const struct soc_enum Afe_Anc_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Afe_Anc_function), Afe_Anc_function),
};

static const struct snd_kcontrol_new Afe_Anc_controls[] = {
	SOC_ENUM_EXT("Pmic_Anc_Switch", Afe_Anc_Enum[0], Afe_Anc_Get, Afe_Anc_Set),
};

static struct snd_soc_pcm_runtime *pruntimepcm;

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_normal_supported_sample_rates),
	.list = soc_normal_supported_sample_rates,
	.mask = 0,
};

static int mtk_routing_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_debug("mtk_routing_pcm_open\n");
	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	if (ret < 0)
		pr_err("snd_pcm_hw_constraint_integer failed\n");

	/* print for hw pcm information */
	pr_debug("mtk_routing_pcm_open runtime rate = %d channels = %d\n",
		runtime->rate, runtime->channels);

	if (substream->pcm->device & 1) {
		runtime->hw.info &= ~SNDRV_PCM_INFO_INTERLEAVED;
		runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	}

	if (substream->pcm->device & 2)
		runtime->hw.info &= ~(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_playback_constraints\n");

	if (ret < 0) {
		pr_err("mtk_routing_pcm_close\n");
		mtk_routing_pcm_close(substream);
		return ret;
	}
	pr_debug("mtk_routing_pcm_open return\n");
	return 0;
}

static int mtk_routing_pcm_close(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_routing_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("%s cmd = %d\n", __func__, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}
	return -EINVAL;
}

static int mtk_routing_pcm_copy(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				void __user *dst, snd_pcm_uframes_t count)
{
	count = count << 2;
	return 0;
}

static int mtk_routing_pcm_silence(struct snd_pcm_substream *substream,
				   int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	pr_debug("mtk_routing_pcm_silence\n");
	return 0;		/* do nothing */
}


static void *dummy_page[2];

static struct page *mtk_routing_pcm_page(struct snd_pcm_substream *substream, unsigned long offset)
{
	pr_debug("dummy_pcm_page\n");
	return virt_to_page(dummy_page[substream->stream]);	/* the same page */
}

static int mtk_routing_pcm_prepare(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_alsa_prepare\n");
	return 0;
}

static int mtk_routing_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	PRINTK_AUDDRV("mtk_routing_pcm_hw_params\n");
	return ret;
}

static int mtk_routing_pcm_hw_free(struct snd_pcm_substream *substream)
{
	PRINTK_AUDDRV("mtk_routing_pcm_hw_free\n");
	return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_ops mtk_afe_ops = {
	.open = mtk_routing_pcm_open,
	.close = mtk_routing_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_routing_pcm_hw_params,
	.hw_free = mtk_routing_pcm_hw_free,
	.prepare = mtk_routing_pcm_prepare,
	.trigger = mtk_routing_pcm_trigger,
	.copy = mtk_routing_pcm_copy,
	.silence = mtk_routing_pcm_silence,
	.page = mtk_routing_pcm_page,
};

static struct snd_soc_platform_driver mtk_soc_routing_platform = {
	.ops = &mtk_afe_ops,
	.pcm_new = mtk_asoc_routing_pcm_new,
	.probe = mtk_afe_routing_platform_probe,
};

static int mtk_afe_routing_probe(struct platform_device *pdev)
{
	pr_debug("mtk_afe_routing_probe\n");
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_ROUTING_PCM);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev, &mtk_soc_routing_platform);
}

static int mtk_asoc_routing_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pruntimepcm = rtd;
	pr_debug("%s\n", __func__);
	return ret;
}

static int mtk_afe_routing_platform_probe(struct snd_soc_platform *platform)
{
	pr_debug("mtk_afe_routing_platform_probe\n");
	/*add  controls */
	snd_soc_add_platform_controls(platform, Audio_snd_routing_controls,
				      ARRAY_SIZE(Audio_snd_routing_controls));
	snd_soc_add_platform_controls(platform, Afe_Anc_controls, ARRAY_SIZE(Afe_Anc_controls));
	Auddrv_Devtree_Init();
	return 0;
}


static int mtk_afe_routing_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

/* supend and resume function */
static int mtk_routing_pm_ops_suspend(struct device *device)
{
	pr_debug("%s\n", __func__);

	if (AudDrvSuspendStatus == false) {
		/*AudDrv_Clk_Power_On();*/ /*no need?*/
		backup_audio_register();
		if (ConditionEnterSuspend() == true) {
			SetAnalogSuspend(true);
#if 0
			clkmux_sel(MT_MUX_AUDINTBUS, 0, "AUDIO");	/* select 26M */
#endif
			mt_afe_suspend_clk_off();
		}
		AudDrvSuspendStatus = true;
	}
	return 0;
}

static int mtk_pm_ops_suspend_ipo(struct device *device)
{
	pr_debug("%s", __func__);
	AudDrvSuspend_ipoh_Status = true;
	return mtk_routing_pm_ops_suspend(device);
}

static int mtk_routing_pm_ops_resume(struct device *device)
{
	pr_debug("%s\n ", __func__);

	if (AudDrvSuspendStatus == true) {
		mt_afe_suspend_clk_on();
		if (ConditionEnterSuspend() == true) {
			afe_restore_audio_register();
			SetAnalogSuspend(false);
		}
		AudDrvSuspendStatus = false;
	}

	return 0;
}

static int mtk_pm_ops_resume_ipo(struct device *device)
{
	pr_debug("%s", __func__);
	return mtk_routing_pm_ops_resume(device);
}

const struct dev_pm_ops mtk_routing_pm_ops = {
	.suspend = mtk_routing_pm_ops_suspend,
	.resume = mtk_routing_pm_ops_resume,
	.freeze = mtk_pm_ops_suspend_ipo,
	.thaw = mtk_pm_ops_suspend_ipo,
	.poweroff = mtk_pm_ops_suspend_ipo,
	.restore = mtk_pm_ops_resume_ipo,
	.restore_noirq = mtk_pm_ops_resume_ipo,
};

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_routing_of_ids[] = {
	{.compatible = "mediatek," MT_SOC_ROUTING_PCM,},
	{}
};
MODULE_DEVICE_TABLE(of, mt_soc_pcm_routing_of_ids);

#endif

static struct platform_driver mtk_afe_routing_driver = {
	.driver = {
		   .name = MT_SOC_ROUTING_PCM,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_pcm_routing_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &mtk_routing_pm_ops,
#endif
		   },
	.probe = mtk_afe_routing_probe,
	.remove = mtk_afe_routing_remove,
};

#ifndef CONFIG_OF
static struct platform_device *mtk_afe_routing_driver;
#endif
#ifdef CONFIG_OF
module_platform_driver(mtk_afe_routing_driver);
#else
static int __init mtk_soc_routing_platform_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkafe_routing_dev = platform_device_alloc(MT_SOC_ROUTING_PCM, -1);

	if (!soc_mtkafe_routing_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkafe_routing_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkafe_routing_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_afe_routing_driver);
	return ret;
}
module_init(mtk_soc_routing_platform_init);

static void __exit mtk_soc_routing_platform_exit(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&mtk_afe_routing_driver);
}
module_exit(mtk_soc_routing_platform_exit);
#endif
MODULE_DESCRIPTION("afe eouting driver");
MODULE_LICENSE("GPL");
