/* max98927.c -- ALSA SoC Stereo MAX98927 driver
 * Copyright 2013-15 Maxim Integrated Products
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <sound/tlv.h>
#include "max98927.h"
#include <linux/regulator/consumer.h>

#define F0_DETECT 1

#define Q_DSM_ADAPTIVE_FC 9
#define Q_DSM_ADAPTIVE_DC_RES 27

static int i2c_states = -1;

int reg_common_map[][2] = {
	{MAX98927_Brownout_level_infinite_hold,  0x00},
	{MAX98927_Brownout_level_hold,  0x00},
	{MAX98927_Brownout__level_1_current_limit,  0x14},
	{MAX98927_Brownout__level_1_amp_1_control_1,  0x00},
	{MAX98927_Brownout__level_1_amp_1_control_2,  0x0c},
	{MAX98927_Brownout__level_1_amp_1_control_3,  0x00},
	{MAX98927_Brownout__level_2_current_limit,  0x10},
	{MAX98927_Brownout__level_2_amp_1_control_1,  0x00},
	{MAX98927_Brownout__level_2_amp_1_control_2,  0x0c},
	{MAX98927_Brownout__level_2_amp_1_control_3,  0x00},
	{MAX98927_Brownout__level_3_current_limit,  0x0c},
	{MAX98927_Brownout__level_3_amp_1_control_1,  0x06},
	{MAX98927_Brownout__level_3_amp_1_control_2,  0x18},
	{MAX98927_Brownout__level_3_amp_1_control_3,  0x0c},
	{MAX98927_Brownout__level_4_current_limit,  0x08},
	{MAX98927_Brownout__level_4_amp_1_control_1,  0x0e},
	{MAX98927_Brownout__level_4_amp_1_control_2,  0x80},
	{MAX98927_Brownout__level_4_amp_1_control_3,  0x00},
	{MAX98927_Brownout_threshold_hysterysis,  0x00},
	{MAX98927_Brownout_AMP_limiter_attack_release,  0x00},
	{MAX98927_Brownout_AMP_gain_attack_release,  0x00},
	{MAX98927_Brownout_AMP1_clip_mode,  0x00},
	{MAX98927_Meas_ADC_Config, 0x07},
	{MAX98927_Meas_ADC_Thermal_Warning_Threshhold, 0x78},
	{MAX98927_Meas_ADC_Thermal_Shutdown_Threshhold, 0xFF},
	{MAX98927_Pin_Config,  0x55},
	{MAX98927_Measurement_DSP_Config, 0x07},
	{MAX98927_PCM_Tx_Enables_B, 0x00},
	{MAX98927_PCM_Rx_Enables_B, 0x00},
	{MAX98927_PCM_Tx_Channel_Sources_B, 0x00},
	{MAX98927_PCM_Tx_HiZ_Control_B, 0xFF},
	{MAX98927_Measurement_enables, 0x03},
	{MAX98927_PDM_Rx_Enable,  0x00},
	{MAX98927_AMP_volume_control,  0x38},
	{MAX98927_AMP_DSP_Config,  0x33},
	{MAX98927_DRE_Control, 0x01},
	{MAX98927_Speaker_Gain,  0x05},
	{MAX98927_SSM_Configuration,  0x85},
	{MAX98927_Boost_Control_0, 0x1c},
	{MAX98927_Boost_Control_1, 0x3f},
	{MAX98927_Meas_ADC_Base_Divide_MSByte, 0x00},
	{MAX98927_Meas_ADC_Base_Divide_LSByte, 0x00},
	{MAX98927_Meas_ADC_Thermal_Hysteresis, 0x00},
	{MAX98927_Env_Tracker_Vout_Headroom, 0x08},
	{MAX98927_Env_Tracker_Control,  0x01},
	{MAX98927_Brownout_enables,  0x00},
};

int reg_mono_map[][2] = {
	{MAX98927_Boost_Control_3, 0x01},
	{MAX98927_PCM_Tx_Channel_Sources_A, 0x01},
	{MAX98927_PCM_Rx_Enables_A, 0x03},
	{MAX98927_PCM_Tx_Enables_A, 0x03},
	{MAX98927_PCM_Tx_HiZ_Control_A, 0xFC},
	{MAX98927_PCM_to_speaker_monomix_A, 0x80},
	{MAX98927_PCM_to_speaker_monomix_B, 0x00},
};

int reg_left_map[][2] = {
	{MAX98927_Boost_Control_3, 0x01},
	{MAX98927_PCM_Tx_Channel_Sources_A, 0x00},
	{MAX98927_PCM_Rx_Enables_A, 0x01},
	{MAX98927_PCM_Tx_Enables_A, 0x01},
	{MAX98927_PCM_Tx_HiZ_Control_A, 0xFE},
	{MAX98927_PCM_to_speaker_monomix_A, 0x80},
	{MAX98927_PCM_to_speaker_monomix_B, 0x00},
};

int reg_right_map[][2] = {
	{MAX98927_Boost_Control_3, 0x09},
	{MAX98927_PCM_Tx_Channel_Sources_A, 0x11},
	{MAX98927_PCM_Rx_Enables_A, 0x02},
	{MAX98927_PCM_Tx_Enables_A, 0x02},
	{MAX98927_PCM_Tx_HiZ_Control_A, 0xFD},
	{MAX98927_PCM_to_speaker_monomix_A, 0x81},
	{MAX98927_PCM_to_speaker_monomix_B, 0x01},
};

static bool max98927_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98927_Interrupt_Raw_1:
	case MAX98927_Interrupt_Raw_2:
	case MAX98927_Interrupt_Raw_3:
	case MAX98927_Interrupt_State_1:
	case MAX98927_Interrupt_State_2:
	case MAX98927_Interrupt_State_3:
	case MAX98927_Interrupt_Flag_1:
	case MAX98927_Interrupt_Flag_2:
	case MAX98927_Interrupt_Flag_3:
	case MAX98927_Interrupt_Enable_1:
	case MAX98927_Interrupt_Enable_2:
	case MAX98927_Interrupt_Enable_3:
	case MAX98927_IRQ_Control:
	case MAX98927_Clock_monitor_enable:
	case MAX98927_Watchdog_Control:
	case MAX98927_Meas_ADC_Thermal_Warning_Threshhold:
	case MAX98927_Meas_ADC_Thermal_Shutdown_Threshhold:
	case MAX98927_Meas_ADC_Thermal_Hysteresis:
	case MAX98927_Pin_Config:
	case MAX98927_PCM_Rx_Enables_A:
	case MAX98927_PCM_Rx_Enables_B:
	case MAX98927_PCM_Tx_Enables_A:
	case MAX98927_PCM_Tx_Enables_B:
	case MAX98927_PCM_Tx_HiZ_Control_A:
	case MAX98927_PCM_Tx_HiZ_Control_B:
	case MAX98927_PCM_Tx_Channel_Sources_A:
	case MAX98927_PCM_Tx_Channel_Sources_B:
	case MAX98927_PCM_Mode_Config:
	case MAX98927_PCM_Master_Mode:
	case MAX98927_PCM_Clock_setup:
	case MAX98927_PCM_Sample_rate_setup_1:
	case MAX98927_PCM_Sample_rate_setup_2:
	case MAX98927_PCM_to_speaker_monomix_A:
	case MAX98927_PCM_to_speaker_monomix_B:
	case MAX98927_ICC_RX_Enables_A:
	case MAX98927_ICC_RX_Enables_B:
	case MAX98927_ICC_TX_Enables_A:
	case MAX98927_ICC_TX_Enables_B:
	case MAX98927_ICC_Data_Order_Select:
	case MAX98927_ICC_HiZ_Manual_Mode:
	case MAX98927_ICC_TX_HiZ_Enables_A:
	case MAX98927_ICC_TX_HiZ_Enables_B:
	case MAX98927_ICC_Link_Enables:
	case MAX98927_PDM_Tx_Enables:
	case MAX98927_PDM_Tx_HiZ_Control:
	case MAX98927_PDM_Tx_Control:
	case MAX98927_PDM_Rx_Enable:
	case MAX98927_AMP_volume_control:
	case MAX98927_AMP_DSP_Config:
	case MAX98927_Tone_Generator_and_DC_Config:
	case MAX98927_DRE_Control:
	case MAX98927_AMP_enables:
	case MAX98927_Speaker_source_select:
	case MAX98927_Speaker_Gain:
	case MAX98927_SSM_Configuration:
	case MAX98927_Measurement_enables:
	case MAX98927_Measurement_DSP_Config:
	case MAX98927_Boost_Control_0:
	case MAX98927_Boost_Control_3:
	case MAX98927_Boost_Control_1:
	case MAX98927_Meas_ADC_Config:
	case MAX98927_Meas_ADC_Base_Divide_MSByte:
	case MAX98927_Meas_ADC_Base_Divide_LSByte:
	case MAX98927_Meas_ADC_Chan_0_Divide:
	case MAX98927_Meas_ADC_Chan_1_Divide:
	case MAX98927_Meas_ADC_Chan_2_Divide:
	case MAX98927_Meas_ADC_Chan_0_Filt_Config:
	case MAX98927_Meas_ADC_Chan_1_Filt_Config:
	case MAX98927_Meas_ADC_Chan_2_Filt_Config:
	case MAX98927_Meas_ADC_Chan_0_Readback:
	case MAX98927_Meas_ADC_Chan_1_Readback:
	case MAX98927_Meas_ADC_Chan_2_Readback:
	case MAX98927_Brownout_status:
	case MAX98927_Brownout_enables:
	case MAX98927_Brownout_level_infinite_hold:
	case MAX98927_Brownout_level_hold:
	case MAX98927_Brownout__level_1_threshold:
	case MAX98927_Brownout__level_2_threshold:
	case MAX98927_Brownout__level_3_threshold:
	case MAX98927_Brownout__level_4_threshold:
	case MAX98927_Brownout_threshold_hysterysis:
	case MAX98927_Brownout_AMP_limiter_attack_release:
	case MAX98927_Brownout_AMP_gain_attack_release:
	case MAX98927_Brownout_AMP1_clip_mode:
	case MAX98927_Brownout__level_1_current_limit:
	case MAX98927_Brownout__level_1_amp_1_control_1:
	case MAX98927_Brownout__level_1_amp_1_control_2:
	case MAX98927_Brownout__level_1_amp_1_control_3:
	case MAX98927_Brownout__level_2_current_limit:
	case MAX98927_Brownout__level_2_amp_1_control_1:
	case MAX98927_Brownout__level_2_amp_1_control_2:
	case MAX98927_Brownout__level_2_amp_1_control_3:
	case MAX98927_Brownout__level_3_current_limit:
	case MAX98927_Brownout__level_3_amp_1_control_1:
	case MAX98927_Brownout__level_3_amp_1_control_2:
	case MAX98927_Brownout__level_3_amp_1_control_3:
	case MAX98927_Brownout__level_4_current_limit:
	case MAX98927_Brownout__level_4_amp_1_control_1:
	case MAX98927_Brownout__level_4_amp_1_control_2:
	case MAX98927_Brownout__level_4_amp_1_control_3:
	case MAX98927_Env_Tracker_Vout_Headroom:
	case MAX98927_Env_Tracker_Boost_Vout_Delay:
	case MAX98927_Env_Tracker_Release_Rate:
	case MAX98927_Env_Tracker_Hold_Rate:
	case MAX98927_Env_Tracker_Control:
	case MAX98927_Env_Tracker__Boost_Vout_ReadBack:
	case MAX98927_Global_Enable:
	case MAX98927_REV_ID:
		return true;
	default:
		return false;
	}
}

static bool max98927_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98927_Interrupt_Raw_1:
	case MAX98927_Interrupt_Raw_2:
	case MAX98927_Interrupt_Raw_3:
	case MAX98927_Interrupt_State_1:
	case MAX98927_Interrupt_State_2:
	case MAX98927_Interrupt_State_3:
	case MAX98927_Interrupt_Flag_1:
	case MAX98927_Interrupt_Flag_2:
	case MAX98927_Interrupt_Flag_3:
	case MAX98927_Meas_ADC_Chan_0_Readback:
	case MAX98927_Meas_ADC_Chan_1_Readback:
	case MAX98927_Meas_ADC_Chan_2_Readback:
	case MAX98927_Brownout_status:
	case MAX98927_Env_Tracker__Boost_Vout_ReadBack:
		return true;
	default:
		return false;
	}
}

#define PKG_HEADER (48)
#define PAYLOAD_COUNT (256)

typedef enum {
	DSM_API_MONO_SPKER                  = 0x00000000,
	DSM_API_STEREO_SPKER                = 0x03000000,

	DSM_API_L_CHAN                      = 0x01000000,
	DSM_API_R_CHAN                      = 0x02000000,

	DSM_API_CHANNEL_1                   = 0x01000000,
	DSM_API_CHANNEL_2                   = 0x02000000,
	DSM_API_CHANNEL_3                   = 0x04000000,
	DSM_API_CHANNEL_4                   = 0x08000000,
	DSM_API_CHANNEL_5                   = 0x10000000,
	DSM_API_CHANNEL_6                   = 0x20000000,
	DSM_API_CHANNEL_7                   = 0x40000000,
	DSM_API_CHANNEL_8                   = 0x80000000,

	DSM_MAX_SUPPORTED_CHANNELS          = 8
} DSM_API_CHANNEL_ID;

#define DSM_SET_MONO_PARAM(cmdId)       ((cmdId&0x00FFFFFF)|DSM_API_MONO_SPKER)
#define DSM_SET_STEREO_PARAM(cmdId)     ((cmdId&0x00FFFFFF)|DSM_API_STEREO_SPKER)
#define DSM_SET_LEFT_PARAM(cmdId)       ((cmdId&0x00FFFFFF)|DSM_API_L_CHAN)
#define DSM_SET_RIGHT_PARAM(cmdId)      ((cmdId&0x00FFFFFF)|DSM_API_R_CHAN)

typedef struct dsm_params {
    uint32_t mode;
    uint32_t pcount;
    uint32_t pdata[PAYLOAD_COUNT];
} dsm_param_t;


static uint32_t gParam[PKG_HEADER+PAYLOAD_COUNT];

#ifdef F0_DETECT
static int f0_detect_open(struct inode *inode, struct file *filep)
{
	return 0;
}

static ssize_t f0_detect_read(struct file *filep, char __user *buf,
		size_t count, loff_t *ppos)
{
	int ret = 0;
	char param[20];
	int fc_left = 0;
	int fc_right = 0;

	if (*ppos > 1) {
		return 0;
	}

	snprintf(param, sizeof(param), "%d|%d", fc_left, fc_right);

	ret = copy_to_user(buf, param, strlen(param));
	if (ret != 0) {
		pr_err("%s: copy_to_user failed - %d\n", __func__, ret);
		return -EFAULT;
	}
	*ppos += strlen(param);

	pr_info("%s value:%s\n", __func__, param);
	return strlen(param);
}

static ssize_t f0_detect_write(struct file *filep, const char __user *buf,
		size_t count, loff_t *ppos)
{
	int rc = 0;
	char param[10];
	rc = copy_from_user(&param, buf, count);
	if (rc != 0) {
		pr_err("%s: copy_from_user failed - %d\n", __func__, rc);
		return rc;
	}
	pr_info("%s value:%s\n", __func__, param);
	return count;
}

static const struct file_operations f0_detect_ctrl_fops = {
	.owner		= THIS_MODULE,
	.open		= f0_detect_open,
	.release	= NULL,
	.read		= f0_detect_read,
	.write		= f0_detect_write,
	.mmap		= NULL,
	.poll		= NULL,
	.fasync		= NULL,
	.llseek		= NULL,
};

static struct miscdevice f0_detect_ctrl_miscdev = {
	.minor =	MISC_DYNAMIC_MINOR,
	.name =		"smartpa_f0_detect",
	.fops =		&f0_detect_ctrl_fops
};

static int f0_detect_init(void)
{
	int result;
	pr_info("%s\n", __func__);
	result = misc_register(&f0_detect_ctrl_miscdev);
	if (result != 0) {
		pr_err("%s error:%d\n", __func__, result);
	}
	return result;
}

static int f0_detect_deinit(void)
{
	int result;
	pr_info("%s\n", __func__);
	result = misc_deregister(&f0_detect_ctrl_miscdev);
	return result;
}
#endif

int max98927_wrapper_read(struct max98927_priv *max98927, bool speaker,
		unsigned int reg, unsigned int *val)
{
	int ret = 0;
	if (speaker == MAX98927L && max98927->left_i2c)
		ret = regmap_read(max98927->regmap_l, reg, val);
	if (speaker == MAX98927R && max98927->mono_stereo && max98927->right_i2c)
		ret = regmap_read(max98927->regmap_r, reg, val);
	return ret;
}

void max98927_wrapper_write(struct max98927_priv *max98927,
		unsigned int reg, unsigned int val)
{
	if (max98927->left_i2c)
		regmap_write(max98927->regmap_l, reg, val);
	if (max98927->mono_stereo && max98927->right_i2c)
		regmap_write(max98927->regmap_r, reg, val);
}

void max98927_wrap_update_bits(struct max98927_priv *max98927,
		unsigned int reg, unsigned int mask, unsigned int val)
{
	if (max98927->left_i2c)
		regmap_update_bits(max98927->regmap_l, reg, mask, val);
	if (max98927->mono_stereo && max98927->right_i2c)
		regmap_update_bits(max98927->regmap_r, reg, mask, val);
}

static int max98927_reg_get_w(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;

	max98927_wrapper_read(max98927, 0, reg, &val);

	val = (val >> shift) & mask;

	if (invert)
		ucontrol->value.integer.value[0] = max - val;
	else
		ucontrol->value.integer.value[0] = val;

	return 0;
}

static int max98927_reg_put_w(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;

	unsigned int val = (ucontrol->value.integer.value[0] & mask);
	if (invert)
		val = max - val;
	mask = mask << shift;
	val = val << shift;

	max98927_wrap_update_bits(max98927, reg, mask, val);
	pr_info("%s: register 0x%02X, value 0x%02X\n",
			__func__, reg, val);
	return 0;
}
static int max98927_reg_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, unsigned int reg,
		unsigned int mask, unsigned int shift)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data;

	max98927_wrapper_read(max98927, 0, reg, &data);
	ucontrol->value.integer.value[0] =
		(data & mask) >> shift;
	return 0;
}

static int max98927_reg_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol, unsigned int reg,
		unsigned int mask, unsigned int shift)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	unsigned int sel = ucontrol->value.integer.value[0];
	max98927_wrap_update_bits(max98927, reg, mask, sel << shift);
	pr_info("%s: register 0x%02X, value 0x%02X\n",
			__func__, reg, sel);
	return 0;
}

static int max98927_dai_set_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	pr_info("------%s------\n", __func__);

	pr_info("%s: fmt 0x%08X\n", __func__, fmt);
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
				MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_Mask,
				MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_SLAVE);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		max98927->master = true;
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
				MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_Mask,
				MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_MASTER);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
				MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_Mask,
				MAX98927_PCM_Master_Mode_PCM_MSTR_MODE_HYBRID);
	default:
		pr_info("DAI clock mode unsupported");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Mode_Config,
				MAX98927_PCM_Mode_Config_PCM_BCLKEDGE,
				0);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Mode_Config,
				MAX98927_PCM_Mode_Config_PCM_BCLKEDGE,
				MAX98927_PCM_Mode_Config_PCM_BCLKEDGE);
		break;
	default:
		pr_info("DAI invert mode unsupported");
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		max98927->iface |= SND_SOC_DAIFMT_I2S;
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Mode_Config,
				MAX98927_PCM_Mode_Config_PCM_FORMAT_Mask,
				MAX98927_PCM_Mode_Config_PCM_FORMAT_I2S);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		max98927->iface |= SND_SOC_DAIFMT_LEFT_J;
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Mode_Config,
				MAX98927_PCM_Mode_Config_PCM_FORMAT_Mask,
				MAX98927_PCM_Mode_Config_PCM_FORMAT_LEFT);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/* codec MCLK rate in master mode */
static const int rate_table[] = {
	5644800, 6000000, 6144000, 6500000,
	9600000, 11289600, 12000000, 12288000,
	13000000, 19200000,
};

static int max98927_set_clock(struct max98927_priv *max98927,
		struct snd_pcm_hw_params *params)
{
	/* BCLK/LRCLK ratio calculation */
	int blr_clk_ratio = 2*max98927->ch_size;
	int reg = MAX98927_PCM_Clock_setup;
	int mask = MAX98927_PCM_Clock_setup_PCM_BSEL_Mask;
	int value;
	pr_info("------%s------\n", __func__);

	if (max98927->master) {
		int i;
		/* match rate to closest value */
		for (i = 0; i < ARRAY_SIZE(rate_table); i++) {
			if (rate_table[i] >= max98927->sysclk)
				break;
		}
		if (i == ARRAY_SIZE(rate_table)) {
			pr_err("%s couldn't get the MCLK to match codec\n", __func__);
			return -EINVAL;
		}
		max98927_wrap_update_bits(max98927, MAX98927_PCM_Master_Mode,
				MAX98927_PCM_Master_Mode_PCM_MCLK_RATE_Mask,
				i << MAX98927_PCM_Master_Mode_PCM_MCLK_RATE_SHIFT);
	}
	switch (blr_clk_ratio) {
	case 32:
		value = 2;
		break;
	case 48:
		value = 3;
		break;
	case 64:
		value = 4;
		break;
	default:
		return -EINVAL;
	}
	max98927_wrap_update_bits(max98927,
			reg, mask, value);
	return 0;
}

static int max98927_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int sampling_rate = 0;
	pr_info("------%s------\n", __func__);

	switch (snd_pcm_format_width(params_format(params))) {
	case 16:
		max98927_wrap_update_bits(max98927,
				MAX98927_PCM_Mode_Config,
				MAX98927_PCM_Mode_Config_PCM_CHANSZ_16,
				MAX98927_PCM_Mode_Config_PCM_CHANSZ_16);
		max98927->ch_size = 16;
		break;
	case 24:
	case 32:
		max98927_wrap_update_bits(max98927,
				MAX98927_PCM_Mode_Config,
				MAX98927_PCM_Mode_Config_PCM_CHANSZ_32,
				MAX98927_PCM_Mode_Config_PCM_CHANSZ_32);
		max98927->ch_size = 32;
		break;
	default:
		pr_err("%s: format unsupported %d",
				__func__, params_format(params));
		goto err;
	}
	pr_info("%s: format supported %d",
			__func__, params_format(params));

	switch (params_rate(params)) {
	case 8000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_8000;
		break;
	case 11025:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_11025;
		break;
	case 12000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_12000;
		break;
	case 16000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_16000;
		break;
	case 22050:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_22050;
		break;
	case 24000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_24000;
		break;
	case 32000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_32000;
		break;
	case 44100:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_44100;
		break;
	case 48000:
		sampling_rate |=
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_48000;
		break;
	default:
		pr_err("%s rate %d not supported\n", __func__, params_rate(params));
		goto err;
	}
	/* set DAI_SR to correct LRCLK frequency */
	max98927_wrap_update_bits(max98927, MAX98927_PCM_Sample_rate_setup_1,
			MAX98927_PCM_Sample_rate_setup_1_DIG_IF_SR_Mask, sampling_rate);
	max98927_wrap_update_bits(max98927, MAX98927_PCM_Sample_rate_setup_2,
			MAX98927_PCM_Sample_rate_setup_2_SPK_SR_Mask, sampling_rate<<4);
	max98927_wrap_update_bits(max98927, MAX98927_PCM_Sample_rate_setup_2,
			MAX98927_PCM_Sample_rate_setup_2_IVADC_SR_Mask, sampling_rate);
	return max98927_set_clock(max98927, params);
err:
	return -EINVAL;
}

#define MAX98927_RATES SNDRV_PCM_RATE_8000_48000

#define MAX98927_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static int max98927_dai_set_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	pr_info("------%s------\n", __func__);

	pr_info("%s: clk_id %d, freq %d, dir %d\n", __func__, clk_id, freq, dir);

	max98927->sysclk = freq;
	return 0;
}

static int max98927_stream_mute(struct snd_soc_dai *codec_dai, int mute, int stream)
{
    struct snd_soc_codec *codec = codec_dai->codec;
    struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
    struct dsm_params *params = (struct dsm_params *)&gParam[PKG_HEADER];

    pr_info("%s--- stream %d, mute %d \n", __func__, stream, mute);

    if (!max98927) {
		pr_err("%s ------ priv data null pointer\n", __func__);
		return 0;
    }

    if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (mute) {
			params->pcount = 3;
			params->pdata[0] = DSM_SET_LEFT_PARAM(99);
			params->pdata[1] = 15;
			params->pdata[2] = DSM_SET_LEFT_PARAM(100);
			params->pdata[3] = 5000;
			params->pdata[4] = DSM_SET_LEFT_PARAM(102);
			params->pdata[5] = 1;

			usleep_range(20000, 20010);
			pr_info("%s ------ disable max98927 \n", __func__);
			max98927_wrap_update_bits(max98927, MAX98927_Global_Enable, 1, 0);
			max98927_wrap_update_bits(max98927, MAX98927_AMP_enables, 1, 0);
		} else {
			max98927_wrap_update_bits(max98927, MAX98927_AMP_enables, 1, 1);
			max98927_wrap_update_bits(max98927, MAX98927_Global_Enable, 1, 1);
		}
    }

    return 0;
}


static const struct snd_soc_dai_ops max98927_dai_ops = {
	.set_sysclk = max98927_dai_set_sysclk,
	.set_fmt = max98927_dai_set_fmt,
	.hw_params = max98927_dai_hw_params,
	.mute_stream =  max98927_stream_mute,
};


static int max98927_feedback_event(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol,
				    int event)
{
	u32  ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct max98927_priv  *max98927 = snd_soc_codec_get_drvdata(codec);
	if (!max98927) {
		pr_err("%s------priv data null pointer\n", __func__);
		return ret;
	}
	pr_info("%s---feedback event %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		max98927_wrapper_write(max98927, MAX98927_Measurement_enables, 0x3);
		break;
	case SND_SOC_DAPM_POST_PMD:
		max98927_wrapper_write(max98927, MAX98927_Measurement_enables, 0x0);
		break;
	default:
		break;
	}
	return ret;
}


static const struct snd_soc_dapm_widget max98927_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DAI_OUT", "HiFi Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("iv_feedback", "HiFi Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT_E("iv_feedback_e", "HiFi Capture", 0, SND_SOC_NOPM, 0, 0,
			max98927_feedback_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_INPUT("BE_IN"),
	SND_SOC_DAPM_OUTPUT("BE_OUT"),
};

static DECLARE_TLV_DB_SCALE(max98927_spk_tlv, 300, 300, 0);
static DECLARE_TLV_DB_SCALE(max98927_digital_tlv, -1600, 25, 0);

static int max98927_spk_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98927->spk_gain;
	pr_info("max98927_spk_gain_get: spk_gain setting returned %d\n",
			(int) ucontrol->value.integer.value[0]);

	return 0;
}

static int max98927_spk_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	pr_info("max98927_spk_gain_put: %d\n", sel);

	if (sel < ((1 << MAX98927_Speaker_Gain_Width) - 1)) {
		max98927_wrap_update_bits(max98927, MAX98927_Speaker_Gain,
				MAX98927_Speaker_Gain_SPK_PCM_GAIN_Mask, sel);
		max98927->spk_gain = sel;
	}
	return 0;
}

static int max98927_digital_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98927->digital_gain;
	pr_info("%s: spk_gain setting returned %d\n", __func__,
			(int) ucontrol->value.integer.value[0]);
	return 0;
}

static int max98927_digital_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	pr_info("max98927_digital_gain_put: %d\n", sel);

	if (sel <= ((1 << MAX98927_AMP_VOL_WIDTH) - 1)) {
		max98927_wrap_update_bits(max98927, MAX98927_AMP_volume_control,
				MAX98927_AMP_volume_control_AMP_VOL_Mask, sel);
		max98927->digital_gain = sel;
	}
	return 0;
}

static int max98927_boost_voltage_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_Boost_Control_0,
			MAX98927_Boost_Control_0_BST_VOUT_Mask, 0);
}

static int max98927_boost_voltage_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_Boost_Control_0,
			MAX98927_Boost_Control_0_BST_VOUT_Mask, 0);
}

static int max98927_boost_input_limit_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_Boost_Control_1,
			MAX98927_Boost_Control_1_BST_ILIM_Mask, MAX98927_BST_ILIM_SHIFT);
}

static int max98927_boost_input_limit_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_Boost_Control_1,
			MAX98927_Boost_Control_1_BST_ILIM_Mask, MAX98927_BST_ILIM_SHIFT);
}

static int max98927_spk_src_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_Speaker_source_select,
			MAX98927_Speaker_source_select_SPK_SOURCE_Mask, 0);
}

static int max98927_spk_src_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_Speaker_source_select,
			MAX98927_Speaker_source_select_SPK_SOURCE_Mask, 0);
}

static int max98927_mono_out_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_get(kcontrol, ucontrol, MAX98927_PCM_to_speaker_monomix_A,
			MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask, 0);
}

static int max98927_mono_out_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return max98927_reg_put(kcontrol, ucontrol, MAX98927_PCM_to_speaker_monomix_A,
			MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask, 0);
}

static int max98927_mono_out_get_l(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data = 0;

	if (max98927->left_i2c) {
		regmap_read(max98927->regmap_l, MAX98927_PCM_to_speaker_monomix_A, &data);
		ucontrol->value.integer.value[0] =
			(data & MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask);
		pr_info("%s: value:%d", __func__, data);
	}

	return 0;
}

static int max98927_mono_out_put_l(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	if (max98927->left_i2c) {
		regmap_update_bits(max98927->regmap_l, MAX98927_PCM_to_speaker_monomix_A,
				MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask, sel);
		regmap_update_bits(max98927->regmap_l, MAX98927_PCM_Rx_Enables_A,
				0xf, sel+1);
		pr_info("%s: register 0x%02X, value 0x%02X\n",
				__func__, MAX98927_PCM_to_speaker_monomix_A, sel);
	}

	return 0;
}

static int max98927_mono_out_get_r(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data = 0;

	if (max98927->mono_stereo && max98927->right_i2c) {
		regmap_read(max98927->regmap_r, MAX98927_PCM_to_speaker_monomix_A, &data);
		ucontrol->value.integer.value[0] =
			(data & MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask);
	}
	pr_info("%s: value:%d", __func__, data);
	return 0;
}

static int max98927_mono_out_put_r(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	if (max98927->mono_stereo && max98927->right_i2c) {
		regmap_update_bits(max98927->regmap_r, MAX98927_PCM_to_speaker_monomix_A,
				MAX98927_PCM_to_speaker_monomix_A_DMONOMIX_CH0_SOURCE_Mask, sel);
		regmap_update_bits(max98927->regmap_r, MAX98927_PCM_Rx_Enables_A, 0xf, sel+1);
		pr_info("%s: register 0x%02X, value 0x%02X\n",
				__func__, MAX98927_PCM_to_speaker_monomix_A, sel);
	} else {
		pr_info("%s: mono mode not support!!\n", __func__);
	}
	return 0;
}

static int max98927_feedback_en_get_l(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data = 0;

	if (max98927->left_i2c) {
		regmap_read(max98927->regmap_l, MAX98927_Measurement_enables, &data);
		ucontrol->value.integer.value[0] = data;
		pr_info("%s: value:%d", __func__, data);
	}

	return 0;
}

static int max98927_feedback_en_put_l(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];

	if (max98927->left_i2c) {
		regmap_write(max98927->regmap_l, MAX98927_Measurement_enables, sel);
		pr_info("%s: register 0x%02X, value 0x%02X\n",
			__func__, MAX98927_Measurement_enables, sel);
	}
	return 0;
}

static int max98927_feedback_en_get_r(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data = 0;

	if (max98927->mono_stereo && max98927->right_i2c) {
		regmap_read(max98927->regmap_r, MAX98927_Measurement_enables, &data);
		ucontrol->value.integer.value[0] = data;
	}
	pr_info("%s: value:%d", __func__, data);
	return 0;
}

static int max98927_feedback_en_put_r(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	if (max98927->mono_stereo && max98927->right_i2c) {
		regmap_write(max98927->regmap_r, MAX98927_Measurement_enables, sel);
		pr_info("%s: register 0x%02X, value 0x%02X\n",
				__func__, MAX98927_Measurement_enables, sel);
	} else {
		pr_info("%s: mono mode not support!!\n", __func__);
	}
	return 0;
}

static int max98927_left_channel_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data_global = 0;
	int data_amp = 0;


	if (max98927->left_i2c) {
		regmap_read(max98927->regmap_l, MAX98927_Global_Enable, &data_global);
		regmap_read(max98927->regmap_l, MAX98927_AMP_enables, &data_amp);
		ucontrol->value.integer.value[0] = (data_global & MAX98927_Global_Enable_EN)
			& (data_amp & MAX98927_AMP_enables_SPK_EN);
	}

	pr_info("%s: value:%d", __func__, (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int max98927_left_channel_enable_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	if (max98927->regmap_l && max98927->left_i2c) {
		max98927->spk_mode &= ~0x1;
		max98927->spk_mode |= sel;

		pr_info("%s: register 0x%02X, value 0x%02X\n",
				__func__, MAX98927_Global_Enable, sel);
	}
	return 0;
}

static int max98927_right_channel_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int data_global = 0;
	int data_amp = 0;

	if (max98927->mono_stereo && max98927->right_i2c) {
		regmap_read(max98927->regmap_r, MAX98927_Global_Enable, &data_global);
		regmap_read(max98927->regmap_r, MAX98927_AMP_enables, &data_amp);
		ucontrol->value.integer.value[0] = (data_global & MAX98927_Global_Enable_EN)
			& (data_amp & MAX98927_AMP_enables_SPK_EN);
	}

	pr_info("%s: value:%d", __func__, (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int max98927_right_channel_enable_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	unsigned int sel = ucontrol->value.integer.value[0];
	if (max98927->regmap_r && max98927->right_i2c) {
		max98927->spk_mode &= ~0x2;
		max98927->spk_mode |= sel<<0x1;
		pr_info("%s: register 0x%02X, value 0x%02X\n",
				__func__, MAX98927_Global_Enable, sel);
	} else {
		pr_info("%s: mono mode not support!!\n", __func__);
	}
	return 0;
}

static const char * const max98927_boost_voltage_text[] = {
	"6.5V", "6.625V", "6.75V", "6.875V", "7V", "7.125V", "7.25V", "7.375V",
	"7.5V", "7.625V", "7.75V", "7.875V", "8V", "8.125V", "8.25V", "8.375V",
	"8.5V", "8.625V", "8.75V", "8.875V", "9V", "9.125V", "9.25V", "9.375V",
	"9.5V", "9.625V", "9.75V", "9.875V", "10V"
};

static const char * const max98927_boost_current_limit_text[] = {
	"1.0A", "1.1A", "1.2A", "1.3A", "1.4A", "1.5A", "1.6A", "1.7A", "1.8A", "1.9A",
	"2.0A", "2.1A", "2.2A", "2.3A", "2.4A", "2.5A", "2.6A", "2.7A", "2.8A", "2.9A",
	"3.0A", "3.1A", "3.2A", "3.3A", "3.4A", "3.5A", "3.6A", "3.7A", "3.8A", "3.9A",
	"4.0A", "4.1A"
};

static const char * const max98927_speaker_source_text[] = {
	"i2s", "reserved", "tone", "pdm"
};

static const char * const max98927_monomix_output_text[] = {
	"ch_0", "ch_1", "ch_1_2_div"
};
static const char * const max98927_feedback_switch_text[] = {
	"OFF", "V_EN", "I_EN", "ON"
};

static const struct soc_enum max98927_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_monomix_output_text), max98927_monomix_output_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_speaker_source_text), max98927_speaker_source_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_boost_voltage_text), max98927_boost_voltage_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_feedback_switch_text), max98927_feedback_switch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(max98927_boost_current_limit_text), max98927_boost_current_limit_text),
};

static const struct snd_kcontrol_new max98927_snd_controls[] = {
	SOC_SINGLE_EXT_TLV("Speaker Volume", MAX98927_Speaker_Gain,
			0, (1<<MAX98927_Speaker_Gain_Width)-1, 0,
			max98927_spk_gain_get, max98927_spk_gain_put, max98927_spk_tlv),

	SOC_SINGLE_EXT_TLV("Digital Gain", MAX98927_AMP_volume_control,
			0, (1<<MAX98927_AMP_VOL_WIDTH)-1, 0,
			max98927_digital_gain_get, max98927_digital_gain_put, max98927_digital_tlv),

	SOC_SINGLE_EXT("BDE Enable", MAX98927_Brownout_enables,
			0, 1, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Amp DSP Enable", MAX98927_Brownout_enables,
			MAX98927_BDE_DSP_SHIFT, 1, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("BDE AMP Enable", MAX98927_Brownout_enables,
			1, 1, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Ramp Switch", MAX98927_AMP_DSP_Config,
			MAX98927_SPK_RMP_EN_SHIFT, 1, 1, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("DRE EN", MAX98927_DRE_Control,
			0, 1, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Amp Volume Location", MAX98927_AMP_volume_control,
			MAX98927_AMP_VOL_LOCATION_SHIFT, 1, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Level1 Threshold", MAX98927_Brownout__level_1_threshold,
			0, 255, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Level2 Threshold", MAX98927_Brownout__level_2_threshold,
			0, 255, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Level3 Threshold", MAX98927_Brownout__level_3_threshold,
			0, 255, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Level4 Threshold", MAX98927_Brownout__level_4_threshold,
			0, 255, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Level1 Current Limit", MAX98927_Brownout__level_1_current_limit,
			0, 63, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Level2 Current Limit", MAX98927_Brownout__level_2_current_limit,
			0, 63, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Level3 Current Limit", MAX98927_Brownout__level_3_current_limit,
			0, 63, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_SINGLE_EXT("Level4 Current Limit", MAX98927_Brownout__level_4_current_limit,
			0, 63, 0, max98927_reg_get_w, max98927_reg_put_w),

	SOC_ENUM_EXT("Boost Output Voltage", max98927_enum[2],
			max98927_boost_voltage_get, max98927_boost_voltage_put),

	SOC_ENUM_EXT("Boost Current Limit", max98927_enum[4],
			max98927_boost_input_limit_get, max98927_boost_input_limit_put),

	SOC_ENUM_EXT("Speaker Source", max98927_enum[1],
			max98927_spk_src_get, max98927_spk_src_put),

	SOC_ENUM_EXT("Monomix Output", max98927_enum[0],
			max98927_mono_out_get, max98927_mono_out_put),


	SOC_ENUM_EXT("Left Monomix Output", max98927_enum[0],
			max98927_mono_out_get_l, max98927_mono_out_put_l),


	SOC_ENUM_EXT("Right Monomix Output", max98927_enum[0],
			max98927_mono_out_get_r, max98927_mono_out_put_r),
	SOC_ENUM_EXT("Left Feedback Enable", max98927_enum[3],
			max98927_feedback_en_get_l, max98927_feedback_en_put_l),


	SOC_ENUM_EXT("Right Feedback Enable", max98927_enum[3],
			max98927_feedback_en_get_r, max98927_feedback_en_put_r),


	SOC_SINGLE_EXT("Left Channel Enable", MAX98927_Global_Enable,
			0, 1, 0, max98927_left_channel_enable_get, max98927_left_channel_enable_set),

	SOC_SINGLE_EXT("Right Channel Enable", MAX98927_Global_Enable,
			0, 1, 0, max98927_right_channel_enable_get, max98927_right_channel_enable_set),
};

static const struct snd_soc_dapm_route max98927_audio_map[] = {
	{"BE_OUT", NULL, "DAI_OUT"},
	{"iv_feedback_e", NULL, "BE_IN"},
};

static struct snd_soc_dai_driver max98927_dai[] = {
	{
		.name = "max98927-aif1",
		.playback = {
			.stream_name = "HiFi Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98927_RATES,
			.formats = MAX98927_FORMATS,
		},
		.capture = {
			.stream_name = "HiFi Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98927_RATES,
			.formats = MAX98927_FORMATS,
		},
		.ops = &max98927_dai_ops,
	}
};

static int max98927_probe(struct snd_soc_codec *codec)
{
	struct max98927_priv *max98927 = snd_soc_codec_get_drvdata(codec);
	int i;
		struct snd_soc_dapm_context *dapm = &codec->dapm;

	pr_info("------%s------\n", __func__);

	max98927->codec = codec;
	codec->control_data = max98927->regmap_l;
	codec->cache_bypass = 1;

	if (max98927->left_i2c) {
		if (max98927->mono_stereo) {
			for (i = 0; i < sizeof(reg_left_map)/sizeof(reg_left_map[0]); i++)
				regmap_write(max98927->regmap_l, reg_left_map[i][0], reg_left_map[i][1]);
		} else{
			for (i = 0; i < sizeof(reg_mono_map)/sizeof(reg_mono_map[0]); i++)
				regmap_write(max98927->regmap_l, reg_mono_map[i][0], reg_mono_map[i][1]);
		}
	}
	if (max98927->mono_stereo && max98927->right_i2c) {
		for (i = 0; i < sizeof(reg_right_map)/sizeof(reg_right_map[0]); i++)
			regmap_write(max98927->regmap_r, reg_right_map[i][0], reg_right_map[i][1]);
	}
	for (i = 0; i < sizeof(reg_common_map)/sizeof(reg_common_map[0]); i++)
		max98927_wrapper_write(max98927, reg_common_map[i][0], reg_common_map[i][1]);

	pr_info("%s: enter\n", __func__);

	max98927->codec = codec;
	snd_soc_dapm_ignore_suspend(dapm, "BE_OUT");
	snd_soc_dapm_ignore_suspend(dapm, "BE_IN");
	snd_soc_dapm_ignore_suspend(dapm, "HiFi Playback");
	snd_soc_dapm_ignore_suspend(dapm, "HiFi Capture");

	snd_soc_dapm_sync(dapm);
	return 0;
}

static const struct snd_soc_codec_driver soc_codec_dev_max98927 = {
	.probe			 = max98927_probe,
	.dapm_routes = max98927_audio_map,
	.num_dapm_routes = ARRAY_SIZE(max98927_audio_map),
	.dapm_widgets = max98927_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(max98927_dapm_widgets),
	.controls = max98927_snd_controls,
	.num_controls = ARRAY_SIZE(max98927_snd_controls),
};

static const struct regmap_config max98927_regmap = {
	.reg_bits		 = 16,
	.val_bits		 = 8,
	.max_register	 = MAX98927_REV_ID,
	.readable_reg = max98927_readable_register,
	.volatile_reg = max98927_volatile_register,
	.cache_type		 = REGCACHE_RBTREE,
};

int max98927_get_i2c_states(void)
{
	return i2c_states;
}
EXPORT_SYMBOL(max98927_get_i2c_states);

int probe_common(struct i2c_client *i2c, struct max98927_priv *max98927)
{
	int ret = -1, ret_l = -1, ret_r = -1, reg = 0;

	max98927->left_i2c = false;
	max98927->right_i2c = false;

	ret_l = regmap_read(max98927->regmap_l, MAX98927_REV_ID, &reg);
	pr_info("max98927 L device version 0x%02X\n", reg);
	if (ret_l == 0) {
		max98927->left_i2c = true;
		pr_info("max98927 L read ok.\n");
	}
	reg = 0;
	pr_info("max98927 R read enter.\n");
	if (max98927->mono_stereo) {
		ret_r = regmap_read(max98927->regmap_r, MAX98927_REV_ID, &reg);
		pr_info("max98927 R device version 0x%02X\n", reg);
		if (ret_r == 0) {
			max98927->right_i2c = true;
			pr_info("max98927 R read ok.\n");
		}
	}

	if ((ret_l != 0) && (ret_r != 0)) {
		pr_err("max98927 i2c connection error ret_l: %d ret_r:%d\n", ret_l, ret_r);
		return ret_l;
	}
	dev_set_name(&i2c->dev, "%s", "max98927");
	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_max98927,
			max98927_dai, ARRAY_SIZE(max98927_dai));
	if (ret < 0) {
		pr_err("Failed to register codec: %d\n", ret);
		if (max98927->regmap_l)
			regmap_exit(max98927->regmap_l);
		if (max98927->regmap_r)
			regmap_exit(max98927->regmap_r);
		kfree(max98927);
		return ret;
	}
	i2c_states = 0;

	pr_info("max98927 driver probe end.\n");

#ifdef F0_DETECT
	f0_detect_init();
#endif

	return 0;
}

static int max98927_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	static struct max98927_priv *max98927;
	int ret = 0, value;


	if (!max98927) {
		max98927 = devm_kzalloc(&i2c->dev,
				sizeof(*max98927), GFP_KERNEL);
		if (!max98927) {
			pr_info("------%s devm_kzalloc error!!\n", __func__);
			return -ENOMEM;
		}
	}
	max98927->dev = &i2c->dev;
	pr_info("max98927 reset gpio requse  name:%s------\n", i2c->name);

	max98927->reset_gpio_l = of_get_named_gpio(i2c->dev.of_node, "maxim,98927-reset-gpio", 0);
	pr_info("reset_gpio_l:%d------\n", max98927->reset_gpio_l);

    if (max98927->reset_gpio_l > 0) {
		ret = gpio_request(max98927->reset_gpio_l, "max_98927_reset");
		if (ret) {
			pr_err("max98927_i2c_probe : failed to request rest gpio %d error:%d\n",
					max98927->reset_gpio_l, ret);
			gpio_free(max98927->reset_gpio_l);
			return ret;
		}
		gpio_direction_output(max98927->reset_gpio_l, 0);
    }
	if (max98927->reset_gpio_l > 0) {
		msleep(10);
		gpio_direction_output(max98927->reset_gpio_l, 1);
		msleep(5);
	}

	i2c_set_clientdata(i2c, max98927);

	if (!of_property_read_u32(i2c->dev.of_node, "mono_stereo_mode", &value)) {
		if (value > 1) {
			pr_info("mono_stereo number is wrong:\n");
		}
		max98927->mono_stereo = value;
	}

	if (!of_property_read_u32(i2c->dev.of_node, "interleave_mode", &value)) {
		if (value > 1) {
			pr_info("interleave number is wrong:\n");
		}
		max98927->interleave_mode = value;
	}
	/* by default we are assuming interleave mode */
	if (id->driver_data == MAX98927L) {
		max98927->regmap_l =
			devm_regmap_init_i2c(i2c, &max98927_regmap);
		if (IS_ERR(max98927->regmap_l)) {
			ret = PTR_ERR(max98927->regmap_l);
			dev_err(&i2c->dev,
					"Failed to allocate regmap_l: %d\n", ret);
		}

	} else {/* check for second MAX98927 */
		if (id->driver_data == MAX98927R) {
			max98927->regmap_r =
				devm_regmap_init_i2c(i2c, &max98927_regmap);
			if (IS_ERR(max98927->regmap_r)) {
				ret = PTR_ERR(max98927->regmap_r);
				dev_err(&i2c->dev,
						"Failed to allocate regmap_r: %d\n", ret);
			}
		}
	}

	if (max98927->mono_stereo) {
		if (max98927->regmap_r && max98927->regmap_l)
			ret = probe_common(i2c, max98927);
	} else if (max98927->regmap_l && !max98927->mono_stereo) {
		ret = probe_common(i2c, max98927);
	}

	return ret;
}

static int max98927_i2c_remove(struct i2c_client *client)
{
	struct max98927_priv *max98927 = i2c_get_clientdata(client);
	if (max98927) {
		if (max98927->dev == &client->dev) {
			snd_soc_unregister_codec(&client->dev);
			if (max98927->regmap_l)
				regmap_exit(max98927->regmap_l);
			if (max98927->regmap_r)
				regmap_exit(max98927->regmap_r);
			kfree(max98927);
#ifdef F0_DETECT
			f0_detect_deinit();
#endif
		}
	}

	return 0;
}

static const struct i2c_device_id max98927_i2c_id[] = {
	{ "max98927L", MAX98927L },
	{ },
};

MODULE_DEVICE_TABLE(i2c, max98927_i2c_id);

static const struct of_device_id max98927_of_match[] = {
	{ .compatible = "maxim,max98927L", },
	{ }
};
MODULE_DEVICE_TABLE(of, max98927_of_match);

static struct i2c_driver max98927_i2c_driver = {
	.driver = {
		.name = "max98927",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max98927_of_match),
		.pm = NULL,
	},
	.probe	= max98927_i2c_probe,
	.remove = max98927_i2c_remove,
	.id_table = max98927_i2c_id,
};

module_i2c_driver(max98927_i2c_driver)

MODULE_DESCRIPTION("ALSA SoC MAX98927 driver");
MODULE_AUTHOR("Anish kumar <anish.kumar@maximintegrated.com>");
MODULE_LICENSE("GPL");
