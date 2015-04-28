/*
 * linux/sound/soc/codecs/tlv320aic31xx.c
 *
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * Based on sound/soc/codecs/tlv320aic326x.c
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED AS IS AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * The TLV320AIC31xx series of audio codec is a low-power, highly integrated
 * high performance codec which provides a stereo DAC, a mono ADC,
 * and mono/stereo Class-D speaker driver.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/acpi.h>
#include <asm/div64.h>
#include <sound/jack.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tlv320aic31xx.h"

static const struct regmap_range_cfg aic31xx_ranges[] = {
{
	.name = "codec-regmap",
	.range_min = 128,
	.range_max = 13 * 128,
	.selector_reg = 0,
	.selector_mask = 0xff,
	.selector_shift = 0,
	.window_start = 0,
	.window_len = 128,
},
};

struct regmap_config aicxxx_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
	.ranges = aic31xx_ranges,
	.num_ranges = ARRAY_SIZE(aic31xx_ranges),
	.max_register =  13 * 128,
};
struct aic31xx_driver_data aic31xx_acpi_data = {
	.acpi_device = 1,
};
struct aic31xx_driver_data aic31xx_i2c_data = {
	.acpi_device = 0,
};

/* Custom micbias widget since mic bias has two bits */
#define SND_SOC_DAPM_MICBIASCUSTOM(wname, wreg, wshift, wevent, wflags) \
{	.id = snd_soc_dapm_micbias, .name = wname, .reg = wreg,	\
	.shift = wshift, .event = wevent, \
	.event_flags = wflags}

static int aic31xx_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params,
			struct snd_soc_dai *dai);

static int aic31xx_set_dai_fmt(struct snd_soc_dai *codec_dai,
			unsigned int fmt);

static int aic31xx_set_bias_level(struct snd_soc_codec *codec,
		enum snd_soc_bias_level level);
static int aic31xx_set_sysclk(struct snd_soc_codec *codec,
		int clk_id, int source, unsigned int freq, int dir);

static const struct aic31xx_rate_divs aic31xx_divs[] = {
/* 8k rate */
	{AIC31XX_FREQ_25000000, 8000, 2, 6, 5536, 128, 4, 20, 128, 4, 20, 4},

/* 11.025k rate */
	{AIC31XX_FREQ_25000000, 11025, 2, 7, 7738, 128, 3, 20, 128, 3, 20, 4},

/* 16k rate */
	{AIC31XX_FREQ_25000000, 16000, 2, 6, 5536, 128, 4, 10, 128, 4, 10, 4},

/* 22.05k rate */
	{AIC31XX_FREQ_25000000, 22050, 2, 7, 7738, 128, 3, 10, 128, 3, 10, 4},
/* 32k rate */
	{AIC31XX_FREQ_25000000, 32000, 2, 6, 5536, 128, 4, 5, 128, 4, 5, 4},

/* 44.1k rate */
	{AIC31XX_FREQ_25000000, 44100, 2, 6, 5536, 128, 4, 5, 128, 4, 5, 4},

/* 48k rate */
	{AIC31XX_FREQ_25000000, 48000, 2, 7, 3728, 128, 3, 5, 128, 3, 5, 4},
};

/*
 * Global Variables introduced to reduce Headphone Analog Volume Control
 * Registers at run-time
 */
static const char *const micbias_voltage[] = {"off", "2 V", "2.5 V", "AVDD"};

/* Global Variables for ASI1 Routing*/
static const char *const asilin_text[] = {
	"off", "ASI Left In", "ASI Right In", "ASI MonoMix In"
};

static const char *const asirin_text[] = {
	"off", "ASI Right In", "ASI Left In", "ASI MonoMix In"
};

/*ASI left*/
static SOC_ENUM_SINGLE_DECL(asilin_enum, AIC31XX_DACSETUP, 4, asilin_text);

/*ASI right*/
static SOC_ENUM_SINGLE_DECL(asirin_enum, AIC31XX_DACSETUP, 2, asirin_text);

static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -6350, 50, 0);
static const DECLARE_TLV_DB_SCALE(adc_fgain_tlv, 0, 10, 0);
static const DECLARE_TLV_DB_SCALE(adc_cgain_tlv, -2000, 50, 0);
static const DECLARE_TLV_DB_SCALE(mic_pga_tlv, 0, 50, 0);
static const DECLARE_TLV_DB_SCALE(hp_drv_tlv, 0, 100, 0);
static const DECLARE_TLV_DB_SCALE(class_D_drv_tlv, 600, 600, 0);
static const DECLARE_TLV_DB_SCALE(hp_vol_tlv, -7830, 60, 0);
static const DECLARE_TLV_DB_SCALE(sp_vol_tlv, -7830, 60, 0);

/*
 * controls that need to be exported to the user space
 */
static const struct snd_kcontrol_new aic31xx_snd_controls[] = {
	/* DAC Volume Control*/
	 SOC_DOUBLE_R_SX_TLV("DAC Playback Volume", AIC31XX_LDACVOL,
			AIC31XX_RDACVOL, 0, 0x81, 0xaf, dac_vol_tlv),
	/* DAC Volume soft stepping control */
	/* HP driver mute control */
	SOC_DOUBLE_R("HP driver mute", AIC31XX_HPLGAIN,
			AIC31XX_HPRGAIN, 2, 1, 0),


	/* ADC FINE GAIN */
	SOC_SINGLE_TLV("ADC FINE GAIN", AIC31XX_ADCFGA, 4, 4, 1,
			adc_fgain_tlv),
	/* ADC COARSE GAIN */
	SOC_DOUBLE_R_SX_TLV("ADC Capture Volume", AIC31XX_ADCVOL,
			AIC31XX_ADCVOL,	0, 0x28, 0x40,
			adc_cgain_tlv),
	/* ADC MIC PGA GAIN */
	SOC_SINGLE_TLV("Mic PGA Gain", AIC31XX_MICPGA, 0,
			119, 0, mic_pga_tlv),

	/* HP driver Volume Control */
	SOC_DOUBLE_R_TLV("HP Driver Gain", AIC31XX_HPLGAIN,
			AIC31XX_HPRGAIN, 3, 0x09, 0, hp_drv_tlv),
	/* Left DAC input selection control */

	/* DAC Processing Block Selection */
	SOC_SINGLE("DAC Processing Block Selection(0 <->25)",
			AIC31XX_DACPRB, 0, 0x19, 0),
	/* ADC Processing Block Selection */
	SOC_SINGLE("ADC Processing Block Selection(0 <->25)",
			AIC31XX_ADCPRB, 0, 0x12, 0),

	/* Throughput of 7-bit vol ADC for pin control */
	/* HP Analog Gain Volume Control */
	SOC_DOUBLE_R_TLV("HP Analog Gain", AIC31XX_LANALOGHPL,
			AIC31XX_RANALOGHPR, 0, 0x7F, 1, hp_vol_tlv),
	/* ADC MUTE */
	SOC_SINGLE("ADC mute", AIC31XX_ADCFGA,
			 7, 2, 0),
};

static const struct snd_kcontrol_new aic311x_snd_controls[] = {
	/* SP Class-D driver output stage gain Control */
	SOC_DOUBLE_R_TLV("SP Driver Gain", AIC31XX_SPLGAIN,
			AIC31XX_SPRGAIN, 3, 0x03, 0, class_D_drv_tlv),
	/* SP Analog Gain Volume Control */
	SOC_DOUBLE_R_TLV("Analog Channel Gain", AIC31XX_LANALOGSPL,
			AIC31XX_RANALOGSPR, 0, 0x7F, 1, sp_vol_tlv),
	/* SP driver mute control */
	SOC_DOUBLE_R("SP driver mute", AIC31XX_SPLGAIN,
			AIC31XX_SPRGAIN, 2, 1, 0),
};

static const struct snd_kcontrol_new aic310x_snd_controls[] = {
	/* SP Class-D driver output stage gain Control */
	SOC_SINGLE_TLV("SP Driver Gain", AIC31XX_SPLGAIN,
			3, 0x03, 0, class_D_drv_tlv),
	/* SP Analog Gain Volume Control */
	SOC_SINGLE_TLV("Left Analog Channel Gain", AIC31XX_LANALOGSPL,
			0, 0x7F, 1, sp_vol_tlv),
	SOC_SINGLE("SP driver mute", AIC31XX_SPLGAIN,
			 2, 1, 0),
};

static const struct snd_kcontrol_new asilin_control =
			 SOC_DAPM_ENUM("ASIIn Left Route", asilin_enum);

static const struct snd_kcontrol_new asirin_control =
			 SOC_DAPM_ENUM("ASIIn Right Route", asirin_enum);

/* Local wait_bits function to handle wait events*/
int aic31xx_wait_bits(struct aic31xx_priv *aic31xx, unsigned int reg,
			unsigned int mask, unsigned char val, int sleep,
			int counter)
{
	unsigned int value;
	int timeout = sleep * counter;

	value = snd_soc_read(aic31xx->codec, reg);
	while (((value & mask) != val) && counter) {
		usleep_range(sleep, sleep + 100);
		value = snd_soc_read(aic31xx->codec, reg);
		counter--;
	};
	if (!counter)
		dev_err(aic31xx->dev,
			"wait_bits timedout (%d millisecs). lastval 0x%x\n",
			timeout, value);
	return counter;
}

/**
 *aic31xx_dac_power_up_event: Headset popup reduction and powering up dsps together
 *			when they are in sync mode
 * @w: pointer variable to dapm_widget
 * @kcontrol: pointer to sound control
 * @event: event element information
 *
 * Returns 0 for success.
 */
static int aic31xx_dac_power_up_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	int reg_mask = 0;
	int ret_wbits = 0;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(w->codec);

	if (w->shift == 7)
		reg_mask = AIC31XX_LDACPWRSTATUS_MASK;

	if (w->shift == 6)
		reg_mask = AIC31XX_RDACPWRSTATUS_MASK;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret_wbits = aic31xx_wait_bits(aic31xx,
						AIC31XX_DACFLAG1,
						reg_mask, reg_mask,
						AIC31XX_TIME_DELAY,
						AIC31XX_DELAY_COUNTER);
		if (!ret_wbits) {
			dev_err(w->codec->dev, "DAC_post_pmu timed out\n");
			return -1;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret_wbits = aic31xx_wait_bits(aic31xx,
			AIC31XX_DACFLAG1, reg_mask, 0,
			AIC31XX_TIME_DELAY, AIC31XX_DELAY_COUNTER);
		if (!ret_wbits) {
			dev_err(w->codec->dev, "DAC_post_pmd timed out\n");
			return -1;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/**
 * aic31xx_adc_power_up_event: To get DSP run state to perform synchronization
 * @w: pointer variable to dapm_widget
 * @kcontrol: pointer to sound control
 * @event: event element information
 *
 * Returns 0 for success.
 */
static int aic31xx_adc_power_up_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	int reg_mask = 0;
	int ret_wbits = 0;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(w->codec);

	if (w->shift == 7)
		reg_mask = AIC31XX_ADCPWRSTATUS_MASK;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret_wbits = aic31xx_wait_bits(aic31xx,
						AIC31XX_ADCFLAG, reg_mask,
						reg_mask, AIC31XX_TIME_DELAY,
						AIC31XX_DELAY_COUNTER);
		if (!ret_wbits) {
			dev_err(w->codec->dev, "ADC POST_PMU timedout\n");
			return -1;
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
		ret_wbits = aic31xx_wait_bits(aic31xx,
						AIC31XX_ADCFLAG, reg_mask, 0,
						AIC31XX_TIME_DELAY,
						AIC31XX_DELAY_COUNTER);
		if (!ret_wbits) {
			dev_err(w->codec->dev, "ADC POST_PMD timedout\n");
			return -1;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/*Hp_power_up_event without powering on/off headphone driver,
* instead muting hpl & hpr */
static int aic31xx_hp_power_up_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	int ret_wbits = 0;
	/* TODO: Already checked with TI, why same thing is getting
		 done for powerup and poweron both
	 */
	if (SND_SOC_DAPM_EVENT_ON(event)) {

		if (!(strcmp(w->name, "HPL Driver"))) {
			ret_wbits = aic31xx_wait_bits(aic31xx,
					AIC31XX_DACFLAG1, AIC31XX_HPL_MASK,
					AIC31XX_HPL_MASK, AIC31XX_TIME_DELAY,
					AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "HPL Power Timedout\n");

		}
		if (!(strcmp(w->name, "HPR Driver"))) {
			ret_wbits = aic31xx_wait_bits(aic31xx,
					AIC31XX_DACFLAG1, AIC31XX_HPR_MASK,
					AIC31XX_HPR_MASK, AIC31XX_TIME_DELAY,
					AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "HPR Power Timedout\n");
		}
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {

		if (!(strcmp(w->name, "HPL Driver"))) {
			ret_wbits = aic31xx_wait_bits(aic31xx,
					AIC31XX_DACFLAG1, AIC31XX_HPL_MASK,
					0x0, AIC31XX_TIME_DELAY,
					AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "HPL Power Timedout\n");

		}
		if (!(strcmp(w->name, "HPR Driver"))) {
			ret_wbits = aic31xx_wait_bits(aic31xx,
					AIC31XX_DACFLAG1, AIC31XX_HPR_MASK,
					0x0, AIC31XX_TIME_DELAY,
					AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "HPR Power Timedout\n");
		}
	}
	return 0;
}


static int aic31xx_sp_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	int ret_wbits = 0;
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Check for the DAC FLAG register to know if the SPL & SPR are
		 * really powered up
		 */
		if (w->shift == 7) {
			ret_wbits = aic31xx_wait_bits(aic31xx,
					AIC31XX_DACFLAG1,  AIC31XX_SPL_MASK,
					AIC31XX_SPL_MASK, AIC31XX_TIME_DELAY,
					AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "SPL power timedout\n");
			}


		if (w->shift == 6) {
			ret_wbits = aic31xx_wait_bits(aic31xx,
					AIC31XX_DACFLAG1,  AIC31XX_SPR_MASK,
					AIC31XX_SPR_MASK, AIC31XX_TIME_DELAY,
					AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "SPR power timedout\n");

			}
		}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		/* Check for the DAC FLAG register to know if the SPL & SPR are
		 * powered down
		 */
		if (w->shift == 7) {
			ret_wbits = aic31xx_wait_bits(aic31xx,
					AIC31XX_DACFLAG1,  AIC31XX_SPL_MASK,
					0x0, AIC31XX_TIME_DELAY,
					AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "SPL power timedout\n");
			}
		if (w->shift == 6) {
			ret_wbits = aic31xx_wait_bits(aic31xx,
					AIC31XX_DACFLAG1,  AIC31XX_SPR_MASK,
					0x0, AIC31XX_TIME_DELAY,
					AIC31XX_DELAY_COUNTER);
			if (!ret_wbits)
				dev_dbg(codec->dev, "SPR power timedout\n");
			}
	}
	return 0;
}

/* Left Output Mixer */
static const struct snd_kcontrol_new
left_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("From DAC_L", AIC31XX_DACMIXERROUTE, 6, 1, 0),
	SOC_DAPM_SINGLE("From MIC1LP", AIC31XX_DACMIXERROUTE, 5, 1, 0),
	SOC_DAPM_SINGLE("From MIC1RP", AIC31XX_DACMIXERROUTE, 4, 1, 0),
};

/* Right Output Mixer - Valid only for AIC31xx, 3110, 3100 */
static const struct
snd_kcontrol_new right_output_mixer_controls[] = {
	SOC_DAPM_SINGLE("From DAC_R", AIC31XX_DACMIXERROUTE, 2, 1, 0),
	SOC_DAPM_SINGLE("From MIC1RP", AIC31XX_DACMIXERROUTE, 1, 1, 0),
};

static const struct
snd_kcontrol_new pos_mic_imp_controls[] = {
	SOC_DAPM_SINGLE("MIC1LP_IMP_CNTL", AIC31XX_MICPGAPI, 6, 0x3, 0),
	SOC_DAPM_SINGLE("MIC1RP_IMP_CNTL", AIC31XX_MICPGAPI, 4, 0x3, 0),
	SOC_DAPM_SINGLE("MIC1LM_IMP_CNTL", AIC31XX_MICPGAPI, 2, 0x3, 0),
};

static const struct
snd_kcontrol_new neg_mic_imp_controls[] = {
	SOC_DAPM_SINGLE("CM_IMP_CNTL", AIC31XX_MICPGAMI, 6, 0x3, 0),
	SOC_DAPM_SINGLE("MIC1LM_IMP_CNTL", AIC31XX_MICPGAMI, 4, 0x3, 0),
};

static const struct snd_kcontrol_new aic31xx_dapm_hpl_vol_control =
	SOC_DAPM_SINGLE("Switch", AIC31XX_LANALOGHPL, 7, 1, 0);

static const struct snd_kcontrol_new aic31xx_dapm_hpr_vol_control =
	SOC_DAPM_SINGLE("Switch", AIC31XX_RANALOGHPR, 7, 1, 0);

static const struct snd_kcontrol_new aic31xx_dapm_spl_vol_control =
	SOC_DAPM_SINGLE("Switch", AIC31XX_LANALOGSPL, 7, 1, 0);

static const struct snd_kcontrol_new aic31xx_dapm_spr_vol_control =
	SOC_DAPM_SINGLE("Switch", AIC31XX_RANALOGSPR, 7, 1, 0);

/* TODO: Already check with TI on PLL not getting switched
	 off and on in event. Is it required or not
 */
static int pll_power_on_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	if (SND_SOC_DAPM_EVENT_ON(event))
		dev_dbg(codec->dev, "pll->on pre_pmu");
	else if (SND_SOC_DAPM_EVENT_OFF(event))
		dev_dbg(codec->dev, "pll->off\n");

	/* Sleep for 10 ms minumum */
	usleep_range(10000, 15000);

	return 0;
}

static int micbias_power_on_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	if (SND_SOC_DAPM_EVENT_ON(event))
		snd_soc_update_bits(codec, AIC31XX_MICBIAS, 0x03, (0x02));
	else if (SND_SOC_DAPM_EVENT_OFF(event))
		snd_soc_update_bits(codec, AIC31XX_MICBIAS, 0x03, (0x0));

	return 0;
}

static const struct snd_soc_dapm_widget aic31xx_dapm_widgets[] = {
	/*ASI*/
	SND_SOC_DAPM_AIF_IN("ASIIN", "ASI Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("ASILIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASIRIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASIMonoMixIN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ASI IN Port", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("ASIIn Left Route",
				SND_SOC_NOPM, 0, 0, &asilin_control),
	SND_SOC_DAPM_MUX("ASIIn Right Route",
				SND_SOC_NOPM, 0, 0, &asirin_control),
	/* DACs */
	SND_SOC_DAPM_DAC_E("Left DAC", "Left Playback",
			AIC31XX_DACSETUP, 7, 0, aic31xx_dac_power_up_event,
			SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("Right DAC", "Right Playback",
			AIC31XX_DACSETUP, 6, 0, aic31xx_dac_power_up_event,
			SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),

	/* Output Mixers */
	SND_SOC_DAPM_MIXER("Left Output Mixer", SND_SOC_NOPM, 0, 0,
			left_output_mixer_controls,
			ARRAY_SIZE(left_output_mixer_controls)),
	SND_SOC_DAPM_MIXER("Right Output Mixer", SND_SOC_NOPM, 0, 0,
			right_output_mixer_controls,
			ARRAY_SIZE(right_output_mixer_controls)),


	SND_SOC_DAPM_SWITCH("HP Left Analog Volume", SND_SOC_NOPM, 0, 0,
				&aic31xx_dapm_hpl_vol_control),
	SND_SOC_DAPM_SWITCH("HP Right Analog Volume", SND_SOC_NOPM, 0, 0,
				&aic31xx_dapm_hpr_vol_control),

	/* Output drivers */
	SND_SOC_DAPM_OUT_DRV_E("HPL Driver", AIC31XX_HPDRIVER, 7, 0,
			NULL, 0, aic31xx_hp_power_up_event,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUT_DRV_E("HPR Driver", AIC31XX_HPDRIVER, 6, 0,
			NULL, 0, aic31xx_hp_power_up_event,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* ADC */
	SND_SOC_DAPM_ADC_E("ADC", "Capture", AIC31XX_ADCSETUP, 7, 0,
			aic31xx_adc_power_up_event, SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_POST_PMD),

	/*Input Selection to MIC_PGA*/
	SND_SOC_DAPM_MIXER("P_Input_Mixer", SND_SOC_NOPM, 0, 0,
		pos_mic_imp_controls, ARRAY_SIZE(pos_mic_imp_controls)),
	SND_SOC_DAPM_MIXER("M_Input_Mixer", SND_SOC_NOPM, 0, 0,
		neg_mic_imp_controls, ARRAY_SIZE(neg_mic_imp_controls)),

	/*Enabling & Disabling MIC Gain Ctl */
	SND_SOC_DAPM_PGA("MIC_GAIN_CTL", AIC31XX_MICPGA,
		7, 1, NULL, 0),

	SND_SOC_DAPM_SUPPLY("PLLCLK", AIC31XX_PLLPR, 7, 0, pll_power_on_event,
				 SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("CODEC_CLK_IN", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("NDAC_DIV", AIC31XX_NDAC, 7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MDAC_DIV", AIC31XX_MDAC, 7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("NADC_DIV", AIC31XX_NADC, 7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MADC_DIV", AIC31XX_MADC, 7, 0, NULL, 0),
	/* Outputs */
	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),

	/* Inputs */
	SND_SOC_DAPM_INPUT("MIC1LP"),
	SND_SOC_DAPM_INPUT("MIC1RP"),
	SND_SOC_DAPM_INPUT("MIC1LM"),
	SND_SOC_DAPM_MICBIASCUSTOM("micbias", SND_SOC_NOPM, 0,
		micbias_power_on_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

};

static const struct snd_soc_dapm_widget aic311x_dapm_widgets[] = {
	/* For AIC31XX and AIC3110 as it is stereo both left and right channel
	 * class-D can be powered up/down
	 */
	SND_SOC_DAPM_OUT_DRV_E("SPL ClassD", AIC31XX_SPKAMP, 7, 0, NULL, 0,
				aic31xx_sp_event, SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUT_DRV_E("SPR ClassD", AIC31XX_SPKAMP, 6, 0, NULL, 0,
				aic31xx_sp_event, SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH("SP Left Analog Volume", SND_SOC_NOPM, 0, 0,
				&aic31xx_dapm_spl_vol_control),
	SND_SOC_DAPM_SWITCH("SP Right Analog Volume", SND_SOC_NOPM, 0, 0,
				&aic31xx_dapm_spr_vol_control),
	SND_SOC_DAPM_OUTPUT("SPL"),
	SND_SOC_DAPM_OUTPUT("SPR"),
};

static const struct snd_soc_dapm_widget aic310x_dapm_widgets[] = {
	SND_SOC_DAPM_OUT_DRV_E("SPK ClassD", AIC31XX_SPKAMP, 7, 0, NULL, 0,
			aic31xx_sp_event, SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH("Speaker Analog Volume", SND_SOC_NOPM, 0, 0,
				&aic31xx_dapm_spl_vol_control),
	SND_SOC_DAPM_OUTPUT("SPK"),
};

static const struct snd_soc_dapm_route
aic31xx_audio_map[] = {

	{"CODEC_CLK_IN", NULL, "PLLCLK"},
	{"NDAC_DIV", NULL, "CODEC_CLK_IN"},
	{"NADC_DIV", NULL, "CODEC_CLK_IN"},
	{"MDAC_DIV", NULL, "NDAC_DIV"},
	{"MADC_DIV", NULL, "NADC_DIV"},

	/* Clocks for ADC */
	{"ADC", NULL, "MADC_DIV"},
	/*ASI Routing*/
	{"ASILIN", NULL, "ASIIN"},
	{"ASIRIN", NULL, "ASIIN"},
	{"ASIMonoMixIN", NULL, "ASIIN"},
	{"ASIIn Left Route", "ASI Left In", "ASILIN"},
	{"ASIIn Left Route", "ASI Right In", "ASIRIN"},
	{"ASIIn Left Route", "ASI MonoMix In", "ASIMonoMixIN"},
	{"ASIIn Right Route", "ASI Left In", "ASILIN"},
	{"ASIIn Right Route", "ASI Right In", "ASIRIN"},
	{"ASIIn Right Route", "ASI MonoMix In", "ASIMonoMixIN"},
	{"ASI IN Port", NULL, "ASIIn Left Route"},
	{"ASI IN Port", NULL, "ASIIn Right Route"},
	{"Left DAC", NULL, "ASI IN Port"},
	{"Right DAC", NULL, "ASI IN Port"},

	/* Mic input */
	{"P_Input_Mixer", "MIC1LP_IMP_CNTL", "MIC1LP"},
	{"P_Input_Mixer", "MIC1RP_IMP_CNTL", "MIC1RP"},
	{"P_Input_Mixer", "MIC1LM_IMP_CNTL", "MIC1LM"},

	{"M_Input_Mixer", "CM_IMP_CNTL", "MIC1LM"},
	{"M_Input_Mixer", "MIC1LM_IMP_CNTL", "MIC1LM"},
	{"MIC1LM", NULL, "micbias"},

	{"MIC_GAIN_CTL", NULL, "P_Input_Mixer"},
	{"MIC_GAIN_CTL", NULL, "M_Input_Mixer"},

	{"ADC", NULL, "MIC_GAIN_CTL"},

	/* Clocks for DAC */
	{"Left DAC", NULL, "MDAC_DIV" },
	{"Right DAC", NULL, "MDAC_DIV"},

	/* Left Output */
	{"Left Output Mixer", "From DAC_L", "Left DAC"},
	{"Left Output Mixer", "From MIC1LP", "MIC1LP"},
	{"Left Output Mixer", "From MIC1RP", "MIC1RP"},
	{"MIC1LP", NULL, "micbias"},

	/* Right Output */
	{"Right Output Mixer", "From DAC_R", "Right DAC"},
	{"Right Output Mixer", "From MIC1RP", "MIC1RP"},

	/* HPL path */
	{"HP Left Analog Volume", "Switch", "Left Output Mixer"},
	{"HPL Driver", NULL, "HP Left Analog Volume"},
	{"HPL", NULL, "HPL Driver"},

	/* HPR path */
	{"HP Right Analog Volume", "Switch", "Right Output Mixer"},
	{"HPR Driver", NULL, "HP Right Analog Volume"},
	{"HPR", NULL, "HPR Driver"},

};


static const struct snd_soc_dapm_route
aic311x_audio_map[] = {
	/* SPK L path */
	{"SP Left Analog Volume", "Switch", "Left Output Mixer"},
	{"SPL ClassD", NULL, "SP Left Analog Volume"},
	{"SPL", NULL, "SPL ClassD"},

	/* SPK R path */
	{"SP Left Analog Volume", "Switch", "Left Output Mixer"},
	{"SPR ClassD", NULL, "SP Right Analog Volume"},
	{"SPR", NULL, "SPR ClassD"},
};


static const struct snd_soc_dapm_route
aic310x_audio_map[] = {
	/* SPK L path */
	{"Speaker Analog Volume", "Switch", "Left Output Mixer"},
	{"SPK ClassD", NULL, "Speaker Analog Volume"},
	{"SPK", NULL, "SPK ClassD"},

};

/*
 * aic31xx_add_controls - add non dapm kcontrols.
 *
 * The different controls are in "aic31xx_snd_controls" table. The following
 * different controls are supported
 *
 *	# DAC Playback volume control
 *	# PCM Playback Volume
 *	# HP Driver Gain
 *	# HP DAC Playback Switch
 *	# PGA Capture Volume
 *	# Program Registers
 */
static int aic31xx_add_controls(struct snd_soc_codec *codec)
{
	int err = 0;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s\n", __func__);
	if (aic31xx->pdata.codec_type == AIC311X) {
		err = snd_soc_add_codec_controls(codec, aic311x_snd_controls,
				ARRAY_SIZE(aic311x_snd_controls));
		if (err < 0)
			dev_dbg(codec->dev, "Invalid control\n");

	} else if (aic31xx->pdata.codec_type == AIC310X) {
		err = snd_soc_add_codec_controls(codec, aic310x_snd_controls,
				ARRAY_SIZE(aic310x_snd_controls));
		if (err < 0)
			dev_dbg(codec->dev, "Invalid Control\n");

	}
	return 0;
}

/*
 * aic31xx_add_widgets
 *
 * adds all the ASoC Widgets identified by aic31xx_snd_controls array. This
 * routine will be invoked * during the Audio Driver Initialization.
 */
static int aic31xx_add_widgets(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	dev_dbg(codec->dev, "###aic31xx_add_widgets\n");

	if (aic31xx->pdata.codec_type == AIC311X) {
		ret = snd_soc_dapm_new_controls(dapm, aic311x_dapm_widgets,
					ARRAY_SIZE(aic311x_dapm_widgets));
		if (!ret)
			dev_dbg(codec->dev, "#Completed adding dapm widgets size = %ld\n",
					ARRAY_SIZE(aic311x_dapm_widgets));
		ret = snd_soc_dapm_add_routes(dapm, aic311x_audio_map,
					ARRAY_SIZE(aic311x_audio_map));
		if (!ret)
			dev_dbg(codec->dev, "#Completed adding DAPM routes = %ld\n",
					ARRAY_SIZE(aic311x_audio_map));
	} else if (aic31xx->pdata.codec_type == AIC310X) {
		ret = snd_soc_dapm_new_controls(dapm, aic310x_dapm_widgets,
					ARRAY_SIZE(aic310x_dapm_widgets));
		if (!ret)
			dev_dbg(codec->dev, "#Completed adding dapm widgets size = %ld\n",
					ARRAY_SIZE(aic310x_dapm_widgets));
		ret = snd_soc_dapm_add_routes(dapm, aic310x_audio_map,
					ARRAY_SIZE(aic310x_audio_map));
		if (!ret)
			dev_dbg(codec->dev, "#Completed adding DAPM routes = %ld\n",
					ARRAY_SIZE(aic310x_audio_map));
	}

	return 0;
}

/*
 * This function is to set the hardware parameters for aic31xx.  The
 * functions set the sample rate and audio serial data word length.
 */
static int aic31xx_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *tmp)
{
	struct snd_soc_codec *codec = tmp->codec;
	u8 data;
	dev_dbg(codec->dev, "%s\n", __func__);


	data = snd_soc_read(codec, AIC31XX_IFACE1);
	data = data & ~(3 << 4);

	dev_dbg(codec->dev, "##- Data length: %d\n", params_format(params));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		data |= (AIC31XX_WORD_LEN_20BITS <<
				AIC31XX_IFACE1_DATALEN_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data |= (AIC31XX_WORD_LEN_24BITS <<
				AIC31XX_IFACE1_DATALEN_SHIFT);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (AIC31XX_WORD_LEN_32BITS <<
				AIC31XX_IFACE1_DATALEN_SHIFT);
		break;
	}

	/* Write to Page 0 Reg 27 for the Codec Interface control 1
	 * Register */
	snd_soc_write(codec, AIC31XX_IFACE1, data);

	return 0;
}

/*
 * aic31xx_dac_mute - mute or unmute the left and right DAC

 */
static int aic31xx_dac_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;

	if (mute)
		snd_soc_update_bits(codec, AIC31XX_DACMUTE,
					AIC31XX_DACMUTE_MASK,
					AIC31XX_DACMUTE_MASK);
	else
		snd_soc_update_bits(codec, AIC31XX_DACMUTE,
					AIC31XX_DACMUTE_MASK, 0x0);

	return 0;
}

/*
 * aic31xx_set_dai_fmt
 *
 * This function is to set the DAI format
 */
static int aic31xx_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u8 iface_reg1 = 0;
	u8 iface_reg3 = 0;
	u8 dsp_a_val = 0;

	dev_dbg(codec->dev, "%s: Entered\n", __func__);
	dev_dbg(codec->dev, "###aic31xx_set_dai_fmt %x\n", fmt);


	dev_dbg(codec->dev, "##+ aic31xx_set_dai_fmt (%x)\n", fmt);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface_reg1 |= AIC31XX_BCLK_MASTER | AIC31XX_WCLK_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		iface_reg1 &= ~(AIC31XX_BCLK_MASTER | AIC31XX_WCLK_MASTER);
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		iface_reg1 |= AIC31XX_BCLK_MASTER;
		iface_reg1 &= ~(AIC31XX_WCLK_MASTER);
		break;
	default:
		dev_alert(codec->dev, "Invalid DAI master/slave interface\n");
		return -EINVAL;
	}
	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_DSP_A:
			dsp_a_val = 0x1;
	case SND_SOC_DAIFMT_DSP_B:
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
				break;
		case SND_SOC_DAIFMT_IB_NF:
				iface_reg3 |= AIC31XX_BCLKINV_MASK;
				break;
		default:
				return -EINVAL;
		}
		iface_reg1 |= (AIC31XX_DSP_MODE <<
				AIC31XX_IFACE1_DATATYPE_SHIFT);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface_reg1 |= (AIC31XX_RIGHT_JUSTIFIED_MODE <<
				AIC31XX_IFACE1_DATATYPE_SHIFT);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_reg1 |= (AIC31XX_LEFT_JUSTIFIED_MODE <<
				AIC31XX_IFACE1_DATATYPE_SHIFT);
		break;
	default:
		dev_alert(codec->dev, "Invalid DAI interface format\n");
		return -EINVAL;
	}

	snd_soc_update_bits(codec, AIC31XX_IFACE1,
			AIC31XX_IFACE1_DATATYPE_MASK |
			AIC31XX_IFACE1_MASTER_MASK,
			iface_reg1);
	snd_soc_update_bits(codec, AIC31XX_DATA_OFFSET,
			AIC31XX_DATA_OFFSET_MASK,
			dsp_a_val);
	snd_soc_update_bits(codec, AIC31XX_IFACE2,
			AIC31XX_BCLKINV_MASK,
			iface_reg3);

	dev_dbg(codec->dev, "##-aic31xx_set_dai_fmt Master\n");
	dev_dbg(codec->dev, "%s: Exiting\n", __func__);

	return 0;
}


/*
* aic31xx_set_dai_pll
*
* This function is invoked as part of the PLL call-back
* handler from the ALSA layer.
*/
static int aic31xx_set_dai_pll(struct snd_soc_dai *dai,
		int pll_id, int source, unsigned int freq_in,
		unsigned int freq_out)
{
	struct snd_soc_codec *codec = dai->codec;
	int i;
	dev_dbg(codec->dev, "In aic31xx: dai_set_pll\n");

	snd_soc_update_bits(codec, AIC31XX_CLKMUX,
		AIC31XX_PLL_CLKIN_MASK, source << AIC31XX_PLL_CLKIN_SHIFT);

	/* Use PLL as CODEC_CLKIN and DAC_MOD_CLK as BDIV_CLKIN */
	snd_soc_update_bits(codec, AIC31XX_CLKMUX,
		AIC31XX_CODEC_CLKIN_MASK, AIC31XX_CODEC_CLKIN_PLL);
	snd_soc_update_bits(codec, AIC31XX_IFACE2, AIC31XX_BDIVCLK_MASK,
				AIC31XX_DACMOD2BCLK);

	for (i = 0; i < ARRAY_SIZE(aic31xx_divs); i++) {
		if ((aic31xx_divs[i].rate == freq_out)
			&& (aic31xx_divs[i].mclk == freq_in)) {
				break;
		}
	}

	if (i == ARRAY_SIZE(aic31xx_divs)) {
		dev_err(codec->dev, "sampling rate not supported\n");
		return -EINVAL;
	}

	snd_soc_update_bits(codec, AIC31XX_PLLPR, AIC31XX_PLL_MASK,
			(aic31xx_divs[i].p_val << 4) | 0x01);
	snd_soc_write(codec, AIC31XX_PLLJ, aic31xx_divs[i].pll_j);

	snd_soc_write(codec, AIC31XX_PLLDMSB, (aic31xx_divs[i].pll_d >> 8));
	snd_soc_write(codec, AIC31XX_PLLDLSB,
		      (aic31xx_divs[i].pll_d & 0xff));

	/* NDAC divider value */
	snd_soc_update_bits(codec, AIC31XX_NDAC, AIC31XX_PLL_MASK,
			aic31xx_divs[i].ndac);

	/* MDAC divider value */
	snd_soc_update_bits(codec, AIC31XX_MDAC, AIC31XX_PLL_MASK,
			aic31xx_divs[i].mdac);


	/* DOSR MSB & LSB values */
	snd_soc_write(codec, AIC31XX_DOSRMSB, aic31xx_divs[i].dosr >> 8);
	snd_soc_write(codec, AIC31XX_DOSRLSB,
		      (aic31xx_divs[i].dosr & 0xff));
	/* NADC divider value */
	snd_soc_update_bits(codec, AIC31XX_NADC, AIC31XX_PLL_MASK,
			aic31xx_divs[i].nadc);
	/* MADC divider value */
	snd_soc_update_bits(codec, AIC31XX_MADC, AIC31XX_PLL_MASK,
			aic31xx_divs[i].madc);
	/* AOSR value */
	snd_soc_write(codec, AIC31XX_AOSR, aic31xx_divs[i].aosr);
	/* BCLK N divider */
	snd_soc_update_bits(codec, AIC31XX_BCLKN, AIC31XX_PLL_MASK,
			aic31xx_divs[i].bclk_n);


	dev_dbg(codec->dev, "%s: DAI ID %d PLL_ID %d InFreq %d OutFreq %d\n",
		__func__, pll_id, dai->id, freq_in, freq_out);
	return 0;
}


/*
 * aic31xx_set_sysclk - This function is called from machine driver to switch
 *			clock source based on DAPM Event
 */
static int aic31xx_set_sysclk(struct snd_soc_codec *codec,
		int clk_id, int source, unsigned int freq, int dir)
{
	/* Frequency required by jack detection logic when
	 * on external clock
	 */
	int divider = 1;
	if (clk_id == AIC31XX_MCLK) {
		snd_soc_update_bits(codec, AIC31XX_TIMERCLOCK,
			 AIC31XX_CLKSEL_MASK, AIC31XX_CLKSEL_MASK);

		divider = freq / AIC31XX_REQ_TIMER_FREQ;
		/* Added +1 to divider if divider is not exact.
		 * This will make sure divider is always == ||
		 * > required divider. So frequency will
		 * round off to lower than ~1MHz
		 */
		if (freq % AIC31XX_REQ_TIMER_FREQ)
			divider++;

		snd_soc_update_bits(codec, AIC31XX_TIMERCLOCK,
			 AIC31XX_DIVIDER_MASK, divider);

		dev_dbg(codec->dev, "%s: input freq = %d divider = %d",
		__func__, freq, divider);

	} else if (clk_id == AIC31XX_INTERNALCLOCK) {
		snd_soc_update_bits(codec, AIC31XX_TIMERCLOCK,
			AIC31XX_CLKSEL_MASK, 0x0);
	} else {
		dev_err(codec->dev, "Wrong clock src\n");
		return -EINVAL;
	}
	return 0;
}

/*
 * aic31xx_set_bias_level - This function is to get triggered when dapm
 * events occurs.
 */
static int aic31xx_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	dev_dbg(codec->dev, "## aic31xx_set_bias_level %d\n", level);
	if (level == codec->dapm.bias_level) {
		dev_dbg(codec->dev, "##set_bias_level: level returning...\r\n");
		return 0;
	}
	switch (level) {
	/* full On */
	case SND_SOC_BIAS_ON:
		/* All power is driven by DAPM system*/
		dev_dbg(codec->dev, "###aic31xx_set_bias_level BIAS_ON\n");
			snd_soc_update_bits(codec, AIC31XX_BCLKN,
					AIC31XX_PLL_MASK, AIC31XX_PLL_MASK);
		break;

	/* partial On */
	case SND_SOC_BIAS_PREPARE:
		dev_dbg(codec->dev, "###aic31xx_set_bias_level BIAS_PREPARE\n");
		if (codec->dapm.bias_level == ((SND_SOC_BIAS_ON))) {
			snd_soc_update_bits(codec, AIC31XX_BCLKN,
					AIC31XX_PLL_MASK, AIC31XX_PLL_MASK);
		}
		break;

	/* Off, with power */
	case SND_SOC_BIAS_STANDBY:
		dev_dbg(codec->dev, "###aic31xx_set_bias_level STANDBY\n");
		break;

	/* Off, without power */
	case SND_SOC_BIAS_OFF:
		dev_dbg(codec->dev, "###aic31xx_set_bias_level OFF\n");
		break;

	}
	codec->dapm.bias_level = level;
	dev_dbg(codec->dev, "## aic31xx_set_bias_level %d\n", level);

	return 0;
}


static int aic31xx_suspend(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s: Entered\n", __func__);

	aic31xx_set_bias_level(codec, SND_SOC_BIAS_OFF);
	dev_dbg(codec->dev, "%s: Exiting\n", __func__);
	return 0;
}

static int aic31xx_resume(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s: Entered\n", __func__);

	aic31xx_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	dev_dbg(codec->dev, "%s: Exiting\n", __func__);

	return 0;
}

void aic31xx_btn_press_intr_enable(struct snd_soc_codec *codec,
		int enable)
{
	dev_dbg(codec->dev, "%s: %s\n", __func__,
				enable ? "enable" : "disable");
	if (enable)
		snd_soc_update_bits(codec, AIC31XX_INT1CTRL,
				AIC31XX_BUTTONPRESSDET_MASK,
				AIC31XX_BUTTONPRESSDET_MASK);
	else
		snd_soc_update_bits(codec, AIC31XX_INT1CTRL,
				AIC31XX_BUTTONPRESSDET_MASK,
				0x0);
}
EXPORT_SYMBOL_GPL(aic31xx_btn_press_intr_enable);

/**
* aic31xx_hs_jack_report: Report jack notification to upper layer
* @codec: pointer variable to codec having information related to codec
* @jack: Pointer variable to snd_soc_jack having information of codec
* and pin number$
* @report: Provides informaton of whether it is headphone or microphone
*
*/
static void aic31xx_hs_jack_report(struct snd_soc_codec *codec,
			struct snd_soc_jack *jack, int report)
{
	struct aic31xx_priv *aic31xx = snd_soc_codec_get_drvdata(codec);
	int status, state = 0, switch_state = 0;
	u8 val;

	mutex_lock(&aic31xx->mutex);

	val = snd_soc_read(codec, AIC31XX_INTRDACFLAG);
	status = snd_soc_update_bits(codec, AIC31XX_HSDETECT,
				AIC31XX_HSPLUGDET_MASK, 0);
	/* Sleep for 10 ms minumum */
	usleep_range(10000, 15000);

	snd_soc_update_bits(codec, AIC31XX_MICBIAS,
				0x03, (0x03));
	/* Sleep for 10 ms minumum */
	usleep_range(10000, 15000);
	snd_soc_update_bits(codec, AIC31XX_HSDETECT, AIC31XX_HSPLUGDET_MASK,
				AIC31XX_HSPLUGDET_MASK);
	msleep(64);
	status = snd_soc_read(codec, AIC31XX_HSDETECT);

	switch (status & AIC31XX_HS_MASK) {
	case  AIC31XX_HS_MASK:
		state |= SND_JACK_HEADSET;
		break;
	case AIC31XX_HP_MASK:
		state |= SND_JACK_HEADPHONE;
		break;
	default:
		break;
	}
	mutex_unlock(&aic31xx->mutex);
	snd_soc_jack_report(jack, state, report);
	if ((state & SND_JACK_HEADSET) == SND_JACK_HEADSET)
		switch_state |= (1<<0);
	else if (state & SND_JACK_HEADPHONE)
		switch_state |= (1<<1);
	dev_dbg(codec->dev, "Headset status =%x, state = %x, switch_state = %x\n",
		status, state, switch_state);

}
EXPORT_SYMBOL_GPL(aic31xx_hs_jack_report);

void aic31xx_enable_mic_bias(struct snd_soc_codec *codec, int enable)
{
	if (enable)
		snd_soc_update_bits(codec, AIC31XX_MICBIAS, 0x03, (0x03));
	else
		snd_soc_update_bits(codec, AIC31XX_MICBIAS, 0x03, (0x0));
}
EXPORT_SYMBOL_GPL(aic31xx_enable_mic_bias);

/**
* aic31xx_hs_jack_report: Report jack notification to upper layer
* @codec: pointer variable to codec having information related to codec
* @jack: Pointer variable to snd_soc_jack having information of codec
* and pin number$
* @report: Provides informaton of whether it is headphone or microphone
*
*/
int aic31xx_query_jack_status(struct snd_soc_codec *codec)
{
	int status, state = 0;

	status = snd_soc_read(codec, AIC31XX_HSDETECT);

	switch (status & AIC31XX_HS_MASK) {
	case  AIC31XX_HS_MASK:
		state |= SND_JACK_HEADSET;
		break;
	case AIC31XX_HP_MASK:
		state |= SND_JACK_HEADPHONE;
		break;
	default:
		break;
	}
	dev_dbg(codec->dev,
			"AIC31XX_HSDETECT=0x%X,Jack Status returned is %x\n",
								 status, state);
	return state;
}
EXPORT_SYMBOL_GPL(aic31xx_query_jack_status);

int aic31xx_query_btn_press(struct snd_soc_codec *codec)
{
	int state = 0, status;

	status = snd_soc_read(codec, AIC31XX_INTRFLAG);
	dev_dbg(codec->dev, "Status(P0/46): %x\n", status);
	/** when HS is plugging out, BTN interrupts may be triggered
	*  It is fake BTN press, should not be reported */
	if ((status & AIC31XX_BTN_HS_STATUS_MASK) == AIC31XX_BTN_HS_STATUS_MASK)
		return state | SND_JACK_BTN_0;
	return state;

}
EXPORT_SYMBOL_GPL(aic31xx_query_btn_press);

/**
 * Instantiate the generic non-control parts of the device.
 */
int aic31xx_device_init(struct aic31xx_priv *aic31xx)
{
	int ret, i;
	u8 resetval = 1;
	unsigned int val;

	dev_info(aic31xx->dev, "aic31xx_device_init beginning\n");

	dev_set_drvdata(aic31xx->dev, aic31xx);

	if (dev_get_platdata(aic31xx->dev))
		memcpy(&aic31xx->pdata, dev_get_platdata(aic31xx->dev),
			sizeof(aic31xx->pdata));
	/*GPIO reset for TLV320AIC31xx codec */
	if (aic31xx->pdata.gpio_reset) {
		ret = devm_gpio_request_one(aic31xx->dev,
				aic31xx->pdata.gpio_reset,
				GPIOF_OUT_INIT_HIGH, "aic31xx-reset-pin");
		if (ret < 0) {
			dev_err(aic31xx->dev, "not able to acquire gpio\n");
			goto err;
		}
	}

	/* run the codec through software reset */
	ret = regmap_write(aic31xx->regmap, AIC31XX_RESET, resetval);
	if (ret < 0) {
		dev_err(aic31xx->dev, "Could not write to AIC31xx register\n");
		goto err;
	}

	/* This is according to datasheet */
	/* Sleep for 10 ms minumum */
	usleep_range(10000, 15000);
	ret = regmap_read(aic31xx->regmap, AIC31XX_REV_PG_ID, &val);

	if (ret < 0) {
		dev_err(aic31xx->dev, "Failed to read ID register\n");
		goto err;
	}
	/* Init the gpio registers */
	for (i = 0; i < aic31xx->pdata.num_gpios; i++) {
		regmap_write(aic31xx->regmap,
			aic31xx->pdata.gpio_defaults[i].reg,
			aic31xx->pdata.gpio_defaults[i].value);
	}

	return 0;

err:
	return ret;
}

void aic31xx_device_exit(struct aic31xx_priv *aic31xx)
{
}

/*
 * Codec probe function, called upon codec registration
 *
 */

static int aic31xx_codec_probe(struct snd_soc_codec *codec)
{


	int ret = 0;
	struct aic31xx_priv *aic31xx;

	aic31xx = snd_soc_codec_get_drvdata(codec);
	codec->control_data = aic31xx->regmap;

	aic31xx->codec = codec;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_REGMAP);

	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache i/o:%d\n", ret);
		return ret;
	}

	mutex_init(&aic31xx->mutex);

	/* Dynamic Headset detection enabled */
	snd_soc_update_bits(codec, AIC31XX_HSDETECT,
			AIC31XX_HSPLUGDET_MASK,
			AIC31XX_HSPLUGDET_MASK);
	snd_soc_update_bits(codec, AIC31XX_INT1CTRL,
			AIC31XX_HSPLUGDET_MASK,
			AIC31XX_HSPLUGDET_MASK);
	snd_soc_update_bits(codec, AIC31XX_INT1CTRL,
			AIC31XX_BUTTONPRESSDET_MASK,
			AIC31XX_BUTTONPRESSDET_MASK);
	/* Program codec to use internal clock */
	snd_soc_update_bits(codec, AIC31XX_TIMERCLOCK,
			AIC31XX_CLKSEL_MASK, 0x0);

	/* Debounce time depends on input clock. Set
	 * debounce time for internal clock, since at
	 * start we will be working with internal clock
	 * 0x4 - 256ms debounce
	 */
	snd_soc_update_bits(codec, AIC31XX_HSDETECT,
			AIC31XX_JACK_DEBOUCE_MASK, 0x4<<2);

	/* set debounce time for button */
	snd_soc_update_bits(codec, AIC31XX_HSDETECT,
			AIC31XX_BTN_DEBOUCE_MASK, 0x03);

	/*Reconfiguring CM to band gap mode*/
	snd_soc_update_bits(codec, AIC31XX_HPPOP, 0xff, 0xA8);

	/*disable soft stepping of DAC volume */
	snd_soc_update_bits(codec, AIC31XX_DACSETUP,
				AIC31XX_SOFTSTEP_MASK, 0x02);

	/* HP driver weakly driven to common mode during powerdown */

	/*Speaker ramp up time scaled to 30.5ms*/
	snd_soc_write(codec, AIC31XX_SPPGARAMP, 0x70);

	/* ADC Umnute */
	snd_soc_update_bits(codec, AIC31XX_ADCFGA, AIC31XX_ADCMUTE_MASK, 0x0);

	/* off, with power on */
	aic31xx_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	aic31xx_add_controls(codec);
	aic31xx_add_widgets(codec);
	dev_dbg(codec->dev, "%d, %s, Firmware test\n", __LINE__, __func__);

	return ret;
}

/*
 * Remove aic31xx soc device
 */
static int aic31xx_codec_remove(struct snd_soc_codec *codec)
{
	/* power down chip */
	aic31xx_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_device |
 *          This structure is soc audio codec device sturecute which pointer
 *          to basic functions aic31xx_probe(), aic31xx_remove(),
 *          aic31xx_suspend() and aic31xx_resume()
 *----------------------------------------------------------------------------
 */
static struct snd_soc_codec_driver soc_codec_driver_aic31xx = {
	.probe			= aic31xx_codec_probe,
	.remove			= aic31xx_codec_remove,
	.suspend		= aic31xx_suspend,
	.resume			= aic31xx_resume,
	.set_bias_level		= aic31xx_set_bias_level,
	.controls		= aic31xx_snd_controls,
	.num_controls		= ARRAY_SIZE(aic31xx_snd_controls),
	.dapm_widgets		= aic31xx_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(aic31xx_dapm_widgets),
	.dapm_routes		= aic31xx_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(aic31xx_audio_map),
	.reg_cache_size		= 0,
	.reg_word_size		= sizeof(u8),
	.reg_cache_default	= NULL,
	.set_sysclk		= aic31xx_set_sysclk,
};

/*
 *----------------------------------------------------------------------------
 * @struct  snd_soc_codec_dai |
 *		It is SoC Codec DAI structure which has DAI capabilities viz.,
 *		playback and capture, DAI runtime information viz. state of DAI
 *		and pop wait state, and DAI private data.
 *		The AIC31xx rates ranges from 8k to 192k
 *		The PCM bit format supported are 16, 20, 24 and 32 bits
 *----------------------------------------------------------------------------
 */

/*
 * DAI ops
 */

static struct snd_soc_dai_ops aic31xx_dai_ops = {
	.hw_params	= aic31xx_hw_params,
	.set_pll	= aic31xx_set_dai_pll,
	.set_fmt	= aic31xx_set_dai_fmt,
	.digital_mute	= aic31xx_dac_mute,
};

/*
 * It is SoC Codec DAI structure which has DAI capabilities viz.,
 * playback and capture, DAI runtime information viz. state of DAI and
 * pop wait state, and DAI private data.  The aic31xx rates ranges
 * from 8k to 192k The PCM bit format supported are 16, 20, 24 and 32
 * bits
 */
static struct snd_soc_dai_driver aic31xx_dai_driver[] = {
{
	.name = "tlv320aic31xx-codec",
		.playback = {
			.stream_name	 = "Playback",
			.channels_min	 = 1,
			.channels_max	 = 2,
			.rates		 = AIC31XX_RATES,
			.formats	 = AIC31XX_FORMATS,
		},
		.capture = {
			.stream_name	 = "Capture",
			.channels_min	 = 1,
			.channels_max	 = 2,
			.rates		 = AIC31XX_RATES,
			.formats	 = AIC31XX_FORMATS,
		},
	.ops = &aic31xx_dai_ops,
}
};

#ifdef CONFIG_ACPI
static int aic31xx_get_acpi_data(struct aic31xx_priv *aic31xx)
{
	acpi_status status;
	acpi_handle handle;
	struct acpi_buffer pdata_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *pdata_buf_ptr;
	union acpi_object *element;
	struct aic31xx_pdata *pdata;
	int ret = 0, ret1 = 0;
	struct gpio_desc *desc;

	dev_dbg(aic31xx->dev, "acpi get data\n");
	pdata = &aic31xx->pdata;
	handle = DEVICE_ACPI_HANDLE(aic31xx->dev);

	status = acpi_evaluate_object(handle, "OBJ1", NULL, &pdata_buffer);
	if (ACPI_FAILURE(status)) {
		dev_err(aic31xx->dev, "Error evaluating OBJ1\n");
		ret = -ENODEV;
		goto end;
	}

	pdata_buf_ptr = pdata_buffer.pointer;
	if (!pdata_buf_ptr || pdata_buf_ptr->type != ACPI_TYPE_PACKAGE) {
		dev_err(aic31xx->dev, "Invalid OBJ1 package data\n");
		ret = -EFAULT;
		goto end;
	}
	element = &(pdata_buf_ptr->package.elements[0]);
	pdata->codec_type = element->integer.value;
	dev_dbg(aic31xx->dev, "element 0 %llx\n", element->integer.value);

	element = &(pdata_buf_ptr->package.elements[1]);
	pdata->audio_mclk1 = element->integer.value;
	dev_dbg(aic31xx->dev, "element 1 %llx\n", element->integer.value);

	desc = devm_gpiod_get_index(aic31xx->dev, NULL, 0);
	if (!IS_ERR(desc)) {
		pdata->gpio_reset =  desc_to_gpio(desc);
		devm_gpiod_put(aic31xx->dev, desc);
		dev_info(aic31xx->dev, "Codec-reset gpio: %d\n",
						 pdata->gpio_reset);
	} else {
		pdata->gpio_reset = -1;
		dev_err(aic31xx->dev, "failed to get gpiod for codec-reset\n");
	}

	element = &(pdata_buf_ptr->package.elements[4]);
	pdata->num_gpios = element->integer.value;
	dev_dbg(aic31xx->dev, "element 4 %llx\n", element->integer.value);

	pdata->gpio_defaults = devm_kzalloc(aic31xx->dev,
				 sizeof(struct aic31xx_gpio_setup),
				 GFP_KERNEL);
	if (pdata->gpio_defaults == NULL)
		return -ENOMEM;

	element = &(pdata_buf_ptr->package.elements[8]);
	pdata->gpio_defaults[0].value = element->integer.value;
	dev_dbg(aic31xx->dev, "element 8 %llx\n", element->integer.value);

	element = &(pdata_buf_ptr->package.elements[9]);
	pdata->gpio_defaults[0].reg = element->integer.value;
	dev_dbg(aic31xx->dev, "element 9 %llx\n", element->integer.value);

end:
	ACPI_FREE(pdata_buffer.pointer);
	return ret;

}
#else
static int aic31xx_get_acpi_data(struct aic31xx_priv *aic31xx)
{
	dev_dbg("CONFIG_ACPI not defined\n");
	return 0;
}

#endif
static struct acpi_device_id aic31xx_acpi_match[] = {
	{ "10TI3100", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, aic31xx_acpi_match);

static int aic31xx_i2c_probe(struct i2c_client *i2c,
					const struct i2c_device_id *id)
{
	struct aic31xx_priv *aic31xx;
	int ret;
	const struct regmap_config *regmap_config;
	struct aic31xx_driver_data *driver_data =
		 (struct aic31xx_driver_data *)id->driver_data;


	regmap_config = &aicxxx_i2c_regmap;

	aic31xx = devm_kzalloc(&i2c->dev, sizeof(*aic31xx), GFP_KERNEL);
	if (aic31xx == NULL)
		return -ENOMEM;

	aic31xx->regmap = devm_regmap_init_i2c(i2c, regmap_config);

	if (IS_ERR(aic31xx->regmap)) {
		ret = PTR_ERR(aic31xx->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}
	aic31xx->dev = &i2c->dev;
	aic31xx->irq = i2c->irq;
	if (driver_data && driver_data->acpi_device) {
		ret = aic31xx_get_acpi_data(aic31xx);
		if (ret) {
			dev_err(&i2c->dev,
				"Failed to get ACPI data: %d\n", ret);
			return ret;
		}
	}
	aic31xx_device_init(aic31xx);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_driver_aic31xx,
			aic31xx_dai_driver, ARRAY_SIZE(aic31xx_dai_driver));

	return ret;
}

static int aic31xx_i2c_remove(struct i2c_client *i2c)
{

	struct aic31xx_priv *aic31xx = dev_get_drvdata(&i2c->dev);
	snd_soc_unregister_codec(aic31xx->dev);
	aic31xx_device_exit(aic31xx);
	return 0;
}

static const struct i2c_device_id aic31xx_i2c_id[] = {
	{ "tlv320aic31xx-codec", (kernel_ulong_t) &aic31xx_i2c_data},
	{"10TI3100:00", (kernel_ulong_t) &aic31xx_acpi_data},
	{"10TI3100", (kernel_ulong_t) &aic31xx_acpi_data},
	{ "i2c-10TI3100:00:1c", (kernel_ulong_t) &aic31xx_acpi_data},
	{ }
};
MODULE_DEVICE_TABLE(i2c, aic31xx_i2c_id);

static struct i2c_driver aic31xx_i2c_driver = {
	.driver = {
		.name	= "tlv320aic31xx-codec",
		.owner	= THIS_MODULE,
		.acpi_match_table = ACPI_PTR(aic31xx_acpi_match),
	},
	.probe		= aic31xx_i2c_probe,
	.remove		= (aic31xx_i2c_remove),
	.id_table	= aic31xx_i2c_id,
};
module_i2c_driver(aic31xx_i2c_driver);

MODULE_DESCRIPTION("ASoC TLV320AIC3111 codec driver");
MODULE_AUTHOR("Ajit Kulkarni");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:tlv320aic31xx-codec");
