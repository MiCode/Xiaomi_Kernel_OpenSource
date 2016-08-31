/*
 * linux/sound/soc/codecs/tlv320aic326x.c
 *
 * Copyright (C) 2011 Texas Instruments Inc.,
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * The TLV320AIC3262 is a flexible, low-power, low-voltage stereo audio
 * codec with digital microphone inputs and programmable outputs.
 *
 * History:
 *
 * Rev 0.1   ASoC driver support    TI	20-01-2011
 *
 *		The AIC325x ASoC driver is ported for the codec AIC3262.
 * Rev 0.2   ASoC driver support    TI	21-03-2011
 *		The AIC326x ASoC driver is updated for linux 2.6.32 Kernel.
 * Rev 0.3   ASoC driver support    TI	   20-04-2011
 *		The AIC326x ASoC driver is ported to 2.6.35 omap4 kernel
 */

/*
 *****************************************************************************
 * INCLUDES
 *****************************************************************************
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <sound/jack.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/input.h>

#include <sound/tlv.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/mfd/tlv320aic3262-registers.h>
#include <linux/mfd/tlv320aic3xxx-core.h>
#include "aic3xxx/aic3xxx_cfw.h"
#include "aic3xxx/aic3xxx_cfw_ops.h"

#include "tlv320aic326x.h"

#define CHECK_AIC326x_I2C_SHUTDOWN(a, c) { if (a && *(a->shutdown)) { \
dev_err(c->dev, "error: i2c state is 'shutdown'\n"); \
mutex_unlock(&a->mutex); return -ENODEV; } }

#define SOC_DOUBLE_R_SX_TLV3262(xname, xreg_left, xreg_right, xshift, \
		xmin, xmax, tlv_array) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ | \
		  SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.tlv.p = (tlv_array), \
	.info = snd_soc_info_volsw, \
	.get = snd_soc_get_volsw_sx, \
	.put = snd_soc_put_volsw_2r_sx_aic3262, \
	.private_value = (unsigned long)&(struct soc_mixer_control) \
		{.reg = xreg_left, \
		 .rreg = xreg_right, .shift = xshift, \
		 .min = xmin, .max = xmax} }

/******************************************************************************
			 Macros
******************************************************************************

******************************************************************************
		  Function Prototype
******************************************************************************/

static int aic3262_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai);

static int aic3262_mute(struct snd_soc_dai *dai, int mute);

static int aic3262_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt);

static int aic3262_dai_set_pll(struct snd_soc_dai *dai, int pll_id, int source,
				unsigned int Fin, unsigned int Fout);

static int aic3262_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level);

static int aic3262_set_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);
static int aic3262_set_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);

static int aic326x_adc_dsp_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event);
static int aic3262_get_runstate(struct snd_soc_codec *codec);
static int aic3262_dsp_pwrdwn_status(struct snd_soc_codec *codec);
static int aic3262_dsp_pwrup(struct snd_soc_codec *codec, int state);
static int aic3262_restart_dsps_sync(struct snd_soc_codec *codec, int rs);

static inline unsigned int dsp_non_sync_mode(unsigned int state)
			{ return (!((state & 0x03) && (state & 0x30))); }

/**
 * snd_soc_put_volsw_2r_sx - double with tlv and variable data size
 *  mixer put callback
 * @kcontrol: mixer control
 * @uinfo: control element information
 *
 * Returns 0 for success.
 */
int snd_soc_put_volsw_2r_sx_aic3262(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned int mask = (1<<mc->shift)-1;
	int min = mc->min;
	int ret;
	unsigned int val, valr;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	mutex_lock(&aic3262->mutex);
	CHECK_AIC326x_I2C_SHUTDOWN(aic3262, codec)

	val = ((ucontrol->value.integer.value[0]+min) & 0xff);
	val &= mask;
	valr = ((ucontrol->value.integer.value[1]+min) & 0xff);
	valr &= mask;

	ret = 0;
	ret = snd_soc_update_bits_locked(codec, mc->reg, mask, val);
	if (ret < 0) {
		mutex_unlock(&aic3262->mutex);
		return ret;
	}
	ret = snd_soc_update_bits_locked(codec, mc->rreg, mask, valr);
	if (ret < 0) {
		mutex_unlock(&aic3262->mutex);
		return ret;
	}
	mutex_unlock(&aic3262->mutex);
	return 0;
}

static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -6350, 50, 0);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -1200, 50, 0);
static const DECLARE_TLV_DB_SCALE(spk_gain_tlv, 600, 600, 0);
static const DECLARE_TLV_DB_SCALE(output_gain_tlv, -600, 100, 1);
static const DECLARE_TLV_DB_SCALE(micpga_gain_tlv, 0, 50, 0);
static const DECLARE_TLV_DB_SCALE(adc_fine_gain_tlv, -40, 10, 0);
static const DECLARE_TLV_DB_SCALE(beep_gen_volume_tlv, -6300, 100, 0);

/* Chip-level Input and Output CM Mode Controls */
static const char * const input_common_mode_text[] = {
	"0.9v", "0.75v"
};

static const char * const output_common_mode_text[] = {
	"Input CM", "1.25v", "1.5v", "1.65v"
};

static const struct soc_enum input_cm_mode =
SOC_ENUM_SINGLE(AIC3262_CM_REG, 2, 2, input_common_mode_text);

static const struct soc_enum output_cm_mode =
SOC_ENUM_SINGLE(AIC3262_CM_REG, 0, 4, output_common_mode_text);
/*
 *****************************************************************************
 * Structure Initialization
 *****************************************************************************
 */
static const struct snd_kcontrol_new aic3262_snd_controls[] = {
	/* Output */
#ifndef DAC_INDEPENDENT_VOL
	/* sound new kcontrol for PCM Playback volume control */

	SOC_DOUBLE_R_SX_TLV3262("PCM Playback Volume",
				AIC3262_DAC_LVOL, AIC3262_DAC_RVOL, 8,
				0xffffff81,
				0x30, dac_vol_tlv),
#endif
	/*HP Driver Gain Control */
	SOC_DOUBLE_R_SX_TLV3262("HeadPhone Driver Amplifier Volume",
				AIC3262_HPL_VOL, AIC3262_HPR_VOL, 6, 0xffffffb9,
				0xffffffce, output_gain_tlv),
	/*LO Driver Gain Control */
	SOC_DOUBLE_TLV("Speaker Amplifier Volume", AIC3262_SPK_AMP_CNTL_R4, 4,
			0, 5, 0, spk_gain_tlv),

	SOC_DOUBLE_R_SX_TLV3262("Receiver Amplifier Volume",
				AIC3262_REC_AMP_CNTL_R5, AIC3262_RAMPR_VOL, 6,
				0xffffffb9, 0xffffffd6, output_gain_tlv),

	SOC_DOUBLE_R_SX_TLV3262("PCM Capture Volume", AIC3262_LADC_VOL,
				AIC3262_RADC_VOL, 7, 0xffffff68, 0xffffffa8,
				adc_vol_tlv),

	SOC_DOUBLE_R_TLV("MicPGA Volume Control", AIC3262_MICL_PGA,
			 AIC3262_MICR_PGA, 0, 0x5F, 0, micpga_gain_tlv),

	SOC_DOUBLE_TLV("PCM Capture Fine Gain Volume", AIC3262_ADC_FINE_GAIN,
			4, 0, 5, 1, adc_fine_gain_tlv),

	SOC_DOUBLE("ADC channel mute", AIC3262_ADC_FINE_GAIN, 7, 3, 1, 0),

	SOC_DOUBLE("DAC MUTE", AIC3262_DAC_MVOL_CONF, 2, 3, 1, 1),

	SOC_SINGLE("RESET", AIC3262_RESET_REG, 0, 1, 0),

	SOC_SINGLE("DAC VOL SOFT STEPPING", AIC3262_DAC_MVOL_CONF, 0, 2, 0),

	SOC_SINGLE("DAC AUTO MUTE CONTROL", AIC3262_DAC_MVOL_CONF, 4, 7, 0),

	SOC_SINGLE("RIGHT MODULATOR SETUP", AIC3262_DAC_MVOL_CONF, 7, 1, 0),

	SOC_SINGLE("ADC Volume soft stepping", AIC3262_ADC_CHANNEL_POW,
		   0, 3, 0),

	SOC_SINGLE("Mic Bias ext independent enable", AIC3262_MIC_BIAS_CNTL,
		   7, 1, 0),

	SOC_SINGLE("MICBIAS EXT Power Level", AIC3262_MIC_BIAS_CNTL, 4, 3, 0),

	SOC_SINGLE("MICBIAS INT Power Level", AIC3262_MIC_BIAS_CNTL, 0, 3, 0),

	SOC_SINGLE("BEEP_GEN_EN", AIC3262_BEEP_CNTL_R1, 7, 1, 0),

	SOC_DOUBLE_R("BEEP_VOL_CNTL", AIC3262_BEEP_CNTL_R1,
		     AIC3262_BEEP_CNTL_R2, 0, 0x0F, 1),

	SOC_SINGLE("BEEP_MAS_VOL", AIC3262_BEEP_CNTL_R2, 6, 3, 0),

	SOC_SINGLE("DAC PRB Selection", AIC3262_DAC_PRB, 0, 26, 0),

	SOC_SINGLE("ADC PRB Selection", AIC3262_ADC_PRB, 0, 18, 0),

	SOC_ENUM("Input CM mode", input_cm_mode),

	SOC_ENUM("Output CM mode", output_cm_mode),

	SOC_SINGLE_EXT("FIRMWARE SET MODE", SND_SOC_NOPM, 0, 0xffff, 0,
			aic3262_set_mode_get, aic3262_set_mode_put),
};

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *	It is SoC Codec DAI structure which has DAI capabilities viz.,
 *	playback and capture, DAI runtime information viz. state of DAI
 *			and pop wait state, and DAI private data.
 *	The AIC3262 rates ranges from 8k to 192k
 *	The PCM bit format supported are 16, 20, 24 and 32 bits
 *----------------------------------------------------------------------------
 */
struct snd_soc_dai_ops aic3262_asi1_dai_ops = {
	.hw_params = aic3262_hw_params,
	.digital_mute = aic3262_mute,
	.set_fmt = aic3262_set_dai_fmt,
	.set_pll = aic3262_dai_set_pll,
};

struct snd_soc_dai_ops aic3262_asi2_dai_ops = {
	.hw_params = aic3262_hw_params,
	.digital_mute = aic3262_mute,
	.set_fmt = aic3262_set_dai_fmt,
	.set_pll = aic3262_dai_set_pll,
};

struct snd_soc_dai_ops aic3262_asi3_dai_ops = {
	.hw_params = aic3262_hw_params,
	.digital_mute = aic3262_mute,
	.set_fmt = aic3262_set_dai_fmt,
	.set_pll = aic3262_dai_set_pll,
};

struct snd_soc_dai_driver aic326x_dai_driver[] = {
	{
	 .name = "aic326x-asi1",
	 .playback = {
		      .stream_name = "ASI1 Playback",
		      .channels_min = 1,
		      .channels_max = 8,
		      .rates = AIC3262_RATES,
		      .formats = AIC3262_FORMATS,
		      },
	 .capture = {
		     .stream_name = "ASI1 Capture",
		     .channels_min = 1,
		     .channels_max = 8,
		     .rates = AIC3262_RATES,
		     .formats = AIC3262_FORMATS,
		     },
	 .ops = &aic3262_asi1_dai_ops,
	 },
	{
	 .name = "aic326x-asi2",
	 .playback = {
		      .stream_name = "ASI2 Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = AIC3262_RATES,
		      .formats = AIC3262_FORMATS,
		      },
	 .capture = {
		     .stream_name = "ASI2 Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = AIC3262_RATES,
		     .formats = AIC3262_FORMATS,
		     },
	 .ops = &aic3262_asi2_dai_ops,
	 },
	{
	 .name = "aic326x-asi3",
	 .playback = {
		      .stream_name = "ASI3 Playback",
		      .channels_min = 1,
		      .channels_max = 2,
		      .rates = AIC3262_RATES,
		      .formats = AIC3262_FORMATS,
		      },
	 .capture = {
		     .stream_name = "ASI3 Capture",
		     .channels_min = 1,
		     .channels_max = 2,
		     .rates = AIC3262_RATES,
		     .formats = AIC3262_FORMATS,
		     },
	 .ops = &aic3262_asi3_dai_ops,
	 },

};

static const unsigned int adc_ma_tlv[] = {
	TLV_DB_RANGE_HEAD(7),
	1, 1, TLV_DB_SCALE_ITEM(-3610, 0, 0),
	2, 2, TLV_DB_SCALE_ITEM(-3010, 0, 0),
	3, 3, TLV_DB_SCALE_ITEM(-2660, 0, 0),
	4, 4, TLV_DB_SCALE_ITEM(-2410, 0, 0),
	5, 7, TLV_DB_SCALE_ITEM(-2210, 1500, 0),
	8, 11, TLV_DB_SCALE_ITEM(-1810, 1000, 0),
	12, 41 , TLV_DB_SCALE_ITEM(-1450, 500, 0)
};

static const DECLARE_TLV_DB_SCALE(lo_hp_tlv, -7830, 50, 0);
static const struct snd_kcontrol_new mal_pga_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN1 Left Capture Switch", AIC3262_MA_CNTL, 5, 1, 0),
	SOC_DAPM_SINGLE_TLV("Left MicPGA Volume", AIC3262_LADC_PGA_MAL_VOL,
				0, 0x3f, 1, adc_ma_tlv),
};

static const struct snd_kcontrol_new mar_pga_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN1 Right Capture Switch", AIC3262_MA_CNTL, 4, 1, 0),
	SOC_DAPM_SINGLE_TLV("Right MicPGA Volume", AIC3262_RADC_PGA_MAR_VOL,
				0, 0x3f, 1, adc_ma_tlv),
};

/* Left HPL Mixer */
static const struct snd_kcontrol_new hpl_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("MA Left Playback Switch", AIC3262_HP_AMP_CNTL_R1, 7, 1,
			0),
	SOC_DAPM_SINGLE("Left DAC Playback Switch", AIC3262_HP_AMP_CNTL_R1,
			5, 1, 0),
	SOC_DAPM_SINGLE_TLV("LO Left-B1 Playback Volume",
				AIC3262_HP_AMP_CNTL_R2, 0, 0x7f, 1, lo_hp_tlv),
};

/* Right HPR Mixer */
static const struct snd_kcontrol_new hpr_output_mixer_controls[] = {
	SOC_DAPM_SINGLE_TLV("LO Right-B1 Playback Volume",
				AIC3262_HP_AMP_CNTL_R3, 0, 0x7f, 1, lo_hp_tlv),
	SOC_DAPM_SINGLE("Left DAC Playback Switch", AIC3262_HP_AMP_CNTL_R1,
			2, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Playback Switch", AIC3262_HP_AMP_CNTL_R1,
			4, 1, 0),
	SOC_DAPM_SINGLE("MA Right Playback Switch", AIC3262_HP_AMP_CNTL_R1,
			6, 1, 0),
};

/* Left LOL Mixer */
static const struct snd_kcontrol_new lol_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("MA Left Playback Switch", AIC3262_LINE_AMP_CNTL_R2,
			7, 1, 0),
	SOC_DAPM_SINGLE("IN1 Left-B Capture Switch", AIC3262_LINE_AMP_CNTL_R2,
			3, 1, 0),
	SOC_DAPM_SINGLE("Left DAC Playback Switch", AIC3262_LINE_AMP_CNTL_R1,
			7, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Playback Switch", AIC3262_LINE_AMP_CNTL_R1,
			5, 1, 0),
};

/* Right LOR Mixer */
static const struct snd_kcontrol_new lor_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("LO Left Playback Switch", AIC3262_LINE_AMP_CNTL_R1,
			2, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Playback Switch", AIC3262_LINE_AMP_CNTL_R1,
			6, 1, 0),
	SOC_DAPM_SINGLE("MA Right Playback Switch", AIC3262_LINE_AMP_CNTL_R2,
			6, 1, 0),
	SOC_DAPM_SINGLE("IN1 Right-B Capture Switch", AIC3262_LINE_AMP_CNTL_R2,
			0, 1, 0),
};

/* Left SPKL Mixer */
static const struct snd_kcontrol_new spkl_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("MA Left Playback Switch", AIC3262_SPK_AMP_CNTL_R1,
			7, 1, 0),
	SOC_DAPM_SINGLE_TLV("LO Left Playback Volume",
				AIC3262_SPK_AMP_CNTL_R2, 0, 0x7f, 1, lo_hp_tlv),
	SOC_DAPM_SINGLE("SPR_IN Switch", AIC3262_SPK_AMP_CNTL_R1, 2, 1, 0),
};

/* Right SPKR Mixer */
static const struct snd_kcontrol_new spkr_output_mixer_controls[] = {
	SOC_DAPM_SINGLE_TLV("LO Right Playback Volume",
				AIC3262_SPK_AMP_CNTL_R3, 0, 0x7f, 1, lo_hp_tlv),
	SOC_DAPM_SINGLE("MA Right Playback Switch",
			AIC3262_SPK_AMP_CNTL_R1, 6, 1, 0),
};

/* REC Mixer */
static const struct snd_kcontrol_new rec_output_mixer_controls[] = {
	SOC_DAPM_SINGLE_TLV("LO Left-B2 Playback Volume",
				AIC3262_RAMP_CNTL_R1, 0, 0x7f, 1, lo_hp_tlv),
	SOC_DAPM_SINGLE_TLV("IN1 Left Capture Volume",
				AIC3262_IN1L_SEL_RM, 0, 0x7f, 1, lo_hp_tlv),
	SOC_DAPM_SINGLE_TLV("IN1 Right Capture Volume",
				AIC3262_IN1R_SEL_RM, 0, 0x7f, 1, lo_hp_tlv),
	SOC_DAPM_SINGLE_TLV("LO Right-B2 Playback Volume",
				AIC3262_RAMP_CNTL_R2, 0, 0x7f, 1, lo_hp_tlv),
};

/* Left Input Mixer */
static const struct snd_kcontrol_new left_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN1 Left Capture Switch", AIC3262_LMIC_PGA_PIN,
			6, 3, 0),
	SOC_DAPM_SINGLE("IN2 Left Capture Switch", AIC3262_LMIC_PGA_PIN,
			4, 3, 0),
	SOC_DAPM_SINGLE("IN3 Left Capture Switch", AIC3262_LMIC_PGA_PIN,
			2, 3, 0),
	SOC_DAPM_SINGLE("IN4 Left Capture Switch", AIC3262_LMIC_PGA_PM_IN4,
			5, 1, 0),
	SOC_DAPM_SINGLE("IN1 Right Capture Switch", AIC3262_LMIC_PGA_PIN,
			0, 3, 0),
	SOC_DAPM_SINGLE("IN2 Right Capture Switch", AIC3262_LMIC_PGA_MIN,
			4, 3, 0),
	SOC_DAPM_SINGLE("IN3 Right Capture Switch", AIC3262_LMIC_PGA_MIN,
			2, 3, 0),
	SOC_DAPM_SINGLE("IN4 Right Capture Switch", AIC3262_LMIC_PGA_PM_IN4,
			4, 1, 0),
	SOC_DAPM_SINGLE("CM2 Left Capture Switch", AIC3262_LMIC_PGA_MIN,
			0, 3, 0),
	SOC_DAPM_SINGLE("CM1 Left Capture Switch", AIC3262_LMIC_PGA_MIN,
			6, 3, 0),
};

/* Right Input Mixer */
static const struct snd_kcontrol_new right_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("IN1 Right Capture Switch", AIC3262_RMIC_PGA_PIN,
			6, 3, 0),
	SOC_DAPM_SINGLE("IN2 Right Capture Switch", AIC3262_RMIC_PGA_PIN,
			4, 3, 0),
	SOC_DAPM_SINGLE("IN3 Right Capture Switch", AIC3262_RMIC_PGA_PIN,
			2, 3, 0),
	SOC_DAPM_SINGLE("IN4 Right Capture Switch", AIC3262_RMIC_PGA_PM_IN4,
			5, 1, 0),
	SOC_DAPM_SINGLE("IN2 Left Capture Switch", AIC3262_RMIC_PGA_PIN,
			0, 3, 0),
	SOC_DAPM_SINGLE("IN1 Left Capture Switch", AIC3262_RMIC_PGA_MIN,
			4, 3, 0),
	SOC_DAPM_SINGLE("IN3 Left Capture Switch", AIC3262_RMIC_PGA_MIN,
			2, 3, 0),
	SOC_DAPM_SINGLE("IN4 Left Capture Switch", AIC3262_RMIC_PGA_PM_IN4,
			4, 1, 0),
	SOC_DAPM_SINGLE("CM1 Right Capture Switch", AIC3262_RMIC_PGA_MIN,
			6, 3, 0),
	SOC_DAPM_SINGLE("CM2 Right Capture Switch", AIC3262_RMIC_PGA_MIN,
			0, 3, 0),
};

static const char * const asi1lin_text[] = {
	"Off", "ASI1 Left In", "ASI1 Right In", "ASI1 MonoMix In"
};

SOC_ENUM_SINGLE_DECL(asi1lin_enum, AIC3262_ASI1_DAC_OUT_CNTL, 6, asi1lin_text);

static const struct snd_kcontrol_new asi1lin_control =
SOC_DAPM_ENUM("ASI1LIN Route", asi1lin_enum);

static const char * const asi1rin_text[] = {
	"Off", "ASI1 Right In", "ASI1 Left In", "ASI1 MonoMix In"
};

SOC_ENUM_SINGLE_DECL(asi1rin_enum, AIC3262_ASI1_DAC_OUT_CNTL, 4, asi1rin_text);

static const struct snd_kcontrol_new asi1rin_control =
SOC_DAPM_ENUM("ASI1RIN Route", asi1rin_enum);

static const char * const asi2lin_text[] = {
	"Off", "ASI2 Left In", "ASI2 Right In", "ASI2 MonoMix In"
};

SOC_ENUM_SINGLE_DECL(asi2lin_enum, AIC3262_ASI2_DAC_OUT_CNTL, 6, asi2lin_text);

static const struct snd_kcontrol_new asi2lin_control =
SOC_DAPM_ENUM("ASI2LIN Route", asi2lin_enum);

static const char * const asi2rin_text[] = {
	"Off", "ASI2 Right In", "ASI2 Left In", "ASI2 MonoMix In"
};

SOC_ENUM_SINGLE_DECL(asi2rin_enum, AIC3262_ASI2_DAC_OUT_CNTL, 4, asi2rin_text);

static const struct snd_kcontrol_new asi2rin_control =
SOC_DAPM_ENUM("ASI2RIN Route", asi2rin_enum);

static const char * const asi3lin_text[] = {
	"Off", "ASI3 Left In", "ASI3 Right In", "ASI3 MonoMix In"
};

SOC_ENUM_SINGLE_DECL(asi3lin_enum, AIC3262_ASI3_DAC_OUT_CNTL, 6, asi3lin_text);

static const struct snd_kcontrol_new asi3lin_control =
SOC_DAPM_ENUM("ASI3LIN Route", asi3lin_enum);

static const char * const asi3rin_text[] = {
	"Off", "ASI3 Right In", "ASI3 Left In", "ASI3 MonoMix In"
};

SOC_ENUM_SINGLE_DECL(asi3rin_enum, AIC3262_ASI3_DAC_OUT_CNTL, 4, asi3rin_text);

static const struct snd_kcontrol_new asi3rin_control =
SOC_DAPM_ENUM("ASI3RIN Route", asi3rin_enum);

static const char * const dacminidspin1_text[] = {
	"ASI1 In", "ASI2 In", "ASI3 In", "ADC MiniDSP Out"
};

SOC_ENUM_SINGLE_DECL(dacminidspin1_enum, AIC3262_MINIDSP_DATA_PORT_CNTL, 4,
		     dacminidspin1_text);

static const struct snd_kcontrol_new dacminidspin1_control =
SOC_DAPM_ENUM("DAC MiniDSP IN1 Route", dacminidspin1_enum);

static const char * const dacminidspin2_text[] = {
	"ASI1 In", "ASI2 In", "ASI3 In"
};

SOC_ENUM_SINGLE_DECL(dacminidspin2_enum, AIC3262_MINIDSP_DATA_PORT_CNTL, 2,
		     dacminidspin2_text);

static const struct snd_kcontrol_new dacminidspin2_control =
SOC_DAPM_ENUM("DAC MiniDSP IN2 Route", dacminidspin2_enum);

static const char * const dacminidspin3_text[] = {
	"ASI1 In", "ASI2 In", "ASI3 In"
};

SOC_ENUM_SINGLE_DECL(dacminidspin3_enum, AIC3262_MINIDSP_DATA_PORT_CNTL, 0,
		     dacminidspin3_text);

static const struct snd_kcontrol_new dacminidspin3_control =
SOC_DAPM_ENUM("DAC MiniDSP IN3 Route", dacminidspin3_enum);

static const char * const adcdac_route_text[] = {
	"Off",
	"On",
};

SOC_ENUM_SINGLE_DECL(adcdac_enum, 0, 2, adcdac_route_text);

static const struct snd_kcontrol_new adcdacroute_control =
SOC_DAPM_ENUM_VIRT("ADC DAC Route", adcdac_enum);

static const char * const dout1_text[] = {
	"ASI1 Out",
	"DIN1 Bypass",
	"DIN2 Bypass",
	"DIN3 Bypass",
};

SOC_ENUM_SINGLE_DECL(dout1_enum, AIC3262_ASI1_DOUT_CNTL, 0, dout1_text);
static const struct snd_kcontrol_new dout1_control =
SOC_DAPM_ENUM("DOUT1 Route", dout1_enum);

static const char * const dout2_text[] = {
	"ASI2 Out",
	"DIN1 Bypass",
	"DIN2 Bypass",
	"DIN3 Bypass",
};

SOC_ENUM_SINGLE_DECL(dout2_enum, AIC3262_ASI2_DOUT_CNTL, 0, dout2_text);
static const struct snd_kcontrol_new dout2_control =
SOC_DAPM_ENUM("DOUT2 Route", dout2_enum);

static const char * const dout3_text[] = {
	"ASI3 Out",
	"DIN1 Bypass",
	"DIN2 Bypass",
	"DIN3 Bypass",
};

SOC_ENUM_SINGLE_DECL(dout3_enum, AIC3262_ASI3_DOUT_CNTL, 0, dout3_text);
static const struct snd_kcontrol_new dout3_control =
SOC_DAPM_ENUM("DOUT3 Route", dout3_enum);

static const char * const asi1out_text[] = {
	"Off",
	"ADC MiniDSP Out1",
	"ASI1In Bypass",
	"ASI2In Bypass",
	"ASI3In Bypass",
};

SOC_ENUM_SINGLE_DECL(asi1out_enum, AIC3262_ASI1_ADC_INPUT_CNTL,
		     0, asi1out_text);
static const struct snd_kcontrol_new asi1out_control =
SOC_DAPM_ENUM("ASI1OUT Route", asi1out_enum);

static const char * const asi2out_text[] = {
	"Off",
	"ADC MiniDSP Out1",
	"ASI1In Bypass",
	"ASI2In Bypass",
	"ASI3In Bypass",
	"ADC MiniDSP Out2",
};

SOC_ENUM_SINGLE_DECL(asi2out_enum, AIC3262_ASI2_ADC_INPUT_CNTL,
		     0, asi2out_text);
static const struct snd_kcontrol_new asi2out_control =
SOC_DAPM_ENUM("ASI2OUT Route", asi2out_enum);
static const char * const asi3out_text[] = {
	"Off",
	"ADC MiniDSP Out1",
	"ASI1In Bypass",
	"ASI2In Bypass",
	"ASI3In Bypass",
	"Reserved",
	"ADC MiniDSP Out3",
};

SOC_ENUM_SINGLE_DECL(asi3out_enum, AIC3262_ASI3_ADC_INPUT_CNTL,
		     0, asi3out_text);
static const struct snd_kcontrol_new asi3out_control =
SOC_DAPM_ENUM("ASI3OUT Route", asi3out_enum);
static const char * const asibclk_text[] = {
	"DAC_CLK",
	"DAC_MOD_CLK",
	"ADC_CLK",
	"ADC_MOD_CLK",
};

SOC_ENUM_SINGLE_DECL(asi1bclk_enum, AIC3262_ASI1_BCLK_N_CNTL, 0, asibclk_text);
static const struct snd_kcontrol_new asi1bclk_control =
SOC_DAPM_ENUM("ASI1_BCLK Route", asi1bclk_enum);

SOC_ENUM_SINGLE_DECL(asi2bclk_enum, AIC3262_ASI2_BCLK_N_CNTL, 0, asibclk_text);
static const struct snd_kcontrol_new asi2bclk_control =
SOC_DAPM_ENUM("ASI2_BCLK Route", asi2bclk_enum);
SOC_ENUM_SINGLE_DECL(asi3bclk_enum, AIC3262_ASI3_BCLK_N_CNTL, 0, asibclk_text);
static const struct snd_kcontrol_new asi3bclk_control =
SOC_DAPM_ENUM("ASI3_BCLK Route", asi3bclk_enum);

static const char * const adc_mux_text[] = {
	"Analog",
	"Digital",
};

SOC_ENUM_SINGLE_DECL(adcl_enum, AIC3262_ADC_CHANNEL_POW, 4, adc_mux_text);
SOC_ENUM_SINGLE_DECL(adcr_enum, AIC3262_ADC_CHANNEL_POW, 2, adc_mux_text);

static const struct snd_kcontrol_new adcl_mux =
SOC_DAPM_ENUM("Left ADC Route", adcl_enum);

static const struct snd_kcontrol_new adcr_mux =
SOC_DAPM_ENUM("Right ADC Route", adcr_enum);

/**
 * aic326x_hp_event: - To handle headphone related task before and after
 *			headphone powrup and power down
 * @w: pointer variable to dapm_widget
 * @kcontrol: mixer control
 * @event: event element information
 *
 * Returns 0 for success.
 */
static int aic326x_hp_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	int reg_mask = 0;
	int mute_reg = 0;
	int ret_wbits = 0;
	u8 hpl_hpr;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(w->codec);
	mutex_lock(&aic3262->mutex);
	CHECK_AIC326x_I2C_SHUTDOWN(aic3262, w->codec)

	if (w->shift == 1) {
		reg_mask = AIC3262_HPL_POWER_STATUS_MASK;
		mute_reg = AIC3262_HPL_VOL;
	}
	if (w->shift == 0) {
		reg_mask = AIC3262_HPR_POWER_STATUS_MASK;
		mute_reg = AIC3262_HPR_VOL;
	}
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
			snd_soc_update_bits(w->codec, AIC3262_CHARGE_PUMP_CNTL,
			AIC3262_DYNAMIC_OFFSET_CALIB_MASK,
			AIC3262_DYNAMIC_OFFSET_CALIB);
			snd_soc_write(w->codec, mute_reg, 0x80);
			snd_soc_update_bits(w->codec, AIC3262_HP_CTL,
			AIC3262_HP_STAGE_MASK ,
			AIC3262_HP_STAGE_25 << AIC3262_HP_STAGE_SHIFT);
		break;

	case SND_SOC_DAPM_POST_PMU:
		ret_wbits = aic3xxx_wait_bits(w->codec->control_data,
					      AIC3262_HP_FLAG, reg_mask,
					      reg_mask, AIC326X_TIME_DELAY,
					      AIC326X_DELAY_COUNTER);
		if (!ret_wbits) {
			dev_err(w->codec->dev, "HP POST_PMU timedout\n");
			mutex_unlock(&aic3262->mutex);
			return -1;
		}
		snd_soc_update_bits(w->codec, AIC3262_HP_CTL,
			AIC3262_HP_STAGE_MASK ,
			AIC3262_HP_STAGE_100 << AIC3262_HP_STAGE_SHIFT);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(w->codec, AIC3262_HP_CTL,
				AIC3262_HP_STAGE_MASK ,
				AIC3262_HP_STAGE_25 << AIC3262_HP_STAGE_SHIFT);
		hpl_hpr = snd_soc_read(w->codec, AIC3262_HP_AMP_CNTL_R1);
		if ((hpl_hpr & 0x3) == 0x3) {
			snd_soc_update_bits(w->codec, AIC3262_HP_AMP_CNTL_R1,
						AIC3262_HPL_POWER_MASK, 0x0);
			mdelay(1);
			snd_soc_update_bits(w->codec, AIC3262_HP_AMP_CNTL_R1,
						AIC3262_HPR_POWER_MASK, 0x0);
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
		ret_wbits = aic3xxx_wait_bits(w->codec->control_data,
					      AIC3262_HP_FLAG, reg_mask, 0,
					      AIC326X_TIME_DELAY,
						AIC326X_DELAY_COUNTER);
		if (!ret_wbits) {
			dev_err(w->codec->dev, "HP POST_PMD timedout\n");
			mutex_unlock(&aic3262->mutex);
			return -1;
		}
		snd_soc_write(w->codec, mute_reg, 0xb9);
		snd_soc_write(w->codec, AIC3262_POWER_CONF,
				snd_soc_read(w->codec, AIC3262_POWER_CONF));
		break;
	default:
		BUG();
		mutex_unlock(&aic3262->mutex);
		return -EINVAL;
	}
	mutex_unlock(&aic3262->mutex);
	return 0;
}

/**
 *aic326x_dac_event: Headset popup reduction and powering up dsps together
 *			when they are in sync mode
 * @w: pointer variable to dapm_widget
 * @kcontrol: pointer to sound control
 * @event: event element information
 *
 * Returns 0 for success.
 */
static int aic326x_dac_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	int reg_mask = 0;
	int ret_wbits = 0;
	int run_state_mask;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(w->codec);
	int sync_needed = 0, non_sync_state = 0;
	int other_dsp = 0, run_state = 0;

	if (w->shift == 7) {
		reg_mask = AIC3262_LDAC_POWER_STATUS_MASK;
		run_state_mask = AIC3XXX_COPS_MDSP_D_L;
	}
	if (w->shift == 6) {
		reg_mask = AIC3262_RDAC_POWER_STATUS_MASK;
		run_state_mask = AIC3XXX_COPS_MDSP_D_R;
	}
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:

		ret_wbits = aic3xxx_wait_bits(w->codec->control_data,
					      AIC3262_DAC_FLAG, reg_mask,
					      reg_mask, AIC326X_TIME_DELAY,
					      AIC326X_DELAY_COUNTER);

		sync_needed = aic3xxx_reg_read(w->codec->control_data,
							AIC3262_DAC_PRB);
		non_sync_state = dsp_non_sync_mode(aic3262->dsp_runstate);
		other_dsp = aic3262->dsp_runstate & AIC3XXX_COPS_MDSP_A;

		if (sync_needed && non_sync_state && other_dsp) {
			run_state = aic3262_get_runstate(
						aic3262->codec);
			aic3262_dsp_pwrdwn_status(aic3262->codec);
			aic3262_dsp_pwrup(aic3262->codec, run_state);
		}
		aic3262->dsp_runstate |= run_state_mask;

		if (!ret_wbits) {
			dev_err(w->codec->dev, "DAC POST_PMU timedout\n");
			return -1;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:

		ret_wbits = aic3xxx_wait_bits(w->codec->control_data,
					      AIC3262_DAC_FLAG, reg_mask, 0,
					      AIC326X_TIME_DELAY,
						AIC326X_DELAY_COUNTER);

		aic3262->dsp_runstate = (aic3262->dsp_runstate &
					 ~run_state_mask);
		if (!ret_wbits) {
			dev_err(w->codec->dev, "DAC POST_PMD timedout\n");
			return -1;
		}
		break;
	default:
		BUG();
		return -EINVAL;
	}
	return 0;
}

/**
 * aic326x_spk_event: Speaker related task before and after
 *			 headphone powrup and power down$
 * @w: pointer variable to dapm_widget,
 * @kcontrolr: pointer variable to sound control,
 * @event:	integer to event,
 *
 * Return value: 0 for success
 */
static int aic326x_spk_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	int reg_mask;

	if (w->shift == 1)
		reg_mask = AIC3262_SPKL_POWER_STATUS_MASK;
	if (w->shift == 0)
		reg_mask = AIC3262_SPKR_POWER_STATUS_MASK;
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		mdelay(1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		mdelay(1);
		break;
	default:
		BUG();
		return -EINVAL;
	}
	return 0;
}

/**$
 * pll_power_on_event: provide delay after widget  power up
 * @w:  pointer variable to dapm_widget,
 * @kcontrolr: pointer variable to sound control,
 * @event:	integer to event,
 *
 * Return value: 0 for success
 */
static int pll_power_on_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	if (event == SND_SOC_DAPM_POST_PMU)
		mdelay(10);
	return 0;
}

/**
 * aic3262_set_mode_get: To get different mode of Firmware through tinymix
 * @kcontrolr: pointer to sound control,
 * ucontrol: pointer to control element value,
 *
 * Return value: 0 for success
 */
static int aic3262_set_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct aic3262_priv *priv_ds = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ((priv_ds->cfw_p->cur_mode << 8)
					    | priv_ds->cfw_p->cur_cfg);

	return 0;
}

/**
 * aic3262_set_mode_put: To set different mode of Firmware through tinymix
 * @kcontrolr: pointer to sound control,
 * ucontrol: pointer to control element value,
 *
 * Return value: 0 for success
 */
static int aic3262_set_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct aic3262_priv *priv_ds = snd_soc_codec_get_drvdata(codec);

	int next_mode = 0, next_cfg = 0;
	int ret = 0;

	next_mode = (ucontrol->value.integer.value[0] >> 8);
	next_cfg = (ucontrol->value.integer.value[0]) & 0xFF;
	if (priv_ds == NULL)
		dev_err(codec->dev, "failed to load firmware\n");
	else
		ret = aic3xxx_cfw_setmode_cfg(priv_ds->cfw_p,
					      next_mode, next_cfg);
	return ret;
}

/**
 * aic326x_adc_dsp_event: To get DSP run state to perform synchronization
 * @w: pointer variable to dapm_widget
 * @kcontrol: pointer to sound control
 * @event: event element information
 *
 * Returns 0 for success.
 */
static int aic326x_adc_dsp_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	int run_state = 0;
	int non_sync_state = 0, sync_needed = 0;
	int other_dsp = 0;
	int run_state_mask = 0;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(w->codec);
	int reg_mask = 0;
	int ret_wbits = 0;

	if (w->shift == 7) {
		reg_mask = AIC3262_LADC_POWER_MASK;
		run_state_mask = AIC3XXX_COPS_MDSP_A_L;
	}
	if (w->shift == 6) {
		reg_mask = AIC3262_RADC_POWER_MASK;
		run_state_mask = AIC3XXX_COPS_MDSP_A_R;
	}
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret_wbits = aic3xxx_wait_bits(w->codec->control_data,
					      AIC3262_ADC_FLAG, reg_mask,
					      reg_mask, AIC326X_TIME_DELAY,
					      AIC326X_DELAY_COUNTER);
		sync_needed =  aic3xxx_reg_read(w->codec->control_data,
							AIC3262_DAC_PRB);
		non_sync_state = dsp_non_sync_mode(aic3262->dsp_runstate);
		other_dsp = aic3262->dsp_runstate & AIC3XXX_COPS_MDSP_D;
		if (sync_needed && non_sync_state && other_dsp) {
			run_state = aic3262_get_runstate(
						aic3262->codec);
			aic3262_dsp_pwrdwn_status(aic3262->codec);
			aic3262_dsp_pwrup(aic3262->codec, run_state);
		}
		aic3262->dsp_runstate |= run_state_mask;
		if (!ret_wbits) {
			dev_err(w->codec->dev, "ADC POST_PMU timedout\n");
			return -1;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret_wbits = aic3xxx_wait_bits(w->codec->control_data,
					      AIC3262_ADC_FLAG, reg_mask, 0,
					      AIC326X_TIME_DELAY,
						AIC326X_DELAY_COUNTER);
		aic3262->dsp_runstate = (aic3262->dsp_runstate &
					 ~run_state_mask);
		if (!ret_wbits) {
			dev_err(w->codec->dev, "ADC POST_PMD timedout\n");
			return -1;
		}
		break;
	default:
		BUG();
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dapm_widget aic3262_dapm_widgets[] = {
	/* TODO: Can we switch these off ? */
	SND_SOC_DAPM_AIF_IN("DIN1", "ASI1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("DIN2", "ASI2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("DIN3", "ASI3 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_DAC_E("Left DAC", NULL, AIC3262_PASI_DAC_DP_SETUP, 7, 0,
			   aic326x_dac_event, SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("Right DAC", NULL, AIC3262_PASI_DAC_DP_SETUP, 6, 0,
			   aic326x_dac_event, SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_POST_PMD),

	/* dapm widget (path domain) for HPL Output Mixer */
	SND_SOC_DAPM_MIXER("HP Left Mixer", SND_SOC_NOPM, 0, 0,
			   &hpl_output_mixer_controls[0],
			   ARRAY_SIZE(hpl_output_mixer_controls)),

	/* dapm widget (path domain) for HPR Output Mixer */
	SND_SOC_DAPM_MIXER("HP Right Mixer", SND_SOC_NOPM, 0, 0,
			   &hpr_output_mixer_controls[0],
			   ARRAY_SIZE(hpr_output_mixer_controls)),

	SND_SOC_DAPM_PGA_S("HP Left Playback Driver", 3,
		AIC3262_HP_AMP_CNTL_R1, 1, 0, aic326x_hp_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("HP Right Playback Driver", 3,
		AIC3262_HP_AMP_CNTL_R1, 0, 0, aic326x_hp_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	/* dapm widget (path domain) for LOL Output Mixer */
	SND_SOC_DAPM_MIXER("LO Left Mixer", SND_SOC_NOPM, 0, 0,
			   &lol_output_mixer_controls[0],
			   ARRAY_SIZE(lol_output_mixer_controls)),

	/* dapm widget (path domain) for LOR Output Mixer mixer */
	SND_SOC_DAPM_MIXER("LO Right Mixer", SND_SOC_NOPM, 0, 0,
			   &lor_output_mixer_controls[0],
			   ARRAY_SIZE(lor_output_mixer_controls)),

	SND_SOC_DAPM_PGA_S("LO Left Playback Driver", 2,
			AIC3262_LINE_AMP_CNTL_R1, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("LO Right Playback Driver", 2,
			AIC3262_LINE_AMP_CNTL_R1, 0, 0, NULL, 0),

	/* dapm widget (path domain) for SPKL Output Mixer */
	SND_SOC_DAPM_MIXER("SPK Left Mixer", SND_SOC_NOPM, 0, 0,
			   &spkl_output_mixer_controls[0],
			   ARRAY_SIZE(spkl_output_mixer_controls)),

	/* dapm widget (path domain) for SPKR Output Mixer */
	SND_SOC_DAPM_MIXER("SPK Right Mixer", SND_SOC_NOPM, 0, 0,
			   &spkr_output_mixer_controls[0],
			   ARRAY_SIZE(spkr_output_mixer_controls)),

	SND_SOC_DAPM_PGA_S("SPK Left Playback Driver", 3,
			AIC3262_SPK_AMP_CNTL_R1, 1, 0, aic326x_spk_event,
			SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_S("SPK Right Playback Driver", 3,
			AIC3262_SPK_AMP_CNTL_R1, 0, 0, aic326x_spk_event,
			SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_POST_PMU),

	/* dapm widget (path domain) for SPKR Output Mixer */
	SND_SOC_DAPM_MIXER("REC Mixer", SND_SOC_NOPM, 0, 0,
			   &rec_output_mixer_controls[0],
			   ARRAY_SIZE(rec_output_mixer_controls)),

	SND_SOC_DAPM_PGA_S("RECP Playback Driver", 3, AIC3262_REC_AMP_CNTL_R5,
			 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("RECM Playback Driver", 3, AIC3262_REC_AMP_CNTL_R5,
			 6, 0, NULL, 0),

	SND_SOC_DAPM_MUX("ASI1LIN Route",
			 SND_SOC_NOPM, 0, 0, &asi1lin_control),
	SND_SOC_DAPM_MUX("ASI1RIN Route",
			 SND_SOC_NOPM, 0, 0, &asi1rin_control),
	SND_SOC_DAPM_MUX("ASI2LIN Route",
			 SND_SOC_NOPM, 0, 0, &asi2lin_control),
	SND_SOC_DAPM_MUX("ASI2RIN Route",
			 SND_SOC_NOPM, 0, 0, &asi2rin_control),
	SND_SOC_DAPM_MUX("ASI3LIN Route",
			 SND_SOC_NOPM, 0, 0, &asi3lin_control),
	SND_SOC_DAPM_MUX("ASI3RIN Route",
			 SND_SOC_NOPM, 0, 0, &asi3rin_control),

	SND_SOC_DAPM_PGA("ASI1LIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI1RIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI2LIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI2RIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI3LIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI3RIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI1MonoMixIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI2MonoMixIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI3MonoMixIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	/* TODO: Can we switch the ASIxIN off? */
	SND_SOC_DAPM_PGA("ASI1IN Port", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI2IN Port", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI3IN Port", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("DAC MiniDSP IN1 Route",
			 SND_SOC_NOPM, 0, 0, &dacminidspin1_control),
	SND_SOC_DAPM_MUX("DAC MiniDSP IN2 Route",
			 SND_SOC_NOPM, 0, 0, &dacminidspin2_control),
	SND_SOC_DAPM_MUX("DAC MiniDSP IN3 Route",
			 SND_SOC_NOPM, 0, 0, &dacminidspin3_control),

	SND_SOC_DAPM_VIRT_MUX("ADC DAC Route",
			      SND_SOC_NOPM, 0, 0, &adcdacroute_control),

	SND_SOC_DAPM_PGA("CM", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("CM1 Left Capture", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("CM2 Left Capture", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("CM1 Right Capture", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("CM2 Right Capture", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* TODO: Can we switch these off ? */
	SND_SOC_DAPM_AIF_OUT("DOUT1", "ASI1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("DOUT2", "ASI2 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("DOUT3", "ASI3 Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MUX("DOUT1 Route",
			 SND_SOC_NOPM, 0, 0, &dout1_control),
	SND_SOC_DAPM_MUX("DOUT2 Route",
			 SND_SOC_NOPM, 0, 0, &dout2_control),
	SND_SOC_DAPM_MUX("DOUT3 Route",
			 SND_SOC_NOPM, 0, 0, &dout3_control),

	SND_SOC_DAPM_PGA("ASI1OUT", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI2OUT", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI3OUT", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("ASI1OUT Route",
			 SND_SOC_NOPM, 0, 0, &asi1out_control),
	SND_SOC_DAPM_MUX("ASI2OUT Route",
			 SND_SOC_NOPM, 0, 0, &asi2out_control),
	SND_SOC_DAPM_MUX("ASI3OUT Route",
			 SND_SOC_NOPM, 0, 0, &asi3out_control),

	/* TODO: Can we switch the ASI1 OUT1 off? */
	/* TODO: Can we switch them off? */
	SND_SOC_DAPM_PGA("ADC MiniDSP OUT1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ADC MiniDSP OUT2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ADC MiniDSP OUT3", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("Left ADC Route", SND_SOC_NOPM, 0, 0, &adcl_mux),
	SND_SOC_DAPM_MUX("Right ADC Route", SND_SOC_NOPM, 0, 0, &adcr_mux),

	SND_SOC_DAPM_ADC_E("Left ADC", NULL, AIC3262_ADC_CHANNEL_POW, 7, 0,
			   aic326x_adc_dsp_event, SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("Right ADC", NULL, AIC3262_ADC_CHANNEL_POW, 6, 0,
			   aic326x_adc_dsp_event, SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_S("Left MicPGA", 0, AIC3262_MICL_PGA, 7, 1, NULL, 0),
	SND_SOC_DAPM_PGA_S("Right MicPGA", 0, AIC3262_MICR_PGA, 7, 1, NULL, 0),

	SND_SOC_DAPM_PGA_S("MA Left Playback PGA", 1, AIC3262_MA_CNTL,
			 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("MA Right Playback PGA", 1, AIC3262_MA_CNTL,
			 2, 0, NULL, 0),

	/* dapm widget for MAL PGA Mixer */
	SND_SOC_DAPM_MIXER("MA Left PGA Mixer", SND_SOC_NOPM, 0, 0,
			   &mal_pga_mixer_controls[0],
			   ARRAY_SIZE(mal_pga_mixer_controls)),

	/* dapm widget for MAR PGA Mixer */
	SND_SOC_DAPM_MIXER("MA Right PGA Mixer", SND_SOC_NOPM, 0, 0,
			   &mar_pga_mixer_controls[0],
			   ARRAY_SIZE(mar_pga_mixer_controls)),

	/* dapm widget for Left Input Mixer */
	SND_SOC_DAPM_MIXER("Left Input Mixer", SND_SOC_NOPM, 0, 0,
			   &left_input_mixer_controls[0],
			   ARRAY_SIZE(left_input_mixer_controls)),

	/* dapm widget for Right Input Mixer */
	SND_SOC_DAPM_MIXER("Right Input Mixer", SND_SOC_NOPM, 0, 0,
			   &right_input_mixer_controls[0],
			   ARRAY_SIZE(right_input_mixer_controls)),

	SND_SOC_DAPM_OUTPUT("HP Left Playback"),
	SND_SOC_DAPM_OUTPUT("HP Right Playback"),
	SND_SOC_DAPM_OUTPUT("LO Left Playback"),
	SND_SOC_DAPM_OUTPUT("LO Right Playback"),
	SND_SOC_DAPM_OUTPUT("SPK Left Playback"),
	SND_SOC_DAPM_OUTPUT("SPK Right Playback"),
	SND_SOC_DAPM_OUTPUT("RECP Playback"),
	SND_SOC_DAPM_OUTPUT("RECM Playback"),

	SND_SOC_DAPM_INPUT("IN1 Left Capture"),
	SND_SOC_DAPM_INPUT("IN2 Left Capture"),
	SND_SOC_DAPM_INPUT("IN3 Left Capture"),
	SND_SOC_DAPM_INPUT("IN4 Left Capture"),
	SND_SOC_DAPM_INPUT("IN1 Right Capture"),
	SND_SOC_DAPM_INPUT("IN2 Right Capture"),
	SND_SOC_DAPM_INPUT("IN3 Right Capture"),
	SND_SOC_DAPM_INPUT("IN4 Right Capture"),
	SND_SOC_DAPM_INPUT("Left DMIC Capture"),
	SND_SOC_DAPM_INPUT("Right DMIC Capture"),

	SND_SOC_DAPM_MICBIAS("Mic Bias Ext", AIC3262_MIC_BIAS_CNTL, 6, 0),
	SND_SOC_DAPM_MICBIAS("Mic Bias Int", AIC3262_MIC_BIAS_CNTL, 2, 0),

	SND_SOC_DAPM_SUPPLY_S("PLLCLK", 0, AIC3262_PLL_PR_POW_REG, 7, 0,
			    pll_power_on_event, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY_S("DACCLK", 2, AIC3262_NDAC_DIV_POW_REG, 7, 0,
				NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("CODEC_CLK_IN", 1, SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("DAC_MOD_CLK", 3, AIC3262_MDAC_DIV_POW_REG,
				7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADCCLK", 2, AIC3262_NADC_DIV_POW_REG,
				7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ADC_MOD_CLK", 3, AIC3262_MADC_DIV_POW_REG,
				7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ASI1_BCLK", 4, AIC3262_ASI1_BCLK_N,
				7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ASI1_WCLK", 4, AIC3262_ASI1_WCLK_N,
				7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ASI2_BCLK", 4, AIC3262_ASI2_BCLK_N,
				7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ASI2_WCLK", 4, AIC3262_ASI2_WCLK_N,
				7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ASI3_BCLK", 4, AIC3262_ASI3_BCLK_N,
				7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("ASI3_WCLK", 4, AIC3262_ASI3_WCLK_N,
				7, 0, NULL, 0),
	SND_SOC_DAPM_MUX("ASI1_BCLK Route",
			 SND_SOC_NOPM, 0, 0, &asi1bclk_control),
	SND_SOC_DAPM_MUX("ASI2_BCLK Route",
			 SND_SOC_NOPM, 0, 0, &asi2bclk_control),
	SND_SOC_DAPM_MUX("ASI3_BCLK Route",
			 SND_SOC_NOPM, 0, 0, &asi3bclk_control),
};

static const struct snd_soc_dapm_route aic3262_dapm_routes[] = {
	/* TODO: Do we need only DACCLK for ASIIN's and ADCCLK for ASIOUT??? */
	/* Clock portion */
	{"CODEC_CLK_IN", NULL, "PLLCLK"},
	{"DACCLK", NULL, "CODEC_CLK_IN"},
	{"ADCCLK", NULL, "CODEC_CLK_IN"},
	{"DAC_MOD_CLK", NULL, "DACCLK"},
#ifdef AIC3262_SYNC_MODE
	{"ADC_MOD_CLK", NULL, "DACCLK"},
#else
	{"ADC_MOD_CLK", NULL, "ADCCLK"},
#endif

	{"ASI1_BCLK Route", "DAC_CLK", "DACCLK"},
	{"ASI1_BCLK Route", "DAC_MOD_CLK", "DAC_MOD_CLK"},
	{"ASI1_BCLK Route", "ADC_CLK", "ADCCLK"},
	{"ASI1_BCLK Route", "ADC_MOD_CLK", "ADC_MOD_CLK"},

	{"ASI2_BCLK Route", "DAC_CLK", "DACCLK"},
	{"ASI2_BCLK Route", "DAC_MOD_CLK", "DAC_MOD_CLK"},
	{"ASI2_BCLK Route", "ADC_CLK", "ADCCLK"},
	{"ASI2_BCLK Route", "ADC_MOD_CLK", "ADC_MOD_CLK"},

	{"ASI3_BCLK Route", "DAC_CLK", "DACCLK"},
	{"ASI3_BCLK Route", "DAC_MOD_CLK", "DAC_MOD_CLK"},
	{"ASI3_BCLK Route", "ADC_CLK", "ADCCLK"},
	{"ASI3_BCLK Route", "ADC_MOD_CLK", "ADC_MOD_CLK"},

	{"ASI1_BCLK", NULL, "ASI1_BCLK Route"},
	{"ASI2_BCLK", NULL, "ASI2_BCLK Route"},
	{"ASI3_BCLK", NULL, "ASI3_BCLK Route"},
#ifdef AIC3262_ASI1_MASTER
	{"DIN1", NULL, "ASI1_BCLK"},
	{"DOUT1", NULL, "ASI1_BCLK"},
	{"DIN1", NULL, "ASI1_WCLK"},
	{"DOUT1", NULL, "ASI1_WCLK"},
#endif
#ifdef AIC3262_ASI2_MASTER
	{"DIN2", NULL, "ASI2_BCLK"},
	{"DOUT2", NULL, "ASI2_BCLK"},
	{"DIN2", NULL, "ASI2_WCLK"},
	{"DOUT2", NULL, "ASI2_WCLK"},
#endif
#ifdef AIC3262_ASI3_MASTER
	{"DIN3", NULL, "ASI3_BCLK"},
	{"DOUT3", NULL, "ASI3_BCLK"},
	{"DIN3", NULL, "ASI3_WCLK"},
	{"DOUT3", NULL, "ASI3_WCLK"},
#endif
	{"Left DAC", NULL, "DAC_MOD_CLK"},
	{"Right DAC", NULL, "DAC_MOD_CLK"},
	/* When we are master, ASI bclk and wclk are generated by
	 * DAC_MOD_CLK, so we put them as dependency for ADC too.
	 */
	{"Left ADC", NULL, "DAC_MOD_CLK"},
	{"Right ADC", NULL, "DAC_MOD_CLK"},
	{"Left ADC", NULL, "ADC_MOD_CLK"},
	{"Right ADC", NULL, "ADC_MOD_CLK"},
	/* Playback (DAC) Portion */
	{"HP Left Mixer", "Left DAC Playback Switch", "Left DAC"},
	{"HP Left Mixer", "MA Left Playback Switch", "MA Left Playback PGA"},
	{"HP Left Mixer", "LO Left-B1 Playback Volume", "LO Left Playback"},

	{"HP Right Mixer", "LO Right-B1 Playback Volume", "LO Right Playback"},
	{"HP Right Mixer", "Left DAC Playback Switch", "Left DAC"},
	{"HP Right Mixer", "Right DAC Playback Switch", "Right DAC"},
	{"HP Right Mixer", "MA Right Playback Switch", "MA Right Playback PGA"},

	{"HP Left Playback Driver", NULL, "HP Left Mixer"},
	{"HP Right Playback Driver", NULL, "HP Right Mixer"},

	{"HP Left Playback", NULL, "HP Left Playback Driver"},
	{"HP Right Playback", NULL, "HP Right Playback Driver"},

	{"LO Left Mixer", "MA Left Playback Switch", "MA Left Playback PGA"},
	{"LO Left Mixer", "IN1 Left-B Capture Switch", "IN1 Left Capture"},
	{"LO Left Mixer", "Left DAC Playback Switch", "Left DAC"},
	{"LO Left Mixer", "Right DAC Playback Switch", "Right DAC"},

	{"LO Right Mixer", "LO Left Playback Switch", "LO Left Playback"},
	{"LO Right Mixer", "Right DAC Playback Switch", "Right DAC"},
	{"LO Right Mixer", "MA Right Playback Switch", "MA Right Playback PGA"},
	{"LO Right Mixer", "IN1 Right-B Capture Switch", "IN1 Right Capture"},

	{"LO Left Playback Driver", NULL, "LO Left Mixer"},
	{"LO Right Playback Driver", NULL, "LO Right Mixer"},

	{"LO Left Playback", NULL, "LO Left Playback Driver"},
	{"LO Right Playback", NULL, "LO Right Playback Driver"},

	{"REC Mixer", "LO Left-B2 Playback Volume", "LO Left Playback"},
	{"REC Mixer", "IN1 Left Capture Volume", "IN1 Left Capture"},
	{"REC Mixer", "IN1 Right Capture Volume", "IN1 Right Capture"},
	{"REC Mixer", "LO Right-B2 Playback Volume", "LO Right Playback"},

	{"RECP Playback Driver", NULL, "REC Mixer"},
	{"RECM Playback Driver", NULL, "REC Mixer"},

	{"RECP Playback", NULL, "RECP Playback Driver"},
	{"RECM Playback", NULL, "RECM Playback Driver"},

	{"SPK Left Mixer", "MA Left Playback Switch", "MA Left Playback PGA"},
	{"SPK Left Mixer", "LO Left Playback Volume", "LO Left Playback"},
	{"SPK Left Mixer", "SPR_IN Switch", "SPK Right Mixer"},

	{"SPK Right Mixer", "LO Right Playback Volume", "LO Right Playback"},
	{"SPK Right Mixer", "MA Right Playback Switch",
	 "MA Right Playback PGA"},

	{"SPK Left Playback Driver", NULL, "SPK Left Mixer"},
	{"SPK Right Playback Driver", NULL, "SPK Right Mixer"},

	{"SPK Left Playback", NULL, "SPK Left Playback Driver"},
	{"SPK Right Playback", NULL, "SPK Right Playback Driver"},
	/* ASI Input routing */
	{"ASI1LIN", NULL, "DIN1"},
	{"ASI1RIN", NULL, "DIN1"},
	{"ASI1MonoMixIN", NULL, "DIN1"},
	{"ASI2LIN", NULL, "DIN2"},
	{"ASI2RIN", NULL, "DIN2"},
	{"ASI2MonoMixIN", NULL, "DIN2"},
	{"ASI3LIN", NULL, "DIN3"},
	{"ASI3RIN", NULL, "DIN3"},
	{"ASI3MonoMixIN", NULL, "DIN3"},

	{"ASI1LIN Route", "ASI1 Left In", "ASI1LIN"},
	{"ASI1LIN Route", "ASI1 Right In", "ASI1RIN"},
	{"ASI1LIN Route", "ASI1 MonoMix In", "ASI1MonoMixIN"},

	{"ASI1RIN Route", "ASI1 Right In", "ASI1RIN"},
	{"ASI1RIN Route", "ASI1 Left In", "ASI1LIN"},
	{"ASI1RIN Route", "ASI1 MonoMix In", "ASI1MonoMixIN"},

	{"ASI2LIN Route", "ASI2 Left In", "ASI2LIN"},
	{"ASI2LIN Route", "ASI2 Right In", "ASI2RIN"},
	{"ASI2LIN Route", "ASI2 MonoMix In", "ASI2MonoMixIN"},

	{"ASI2RIN Route", "ASI2 Right In", "ASI2RIN"},
	{"ASI2RIN Route", "ASI2 Left In", "ASI2LIN"},
	{"ASI2RIN Route", "ASI2 MonoMix In", "ASI2MonoMixIN"},

	{"ASI3LIN Route", "ASI3 Left In", "ASI3LIN"},
	{"ASI3LIN Route", "ASI3 Right In", "ASI3RIN"},
	{"ASI3LIN Route", "ASI3 MonoMix In", "ASI3MonoMixIN"},

	{"ASI3RIN Route", "ASI3 Right In", "ASI3RIN"},
	{"ASI3RIN Route", "ASI3 Left In", "ASI3LIN"},
	{"ASI3RIN Route", "ASI3 MonoMix In", "ASI3MonoMixIN"},

	{"ASI1IN Port", NULL, "ASI1LIN Route"},
	{"ASI1IN Port", NULL, "ASI1RIN Route"},
	{"ASI2IN Port", NULL, "ASI2LIN Route"},
	{"ASI2IN Port", NULL, "ASI2RIN Route"},
	{"ASI3IN Port", NULL, "ASI3LIN Route"},
	{"ASI3IN Port", NULL, "ASI3RIN Route"},

	{"DAC MiniDSP IN1 Route", "ASI1 In", "ASI1IN Port"},
	{"DAC MiniDSP IN1 Route", "ASI2 In", "ASI2IN Port"},
	{"DAC MiniDSP IN1 Route", "ASI3 In", "ASI3IN Port"},
	{"DAC MiniDSP IN1 Route", "ADC MiniDSP Out", "ADC MiniDSP OUT1"},

	{"DAC MiniDSP IN2 Route", "ASI1 In", "ASI1IN Port"},
	{"DAC MiniDSP IN2 Route", "ASI2 In", "ASI2IN Port"},
	{"DAC MiniDSP IN2 Route", "ASI3 In", "ASI3IN Port"},

	{"DAC MiniDSP IN3 Route", "ASI1 In", "ASI1IN Port"},
	{"DAC MiniDSP IN3 Route", "ASI2 In", "ASI2IN Port"},
	{"DAC MiniDSP IN3 Route", "ASI3 In", "ASI3IN Port"},

	{"Left DAC", "NULL", "DAC MiniDSP IN1 Route"},
	{"Right DAC", "NULL", "DAC MiniDSP IN1 Route"},
	{"Left DAC", "NULL", "DAC MiniDSP IN2 Route"},
	{"Right DAC", "NULL", "DAC MiniDSP IN2 Route"},
	{"Left DAC", "NULL", "DAC MiniDSP IN3 Route"},
	{"Right DAC", "NULL", "DAC MiniDSP IN3 Route"},

	/* Mixer Amplifier */
	{"MA Left PGA Mixer", "IN1 Left Capture Switch", "IN1 Left Capture"},
	{"MA Left PGA Mixer", "Left MicPGA Volume", "Left MicPGA"},

	{"MA Left Playback PGA", NULL, "MA Left PGA Mixer"},

	{"MA Right PGA Mixer", "IN1 Right Capture Switch", "IN1 Right Capture"},
	{"MA Right PGA Mixer", "Right MicPGA Volume", "Right MicPGA"},

	{"MA Right Playback PGA", NULL, "MA Right PGA Mixer"},

	/* Virtual connection between DAC and ADC for miniDSP IPC */
	{"ADC DAC Route", "On", "Left ADC"},
	{"ADC DAC Route", "On", "Right ADC"},

	{"Left DAC", NULL, "ADC DAC Route"},
	{"Right DAC", NULL, "ADC DAC Route"},

	/* Capture (ADC) portions */
	/* Left Positive PGA input */
	{"Left Input Mixer", "IN1 Left Capture Switch", "IN1 Left Capture"},
	{"Left Input Mixer", "IN2 Left Capture Switch", "IN2 Left Capture"},
	{"Left Input Mixer", "IN3 Left Capture Switch", "IN3 Left Capture"},
	{"Left Input Mixer", "IN4 Left Capture Switch", "IN4 Left Capture"},
	{"Left Input Mixer", "IN1 Right Capture Switch", "IN1 Right Capture"},
	/* Left Negative PGA input */
	{"Left Input Mixer", "IN2 Right Capture Switch", "IN2 Right Capture"},
	{"Left Input Mixer", "IN3 Right Capture Switch", "IN3 Right Capture"},
	{"Left Input Mixer", "IN4 Right Capture Switch", "IN4 Right Capture"},
	{"Left Input Mixer", "CM2 Left Capture Switch", "CM2 Left Capture"},
	{"Left Input Mixer", "CM1 Left Capture Switch", "CM1 Left Capture"},

	/* Right Positive PGA Input */
	{"Right Input Mixer", "IN1 Right Capture Switch", "IN1 Right Capture"},
	{"Right Input Mixer", "IN2 Right Capture Switch", "IN2 Right Capture"},
	{"Right Input Mixer", "IN3 Right Capture Switch", "IN3 Right Capture"},
	{"Right Input Mixer", "IN4 Right Capture Switch", "IN4 Right Capture"},
	{"Right Input Mixer", "IN2 Left Capture Switch", "IN2 Left Capture"},
	/* Right Negative PGA Input */
	{"Right Input Mixer", "IN1 Left Capture Switch", "IN1 Left Capture"},
	{"Right Input Mixer", "IN3 Left Capture Switch", "IN3 Left Capture"},
	{"Right Input Mixer", "IN4 Left Capture Switch", "IN4 Left Capture"},
	{"Right Input Mixer", "CM1 Right Capture Switch", "CM1 Right Capture"},
	{"Right Input Mixer", "CM2 Right Capture Switch", "CM2 Right Capture"},

	{"CM1 Left Capture", NULL, "CM"},
	{"CM2 Left Capture", NULL, "CM"},
	{"CM1 Right Capture", NULL, "CM"},
	{"CM2 Right Capture", NULL, "CM"},

	{"Left MicPGA", NULL, "Left Input Mixer"},
	{"Right MicPGA", NULL, "Right Input Mixer"},

	{"Left ADC Route", "Analog", "Left MicPGA"},
	{"Left ADC Route", "Digital", "Left DMIC Capture"},

	{"Right ADC Route", "Analog", "Right MicPGA"},
	{"Right ADC Route", "Digital", "Right DMIC Capture"},

	{"Left ADC", NULL, "Left ADC Route"},
	{"Right ADC", NULL, "Right ADC Route"},

	/* ASI Output Routing */
	{"ADC MiniDSP OUT1", NULL, "Left ADC"},
	{"ADC MiniDSP OUT1", NULL, "Right ADC"},
	{"ADC MiniDSP OUT2", NULL, "Left ADC"},
	{"ADC MiniDSP OUT2", NULL, "Right ADC"},
	{"ADC MiniDSP OUT3", NULL, "Left ADC"},
	{"ADC MiniDSP OUT3", NULL, "Right ADC"},

	{"ASI1OUT Route", "ADC MiniDSP Out1", "ADC MiniDSP OUT1"},
	{"ASI1OUT Route", "ASI1In Bypass", "ASI1IN Port"},
	{"ASI1OUT Route", "ASI2In Bypass", "ASI2IN Port"},
	{"ASI1OUT Route", "ASI3In Bypass", "ASI3IN Port"},

	{"ASI2OUT Route", "ADC MiniDSP Out1", "ADC MiniDSP OUT1"},
	{"ASI2OUT Route", "ASI1In Bypass", "ASI1IN Port"},
	{"ASI2OUT Route", "ASI2In Bypass", "ASI2IN Port"},
	{"ASI2OUT Route", "ASI3In Bypass", "ASI3IN Port"},
	{"ASI2OUT Route", "ADC MiniDSP Out2", "ADC MiniDSP OUT2"},

	{"ASI3OUT Route", "ADC MiniDSP Out1", "ADC MiniDSP OUT1"},
	{"ASI3OUT Route", "ASI1In Bypass", "ASI1IN Port"},
	{"ASI3OUT Route", "ASI2In Bypass", "ASI2IN Port"},
	{"ASI3OUT Route", "ASI3In Bypass", "ASI3IN Port"},
	{"ASI3OUT Route", "ADC MiniDSP Out3", "ADC MiniDSP OUT3"},

	{"ASI1OUT", NULL, "ASI1OUT Route"},
	{"ASI2OUT", NULL, "ASI2OUT Route"},
	{"ASI3OUT", NULL, "ASI3OUT Route"},

	{"DOUT1 Route", "ASI1 Out", "ASI1OUT"},
	{"DOUT1 Route", "DIN1 Bypass", "DIN1"},
	{"DOUT1 Route", "DIN2 Bypass", "DIN2"},
	{"DOUT1 Route", "DIN3 Bypass", "DIN3"},

	{"DOUT2 Route", "ASI2 Out", "ASI2OUT"},
	{"DOUT2 Route", "DIN1 Bypass", "DIN1"},
	{"DOUT2 Route", "DIN2 Bypass", "DIN2"},
	{"DOUT2 Route", "DIN3 Bypass", "DIN3"},

	{"DOUT3 Route", "ASI3 Out", "ASI3OUT"},
	{"DOUT3 Route", "DIN1 Bypass", "DIN1"},
	{"DOUT3 Route", "DIN2 Bypass", "DIN2"},
	{"DOUT3 Route", "DIN3 Bypass", "DIN3"},

	{"DOUT1", NULL, "DOUT1 Route"},
	{"DOUT2", NULL, "DOUT2 Route"},
	{"DOUT3", NULL, "DOUT3 Route"},
};

#define AIC3262_DAPM_ROUTE_NUM (ARRAY_SIZE(aic3262_dapm_routes)/ \
					sizeof(struct snd_soc_dapm_route))
/* aic3262_firmware_load:   This function is called by the
 *		request_firmware_nowait function as soon
 *		as the firmware has been loaded from the file.
 *		The firmware structure contains the data and$
 *		the size of the firmware loaded.
 * @fw: pointer to firmware file to be dowloaded
 * @context: pointer variable to codec
 *
 * Returns 0 for success.
 */
static void aic3262_firmware_load(const struct firmware *fw, void *context)
{
	struct snd_soc_codec *codec = context;
	struct aic3262_priv *private_ds = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	aic3xxx_cfw_lock(private_ds->cfw_p, 1);
	if (private_ds->cur_fw != NULL)
		release_firmware(private_ds->cur_fw);
	private_ds->cur_fw = NULL;

	if (fw != NULL) {
		dev_dbg(codec->dev, "Firmware binary load\n");
		private_ds->cur_fw = (void *)fw;
		ret = aic3xxx_cfw_reload(private_ds->cfw_p, (void *)fw->data,
			fw->size);
		if (ret < 0) { /* reload failed */
			dev_err(codec->dev, "Firmware binary load failed\n");
			release_firmware(private_ds->cur_fw);
			private_ds->cur_fw = NULL;
			fw = NULL;
		}
	} else {
		/* request_firmware failed*/
		/* could not locate file tlv320aic3262_fw_v1.bin
			under /vendor/firmare
		*/
		dev_err(codec->dev, "request_firmware failed\n");
		ret = -1;
	}

	aic3xxx_cfw_lock(private_ds->cfw_p, 0);
	if (ret >= 0) {
		/*init function for transition */
		aic3xxx_cfw_transition(private_ds->cfw_p, "INIT");
		/* add firmware modes */
		aic3xxx_cfw_add_modes(codec, private_ds->cfw_p);
		/* add runtime controls */
		aic3xxx_cfw_add_controls(codec, private_ds->cfw_p);
		/* set the default firmware mode */
		aic3xxx_cfw_setmode_cfg(private_ds->cfw_p, 0, 0);
	}

}

/*=========================================================

 headset work and headphone/headset jack interrupt handlers

 ========================================================*/

enum headset_accessory_state {
	BIT_NO_ACCESSORY = 0,
	BIT_HEADSET = (1 << 0),
	BIT_HEADPHONE = (1 << 1),
};

/**
 * aic3262_hs_jack_report: Report jack notication to upper layor
 * @codec: pointer variable to codec having information related to codec
 * @jack: Pointer variable to snd_soc_jack having information of codec
 *		 and pin number$
 * @report: Provides informaton of whether it is headphone or microphone
 *
*/
static void aic3262_hs_jack_report(struct snd_soc_codec *codec,
				   struct snd_soc_jack *jack, int report)
{
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	int status, state = 0, switch_state = BIT_NO_ACCESSORY;

	mutex_lock(&aic3262->mutex);

	/* Sync status */
	status = snd_soc_read(codec, AIC3262_DAC_FLAG);
	/* We will check only stereo MIC and headphone */
	switch (status & AIC3262_JACK_TYPE_MASK) {
	case AIC3262_JACK_WITH_MIC:
		state |= SND_JACK_HEADSET;
		break;
	case AIC3262_JACK_WITHOUT_MIC:
		state |= SND_JACK_HEADPHONE;
	}

	mutex_unlock(&aic3262->mutex);

	snd_soc_jack_report(jack, state, report);

	if ((state & SND_JACK_HEADSET) == SND_JACK_HEADSET)
		switch_state |= BIT_HEADSET;
	else if (state & SND_JACK_HEADPHONE)
		switch_state |= BIT_HEADPHONE;

}

/**
 * aic3262_hs_jack_detect: Detect headphone jack during boot time
 * @codec: pointer variable to codec having information related to codec
 * @jack: Pointer variable to snd_soc_jack having information of codec
 *	     and pin number$
 * @report: Provides informaton of whether it is headphone or microphone
 *
*/
void aic3262_hs_jack_detect(struct snd_soc_codec *codec,
			    struct snd_soc_jack *jack, int report)
{
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	struct aic3262_jack_data *hs_jack = &aic3262->hs_jack;

	hs_jack->jack = jack;
	hs_jack->report = report;
	aic3262_hs_jack_report(codec, hs_jack->jack, hs_jack->report);
}
EXPORT_SYMBOL_GPL(aic3262_hs_jack_detect);
/**
 * aic3262_accessory_work: Finished bottom half work from headphone jack
 *		insertion interupt
 * @work: pionter variable to work_struct which is maintaining work queqe
 *
*/
static void aic3262_accessory_work(struct work_struct *work)
{
	struct aic3262_priv *aic3262 = container_of(work,
						    struct aic3262_priv,
						    delayed_work.work);
	struct snd_soc_codec *codec = aic3262->codec;
	struct aic3262_jack_data *hs_jack = &aic3262->hs_jack;
	aic3262_hs_jack_report(codec, hs_jack->jack, hs_jack->report);
}

/**
 * aic3262_audio_handler: audio interrupt handler called
 *		when interupt is generated
 * @irq: provides interupt number which is assigned by aic3262_request_irq,
 * @data having information of data passed by aic3262_request_irq last arg,
 *
 * Return IRQ_HANDLED(means interupt handeled successfully)
*/
static irqreturn_t aic3262_audio_handler(int irq, void *data)
{
	struct snd_soc_codec *codec = data;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);

	queue_delayed_work(aic3262->workqueue, &aic3262->delayed_work,
			   msecs_to_jiffies(200));
	return IRQ_HANDLED;
}

/**
 * Methods for CFW Operations
 *
 * Due to incompatibilites between structures used by MFD and CFW
 * we need to transform the register format before linking to
 * CFW operations.
 */
static inline unsigned int aic3262_ops_cfw2reg(unsigned int reg)
{
	union cfw_register *c = (union cfw_register *) &reg;
	union aic3xxx_reg_union mreg;

	mreg.aic3xxx_register.offset = c->offset;
	mreg.aic3xxx_register.page = c->page;
	mreg.aic3xxx_register.book = c->book;
	mreg.aic3xxx_register.reserved = 0;

	return mreg.aic3xxx_register_int;
}
static int aic3262_ops_reg_read(struct snd_soc_codec *codec, unsigned int reg)
{
	return aic3xxx_reg_read(codec->control_data, aic3262_ops_cfw2reg(reg));
}

static int aic3262_ops_reg_write(struct snd_soc_codec *codec, unsigned int reg,
			  unsigned char val)
{
	return aic3xxx_reg_write(codec->control_data,
					aic3262_ops_cfw2reg(reg), val);
}

static int aic3262_ops_set_bits(struct snd_soc_codec *codec, unsigned int reg,
				unsigned char mask, unsigned char val)
{
	return aic3xxx_set_bits(codec->control_data,
					aic3262_ops_cfw2reg(reg), mask, val);

}

static int aic3262_ops_bulk_read(struct snd_soc_codec *codec, unsigned int reg,
				 int count, u8 *buf)
{
	return aic3xxx_bulk_read(codec->control_data,
					aic3262_ops_cfw2reg(reg), count, buf);
}

static int aic3262_ops_bulk_write(struct snd_soc_codec *codec, unsigned int reg,
			   int count, const u8 *buf)
{
	return aic3xxx_bulk_write(codec->control_data,
					aic3262_ops_cfw2reg(reg), count, buf);
}

/**
 * aic3262_ops_dlock_lock: To Read the run state of the DAC and ADC
 *			by reading the codec and returning the run state
 * @pv: pointer argument to the codec
 *
 * Run state Bit format
 *
 * ------------------------------------------------------
 * D31|..........| D7 | D6|  D5  |  D4  | D3 | D2 | D1  |   D0  |
 * R               R    R   LADC   RADC    R    R   LDAC   RDAC
 * ------------------------------------------------------
 *
 * R- Reserved
 * LDAC- Left DAC
 * RDAC- Right DAC
 *
 * Return value  : Integer
 */
static int aic3262_ops_lock(struct snd_soc_codec *codec)
{
	mutex_lock(&codec->mutex);

	/* Reading the run state of adc and dac */
	return aic3262_get_runstate(codec);

}

/**
 * aic3262_ops_dlock_unlock: To unlock the mutex acqiured for reading
 *			run state of the codec
 * @pv: pointer argument to the codec
 *
 * Return Value: integer returning 0
 */
static int aic3262_ops_unlock(struct snd_soc_codec *codec)
{
	/*Releasing the lock of mutex */
	mutex_unlock(&codec->mutex);
	return 0;
}

/**
 * aic3262_ops_dlock_stop:
 * @pv: pointer Argument to the codec
 * @mask: tells us the bit format of the codec running state
 *
 * Bit Format:
 * ------------------------------------------------------
 * D31|..........| D7 | D6| D5 | D4 | D3 | D2 | D1 | D0 |
 * R               R    R   AL   AR    R    R   DL   DR
 * ------------------------------------------------------
 * R  - Reserved
 * A  - minidsp_A
 * D  - minidsp_D
 *
 * Return: return run state
 */
static int aic3262_ops_stop(struct snd_soc_codec *codec, int mask)
{
	int run_state = 0;

	run_state = aic3262_get_runstate(codec);

	if (mask & AIC3XXX_COPS_MDSP_A)
		aic3xxx_set_bits(codec->control_data,
				 AIC3262_ADC_DATAPATH_SETUP, 0xC0, 0);

	if (mask & AIC3XXX_COPS_MDSP_D)
		aic3xxx_set_bits(codec->control_data,
				 AIC3262_DAC_DATAPATH_SETUP, 0xC0, 0);

	if ((mask & AIC3XXX_COPS_MDSP_A) &&
		!aic3xxx_wait_bits(codec->control_data,
				      AIC3262_ADC_FLAG, AIC3262_ADC_POWER_MASK,
				      0, AIC326X_TIME_DELAY,
					AIC326X_DELAY_COUNTER))
		goto err;

	if ((mask & AIC3XXX_COPS_MDSP_D) &&
		!aic3xxx_wait_bits(codec->control_data,
				      AIC3262_DAC_FLAG, AIC3262_DAC_POWER_MASK,
				      0, AIC326X_TIME_DELAY,
					AIC326X_DELAY_COUNTER))
		goto err;

	return run_state;
err:
	dev_err(codec->dev, "Unable to turn off ADCs or DACs at [%s:%d]",
				__FILE__, __LINE__);
	return -EINVAL;
}

/**
 * aic3262_ops_dlock_restore: To unlock the mutex acqiured for reading
 * @pv: pointer argument to the codec,run_state
 * @run_state:  run state of the codec and to restore the states of the dsp
 *
 * Return Value	: integer returning 0
 */

static int aic3262_ops_restore(struct snd_soc_codec *codec, int run_state)
{
	int sync_state;

	/* This is for read the sync mode register state  */
	sync_state = aic3xxx_reg_read(codec->control_data, AIC3262_DAC_PRB);

	/*checking whether the sync mode has been set or
	   not and checking the current state */
	if (((run_state & 0x30) && (run_state & 0x03)) && (sync_state & 0x80))
		aic3262_restart_dsps_sync(codec, run_state);
	else
		aic3262_dsp_pwrup(codec, run_state);

	return 0;
}

/**
 * aic3262_ops_adaptivebuffer_swap: To swap the coefficient buffers
 *				 of minidsp according to mask
 * @pv: pointer argument to the codec,
 * @mask: tells us which dsp has to be chosen for swapping
 *
 * Return Value    : returning 0 on success
 */
int aic3262_ops_adaptivebuffer_swap(struct snd_soc_codec *codec, int mask)
{
	const int sbuf[][2] = {
		{ AIC3XXX_ABUF_MDSP_A, AIC3262_ADC_ADAPTIVE_CRAM_REG },
		{ AIC3XXX_ABUF_MDSP_D1, AIC3262_DAC_ADAPTIVE_BANK1_REG },
		{ AIC3XXX_ABUF_MDSP_D2, AIC3262_DAC_ADAPTIVE_BANK2_REG },
	};
	int i;

	for (i = 0; i < sizeof(sbuf)/sizeof(sbuf[0]); ++i) {
		if (!(mask & sbuf[i][0]))
			continue;
		aic3xxx_set_bits(codec->control_data, sbuf[i][1], 0x1, 0x1);
		if (!aic3xxx_wait_bits(codec->control_data,
						sbuf[i][1], 0x1, 0, 15, 1))
			goto err;
	}
	return 0;
err:
	dev_err(codec->dev, "miniDSP buffer swap failure at [%s:%d]",
				__FILE__, __LINE__);
	return -EINVAL;
}

/**
 * get_runstate: To read the current state of the dac's and adc's
 * @ps: pointer argument to the codec
 *
 * Return Value	: returning the runstate
 */
static int aic3262_get_runstate(struct snd_soc_codec *codec)
{
	unsigned int dac, adc;
	/* Read the run state */
	dac = aic3xxx_reg_read(codec->control_data, AIC3262_DAC_FLAG);
	adc = aic3xxx_reg_read(codec->control_data, AIC3262_ADC_FLAG);

	return (((adc>>6)&1)<<5)  |
		(((adc>>2)&1)<<4) |
		(((dac>>7)&1)<<1) |
		(((dac>>3)&1)<<0);
}

/**
 * aic3262_dsp_pwrdwn_status: To read the status of dsp's
 * @pv: pointer argument to the codec , cur_state of dac's and adc's
 *
 * Return Value	: integer returning 0
 */
static int aic3262_dsp_pwrdwn_status(struct snd_soc_codec *codec)
{

	aic3xxx_set_bits(codec->control_data,
			AIC3262_ADC_DATAPATH_SETUP, 0XC0, 0);
	aic3xxx_set_bits(codec->control_data,
			AIC3262_DAC_DATAPATH_SETUP, 0XC0, 0);

	if (!aic3xxx_wait_bits(codec->control_data, AIC3262_ADC_FLAG,
			      AIC3262_ADC_POWER_MASK, 0, AIC326X_TIME_DELAY,
			      AIC326X_DELAY_COUNTER))
		goto err;
	if (!aic3xxx_wait_bits(codec->control_data, AIC3262_DAC_FLAG,
			AIC3262_DAC_POWER_MASK, 0, AIC326X_TIME_DELAY,
			AIC326X_DELAY_COUNTER))
		goto err;

	return 0;
err:
	dev_err(codec->dev, "DAC/ADC Power down timedout at [%s:%d]",
				__FILE__, __LINE__);
	return -EINVAL;
}
static int aic3262_dsp_pwrup(struct snd_soc_codec *codec, int state)
{
	int adc_reg_mask = 0;
	int adc_power_mask = 0;
	int dac_reg_mask = 0;
	int dac_power_mask = 0;
	int ret_wbits;

	if (state & AIC3XXX_COPS_MDSP_A_L) {
		adc_reg_mask |= 0x80;
		adc_power_mask |= AIC3262_LADC_POWER_MASK;
	}
	if (state & AIC3XXX_COPS_MDSP_A_R) {
		adc_reg_mask |= 0x40;
		adc_power_mask |= AIC3262_RADC_POWER_MASK;
	}

	if (state & AIC3XXX_COPS_MDSP_A)
		aic3xxx_set_bits(codec->control_data,
					AIC3262_ADC_DATAPATH_SETUP, 0XC0,
					adc_reg_mask);

	if (state & AIC3XXX_COPS_MDSP_D_L) {
		dac_reg_mask |= 0x80;
		dac_power_mask |= AIC3262_LDAC_POWER_STATUS_MASK;
	}
	if (state & AIC3XXX_COPS_MDSP_D_R) {
		dac_reg_mask |= 0x40;
		dac_power_mask |= AIC3262_RDAC_POWER_STATUS_MASK;
	}

	if (state & AIC3XXX_COPS_MDSP_D)
		aic3xxx_set_bits(codec->control_data,
					AIC3262_DAC_DATAPATH_SETUP, 0XC0,
					dac_reg_mask);

	if (state & AIC3XXX_COPS_MDSP_A) {
		ret_wbits = aic3xxx_wait_bits(codec->control_data,
				AIC3262_ADC_FLAG, AIC3262_ADC_POWER_MASK,
				adc_power_mask, AIC326X_TIME_DELAY,
				AIC326X_DELAY_COUNTER);
		if (!ret_wbits)
			dev_err(codec->dev, "ADC Power down timedout\n");
	}

	if (state & AIC3XXX_COPS_MDSP_D) {
		ret_wbits = aic3xxx_wait_bits(codec->control_data,
				AIC3262_DAC_FLAG, AIC3262_DAC_POWER_MASK,
				dac_power_mask, AIC326X_TIME_DELAY,
				AIC326X_DELAY_COUNTER);
		if (!ret_wbits)
			dev_err(codec->dev, "ADC Power down timedout\n");
	}

	return 0;
}

static int aic3262_restart_dsps_sync(struct snd_soc_codec *codec, int run_state)
{

	aic3262_dsp_pwrdwn_status(codec);
	aic3262_dsp_pwrup(codec, run_state);

	return 0;
}

static const struct aic3xxx_codec_ops aic3262_cfw_codec_ops = {
	.reg_read  =	aic3262_ops_reg_read,
	.reg_write =	aic3262_ops_reg_write,
	.set_bits  =	aic3262_ops_set_bits,
	.bulk_read =	aic3262_ops_bulk_read,
	.bulk_write =	aic3262_ops_bulk_write,
	.lock      =	aic3262_ops_lock,
	.unlock    =	aic3262_ops_unlock,
	.stop      =	aic3262_ops_stop,
	.restore   =	aic3262_ops_restore,
	.bswap     =	aic3262_ops_adaptivebuffer_swap,
};


/**
 * aic3262_codec_read: provide read api to read aic3262 registe space
 * @codec: pointer variable to codec having codec information,
 * @reg: register address,
 *
 * Return: Return value will be value read.
 */
unsigned int aic3262_codec_read(struct snd_soc_codec *codec, unsigned int reg)
{

	u8 value;

	union aic3xxx_reg_union *aic_reg = (union aic3xxx_reg_union *) &reg;
	value = aic3xxx_reg_read(codec->control_data, reg);
	dev_dbg(codec->dev, "p %d , r 30 %x %x\n",
		aic_reg->aic3xxx_register.page,
		aic_reg->aic3xxx_register.offset, value);
	return value;
}

/**
 * aic3262_codec_write: provide write api to write at aic3262 registe space
 * @codec: Pointer variable to codec having codec information,
 * @reg: Register address,
 * @value: Value to be written to address space
 *
 * Return: Total no of byte written to address space.
 */
int aic3262_codec_write(struct snd_soc_codec *codec, unsigned int reg,
			unsigned int value)
{
	union aic3xxx_reg_union *aic_reg = (union aic3xxx_reg_union *) &reg;
	dev_dbg(codec->dev, "p %d, w 30 %x %x\n",
		aic_reg->aic3xxx_register.page,
		aic_reg->aic3xxx_register.offset, value);
	return aic3xxx_reg_write(codec->control_data, reg, value);
}

/**
 * aic3262_set_interface_fmt: Setting interface ASI1/2/3 data format
 * @dai: ponter to dai Holds runtime data for a DAI,
 * @fmt: asi format info,
 * @channel: number of channel,
 *
 * Return: On success return 0.
*/
static int aic3262_set_interface_fmt(struct snd_soc_dai *dai, unsigned int fmt,
					unsigned int channel)
{
	int aif_interface_reg;
	int aif_bclk_offset_reg;
	struct snd_soc_codec *codec = dai->codec;
	u8 iface_val = 0;
	u8 dsp_a_val = 0;

	switch (dai->id) {
	case 0:
		aif_interface_reg = AIC3262_ASI1_BUS_FMT;
		aif_bclk_offset_reg = AIC3262_ASI1_LCH_OFFSET;
		break;
	case 1:
		aif_interface_reg = AIC3262_ASI2_BUS_FMT;
		aif_bclk_offset_reg = AIC3262_ASI2_LCH_OFFSET;
		break;
	case 2:
		aif_interface_reg = AIC3262_ASI3_BUS_FMT;
		aif_bclk_offset_reg = AIC3262_ASI3_LCH_OFFSET;
		break;
	default:
		return -EINVAL;

	}
	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface_val = 0;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		dsp_a_val = 0x1;	/* Intentionally falling through */
	case SND_SOC_DAIFMT_DSP_B:
		if (channel == 1)
			iface_val = 0x80;	/* Choose mono PCM */
		else if (channel <= 8)
			iface_val = 0x20;	/* choose multichannel PCM */
		else
			return -EINVAL;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface_val = 0x40;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_val = 0x60;
		break;
	default:
		dev_err(codec->dev, "Invalid DAI interface format\n");
		return -EINVAL;
	}
	snd_soc_update_bits(codec, aif_interface_reg,
					AIC3262_ASI_INTERFACE_MASK, iface_val);
	snd_soc_update_bits(codec, aif_bclk_offset_reg,
					AIC3262_BCLK_OFFSET_MASK, dsp_a_val);
	return 0;

}

/**
 * aic3262_hw_params: This function is to set the hardware parameters
 *		for AIC3262.
 *		The functions set the sample rate and audio serial data word
 *		length.
 * @substream: pointer variable to sn_pcm_substream,
 * @params: pointer to snd_pcm_hw_params structure,
 * @dai: ponter to dai Holds runtime data for a DAI,
 *
 * Return: Return 0 on success.
 */
int aic3262_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	int asi_reg, ret = 0;
	u8 data = 0, value = 0, val = 0, wclk_div = 0, bclk_div = 0;
	unsigned int channels = params_channels(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aic3262->stream_status = 1;
	else
		aic3262->stream_status = 0;

	switch (dai->id) {
	case 0:
		asi_reg = AIC3262_ASI1_BUS_FMT;
		break;
	case 1:
		asi_reg = AIC3262_ASI2_BUS_FMT;
		break;
	case 2:
		asi_reg = AIC3262_ASI3_BUS_FMT;
		break;
	default:
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		data = data | 0x00;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		data |= (0x08);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data |= (0x10);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (0x18);
		break;
	}

	/* Configure TDM for multi chennels */
	switch (channels) {
	case 4:
		value = value | 0x40;
		bclk_div = 0x03;
		wclk_div = 0x40;
		break;
	case 6:
		bclk_div = 0x02;
		wclk_div = 0x60;
		value = value | 0x80;
		break;
	case 8:
		bclk_div = 0x01;
		wclk_div = 0x00;
		value = value | 0xC0;
		break;
	default:
		break;
	}

	mutex_lock(&aic3262->mutex);
	CHECK_AIC326x_I2C_SHUTDOWN(aic3262, codec)
	snd_soc_update_bits(codec, AIC3262_ASI1_CHNL_SETUP,
				AIC3262_ASI1_CHNL_MASK, value);

	if (channels > 2) {
		snd_soc_update_bits(codec, AIC3262_ASI1_BCLK_N,
				AIC3262_ASI1_BCLK_N_MASK, bclk_div);
		snd_soc_update_bits(codec, AIC3262_ASI1_WCLK_N,
				AIC3262_ASI1_WCLK_N_MASK, wclk_div);
	}

	val = snd_soc_read(codec, AIC3262_ASI1_BUS_FMT);
	val = snd_soc_read(codec, AIC3262_ASI1_CHNL_SETUP);

	/* configure the respective Registers for the above configuration */
	snd_soc_update_bits(codec, asi_reg,
			    AIC3262_ASI_DATA_WORD_LENGTH_MASK, data);
	ret = aic3262_set_interface_fmt(dai, aic3262->asi_fmt[dai->id],
					 channels);
	if (ret < 0) {
		dev_err(codec->dev, "failed to set hardware params for AIC3262\n");
		mutex_unlock(&aic3262->mutex);
		return ret;
	}

	mutex_unlock(&aic3262->mutex);
	return 0;
}

/**
 * aic3262_mute: This function is to mute or unmute the left and right DAC
 * @dai: ponter to dai Holds runtime data for a DAI,
 * @mute: integer value one if we using mute else unmute,
 *
 * Return: return 0 on success.
 */
static int aic3262_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "codec : %s : started\n", __func__);
	if (dai->id > 2)
		return -EINVAL;
	mutex_lock(&aic3262->mutex);
	CHECK_AIC326x_I2C_SHUTDOWN(aic3262, codec)
	if (mute) {
		aic3262->mute_asi &= ~((0x1) << dai->id);
		if (aic3262->mute_asi == 0)
			/* Mute only when all asi's are muted */
			snd_soc_update_bits_locked(codec,
						   AIC3262_DAC_MVOL_CONF,
						   AIC3262_DAC_LR_MUTE_MASK,
						   AIC3262_DAC_LR_MUTE);

	} else {	/* Unmute */
		if (aic3262->mute_asi == 0)
			/* Unmute for the first asi that need to unmute.
			   rest unmute will pass */
			snd_soc_update_bits_locked(codec,
						   AIC3262_DAC_MVOL_CONF,
						   AIC3262_DAC_LR_MUTE_MASK,
						   0x0);
		aic3262->mute_asi |= ((0x1) << dai->id);
	}
	dev_dbg(codec->dev, "codec : %s : ended\n", __func__);
	mutex_unlock(&aic3262->mutex);
	return 0;
}


/**
 * aic3262_set_dai_fmt: This function is to set the DAI format
 * @codec_dai: ponter to dai Holds runtime data for a DAI,
 * @fmt: asi format info,
 *
 * return: return 0 on success.
 */
static int aic3262_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct aic3262_priv *aic3262;
	struct snd_soc_codec *codec;
	u8 iface_val, master;
	int aif_bclk_wclk_reg;

	codec = codec_dai->codec;
	aic3262 = snd_soc_codec_get_drvdata(codec);
	iface_val = 0x00;
	master = 0x0;

	switch (codec_dai->id) {
	case 0:
		aif_bclk_wclk_reg = AIC3262_ASI1_BWCLK_CNTL_REG;
		break;
	case 1:
		aif_bclk_wclk_reg = AIC3262_ASI2_BWCLK_CNTL_REG;
		break;
	case 2:
		aif_bclk_wclk_reg = AIC3262_ASI3_BWCLK_CNTL_REG;
		break;
	default:
		return -EINVAL;

	}
	aic3262->asi_fmt[codec_dai->id] = fmt;
	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		aic3262->master = 1;
		master |= (AIC3262_WCLK_OUT_MASK | AIC3262_BCLK_OUT_MASK);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		aic3262->master = 0;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:	/* new case..for debug purpose */
		master |= (AIC3262_WCLK_OUT_MASK);
		aic3262->master = 0;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		master |= (AIC3262_BCLK_OUT_MASK);
		aic3262->master = 0;
		break;

	default:
		dev_err(codec->dev, "Invalid DAI master/slave" " interface\n");

		return -EINVAL;
	}
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_IB_NF:
			master |= AIC3262_BCLK_INV_MASK;
			break;
		default:
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			break;
		case SND_SOC_DAIFMT_IB_NF:
			master |= AIC3262_BCLK_INV_MASK;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	mutex_lock(&aic3262->mutex);
	CHECK_AIC326x_I2C_SHUTDOWN(aic3262, codec)
	snd_soc_update_bits(codec, aif_bclk_wclk_reg,
			    AIC3262_WCLK_BCLK_MASTER_MASK, master);
	mutex_unlock(&aic3262->mutex);
	return 0;
}

/**
 * aic3262_dai_set_pll: This function is to Set pll for aic3262 codec dai
 * @dai: ponter to dai Holds runtime data for a DAI,$
 * @pll_id: integer pll_id
 * @fin: frequency in,
 * @fout: Frequency out,
 *
 * Return: return 0 on success
*/
static int aic3262_dai_set_pll(struct snd_soc_dai *dai, int pll_id, int source,
				unsigned int Fin, unsigned int Fout)
{
	struct snd_soc_codec *codec = dai->codec;
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "In aic3262: dai_set_pll\n");
	dev_dbg(codec->dev, "%d, %s, dai->id = %d\n", __LINE__,
		__func__, dai->id);
	/* select the PLL_CLKIN */
	mutex_lock(&aic3262->mutex);
	CHECK_AIC326x_I2C_SHUTDOWN(aic3262, codec)
	snd_soc_update_bits(codec, AIC3262_PLL_CLKIN_REG,
			    AIC3262_PLL_CLKIN_MASK, source <<
			    AIC3262_PLL_CLKIN_SHIFT);
	/* TODO: How to select low/high clock range? */

	mutex_unlock(&aic3262->mutex);
	aic3xxx_cfw_set_pll(aic3262->cfw_p, dai->id);

	return 0;
}

/**
 *
 * aic3262_set_bias_level: This function is to get triggered
 *			 when dapm events occurs.
 * @codec: pointer variable to codec having informaton related to codec,
 * @level: Bias level-> ON, PREPARE, STANDBY, OFF.
 *
 * Return: Return 0 on success.
 */
static int aic3262_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	mutex_lock(&aic3262->mutex);
	CHECK_AIC326x_I2C_SHUTDOWN(aic3262, codec)
	switch (level) {
		/* full On */
	case SND_SOC_BIAS_ON:

		dev_dbg(codec->dev, "set_bias_on\n");
		break;

		/* partial On */
	case SND_SOC_BIAS_PREPARE:
		dev_dbg(codec->dev, "set_bias_prepare\n");
		break;

		/* Off, with power */
	case SND_SOC_BIAS_STANDBY:
		/*
		 * all power is driven by DAPM system,
		 * so output power is safe if bypass was set
		 */
		dev_dbg(codec->dev, "set_bias_stby\n");
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
			pm_runtime_get_sync(codec->dev);
			snd_soc_update_bits(codec, AIC3262_POWER_CONF,
					    (AIC3262_AVDD_TO_DVDD_MASK |
					     AIC3262_EXT_ANALOG_SUPPLY_MASK),
					    0x0);
			snd_soc_update_bits(codec, AIC3262_REF_PWR_DLY,
					    AIC3262_CHIP_REF_PWR_ON_MASK,
					    AIC3262_CHIP_REF_PWR_ON);
			mdelay(40);

			snd_soc_update_bits(codec, AIC3262_CHARGE_PUMP_CNTL,
				AIC3262_DYNAMIC_OFFSET_CALIB_MASK,
				AIC3262_DYNAMIC_OFFSET_CALIB);
		}
		break;

		/* Off, without power */
	case SND_SOC_BIAS_OFF:
		dev_dbg(codec->dev, "set_bias_off\n");
		/* force all power off */
		if (codec->dapm.bias_level == SND_SOC_BIAS_STANDBY) {
			snd_soc_update_bits(codec, AIC3262_REF_PWR_DLY,
					    AIC3262_CHIP_REF_PWR_ON_MASK, 0x0);
			snd_soc_update_bits(codec, AIC3262_POWER_CONF,
					    (AIC3262_AVDD_TO_DVDD_MASK |
					     AIC3262_EXT_ANALOG_SUPPLY_MASK),
					    (AIC3262_AVDD_TO_DVDD |
					     AIC3262_EXT_ANALOG_SUPPLY_OFF));
			pm_runtime_put(codec->dev);
		}
		break;
	}

	codec->dapm.bias_level = level;
	mutex_unlock(&aic3262->mutex);
	return 0;
}

/**
 *
 * aic3262_suspend; This function is to suspend the AIC3262 driver.
 * @codec: pointer variable to codec having informaton related to codec,
 *
 * Return: Return 0 on success.
 */
static int aic3262_suspend(struct snd_soc_codec *codec)
{
	aic3262_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

/**
 * aic3262_resume: This function is to resume the AIC3262 driver
 *		 from off state to standby
 * @codec: pointer variable to codec having informaton related to codec,
 *
 * Return: Return 0 on success.
 */
static int aic3262_resume(struct snd_soc_codec *codec)
{
	aic3262_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

/**
 * aic3262_probe: This is first driver function called by the SoC core driver.
 * @codec: pointer variable to codec having informaton related to codec,
 *
 * Return: Return 0 on success.
 */
static int aic3262_codec_probe(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct aic3xxx *control;
	struct aic3262_priv *aic3262;

	if (codec == NULL)
		dev_err(codec->dev, "codec pointer is NULL.\n");

	codec->control_data = dev_get_drvdata(codec->dev->parent);
	control = codec->control_data;
	aic3262 = kzalloc(sizeof(struct aic3262_priv), GFP_KERNEL);
	if (aic3262 == NULL)
		return -ENOMEM;

	snd_soc_codec_set_drvdata(codec, aic3262);
	aic3262->pdata = dev_get_platdata(codec->dev->parent);
	aic3262->codec = codec;
	aic3262->shutdown = &control->shutdown_complete;
	aic3262->cur_fw = NULL;
	aic3262->cfw_p = &(aic3262->cfw_ps);
	aic3xxx_cfw_init(aic3262->cfw_p, &aic3262_cfw_codec_ops,
							aic3262->codec);
	aic3262->workqueue = create_singlethread_workqueue("aic3262-codec");
	if (!aic3262->workqueue) {
		ret = -ENOMEM;
		goto work_err;
	}
	INIT_DELAYED_WORK(&aic3262->delayed_work, aic3262_accessory_work);
	mutex_init(&aic3262->mutex);
	mutex_init(&codec->mutex);
	mutex_init(&aic3262->cfw_mutex);
	pm_runtime_enable(codec->dev);
	pm_runtime_resume(codec->dev);
		aic3262->dsp_runstate = 0;

	if (control->irq) {
		ret = aic3xxx_request_irq(codec->control_data,
					  AIC3262_IRQ_HEADSET_DETECT,
					  aic3262_audio_handler,
					  IRQF_NO_SUSPEND,
					  "aic3262_irq_headset", codec);

	if (ret) {
			dev_err(codec->dev, "HEADSET detect irq request"
			"failed: %d\n", ret);
			goto irq_err;
		} else {
			/*  Dynamic Headset Detection Enabled */
			snd_soc_update_bits(codec, AIC3262_HP_DETECT,
			AIC3262_HEADSET_IN_MASK, AIC3262_HEADSET_IN_MASK);
		}
	}

	codec->dapm.idle_bias_off = 1;

	/* Keep the reference voltage ON while in$
	   STANDBY mode for fast power up */

	snd_soc_update_bits(codec, AIC3262_REF_PWR_DLY,
			    AIC3262_CHIP_REF_PWR_ON_MASK,
			    AIC3262_CHIP_REF_PWR_ON);
	mdelay(40);

	snd_soc_update_bits(codec, AIC3262_CHARGE_PUMP_CNTL,
				AIC3262_DYNAMIC_OFFSET_CALIB_MASK,
				AIC3262_DYNAMIC_OFFSET_CALIB);

	aic3262_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	aic3262->mute_asi = 0;

	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				"tlv320aic3262_fw_v1.bin", codec->dev,
				GFP_KERNEL, codec, aic3262_firmware_load);
	if (ret < 0) {
		dev_err(codec->dev, "Firmware request failed\n");
		goto firm_err;
	}

	return 0;
firm_err:
	aic3xxx_free_irq(control,
			 AIC3262_IRQ_HEADSET_DETECT, codec);
irq_err:
	destroy_workqueue(aic3262->workqueue);
work_err:
	kfree(aic3262);
	return 0;
}

/*
 * aic3262_remove: Cleans up and Remove aic3262 soc device
 * @codec: pointer variable to codec having informaton related to codec,
 *
 * Return: Return 0 on success.
 */
static int aic3262_codec_remove(struct snd_soc_codec *codec)
{
	/* power down chip */
	struct aic3262_priv *aic3262 = snd_soc_codec_get_drvdata(codec);
	struct aic3xxx *control = codec->control_data;

	aic3262_set_bias_level(codec, SND_SOC_BIAS_OFF);

	/* free_irq if any */
	switch (control->type) {
	case TLV320AIC3262:
		if (control->irq)
			aic3xxx_free_irq(control,
					 AIC3262_IRQ_HEADSET_DETECT, codec);
		break;
	default:
		dev_info(codec->dev, "Coded is not TLV320AIC3262\n");
	}
	/* release firmware if any */
	if (aic3262->cur_fw != NULL)
		release_firmware(aic3262->cur_fw);
	/* destroy workqueue for jac dev */
	destroy_workqueue(aic3262->workqueue);

	kfree(aic3262);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_driver_aic326x = {
	.probe = aic3262_codec_probe,
	.remove = aic3262_codec_remove,
	.suspend = aic3262_suspend,
	.resume = aic3262_resume,
	.read = aic3262_codec_read,
	.write = aic3262_codec_write,
	.controls = aic3262_snd_controls,
	.num_controls = ARRAY_SIZE(aic3262_snd_controls),
	.dapm_widgets = aic3262_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(aic3262_dapm_widgets),
	.dapm_routes = aic3262_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(aic3262_dapm_routes),
	.set_bias_level = aic3262_set_bias_level,
	.idle_bias_off = true,
	.reg_cache_size = 0,
	.reg_word_size = sizeof(u8),
	.reg_cache_default = NULL,
};

static int aic326x_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &soc_codec_driver_aic326x,
				      aic326x_dai_driver,
				      ARRAY_SIZE(aic326x_dai_driver));

}

static int aic326x_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id aic3262_of_match[] = {
		{ .compatible = "ti,aic3262", },
		{ },
};
MODULE_DEVICE_TABLE(of, aic3262_of_match);

static const struct platform_device_id aic3262_i2c_id[] = {
		{ "tlv320aic3262-codec", 0 },
		{ }
};
MODULE_DEVICE_TABLE(i2c, aic3262_i2c_id);

static struct platform_driver aic326x_codec_driver = {
	.driver = {
		.name = "tlv320aic3262-codec",
		.owner = THIS_MODULE,
		.of_match_table = aic3262_of_match,
	},
	.probe = aic326x_probe,
	.remove = aic326x_remove,
	.id_table = aic3262_i2c_id,
};

module_platform_driver(aic326x_codec_driver);

MODULE_ALIAS("platform:tlv320aic3262-codec");
MODULE_DESCRIPTION("ASoC TLV320AIC3262 codec driver");
MODULE_AUTHOR("Y Preetam Sashank Reddy ");
MODULE_AUTHOR("Barani Prashanth ");
MODULE_AUTHOR("Mukund Navada K <navada@ti.com>");
MODULE_AUTHOR("Naren Vasanad <naren.vasanad@ti.com>");
MODULE_LICENSE("GPL");
