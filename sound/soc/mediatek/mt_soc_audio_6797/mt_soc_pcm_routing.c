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
 *   mt6583.c
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio register
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
#include "mt_soc_afe_control.h"
#include "mt_soc_codec_63xx.h"
#include "mt_soc_pcm_common.h"
/*#include "mt_auddrv_devtree_parser.h"*/
#include "auddrv_underflow_mach.h"
#include "AudDrv_OffloadCommon.h"
#include "AudDrv_Common_func.h"
#include "AudDrv_Gpio.h"

#include <linux/time.h>
#include <linux/clk.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

/*
 *    function implementation
 */

static int mtk_afe_routing_probe(struct platform_device *pdev);
static int mtk_routing_pcm_close(struct snd_pcm_substream *substream);
static int mtk_asoc_routing_pcm_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_routing_platform_probe(struct snd_soc_platform *platform);
/* extern int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd); */

static int mDac_Sinegen = 27;	/* "OFF" */
static const char const *DAC_DL_SINEGEN[] = {
	"I0I1",   "I2",     "I3I4",   "I5I6",
	"I7I8",   "I9",     "I10I11", "I12I13",
	"I14",	  "I15I16", "I17I18", "I19I20",
	"I21I22", "O0O1",   "O2",     "O3O4",
	"O5O6",   "O7O8",   "O9O10",  "O11",
	"O12",	  "O13O14", "O15O16", "O17O18",
	"O19O20", "O21O22", "O23O24", "OFF",
	"O3",	  "O4",     "I25I26", "O25",
	"O28O29"
};

static int mDac_SampleRate = 8;	/* 48kHz */
static const char const *DAC_DL_SINEGEN_SAMEPLRATE[] = {
	"8K", "11K", "12K", "16K",
	"22K", "24K", "32K", "44K",
	"48K", "88k", "96k", "176k",
	"192k"
};

static int mDac_Sinegen_Amplitude = 6; /* "1/2" */
static const char const *DAC_DL_SINEGEN_AMPLITUE[] = {
	"1/128", "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1" };

static bool mEnableSideToneFilter;
static const char const *ENABLESTF[] = { "Off", "On" };

static int mAudio_Mode;
static const char const *ANDROID_AUDIO_MODE[] = {
	"Normal_Mode", "Ringtone_Mode", "Incall_Mode", "Communication_Mode", "Incall2_Mode",
	    "Incall_External_Mode"
};
static const char const *InterModemPcm_ASRC_Switch[] = { "Off", "On" };
static const char const *Audio_Debug_Setting[] = { "Off", "On" };
static const char const *Audio_IPOH_State[] = { "Off", "On" };
static const char const *Audio_I2S1_Setting[] = { "Off", "On" };


static bool AudDrvSuspendStatus;
/* static bool mModemPcm_ASRC_on = false; */
static bool AudioI2S1Setting;

static bool mHplCalibrated;
static int mHplOffset;
static bool mHprCalibrated;
static int mHprOffset;
static bool AudDrvSuspend_ipoh_Status;

int Get_Audio_Mode(void)
{
	return mAudio_Mode;
}

static int Audio_SineGen_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_AmpR_Get = %d\n", mDac_Sinegen);
	ucontrol->value.integer.value[0] = mDac_Sinegen;
	return 0;
}

static int Audio_SineGen_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_SINEGEN)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	switch (index) {
	case 0:
		EnableSineGen(Soc_Aud_InterConnectionInput_I00,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 1:
		EnableSineGen(Soc_Aud_InterConnectionInput_I02,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 2:
		EnableSineGen(Soc_Aud_InterConnectionInput_I03,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 3:
		EnableSineGen(Soc_Aud_InterConnectionInput_I05,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 4:
		EnableSineGen(Soc_Aud_InterConnectionInput_I07,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 5:
		EnableSineGen(Soc_Aud_InterConnectionInput_I09,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 6:
		EnableSineGen(Soc_Aud_InterConnectionInput_I11,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 7:
		EnableSineGen(Soc_Aud_InterConnectionInput_I12,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 8:
		EnableSineGen(Soc_Aud_InterConnectionInput_I14,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 9:
		EnableSineGen(Soc_Aud_InterConnectionInput_I15,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 10:
		EnableSineGen(Soc_Aud_InterConnectionInput_I17,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 11:
		EnableSineGen(Soc_Aud_InterConnectionInput_I19,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 12:
		EnableSineGen(Soc_Aud_InterConnectionInput_I21,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 13:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O01,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 14:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O02,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 15:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O03,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 16:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O05,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 17:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O07,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 18:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O09,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 19:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O11,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 20:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O12,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 21:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O13,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 22:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O15,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 23:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O17,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 24:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O19,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 25:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O21,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 26:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O23,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 27:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O11,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, false);
		EnableSineGen(Soc_Aud_InterConnectionInput_I00,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, false);
		break;
	case 28:
		Afe_Set_Reg(AFE_SGEN_CON0, 0x2E8c28c2, 0xffffffff);	/* o3o4 but mute o4 */
		break;
	case 29:
		Afe_Set_Reg(AFE_SGEN_CON0, 0x2D8c28c2, 0xffffffff);	/* o3o4 but mute o3 */
		break;
	case 30:
		EnableSineGen(Soc_Aud_InterConnectionInput_I25,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
		break;
	case 31:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O25,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	case 32:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O28,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
		break;
	default:
		EnableSineGen(Soc_Aud_InterConnectionOutput_O11,
				Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, false);
		EnableSineGen(Soc_Aud_InterConnectionInput_I00,
				Soc_Aud_MemIF_Direction_DIRECTION_INPUT, false);
		break;
	}
	mDac_Sinegen = index;
	return 0;
}

static int Audio_SineGen_SampleRate_Get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s\n", __func__);
	ucontrol->value.integer.value[0] = mDac_SampleRate;
	return 0;
}

static int Audio_SineGen_SampleRate_Set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_warn("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_SINEGEN_SAMEPLRATE)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];


	switch (index) {
	case 0:
		SetSineGenSampleRate(8000);
		break;
	case 1:
		SetSineGenSampleRate(11025);
		break;
	case 2:
		SetSineGenSampleRate(12000);
		break;
	case 3:
		SetSineGenSampleRate(16000);
		break;
	case 4:
		SetSineGenSampleRate(22050);
		break;
	case 5:
		SetSineGenSampleRate(24000);
		break;
	case 6:
		SetSineGenSampleRate(32000);
		break;
	case 7:
		SetSineGenSampleRate(44100);
		break;
	case 8:
		SetSineGenSampleRate(48000);
		break;
	case 9:
		SetSineGenSampleRate(88200);
		break;
	case 10:
		SetSineGenSampleRate(96000);
		break;
	case 11:
		SetSineGenSampleRate(176400);
		break;
	case 12:
		SetSineGenSampleRate(192000);
		break;
	default:
		SetSineGenSampleRate(48000);
		break;
	}

	mDac_SampleRate = index;
	return 0;
}

static int Audio_SineGen_Amplitude_Get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_AmpR_Get = %d\n", mDac_Sinegen_Amplitude);
	ucontrol->value.integer.value[0] = mDac_Sinegen_Amplitude;
	return 0;
}

static int Audio_SineGen_Amplitude_Set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_warn("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(DAC_DL_SINEGEN_AMPLITUE)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	SetSineGenAmplitude(index);
	mDac_Sinegen_Amplitude = index;
	return 0;
}

static int Audio_STF_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_SideTone_Get = %d\n", mEnableSideToneFilter);
	ucontrol->value.integer.value[0] = mEnableSideToneFilter;
	return 0;
}

static int Audio_STF_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	int index = 0;

	pr_warn("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ENABLESTF)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	index = ucontrol->value.integer.value[0];
	if (mEnableSideToneFilter != index) {
		mEnableSideToneFilter = index;
		EnableSideToneFilter(mEnableSideToneFilter);
	}
	return 0;
}

#if 0				/* not used */
static int Audio_ModemPcm_ASRC_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s(), mModemPcm_ASRC_on=%d\n", __func__, mModemPcm_ASRC_on);
	ucontrol->value.integer.value[0] = mModemPcm_ASRC_on;
	return 0;
}
#endif

static int AudioDebug_Setting_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_Debug_Setting)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	EnableSineGen(Soc_Aud_InterConnectionOutput_O03,
			Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, true);
	msleep(5 * 1000);
	EnableSineGen(Soc_Aud_InterConnectionOutput_O03,
			Soc_Aud_MemIF_Direction_DIRECTION_OUTPUT, false);
	EnableSineGen(Soc_Aud_InterConnectionInput_I03,
			Soc_Aud_MemIF_Direction_DIRECTION_INPUT, true);
	msleep(5 * 1000);
	EnableSineGen(Soc_Aud_InterConnectionInput_I03,
			Soc_Aud_MemIF_Direction_DIRECTION_INPUT, false);

	Ana_Log_Print();
	Afe_Log_Print();

	return 0;
}

static void Auddrv_I2S1GpioSet(void)
{

#ifndef CONFIG_FPGA_EARLY_PORTING

	/* I2S1 gpio set */
#ifdef CONFIG_OF
	AudDrv_GPIO_I2S_Select(true);
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
	AudDrv_GPIO_I2S_Select(false);
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
	pr_warn("%s()\n", __func__);
	return 0;
}

static int AudioI2S1_Setting_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
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
	pr_warn("%s()\n", __func__);
	ucontrol->value.enumerated.item[0] = AudioI2S1Setting;
	return 0;
}

#if 0
static int Audio_ModemPcm_ASRC_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("+%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(InterModemPcm_ASRC_Switch)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	mModemPcm_ASRC_on = (bool) ucontrol->value.integer.value[0];
	Audio_ModemPcm2_ASRC_Set(mModemPcm_ASRC_on);
	pr_warn("-%s(), mModemPcm_ASRC_on=%d\n", __func__, mModemPcm_ASRC_on);
	return 0;
}
#endif

static int Audio_Ipoh_Setting_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = AudDrvSuspend_ipoh_Status;
	return 0;
}

static int Audio_Ipoh_Setting_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("+%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(Audio_IPOH_State)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	AudDrvSuspend_ipoh_Status = ucontrol->value.integer.value[0];
	return 0;
}

static int Audio_Mode_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_SideTone_Get = %d\n", mAudio_Mode);
	ucontrol->value.integer.value[0] = mAudio_Mode;
	return 0;
}

static int Audio_Mode_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(ANDROID_AUDIO_MODE)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}
	mAudio_Mode = ucontrol->value.integer.value[0];
	return 0;
}

/* static struct snd_dma_buffer *Dl1_Playback_dma_buf  = NULL; */

static int GetAudioTrimOffsetAverage(int *buffer_value, int trim_num)
{
	int i , j, tmp;

	for (i = 0; i < trim_num+1; i++) {
		for (j = i+1; j < trim_num+2; j++) {
			if (buffer_value[i] > buffer_value[j]) {
				tmp = buffer_value[i];
				buffer_value[i] = buffer_value[j];
				buffer_value[j] = tmp;
			}
		}
	}

	tmp = 0;
	for (i = 0; i <  trim_num; i++)
		tmp = tmp + buffer_value[i+1];

	tmp = (tmp + 2) / 4;
	return tmp;
}

#ifdef AUDIO_DL2_ISR_COPY_SUPPORT

static int Audio_DL2_DataTransfer(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
#ifdef CONFIG_COMPAT
	void *addr =  compat_ptr(ucontrol->value.integer.value[0]);
#else
	void *addr =  (void *)ucontrol->value.integer.value[0];
#endif

	uint32 size =  ucontrol->value.integer.value[1];

	/* pr_warn("%s(), addr %p, size %d\n", __func__, addr, size); */

	mtk_dl2_copy2buffer(addr, size);
	return 0;
}

#endif

static void GetAudioTrimOffset(int channels)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	const int trim_num = 4;
	const int off_counter = 20, on_counter = 20, Const_DC_OFFSET = 2048;
	int Buffer_on_value = 0, Buffer_offl_value = 0, Buffer_offr_value = 0;
	int Buffer_tmp[6];
	int i;

	pr_warn("%s channels = %d\n", __func__, channels);
	/* open headphone and digital part */
	AudDrv_Clk_On();
	AudDrv_Emi_Clk_On();
	OpenAfeDigitaldl1(true);
	switch (channels) {
	case AUDIO_OFFSET_TRIM_MUX_HPL:
	case AUDIO_OFFSET_TRIM_MUX_HPR:{
			OpenTrimBufferHardware(true);
			setHpGainZero();
			break;
		}
	default:
		break;
	}

	/* Get HPL off offset */
	SetSdmLevel(AUDIO_SDM_LEVEL_MUTE);
	/*msleep(1); */
	usleep_range(1 * 1000, 20 * 1000);
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPL);
	setOffsetTrimBufferGain(3);
	EnableTrimbuffer(true);
	/*msleep(1); */
	usleep_range(1 * 1000, 20 * 1000);
	for (i = 0; i < (trim_num + 2); i++) {
		Buffer_tmp[i] = PMIC_IMM_GetOneChannelValue(PMIC_AUX_CH9, off_counter, 0);
		/* pr_warn("#%d Buffer_off_value_L = %d\n", i, Buffer_tmp[i]); */
	}
	Buffer_offl_value = GetAudioTrimOffsetAverage(Buffer_tmp, trim_num);
	pr_warn("[Average %d times] Buffer_off_value_L = %d\n", trim_num, Buffer_offl_value);
	EnableTrimbuffer(false);

	/* Get HPR off offset */
	SetSdmLevel(AUDIO_SDM_LEVEL_MUTE);
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPR);
	setOffsetTrimBufferGain(3);
	EnableTrimbuffer(true);
	/*msleep(1); */
	usleep_range(1 * 1000, 20 * 1000);
	for (i = 0; i < (trim_num + 2); i++) {
		Buffer_tmp[i] = PMIC_IMM_GetOneChannelValue(PMIC_AUX_CH9, off_counter, 0);
		/* pr_warn("#%d Buffer_off_value_R = %d\n", i, Buffer_tmp[i]); */
	}
	Buffer_offr_value = GetAudioTrimOffsetAverage(Buffer_tmp, trim_num);
	pr_warn("[Average %d times] Buffer_off_value_R = %d\n", trim_num, Buffer_offr_value);
	EnableTrimbuffer(false);

	switch (channels) {
	case AUDIO_OFFSET_TRIM_MUX_HPL:
	case AUDIO_OFFSET_TRIM_MUX_HPR:{
			OpenTrimBufferHardware(false);
			break;
		}
	default:
		break;
	}


	/* calibrate HPL offset trim */
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPL);
	setOffsetTrimBufferGain(3);
	/* EnableTrimbuffer(true); */
	/* msleep(5); */

	switch (channels) {
	case AUDIO_OFFSET_TRIM_MUX_HPL:
	case AUDIO_OFFSET_TRIM_MUX_HPR:{
			OpenAnalogHeadphone(true);
			setHpGainZero();
			break;
		}
	default:
		break;
	}
	EnableTrimbuffer(true);

	/*msleep(10); */
	usleep_range(10 * 1000, 20 * 1000);
	for (i = 0; i < (trim_num + 2); i++) {
		Buffer_tmp[i]  = PMIC_IMM_GetOneChannelValue(PMIC_AUX_CH9, on_counter, 0);
		/* pr_warn("#%d Buffer_on_value_L = %d\n", i, Buffer_tmp[i]); */
	}
	Buffer_on_value = GetAudioTrimOffsetAverage(Buffer_tmp, trim_num);
	pr_warn("[Average %d times] Buffer_onL_value = %d\n", trim_num, Buffer_on_value);
	mHplOffset = Buffer_on_value - Buffer_offl_value + Const_DC_OFFSET;
	pr_warn("Buffer_on_value_L = %d Buffer_off_value_L = %d mHplOffset = %d\n",
		Buffer_on_value, Buffer_offl_value, mHplOffset);

	EnableTrimbuffer(false);

	/* calibrate HPL offset trim */
	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_HPR);
	setOffsetTrimBufferGain(3);
	EnableTrimbuffer(true);
	/*msleep(1);    */
	usleep_range(10 * 1000, 20 * 1000);
	for (i = 0; i < (trim_num + 2); i++) {
		Buffer_tmp[i]  = PMIC_IMM_GetOneChannelValue(PMIC_AUX_CH9, on_counter, 0);
		/* pr_warn("#%d Buffer_on_value_R = %d\n", i, Buffer_tmp[i]); */
	}
	Buffer_on_value = GetAudioTrimOffsetAverage(Buffer_tmp, trim_num);
	pr_warn("[Average %d times] Buffer_onR_value = %d\n", trim_num, Buffer_on_value);
	mHprOffset = Buffer_on_value - Buffer_offr_value + Const_DC_OFFSET;
	pr_warn("Buffer_on_value_R = %d Buffer_off_value_R = %d mHprOffset = %d\n",
		Buffer_on_value, Buffer_offr_value, mHprOffset);

	switch (channels) {
	case AUDIO_OFFSET_TRIM_MUX_HPL:
	case AUDIO_OFFSET_TRIM_MUX_HPR:
		OpenAnalogHeadphone(false);
		break;
	}

	setOffsetTrimMux(AUDIO_OFFSET_TRIM_MUX_GROUND);
	EnableTrimbuffer(false);
	OpenAfeDigitaldl1(false);

	SetSdmLevel(AUDIO_SDM_LEVEL_NORMAL);
	AudDrv_Emi_Clk_Off();
	AudDrv_Clk_Off();
#endif
}

static int Audio_Hpl_Offset_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
#ifndef EFUSE_HP_TRIM
	pr_warn("%s\n", __func__);
	AudDrv_Clk_On();
	if (mHplCalibrated == false) {
		GetAudioTrimOffset(AUDIO_OFFSET_TRIM_MUX_HPL);
		SetHprTrimOffset(mHprOffset);
		SetHplTrimOffset(mHplOffset);
		mHplCalibrated = true;
		mHprCalibrated = true;
	}
	ucontrol->value.integer.value[0] = mHplOffset;
	AudDrv_Clk_Off();
#else
	ucontrol->value.integer.value[0] = 2048;
#endif
	return 0;
}

static int Audio_Hpl_Offset_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
#ifndef EFUSE_HP_TRIM
	pr_warn("%s()\n", __func__);
	mHplOffset = ucontrol->value.integer.value[0];
	SetHplTrimOffset(mHplOffset);
#else
	mHplOffset = ucontrol->value.integer.value[0];
#endif
	return 0;
}

static int Audio_Hpr_Offset_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
#ifndef EFUSE_HP_TRIM
	pr_warn("%s\n", __func__);
	AudDrv_Clk_On();
	if (mHprCalibrated == false) {
		GetAudioTrimOffset(AUDIO_OFFSET_TRIM_MUX_HPR);
		SetHprTrimOffset(mHprOffset);
		SetHplTrimOffset(mHplOffset);
		mHplCalibrated = true;
		mHprCalibrated = true;
	}
	ucontrol->value.integer.value[0] = mHprOffset;
	AudDrv_Clk_Off();
#else
	ucontrol->value.integer.value[0] = 2048;
#endif
	return 0;
}

static int Audio_Hpr_Offset_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
#ifndef EFUSE_HP_TRIM
	pr_warn("%s()\n", __func__);
	mHprOffset = ucontrol->value.integer.value[0];
	SetHprTrimOffset(mHprOffset);
#else
	mHprOffset = ucontrol->value.integer.value[0];
#endif
	return 0;
}

static const struct soc_enum Audio_Routing_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_SINEGEN), DAC_DL_SINEGEN),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_SINEGEN_SAMEPLRATE), DAC_DL_SINEGEN_SAMEPLRATE),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(DAC_DL_SINEGEN_AMPLITUE), DAC_DL_SINEGEN_AMPLITUE),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ENABLESTF), ENABLESTF),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ANDROID_AUDIO_MODE), ANDROID_AUDIO_MODE),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(InterModemPcm_ASRC_Switch), InterModemPcm_ASRC_Switch),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_Debug_Setting), Audio_Debug_Setting),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_IPOH_State), Audio_IPOH_State),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Audio_I2S1_Setting), Audio_I2S1_Setting),
};

static const struct snd_kcontrol_new Audio_snd_routing_controls[] = {
	SOC_ENUM_EXT("Audio_SineGen_Switch", Audio_Routing_Enum[0],
		     Audio_SineGen_Get, Audio_SineGen_Set),
	SOC_ENUM_EXT("Audio_SineGen_SampleRate", Audio_Routing_Enum[1],
		     Audio_SineGen_SampleRate_Get, Audio_SineGen_SampleRate_Set),
	SOC_ENUM_EXT("Audio_SineGen_Amplitude", Audio_Routing_Enum[2],
		     Audio_SineGen_Amplitude_Get, Audio_SineGen_Amplitude_Set),
	SOC_ENUM_EXT("Audio_Sidetone_Switch", Audio_Routing_Enum[3],
		     Audio_STF_Get, Audio_STF_Set),
	SOC_ENUM_EXT("Audio_Mode_Switch", Audio_Routing_Enum[4],
		     Audio_Mode_Get, Audio_Mode_Set),
	SOC_SINGLE_EXT("Audio HPL Offset", SND_SOC_NOPM, 0, 0x20000, 0,
		       Audio_Hpl_Offset_Get, Audio_Hpl_Offset_Set),
	SOC_SINGLE_EXT("Audio HPR Offset", SND_SOC_NOPM, 0, 0x20000, 0,
		       Audio_Hpr_Offset_Get, Audio_Hpr_Offset_Set),
	SOC_ENUM_EXT("Audio_Debug_Setting", Audio_Routing_Enum[6],
		     AudioDebug_Setting_Get, AudioDebug_Setting_Set),
	SOC_ENUM_EXT("Audio_Ipoh_Setting", Audio_Routing_Enum[7],
		     Audio_Ipoh_Setting_Get, Audio_Ipoh_Setting_Set),
	SOC_ENUM_EXT("Audio_I2S1_Setting", Audio_Routing_Enum[8],
		     AudioI2S1_Setting_Get, AudioI2S1_Setting_Set),
#ifdef AUDIO_DL2_ISR_COPY_SUPPORT
	SOC_DOUBLE_EXT("Audio_DL2_DataTransfer", SND_SOC_NOPM, 0, 1, 65536, 0,
	NULL, Audio_DL2_DataTransfer),
#endif
};


void EnAble_Anc_Path(int state)
{
	/* 6752 todo? */
	pr_warn("%s not supported in 6752!!!\n ", __func__);

}

static int m_Anc_State = AUDIO_ANC_ON;
static int Afe_Anc_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	ucontrol->value.integer.value[0] = m_Anc_State;
	return 0;
}

static int Afe_Anc_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s()\n", __func__);
	EnAble_Anc_Path(ucontrol->value.integer.value[0]);
	m_Anc_State = ucontrol->value.integer.value[0];
	return 0;
}

/* here start uplink power function */
static const char const *Afe_Anc_function[] = { "ANCON", "ANCOFF" };

static const struct soc_enum Afe_Anc_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Afe_Anc_function), Afe_Anc_function),
};

static const struct snd_kcontrol_new Afe_Anc_controls[] = {
	SOC_ENUM_EXT("Pmic_Anc_Switch", Afe_Anc_Enum[0], Afe_Anc_Get, Afe_Anc_Set),
};

static struct snd_soc_pcm_runtime *pruntimepcm;

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};

static int mtk_routing_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_warn("mtk_routing_pcm_open\n");

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");

	/* print for hw pcm information */
	pr_warn("mtk_routing_pcm_open runtime rate = %d channels = %d\n", runtime->rate,
		runtime->channels);
	if (substream->pcm->device & 1) {
		runtime->hw.info &= ~SNDRV_PCM_INFO_INTERLEAVED;
		runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	}
	if (substream->pcm->device & 2)
		runtime->hw.info &= ~(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID);

	if (ret < 0) {
		pr_warn("mtk_routing_pcm_close\n");
		mtk_routing_pcm_close(substream);
		return ret;
	}
	pr_warn("mtk_routing_pcm_open return\n");
	return 0;
}

static int mtk_routing_pcm_close(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_routing_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_warn("%s cmd = %d\n", __func__, cmd);
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
	pr_warn("mtk_routing_pcm_silence\n");
	return 0;		/* do nothing */
}


static void *dummy_page[2];

static struct page *mtk_routing_pcm_page(struct snd_pcm_substream *substream, unsigned long offset)
{
	pr_warn("dummy_pcm_page\n");
	return virt_to_page(dummy_page[substream->stream]);	/* the same page */
}

static int mtk_routing_pcm_prepare(struct snd_pcm_substream *substream)
{
	pr_warn("mtk_alsa_prepare\n");
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
	pr_warn("mtk_afe_routing_probe\n");

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_ROUTING_PCM);

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev, &mtk_soc_routing_platform);
}

static int mtk_asoc_routing_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pruntimepcm = rtd;
	pr_warn("%s\n", __func__);
	return ret;
}

static int mtk_afe_routing_platform_probe(struct snd_soc_platform *platform)
{
	pr_warn("mtk_afe_routing_platform_probe\n");

	/* add  controls */
	snd_soc_add_platform_controls(platform, Audio_snd_routing_controls,
				      ARRAY_SIZE(Audio_snd_routing_controls));
	snd_soc_add_platform_controls(platform, Afe_Anc_controls, ARRAY_SIZE(Afe_Anc_controls));
	/*Auddrv_Devtree_Init();*/
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
	pr_warn("%s\n", __func__);
	if (get_voice_status() ||
	    get_voice_md2_status() ||
	    get_voice_ultra_status())
		return 0;

	if (AudDrvSuspendStatus == false) {
		AudDrv_Clk_Power_On();
		/*BackUp_Audio_Register();*/	/* KC: no use */
		if (ConditionEnterSuspend() == true) {
			SetAnalogSuspend(true);
			/* clkmux_sel(MT_MUX_AUDINTBUS, 0, "AUDIO"); //select 26M */
			AudDrv_Suspend_Clk_Off();
		}
		AudDrvSuspendStatus = true;
	}
	return 0;
}

static int mtk_pm_ops_suspend_ipo(struct device *device)
{
	pr_warn("%s", __func__);
	AudDrvSuspend_ipoh_Status = true;
	return mtk_routing_pm_ops_suspend(device);
}

static int mtk_routing_pm_ops_resume(struct device *device)
{
	pr_warn("%s\n ", __func__);
	if (AudDrvSuspendStatus == true) {
		AudDrv_Suspend_Clk_On();
		if (ConditionEnterSuspend() == true) {
			/*Restore_Audio_Register();*/	/* KC: no use */
			SetAnalogSuspend(false);
		}
		AudDrvSuspendStatus = false;
	}
	return 0;
}

static int mtk_pm_ops_resume_ipo(struct device *device)
{
	pr_warn("%s", __func__);
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
	{.compatible = "mediatek,mt_soc_pcm_routing",},
	{}
};
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
static struct platform_device *soc_mtkafe_routing_dev;
#endif

static int __init mtk_soc_routing_platform_init(void)
{
	int ret = 0;

	pr_warn("%s\n", __func__);
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

	pr_warn("%s\n", __func__);
	platform_driver_unregister(&mtk_afe_routing_driver);
}
module_exit(mtk_soc_routing_platform_exit);

MODULE_DESCRIPTION("afe routing driver");
MODULE_LICENSE("GPL");
